/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_dri2.c
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 *-----------------------------------------------------------------------------
 * Description:
 *  EMGD DRI2 server-side implementation
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.dri
#include <xorg-server.h>
/*
 * Scheduled swaps should work fine now, but require proper vblank and
 * pageflip support in the kernel.  Commenting out this macro may help
 * if the DRM driver is known to be buggy, but will result in
 * unavoidable tearing (the equivalent of tearfb always being on).
 */
#define ALLOW_SWAPS

#define SPRITE_USE_VBLANK_FLIP

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <xf86drm.h>
#include <xf86Crtc.h>
#include <X11/Xatom.h>

#include "emgd.h"
#include "emgd_dri2.h"
#include "emgd_crtc.h"
#include "emgd_uxa.h"
#include "emgd_sprite.h"

static DEV_PRIVATE_KEY_TYPE dri2_client_key;

#ifdef ALLOW_SWAPS
static RESTYPE swap_record_drawable_resource;
static RESTYPE swap_record_client_resource;
#endif

static Atom sprite_usage_atom;
static Atom sprite_assignment_atom;

#ifdef CONFIG_DEBUG
static char* dri2_buffer_string(unsigned int type)
{
	switch (type) {
	case DRI2BufferFrontLeft:
		return "DRI2BufferFrontLeft";
	case DRI2BufferFrontRight:
		return "DRI2BufferFrontRight";
	case DRI2BufferFakeFrontLeft:
		return "DRI2BufferFakeFrontLeft";
	case DRI2BufferFakeFrontRight:
		return "DRI2BufferFakeFrontRight";
	case DRI2BufferBackLeft:
		return "DRI2BufferBackLeft";
	case DRI2BufferBackRight:
		return "DRI2BufferBackRight";
	case DRI2BufferAccum:
		return "DRI2BufferAccum";
	case DRI2BufferDepth:
		return "DRI2BufferDepth";
	case DRI2BufferDepthStencil:
		return "DRI2BufferDepthStencil";
	case DRI2BufferHiz:
		return "DRI2BufferHiz";
	default:
		return "UNKNOWN";
	}
}
#endif

/*
 * emgd_dri2_create_buffer()
 *
 * Given an X drawable, return a DRI2 buffer for a specific use with this
 * drawable (back buffer, fake front, real front, etc.).  The memory backing
 * a DRI2 buffer is allocated via the pixmap creation interface.
 */
static DRI2Buffer2Ptr emgd_dri2_create_buffer(DrawablePtr drawable,
		unsigned int attachment, unsigned int format)
{
	ScreenPtr screen = drawable->pScreen;
	DRI2Buffer2Ptr buffer = NULL;
	emgd_dri2_private_t *buffpriv = NULL;
	PixmapPtr pixmap = NULL;
	struct intel_pixmap *pixmap_priv = NULL;
	unsigned long usage_flag = INTEL_CREATE_PIXMAP_DRI2;
	int pixmap_width, pixmap_height, pixmap_cpp;

	OS_TRACE_ENTER;

	OS_DEBUG("DRI2 requested buffer of type %s", dri2_buffer_string(attachment));

	/* Allocate the general DRI2 buffer structure */
	buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		OS_ERROR("Failed to allocate DRI2 buffer");
		return NULL;
	}

	/* Allocate the EMGD private data for the DRI2 buffer */
	buffpriv = calloc(1, sizeof(*buffpriv));
	if (!buffpriv) {
		OS_ERROR("Failed to allocate DRI2 buffer private data");
		free(buffer);
		return NULL;
	}

	/*
	 * If we're being asked for the front buffer, then it's already been
	 * allocated as part of the normal X drawable creation and we just
	 * need to return it.
	 */
	if (attachment == DRI2BufferFrontLeft) {
		if (drawable->type == DRAWABLE_PIXMAP) {
			pixmap = (PixmapPtr)drawable;
		} else {
			pixmap = (*screen->GetWindowPixmap)((WindowPtr)drawable);
		}
	}

	/* Okay, now actually create the buffer requested */
	if (pixmap == NULL) {
		/*
		 * Get the width, height, and cpp from the drawable.  The one exception
		 * is the stencil buffer, which we override below.
		 */
		pixmap_width = drawable->width;
		pixmap_height = drawable->height;
		pixmap_cpp = format ? format : drawable->depth;

		/* Set pixmap creation hints based on type of buffer requested */
		switch (attachment) {
			case DRI2BufferFrontLeft:
				/*
				 * We handled front left above; we shouldn't be able to get
				 * here with pixmap still being NULL.
				 */
				OS_ERROR("Failed to return real front buffer for DRI2");
				free(buffpriv);
				free(buffer);
				return NULL;

			case DRI2BufferFrontRight:
			case DRI2BufferFakeFrontLeft:
			case DRI2BufferFakeFrontRight:
			case DRI2BufferBackLeft:
			case DRI2BufferBackRight:
			case DRI2BufferAccum:
				usage_flag |= INTEL_CREATE_PIXMAP_TILING_X;
				break;

			case DRI2BufferDepth:
			case DRI2BufferDepthStencil:
			case DRI2BufferHiz:
				usage_flag |= INTEL_CREATE_PIXMAP_TILING_Y;
				break;

			case DRI2BufferStencil:
				/*
				 * The stencil buffer has unusual pitch requirements.  It must
				 * be 2x the value computed based on the width since the
				 * stencil buffer is stored with two rows interleaved.  To make
				 * this actually happen at the kernel/GEM level, we double the
				 * CPP and halve the height.
				 */
				usage_flag |= INTEL_CREATE_PIXMAP_TILING_NONE;
				pixmap_width = ALIGN(pixmap_width, 64);
				pixmap_height = ALIGN((pixmap_height + 1) / 2, 64);
				pixmap_cpp *= 2;
				break;

			default:
				OS_ERROR("Unknown DRI2 buffer creation request");
				free(buffpriv);
				free(buffer);
				return NULL;
		}

		/* Okay, create the pixmap with the settings decided above */
		pixmap = screen->CreatePixmap(screen, pixmap_width, pixmap_height,
			pixmap_cpp, usage_flag);
		if (!pixmap) {
			OS_ERROR("Pixmap allocation failed for DRI2 buffer");
			free(buffpriv);
			free(buffer);
			return NULL;
		}
	}

	/* Did we get a valid GEM-based pixmap? */
	pixmap_priv = intel_get_pixmap_private(pixmap);
	if (!pixmap_priv || !pixmap_priv->bo) {
		OS_ERROR("Failed to allocate GEM bo for DRI2 buffer");
		if (attachment != DRI2BufferFrontLeft) {
			screen->DestroyPixmap(pixmap);
		}
		free(buffpriv);
		free(buffer);
		return NULL;
	}

	/* Create a unique global buffer name */
	if (drm_intel_bo_flink(pixmap_priv->bo, &buffer->name) != 0) {
		OS_ERROR("Failed to generate unique name for DRI2 buffer");
		if (attachment != DRI2BufferFrontLeft) {
			screen->DestroyPixmap(pixmap);
		}
		free(buffpriv);
		free(buffer);
		return NULL;
	}

	/* Fill out the DRI2 buffer structure */
	buffer->attachment = attachment;
	buffer->pitch = pixmap->devKind;
	buffer->cpp = pixmap->drawable.bitsPerPixel / 8;
	buffer->format = format;
	buffer->flags = 0;

	/* Fill out the EMGD private buffer structure */
	buffpriv->refcnt = 1;
	buffpriv->pixmap = pixmap;

	/* Link the generic DRI buffer structure and the driver-specific structure */
	buffer->driverPrivate = buffpriv;
	buffpriv->buffer = buffer;

	OS_DEBUG(" --> returned %s", dri2_buffer_string(attachment));

	OS_TRACE_EXIT;

	return buffer;
}

