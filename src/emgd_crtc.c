/*
 * -----------------------------------------------------------------------------
 *  Filename: emgd_crtc.c
 * -----------------------------------------------------------------------------
 *  Copyright (c) 2002-2013, Intel Corporation.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *
 *-----------------------------------------------------------------------------
 * Description:
 *   Support for CTRC/KMS based interfaces.
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.crtc

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <xf86.h>
#include <X11/extensions/dpmsconst.h>

#include "emgd.h"
#include "emgd_dri2.h"
#include "emgd_crtc.h" /* Use this for all typedefs */
#include "emgd_drm_bo.h"
#include "emgd_uxa.h"
#include "intel_batchbuffer.h"
#include "emgd_sprite.h"

/* From emgd_output.c */
void emgd_output_dpms(xf86OutputPtr output, int mode);
/* From emgd_uxa.c */
Bool intel_uxa_create_screen_resources(ScreenPtr screen);


/*
 * emgd_convert_to_kmode
 *
 * Convert a mode record to a KMS mode record.
 */
static void emgd_convert_to_kmode(ScrnInfoPtr scrn,
		drmModeModeInfoPtr k_mode,
		DisplayModePtr mode)
{

	memset(k_mode, 0, sizeof(*k_mode));

	k_mode->flags       = mode->Flags;
	k_mode->clock       = mode->Clock;

	k_mode->hdisplay    = mode->HDisplay;
	k_mode->hsync_start = mode->HSyncStart;
	k_mode->hsync_end   = mode->HSyncEnd;
	k_mode->htotal      = mode->HTotal;
	k_mode->hskew       = mode->HSkew;

	k_mode->vdisplay    = mode->VDisplay;
	k_mode->vsync_start = mode->VSyncStart;
	k_mode->vsync_end   = mode->VSyncEnd;
	k_mode->vtotal      = mode->VTotal;
	k_mode->vscan       = mode->VScan;

	if (mode->name) {
		strncpy(k_mode->name, mode->name, DRM_DISPLAY_MODE_LEN);
	}
	k_mode->name[DRM_DISPLAY_MODE_LEN - 1] = '\0';
}


/****************************************************************************
 * CRTC functions.
 */

/*
 * emgd_crtc_dpms
 *
 * This gets called to set the crtc into a DPMS power saving mode.
 * Currently, it does nothing.  All DPMS handing is done using the
 * output DPMS function.
 */
static void emgd_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	OS_TRACE_ENTER;
	OS_TRACE_EXIT;
}



static Bool emgd_crtc_apply(xf86CrtcPtr crtc)
{
	xf86OutputPtr output;
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	ScrnInfoPtr scrn = crtc->scrn;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;
	emgd_output_priv_t *emgd_output;
	unsigned long fb_id;
	unsigned long *output_ids;
	int output_count = 0;
	int i, x, y;
	int err;

	OS_TRACE_ENTER;

	/* Get the list of outputs attached to this CRTC */
	output_ids = calloc(sizeof(unsigned long), xf86_config->num_output);
	if (!output_ids) {
		return FALSE;
	}

	for (i = 0; i < xf86_config->num_output; i++) {
		output = xf86_config->output[i];

		if (output->crtc == crtc) {
			emgd_output = output->driver_private;
			output_ids[output_count] = emgd_output->output->connector_id;
			output_count++;
		}
	}

	if (!xf86CrtcRotate(crtc)) {
		free(output_ids);
		return FALSE;
	}

	/* Update gamma */
	OS_DEBUG("Calling gamma_set");
	crtc->funcs->gamma_set(crtc,
			crtc->gamma_red, crtc->gamma_green, crtc->gamma_blue,
			crtc->gamma_size);

	x = crtc->x;
	y = crtc->y;
	fb_id = drmmode->fb_id;
	if (emgd_crtc->shadow_fb_id) {
		OS_DEBUG("Using rotated fb_id of %lu",
			(unsigned long)emgd_crtc->shadow_fb_id);
		fb_id = emgd_crtc->shadow_fb_id;
		x = 0;
		y = 0;
	}

	OS_DEBUG("Calling drmModeSetCrtc(%d, %u, %lu, %d, %d, [%d], mode",
			drmmode->fd, emgd_crtc->crtc->crtc_id, fb_id,
			x, y, output_count);

	OS_DEBUG("mode = \"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x\n",
			emgd_crtc->k_mode.name, emgd_crtc->k_mode.vrefresh,
			emgd_crtc->k_mode.clock,
			emgd_crtc->k_mode.hdisplay, emgd_crtc->k_mode.hsync_start,
			emgd_crtc->k_mode.hsync_end, emgd_crtc->k_mode.htotal,
			emgd_crtc->k_mode.vdisplay, emgd_crtc->k_mode.vsync_start,
			emgd_crtc->k_mode.vsync_end, emgd_crtc->k_mode.vtotal,
			emgd_crtc->k_mode.type, emgd_crtc->k_mode.flags);

	err = drmModeSetCrtc(drmmode->fd,
			emgd_crtc->crtc->crtc_id,
			fb_id,
			(unsigned long)x, (unsigned long)y,
			(uint32_t *)output_ids, output_count,
			(drmModeModeInfoPtr)&emgd_crtc->k_mode);
	if (err) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
				"drmModeSetCrtc() failed to set mode: %s", strerror(-err));
		/* Clean up */
		free(output_ids);
		return FALSE;
	} else {
		/* Turn on outputs */
		for (i = 0; i < xf86_config->num_output; i++) {
			output = xf86_config->output[i];

			if (output->crtc == crtc) {
				OS_DEBUG("Calling emgd_output_dpms");
				emgd_output_dpms(output, DPMSModeOn);
			}
		}

		/* Set cursors (if using hardware cursor) */
		if (scrn->pScreen && iptr->cfg.hw_cursor) {
			OS_DEBUG("Calling xf86_reload_cursors");
			xf86_reload_cursors(scrn->pScreen);
		}
	}

	free(output_ids);
	OS_TRACE_EXIT;
	return TRUE;
}

