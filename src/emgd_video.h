/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_video.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
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
 *  include file for Xv - the XFree86 Video Extension
 *-----------------------------------------------------------------------------
 */

#ifndef __EMGD_VIDEO_H
#define __EMGD_VIDEO_H

#include "emgd_sprite.h"

#define XV_BLEND_COLORKEY_MASK     0x1
#define XV_BLEND_COLORKEY_ENABLED  0x1
#define XV_BLEND_COLORKEY_DISABLED 0

#define EGD_XV_TYPE_OVERLAY 0x01
#define EGD_XV_TYPE_BLEND   0x02

#define intel_adaptor_private emgd_xv_t

int is_planar_fourcc(int id);
void IntelEmitInvarientState(ScrnInfoPtr scrn);


typedef struct {

	RegionRec clip;
	int flags;
	unsigned int blend_colorkey_val;

	/* Attributes only valid for the overlay. */
	int xv_brightness;
	int xv_contrast;
	int xv_saturation;
	uint32_t xv_gamma0;
	uint32_t xv_gamma1;
	uint32_t xv_gamma2;
	uint32_t xv_gamma3;
	uint32_t xv_gamma4;
	uint32_t xv_gamma5;
	/* Attirbutes valid for overlay and blend. */
	uint32_t xv_colorkey;
	/* Attributes only valid for blend. */
	uint32_t xv_alpha;

	emgd_sprite_t *(ovlplane[2]);

	/* Disable the overlay for example during a mode change */
	int disable;
	unsigned long video_status;

	/* overlay type; overlay or blend */
	int ov_type;
	int rotation;
	uint32_t YBufOffset;
	uint32_t UBufOffset;
	uint32_t VBufOffset;
	drm_intel_bo *buf;
	Time off_time;

	/* Scaled pixmap buffers and framebuffers, and current used buffer. */
	int pixmap_current;
	PixmapPtr pixmaps[3];   /* Tripple buffer */
	uint32_t fb_ids[3];
	dri_bo *shmbo[3];       /* vaxv share buffer objects */
	int vaxv;               /* vaxv mode */

	/* Previous put image crtc and display mode. */
        int prev_crtcnum;
        int prev_mode;
} emgd_xv_t;

#endif
