/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_options.c
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
 *  This file containes funtions that parse the driver options from the
 *  X config file. It containes routines to parse both pre-5.0 style
 *  options and the "new-style" non-binary options.
 *
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.options

#include <stdio.h>
#include <string.h>
#include <xorg-server.h>
#include <xf86.h>

#include "emgd.h"
#include "emgd_sprite.h"


/* Enumeration of options */
typedef enum {
	OPTION_SHADOW_FB,
	OPTION_TEAR_FB,
	OPTION_ACCEL_2D,
	OPTION_UXA_OFFSCREEN,
	OPTION_UXA_SOLID,
	OPTION_UXA_COPY,
	OPTION_UXA_COMPOSITE,
	OPTION_UXA_1X1MASK,
	OPTION_UXA_MASK,
	OPTION_HW_CURSOR,
	OPTION_XV_OVERLAY,
	OPTION_XV_BLEND,
	OPTION_XV_MC,
	OPTION_VIDEO_KEY,
	OPTION_OVL_GAMMA_R,
	OPTION_OVL_GAMMA_G,
	OPTION_OVL_GAMMA_B,
	OPTION_OVL_BRIGHTNESS,
	OPTION_OVL_CONTRAST,
	OPTION_OVL_SATURATION,
	OPTION_QB_SPLASH,
	OPTION_DIH_CLONE,
	OPTION_DEBUG,
	OPTION_SPRITE_ASSIGNMENT_D1,
	OPTION_SPRITE_ASSIGNMENT_D2,
	OPTION_SPRITE_ZORDER,
} emgd_options_list;

static OptionInfoRec emgd_options[] = {
	{OPTION_SHADOW_FB,     "ShadowFB",         OPTV_BOOLEAN, {0}, FALSE},
	{OPTION_TEAR_FB,       "TearFB",           OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_ACCEL_2D,      "Accel",            OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_OFFSCREEN, "UXAOffscreen",     OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_SOLID,     "UXASolid",         OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_COPY,      "UXACopy",          OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_COMPOSITE, "UXAComposite",     OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_1X1MASK,   "UXAComposite1x1",  OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_UXA_MASK,      "UXACompositeMask", OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_HW_CURSOR,     "HWcursor",         OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_XV_OVERLAY,    "XVideo",           OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_XV_BLEND,      "XVideoBlend",      OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_XV_BLEND,      "XVideoTexture",    OPTV_BOOLEAN, {0}, TRUE},
	{OPTION_XV_MC,         "XVideoMC",         OPTV_BOOLEAN, {0}, FALSE},
	{OPTION_VIDEO_KEY,     "VideoKey",         OPTV_INTEGER, {0}, FALSE},
	{OPTION_OVL_GAMMA_R,   "XVGammaR",         OPTV_INTEGER, {0x100}, FALSE},
	{OPTION_OVL_GAMMA_G,   "XVGammaG",         OPTV_INTEGER, {0x100}, FALSE},
	{OPTION_OVL_GAMMA_B,   "XVGammaB",         OPTV_INTEGER, {0x100}, FALSE},
	{OPTION_OVL_BRIGHTNESS,"XVBrightness",     OPTV_INTEGER, {0x8000}, FALSE},
	{OPTION_OVL_CONTRAST,  "XVContrast",       OPTV_INTEGER, {0x8000}, FALSE},
	{OPTION_OVL_SATURATION,"XVSaturation",     OPTV_INTEGER, {0x8000}, FALSE},
	{OPTION_DEBUG,         "DebugLevel",       OPTV_ANYSTR,  {0}, FALSE},
	{OPTION_QB_SPLASH,     "SplashScreen",     OPTV_BOOLEAN, {0}, FALSE},
	{OPTION_DIH_CLONE,     "DihCloneEnable",   OPTV_INTEGER, {0}, FALSE},
	{OPTION_SPRITE_ASSIGNMENT_D1,   "SpriteAssignmentD1",   OPTV_ANYSTR, {0}, FALSE},
	{OPTION_SPRITE_ASSIGNMENT_D2,   "SpriteAssignmentD2",   OPTV_ANYSTR, {0}, FALSE},
	{OPTION_SPRITE_ZORDER, "SpriteZorder",     OPTV_ANYSTR,  {0}, FALSE},
	{-1,                   NULL,               OPTV_NONE,    {0}, FALSE},
};



extern PciChipsets emgd_PCIChipsets[];

/* Helper functions */
static void initialize_config(ScrnInfoPtr scrn, emgd_priv_t *iptr);
static void emgd_print_options (ScrnInfoPtr scrn, emgd_priv_t *iptr);
static void GetOptValBool(const OptionInfoRec *emgd_options, int token,
		int *option);