/*
 * emgd_dri2_destroy_buffer()
 *
 * Destroy a DRI2 buffer and all driver-private structures associated with it.
 */
void emgd_dri2_destroy_buffer(DrawablePtr drawable,
		DRI2Buffer2Ptr buffer)
{
	emgd_dri2_private_t *buffpriv;

	OS_TRACE_ENTER;

	if (!buffer) {
		return;
	}

	buffpriv = buffer->driverPrivate;
	if (!buffpriv) {
		free(buffer);
		return;
	}

	/*
	 * Decrement reference count.  If 0, destroy the underlying pixmap and free
	 * the buffer structures.
	 */
	buffpriv->refcnt--;
	if (buffpriv->refcnt == 0) {
		if (buffer->attachment != DRI2BufferFrontLeft) {
			buffpriv->pixmap->drawable.pScreen->DestroyPixmap(buffpriv->pixmap);
		}
		free(buffpriv);
		free(buffer);
	}

	OS_TRACE_EXIT;

	return;
}


/*
 * emgd_dri2_copy_region()
 *
 * Copies part or all of one of a drawable's DRI2 buffers to another
 * DRI2 buffer (e.g., back -> front or fake front -> front).  This interface
 * performs an immediate "blit" without waiting for vblank.  The
 * emgd_dri2_schedule_swap() can now be used for scheduled swaps (which
 * allows for both "blit" and "flip" types of swaps).
 */
static void emgd_dri2_copy_region(DrawablePtr drawable, RegionPtr region,
		DRI2BufferPtr dst_buf, DRI2BufferPtr src_buf)
{
	ScreenPtr screen = drawable->pScreen;
	emgd_dri2_private_t* dst_priv = dst_buf->driverPrivate;
	emgd_dri2_private_t* src_priv = src_buf->driverPrivate;
	RegionPtr copy_clip;
	GCPtr gc;
	DrawablePtr dstdraw = NULL;
	DrawablePtr srcdraw = NULL;

	/* Get the underlying pixmap for each DRI2 buffer */
	srcdraw = (src_buf->attachment == DRI2BufferFrontLeft) ?
		drawable :
		&src_priv->pixmap->drawable;
	dstdraw = (dst_buf->attachment == DRI2BufferFrontLeft) ?
		drawable :
		&dst_priv->pixmap->drawable;

	/*
	 * DRI2 buffers should always be associated with drawables.  The following
	 * should never happen.
	 */
	if (dstdraw == NULL || srcdraw == NULL) {
		OS_ERROR("[dri] Invalid buffer (Source:%u Destination:%u)",
			src_buf->attachment, dst_buf->attachment);
		return;
	}

	gc = GetScratchGC(drawable->depth, screen);
	if (!gc) {
		OS_ERROR("Failed to get scratch GC");
		return;
	}

	/* Clip the desired copy region to the dimensions of the drawable */
	copy_clip = REGION_CREATE(screen, NULL, 0);
	REGION_COPY(screen, copy_clip, region);
	gc->funcs->ChangeClip(gc, CT_REGION, copy_clip, 0);
	ValidateGC(dstdraw, gc);

	/*
	 * OTC waits for the scanline here to prevent tearing even in windowed
	 * mode.  We may want to do the same.
	 */

	/*
	 * OTC also has some extra logic to handle shadowfb here.  We may want to
	 * investigate further.
	 */

	/* Perform the requested copy */
	(gc->ops->CopyArea)(srcdraw, dstdraw, gc, 0, 0,
		drawable->width, drawable->height, 0, 0);

	FreeScratchGC(gc);
}


#ifdef ALLOW_SWAPS
/*
 * crtc_for_drawable()
 *
 * Returns the display (CRTC number) that this drawable is displayed on
 * (or mostly displayed on in cases where it spans displays).
 */
int crtc_for_drawable(ScrnInfoPtr scrn, DrawablePtr drawable)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr crtc;
	BoxRec drawable_box, *overlap_box;
	RegionRec drawable_region, crtc_region, overlap_region;
	unsigned long area, bestarea, bestcrtc;
	int i;

	/* Initialize box with drawable's size/location */
	drawable_box.x1 = drawable->x;
	drawable_box.y1 = drawable->y;
	drawable_box.x2 = drawable->x + drawable->width;
	drawable_box.y2 = drawable->y + drawable->height;
	RegionInit(&drawable_region, &drawable_box, 0);

	/* Check each CRTC to see how much of its area overlaps the drawable */
	RegionInit(&overlap_region, NullBox, 0);
	bestarea = 0;
	bestcrtc = 0;
	for (i = 0; i < xf86_config->num_crtc; i++) {
		crtc = xf86_config->crtc[i];
		RegionInit(&crtc_region, &crtc->bounds, 0);
		RegionIntersect(&overlap_region, &crtc_region, &drawable_region);
		overlap_box = RegionExtents(&overlap_region);
		area = (overlap_box->x2 - overlap_box->x1) *
			   (overlap_box->y2 - overlap_box->y1);
		if (area > bestarea) {
			bestarea = area;
			bestcrtc = i;
		}
		RegionUninit(&crtc_region);
	}
	RegionUninit(&overlap_region);
	RegionUninit(&drawable_region);

	return (bestarea > 0) ? bestcrtc : -1;
}


/*
 * emgd_dri2_get_msc()
 *
 * GetMSC() handler.  (MSC = media stamp counter)
 *
 * Returns the current frame counter and the timestamp of when the frame count
 * was last incremented.  See dri2.h in the X server source code for a more
 * complete explanation.
 *
 * Returns true on success, false on failure.
 */
static int emgd_dri2_get_msc(DrawablePtr drawable, CARD64 *ust, CARD64 *msc)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrninfo);
	drmVBlank vbl;
	int ret, crtcnum;

	crtcnum = crtc_for_drawable(scrninfo, drawable);

	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (crtcnum > 0) {
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	}
	vbl.request.sequence = 0;
	ret = drmWaitVBlank(iptr->drm_fd, &vbl);
	if (ret) {
		OS_ERROR("Failed to get vblank counter: %s\n", strerror(errno));
		return FALSE;
	}

	/* We got the response.  Update the timestamp and counter */
	*ust = ((CARD64)vbl.reply.tval_sec * 1000000) + vbl.reply.tval_usec;
	*msc = vbl.reply.sequence;

	return 1;
}


/*
 * Returns the driver resource identified by the specified id, or allocates
 * the resource if it does not already exist.
 */
static struct emgd_resource* lookup_emgd_resource(XID id, RESTYPE type)
{
	struct emgd_resource *res;
	void *data = NULL;

	dixLookupResourceByType(&data, id, type, NULL, DixWriteAccess);
	if (data) {
		return data;
	}

