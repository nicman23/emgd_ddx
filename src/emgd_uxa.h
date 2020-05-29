/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_uxa.h
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
 *  Compatibility header that allows OTC UXA code to be used "as-is" in
 *  the EMGD codebase.
 *-----------------------------------------------------------------------------
 */

#ifndef _EMGD_UXA_H
#define _EMGD_UXA_H

#include <i915_drm.h>
#include <intel_bufmgr.h>
#include <list.h>

/*
 * OTC's UXA code (which we use as-is) assumes different names for several
 * structures.
 */
#define intel_screen_private emgd_priv_t
#define intel_pixmap _emgd_pixmap
#define intel_get_screen_private(p)  EMGDPTR(p)

#define PUNT_UXA_OFFSCREEN_PIXMAPS   0
#define PUNT_UXA_SOLID_FILL          0
#define PUNT_UXA_COPY                0
#define PUNT_UXA_COMPOSITE           0
#define PUNT_UXA_COMPOSITE_1X1_MASK  0
#define PUNT_UXA_COMPOSITE_MASK      0

typedef struct _emgd_pixmap {
	/* GEM buffer object for pixmap memory buffer */
	drm_intel_bo *bo;

	/* KMS framebuffer ID if this pixmap will be used for scanout */
	uint32_t kms_fb;

	struct LIST flush, batch, in_flight;

	uint16_t stride;
	uint8_t tiling;
	int8_t busy :2;
	int8_t batch_write :1;
	int8_t offscreen :1;
	int8_t pinned :1;
} emgd_pixmap_t;

static inline struct intel_pixmap *intel_get_pixmap_private(PixmapPtr pixmap)
{
	return dixLookupPrivate(&pixmap->devPrivates, &uxa_pixmap_index);
}

static inline Bool intel_pixmap_is_busy(struct intel_pixmap *priv)
{
	if (priv->busy == -1)
		priv->busy = drm_intel_bo_busy(priv->bo);
	return priv->busy;
}

static inline void intel_set_pixmap_private(PixmapPtr pixmap, struct intel_pixmap *intel)
{
	dixSetPrivate(&pixmap->devPrivates, &uxa_pixmap_index, intel);
}

static inline Bool intel_pixmap_is_dirty(PixmapPtr pixmap)
{
	return !LIST_IS_EMPTY(&intel_get_pixmap_private(pixmap)->flush);
}

static inline Bool intel_pixmap_tiled(PixmapPtr pixmap)
{
	return intel_get_pixmap_private(pixmap)->tiling != I915_TILING_NONE;
}

dri_bo *intel_get_pixmap_bo(PixmapPtr pixmap);
void intel_set_pixmap_bo(PixmapPtr pixmap, dri_bo * bo);

static inline unsigned long intel_pixmap_pitch(PixmapPtr pixmap)
{
	return (unsigned long)pixmap->devKind;
}

static inline unsigned long intel_get_fence_size(intel_screen_private *iptr,
	unsigned long size)
{
	return ALIGN(size, GTT_PAGE_SIZE);
}

static inline unsigned long intel_get_fence_pitch(intel_screen_private *iptr,
	unsigned long pitch,
	uint32_t tiling_mode)
{
	switch (tiling_mode) {
	case I915_TILING_NONE:
		return pitch;
	case I915_TILING_Y:
		return ALIGN(pitch, 128);
	default:
		return ALIGN(pitch, 512);
	}
}

/* i965_render.c */
unsigned int gen4_render_state_size(ScrnInfoPtr scrn);
void gen4_render_state_init(ScrnInfoPtr scrn);
void gen4_render_state_cleanup(ScrnInfoPtr scrn);
Bool i965_check_composite(int op,
			  PicturePtr sourcec, PicturePtr mask, PicturePtr dest,
			  int width, int height);
Bool i965_check_composite_texture(ScreenPtr screen, PicturePtr picture);
Bool i965_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
				PicturePtr dest, PixmapPtr sourcecPixmap,
				PixmapPtr maskPixmap, PixmapPtr destPixmap);
void i965_composite(PixmapPtr dest, int srcX, int srcY,
			int maskX, int maskY, int dstX, int dstY, int w, int h);

void i965_vertex_flush(emgd_priv_t *intel);
void i965_batch_flush(emgd_priv_t *intel);
void i965_batch_commit_notify(emgd_priv_t *intel);

/* i965_3d.c */
void gen6_upload_invariant_states(emgd_priv_t *intel);
void gen6_upload_viewport_state_pointers(emgd_priv_t *intel,
					 drm_intel_bo *cc_vp_bo);
void gen7_upload_viewport_state_pointers(emgd_priv_t *intel,
					 drm_intel_bo *cc_vp_bo);
void gen6_upload_urb(emgd_priv_t *intel);
void gen7_upload_urb(emgd_priv_t *intel);
void gen6_upload_cc_state_pointers(emgd_priv_t *intel,
				   drm_intel_bo *blend_bo, drm_intel_bo *cc_bo,
				   drm_intel_bo *depth_stencil_bo,
				   uint32_t blend_offset);
