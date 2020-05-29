/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd_api.h
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
 *  This file defines the API for communication with the EMGD driver's
 *  proprietary API interfaces.
 *-----------------------------------------------------------------------------
 */

#ifndef _EMGD_API_H
#define _EMGD_API_H

#define MAX_NAME_LEN 256

#define EMGD_CONTROL_NOT_SUPPORTED  -1
#define EMGD_CONTROL_SUCCESS         0
#define EMGD_CONTROL_ERROR           1
#define EMGD_CONTROL_STATUS_NOERROR  0

#define EMGD_CONTROL_NO_REPLY                0x01000 /* don't send reply */

#define EMGD_CONTROL_GET_DRIVER_INFO         0x20012
#define EMGD_CONTROL_GET_PORT_INFO           0x20014
#define EMGD_CONTROL_GET_OVL_COLOR_PARAMS    0x20019
#define EMGD_CONTROL_SET_OVL_COLOR_PARAMS    0x2001A
#define EMGD_CONTROL_NOOP                    0x20020
#define EMGD_CONTROL_NOOP_DRAWABLE          (0x20021 | EMGD_CONTROL_NO_REPLY)
#define EMGD_CONTROL_GET_DEBUG               0x20022
#define EMGD_CONTROL_SET_DEBUG               0x20023
#define EMGD_CONTROL_OVERLAY_PIPEBLEND       0x20105
#define EMGD_CONTROL_SPLASHSCREEN            0x2021A

#ifdef CONFIG_DEBUG
static __inline char *intel_esc_str(unsigned int esc)
{
	switch (esc) {
	case EMGD_CONTROL_GET_DRIVER_INFO:
		return "EMGD_CONTROL_GET_DRIVER_INFO";
	case EMGD_CONTROL_GET_PORT_INFO:
		return "EMGD_CONTROL_GET_PORT_INFO";
	case EMGD_CONTROL_GET_OVL_COLOR_PARAMS:
		return "EMGD_CONTROL_GET_OVL_COLOR_PARAMS";
	case EMGD_CONTROL_SET_OVL_COLOR_PARAMS:
		return "EMGD_CONTROL_SET_OVL_COLOR_PARAMS";
	case EMGD_CONTROL_NOOP:
		return "EMGD_CONTROL_NOOP";
	case EMGD_CONTROL_NOOP_DRAWABLE:
		return "EMGD_CONTROL_NOOP_DRAWABLE";
	case EMGD_CONTROL_GET_DEBUG:
		return "EMGD_CONTROL_GET_DEBUG";
	case EMGD_CONTROL_SET_DEBUG:
		return "EMGD_CONTROL_SET_DEBUG";
	case EMGD_CONTROL_OVERLAY_PIPEBLEND:
		return "EMGD_CONTROL_OVERLAY_PIPEBLEND";
	case EMGD_CONTROL_SPLASHSCREEN:
		return "EMGD_CONTROL_SPLASHSCREEN";
	default:
		return "EMGD_CONTROL Invalid";
	}
}
#endif



/*
 * Data structures used by the escape I/O structures defined in the
 * section below.
 *
 * Please use size-specific types ({u}int{8,16,32}_t) rather than
 * architecture-specific types (char, int, long, etc.).  We may have
 * cases were the driver is 64-bit, but a 32-bit client application
 * is trying to make escape calls; we need the structures, as compiled
 * by both 32-bit and 64-bit compilers to match.
 */

/* Corners of a rectangle.  Should be identical to igd_rect_t */
typedef struct _iegd_rect {
	uint32_t x1;
	uint32_t y1;
	uint32_t x2;
	uint32_t y2;
} iegd_rect_t, *piegd_rect_t;


/*
 * Coefficients for converting YUV to RGB, must be identical to
 * igd_yuv_coeffs
 */
typedef struct _iegd_yuv_coeffs {
	int8_t ry;
	int8_t ru;
	int8_t rv;
	int8_t gy;
	int8_t gu;
	int8_t gv;
	int8_t by;
	int8_t bu;
	int8_t bv;

	int16_t r_const;
	int16_t g_const;
	int16_t b_const;

	uint8_t r_shift;
	uint8_t g_shift;
	uint8_t b_shift;
} iegd_yuv_coeffs, *piegd_yuv_coeffs;



/*
 * The following typedefs define the input and output structures
 * that are passed between the driver and the client.  These are
 * versioned, and must not be changed without incrementing the
 * protocal version number.
 */

typedef struct _iegd_esc_port_in {
	uint32_t port_number;
} iegd_esc_port_in_t;

/* For the functions that return a count it will return this */
typedef struct _iegd_esc_count {
	uint32_t count;
} iegd_esc_count_t;