	/* Resource doesn't exist yet; create and register it */
	res = malloc(sizeof(*res));
	if (!res) {
		OS_ERROR("Failed to allocate resource");
		return NULL;
	}

	/* Add resource to X server database */
	if (!AddResource(id, type, res)) {
		free(res);
		return NULL;
	}

	res->id = id;
	res->type = type;
	LIST_INIT(&res->list);

	return res;
}


/*
 * Returns the client XID for a ClientPtr.
 */
static XID get_client_id(ClientPtr client)
{
	XID *ptr = dixLookupPrivate(&client->devPrivates, &dri2_client_key);
	if (*ptr == 0) {
		*ptr = FakeClientID(client->index);
	}

	return *ptr;
}



/*
 * register_swapinfo()
 *
 * Registers the swap info structure as an X server resource so that it will
 * get cleaned up properly if the client exists while the swap is pending or
 * if the drawable is otherwise destroyed.
 */
static int register_swapinfo(emgd_dri2_swap_record_t *swapinfo)
{
	struct emgd_resource *clientres, *drawableres;

	/* Hook swap record to X client */
	clientres = lookup_emgd_resource(get_client_id(swapinfo->client),
		swap_record_client_resource);
	if (!clientres) {
		/* Failed to register swap record as a client resource */
		return 0;
	}
	LIST_ADD(&swapinfo->client_resource, &clientres->list);

	/* Hook swap record to X drawable */
	drawableres = lookup_emgd_resource(swapinfo->drawable_id,
		swap_record_drawable_resource);
	if (!drawableres) {
		/*
		 * Failed to register swap record as a drawable resource.
		 * Remove it from the client resource list as well.
		 */
		LIST_DEL(&swapinfo->client_resource);
		return 0;
	}
	LIST_ADD(&swapinfo->drawable_resource, &drawableres->list);

	return 1;
}


/*
 * emgd_dri2_swap_record_client_gone()
 *
 * Called when an X client disconnects.  Allows us to remove the reference
 * to the client from any in-flight swap records so that we don't try to
 * use it later and crash.
 */
static int emgd_dri2_swap_record_client_gone(void *data, XID id)
{
	struct emgd_resource *res = data;
	emgd_dri2_swap_record_t *swapinfo = NULL, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(swapinfo, tmp, &res->list, client_resource) {
		/*
		 * Remove reference to now-invalid client and the callback that
		 * we had been planning to call.
		 */
		swapinfo->client = NULL;
		swapinfo->callback = NULL;
		swapinfo->callback_data = NULL;

		/* Remove swapinfo from list */
		LIST_DEL(&swapinfo->client_resource);
	}
	free(res);

	return 0;
}


/*
 * emgd_dri2_swap_record_drawable_gone()
 *
 * Called when an X drawable goes away.  Allows us to remove the reference
 * to the drawable from any in-flight swap records so that we don't try
 * to access it later and crash.
 */
static int emgd_dri2_swap_record_drawable_gone(void *data, XID id)
{
	struct emgd_resource *res = data;
	emgd_dri2_swap_record_t *swapinfo, *tmp;
        DrawablePtr drawable = NULL;
	ScreenPtr screen = NULL;
	emgd_priv_t *iptr = NULL;
	emgd_sprite_t *plane = NULL;
	ClientPtr client = NULL;

	LIST_FOR_EACH_ENTRY_SAFE(swapinfo, tmp, &res->list, drawable_resource) {
		/* Remove reference to now-invalid drawable */
		swapinfo->drawable_id = 0;

		/* Remove swapinfo from list */
		LIST_DEL(&swapinfo->drawable_resource);
	}
	
	/* Check if the drawable owns a sprite plane, if it does we need to turn it off
	 * and ungrab it
	 */
	dixLookupDrawable(&drawable, id, serverClient, M_ANY, DixWriteAccess);
	if (!drawable) {
		OS_DEBUG("No drawable found for swap_record_drawable_gone");
		goto exit_free_res;
	}

	dixLookupClient(&client, id, serverClient, DixReadAccess);
	if(!client){
		OS_DEBUG("No client found for swap_record_drawable_gone");
	}
	
	
	screen = drawable->pScreen;
	iptr = EMGDPTR(xf86Screens[screen->myNum]);

        /* Go thru the entire list of known sprites */
        LIST_FOR_EACH_ENTRY(plane, &iptr->sprite_planes, link) {
		if (plane->curr_state.drawable == drawable) {
			/* Found the sprite plane this drawable owns */
			emgd_sprite_disable_core(iptr, plane);
			emgd_sprite_ungrab(drawable->pScreen, iptr, plane, client, IGD_GRAB_FOR_XV_ON);

			if (plane->flip_buffer1) {
				/* Destroy the flip buffer allocated */
				emgd_sprite_destroy_flip_bo(plane->flip_buffer1);
				free(plane->flip_buffer1);
				plane->flip_buffer1 = NULL;
			}
			
			if (plane->flip_buffer2) {
				emgd_sprite_destroy_flip_bo(plane->flip_buffer2);
				free(plane->flip_buffer2);
				plane->flip_buffer2 = NULL;
			}
		}
	}

exit_free_res:
	free(res);

	return 0;
}



/*
 * flip_possible()
 *
 * Determines whether it's possible to satisfy a swap request via a flip
 * or whether we need to blit.
 */
static int flip_possible(emgd_priv_t *iptr,
	DrawablePtr drawable,
	DRI2BufferPtr front,
	DRI2BufferPtr back)
{
	/* EMGD DRI2 buffer privates */
	emgd_dri2_private_t *front_priv = front->driverPrivate;
	emgd_dri2_private_t *back_priv  = back->driverPrivate;

	/* Underlying pixmaps for DRI2 buffers */
	PixmapPtr front_pixmap = front_priv->pixmap;
	PixmapPtr back_pixmap = back_priv->pixmap;

	/* Can the drawable itself be flipped? */
	if (!drawable || !DRI2CanFlip(drawable)) {
		return 0;
	}

	/*
	 * If we're running rotated (i.e., we have a shadowfb), we can't flip since
	 * out buffers would be, for example, 768x1024 while our real scanout would
	 * be 1024x768.
	 */
	if (iptr->shadow_present) {
		return 0;
	}

	/* Do the front and backbuffer dimensions match? */
	if (front_pixmap->drawable.width != back_pixmap->drawable.width ||
		front_pixmap->drawable.height != back_pixmap->drawable.height ||
		front_pixmap->drawable.bitsPerPixel != back_pixmap->drawable.bitsPerPixel)
	{
		return 0;
	}

	return 1;
}


/*
 * emgd_dri2_flip_complete()
 *
 * Callback that executes when the pageflip operation has been completed
 * across all CRTC's by the kernel.  At this point we should notify
 * the client that the swap operation is complete.
 */