#if DEBUG
static void set_debug_level(ScrnInfoPtr scrn, unsigned long long level);
static unsigned long long str_to_num(char *s);
#endif

/*!
 * emgd_available_options()
 *
 * This function is used by X to get a list of options supported by the
 * driver.  For 5.0, this might have to be dynamiclly generated!
 *
 * @param chipid Device ID to report option list for.
 * @param busid Bus ID to differenciate between multiple intances of chipid
 *
 * @return pointer to option list on success
 * @return NULL if chipid/busid is not supported.
 */
const OptionInfoRec *emgd_available_options (int chipid, int busid)
{
	int i;

	for (i = 0; emgd_PCIChipsets[i].PCIid > 0; i++) {
		if (chipid == emgd_PCIChipsets[i].PCIid) {
			return emgd_options;
		}
	}
	return NULL;
}



/*!
 * emgd_configure()
 *
 * Process the options present in the xorg.conf file.
 */
void emgd_parse_options(ScrnInfoPtr scrn, const char *name)
{
	emgd_priv_t *iptr;
	char *assignment_str;
	int i;

	iptr = EMGDPTR(scrn);

	initialize_config(scrn, iptr);

	xf86ProcessOptions(scrn->scrnIndex, scrn->options, emgd_options);

	GetOptValBool(emgd_options, OPTION_SHADOW_FB, &iptr->cfg.shadow_fb);
	GetOptValBool(emgd_options, OPTION_TEAR_FB, &iptr->cfg.tear_fb);
	GetOptValBool(emgd_options, OPTION_ACCEL_2D, &iptr->cfg.accel_2d);
	GetOptValBool(emgd_options, OPTION_HW_CURSOR, &iptr->cfg.hw_cursor);
	GetOptValBool(emgd_options, OPTION_XV_OVERLAY, &iptr->cfg.xv_overlay);
	GetOptValBool(emgd_options, OPTION_XV_BLEND, &iptr->cfg.xv_blend);
	GetOptValBool(emgd_options, OPTION_XV_BLEND, &iptr->cfg.xv_blend);
	GetOptValBool(emgd_options, OPTION_XV_MC, &iptr->cfg.xv_mc);
	xf86GetOptValInteger(emgd_options, OPTION_VIDEO_KEY,  (int *)&iptr->cfg.video_key);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_GAMMA_R, &iptr->cfg.ovl_gamma_r);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_GAMMA_G, &iptr->cfg.ovl_gamma_g);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_GAMMA_B, &iptr->cfg.ovl_gamma_b);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_BRIGHTNESS, &iptr->cfg.ovl_brightness);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_CONTRAST,  &iptr->cfg.ovl_contrast);
	xf86GetOptValInteger(emgd_options, OPTION_OVL_SATURATION, &iptr->cfg.ovl_saturation);
	GetOptValBool(emgd_options, OPTION_QB_SPLASH, &iptr->cfg.render_offscreen);
	xf86GetOptValInteger(emgd_options, OPTION_DIH_CLONE, (int *)&iptr->cfg.dih_clone_enable);

	GetOptValBool(emgd_options, OPTION_UXA_OFFSCREEN, &iptr->cfg.punt_uxa_offscreen_pixmaps);
	GetOptValBool(emgd_options, OPTION_UXA_SOLID,     &iptr->cfg.punt_uxa_solid);
	GetOptValBool(emgd_options, OPTION_UXA_COPY,      &iptr->cfg.punt_uxa_copy);
	GetOptValBool(emgd_options, OPTION_UXA_COMPOSITE, &iptr->cfg.punt_uxa_composite);
	GetOptValBool(emgd_options, OPTION_UXA_1X1MASK,   &iptr->cfg.punt_uxa_composite_1x1_mask);
	GetOptValBool(emgd_options, OPTION_UXA_MASK,      &iptr->cfg.punt_uxa_composite_mask);

	assignment_str = xf86GetOptValString(emgd_options, OPTION_SPRITE_ASSIGNMENT_D1);
	if (assignment_str) {
		for (i = 0; i < 2; i++) {
			switch (*(assignment_str+i)) {
				case 'V':
					iptr->cfg.sprite_assignment[i] = SPRITE_PREASSIGNED_VA;
					break;
				case 'G':
					iptr->cfg.sprite_assignment[i] = SPRITE_PREASSIGNED_OGL;
					break;
				case 'X':
					iptr->cfg.sprite_assignment[i] = SPRITE_PREASSIGNED_XV;
					break;
				default:
					OS_ERROR("Invalid sprite assignment: %c.", *(assignment_str+i));
					break;
			}
		}
	}

	assignment_str = xf86GetOptValString(emgd_options, OPTION_SPRITE_ASSIGNMENT_D2);
	if (assignment_str) {
		for (i = 0; i < 2; i++) {
			switch (*(assignment_str+i)) {
				case 'V':
					iptr->cfg.sprite_assignment[i+2] = SPRITE_PREASSIGNED_VA;
					break;
				case 'G':
					iptr->cfg.sprite_assignment[i+2] = SPRITE_PREASSIGNED_OGL;
					break;
				case 'X':
					iptr->cfg.sprite_assignment[i+2] = SPRITE_PREASSIGNED_XV;
					break;
				default:
					OS_ERROR("Invalid sprite assignment: %c.", *(assignment_str+i));
					break;
			}
		}
	}

	iptr->cfg.sprite_zorder = xf86GetOptValString(emgd_options, OPTION_SPRITE_ZORDER);

	/*
	 * If all acceleration is turned off, simply punt all the
	 * 2D UXA functions.
	 */
	if (iptr->cfg.accel_2d == FALSE) {
		iptr->cfg.punt_uxa_offscreen_pixmaps = TRUE;
		iptr->cfg.punt_uxa_solid = TRUE;
		iptr->cfg.punt_uxa_copy = TRUE;
		iptr->cfg.punt_uxa_composite = TRUE;
		iptr->cfg.punt_uxa_composite_1x1_mask = TRUE;
		iptr->cfg.punt_uxa_composite_mask = TRUE;
	}

