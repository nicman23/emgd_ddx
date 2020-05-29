/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_crtc.h
 *-----------------------------------------------------------------------------
 * Copyright (c) 2002-2013, Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software")
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
 *
 * Common typdefs / function prototypes
 *
 *-----------------------------------------------------------------------------
 */

#ifndef _EMGD_CRTC_H_
#define _EMGD_CRTC_H_

#include <damage.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xf86Crtc.h>

typedef struct _drmmode_prop {
	drmModePropertyPtr k_prop;
	unsigned long long value;
	int num_atoms;
	Atom *atoms;
} drmmode_prop_t;

typedef struct _drmmode {
	int fd;
	drmModeResPtr mode_res;
	drmEventContext event_context;

	/*
	 * How many CRTC's are participating in the current flip operation?
	 * (we don't want to "complete" the flip until all CRTC's are done
	 * flipping to the new framebuffer).
	 */
	int flip_count;

	/*
	 * If the current flip operation was triggered by DRI2, we need to
	 * keep a record of that flip request so that we can notify the client
	 * upon completion.
	 */
	emgd_dri2_swap_record_t *dri2_swapinfo;

	/*
	 * Current DRM framebuffer (and old framebuffer we're flipping away from
	 * if we're mid-flip).
	 */
	uint32_t fb_id, old_fb_id;

	/* Linked lists of all CRTC's and outputs being used */
	struct LIST crtcs;
	struct LIST outputs;
} drmmode_t;

typedef struct _emgd_output_priv {
	drmmode_t *drmmode;
	int output_id;
	drmModeConnectorPtr output;
	drmModeEncoderPtr encoder;
	drmModePropertyBlobPtr edid;
	int num_props;
	drmmode_prop_t *props;
	void *private_data;
	int dpms_mode;
	char *backlight_iface;
	int backlight_max;
	int backlight_active_level;

	/* X output */
	xf86OutputPtr xf86output;

	/* Node in KMS context's list of outputs */
	struct LIST link;
} emgd_output_priv_t;

typedef struct _emgd_crtc_priv {
	drmmode_t *drmmode;
	drmModeCrtcPtr crtc;
	drm_intel_bo *cursor;
	drmModeModeInfo k_mode;

	/*
	 * Shadow framebuffer for this CRTC.
	 *
	 * Please note that xrandr's use of the term "shadow" is different from how
	 * we used to refer to it in classic EMGD.  With shadowfb, you essentially
	 * have a backbuffer and a rotated (usually) frontbuffer.  Under XRandR
	 * terminology, "shadow" refers to the frontbuffer (scanout buffer).  Older
	 * EMGD drivers referred to the backbuffer as the "shadow."
	 */
	drm_intel_bo *shadow_bo;
	unsigned long shadow_pitch;
	uint32_t shadow_fb_id;

	/* X CRTC structure */
	xf86CrtcPtr xf86crtc;

	/* Node in KMS context's list of CRTC's */
	struct LIST link;
} emgd_crtc_priv_t;

Bool drmmode_pre_init(ScrnInfoPtr scrn, int fd);
void drmmode_shutdown(emgd_priv_t*);
void drmmode_output_init(ScrnInfoPtr scrn, drmmode_t *drmmode, int num);
int emgd_crtc_schedule_flip(emgd_dri2_swap_record_t *swapinfo);

#endif /* _EMGD_CRTC_H_ */