void emgd_dri2_flip_complete(unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	emgd_dri2_swap_record_t *swapinfo)
{
	DrawablePtr drawable = NULL;

	/* Lookup the drawable from its XID */
	dixLookupDrawable(&drawable, swapinfo->drawable_id, serverClient,
		M_ANY, DixWriteAccess);

	/*
	 * Make sure the drawable hasn't been destroyed while we were
	 * waiting for the flip to complete.
	 */
	if (!drawable) {
		OS_DEBUG("No drawable found for flip completion");
		return;
	}

	/*
	 * Sanity check: we should only get to this point for flip-based swaps.
	 * Scheduled blits should never get here.
	 */
	if (swapinfo->type != SWAP_FLIP) {
		OS_ERROR("Flip handler trying to process blit-based swap");
		return;
	}

	/*
	 * Sanity check: MSC at flip completion should not be lower than MSC at
	 * flip submission (except in wraparound situations).
	 */
	if ((frame < swapinfo->framenum) &&
		(swapinfo->framenum - frame < WRAP_THRESH))
	{
		OS_ERROR("Swap completion has lower MSC than swap submission");

		/* Timestamps are screwed up; just give client 0's for everything */
		frame = tv_sec = tv_usec = 0;
	}

	/* Send the swap completion event to the DRI2 client process */
	DRI2SwapComplete(swapinfo->client, drawable, frame, tv_sec, tv_usec,
		DRI2_FLIP_COMPLETE, swapinfo->callback, swapinfo->callback_data);
	
        /* Unlink resources */
        LIST_DEL(&swapinfo->client_resource);
        LIST_DEL(&swapinfo->drawable_resource);

        /* Remove reference to front and back buffers */
        if (swapinfo->front) {
                emgd_dri2_destroy_buffer(drawable, swapinfo->front);
        }
        if (swapinfo->back) {
                emgd_dri2_destroy_buffer(drawable, swapinfo->back);
        }

	/* Free swapinfo */
        free(swapinfo);
}


/*
 * emgd_dri2_exchange_buffers
 *
 * Given front and back DRI2 buffers, swaps the GEM bo's they point
 * at (which we need to do following a pageflip) so that future
 * rendering and scanout use the correct buffers.
 */
static void emgd_dri2_exchange_buffers(DrawablePtr drawable,
	DRI2BufferPtr front, DRI2BufferPtr back)
{
	ScreenPtr screen = drawable->pScreen;
	emgd_priv_t *iptr = EMGDPTR(xf86Screens[screen->myNum]);
	emgd_dri2_private_t *frontpriv, *backpriv;
	emgd_pixmap_t *frontpixmap, *backpixmap;
	drm_intel_bo *new_front_bo;
	int tmp;

	frontpriv = front->driverPrivate;
	backpriv = back->driverPrivate;
	frontpixmap = intel_get_pixmap_private(frontpriv->pixmap);
	backpixmap = intel_get_pixmap_private(backpriv->pixmap);

	/* Swap GEM bo lookup handles stored in the DRI2 buffer privates */
	tmp = front->name;
	front->name = back->name;
	back->name = tmp;

	/*
	 * The GEM bo associated with the current backbuffer is the bo that is
	 * becoming the new front (scanout).
	 */
	new_front_bo = backpixmap->bo;

	/*
	 * Swap the pixmap private data for the front and back pixmaps.  This
	 * essentially switches which GEM bo and DRM framebuffer are associated
	 * with the front and back buffers.  In addition to the two pixmaps that
	 * back the front and back DRI2 buffers, we also need to ensure that the
	 * screen pixmap is also updated to point at the new front bo.
	 */
	intel_set_pixmap_private(frontpriv->pixmap, backpixmap);
	intel_set_pixmap_private(backpriv->pixmap, frontpixmap);
	intel_set_pixmap_private(screen->GetScreenPixmap(screen), backpixmap);

	/*
	 * The iptr also hold a reference to the GEM bo that is our scanout.  At
	 * the moment it's still pointing to the bo that we just flipped away from.
	 * Switch it to the new front buffer's bo.  Note that the iptr's back
	 * buffer (which will only be different in the case of shadowfb) is
	 * unaffected by any of this so we don't need to touch it.
	 */
	drm_intel_bo_unreference(iptr->front_buffer);
	iptr->front_buffer = new_front_bo;
	drm_intel_bo_reference(iptr->front_buffer);

	/*
	 * Finally, update the busy status of both of the DRI2 buffer pixmaps.
	 * backpixmap (which is the new front buffer) is definitely busy being used
	 * as a scanout.  frontpixmap (which is the old frontbuffer that we're
	 * flipping away from) * will be busy until the flip actually completes so
	 * we set it to "-1" as a way of saying "ask the DRM if this is still busy."
	 * The busy status of a pixmap affects the get_image and put_image UXA
	 * operations; see code in emgd_uxa.c for details.
	 */
	backpixmap->busy = 1;
	frontpixmap->busy = -1;
}


/*
 * emgd_dri2_vblank_handler()
 *
 * DRI2-specific handling of vblank events.  Scheduled swaps that we can't
 * flip on need to be blitted here.  We might also postpone the actual
 * dispatch of a flip to the DRM, in which case we need to actually schedule
 * the flip here.
 */
void emgd_dri2_vblank_handler(unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	emgd_dri2_swap_record_t *swapinfo)
{
	DrawablePtr drawable = NULL;
	BoxRec blitbox;
	RegionRec blitregion;
	int ret;
	emgd_sprite_t *(planes[2]);

	/*
	 * Lookup the drawable from its XID (and make sure it hasn't been destroyed
	 * since we asked for the vblank event).
	 */
	if (!swapinfo->drawable_id) {
		/* Drawable was destroyed before vblank event came in. */
		drawable = NULL;
	} else if (dixLookupDrawable(&drawable, swapinfo->drawable_id, serverClient,
			M_ANY, DixWriteAccess)) {
		/* Shouldn't be possible, but fail if the ID doesn't seem valid */
		drawable = NULL;
	}

	if (!drawable) {
		OS_DEBUG("No drawable found for swap completion");
		goto vblank_del_swapinfo;
	}

	planes[0] = swapinfo->plane_pointer[0];
	planes[1] = swapinfo->plane_pointer[1];

	/* What were we waiting to do?  Blit or flip? */
	switch (swapinfo->type) {
	case SWAP_FLIP:
#ifdef SPRITE_USE_VBLANK_FLIP
		if (NULL == planes[0]) {
#endif
			/* Is a flip still possible? */
			if (flip_possible(swapinfo->iptr, drawable, swapinfo->front,
				swapinfo->back))
			{
				ret = emgd_crtc_schedule_flip(swapinfo);
				if (ret) {
					/*
					 * Flip scheduled successfully with DRM.  Exchange the front
					 * and back buffers.
					 */
					emgd_dri2_exchange_buffers(drawable, swapinfo->front,
						swapinfo->back);
					return;
				}
			}
			/* Failure to flip will fall through to the blit case */
#ifdef SPRITE_USE_VBLANK_FLIP
		} else {

			/* This is for sprite flip */
			ret = emgd_dri2_sprite_flip_wrapper(swapinfo->client, drawable, 
				swapinfo->back, planes);
			if (ret) {
				/*
				 * Flip successfully. Rotate the flip buffer objects
				 * BUT if sprite's plane was disabled we should not do anything
				 */
				if (planes[0]->curr_state.enabled != 0) {
					emgd_sprite_exchange_flip_bo(drawable, swapinfo->back,
						planes[0]->flip_buffer1, planes[0]->flip_buffer2);

					/* Release the old framebuffer
					 * But for the first sprite flip(!) there is no old frame buffer yet
					 * So, do delete FB if old_fb_id is not 0 only*/
					if (planes[0]->curr_state.old_fb_id != 0){
						drmModeRmFB(swapinfo->iptr->drm_fd, 
							planes[0]->curr_state.old_fb_id);
					}
				}

				DRI2SwapComplete(swapinfo->client, drawable, frame, tv_sec, tv_usec,
					DRI2_FLIP_COMPLETE, swapinfo->callback, swapinfo->callback_data);

				break;
			}
			 /* Failure to flip will fall through to the blit case */
		}
#endif

	case SWAP_BLIT:
		/* Blit region is the full size of the drawable */
		blitbox.x1 = 0;
		blitbox.y1 = 0;
		blitbox.x2 = drawable->width;
		blitbox.y2 = drawable->height;
		REGION_INIT(screen, &blitregion, &blitbox, 0);

		/* Blit back -> front */
		if (swapinfo->front && swapinfo->back) {
			emgd_dri2_copy_region(drawable, &blitregion, swapinfo->front,
				swapinfo->back);
		}

		/* Notify client that swap is complete */
		DRI2SwapComplete(swapinfo->client, drawable, frame, tv_sec, tv_usec,
			DRI2_BLIT_COMPLETE, swapinfo->callback, swapinfo->callback_data);
		break;

	case SWAP_WAIT:
		/*
		 * We're not really swapping, this was just a 'WaitMSC' request.
		 * Tell the client that we've reached the target MSC.
		 */
		if (swapinfo->client) {
			DRI2WaitMSCComplete(swapinfo->client, drawable, frame, tv_sec, tv_usec);
		}
		break;

	default:
		OS_ERROR("DRI2 vblank handler got unknown swap type");
		break;
	}

vblank_del_swapinfo:
	/* Unlink resources */
	LIST_DEL(&swapinfo->client_resource);
	LIST_DEL(&swapinfo->drawable_resource);

	/* Remove reference to front and back buffers */
	if (swapinfo->front) {
		emgd_dri2_destroy_buffer(drawable, swapinfo->front);
	}
	if (swapinfo->back) {
		emgd_dri2_destroy_buffer(drawable, swapinfo->back);
	}

	free(swapinfo);
}


