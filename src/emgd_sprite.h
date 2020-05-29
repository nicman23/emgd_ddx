/*
*-----------------------------------------------------------------------------
* Filename: emgd_sprite.h
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
*  include file for emgd_sprite.c
*-----------------------------------------------------------------------------
*/

#ifndef _EMGD_SPRITE_H_
#define _EMGD_SPRITE_H_

#include "emgd_dri2.h"
#include "emgd_uxa.h"
#include "emgd_crtc.h"
#include "regionstr.h"

#include <drm/drm.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

typedef struct _igd_rect {
	__u32 x1;
	__u32 y1;
	__u32 x2;
	__u32 y2;
} igd_rect_t;

struct drm_intel_sprite_colorcorrect {
        __u32 plane_id;
        __u32 brightness;  /* range = 0x00000000 to 0xFFFFFFFF, default = 0x80000000 */
        __u32 contrast;    /* range = 0x00000000 to 0xFFFFFFFF, default = 0x80000000 */
        __u32 saturation;  /* range = 0x00000000 to 0xFFFFFFFF, default = 0x80000000 */
        __u32 hue;         /* range = 0x00000000 to 0xFFFFFFFF, default = 0x80000000 */
};

#define I915_SPRITEFLAG_PIPEBLEND_FBBLENDOVL    0x00000001
#define I915_SPRITEFLAG_PIPEBLEND_CONSTALPHA    0x00000002
#define I915_SPRITEFLAG_PIPEBLEND_ZORDER        0x00000004
/* Set or Get the current color correction info on a given sprite */
/* This MUST be an immediate update because of customer requirements */
struct drm_intel_sprite_pipeblend {
	__u32 plane_id;
	__u32 crtc_id;
		/* I915_SPRITEFLAG_PIPEBLEND_FBBLENDOVL = 0x00000001*/
		/* I915_SPRITEFLAG_PIPEBLEND_CONSTALPHA = 0x00000002*/
		/* I915_SPRITEFLAG_PIPEBLEND_ZORDER     = 0x00000004*/
	__u32 enable_flags;
	__u32 fb_blend_ovl;
	__u32 has_const_alpha;
	__u32 const_alpha; /* 8 LSBs is alpha channel*/
	__u32 zorder_value;
};

#define BASE DRM_COMMAND_BASE

#define DRM_IGD_MODE_GETPLANERESOURCES   0x41
#define DRM_IGD_MODE_GETPLANE            0x42
#define DRM_IGD_MODE_SETPLANE            0x43
#define DRM_IGD_MODE_ADDFB2              0x44
#define DRM_IGD_MODE_SET_PIPEBLEND			 0x45
#define DRM_IGD_MODE_GET_PIPEBLEND			 0x46


#define DRM_I915_GET_SPRITE_COLORCORRECT 0x2c
#define DRM_I915_SET_SPRITE_COLORCORRECT 0x2d
#define DRM_I915_GET_SPRITE_PIPEBLEND    0x2e
#define DRM_I915_SET_SPRITE_PIPEBLEND    0x2f

#define DRM_IOCTL_IGD_MODE_GETPLANERESOURCES     DRM_IOWR(BASE + DRM_IGD_MODE_GETPLANERESOURCES,\
                        struct drm_mode_get_plane_res)
#define DRM_IOCTL_IGD_MODE_GETPLANE              DRM_IOWR(BASE + DRM_IGD_MODE_GETPLANE,\
                        struct drm_mode_get_plane)
#define DRM_IOCTL_IGD_MODE_SETPLANE              DRM_IOWR(BASE + DRM_IGD_MODE_SETPLANE,\
                        struct drm_mode_set_plane)
#define DRM_IOCTL_IGD_MODE_ADDFB2                DRM_IOWR(BASE + DRM_IGD_MODE_ADDFB2,\
                        struct drm_mode_fb_cmd2)
#define DRM_IOCTL_IGD_SET_PIPEBLEND                DRM_IOWR(BASE + DRM_IGD_MODE_SET_PIPEBLEND,\
                        struct drm_intel_sprite_pipeblend)
#define DRM_IOCTL_IGD_GET_PIPEBLEND                DRM_IOWR(BASE + DRM_IGD_MODE_GET_PIPEBLEND,\
                        struct drm_intel_sprite_pipeblend)

#define DRM_IOCTL_I915_SET_SPRITE_COLORCORRECT   DRM_IOWR(BASE + DRM_I915_SET_SPRITE_COLORCORRECT,\
                        struct drm_intel_sprite_colorcorrect)
#define DRM_IOCTL_I915_GET_SPRITE_COLORCORRECT   DRM_IOWR(BASE + DRM_I915_SET_SPRITE_COLORCORRECT,\
                        struct drm_intel_sprite_colorcorrect)
#define DRM_IOCTL_I915_SET_SPRITE_PIPEBLEND      DRM_IOWR(BASE + DRM_I915_SET_SPRITE_PIPEBLEND,\
                        struct drm_intel_sprite_pipeblend)
#define DRM_IOCTL_I915_GET_SPRITE_PIPEBLEND      DRM_IOWR(BASE + DRM_I915_SET_SPRITE_PIPEBLEND,\
                        struct drm_intel_sprite_pipeblend)