/*
 * emgd_crtc_set_mode_major
 */
static Bool emgd_crtc_set_mode_major(xf86CrtcPtr crtc,
		DisplayModePtr mode,
		Rotation rotation,
		int x, int y)
{
	ScrnInfoPtr scrn = crtc->scrn;
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;
	unsigned long pitch;
	int handle;
	int err;
	DisplayModeRec saved_mode;
	int saved_x, saved_y;
	Rotation saved_rotation;


	OS_TRACE_ENTER;
	/*
	 * Check to make sure a framebuffer is attached, and if no,
	 * attach it.
	 */
	if (drmmode->fb_id == 0) {
		OS_DEBUG("Calling emgd_frontbuffer_handle");
		emgd_frontbuffer_handle(scrn, &handle, &pitch);
		OS_DEBUG("Calling drmModeAddFB");
		err = drmModeAddFB(drmmode->fd,
				scrn->virtualX, scrn->virtualY,
				scrn->depth, scrn->bitsPerPixel,
				pitch,
				handle,
				(uint32_t *)&drmmode->fb_id);
		if (err < 0) {
			/* Error */
			OS_ERROR("Failed to add FB: %s", strerror(err));
			OS_DEBUG("%d x %d @ %d  %d bpp, pitch = %ld, handle = %d",
					scrn->virtualX, scrn->virtualY,
					scrn->depth, scrn->bitsPerPixel, pitch, handle);

			return FALSE;
		}
		OS_DEBUG("Added FB, id = %lu", (unsigned long)drmmode->fb_id);
	}

	/* Save the current mode information in case the set mode fails */
	saved_mode     = crtc->mode;
	saved_x        = crtc->x;
	saved_y        = crtc->y;
	saved_rotation = crtc->rotation;

	/* Attempt to set the requested mode */
	crtc->mode     = *mode;
	crtc->rotation = rotation;
	crtc->x        = x;
	crtc->y        = y;

	emgd_convert_to_kmode(crtc->scrn, &emgd_crtc->k_mode, mode);
	err = emgd_crtc_apply(crtc);
	if (!err) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
	}

	OS_TRACE_EXIT;
	return err;
}

/*
 * emgd_crtc_set_cursor_colors
 */
static void emgd_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	OS_TRACE_ENTER;
	OS_TRACE_EXIT;
}


/*
 * emgd_crtc_set_cursor_position
 *
 * Move the hardware cursor.
 */
static void emgd_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;

	OS_TRACE_ENTER;
	drmModeMoveCursor(drmmode->fd, emgd_crtc->crtc->crtc_id, x, y);
	OS_TRACE_EXIT;
}



/*
 * emgd_crtc_show_cursor
 *
 * Display the hardware cursor
 */