/*
 * emgd_dri2_schedule_swap()
 *
 * Schedules a buffer swap for a future vblank event.  The handling here
 * is a bit hard to follow, but this is pretty much the exact same logic
 * used by other DDX drivers (well, at least i915 and radeon; nouveau is
 * slightly different).  Depending on the situation, this swap might be
 * handled via the DRM pageflip ioctl, via a blit, or via a simple buffer
 * exchange.  Note that pageflipping incurs an extra frame of delay (when
 * the target MSC comes up, you queue the flip request and it doesn't
 * actually happen until the next vblank), so there's additional logic
 * here to adjust the target MSC to take this delay into account.
 *
 * If scheduling the swap fails for any reason, we'll fall back to doing
 * an immediate blit (without waiting for vblank).
 */
static int emgd_dri2_schedule_swap(ClientPtr client, DrawablePtr drawable,
	DRI2BufferPtr front, DRI2BufferPtr back, CARD64 *target_msc,
	CARD64 divisor, CARD64 remainder, DRI2SwapEventPtr callback, void *cbdata)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrninfo);
	drmVBlank vbl;
	emgd_dri2_swap_record_t *swapinfo = NULL;
	WindowPtr drawwin;
	PropertyPtr winprop;
	CARD64 current_msc;
	int ret, crtcnum, isflip = 0;
	int  isSprite = 0;
	int spriteAssignmentRequest = SPRITE_PREASSIGNED_NONE;
	emgd_sprite_t *(planes[2]);

	/* Used by blit fallback in case we can't flip */
	BoxRec blitbox;
	RegionRec blitregion;

	/* Used to retrieve display rotation status */
	int display_rotation = RR_Rotate_0; 
	int mode, num_planes;

	/* Used to check property in extended mode */
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);
	emgd_sprite_t *tmp_sprite = NULL;
	emgd_crtc_priv_t *emgd_crtc =NULL;
	


	/*
	 * If the drawable isn't visible on any CRTC, then just blit immediately
	 * since we don't have a CRTC's vblank that we want to schedule with.
	 */
	crtcnum = crtc_for_drawable(scrninfo, drawable);
	if (crtcnum < 0) {
		goto blit_fallback;
	}

	/*
	 * Embedded "OGL on sprite" feature.
	 *
	 * TODO: We should probably figure out sprite availability *before* looking
	 * for the window property since this will just be wasted work if the
	 * sprites are already tied up.
	 */
	if (drawable->type == DRAWABLE_WINDOW) {
		drawwin = (WindowPtr)drawable;

		/* Get user property list for window */
		if (drawwin->optional) {
			winprop = drawwin->optional->userProps;
		} else {
			winprop = NULL;
		}

		/*
		 * Walk over all properties, looking for the EMGD "use sprite"
		 * window property.
		 */
		while (winprop) {
			if (winprop->propertyName == sprite_usage_atom &&
				winprop->type == XA_STRING &&
				winprop->format == 8 &&
				winprop->data)
			{
				/*
				 * Okay, we found a window destined for a sprite plane rather
				 * than the framebuffer.  It might make sense to cache this
				 * in the drawable private so that we don't have to walk
				 * a (potentially long) linked list of properties on every
				 * flip.
				 *
				 * TODO:  Actually flip this window onto the sprite rather
				 * than the FB.
				 */
				OS_DEBUG("Found 'use sprite' window property with value '%s'",
					(char*)winprop->data);

				/* Flip to Sprite request is detected */
				isSprite = atoi((char*)winprop->data);
			}

			if (winprop->propertyName == sprite_assignment_atom &&
				winprop->type == XA_STRING &&
				winprop->format == 8 &&
				winprop->data)
			{
				/*
				 * Okay, we found a window trying to assign itself to a
				 * specific sprite plane. This has NO-IMPACT if the
				 * client didnt set the other XDrawable property
				 * "EMGD_USE_SPRITE" to even request for sprites.
				 */
				OS_DEBUG("Found 'assign_sprite' window property with value '%s'",
					(char*)winprop->data);
				/* Set the Sprite assignment request value*/
				spriteAssignmentRequest = atoi((char*)winprop->data);
				OS_DEBUG("Translated 'assign_sprite' value = %d",
						spriteAssignmentRequest);
			}

			winprop = winprop->next;
		}
	}


	/*
	 * If the buffer is going to sprite, we will skip flip_possible checking
	 * since that is "flippable".
	 *
	 * If we decide we can't actually flip, and if TearFB is on, then we
	 * shouldn't bother with a scheduled blit...we should just call
	 * the CopyRegion fallback immediately to get the content to the screen
	 * ASAP.
	 *
	 * We should do this before registering a swapinfo structure so that
	 * we don't waste as much time registering resources and then immediately
	 * turning around and unregistering them.
	 */

	if (!isSprite) {
		if (!flip_possible(iptr, drawable, front, back) || iptr->cfg.tear_fb) {
			goto blit_fallback;
		}
	}

	/*
	 * If this is a pixmap, just blit immediately (we don't care about syncing
	 * to vblank for offscreen surfaces).
	 */
	if (drawable->type == DRAWABLE_PIXMAP) {
		goto blit_fallback;
	}

	/*
	 * Truncate inputs to match kernel interfaces.  This will result in
	 * occasional overflow misses, but that's generally considered acceptable
	 * (other vendors' DDX's do the same thing).
	 */
	*target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	/* If isSPrite is set, we flip the back buffer to sprite plane then return */
	if (isSprite) {
		
		/* Fallback to blit if in vt console mode */
		if(scrninfo->vtSema == 0){
			OS_ERROR("Failed to get a sprite plane in console mode");
			goto blit_fallback;
		}

		emgd_sprite_check_mode(drawable, &mode, &num_planes);

		
		/* check wheather that is same windows property is been using for two
		 * sprite in extended mode
		 */
		if(mode == SPRITE_MODE_EXTENDED){
			emgd_crtc = xf86_config->crtc[crtcnum]->driver_private;
			LIST_FOR_EACH_ENTRY(tmp_sprite, &iptr->sprite_planes, link) {
				if((tmp_sprite->curr_state.drawable == drawable) && 
				   (tmp_sprite->crtc_id != emgd_crtc->crtc->crtc_id)){
					emgd_sprite_disable_core(iptr, tmp_sprite);
					emgd_sprite_ungrab(drawable->pScreen, iptr,
					   tmp_sprite, client, IGD_GRAB_FOR_XV_ON);
				}
			}
		}

		/* Get a sprite plane */
		ret = emgd_sprite_get_plane(iptr, drawable, spriteAssignmentRequest, 
			planes, client, IGD_GRAB_FOR_XV_ON);
		if (!ret) {
			OS_ERROR("Failed to get a sprite plane");
			goto blit_fallback;
		}


        	/* Fallback to blit if the display is rotate to 90, 180, 270 */
        	display_rotation = emgd_sprite_check_display_rotation_status(drawable, iptr, crtcnum, mode);

        	if((display_rotation == RR_Rotate_90) || 
           	   (display_rotation == RR_Rotate_180)|| 
           	   (display_rotation == RR_Rotate_270)) {

			OS_DEBUG("\nFallback to blit display rotate to 90, 180 and 270");
			emgd_sprite_disable_core(iptr, planes[0]);
			emgd_sprite_ungrab(drawable->pScreen, iptr, planes[0], client, IGD_GRAB_FOR_XV_ON);
			if(planes[1] != NULL){
				emgd_sprite_disable_core(iptr, planes[1]);
				emgd_sprite_ungrab(drawable->pScreen, iptr, planes[1], client, IGD_GRAB_FOR_XV_ON);
			}
            		goto blit_fallback;
		}


		ret = emgd_sprite_allocate_tripple_buffer(drawable, back, planes[0]);
		if (!ret) {
			OS_ERROR("Failed to allocate buffers for tripple buffering");
			goto blit_fallback;
		}

		/* Allocate a record of the swap request */
		swapinfo = calloc(1, sizeof(*swapinfo));
		if (!swapinfo) {
			OS_ERROR("Failed to allocate swap record");
			goto blit_fallback;
		}

		swapinfo->iptr = iptr;
		swapinfo->drawable_id = drawable->id;
		swapinfo->client = client;
		swapinfo->crtc = crtcnum;
		swapinfo->callback = callback;
		swapinfo->callback_data = cbdata;
		swapinfo->front = front; 
		swapinfo->back = back;
		swapinfo->plane_pointer[0] = planes[0];
		swapinfo->plane_pointer[1] = planes[1];

		/*
		 * The swap record hold an additional reference to the DRI2 buffer (which
		 * will be deleted when the swap is complete).  Increment the reference
		 * count here to ensure that we don't destroy the buffer too early.
		 */
		((emgd_dri2_private_t*)front->driverPrivate)->refcnt++;
		((emgd_dri2_private_t*)back->driverPrivate)->refcnt++;

		if (!register_swapinfo(swapinfo)) {
			/*
			 * Shouldn't be possible (couldn't create X resource for some reason),
			 * but fallback to immediate blit if we can't create a swapinfo
			 * structure for some reason.
			 */
			OS_ERROR("Failed to register swapinfo record");
			goto blit_fallback_cleanup_swapinfo;
		}
#ifdef SPRITE_USE_VBLANK_FLIP
		/* Get the current MSC */
		vbl.request.type = DRM_VBLANK_RELATIVE;
		if (crtcnum > 0) {
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		}
		vbl.request.sequence = 0;
		ret = drmWaitVBlank(iptr->drm_fd, &vbl);
		if (ret) {
			OS_ERROR("Failed to get current MSC: %s", strerror(errno));
			goto blit_fallback_cleanup_swapinfo;
		}
		current_msc = vbl.reply.sequence;

		swapinfo->type = SWAP_FLIP;
		
		vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT | DRM_VBLANK_NEXTONMISS;
		if (crtcnum > 0) {
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		}

		/*
		 * Target MSC for flips are one vblank earlier than for blits since we
		 * need to queue the flip and then wait for the following vblank for it
		 * to actually take effect (whereas blits happen immediately when
		 * performed).
		 */
		if (*target_msc > 0) {
			(*target_msc)--;
		}

		/*
		 * If we've already reached the target MSC, set it to the current
		 * MSC.  This ensures the caller gets a reasonable value back.
	 	 */
		if (current_msc >= *target_msc) {
			*target_msc = current_msc;
		}

		vbl.request.sequence = *target_msc;
		vbl.request.signal = (unsigned long)swapinfo;
		ret = drmWaitVBlank(iptr->drm_fd, &vbl);
		if (ret) {
			OS_ERROR("Failed to get vblank counter: %s", strerror(errno));
			goto blit_fallback_cleanup_swapinfo;
		}
#else
		ret = emgd_dri2_sprite_flip_wrapper(client, drawable, back, 
			planes, swapinfo);
		if (!ret) {
			OS_ERROR("Failed to flip window onto the sprite");
			/*
			 * Shall we return with error or fallback to blit?
			 * Fallback to blit for now.
			 */
			goto blit_fallback_cleanup_swapinfo;
		}

		/* Exchange the current back buffer with the fresh bo */
		emgd_sprite_exchange_flip_bo(drawable, back, planes[0]->flip_buffer1, 
			planes[0]->flip_buffer2);
#endif
			
		return 1;
	} else {		/* Regular page flip */
		/* Allocate a record of the swap request */
		swapinfo = calloc(1, sizeof(*swapinfo));
		if (!swapinfo) {
			goto blit_fallback;
		}

		swapinfo->iptr = iptr;
		swapinfo->drawable_id = drawable->id;
		swapinfo->client = client;
		swapinfo->crtc = crtcnum;
		swapinfo->callback = callback;
		swapinfo->callback_data = cbdata;
		swapinfo->back = back;
		swapinfo->front = front;
		/*
		 * The swap record hold an additional reference to the DRI2 buffer (which
		 * will be deleted when the swap is complete).  Increment the reference
		 * count here to ensure that we don't destroy the buffer too early.
		 */
		((emgd_dri2_private_t*)front->driverPrivate)->refcnt++;
		((emgd_dri2_private_t*)back->driverPrivate)->refcnt++;

		if (!register_swapinfo(swapinfo)) {
			/*
			 * Shouldn't be possible (couldn't create X resource for some reason),
			 * but fallback to immediate blit if we can't create a swapinfo
			 * structure for some reason.
			 */
			OS_ERROR("Failed to register swapinfo record");
			goto blit_fallback_cleanup_swapinfo;
		}

		/* Get the current MSC */
		vbl.request.type = DRM_VBLANK_RELATIVE;
		if (crtcnum > 0) {
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		}
		vbl.request.sequence = 0;
		ret = drmWaitVBlank(iptr->drm_fd, &vbl);
		if (ret) {
			OS_ERROR("Failed to get current MSC: %s", strerror(errno));
			goto blit_fallback_cleanup_swapinfo;
		}
		current_msc = vbl.reply.sequence;

		/* Can we use the DRM pageflip ioctl to handle this swap? */
		if (flip_possible(iptr, drawable, front, back)) {
			swapinfo->type = SWAP_FLIP;
			isflip = 1;
		} else {
			swapinfo->type = SWAP_BLIT;
		}

		/*
		 * Target MSC for flips are one vblank earlier than for blits since we
		 * need to queue the flip and then wait for the following vblank for it
		 * to actually take effect (whereas blits happen immediately when
		 * performed).
		 */
		if (*target_msc > 0 && isflip) {
			(*target_msc)--;
		}

		/*
		 * If there's no divisor, or if we haven't reached target_msc yet, we just
		 * need to wait for target_msc to pass before unblocking the client.
		 */
		if (divisor == 0 || current_msc < *target_msc) {
			/* The DRM can handle the scheduling needs if we're flipping. */
			if (isflip) {
				ret = emgd_crtc_schedule_flip(swapinfo);
				if (ret) {
					/*
					 * Flip scheduled successfully with DRM.  Exchange the front
					 * and back buffers.
					 */
					emgd_dri2_exchange_buffers(drawable, front, back);
					return 1;
				}

				/*
				 * If we get here, DRM flip scheduling failed for some reason.
				 * Fall back to a scheduled blit below instead
				 */
				swapinfo->type = SWAP_BLIT;
			}

			vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
			if (crtcnum > 0) {
				vbl.request.type |= DRM_VBLANK_SECONDARY;
			}
			if (!isflip) {
				vbl.request.type |= DRM_VBLANK_NEXTONMISS;
			}

			/*
			 * If we've already reached the target MSC, set it to the current
			 * MSC.  This ensures the caller gets a reasonable value back.
			 */
			if (current_msc >= *target_msc) {
				*target_msc = current_msc;
			}

			vbl.request.sequence = *target_msc;
			vbl.request.signal = (unsigned long)swapinfo;
			ret = drmWaitVBlank(iptr->drm_fd, &vbl);
			if (ret) {
				OS_ERROR("Failed to get vblank counter: %s", strerror(errno));
				goto blit_fallback_cleanup_swapinfo;
			}
			*target_msc = vbl.reply.sequence + isflip;
			swapinfo->framenum = *target_msc;

			/* The swap is dispatched; we can return */
			return 1;
		}

		/*
		 * If we get here, target_msc has already passed or we don't have one,
		 * so we queue an event that satisfies the divisor/remainder equation.
		 */
		vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (crtcnum > 0) {
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		}
		if (!isflip) {
			vbl.request.type |= DRM_VBLANK_NEXTONMISS;
		}
		vbl.request.sequence = current_msc - (current_msc % divisor) + remainder;

		/*
		 * If the calculated remainder is greater than the requested remainder,
		 * then we've missed it for this divisor cycle.  Wait for the next
		 * time around.
		 *
		 * This should take the 1 frame swap delay for pageflipping into account
		 * as well as the DRM_VBLANK_NEXTONMISS delay when blitting/exchanging.
		 */
		if (vbl.request.sequence <= current_msc) {
			vbl.request.sequence += divisor;
		}

		/* Account for extra pageflip delay */
		vbl.request.sequence -= isflip;

		vbl.request.signal = (unsigned long)swapinfo;
		ret = drmWaitVBlank(iptr->drm_fd, &vbl);
		if (ret) {
			OS_ERROR("Failed to get vblank counter: %s", strerror(errno));
			goto blit_fallback_cleanup_swapinfo;
		}

		/* Adjust for 1 frame delay when pageflipping */
		*target_msc = vbl.reply.sequence + isflip;

		swapinfo->framenum = vbl.reply.sequence;

		return 1;
	}

