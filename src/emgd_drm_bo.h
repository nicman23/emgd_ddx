/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd_drm_bo.h
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
 *  include file for DRM based functions.
 *-----------------------------------------------------------------------------
 */

#ifndef _EMGD_DRM_BO_H_
#define _EMGD_DRM_BO_H_
#include <windowstr.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>


Bool emgd_buffer_manager_init(emgd_priv_t *iptr);

void emgd_buffer_manager_destroy(emgd_priv_t *iptr);

drm_intel_bo *emgd_create_cursor(ScrnInfoPtr scrn,
		int width,
		int height);

void emgd_destroy_cursor(ScrnInfoPtr scrn, drm_intel_bo *cursor);

void emgd_cursor_handle(emgd_priv_t *iptr,
		unsigned long *handle,
		unsigned long *pitch);

Bool emgd_frontbuffer_resize(ScrnInfoPtr scrn, int width, int height);

void emgd_frontbuffer_handle(ScrnInfoPtr scrn,
		int *handle,
		unsigned long *pitch);

Bool emgd_init_front_buffer(ScrnInfoPtr scrn, int width, int height,
		unsigned long *pitch);

void emgd_create_egl_pixmap_image(ScrnInfoPtr scrn,
		unsigned long *handle,
		unsigned long *pitch);

void emgd_close_egl_screen(ScreenPtr screen);

/*
PixmapPtr emgd_create_pixmap(ScrnInfoPtr scrn,
		int width, int height,
		int depth,
		unsigned long flag,
		struct gbm_bo **pixmap_bo);
*/


#endif  /* _EMGD_DRM_BO_H_ */