static void emgd_crtc_show_cursor(xf86CrtcPtr crtc)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;

	OS_TRACE_ENTER;

	drmModeSetCursor(drmmode->fd, emgd_crtc->crtc->crtc_id,
			emgd_crtc->cursor->handle, 64, 64);

	OS_TRACE_EXIT;
}



/*
 * emgd_crtc_hide_cursor
 *
 * Turn off the hardware cursor.
 */
static void emgd_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;

	OS_TRACE_ENTER;

	/* Set the cursor to a null handle */
	drmModeSetCursor(drmmode->fd, emgd_crtc->crtc->crtc_id, 0, 64, 64);

	OS_TRACE_EXIT;
}


/*
 * emgd_crtc_load_cursor_argb
 *
 * Load a new ARGB cursor image.
 */
static void emgd_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;
	int ret;

	OS_TRACE_ENTER;

	if (emgd_crtc->cursor == NULL) {
		/* Need to allocate a cursor */
		emgd_crtc->cursor = emgd_create_cursor(crtc->scrn, 64, 64);
	}

	/* Copy cursor image into cursor memory. */
	ret = drm_intel_bo_subdata(emgd_crtc->cursor, 0, (64 * 64 * 4), image);
	if (ret) {
		OS_ERROR("Failed to load ARGB cursor data");
		return;
	}

	/* Turn on the cursor -- This should be removed. */
	drmModeSetCursor(drmmode->fd, emgd_crtc->crtc->crtc_id,
			emgd_crtc->cursor->handle, 64, 64);

	OS_TRACE_EXIT;
}


/*
 * emgd_crtc_gamma_set
 */
static void emgd_crtc_gamma_set(xf86CrtcPtr crtc,
		CARD16 *red,
		CARD16 *green,
		CARD16 *blue,
		int size)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;

	OS_TRACE_ENTER;
	drmModeCrtcSetGamma(drmmode->fd, emgd_crtc->crtc->crtc_id, size,
			red, green, blue);
	OS_TRACE_EXIT;
}


/*
 * emgd_crtc_destroy
 *
 * Clean up CRTC resources.
 */
static void emgd_crtc_destroy(xf86CrtcPtr crtc)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	ScrnInfoPtr scrn = crtc->scrn;

	OS_TRACE_ENTER;
	/* Free cursor if it was allocated */
	if (emgd_crtc->cursor) {
		emgd_crtc_hide_cursor(crtc);
		emgd_destroy_cursor(scrn, emgd_crtc->cursor);
		free(emgd_crtc->cursor);
		emgd_crtc->cursor = NULL;
	}

	/* Free private data */
	free(crtc->driver_private);
	crtc->driver_private = NULL;
	OS_TRACE_EXIT;
}


/*
 * emgd_crtc_shadow_allocate_bo()
 *
 * Allocates a shadow buffer for rotation for this CRTC (required when using
 * xrandr's rotation).  The creation of an X pixmap to wrap the bo is deferred
 * until later.
 *
 * The shadow bo is the true (rotated) scanout bo; note that this terminology
 * differs from previous EMGD drivers where we tended to refer to the
 * backbuffer (unrotated) as the "shadow."
 *
 * Note that the buffer allocated here is specifically for the purposes of
 * XRandR rotation.  A separate shadow framebuffer is allocated when the
 * driver is in non-rotated "always use shadow" mode.
 */
static void*
emgd_crtc_shadow_allocate_bo(xf86CrtcPtr crtc, int width, int height)
{
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	ScrnInfoPtr scrn = crtc->scrn;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	int aligned_width;
	/* TODO: should probably actually use tiling here */
	uint32_t tiling_mode = I915_TILING_NONE;
	int ret;

	/* Sanity check:  don't leak shadow buffers */
	if (emgd_crtc->shadow_bo) {
		OS_ERROR("Shadow buffer already exists");
		return emgd_crtc->shadow_bo;
	}

	/* Allocate the GEM buffer for this shadowfb */
	aligned_width = ALIGN(width, 64);
	emgd_crtc->shadow_bo = drm_intel_bo_alloc_tiled(iptr->bufmgr, "shadowfb",
		aligned_width, height, iptr->cpp, &tiling_mode,
		&emgd_crtc->shadow_pitch, 0);
	if (!emgd_crtc->shadow_bo) {
		OS_ERROR("Failed to allocate shadow buffer for rotation");
		return FALSE;
	}
	drm_intel_bo_disable_reuse(emgd_crtc->shadow_bo);

	/* Register it as a DRM framebuffer */
	ret = drmModeAddFB(emgd_crtc->drmmode->fd, width, height,
		crtc->scrn->depth, crtc->scrn->bitsPerPixel, emgd_crtc->shadow_pitch,
		emgd_crtc->shadow_bo->handle, &emgd_crtc->shadow_fb_id);
	if (ret) {
		OS_ERROR("Failed to register shadow fb with DRM");
		drm_intel_bo_unreference(emgd_crtc->shadow_bo);
		return NULL;
	}

	return emgd_crtc->shadow_bo;
}