blit_fallback_cleanup_swapinfo:
	/*
	 * Extra cleanup we need to do if we've already registered the swapinfo
	 * and incremented the buffer refcounts.
	 */
	((emgd_dri2_private_t*)front->driverPrivate)->refcnt--;
	((emgd_dri2_private_t*)back->driverPrivate)->refcnt--;
	LIST_DEL(&swapinfo->client_resource);
	LIST_DEL(&swapinfo->drawable_resource);
	if (swapinfo->front) {
		emgd_dri2_destroy_buffer(drawable, swapinfo->front);
	}
	if (swapinfo->back) {
		emgd_dri2_destroy_buffer(drawable, swapinfo->back);
	}
blit_fallback:
	/* Blit region is the full size of the drawable */
	blitbox.x1 = 0;
	blitbox.y1 = 0;
	blitbox.x2 = drawable->width;
	blitbox.y2 = drawable->height;
	REGION_INIT(screen, &blitregion, &blitbox, 0);

	/* Blit back -> front */
	emgd_dri2_copy_region(drawable, &blitregion, front, back);

	/* Notify client that swap is complete */
	DRI2SwapComplete(client, drawable, 0, 0, 0, DRI2_BLIT_COMPLETE,
		callback, cbdata);

	/* If we allocated a swapinfo strucutre, clean it up */
	free(swapinfo);

	/* Clear target MSC */
	*target_msc = 0;

	return 1;
}