#if DEBUG
	{
		char *debug_str;
		unsigned long long debug_level;

		debug_str = xf86GetOptValString(emgd_options, OPTION_DEBUG);
		if (debug_str) {
			debug_level = str_to_num(debug_str);
			set_debug_level(scrn, debug_level);
		}
	}
#endif

	emgd_print_options(scrn, iptr);
}

/*
 * emgd_print_options
 *
 * Display the status of all the user setable options, as set in the X
 * config file.
 */
static void emgd_print_options (ScrnInfoPtr scrn, emgd_priv_t *iptr)
{
	oal_screen = scrn->scrnIndex;

	OS_PRINT("General Driver Configuration Options");

	OS_PRINT("  FRAMEBUFFER OPTIONS");
	OS_PRINT("    Offscreen FB:         %s",
			(iptr->cfg.render_offscreen) ? "On" : "Off");
	OS_PRINT("    DIH / Clone Switching %s",
			(iptr->cfg.dih_clone_enable) ? "On" : "Off");
	OS_PRINT("    Shadow FB:            %s",
		(iptr->cfg.shadow_fb) ? "On" : "Off");
	OS_PRINT("    Tear FB:              %s",
		(iptr->cfg.tear_fb) ? "On" : "Off");

	OS_PRINT("  HARDWARE ACCELERATION OPTIONS");
	OS_PRINT("    HW 2D Accel:          %s",
		(iptr->cfg.accel_2d) ? "On" : "Off");
	OS_PRINT("    Offscreen Pixmaps:    %s",
			(iptr->cfg.punt_uxa_offscreen_pixmaps) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    Solid fills:          %s",
			(iptr->cfg.punt_uxa_solid) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    Copy:                 %s",
			(iptr->cfg.punt_uxa_copy) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    Composite:            %s",
			(iptr->cfg.punt_uxa_composite) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    Composite 1x1 mask:   %s",
			(iptr->cfg.punt_uxa_composite_1x1_mask) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    Composite mask:       %s",
			(iptr->cfg.punt_uxa_composite_mask) ? "Unaccelerated" : "Accelerated");
	OS_PRINT("    HW Cursor:            %s",
		(iptr->cfg.hw_cursor) ? "On" : "Off");

	OS_PRINT("  XVIDEO OPTIONS");
	OS_PRINT("    XVideo:               %s",
		(iptr->cfg.xv_overlay) ? "On" : "Off");
	OS_PRINT("    XVideoBlend:          %s",
		(iptr->cfg.xv_blend) ? "On" : "Off");
	OS_PRINT("    XVideoMC:             %s",
		(iptr->cfg.xv_mc) ? "On" : "Off");
	OS_PRINT("    XVideoKey:            0x%x", iptr->cfg.video_key);
	OS_PRINT("    Overlay Gamma Red:    0x%x", iptr->cfg.ovl_gamma_r);
	OS_PRINT("    Overlay Gamma Green:  0x%x", iptr->cfg.ovl_gamma_g);
	OS_PRINT("    Overlay Gamma Blue:   0x%x", iptr->cfg.ovl_gamma_b);
	OS_PRINT("    Overlay Brightness:   0x%x", iptr->cfg.ovl_brightness);
	OS_PRINT("    Overlay Contrast:     0x%x", iptr->cfg.ovl_contrast);
	OS_PRINT("    Overlay Saturation:   0x%x", iptr->cfg.ovl_saturation);
}