/*
 * emgd_crtc_shadow_create_pixmap()
 *
 * Creates the X pixmap representing the shadow framebuffer.  The
 * bo parameter passed here is the shadow bo allocated with the
 * shadow_allocate function above (if NULL is passed, we take care
 * of bo allocation along with pixmap creation).
 */
static PixmapPtr
emgd_crtc_shadow_create_pixmap(xf86CrtcPtr crtc, void *bo, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	PixmapPtr shadow_pixmap;

	/* If no bo was passed, allocate one now. */
	if (!bo) {
		bo = emgd_crtc_shadow_allocate_bo(crtc, width, height);
		if (!bo) {
			OS_ERROR("Failed to allocate shadow pixmap for rotation");
			return NULL;
		}
	}

	/*
	 * Sanity check:  the bo passed in should always be the one created via
	 * the shadow_allocate interface (and thus should be held in our CRTC
	 * pointer.  Let's just make sure the X server didn't pass us some
	 * random BO.
	 */
	OS_ASSERT(bo == emgd_crtc->shadow_bo,
		"Shadow pixmap creation passed invalid bo", NULL);

	/*
	 * Use the scratch pixmap as the shadow pixmap and associate the shadow
	 * buffer as its backing storage.
	 */
	shadow_pixmap = GetScratchPixmapHeader(scrn->pScreen, width, height,
		scrn->depth, scrn->bitsPerPixel, emgd_crtc->shadow_pitch, NULL);
	if (!shadow_pixmap) {
		OS_ERROR("Failed to get scratch pixmap for shadow FB");
		return NULL;
	}
	intel_set_pixmap_bo(shadow_pixmap, emgd_crtc->shadow_bo);

	iptr->shadow_present = TRUE;

	return shadow_pixmap;
}


/*
 * emgd_crtc_shadow_destroy()
 *
 * Destroys both the shadow buffer (and, if created, the shadow pixmap).
 */
static void emgd_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr pixmap,
	void *bo)
{
	ScrnInfoPtr scrn = crtc->scrn;
	emgd_crtc_priv_t *emgd_crtc = crtc->driver_private;
	emgd_priv_t *iptr = EMGDPTR(scrn);

	/* Shadow pixmap may not have been created.  Ignore it if NULL. */
	if (pixmap) {
		FreeScratchPixmapHeader(pixmap);
	}

	/*
	 * Sanity check:  bo passed should be the shadow bo we allocated for
	 * this CRTC.
	 */
	OS_ASSERT(bo == emgd_crtc->shadow_bo,
		"Wrong buffer passed for shadow destruction", );

	if (bo) {
		/* Deregister shadow buffer from DRM */
		drmModeRmFB(emgd_crtc->drmmode->fd, emgd_crtc->shadow_fb_id);
		emgd_crtc->shadow_fb_id = 0;

		/* Release the GEM buffer */
		drm_intel_bo_unreference(emgd_crtc->shadow_bo);
		emgd_crtc->shadow_bo = NULL;
	}

	/*
	 * We're no longer using the rotation shadow framebuffer, but there might
	 * still be a general (non-rotated) shadowfb in use by the driver.
	 */
	iptr->shadow_present = iptr->use_shadow;
}



/****************************************************************************
 * CRTC function tables.
 *
 * These are the function tables that are given to the the X server as
 * part of the KMS/XRandR implementation.
 */