/*
 * emgd_dri2_schedule_waitmsc()
 *
 * Similar to the scheduled swap interface above, but instead of actually
 * performing a swap, this interface simply returns a notification to the DRI2
 * client and performs no other action.
 */
static int emgd_dri2_schedule_waitmsc(ClientPtr client, DrawablePtr drawable,
	CARD64 target_msc, CARD64 divisor, CARD64 remainder)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrninfo);
	drmVBlank vbl;
	emgd_dri2_swap_record_t *swapinfo = NULL;
	CARD64 current_msc;
	int ret, crtcnum;

	/*
	 * If the drawable isn't visible on any CRTC, then just blit immediately
	 * since we don't have a CRTC's vblank that we want to schedule with.
	 */
	crtcnum = crtc_for_drawable(scrninfo, drawable);
	if (crtcnum < 0) {
		goto wait_complete;
	}

	/*
	 * Truncate inputs to match kernel interfaces.  This will result in
	 * occasional overflow misses, but that's generally considered acceptable
	 * (other vendors' DDX's do the same thing).
	 */
	target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	/* Allocate a record of the swap request */
	swapinfo = calloc(1, sizeof(*swapinfo));
	if (!swapinfo) {
		goto wait_complete;
	}
	swapinfo->iptr = iptr;
	swapinfo->drawable_id = drawable->id;
	swapinfo->client = client;
	swapinfo->crtc = crtcnum;
	swapinfo->callback = NULL;
	swapinfo->callback_data = NULL;
	swapinfo->back = NULL;
	swapinfo->front = NULL;
	swapinfo->type = SWAP_WAIT;

	if (!register_swapinfo(swapinfo)) {
		/*
		 * Shouldn't be possible (implies failure to register an X
		 * resource).  If we do fail for some reason, just consider
		 * the wait to be complete.
		 */
		goto wait_complete;
	}

	/* Get the current MSC */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (crtcnum > 0) {
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	}
	vbl.request.sequence = 0;
	ret = drmWaitVBlank(iptr->drm_fd, &vbl);
	if (ret) {
		OS_ERROR("Failed to get current MSC: %s", strerror(errno));
		goto wait_complete_cleanup_swapinfo;
	}
	current_msc = vbl.reply.sequence;

	/*
	 * If there's no divisor, or if we haven't reached target_msc yet, we just
	 * need to wait for target_msc to pass before unblocking the client.
	 */
	if (divisor == 0 || current_msc < target_msc) {
		vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (crtcnum > 0) {
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		}

		/*
		 * If we've already reached the target MSC, set it to the current
		 * MSC.  This ensures the caller catches up on what the current
		 * time is so that it won't continue to send us a whole bunch of
		 * wait requests for timestamps in the past.
		 */
		if (current_msc >= target_msc) {
			target_msc = current_msc;
		}

		vbl.request.sequence = target_msc;
		vbl.request.signal = (unsigned long)swapinfo;
		ret = drmWaitVBlank(iptr->drm_fd, &vbl);
		if (ret) {
			OS_ERROR("Failed to get vblank counter: %s", strerror(errno));
			goto wait_complete_cleanup_swapinfo;
		}
		target_msc = vbl.reply.sequence;
		swapinfo->framenum = target_msc;

		/*
		 * The kernel will wake us up when the target vblank arrives.  In the
		 * mean time, put the client app to sleep.
		 */
		DRI2BlockClient(client, drawable);
		return 1;
	}

	/*
	 * If we get here, target_msc has already passed or we don't have one,
	 * so we queue an event that satisfies the divisor/remainder equation.
	 */
	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	if (crtcnum > 0) {
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	}
	vbl.request.sequence = current_msc - (current_msc % divisor) + remainder;

	/*
	 * If the calculated remainder is greater than the requested remainder,
	 * then we've missed it for this divisor cycle.  Wait for the next
	 * time around.
	 *
	 * This should take the 1 frame swap delay for pageflipping into account
	 * as well as the DRM_VBLANK_NEXTONMISS delay when blitting/exchanging.
	 */
	if (vbl.request.sequence <= current_msc) {
		vbl.request.sequence += divisor;
	}

	vbl.request.signal = (unsigned long)swapinfo;
	ret = drmWaitVBlank(iptr->drm_fd, &vbl);
	if (ret) {
		OS_ERROR("Failed to get vblank counter: %s", strerror(errno));
		goto wait_complete_cleanup_swapinfo;
	}

	swapinfo->framenum = vbl.reply.sequence;
	DRI2BlockClient(client, drawable);

	return 1;

