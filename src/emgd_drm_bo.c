/*
 * -----------------------------------------------------------------------------
 *  Filename: emgd_drm_bo.c
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
 *   Interface functions to the DRM library.
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.buffer

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <xf86drm.h>
#include <xf86.h>

#include "emgd.h"
#include "emgd_drm_bo.h"
#include "emgd_uxa.h"


/*
 * Initialize the buffer managment code
 */
Bool emgd_buffer_manager_init(emgd_priv_t *iptr)
{
	size_t aperture_size = iptr->PciInfo->regions[2].size;

	iptr->bufmgr = drm_intel_bufmgr_gem_init(iptr->drm_fd, EMGD_BATCH_SIZE);
	if (!iptr->bufmgr) {
		return FALSE;
	}

	/*
	 * Memory management thresholds taken from OTC's driver to avoid
	 * thrashing and poor memory usage patterns.
	 */
	iptr->max_gtt_map_size = aperture_size / 4;
	iptr->max_tiling_size = iptr->max_gtt_map_size;
	iptr->max_bo_size = iptr->max_gtt_map_size;

	drm_intel_bufmgr_gem_enable_reuse(iptr->bufmgr);
	drm_intel_bufmgr_gem_set_vma_cache_size(iptr->bufmgr, 512);
	drm_intel_bufmgr_gem_enable_fenced_relocs(iptr->bufmgr);

	/* Initialize some memory management bo lists */
	LIST_INIT(&iptr->batch_pixmaps);
	LIST_INIT(&iptr->flush_pixmaps);
	LIST_INIT(&iptr->in_flight);

	if ((INTEL_INFO(iptr)->gen == 60)) {
		iptr->wa_scratch_bo =
			drm_intel_bo_alloc(iptr->bufmgr, "wa scratch",
				4096, 4096);
	}

	return TRUE;
}

void emgd_buffer_manager_destroy(emgd_priv_t *iptr)
{
	if (iptr->wa_scratch_bo) {
		drm_intel_bo_unreference(iptr->wa_scratch_bo);
	}
	if (iptr->bufmgr) {
		drm_intel_bufmgr_destroy(iptr->bufmgr);
	}
}


/*
 * emgd_init_front_buffer
 *
 * Create a buffer object for the front buffer.
 */
Bool emgd_init_front_buffer(ScrnInfoPtr scrn,
		int width, int height,
		unsigned long *pitch)
{
	emgd_priv_t *iptr;
	uint32_t tiling_mode = I915_TILING_X;
	int aligned_width;

	iptr = EMGDPTR(scrn);

	/*
	 * Make sure width and stride are correct.
	 * What about tiling?
	 */

	OS_PRINT("Allocating front buffer sized %d x %d", width, height);

	aligned_width = ALIGN(width, 64);
	iptr->front_buffer = drm_intel_bo_alloc_tiled(
			iptr->bufmgr,
			"front buffer",
			aligned_width, height, iptr->cpp, &tiling_mode, pitch, 0);

	if (!iptr->front_buffer) {
		OS_ERROR("Failed to allocate front buffer (drm_intel_bo_alloc_tiled).");
		return FALSE;
	}

	drm_intel_bo_disable_reuse(iptr->front_buffer);

	scrn->virtualX = width;
	scrn->virtualY = height;

	return TRUE;
}

/*
 * emgd_frontbuffer_handle
 *
 * Get the buffer handle and pitch of the buffer currentally allocated
 * as the front framebuffer.
 */
void emgd_frontbuffer_handle(ScrnInfoPtr scrn,
		int *handle,
		unsigned long *pitch)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);
	drm_intel_bo *front_buffer;

	front_buffer = iptr->front_buffer;

	*handle = front_buffer->handle;
	*pitch = iptr->fb_pitch;
}


/*
 * Resize the frame buffer.  Free the existing buffer and allocate
 * a new one based on the new size.
 */
Bool emgd_frontbuffer_resize(ScrnInfoPtr scrn, int width, int height)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);

	if (iptr->front_buffer) {
		drm_intel_bo_unreference(iptr->front_buffer);
		iptr->front_buffer = NULL;
	}

	if (!emgd_init_front_buffer(scrn, width, height, &iptr->fb_pitch)) {
		return FALSE;
	}

	return TRUE;
}


void emgd_destroy_cursor(ScrnInfoPtr scrn, drm_intel_bo *cursor)
{
	if (cursor) {
		drm_intel_bo_unreference(cursor);
		cursor = NULL;
	}
}


drm_intel_bo *emgd_create_cursor(ScrnInfoPtr scrn,
		int width,
		int height)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);
	drm_intel_bo *cursor;

	cursor = drm_intel_bo_alloc(iptr->bufmgr, "ARGB Cursor",
			HWCURSOR_SIZE_ARGB, GTT_PAGE_SIZE);

	return cursor;
}