/* Input to driver with drawable info */
typedef struct _iegd_esc_drawable_info {
	uint32_t drawable_id;
} iegd_esc_drawable_info_t;

/* Output from driver */
typedef struct _iegd_esc_driver_info {
	char name[MAX_NAME_LEN];       /* EGD Driver Name                */
	char chipset[MAX_NAME_LEN];    /* Chipset name                    */
	uint32_t major;                /* Major version                   */
	uint32_t minor;                /* Minor version                   */
	uint32_t build;                /* Build number                    */
	char date[MAX_NAME_LEN];       /* Date of build                   */
	unsigned short config_id;      /* Current PCF config id */
	uint32_t device_id;            /* PCI Device ID */
	uint32_t revision_id;          /* PCI Revision ID */
} iegd_esc_driver_info_t;

/* Output from driver with information about any port */
typedef struct _iegd_esc_port_info {
	uint32_t width;                /* Current mode: screen width     */
	uint32_t height;               /* Current mode: screen height    */
	uint32_t depth;                /* Current mode: screen depth     */
	uint32_t refresh;              /* Current mode: screen refresh   */
	uint32_t edid;                 /* Edid enable, 0 or 1            */

	uint32_t enable;               /* Port is enabled or disabled    */
	uint32_t timing_owner;         /* Set if port owns timings       */

	char pd_name[MAX_NAME_LEN];     /* Port driver name see pd.h      */
	char user_alias[MAX_NAME_LEN];  /* User(via IAL reg/cfg file)port name */
	uint32_t pd_version;            /* Port driver version see pd.h   */
	uint32_t pd_type;               /* Port driver type see pd.h      */
	uint32_t display_id;            /* display currently associated with port*/
	uint32_t flags;			        /* Display flag. Default: 0x0	  */
} iegd_esc_port_info_t;


/*
 * Get/set debug info for drivers with debugging enabled.
 */
typedef struct _iegd_esc_debug_info {
	uint32_t hal_flags;
	uint32_t ial_flags;
} iegd_esc_debug_info_t;


#define GAMMA_FLAG          0x1
#define BRIGHTNESS_FLAG     0x2
#define CONTRAST_FLAG       0x4
#define SATURATION_FLAG     0x8
#define COLORKEY_FLAG       0x20
#define OVL_COLOR_FLAG      0x10

/*
 * the following escape structure is for dynamically changing the overlay
 * color attributes or frame buffer gamma correction
 *  ** this backdoor is temporary and is for MPD only! **
 */
typedef struct _iegd_esc_color_params {
	uint8_t flag; /* 0x1 = gamma, 0x2 = brightness */
	              /* 0x4 = contrast, 0x8 = saturation */
	              /* 0x10 = Overlay colors; if not set for FB */
	uint32_t gamma;
	uint32_t brightness;
	uint32_t contrast;
	uint32_t saturation;
	uint32_t colorkey;
} iegd_esc_color_params_t;


/*--------------------------------------------------------------------------
 * iegd_esc_overlay_pipeblend_t: Overlay Planes Z-Order Control
 * USED with escape code EMGD_CONTROL_PIPEBLEND_OVERLAY.
 * See below comments after structure definition for description of
 * definitions and how they are used. Need to keep these comments
 * and definitions in sync with igd_ovl.h
 */
/* Definitions for bottom, middle and top
 * in zorder_combined, nibbles read right to left
 * = top, middle, bottom
 */
#define EGD_OVL_ZORDER_DISPLAY1   0x1
/*#define IGD_OVL_ZORDER_DISPLAY2 0x02*/
#define EGD_OVL_ZORDER_SPRITE1    0x4
#define EGD_OVL_ZORDER_SPRITE2    0x8
#define EGD_OVL_ZORDER_DEFAULT    0x00010408
	/*use as "combined" in struct*/

/* Definitions for enable_flags */
#define EGD_OVL_PIPEBLEND_SET_FBBLEND      0x00000001
#define EGD_OVL_PIPEBLEND_SET_ZORDER       0x00000002
#define EGD_OVL_PIPEBLEND_SET_CONSTALPHA   0x00000004

/* Definitions for ret */