static const xf86CrtcFuncsRec drmmode_crtc_funcs = {
	.dpms = emgd_crtc_dpms,
	.set_mode_major = emgd_crtc_set_mode_major,
	.set_cursor_colors = emgd_crtc_set_cursor_colors,
	.set_cursor_position = emgd_crtc_set_cursor_position,
	.show_cursor = emgd_crtc_show_cursor,
	.hide_cursor = emgd_crtc_hide_cursor,
	.load_cursor_argb = emgd_crtc_load_cursor_argb,
	.load_cursor_image = NULL,
	.gamma_set = emgd_crtc_gamma_set,
	.destroy = emgd_crtc_destroy,
	.shadow_create = emgd_crtc_shadow_create_pixmap,
	.shadow_allocate = emgd_crtc_shadow_allocate_bo,
	.shadow_destroy = emgd_crtc_shadow_destroy,
};


/*
 * This function is used by our control API when the splashscreen quickboot
 * option was enabled.  Hence, it needs to be available outside this
 * file.
 */
Bool emgd_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	emgd_crtc_priv_t *emgd_crtc = xf86_config->crtc[0]->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	xf86CrtcPtr crtc;
	int i;
	unsigned long pitch;
	int handle;
	unsigned long old_fb_id = 0;
	int old_width, old_height, old_pitch;
	drm_intel_bo *old_fb;
	int ret;

	OS_TRACE_ENTER;

	OS_DEBUG("Resizing to %d x %d", width, height);

	if (scrn->virtualX != width || scrn->virtualY != height) {
		old_fb_id = drmmode->fb_id;
		old_width = scrn->virtualX;
		old_height = scrn->virtualY;
		old_pitch = scrn->displayWidth;
		old_fb = iptr->front_buffer;

		/* Allocate a new frame buffer */
		if (!emgd_frontbuffer_resize(scrn, width, height)) {
			OS_ERROR("Failed to resize the frame buffer.");
			if (iptr->front_buffer) {
				drm_intel_bo_unreference(iptr->front_buffer);
			}
			iptr->front_buffer = old_fb;
			scrn->virtualX = old_width;
			scrn->virtualY = old_height;
			scrn->displayWidth = old_pitch;
			return FALSE;
		}
		scrn->virtualX = width;
		scrn->virtualY = height;

		emgd_frontbuffer_handle(scrn, &handle, &pitch);
		ret = drmModeAddFB(drmmode->fd, width, height, scrn->depth,
				scrn->bitsPerPixel, pitch, handle, (uint32_t *)&drmmode->fb_id);
		if (ret) {
			OS_ERROR("Failed to add new FB (drmModeAddFB) %d", ret);
			if (iptr->front_buffer) {
				drm_intel_bo_unreference(iptr->front_buffer);
			}
			iptr->front_buffer = old_fb;
			scrn->virtualX = old_width;
			scrn->virtualY = old_height;
			scrn->displayWidth = old_pitch;
			if (drmmode->fb_id != old_fb_id) {
				drmModeRmFB(drmmode->fd, drmmode->fb_id);
			}
			drmmode->fb_id = old_fb_id;
			OS_TRACE_EXIT;
			return FALSE;
		}

		intel_uxa_create_screen_resources(scrn->pScreen);
	}

	for (i = 0; i < xf86_config->num_crtc; i++) {
		crtc = xf86_config->crtc[i];

		if (crtc->enabled) {
			emgd_crtc_apply(crtc);
		}
	}


	/* Only remove the FB if one has been replaced. */
	if (old_fb_id) {
		OS_PRINT("Remove old framebuffer %lu", old_fb_id);
		drmModeRmFB(drmmode->fd, old_fb_id);
	}

	OS_TRACE_EXIT;
	return TRUE;
}

static const xf86CrtcConfigFuncsRec drmmode_xf86crtc_config_funcs = {

	emgd_xf86crtc_resize /* drmmode_xf86crtc_resize */
};

/****************************************************************************/


/*
 * drmmode_crtc_init
 *
 * Initialize the xf86 crtc.
 */
static void drmmode_crtc_init(ScrnInfoPtr scrn, drmmode_t *drmmode, int num)
{
	xf86CrtcPtr crtc;
	emgd_crtc_priv_t *priv;

	/* Create the crtc object */
	crtc = xf86CrtcCreate(scrn, &drmmode_crtc_funcs);
	if (!crtc) {
		return;
	}

	/* Create our private object */
	priv = xnfcalloc(sizeof(emgd_crtc_priv_t), 1);
	if (!priv) {
		xf86CrtcDestroy(crtc);
		return;
	}

	priv->crtc = drmModeGetCrtc(drmmode->fd,
			drmmode->mode_res->crtcs[num]);
	if (!priv->crtc) {
		OS_ERROR("Failed to save original CRTC settings for CRTC %d\n", num);
	}
	priv->drmmode = drmmode;

	crtc->driver_private = priv;

	return;
}