/*
 * initialize_config
 *
 * Initialize the driver configuration to default values. This is what we
 * get if the device section has nothing in it.
 */
static void initialize_config(ScrnInfoPtr scrn, emgd_priv_t *iptr)
{
	/*
	 * Initialize config structure.
	 */
	memset(&iptr->cfg, 0, sizeof(emgd_config_info_t));

	iptr->cfg.shadow_fb = FALSE;                /* DRM doesn't have */
	iptr->cfg.tear_fb = FALSE;                  /* DRM doesn't have */

	iptr->cfg.accel_2d = TRUE;                  /* DRM doesn't have */
	iptr->cfg.hw_cursor = TRUE;                 /* DRM doesn't have */

	iptr->cfg.xv_overlay = TRUE;                /* DRM doesn't have */
	iptr->cfg.xv_blend = TRUE;                  /* DRM doesn't have */
	iptr->cfg.xv_mc = FALSE;                    /* DRM doesn't have */
	iptr->cfg.video_key = 0xff00ff00;           /* DRM doesn't have */

	/* Default Color Correction Values */
	iptr->cfg.ovl_gamma_r    = 0x100;           /* DRM doesn't have */
	iptr->cfg.ovl_gamma_g    = 0x100;           /* DRM doesn't have */
	iptr->cfg.ovl_gamma_b    = 0x100;           /* DRM doesn't have */
	iptr->cfg.ovl_brightness = 0x8000;          /* DRM doesn't have */
	iptr->cfg.ovl_contrast   = 0x8000;          /* DRM doesn't have */
	iptr->cfg.ovl_saturation = 0x8000;          /* DRM doesn't have */

	/* Default Embedded Features */
	iptr->cfg.render_offscreen = 0;
	iptr->cfg.dih_clone_enable = 0;

	/* Tiled or linear buffers */
	iptr->tiling = INTEL_TILING_ALL;

	/* Default is to not punt any 2D operations */
	iptr->cfg.punt_uxa_offscreen_pixmaps = FALSE;
	iptr->cfg.punt_uxa_solid = FALSE;
	iptr->cfg.punt_uxa_copy = FALSE;
	iptr->cfg.punt_uxa_composite = FALSE;
	iptr->cfg.punt_uxa_composite_1x1_mask = FALSE;
	iptr->cfg.punt_uxa_composite_mask = FALSE;
}


static void GetOptValBool(const OptionInfoRec *emgd_opions,
		int token, int *option)
{
	Bool def_bool;

	def_bool = *option ? TRUE : FALSE;

	*option = xf86ReturnOptValBool(emgd_options, token, def_bool) ?
		TRUE : FALSE;
}

#if DEBUG
/*
 * set_debug_level
 *
 * Set the debug flag for both the IAL and HAL based on the value in the
 * config file. This code is only used if a debug version of the driver
 * is built. Also, make sure we output a warning message to the log file
 * indicating that this is a debug version of the driver so that it never
 * gets released.
 */

static void set_debug_level(ScrnInfoPtr scrn, unsigned long long level)
{
	unsigned long hlevel, ilevel;

	ilevel = (unsigned long)(level >> 32);
	hlevel = (unsigned long)(level & 0xffffffff);

	OS_PRINT("IAL Debugging level set to 0x%lx", ilevel);
	OS_PRINT("HAL Debugging level set to 0x%lx", hlevel);

	memcpy(igd_debug, &level, sizeof(igd_drv_debug_t));

}


/*
 * str_to_num
 *
 * An adapatation of strtoul().
 */
static unsigned long long str_to_num(char *s)
{
	unsigned long long val;
	char *tmp;
	int radix = 10;  /* default is base 10 */

	tmp = s;
	val = 0;

	/*
	 * Is this a hex or octal number?
	 * hex numbers start with 0x
	 * octal start with 0
	 */
	if (*tmp == '0') {
		tmp++;
		if ((*tmp == 'x') || (*tmp == 'X')) {
			tmp++;
			radix = 16;
		} else {
			radix = 8;
		}
	}

	while (*tmp) {
		if ((*tmp >= '0') && (*tmp <= ((radix == 8) ? '7' : '9'))) {
			val = (val * radix) + (unsigned long)(*tmp - '0');
		} else if ((radix == 16) && (*tmp >= 'a') && (*tmp <= 'f')) {
			val = (val * radix) + 10 + (unsigned long)(*tmp - 'a');
		} else if ((radix == 16) && (*tmp >= 'A') && (*tmp <= 'F')) {
			val = (val * radix) + 10 + (unsigned long)(*tmp - 'A');
		} else {
			return val;
		}
		tmp++;
	}

	return val;
}
#endif