wait_complete_cleanup_swapinfo:
	/* Extra cleanup we need to do if we'd already registered the swapinfo */
	LIST_DEL(&swapinfo->client_resource);
	LIST_DEL(&swapinfo->drawable_resource);
wait_complete:
	/*
	 * Something went wrong above.  Just send the waitmsc completion notification
	 * immediately to the client to wake it up and let it proceed.
	 */
	DRI2WaitMSCComplete(client, drawable, target_msc, 0, 0);
	free(swapinfo);
	return 1;
}
#endif


Bool emgd_dri2_screen_init(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrn);
	DRI2InfoRec info;
	char proc_fs_node[100];
	int len;

	OS_TRACE_ENTER;

	if (!DIX_REGISTER_PRIVATE(&dri2_client_key, PRIVATE_CLIENT, sizeof(XID))) {
		return FALSE;
	}

	iptr->dev_dri_name = calloc(100, 1);
	if (iptr->dev_dri_name == NULL) {
		OS_ERROR("Failed to allocate /dev/dri/ name string");
		return FALSE;
	}

	/* If we have no DRM connection, bail out on DRI2 */
	if (iptr->drm_fd < 0) {
		OS_ERROR("No DRM connection; disabling DRI2");
		free(iptr->dev_dri_name);
		iptr->dev_dri_name = NULL;
		return FALSE;
	}

	/*
	 * Zero the DRI2 info rec.  If we compile against newer X servers that
	 * bump the info rec version, there may be extra fields at the end
	 * of this structure that need to be NULL.
	 */
	memset(&info, 0, sizeof(DRI2InfoRec));

	/* Pass the DRM's FD to the DRI2 initialization */
	info.fd = iptr->drm_fd;

	/*
	 * Let's find the /dev/dri/cardXXXX filename that matches our DRI file
	 * descriptor.  Most drivers loop over all the possible file names
	 * and either compare bus ID's or device minor numbers.  We have
	 * a better solution:  use the /proc/$$/fd/XXXXX symlink to figure
	 * out the name without looping.
	 */
	snprintf(proc_fs_node, sizeof(proc_fs_node), "/proc/%d/fd/%d",
			(int)getpid(), iptr->drm_fd);
	len = readlink(proc_fs_node, iptr->dev_dri_name, 99);
	iptr->dev_dri_name[len] = '\0';

	info.driverName = DRI_DRIVER_NAME;
	info.deviceName = iptr->dev_dri_name;
	info.version = 1;

	/*
	 * We no longer support DRI inforec version 1 (i.e., X 1.5), so set these
	 * pointers to NULL.  They were removed completely in inforec version 3.
	 */
#if DRI2INFOREC_VERSION < 3
	info.CreateBuffers = NULL;
	info.DestroyBuffers = NULL;
#endif

	info.version = DRI2INFOREC_VERSION;
	info.CreateBuffer = emgd_dri2_create_buffer;
	info.DestroyBuffer = emgd_dri2_destroy_buffer;
	info.CopyRegion = emgd_dri2_copy_region;

	/*
	 * X 1.8 bumps up to DRI2INFOREC_VERSION = 4 which adds new
	 * interfaces for ScheduleSwap, GetMSC, and ScheduleWaitMSC.
	 */
#if DRI2INFOREC_VERSION >= 4 && defined(ALLOW_SWAPS)
	info.ScheduleSwap = emgd_dri2_schedule_swap;
	info.GetMSC = emgd_dri2_get_msc;
	info.ScheduleWaitMSC = emgd_dri2_schedule_waitmsc;

	info.numDrivers = 1;
	info.driverNames = &info.driverName;

	/* Create new resource types to track our swap records */
	swap_record_client_resource =
		CreateNewResourceType(emgd_dri2_swap_record_client_gone,
			"Swap Record Client");
	swap_record_drawable_resource =
		CreateNewResourceType(emgd_dri2_swap_record_drawable_gone,
			"Swap Record Drawable");
#endif

	/* Create an Atom for the "use sprite" window property */
	sprite_usage_atom = MakeAtom("EMGD_USE_SPRITE", 15, 1);
	sprite_assignment_atom = MakeAtom("EMGD_SPRITE_ASSIGN", 18, 1);
	iptr->dri2_inuse = 1;

	OS_TRACE_EXIT;
	return DRI2ScreenInit(screen, &info);
}