/*
 * emgd_vblank_handler()
 *
 * libdrm callback for handling vblank events generated by the kernel.
 */
static void emgd_vblank_handler(int fd,
	unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	void *event)
{
	/* At the moment the only thing we use vblanks for are DRI2 schedules */
	emgd_dri2_vblank_handler(frame, tv_sec, tv_usec, event);
}


/*
 * emgd_page_flip_handler()
 *
 * libdrm callback for handling pageflip events generated by the kernel.
 */
static void emgd_page_flip_handler(int fd,
	unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	void *event)
{
	drmmode_t *kms = event;

	/* Simply return if there is no event */
	if (NULL == event) {
		OS_DEBUG("No event pointer given to emgd_page_flip_handler");
		return;
	}

	/*
	 * This CRTC has completed the flip to the new framebuffer, but others
	 * may not have finished flipping yet.  We don't want to remove the
	 * old framebuffer or consider the flip truly "done" until *all*
	 * CRTC's have finished.
	 */
	if (--kms->flip_count > 0) {
		return;
	}

	/* Release the old framebuffer */
	drmModeRmFB(kms->fd, kms->old_fb_id);

	/*
	 * If this flip was triggered by DRI2, we have some additional processing
	 * to do (i.e., send a "SwapComplete" message to the client.).  If this
	 * flip didn't come from DRI2, we're done.
	 */
	if (!kms->dri2_swapinfo) {
		return;
	}

	/* Let DRI2 know that the flip is fully complete. */
	emgd_dri2_flip_complete(frame, tv_sec, tv_usec, kms->dri2_swapinfo);
}


/*
 * drmmode_pre_init
 *
 * Called by the DDX driver to initialize with KMS.
 */
Bool drmmode_pre_init(ScrnInfoPtr scrn, int fd)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);
	drmmode_t *drmmode;
	int i;

	drmmode = xnfalloc(sizeof(drmmode_t));
	drmmode->fd = fd;
	drmmode->fb_id = 0;
	drmmode->flip_count = 0;

	/* Initialize CRTC support */
	xf86CrtcConfigInit(scrn, &drmmode_xf86crtc_config_funcs);

	/* Get resources */
	drmmode->mode_res = drmModeGetResources(drmmode->fd);
	if (!drmmode->mode_res) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				"failed to get resources: %s\n", strerror(errno));
		return FALSE;
	}

	/* Set size range for CRTC */
	xf86CrtcSetSizeRange(scrn, 320, 200, drmmode->mode_res->max_width,
			drmmode->mode_res->max_height);

	/* Initialize each CRTC */
	for (i = 0; i < drmmode->mode_res->count_crtcs; i++) {
		drmmode_crtc_init(scrn, drmmode, i);
	}

	/* Initialize each output/connector */
	for (i = 0; i < drmmode->mode_res->count_connectors; i++) {
		drmmode_output_init(scrn, drmmode, i);
	}

	/* Setup DRM pageflip and vblank handlers */
	drmmode->event_context.version = DRM_EVENT_CONTEXT_VERSION;
	drmmode->event_context.vblank_handler = emgd_vblank_handler;
	drmmode->event_context.page_flip_handler = emgd_page_flip_handler;

	xf86InitialConfiguration(scrn, TRUE);

	iptr->kms = drmmode;

	return TRUE;
}


/*
 * drmmode_shutdown
 *
 * Called during driver shutdown to clean up KMS usage and restore original
 * CRTC state.
 */
void drmmode_shutdown(emgd_priv_t *iptr)
{
	drmmode_t *kms = iptr->kms;
	emgd_crtc_priv_t *emgdcrtc;
	emgd_output_priv_t *emgdoutput;

	/* If we're giving up before KMS is initialized, do nothing */
	if (!kms) {
		return;
	}

	/* Destroy all CRTC's */
	while (!LIST_IS_EMPTY(&kms->crtcs)) {
		emgdcrtc = LIST_FIRST_ENTRY(&kms->crtcs, emgd_crtc_priv_t, link);
		xf86CrtcDestroy(emgdcrtc->xf86crtc);
	}

	/* Destroy all outputs */
	while (!LIST_IS_EMPTY(&kms->outputs)) {
		emgdoutput = LIST_FIRST_ENTRY(&kms->outputs, emgd_output_priv_t, link);
		xf86OutputDestroy(emgdoutput->xf86output);
	}

	/* Release KMS framebuffer */
	if (kms->fb_id) {
		drmModeRmFB(iptr->drm_fd, kms->fb_id);
	}

	/* Free KMS context */
	free(kms);
	iptr->kms = NULL;
}