void gen7_upload_cc_state_pointers(emgd_priv_t *intel,
				   drm_intel_bo *blend_bo, drm_intel_bo *cc_bo,
				   drm_intel_bo *depth_stencil_bo,
				   uint32_t blend_offset);
void gen6_upload_sampler_state_pointers(emgd_priv_t *intel,
					drm_intel_bo *sampler_bo);
void gen7_upload_sampler_state_pointers(emgd_priv_t *intel,
					drm_intel_bo *sampler_bo);
void gen7_upload_bypass_states(emgd_priv_t *intel);
void gen6_upload_gs_state(emgd_priv_t *intel);
void gen6_upload_vs_state(emgd_priv_t *intel);
void gen6_upload_clip_state(emgd_priv_t *intel);
void gen6_upload_sf_state(emgd_priv_t *intel, int num_sf_outputs, int read_offset);
void gen7_upload_sf_state(emgd_priv_t *intel, int num_sf_outputs, int read_offset);
void gen6_upload_binding_table(emgd_priv_t *intel, uint32_t ps_binding_table_offset);
void gen7_upload_binding_table(emgd_priv_t *intel, uint32_t ps_binding_table_offset);
void gen6_upload_depth_buffer_state(emgd_priv_t *intel);
void gen7_upload_depth_buffer_state(emgd_priv_t *intel);

Bool intel_transform_is_affine(PictTransformPtr t);
Bool
intel_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out);

Bool
intel_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
					float *x_out, float *y_out, float *z_out);

static inline void
intel_debug_fallback(ScrnInfoPtr scrn, char *format, ...)
{
	emgd_priv_t *intel = intel_get_screen_private(scrn);
	va_list ap;

	va_start(ap, format);
	if (intel->fallback_debug) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO, "fallback: ");
		LogVMessageVerb(X_INFO, 1, format, ap);
	}
	va_end(ap);
}

static inline Bool
intel_check_pitch_2d(PixmapPtr pixmap)
{
	uint32_t pitch = intel_pixmap_pitch(pixmap);
	if (pitch > KB(32)) {
		ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
		intel_debug_fallback(scrn, "pitch exceeds 2d limit 32K\n");
		return FALSE;
	}
	return TRUE;
}

/**
 * Little wrapper around drm_intel_bo_reloc to return the initial value you
 * should stuff into the relocation entry.
 *
 * If only we'd done this before settling on the library API.
 */
static inline uint32_t
intel_emit_reloc(drm_intel_bo * bo, uint32_t offset,
		 drm_intel_bo * target_bo, uint32_t target_offset,
		 uint32_t read_domains, uint32_t write_domain)
{
	drm_intel_bo_emit_reloc(bo, offset, target_bo, target_offset,
				read_domains, write_domain);

	return target_bo->offset + target_offset;
}

static inline drm_intel_bo *intel_bo_alloc_for_data(emgd_priv_t *intel,
							const void *data,
							unsigned int size,
							char *name)
{
	drm_intel_bo *bo;

	bo = drm_intel_bo_alloc(intel->bufmgr, name, size, 4096);
	if (bo)
		drm_intel_bo_subdata(bo, 0, size, data);
	return bo;
}

void intel_debug_flush(ScrnInfoPtr scrn);

static inline PixmapPtr get_drawable_pixmap(DrawablePtr drawable)
{
	ScreenPtr screen = drawable->pScreen;

	if (drawable->type == DRAWABLE_PIXMAP)
		return (PixmapPtr) drawable;
	else
		return screen->GetWindowPixmap((WindowPtr) drawable);
}

static inline Bool pixmap_is_scanout(PixmapPtr pixmap)
{
	ScreenPtr screen = pixmap->drawable.pScreen;

	return pixmap == screen->GetScreenPixmap(screen);
}

const OptionInfoRec *intel_uxa_available_options(int chipid, int busid);

Bool intel_uxa_init(ScreenPtr pScreen);
Bool intel_uxa_create_screen_resources(ScreenPtr pScreen);
void intel_uxa_block_handler(emgd_priv_t *intel);
Bool intel_get_aperture_space(ScrnInfoPtr scrn, drm_intel_bo ** bo_table,
				  int num_bos);

/* intel_shadow.c */
void intel_shadow_blt(emgd_priv_t *intel);
void intel_shadow_create(emgd_priv_t *intel);

static inline Bool intel_pixmap_is_offscreen(PixmapPtr pixmap)
{
	struct intel_pixmap *priv = intel_get_pixmap_private(pixmap);
	return priv && priv->offscreen;
}

enum {
	DEBUG_FLUSH_BATCHES = 0x1,
	DEBUG_FLUSH_CACHES = 0x2,
	DEBUG_FLUSH_WAIT = 0x4,
};

#endif