#define SPRITE_PREASSIGNED_NONE     0x0000
#define SPRITE_PREASSIGNED_VA       0x0001
#define SPRITE_PREASSIGNED_OGL      0x0002
#define SPRITE_PREASSIGNED_XV       0x0004

#define SPRITE_MODE_SINGLE	0x0
#define SPRITE_MODE_EXTENDED	0x1
#define SPRITE_MODE_CLONE	0x2

/* 
 *  * Ioctl to query kernel params (EMGD)
 *   * I915 definition currently occupies from value 0 up to 17
 *    */
#define I915_PARAM_HAS_MULTIPLANE_DRM   30

typedef struct emgd_sprite_buffer {
	unsigned int name;
	drm_intel_bo *bo;
} emgd_sprite_buffer_t;

typedef struct emgd_sprite {
	uint32_t count_formats;
	uint32_t *formats;
	uint32_t plane_id;
	uint32_t crtc_id;

	struct {
		uint32_t locked; /* has been grabbed by a client*/
		uint32_t enabled;
		uint32_t assignment; /* see SPRITE_PREASSIGNED_foo above */
		DrawablePtr drawable;
		uint32_t fb_id;
		uint32_t crtc_x, crtc_y;
		uint32_t crtc_w, crtc_h;
		uint32_t src_x, src_y;
		uint32_t src_w, src_h;
		uint32_t bo_handle;
		uint32_t old_fb_id;
		RegionRec region;
	} curr_state;

	uint32_t possible_crtcs;
	uint32_t gamma_size;
	struct LIST link;
	emgd_sprite_buffer_t *flip_buffer1;
	emgd_sprite_buffer_t *flip_buffer2;    
} emgd_sprite_t;

int emgd_sprite_init(emgd_priv_t *iptr);
int emgd_sprite_shutdown(emgd_priv_t *iptr);
void emgd_sprite_leave_vt(emgd_priv_t *iptr);

emgd_sprite_t * emgd_sprite_grab(emgd_priv_t *iptr, DrawablePtr drawable,
			int crtc_id, uint32_t assignment, ClientPtr client, 
			Bool grab_for_xv);
	/* pass in the drawable, function will determine the crtc_id
	 * and based on any pre-defined sprite assignment rules, will
	 * grab an available sprite/ovl plane.
	 * Returns the plane-id of that sprite/ovl plane
	 */
emgd_sprite_t * emgd_sprite_plane_from_drawable(emgd_priv_t *iptr,
			DrawablePtr drawable, int crtc_id);
void emgd_sprite_ungrab(ScreenPtr screen, emgd_priv_t *iptr, emgd_sprite_t * plane, 
			ClientPtr client, Bool grab_for_xv);
	/* pass in the sprite / ovl plane id and it frees it back to the DDX
	 * sprite management link list
	 */
int emgd_sprite_create_flip_bo(DrawablePtr drawable,
		DRI2BufferPtr buffer,
		emgd_sprite_buffer_t *flip_buffer);
void emgd_sprite_destroy_flip_bo(emgd_sprite_buffer_t *flip_buffer);
void emgd_sprite_check_mode(DrawablePtr drawable, int * mode, int * num_planes);
int emgd_sprite_check_flip_bo(DrawablePtr drawable, 
		DRI2BufferPtr buffer, 
		emgd_sprite_buffer_t *flip_buffer);
void emgd_sprite_exchange_flip_bo(DrawablePtr drawable, 
		DRI2BufferPtr buffer,
		emgd_sprite_buffer_t *flip_buffer1,
		emgd_sprite_buffer_t *flip_buffer2);
int emgd_sprite_flip_core(
		emgd_sprite_t *(planes[2]),
		DrawablePtr drawable, uint32_t fb_id,
		igd_rect_t src_rect, igd_rect_t dst_rect,
		uint32_t pixel_format);
	/* pass in a sprite/ovl plane id, along with the drawable info, the
	 * drm gem buffer object, some flags and the rects info and surface info
	 * and this function will call into the kernel module to flip the sprite
	 */
int emgd_sprite_disable_core(emgd_priv_t *iptr, emgd_sprite_t * plane);
	/* call to turn of the sprite for this sprite/ovl plane */

int emgd_sprite_update_attr(emgd_priv_t *iptr,
		struct drm_intel_sprite_colorkey * ckey,
		struct drm_intel_sprite_colorcorrect * ccorrect,
		struct drm_intel_sprite_pipeblend *pipeblend,
		uint32_t flags);
	/* pass in the type of attribute and the value for it.
	 * this might probably be some kind of structure like the one
	 * used in DRM - igd_ovl_info perhaps
	 */

int emgd_sprite_check_display_rotation_status(DrawablePtr drawable,
	emgd_priv_t *iptr, 
	int crtcnum,
	int mode);

Bool emgd_dri2_sprite_flip_wrapper(ClientPtr client,
        DrawablePtr drawable,
        DRI2BufferPtr src_buf,
	emgd_sprite_t *(planes[2]));
Bool emgd_sprite_get_plane(emgd_priv_t *iptr,
	DrawablePtr drawable,
	int assignment,
	emgd_sprite_t *(planes[2]),
	ClientPtr client,
	Bool grab_for_xv);
Bool emgd_sprite_allocate_tripple_buffer(DrawablePtr drawable,
	DRI2BufferPtr dri_buffer,
	emgd_sprite_t *plane);
#endif