/*
 * EGD_OVL_PIPEBLEND_SET_FBBLEND will update the state of the Fb-Blend-Ovl
 * depending on "fb_blend_ovl" param if igd_ovl_pipeblend_t. Enabling will
 * cause blending the contents of the overlay with the contents of the
 * frambuffer using the framebuffer's alpha channel (as if the framebuffer
 * is on top of the overlay). This feature requires
 * that dest colorkey be enabled. Blending will be per pixel. Even if
 * FB is XRGB, u can use this flag, and overlay module will change Display
 * Plane to ARGB - but the trick would be about how the IAL would allow
 * apps to get the right alpha into the FB.
 *
 * EGD_OVL_PIPEBLEND_SET_ZORDER updates the state of the current display pipes'
 * Zorder in relation to the other sprite and cursor planes on that pipe.
 * 3 nibbles will hold the bottom, middle and top most planes. These are the
 * variables "top", "middle" and "bottom" of the igd_ovl_pipeblend_t struct.
 * Each nibble can be either OVL_SPRITE1_ZORDER or OVL_SPRITE2_ZORDER or
 * OVL_DISPLAY_ZORDER.
 *
 * If flag EGD_OVL_PIPEBLEND_SET_CONSTALPHA is set,
 * then variable "has_const_alpha" (part of igd_ovl_pipeblend_t structure)
 * will be used to enable or disable constant alpha. If enabled, the
 * alpha value (from "const_alpha" variable) is used to multiply against
 * video pixels before blending in pipe. Used for video fade out effect
 *
 */
typedef struct _iegd_esc_overlay_pipeblend {
	uint32_t screen; /* Screen number */
	union {
		struct {
			unsigned long top:4;
			unsigned long :4;
			unsigned long middle:4;
			unsigned long :4;
			unsigned long bottom:4;
			unsigned long :12;
		}; /* See above comment */
		unsigned char zorder_array[4];
		uint32_t zorder_combined;
	}; /* zorder impacts all planes of this pipe */
	unsigned long   fb_blend_ovl;
		/* fb-blend-ovl impact all planes of this pipe */
	unsigned long   has_const_alpha;
	unsigned long   const_alpha;
		/* const alpha impact only this ovl/sprite plane */
		/* cannot be used for querrying */
	uint32_t        enable_flags; /* See above comment */
	uint32_t ret; /* 0 for success - non zero contains failure reason hint*/
} iegd_esc_overlay_pipeblend_t, iegd_esc_overlay_pipeblend_reply_t;

/*
 * **** For the variables "top, middle and bottom" ****
 *#define IGD_OVL_FB_BLEND_OVL        0x00000200
 *
 * - EGD_OVL_ZORDER_DISPLAY1 used as value to put into igd_ovl_zorder_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Setting 'top' to IGD_OVL_ZORDER_DISPLAY1 means Display Plane becomes
 *           top most. Setting 'bottom' to IGD_OVL_ZORDER_DISPLAY1 Display Plane
 *           becomes bottom most. ... and etc... this also applies similiarly
 *           for IGD_OVL_ZORDER_SPRITE1 and IGD_OVL_ZORDER_SPRITE2.
 * - EGD_OVL_ZORDER_SPRITE1  used as value to put into igd_ovl_zorder_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 * - EGD_OVL_ZORDER_SPRITE2  used as value to put into igd_ovl_zorder_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 * - EGD_OVL_ZORDER_DEFAULT  used as value to put into igd_ovl_zorder_t.top,
 *           middle or bottom. Cannot be shared with other bits here.
 *           Read description forIGD_OVL_ZORDER_DISPLAY1
 *
 * NOTE1: The default kernel module state if alter_ovl_pipeblend is never called:
 *		  SPRITE2=TOP, SPRITE1=MIDDLE, DISPLAY=BOTTOM.
 *
 * NOTE2: Using the IGD_OVL_OSD_ON_SPRITEC flag with an alter_ovl call
 * to either one of the sprites for that display pipeline will override
 * the current z_order for that display pipeline with the following:
 * DISPLAY=TOP, SPRITE2=MIDDLE, SPRITE1=BOTTOM
 * (where SPRITE1 "was" HW_OVL and SPRITE2 "was" SPRITE_C on prev chips).
 * This is for backwards compatibility.
 * This happens in VA driver when implementing subpicture using Sprites
 *
 * NOTE3: Using dest color keying for a particular video sprite will
 * make that sprite pop above the DISPLAY plane on next flip - changing
 * the current Z-order. HW limitation.
 *
 * NOTE4: WRT NOTE2 and NOTE3, if the IGD_OVL_PIPEBLEND_SET_ZORDER flag is
 * passed in on every flip, then the Zorder determined by igd_ovl_pipblend_t
 * zorder params will always override NOTE2 adn NOTE3.
 *
 * NOTE4: Current XServer DDX default behavior (user space code) for Clone
 * display configuration is that the z-order will be mirror'ed to the
 * other display pipeline as well. XServer DDX Driver behavior can
 * Kernel module doesnt care about "DC" anymore so it will be up to XSErver
 * and its current XRandR configuration to handle this from client side.
 *
 *--------------------------------------------------------------------------
 */

#endif