/*
 * emgd_crtc_schedule_flip()
 *
 * Dispatches a pageflip request to the DRM.
 */
int emgd_crtc_schedule_flip(emgd_dri2_swap_record_t *swapinfo)
{
	emgd_priv_t *iptr = swapinfo->iptr;
	ScrnInfoPtr pScrn = iptr->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	emgd_crtc_priv_t *emgd_crtc = xf86_config->crtc[0]->driver_private;
	drmmode_t *drmmode = emgd_crtc->drmmode;
	emgd_dri2_private_t *dri2backpriv = swapinfo->back->driverPrivate;
	struct _emgd_pixmap *back_pixmap;
	drm_intel_bo *bo;
	int i, old_fbid, ret;

	/*
	 * What is the DRM framebuffer ID of the framebuffer we're flipping
	 * away from?  We may need to restore this if one of our libdrm
	 * calls here fails and we don't actually flip.
	 */
	old_fbid = drmmode->fb_id;

	/* Get the GEM buffer for the backbuffer that we're flipping to */
	back_pixmap = intel_get_pixmap_private(dri2backpriv->pixmap);
	if (!back_pixmap || !back_pixmap->bo) {
		OS_ERROR("No EMGD private data for back buffer pixmap");
		return 0;
	}
	bo = back_pixmap->bo;

	/*
	 * Register the GEM buffer as a DRM framebuffer so that we get a
	 * handle that we can pass to KMS to flip to it.
	 */
	ret = drmModeAddFB(iptr->drm_fd, pScrn->virtualX, pScrn->virtualY,
		pScrn->depth, pScrn->bitsPerPixel, iptr->fb_pitch, bo->handle,
		&drmmode->fb_id);
	if (ret) {
		OS_ERROR("Failed to register DRM framebuffer");
		drmmode->fb_id = old_fbid;
		return 0;
	}

	/*
	 * Submit any batchbuffers that we've been building, but haven't submitted
	 * to the kernel yet.  This will allow GEM to ensure the batchbuffers are
	 * processed before the flip actually takes place.
	 */
	intel_batch_submit(pScrn);

	/*
	 * Call the pageflip ioctl on all CRTC's with the appropriate framebuffer
	 * offsets that each CRTC is covering.
	 *
	 * TODO:  This works for XRandR 1.3, but will need to be revisited with
	 * XRandR 1.4 when per-crtc pixmaps show up.
	 */
	for (i = 0; i < xf86_config->num_crtc; i++) {
		emgd_crtc = xf86_config->crtc[i]->driver_private;

		/* Ignore CRTC's that aren't in use */
		if (!xf86_config->crtc[i]->enabled) {
			continue;
		}

		ret = drmModePageFlip(iptr->drm_fd, emgd_crtc->crtc->crtc_id,
			drmmode->fb_id, DRM_MODE_PAGE_FLIP_EVENT, drmmode);
		if (ret) {
			OS_ERROR("Failed to queue pageflip request with DRM (%d)", ret);
			/*
			 * TODO:  Is it possible for a flip to succeed on CRTC 0, but
			 * fail on CRTC 1?  We should probably only remove the framebuffer
			 * if we haven't dispatched any successful flips yet.
			 */
			if (drmmode->flip_count == 0) {
				drmModeRmFB(iptr->drm_fd, drmmode->fb_id);
			}
			drmmode->fb_id = old_fbid;
			return 0;
		}

		/*
		 * Keep track of how many CRTC's are flipping to this new framebuffer
		 * so that we only deregister the old framebuffer after all of the
		 * CRTC's are done flipping.
		 */
		drmmode->flip_count++;
	}

	/*
	 * Record the old FB ID so that we can deregister it with the DRM when the
	 * flip is complete.
	 */
	drmmode->old_fb_id = old_fbid;

	/*
	 * Keep track of the swapinfo associated with this pending flip so that we
	 * can finish handling the swap request (waking up the client and such)
	 * when we get the flip done event.
	 */
	drmmode->dri2_swapinfo = swapinfo;

	return 1;
}
