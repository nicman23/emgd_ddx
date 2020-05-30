/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_video.c
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
 *  XFree86 Video Extension (Xv)
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.video

#include <fourcc.h>
#include <X11/extensions/Xv.h>
#include <xf86drm.h>

#include "emgd.h"
#include "emgd_video.h"
#include "emgd_sprite.h"
#include "emgd_uxa.h"

#define USE_OVERLAY  0

// Currently don't enable the vaxv interface.
#define VAXV


#ifdef VAXV
#include "va_xv_ddx.h"
#endif

#if 0
/* Turn on tracing for this file only */
#define OS_DEBUG _OS_DEBUG
#define OS_TRACE_ENTER OS_DEBUG("ENTER")
#define OS_TRACE_EXIT OS_DEBUG("EXIT")
#endif

#define IMAGE_IMPOSSIBLE_MAX_WIDTH	10000
#define IMAGE_IMPOSSIBLE_MAX_HEIGHT	10000
#define IMAGE_SUGGESTED_MAX_WIDTH	2048
#define IMAGE_SUGGESTED_MAX_HEIGHT	2048
#define IMAGE_DEFAULT_WIDTH 320
#define IMAGE_DEFAULT_HEIGHT 200
#define OFF_DELAY 250 /* milliseconds */
#define OFF_TIMER 0x01
#define CLIENT_VIDEO_ON 0x02

void DamageDamageRegion(DrawablePtr pDrawable, const RegionPtr pRegion);

static XF86VideoEncodingRec XvOverlayEncoding[4] = {
	{ 0,
		"XV_IMAGE",
		IMAGE_IMPOSSIBLE_MAX_WIDTH,
		IMAGE_IMPOSSIBLE_MAX_HEIGHT,
		{1,1}
	},
	{ 1,
		"XV_YUV_PACKED_IMAGE",
		IMAGE_IMPOSSIBLE_MAX_WIDTH,
		IMAGE_IMPOSSIBLE_MAX_HEIGHT,
		{1,1}
	},
	{ 2,
		"XV_YUV_PLANAR_IMAGE",
		IMAGE_IMPOSSIBLE_MAX_WIDTH,
		IMAGE_IMPOSSIBLE_MAX_HEIGHT,
		{1,1}
	},
	{ 3,
		"XV_RGB_IMAGE",
		IMAGE_IMPOSSIBLE_MAX_WIDTH,
		IMAGE_IMPOSSIBLE_MAX_HEIGHT,
		{1,1}
	},
};

static XF86VideoEncodingRec XvBlendEncoding[1] = {
	{ 0,
		"XV_IMAGE",
		IMAGE_SUGGESTED_MAX_WIDTH,
		IMAGE_SUGGESTED_MAX_HEIGHT,
		{1,}
	}
};

static XF86VideoFormatRec emgd_formats[] = {
	{ 15, TrueColor }, { 16, TrueColor }, { 24, TrueColor }
};
#define NUM_FORMATS (sizeof(emgd_formats)/sizeof(XF86VideoFormatRec))


static XF86AttributeRec attributes[] = {
	{ XvSettable | XvGettable, 0, 0x00ffffff, "XV_COLORKEY" },
	{ XvSettable | XvGettable, 0, 65535, "XV_SATURATION" },
	{ XvSettable | XvGettable, 0, 65535, "XV_BRIGHTNESS" },
	{ XvSettable | XvGettable, 0, 65535, "XV_CONTRAST" },
	{ XvSettable | XvGettable, 0, 65535, "XV_ALPHA" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA0" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA1" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA2" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA3" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA4" },
	{ XvSettable | XvGettable, 0, 65535, "XV_GAMMA5" },
#ifdef FOURCC_VAXV
	{ XvSettable | XvGettable, 0, 2, VAXV_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, VAXV_VERSION_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, VAXV_CAPS_ATTR_STR },
	{ XvGettable, -1, 0x7fffffff, VAXV_FORMAT_ATTR_STR },
#endif
};
#define NUM_ATTRIBUTES (sizeof(attributes)/sizeof(XF86AttributeRec))

static XF86ImageRec emgd_images[] = {
	XVIMAGE_YUY2,
	XVIMAGE_UYVY,
	XVIMAGE_YV12,
	XVIMAGE_I420,
#ifdef INTEL_XVMC
	{
		FOURCC_XVMC,
		XvYUV,
		LSBFirst,
		{'X', 'V', 'M', 'C',
			0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00,
			0x38, 0x9B, 0x71},
		12,
		XvPlanar,
		3,
		0, 0, 0, 0,
		8, 8, 8,
		1, 2, 2,
		1, 2, 2,
		{'Y', 'V', 'U',
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		XvTopToBottom
	},
#endif
#ifdef FOURCC_VAXV
	{
		FOURCC_VAXV,
		XvYUV,
		LSBFirst,
		{'V', 'A', 'X', 'V',
			0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00,
			0x38, 0x9B, 0x71},
		12,
		XvPlanar,
		3,
		0, 0, 0, 0,
		8, 8, 8,
		1, 2, 2,
		1, 2, 2,
		{'Y', 'V', 'U',
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		XvTopToBottom
	},
#endif
};

#define NUM_IMAGES (sizeof(emgd_images)/sizeof(XF86ImageRec))
#define NUM_TEXTURE_IMAGES 4


#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

/***************************************************************************
 * Function prototypes
 ***************************************************************************/
static XF86VideoAdaptorPtr emgd_setup_overlay_adaptor(ScreenPtr screen);
static XF86VideoAdaptorPtr emgd_setup_blend_adaptor(ScreenPtr screen);

static int emgd_set_port_attribute(ScrnInfoPtr scrn, Atom attribute,
	INT32 value, pointer data);

static int emgd_get_port_attribute(ScrnInfoPtr scrn, Atom attribute,
	INT32 *value, pointer data);

int emgd_overlay_put_image(ScrnInfoPtr scrn, short src_x, short src_y,
	short drw_x, short drw_y, short src_w, short src_h,
	short drw_w, short drw_h, int id, unsigned char* buf,
	short width, short height, Bool sync,
	RegionPtr clipBoxes, pointer data, DrawablePtr pDraw);
static int emgd_blend_put_image(ScrnInfoPtr scrn, short src_x, short src_y,
	short drw_x, short drw_y, short src_w, short src_h,
	short drw_w, short drw_h, int id, unsigned char* buf,
	short width, short height, Bool sync,
	RegionPtr clipBoxes, pointer data, DrawablePtr pDraw);

static Bool
emgd_clip_video_helper(ScrnInfoPtr scrn, int id,
                        short src_x, short src_y, short src_w, short src_h,
                        short drw_x, short drw_y, short drw_w, short drw_h,
                        int *top, int *left, int *npixels, int *nlines,
                        RegionPtr clipBoxes, INT32 width, INT32 height);

static int emgd_query_image_attriabutes(ScrnInfoPtr scrn, int id,
	unsigned short *w, unsigned short *h,
	int *pitches, int *offset);

static void emgd_stop_video(ScrnInfoPtr scrn, pointer data, Bool cleanup);

static void emgd_query_best_size(ScrnInfoPtr scrn, Bool motion,
	short vid_w, short vid_h, short draw_w, short draw_h,
	unsigned int *p_w, unsigned int *p_h, pointer data);

void emgd_destroy_video(ScreenPtr screen);
void Gen6DisplayVideoTextured(ScrnInfoPtr scrn,
		intel_adaptor_private *adaptor_priv, int id,
		RegionPtr dstRegion,
		short width, short height,
		int video_pitch, int video_pitch2,
		short src_w, short src_h,
		short drw_w, short drw_h, PixmapPtr pixmap);
void Gen7ConvertVideoTextured(ScrnInfoPtr scrn,
		intel_adaptor_private *adaptor_priv, int id,
		short width, short height,
		int video_pitch, int video_pitch2,
		short src_w, short src_h,
		short drw_w, short drw_h, PixmapPtr pixmap);


static Atom xv_colorkey, xv_brightness, xv_contrast, xv_saturation, xv_alpha;
static Atom xv_gamma0, xv_gamma1, xv_gamma2, xv_gamma3, xv_gamma4, xv_gamma5;

#ifdef FOURCC_VAXV
static Atom xv_va_shmbo;
static Atom xv_va_version;
static Atom xv_va_caps;
static Atom xv_va_format;
#endif

/***************************************************************************
 * Functions
 ***************************************************************************/


/* Kernel modesetting overlay functions */
static Bool emgd_has_overlay(emgd_priv_t *iptr)
{
	struct drm_i915_getparam param;
	int overlay = 0;
	int ret;

	param.param = I915_PARAM_HAS_OVERLAY;
	param.value = &overlay;

	ret = drmCommandWriteRead(iptr->drm_fd, DRM_I915_GETPARAM, &param,
			sizeof(param));
	if (ret) {
		OS_ERROR("DRM reports no overlay support!\n");
		return FALSE;
	}

	return overlay;
}


/*
 * Check the format ID. If planar return 1 else return 0
 */
int is_planar_fourcc(int id)
{
	switch (id) {
	case FOURCC_YV12:
	case FOURCC_I420:
#ifdef INTEL_XVMC
	case FOURCC_XVMC:
#endif
#ifdef FOURCC_VAXV
	case FOURCC_VAXV:
#endif
		return 1;
	case FOURCC_UYVY:
	case FOURCC_YUY2:
		return 0;
	default:
		OS_ERROR("Unknown format 0x%x", id);
		return 0;
	}
}


/* Send the overlay attriabutes to the DRM module */
static void emgd_update_overlay_attributes(emgd_priv_t *iptr)
{

	emgd_xv_t *priv;
	struct drm_intel_sprite_colorkey ckey;
	struct drm_intel_sprite_colorcorrect ccorrect;
	int num_planes, i;

	priv = (emgd_xv_t *)iptr->xvOverlayAdaptor->pPortPrivates[0].ptr;

	if(priv->ovlplane[0]) {
		if (NULL == priv->ovlplane[1]) {
			num_planes = 1;
		} else {
			num_planes = 2;
		}
		
		for (i = 0; i < num_planes; i++) {
			ckey.plane_id  = priv->ovlplane[i]->plane_id;
			ckey.min_value = priv->xv_colorkey;
			ckey.flags     = I915_SET_COLORKEY_DESTINATION;
			ckey.channel_mask = 0x00ffffff;

			ccorrect.brightness = priv->xv_brightness;
			ccorrect.contrast   = priv->xv_contrast;
			ccorrect.saturation = priv->xv_saturation;

			emgd_sprite_update_attr(iptr,
					&ckey, &ccorrect, NULL, 0);
		}
	}
/*
	emgd_xv_t *priv;
	struct drm_intel_overlay_attrs attrs;

	priv = (emgd_xv_t *)iptr->xvOverlayAdaptor->pPortPrivates[0].ptr;

	attrs.flags      = I915_OVERLAY_UPDATE_ATTRS;

	attrs.brightness = priv->xv_brightness;
	attrs.contrast   = priv->xv_contrast;
	attrs.saturation = priv->xv_saturation;
	attrs.gamma0     = priv->xv_gamma0;
	attrs.gamma1     = priv->xv_gamma1;
	attrs.gamma2     = priv->xv_gamma2;
	attrs.gamma3     = priv->xv_gamma3;
	attrs.gamma4     = priv->xv_gamma4;
	attrs.gamma5     = priv->xv_gamma5;

	drmCommandWriteRead(iptr->drm_fd, DRM_I915_OVERLAY_ATTRS, &attrs,
			sizeof(attrs));
			
	Replace with new sprite functions common to all 
	of DDX that calls new IOCTLs
*/

}


/*
 * Main entry point to initialize support for the XV video port extension
 */
Bool emgd_init_video(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
	XF86VideoAdaptorPtr overlay_adaptor = NULL;
	XF86VideoAdaptorPtr blend_adaptor = NULL;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	int num_adaptors;
	int num_new_adaptors = 0;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;


	if(iptr->cfg.xv_overlay && emgd_has_overlay(iptr)){
		overlay_adaptor = emgd_setup_overlay_adaptor(screen);
		if(!overlay_adaptor) {
			OS_PRINT("No hardware overlay capabilities found.");
			OS_DEBUG("no overlay_adaptor");
		}else{
			num_new_adaptors++;
		}
	} else {
		OS_PRINT("Hardware based XV adaptors disabled.");
	}

	if(iptr->cfg.xv_blend){
		blend_adaptor = emgd_setup_blend_adaptor(screen);
		if(!blend_adaptor) {
			OS_DEBUG("no blend_adaptor");
			OS_PRINT("Texture based XV initialization failed.");
		}else{
			num_new_adaptors++;
		}
	} else {
		OS_PRINT("Texture based XV adaptors disabled.");
	}

	if(!num_new_adaptors) {
		OS_TRACE_EXIT;
		return FALSE;
	}

	/* Expand the apdaptor list so it can hold our new adaptors */
	num_adaptors = xf86XVListGenericAdaptors(scrn, &adaptors);

	newAdaptors = malloc((num_adaptors + num_new_adaptors) *
			sizeof(XF86VideoAdaptorPtr*));
	if(newAdaptors) {
		memcpy(newAdaptors, adaptors,
				num_adaptors * sizeof(XF86VideoAdaptorPtr));

		/* Always prefer to use overlay */
		if(overlay_adaptor) {
			newAdaptors[num_adaptors++] = overlay_adaptor;
		}
		if(blend_adaptor){
			newAdaptors[num_adaptors++] = blend_adaptor;
		}

		if(num_adaptors) {
			xf86XVScreenInit(screen, newAdaptors, num_adaptors);
		}

		free(newAdaptors);
	}

	OS_TRACE_EXIT;
	return TRUE;
}



/*
 * Initialize a video adaptor based on the hardware overlay.
 */
static XF86VideoAdaptorPtr emgd_setup_overlay_adaptor(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrn);
	XF86VideoAdaptorPtr adapt;
	emgd_xv_t *privs;
	DevUnion *d_unions;
	emgd_xv_t *priv;
	int nports = 4;
	int i = 0;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	/* allocate the adaptor port private structure */
	adapt = calloc(1, sizeof(XF86VideoAdaptorRec));
	if(!adapt) {
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		return NULL;
	}

	privs = calloc(nports, sizeof(emgd_xv_t));
	if(!privs) {
		free(adapt);
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		return NULL;
	}

	d_unions = calloc(nports, sizeof(DevUnion));
	if(!d_unions) {
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		free(privs);
		free(adapt);
		return NULL;
	}

	adapt->type          = XvWindowMask | XvInputMask | XvImageMask;
	adapt->flags         = VIDEO_OVERLAID_IMAGES;
	adapt->name          = "EMGD Video Using Overlay";
	adapt->nFormats      = NUM_FORMATS;
	adapt->pFormats      = emgd_formats;
	adapt->nPorts        = nports;
	adapt->pPortPrivates = d_unions;
	adapt->nAttributes	 = NUM_ATTRIBUTES;
	adapt->pAttributes	 = attributes;
	adapt->nImages		 = NUM_IMAGES;
	adapt->pImages		 = emgd_images;
	adapt->PutVideo		 = NULL;
	adapt->PutStill		 = NULL;
	adapt->GetVideo		 = NULL;
	adapt->GetStill		 = NULL;
	adapt->StopVideo	 = emgd_stop_video;
	adapt->PutImage		 = emgd_overlay_put_image;
	adapt->SetPortAttribute	= emgd_set_port_attribute;
	adapt->GetPortAttribute	= emgd_get_port_attribute;
	adapt->QueryBestSize	= emgd_query_best_size;
	adapt->QueryImageAttributes = emgd_query_image_attriabutes;

	/*
	 * TODO: Query the DRM driver to get the max width/height for each
	 * overlay format.
	 * Honestly, it is not actual now...
	 */
	adapt->nEncodings		= 4;
	adapt->pEncodings		= XvOverlayEncoding;
	iptr->xvOverlayAdaptor = adapt;

	/*
	 * Providing support for 16 XV ports
	 */
	for (i = 0; i < nports; i++) {
		priv = &privs[i];
		/* Default values for the overlay attributes */
		priv->xv_colorkey = iptr->cfg.video_key;
		priv->xv_brightness = -19; /* 255/219 * -16 */
		priv->xv_contrast   = 75;  /* 255/219 * 64 */
		priv->xv_saturation = 146; /* 128/112 * 128 */
		priv->xv_gamma0 = 0x080808;
		priv->xv_gamma1 = 0x101010;
		priv->xv_gamma2 = 0x202020;
		priv->xv_gamma3 = 0x404040;
		priv->xv_gamma4 = 0x808080;
		priv->xv_gamma5 = 0xc0c0c0;
		priv->xv_alpha = 0x00000000;
		priv->ov_type = EGD_XV_TYPE_OVERLAY;
		priv->disable = FALSE;
		REGION_NULL(screen, &priv->clip);
		adapt->pPortPrivates[i].ptr = (pointer)(priv);
	}

	/* Make ATOMs for all the overlay attributes. */
	xv_colorkey = MAKE_ATOM("XV_COLORKEY");
	xv_brightness = MAKE_ATOM("XV_BRIGHTNESS");
	xv_contrast = MAKE_ATOM("XV_CONTRAST");
	xv_saturation = MAKE_ATOM("XV_SATURATION");
	xv_gamma0 = MAKE_ATOM("XV_GAMMA0");
	xv_gamma1 = MAKE_ATOM("XV_GAMMA1");
	xv_gamma2 = MAKE_ATOM("XV_GAMMA2");
	xv_gamma3 = MAKE_ATOM("XV_GAMMA3");
	xv_gamma4 = MAKE_ATOM("XV_GAMMA4");
	xv_gamma5 = MAKE_ATOM("XV_GAMMA5");
#ifdef FOURCC_VAXV
	xv_va_shmbo = MAKE_ATOM(VAXV_ATTR_STR);
	xv_va_version = MAKE_ATOM(VAXV_VERSION_ATTR_STR);
	xv_va_caps = MAKE_ATOM(VAXV_CAPS_ATTR_STR);
	xv_va_format = MAKE_ATOM(VAXV_FORMAT_ATTR_STR);
#endif

	/* Update the DRM with the current (default) overlay attributes */
	/* Actually it is not needed now */
	emgd_update_overlay_attributes(iptr);

	OS_TRACE_EXIT;
	return adapt;
}


/*
 * Initialize a video port that uses the 3D engine.
 */
static XF86VideoAdaptorPtr emgd_setup_blend_adaptor(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrn);
	XF86VideoAdaptorPtr adapt;
	emgd_xv_t *privs;
	emgd_xv_t *priv;
	DevUnion *d_unions;
	int nports = 16;
	int i;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	/* allocate the port private structure */
	adapt = calloc(1, sizeof(XF86VideoAdaptorRec));
	if(!adapt) {
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		return NULL;
	}

	privs = calloc(nports, sizeof(emgd_xv_t));
	if(!privs) {
		free(adapt);
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		return NULL;
	}

	d_unions = calloc(nports, sizeof(DevUnion));
	if(!d_unions) {
		OS_ERROR("Allocate XF86VideoAdaptorRec fail");
		free(privs);
		free(adapt);
		return NULL;
	}

	adapt->type          = XvWindowMask | XvInputMask | XvImageMask;
	adapt->flags         = VIDEO_OVERLAID_IMAGES;
	adapt->name          = "EMGD Video Using Textures";
	adapt->nEncodings    = 1;
	adapt->pEncodings    = XvBlendEncoding;
	adapt->nFormats      = NUM_FORMATS;
	adapt->pFormats      = emgd_formats;
	adapt->nPorts        = nports;
	adapt->pPortPrivates = d_unions;
	adapt->nAttributes   = 0;
	adapt->pAttributes   = NULL;
	adapt->nImages       = NUM_TEXTURE_IMAGES;
	adapt->pImages       = emgd_images;
	adapt->PutVideo      = NULL;
	adapt->PutStill      = NULL;
	adapt->GetVideo      = NULL;
	adapt->GetStill      = NULL;
	adapt->StopVideo     = emgd_stop_video;
	adapt->PutImage      = emgd_blend_put_image;
	adapt->SetPortAttribute = emgd_set_port_attribute;
	adapt->GetPortAttribute = emgd_get_port_attribute;
	adapt->QueryBestSize    = emgd_query_best_size;
	adapt->QueryImageAttributes = emgd_query_image_attriabutes;

	/*
	 * Providing support for 16 XV ports
	 */
	for (i = 0; i < nports; i++) {
		priv = &privs[i];
		priv->disable = FALSE;
		priv->ov_type = EGD_XV_TYPE_BLEND;
		priv->xv_alpha = 0x00000000;
		priv->blend_colorkey_val = iptr->cfg.video_key ^ 1;
		/*
		 * The color key for blend is disabled by default.  The default
		 * alpha for the color key is 0xff.  If the color key is set in
		 * the config file, then it is & 0x00 for the alpha.  So checking
		 * if alpha is non-zero indicates it is using the default alpha and
		 * thus disabled for blend.
		 * It is only enabled if the color key is set in the config file
		 * or the application sets the color key.
		 */
		if (priv->blend_colorkey_val & 0xff000000) {
			priv->flags = XV_BLEND_COLORKEY_DISABLED;
		} else {
			priv->flags = XV_BLEND_COLORKEY_ENABLED;
		}

		REGION_NULL(screen, &priv->clip);

		adapt->pPortPrivates[i].ptr = (pointer)(priv);
	}

	iptr->xvBlendAdaptor = adapt;

	OS_TRACE_EXIT;
	return adapt;
}



/*
 * Set an adaptor port attribute.
 *
 * This updates our internal data structures with the attribute
 * value. This works for texture based ports but is untested for
 * overlay/sprite based ports.
 */
static int emgd_set_port_attribute(
	ScrnInfoPtr scrn,
	Atom attribute,
	INT32 value,
	pointer data)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);
	emgd_priv_t *iptr_prim = iptr->entity->primary;
	emgd_xv_t *priv = (emgd_xv_t *)data;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	/*
	 * There is only 1 set of video quality and gamma settings, so
	 * always use the primary.
	 */
	if(attribute == xv_brightness){
		if ((value < -128) || (value > 127)) {
			return BadValue;
		}
		iptr_prim->cfg.ovl_brightness = value;
		priv->xv_brightness = value;
	} else if(attribute == xv_contrast){
		if ((value < 0) || (value > 255)) {
			return BadValue;
		}
		iptr_prim->cfg.ovl_contrast = value;
		priv->xv_contrast = value;
	} else if(attribute == xv_saturation){
		if ((value < 0) || (value > 1023)) {
			return BadValue;
		}
		priv->xv_saturation = value;
		iptr_prim->cfg.ovl_saturation = value;
	} else if(attribute == xv_colorkey) {
		priv->xv_colorkey = value;
		REGION_EMPTY(scrn->pScreen, &priv->clip);
#ifdef FOURCC_VAXV
	} else if(attribute == xv_va_shmbo) {
		priv->vaxv = value;
#endif
	} else if(attribute == xv_gamma0) {
		/* Convert the input R-G-B gamma (which is in  3i.5f
		 * format) to 24i.8f format */
		iptr_prim->cfg.ovl_gamma_r = ((value >> 16) & 0xFF) << 3;
		iptr_prim->cfg.ovl_gamma_g = ((value >> 8) & 0xFF) << 3;
		iptr_prim->cfg.ovl_gamma_b = (value & 0xFF) << 3;
		priv->xv_gamma0 = value;
	} else if(attribute == xv_gamma1) {
		priv->xv_gamma1 = value;
	} else if(attribute == xv_gamma2) {
		priv->xv_gamma2 = value;
	} else if(attribute == xv_gamma3) {
		priv->xv_gamma3 = value;
	} else if(attribute == xv_gamma4) {
		priv->xv_gamma4 = value;
	} else if(attribute == xv_gamma5) {
		priv->xv_gamma5 = value;
	} else {
		return BadMatch;
	}

	OS_TRACE_EXIT;
	return Success;
}


/*
 * Get the current setting for an adaptor port attribute and
 * return it.
 */
static int emgd_get_port_attribute(
	ScrnInfoPtr scrn,
	Atom attribute,
	INT32 *value,
	pointer data)
{
	emgd_xv_t * priv = (emgd_xv_t *)data;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	if(attribute == xv_brightness) {
		*value = priv->xv_brightness;
	} else if(attribute == xv_contrast) {
		*value = priv->xv_contrast;
	} else if(attribute == xv_saturation) {
		*value = priv->xv_saturation;
	} else if(attribute == xv_gamma0){
		*value = priv->xv_gamma0;
	} else if(attribute == xv_gamma1){
		*value = priv->xv_gamma1;
	} else if(attribute == xv_gamma2){
		*value = priv->xv_gamma2;
	} else if(attribute == xv_gamma3){
		*value = priv->xv_gamma3;
	} else if(attribute == xv_gamma4){
		*value = priv->xv_gamma4;
	} else if(attribute == xv_gamma5){
		*value = priv->xv_gamma5;
	} else if(attribute == xv_colorkey) {
		*value = priv->xv_colorkey;
	} else if(attribute == xv_alpha){
		*value = 0;

#ifdef FOURCC_VAXV
	} else if (attribute == xv_va_shmbo) {
		*value = priv->vaxv;
	} else if (attribute == xv_va_version) {
		*value = VAXV_VERSION;
	} else if (attribute == xv_va_caps) {
		*value = VAXV_CAP_ROTATION_180 | VAXV_CAP_DIRECT_BUFFER;
	} else if (attribute == xv_va_format) {
		*value = FOURCC_YUY2;
#endif
	}

	OS_TRACE_EXIT;
	return Success;
}

static void emgd_setup_dst_params(
		ScrnInfoPtr scrn,
		emgd_xv_t *priv,
		short width,
		short height,
		int *dst_pitch,
		int *dst_pitch2,
		int *size,
		int id)
{
	int pitch_align;

	if (priv->ov_type == EGD_XV_TYPE_BLEND) {
		pitch_align = 4;
	} else {
		/* Overlay has stricter requirements.
		 * Actually the alignment is 64 bytes. But the stride must
		 * be at least 512 bytes.  Take the easy fix and align on 512
		 * bytes unconditionally.
		 */
		pitch_align = 512;
	}

	/* The destination pitch is determined by the surface format. */
	if (is_planar_fourcc(id)) {
		if (priv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			*dst_pitch  = ALIGN((height / 2), pitch_align);
			*dst_pitch2 = ALIGN(height, pitch_align);
			*size = *dst_pitch * width * 3;
		} else {
			*dst_pitch  = ALIGN((width / 2), pitch_align);
			*dst_pitch2 = ALIGN(width, pitch_align);
			*size = *dst_pitch * height * 3;
		}
	} else {
		if (priv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			*dst_pitch = ALIGN((height << 1), pitch_align);
			*size = *dst_pitch * width;
		} else {
			*dst_pitch = ALIGN((width << 1), pitch_align);
			*size = *dst_pitch * height;
		}
		*dst_pitch2 = 0;
	}

	/* Configure the offsets into the buffer */
	priv->YBufOffset = 0;

	if (priv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
		priv->UBufOffset = priv->YBufOffset + (*dst_pitch2 * width);
		priv->VBufOffset = priv->UBufOffset + (*dst_pitch * width / 2);
	} else {
		priv->UBufOffset = priv->YBufOffset + (*dst_pitch2 * height);
		priv->VBufOffset = priv->UBufOffset + (*dst_pitch * height / 2);
	}
}



/* Get the coordinates of the src and dest rectangle.
 * The src_rect is the surface located in system memory.
 * The dest_rect is the location on the framebuffer.  Rotation, flip, and render
 * scaling will also be compensated for.
 */

int get_src_dest_rect(
	ScrnInfoPtr scrn,
	short src_x, short src_y,
	short drw_x, short drw_y,
	short src_w, short src_h,
	short drw_w, short drw_h,
	short width, short height,
	unsigned char calc_4_blend,
	unsigned int *src_render_ops,
	igd_rect_t *src_rect,
	igd_rect_t *dest_rect,
	RegionPtr clipBoxes)
{
	BoxRec dstBox;
	INT32 x1, x2, y1, y2;
#if 0
	emgd_priv_t *iptr = EMGDPTR(scrn);
	unsigned long xscale, yscale;
#endif

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	/* src_x, src_y are xy coordinates in the source image
	 * having (src_x + src_w)/2 would chop off the right half
	 * of the original source image */
	x1 = src_x;
	x2 = src_x + src_w;
	y1 = src_y;
	y2 = src_y + src_h;

	/* drw_x, drw_y are xy coordinates in entire virtual desktop
	 * meaning they are the absolute offset from 0,0 of virtual desktop */
	dstBox.x1 = drw_x;
	dstBox.x2 = drw_x + drw_w;
	dstBox.y1 = drw_y;
	dstBox.y2 = drw_y + drw_h;

	/* Sanity check the destination rectangle to 
	 * make sure it has no negative coordinates. */
	if(REGION_NUM_RECTS(clipBoxes) == 1) {
		BoxPtr box = &(clipBoxes->extents);
		if (box->x1 < 0) {
			OS_DEBUG("  clipBox x1 negative");
			box->x1 = 0;
		}
		if (box->x2 < 0) {
			OS_DEBUG("  clipBox x2 negative");

			box->x2 = dstBox.x2;
		}
		if (box->y1 < 0) {
			OS_DEBUG("  clipBox y1 negative");
			box->y1 = 0;
		}
		if (box->y2 < 0) {
			OS_DEBUG("  clipBox y2 negative");
			box->y2 = dstBox.y2;
		}

		if (box->x2 > dstBox.x2) {
			OS_DEBUG("  clipBox x2 needs clip");
			box->x2 = dstBox.x2;
		}
		if (box->y2 > dstBox.y2) {
			OS_DEBUG("  clipBox y2 needs clip");
			box->y2 = dstBox.y2;
		}

		if (box->x1 > box->x2) {
			OS_DEBUG("  clipBox x1 > x2");
			box->x1 = box->x2 - 1;
		}
		if (box->y1 > box->y2) {
			OS_DEBUG("  clipBox y1 > y2");
			box->y1 = box->y2 - 1;
		}

	}

	/* This clips the source to screen coordinates.  */
	if(!xf86XVClipVideoHelper(&dstBox,&x1,&x2,&y1,&y2,clipBoxes,
			width,height)) {
		return Success;
	}

	/*
	 * Check the rects in the clipBoxes. The clip regions are generated
	 * by the X server PutImage code and in at least one case, it has
	 * generated invalid regions.
	 *
	 * Clip the  ClipBoxes here.
	 */
	if(REGION_NUM_RECTS(clipBoxes) > 1) {
		int rectloop;
		OS_DEBUG("Checking %lu clipBoxes for out-of-bounds.",
				(unsigned long)REGION_NUM_RECTS(clipBoxes));
		for(rectloop = 0; rectloop < REGION_SIZE(clipBoxes); ++rectloop) {
			BoxPtr box = REGION_BOX(clipBoxes, rectloop);
			if (box->x1 < 0) {
				OS_DEBUG("  clipBox %d x1 negative", rectloop);
				box->x1 = 0;
			}
			if (box->x2 < 0) {
				OS_DEBUG("  clipBox %d x2 negative", rectloop);
				box->x2 = dstBox.x2;
			}
			if (box->y1 < 0) {
				OS_DEBUG("  clipBox %d y1 negative", rectloop);
				box->y1 = 0;
			}
			if (box->y2 < 0) {
				OS_DEBUG("  clipBox %d y2 negative", rectloop);
				box->y2 = dstBox.y2;
			}
			if (box->x2 > dstBox.x2) {
				OS_DEBUG("  clipBox %d x2 needs clip", rectloop);
				box->x2 = dstBox.x2;
			}
			if (box->y2 > dstBox.y2) {
				OS_DEBUG("  clipBox %d y2 needs clip", rectloop);
				box->y2 = dstBox.y2;
			}
			if (box->x1 > box->x2) {
				OS_DEBUG("  clipBox %d x1 > x2", rectloop);
				box->x1 = box->x2 - 1;
			}
			if (box->y1 > box->y2) {
				OS_DEBUG("  clipBox %d y1 > y2", rectloop);
				box->y1 = box->y2 - 1;
			}
		}
	}

	src_rect->x1 = x1 >> 16;
	src_rect->y1 = y1 >> 16;
	src_rect->x2 = x2 >> 16;
	src_rect->y2 = y2 >> 16;

	/* scrnInfo->frameX0 and scrnInfo->frameY0 are
	 * the relative offset of current viewport/monitor from origin of
	 * virtual desktop (which is the framebuffer itself) */
	dest_rect->x1 = dstBox.x1;
	dest_rect->x2 = dstBox.x2;
	dest_rect->y1 = dstBox.y1;
	dest_rect->y2 = dstBox.y2;


#if 0
	/* update the blend surface with the new incoming image */
	get_rotate_rndscale_coord(iptr, &dstBox,
		dest_rect, calc_4_blend,
		src_render_ops, xscale, yscale);
#endif

	OS_TRACE_EXIT;
	return 0;
}

/* Copy the surface located in system memory to video memory for use by the
 * overlay or blend.  This will also ensure the tmp_surface in video memory
 * is large enough. */
static int copy_to_emgd_vid_mem(
		ScrnInfoPtr scrn,
		int id, unsigned char* buf,
		short width, short height,
		emgd_xv_t *priv,
		int *dst_pitch,
		int *dst_pitch2,
		int start_x,
		int start_y,
		int new_src_w,
		int new_src_h)
{
	emgd_priv_t *iptr = EMGDPTR(scrn);
	int alloc_size;

	/* Surface located in system memory being copied from */
	unsigned long surface_src = (unsigned long)buf;
	unsigned long surface_u_src = 0;
	unsigned long surface_v_src = 0;
	unsigned long surface_uv_pitch = width;

	/* Surface located in video memory being copied to */
	unsigned char *surface_dest;
	unsigned char *surface_u_dest;
	unsigned char *surface_v_dest;

	unsigned long surface_bpp;
	int tmp_start_x, tmp_start_y; 
	int tmp_new_src_w, tmp_new_src_h;
	int round_x, round_y;
	/* Flag to convert planar to packed when copying from system memory
	 * to video memory. */
	int convert_planar = 1;
	int planar = 0;
	int new_id = id;
	unsigned int lines;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	tmp_start_x = start_x;
	tmp_start_y = start_y;
	tmp_new_src_w = new_src_w;
	tmp_new_src_h = new_src_h;

	/* Copy a couple of extra pixels, since when blending, pixels which are 
	 * outside the x, y area may partially be included */	
 	if(tmp_start_x >= 2){
		tmp_start_x -= 2;
	} else {
		tmp_start_x = 0;
	}
	if(tmp_start_y >= 2){
		tmp_start_y -= 2;
	} else {
		tmp_start_y = 0;
	}
	if(tmp_new_src_w <= (width -2)) {
		tmp_new_src_w += 2;
	} else {
		tmp_new_src_w = width;
	}
	if(tmp_new_src_h <= (height -2)) {
		tmp_new_src_h += 2;
	} else {
		tmp_new_src_h = height;
	}

	/*
	 * Note: Since we are copying anyway we will convert the planar to
	 * packed. It will then go through the blend faster.
	 */
	switch(id){
	case FOURCC_YV12:
		OS_DEBUG("Overlay pf: YV12");
		surface_bpp = 1;
		planar = 1;
		if (convert_planar) {
			/* Converts to FOURCC_YUY2 */
			new_id = FOURCC_YUY2;
		}

		/* Make sure width is aligned to 4 pixel boundary */
		width = (width + 3) & ~3;

		/* The U and V places are aligned to 8 pixel boundary */
		surface_uv_pitch = ((width + 7) & ~7) >> 1;

		surface_v_src = surface_src + height * width;
		surface_u_src = surface_v_src + (height >> 1) * surface_uv_pitch;
#if 0		/* FIXME: Comment out this two lines follow like EMGD TNC driver.
		 * Because these two lines will causing pointer overflow after *1024.
		 * In future, we need to come back review this issue for XVMC.
		 */
		/*
		 * XvMC should not need to copy the surface, since it is already
		 * in video memory.  However since it currently does this is needed.
		 */
		surface_v_src = surface_src + height * 1024;
		surface_u_src = surface_src + height * 1024 +
			(height>>1) * 1024;
#endif

		round_x = 1;
		round_y = 1;

		break;
	case FOURCC_I420:
		OS_DEBUG("Overlay pf: I420");
		surface_bpp = 1;
		planar = 1;
		if (convert_planar) {
			/* Converts to FOURCC_YUY2 */
			new_id = FOURCC_YUY2;
		}

		/* Make sure width is aligned to 4 pixel boundary */
		width = (width + 3) & ~3;

		/* The U and V planes are aligned to 8 pixel boundary */
		surface_uv_pitch = ((width + 7) & ~7) >> 1;

		surface_u_src = surface_src + height * width;
		surface_v_src = surface_u_src + (height >> 1) * surface_uv_pitch;

		round_x = 1;
		round_y = 1;

		break;
	case FOURCC_YUY2:
		OS_DEBUG("Overlay pf: YUY2");
		surface_bpp = 2;
		planar = 0;

		round_x = 1;
		round_y = 0;

		break;
	case FOURCC_UYVY:
		OS_DEBUG("Overlay pf: UYVY");
		surface_bpp = 2;
		planar = 0;

		round_x = 1;
		round_y = 0;

		break;
	default:
		return 0;
	}

	/* Round top left up and bottom right down. */
    tmp_start_x &= ~round_x;
    tmp_start_y &= ~round_y;
    tmp_new_src_w = (tmp_new_src_w + round_x) & ~round_x;
    tmp_new_src_h = (tmp_new_src_h + round_y) & ~round_y;

	emgd_setup_dst_params(scrn, priv, width, height, dst_pitch,
			dst_pitch2, &alloc_size, new_id);

	/*
	 * Allocate new surface from GMM if old surface is insufficient or
	 * absent
	 */
	if (priv->buf && priv->buf->size < alloc_size) {
		/* Free the surface, it's too small */
		drm_intel_bo_unreference(priv->buf);
		priv->buf = NULL;
	}

	if (!priv->buf) {
		/* Allocate a surface */
		priv->buf = drm_intel_bo_alloc(iptr->bufmgr,
				"xv buffer",
				alloc_size,
				4094);
		if (!priv->buf) {
			return 0;
		}
	}

	/* map the destination surface into the GTT */
	if (drm_intel_gem_bo_map_gtt(priv->buf)) {
		return 0;
	}

	surface_dest   = priv->buf->virtual + priv->YBufOffset;
	surface_u_dest = surface_dest + priv->UBufOffset;
	surface_v_dest = surface_dest + priv->VBufOffset;

	/*
	 * Note: Converting the planar to packed YUY2
	 * Since blend as well as second overlay are much more efficient when
	 * in YUV packed instead of YUV planar.
	 */
	if(convert_planar && planar) {
		CARD32 two_ys;
		CARD32 u, v;
		CARD32 pixel;
		int i;

		/* Virtual Surface Data Addresses (Including X,Y offsets) */
        	surface_src += tmp_start_y * tmp_new_src_w  + tmp_start_x;
        	surface_v_src += (tmp_start_y>>1) * (tmp_new_src_w>>1) +
        			 (tmp_start_x>>1);
        	surface_u_src += (tmp_start_y>>1) * (tmp_new_src_w>>1) +
        			 (tmp_start_x>>1);

		/* Copy data from system memory to graphics memory */
        	for (lines=0;
        		lines< tmp_new_src_h;
        		lines+=2) {
			/* Write 2 pixels on two lines per loop */
			for(i=0; i< tmp_new_src_w; i+=2) {
				two_ys = (CARD32) *(CARD16 *)(surface_src + i);
				u = (CARD32) *(CARD8 *)(surface_u_src + (i>>1));
				v = (CARD32) *(CARD8 *)(surface_v_src + (i>>1));

				pixel = ((two_ys & 0xff00)<<8) |
					((two_ys & 0xff)<<0) | (v<<24) | (u<<8);
				*(CARD32 *)(surface_dest + i*2) = pixel;
			}

			surface_dest += *dst_pitch;
			surface_src += width;

			for(i=0; i< tmp_new_src_w; i+=2) {
				two_ys = (CARD32) *(CARD16 *)(surface_src + i);
				u = (CARD32) *(CARD8 *)(surface_u_src + (i>>1));
				v = (CARD32) *(CARD8 *)(surface_v_src + (i>>1));

				pixel = ((two_ys & 0xff00)<<8) |
					((two_ys & 0xff)<<0) | (v<<24) | (u<<8);
				*(CARD32 *)(surface_dest + i*2) = pixel;
			}

			surface_dest += *dst_pitch;
			surface_src += width;
			surface_u_src += surface_uv_pitch;
			surface_v_src += surface_uv_pitch;
		}
	} else {
		/* Virtual Surface Data Addresses (Including X,Y offsets) */
		surface_src += tmp_start_y * tmp_new_src_w * surface_bpp +
			tmp_start_x * surface_bpp;

		/* Copy data from system memory to graphics memory */
		for (lines=0; lines< tmp_new_src_h; lines++) {
			memcpy((void *)surface_dest,
					(void *)surface_src,
					tmp_new_src_w * surface_bpp);
			surface_dest += *dst_pitch;
			surface_src += width * surface_bpp;
		}

		/*
		 * FIXME: This may be broken.  When convert_planar is true
		 * this will never get called.
		 */
		if(planar) {
			surface_v_src += (tmp_start_y>>1) * (tmp_new_src_w>>1) +
				(tmp_start_x>>1);
			for (lines=0;
					lines<tmp_new_src_h;
					lines+=2) {
				memcpy((void *)surface_v_dest,
						(void *)surface_v_src,
						tmp_new_src_w >> 1);
				surface_v_dest += *dst_pitch;
				surface_v_src += width>>1;
			}
			surface_u_src += (start_y>>1) * (new_src_w>>1) +
				(start_x>>1);
			for (lines=0;
					lines<tmp_new_src_h;
					lines+=2) {
				memcpy((void *)surface_u_dest,
						(void *)surface_u_src,
						tmp_new_src_w >> 1);
				surface_u_dest += *dst_pitch;
				surface_u_src += width>>1;
			}
		}
	}

	drm_intel_gem_bo_unmap_gtt(priv->buf);
	OS_TRACE_EXIT;
	return new_id;
}

/*
 * Put the image on the overlay/sprite plane.
 */
int emgd_overlay_put_image(
	ScrnInfoPtr scrn,
	short src_x, short src_y,
	short drw_x, short drw_y,
	short src_w, short src_h,
	short drw_w, short drw_h,
	int id, unsigned char* buf,
	short width, short height,
	Bool sync,
	RegionPtr clipBoxes, pointer data,
	DrawablePtr pDraw)
{
	ScreenPtr screen = pDraw->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	emgd_xv_t *xv_priv = (emgd_xv_t *)data;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	emgd_sprite_t *(planes[2]);
	unsigned int src_render_ops;
	igd_rect_t src_rect;
	igd_rect_t dst_rect;
	int dst_pitch, dst_pitch2;
	uint32_t pitches[4] = { 0 };
    	uint32_t bo_handles[4] = { 0 };
    	uint32_t offsets[4] = { 0 };
    	uint32_t fb_id;
	uint32_t pixel_format = DRM_FORMAT_YUYV;
	struct drm_intel_sprite_pipeblend pipeblend;
	struct _emgd_pixmap *back_pixmap;
	int ret, next, crtcnum, mode;
	short pix_w, pix_h, new_src_x, new_src_y;
	float src_scale_x, src_scale_y;
	int display_rotation = RR_Rotate_0;
	xf86CrtcConfigPtr xf86_config;
#ifdef FOURCC_VAXV
        uint32_t name;
        int i;
    	va_xv_put_image_t *xv_put = (va_xv_put_image_t *) buf;
	dri_bo *bo;
#endif
	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	crtcnum = crtc_for_drawable(scrninfo, pDraw);
	emgd_sprite_check_mode(pDraw, &mode, &next);

	/* If crtcnum is -1.  The drawable is out of screen.  
	 * Turn off the overlay then return.
	 */  
        if (crtcnum == -1) {
		if(xv_priv->ovlplane[0] != NULL) {
			emgd_sprite_disable_core(iptr, xv_priv->ovlplane[0]);
			emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[0], NULL, IGD_GRAB_FOR_XV_OFF);
			xv_priv->ovlplane[0] = NULL;
		}

		if(xv_priv->ovlplane[1] != NULL) {
			emgd_sprite_disable_core(iptr, xv_priv->ovlplane[1]);
			emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[1], NULL, IGD_GRAB_FOR_XV_OFF);
			xv_priv->ovlplane[1] = NULL;
		}
 
		OS_TRACE_EXIT;
		return Success;
        }

	display_rotation = emgd_sprite_check_display_rotation_status(pDraw, iptr, crtcnum, mode);
	if((display_rotation == RR_Rotate_90) ||
	   (display_rotation == RR_Rotate_180) ||
	   (display_rotation == RR_Rotate_270)) {

		if(xv_priv->ovlplane[0] == NULL) {
			goto blend_fallback;
		}
            
		emgd_sprite_disable_core(iptr, xv_priv->ovlplane[0]);
		emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[0], NULL, IGD_GRAB_FOR_XV_OFF);
		xv_priv->ovlplane[0] = NULL;

		if(xv_priv->ovlplane[1] != NULL) {
			emgd_sprite_disable_core(iptr, xv_priv->ovlplane[1]);
			emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[1], NULL, IGD_GRAB_FOR_XV_OFF);
			xv_priv->ovlplane[1] = NULL;
		}                                                                                                                                                    
			goto blend_fallback;                                                                                                                                                                  
	}

    src_rect.x1 = src_x;
    src_rect.x2 = src_x + src_w;
    src_rect.y1 = src_y;
    src_rect.y2 = src_y + src_h;

    dst_rect.x1 = drw_x;
    dst_rect.y1 = drw_y;
    dst_rect.x2 = drw_x + drw_w;
    dst_rect.y2 = drw_y + drw_h;

	/* In some cases, the clipbox rect does not calculate the screen
	 * width and height.  Add a check here.
	 */  
	if (REGION_NUM_RECTS(clipBoxes) == 1) {
		xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);
		BoxPtr box = &(clipBoxes->extents);

		if (box->x2 > xf86_config->crtc[crtcnum]->bounds.x2) {
			box->x2 = xf86_config->crtc[crtcnum]->bounds.x2;
		}
		if (box->y2 > xf86_config->crtc[crtcnum]->bounds.y2) {
			box->y2 = xf86_config->crtc[crtcnum]->bounds.y2;
		}
	}
	/* Grab ourselves an overlay plane if we hadnt done so already */
	if (xv_priv->ovlplane[0] == NULL) {
		ret = emgd_sprite_get_plane(iptr, pDraw, SPRITE_PREASSIGNED_XV, planes, NULL, IGD_GRAB_FOR_XV_OFF);
		if (!ret) {

		    if ((id == FOURCC_VAXV) &&
		        (xv_put->size == sizeof(va_xv_put_image_t)) &&
		        (xv_priv->vaxv == 1)) {
                        OS_ERROR("Failed to get a sprite plane");
                        return -1;
		    } else {
			goto blend_fallback;
		    }
		}
		xv_priv->ovlplane[0] = planes[0];
		xv_priv->ovlplane[1] = planes[1];
	}

	/* Record down previous crtc and display mode. */
	xv_priv->prev_mode = mode;
	xv_priv->prev_crtcnum = crtcnum;

#ifdef FOURCC_VAXV

    /* If the size of the buffer same with xvva structure size and it is YUY2 
     * format.
     */ 
    if ((id == FOURCC_VAXV) &&
        (xv_put->size == sizeof(va_xv_put_image_t)) &&
        (xv_priv->vaxv == 1)) {

        src_rect.x1 = src_x;
        src_rect.x2 = src_x + xv_put->video_image.buf_width;
        src_rect.y1 = src_y;
        src_rect.y2 = src_y + xv_put->video_image.buf_height;

        if (xv_put->video_image.pixel_format == FOURCC_YUY2) {
            pixel_format = DRM_FORMAT_YUYV;
        } else {
            OS_ERROR("Not supported pixel format in VAXV : 0x%lx", xv_put->video_image.pixel_format);
            return -1;
        }

        pix_h = drw_h;
        pix_w = (drw_w + 1) & ~1;     // The width have to be even.

        for (i = 0; i < 3; ++i) {
            // Check to see if this buffer are previously imported
            if (xv_priv->shmbo[i]) {
                dri_bo_flink(xv_priv->shmbo[i], &name);
        
                if (xv_put->video_image.buf_handle == name) {
                    next = i;
                    bo = xv_priv->shmbo[i];
                    break;
                }
            }
        }

        // Import this buffer if it not exist previously
        if (i > 2) {
            if (xv_priv->pixmap_current >= 2) {
                next = 0;
            } else {
                next = xv_priv->pixmap_current;
                ++next;
            }

            if (xv_priv->fb_ids[next]) {
                drmModeRmFB(iptr->drm_fd, xv_priv->fb_ids[next]);
                xv_priv->fb_ids[next] = 0;
            }

            if (xv_priv->shmbo[next]) {
                dri_bo_unreference(xv_priv->shmbo[next]);
                xv_priv->shmbo[next] = NULL;
            }

            bo = intel_bo_gem_create_from_name(iptr->bufmgr, "va YUY2 buffer", xv_put->video_image.buf_handle);
            xv_priv->shmbo[next] = bo;
        }
        bo_handles[0] = bo->handle;
        bo_handles[1] = bo_handles[2] = bo_handles[3] = 0;

        pitches[0] = xv_put->video_image.pitches[0];
        pitches[1] = pitches[2] = pitches[3] = 0;
    } else {
#endif

    id = copy_to_emgd_vid_mem(scrn, id, buf, width,
            height, xv_priv, &dst_pitch, &dst_pitch2, src_x, src_y, src_w, src_h);
    if (!id) {
        return -1;
    }

    if(id == FOURCC_YUY2) {
        pixel_format = DRM_FORMAT_YUYV;
        pix_h = drw_h;
        pix_w = (drw_w + 1) & ~1;  /* Width for YUV buffer have to be even in DRM. */
    } else if (id == FOURCC_UYVY) {
        pixel_format = DRM_FORMAT_UYVY;
        pix_h = drw_h;
        pix_w = (drw_w + 1) & ~1;
    } else {
        OS_ERROR("XVideo ovl_put_img calling sprite with unsupported format!\n");
        return -1;
    }

    /* Get next free buffer object */
    if (xv_priv->pixmap_current >= 2) {
        next = 0;
    } else {
        next = xv_priv->pixmap_current;
        ++next;
    }

    /* Create the pixmap buffer object if it does not exist */
    if (xv_priv->pixmaps[next] == NULL) {
        /* Create the pixmap buffer with depth 16, YUY2 width are muck like RGB16 */
        xv_priv->pixmaps[next] = screen->CreatePixmap(screen, pix_w, pix_h,
                                                      16, INTEL_CREATE_PIXMAP_DRI2);
    } else if ((xv_priv->pixmaps[next]->drawable.width != pix_w) ||
               (xv_priv->pixmaps[next]->drawable.height != pix_h)) {

        /* Remove the framebuffer object if it exist */
        if (xv_priv->fb_ids[next]) {
            drmModeRmFB(iptr->drm_fd, xv_priv->fb_ids[next]);
            xv_priv->fb_ids[next] = 0;
        }

        screen->DestroyPixmap(xv_priv->pixmaps[next]);
        xv_priv->pixmaps[next] = screen->CreatePixmap(screen, pix_w, pix_h,
                                                      16, INTEL_CREATE_PIXMAP_DRI2);
    }

    /* Output and scale the YUV buffer into the pixmap buffer.  Should use the
	 * even out width and height, or may see green line at corner in some case.
	 */
    Gen7ConvertVideoTextured(scrn, xv_priv, id,	width, height,
                             dst_pitch, dst_pitch2, src_w, src_h,
			     pix_w, pix_h, xv_priv->pixmaps[next]);

    back_pixmap = intel_get_pixmap_private(xv_priv->pixmaps[next]);
    bo_handles[0] = back_pixmap->bo->handle;
    bo_handles[1] = bo_handles[2] = bo_handles[3] = 0;

    pitches[0] = intel_pixmap_pitch(xv_priv->pixmaps[next]);
    pitches[1] = pitches[2] = pitches[3] = 0;

#ifdef FOURCC_VAXV
    }
#endif

    if (!xv_priv->fb_ids[next]) {
    	if (drmModeAddFB2(iptr->drm_fd, pix_w, pix_h,
            		pixel_format, bo_handles, pitches,
            		offsets, &fb_id, 0)) {
        	OS_ERROR("Failed to add fb2");
        	return -1;
    	}
    	xv_priv->fb_ids[next] = fb_id;
    } else {
	fb_id = xv_priv->fb_ids[next];
    }

	/* Validate and / or correct the src and dest rects
	 * BUT for this we need to know is fb_blend_ovl turned on or not */
	/* Lets request sprite configuration values...*/
	memset(&pipeblend, 0, sizeof(pipeblend));
	pipeblend.plane_id = xv_priv->ovlplane[0]->plane_id;
	pipeblend.crtc_id = xv_priv->ovlplane[0]->crtc_id;
	ret = drmIoctl(iptr->drm_fd, DRM_IOCTL_IGD_GET_PIPEBLEND, &pipeblend);
	if (ret) {
		OS_ERROR("Failed to get the sprite pipe blend info");
		return -1;
	}

	src_scale_x = ((float)src_w / width) / (float)drw_w;
	src_scale_y = ((float)src_h / height) / (float)drw_h;
	new_src_x = (short) src_scale_x * src_x;
	new_src_y = (short) src_scale_y * src_y;
	if (pipeblend.fb_blend_ovl != 1) {
		/* After scaling, use destination width and height for all */
		get_src_dest_rect(scrn,
			new_src_x, new_src_y, drw_x, drw_y,
			drw_w, drw_h, drw_w, drw_h,
			drw_w, drw_h, 0,
			&src_render_ops, &src_rect, &dst_rect, clipBoxes);
	} else {
		src_rect.x1 = new_src_x;
		src_rect.x2 = new_src_x + drw_w;
		src_rect.y1 = new_src_y;
		src_rect.y2 = new_src_y + drw_h;
	}

#if 0
	OS_ERROR("Rects after util get_rects:\n");
	OS_ERROR("  Src -  x1:%d x2:%d \n", src_x, src_x+src_w);
	OS_ERROR("      -  y1:%d y2:%d \n", src_y, src_y+src_h);
	OS_ERROR("  Dst -  x1:%d x2:%d \n", drw_x, drw_x+drw_w);
	OS_ERROR("      -  y1:%d y2:%d \n", drw_y, drw_y+drw_h);
#endif

	/* FIXME: We need to feed the overlay attributes to a new common emgd_sprite function */
#if 0
	/* The escapes may have changed the video quality or gamma, so re-read
	 * the values. */
	xv_priv->igd_ovl_info.color_correct.contrast   =
		iptr_prim->cfg.ovl_contrast;
	xv_priv->igd_ovl_info.color_correct.brightness =
		iptr_prim->cfg.ovl_brightness;
	xv_priv->igd_ovl_info.color_correct.saturation =
		iptr_prim->cfg.ovl_saturation;

	/* FIXME: Shouldn't priv already have the gamma values? */
	xv_priv->igd_ovl_info.gamma.flags = IGD_OVL_GAMMA_ENABLE;
	xv_priv->igd_ovl_info.gamma.red   = iptr_prim->cfg.ovl_gamma_r;
	xv_priv->igd_ovl_info.gamma.green = iptr_prim->cfg.ovl_gamma_g;
	xv_priv->igd_ovl_info.gamma.blue  = iptr_prim->cfg.ovl_gamma_b;
	/* Enable overlay gamma, valid defaults should be set. */
	xv_priv->igd_ovl_info.gamma.flags = IGD_OVL_GAMMA_ENABLE;

	if (iptr->cfg.video_color_correct) {
		/* Apply individual plane color correction value instead */
		xv_priv->igd_ovl_info.gamma.flags |= IGD_OVL_GAMMA_OVERRIDE;
	}
#endif

#if 0
	/* Step 3 - Write color key - soon, this will be done by the common function*/
	/* FIXME: On FC5, X seems to write the color key.  Not sure when this
	 * was started, but at some point, we should be able to remove this.
	 * However, there is a bug in the X drawing of the color key (at least
	 * on FC5) where it will draw 1 pixel less in width and height (right and
	 * bottom are incorrect).  So for now continue to draw the color key. */
	if(!iptr->cfg.fb_blend_ovl && !RegionEqual(&xv_priv->clip, clipBoxes)) {
		REGION_COPY(screen, &xv_priv->clip, clipBoxes);
		xf86XVFillKeyHelperDrawable (pDraw,
				xv_priv->igd_ovl_info.color_key.dest,
				clipBoxes);
	}
#endif

	/*
	 * During mode change operations the overlay should be disabled
	 * so check first.
	 */
	if (!xv_priv->disable) {
		/* Flip this surface onto the overlay plane */
		ret = emgd_sprite_flip_core(
				xv_priv->ovlplane, pDraw, fb_id,
				src_rect, dst_rect,
				pixel_format);
		if(!ret){
			OS_ERROR("XVideo ovl_put_img call to emgd_sprite_flip_core failed!\n");
			/* Looks like something wrong...
			 * Very-very maybe you are moving XV window from one screen to another one!
			 * Let's disable sprite planes (overlays) first.
			 * When whole area of XV app's window will be belong to one display screen
			 * we can try to flip again, it should be successfully */
			if(NULL != xv_priv->ovlplane[0]) {
				emgd_sprite_disable_core(iptr, xv_priv->ovlplane[0]);
				emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[0], NULL, IGD_GRAB_FOR_XV_OFF);
				xv_priv->ovlplane[0] = NULL;
				}
			if (NULL != xv_priv->ovlplane[1]) {
				emgd_sprite_disable_core(iptr, xv_priv->ovlplane[1]);
				emgd_sprite_ungrab(scrn->pScreen, iptr,
					xv_priv->ovlplane[1], NULL, IGD_GRAB_FOR_XV_OFF);
				xv_priv->ovlplane[1] = NULL;
			}
			xv_priv->video_status = 0;
			return Success;
		}

		/* Tag out status as "VIDEO_ON"*/
		xv_priv->video_status |= CLIENT_VIDEO_ON;
	}

	/* If everything go well, then record down the buffer that is currently been use. */
	xv_priv->pixmap_current = next;

	OS_TRACE_EXIT;
	return Success;
  
blend_fallback:
    OS_TRACE_EXIT;
    return emgd_blend_put_image( 
                scrn, 
                src_x, src_y, 
                drw_x, drw_y, 
                src_w, src_h, 
                drw_w, drw_h, 
                id, buf, 
                width, height, 
                sync, 
                clipBoxes, data, 
                pDraw); 
}

static int emgd_blend_put_image(
	ScrnInfoPtr scrn,
	short src_x, short src_y,
	short drw_x, short drw_y,
	short src_w, short src_h,
	short drw_w, short drw_h,
	int id, unsigned char* buf,
	short width, short height,
	Bool sync,
	RegionPtr clipBoxes, pointer data,
	DrawablePtr pDraw)
{
	emgd_xv_t *xv_priv = (emgd_xv_t *)data;
	int dst_pitch;
	int dst_pitch2;
	PixmapPtr dest;
	int start_src_x, start_src_y;
	int new_src_w, new_src_h;

	int top = 0, left = 0, npixels = 0, nlines = 0;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;
	OS_DEBUG("clipBoxes = %lu", (unsigned long)REGION_NUM_RECTS(clipBoxes));

	dest = get_drawable_pixmap(pDraw);
	if (!intel_pixmap_is_offscreen(dest)) {
		OS_DEBUG("Pixmap isn't an emgd_pixmap.");
		return BadAlloc;
	}

	if(clipBoxes == NULL){
		OS_DEBUG("clipBoxes is NULL.");
		return BadAlloc;
	}

	emgd_clip_video_helper(scrn, id,
					src_x, src_y, src_w, src_h,
					drw_x, drw_y, drw_w, drw_h,
					&top, &left, &npixels, &nlines,
					clipBoxes, width, height);

	start_src_x = left;
	start_src_y = top;
	new_src_w = npixels; 
	new_src_h = nlines;

	/* Copy the video from system memory to graphics memory. */
	id = copy_to_emgd_vid_mem(scrn, id, buf, width,
			height, xv_priv, &dst_pitch, &dst_pitch2, start_src_x, 
			start_src_y, new_src_w, new_src_h);
	if (!id) {
		return -1;
	}

	/*
	 *  This call is to the OTC function that handles displaying
	 *  textured video. It should be used mostly as is just like
	 *  the UXA code.
	 *
	 *  TODO: This may need to be dispatched property to handle
	 *  other chipsets.
	*/
	Gen6DisplayVideoTextured(scrn, xv_priv, id, clipBoxes,
			width, height, dst_pitch, dst_pitch2,
			src_w, src_h,
			drw_w, drw_h, dest);


	DamageDamageRegion(pDraw, clipBoxes);

	OS_TRACE_EXIT;
	return Success;
}

static Bool
emgd_clip_video_helper(ScrnInfoPtr scrn, int id,
                        short src_x, short src_y, short src_w, short src_h,
                        short drw_x, short drw_y, short drw_w, short drw_h,
                        int *top, int *left, int *npixels, int *nlines,
                        RegionPtr clipBoxes, INT32 width, INT32 height)
{

        BoxRec dstBox;
        INT32 x1, x2, y1, y2;

        oal_screen = scrn->scrnIndex;
        OS_TRACE_ENTER;

        /* src_x, src_y are xy coordinates in the source image
         * having (src_x + src_w)/2 would chop off the right half
         * of the original source image */
        x1 = src_x;
        x2 = src_x + src_w;
        y1 = src_y;
        y2 = src_y + src_h;

        /* drw_x, drw_y are xy coordinates in entire virtual desktop
         * meaning they are the absolute offset from 0,0 of virtual desktop */
        dstBox.x1 = drw_x;
        dstBox.x2 = drw_x + drw_w;
        dstBox.y1 = drw_y;
        dstBox.y2 = drw_y + drw_h;


        /* Sanity check the destination rectangle to 
         * make sure it has no negative coordinates. */
        if(REGION_NUM_RECTS(clipBoxes) == 1) {
                BoxPtr box = &(clipBoxes->extents);
                if (box->x1 < 0) {
                        OS_DEBUG("  clipBox x1 negative");
                        box->x1 = 0;
                }
                if (box->x2 < 0) {
                        OS_DEBUG("  clipBox x2 negative");

                        box->x2 = dstBox.x2;
                }
                if (box->y1 < 0) {
                        OS_DEBUG("  clipBox y1 negative");
                        box->y1 = 0;
                }
                if (box->y2 < 0) {
                        OS_DEBUG("  clipBox y2 negative");
                        box->y2 = dstBox.y2;
                }

                if (box->x2 > dstBox.x2) {
                        OS_DEBUG("  clipBox x2 needs clip");
                        box->x2 = dstBox.x2;
                }
                if (box->y2 > dstBox.y2) {
                        OS_DEBUG("  clipBox y2 needs clip");
                        box->y2 = dstBox.y2;
                }

                if (box->x1 > box->x2) {
                        OS_DEBUG("  clipBox x1 > x2");
                        box->x1 = box->x2 - 1;
                }
                if (box->y1 > box->y2) {
                        OS_DEBUG("  clipBox y1 > y2");
                        box->y1 = box->y2 - 1;
                }

        }


        /* This clips the source to screen coordinates.  */
        if(!xf86XVClipVideoHelper(&dstBox,&x1,&x2,&y1,&y2,clipBoxes,
                        width,height)) {
                return Success;
        }


        if(REGION_NUM_RECTS(clipBoxes) > 1) {
                int rectloop;

                for(rectloop=0; rectloop < REGION_SIZE(clipBoxes); ++rectloop) {
                        BoxPtr box = REGION_BOX(clipBoxes, rectloop);

                        if(box->x2 > dstBox.x2){
                                OS_DEBUG("clipBox %d x2 needs clip", rectloop);
                                box->x2 = dstBox.x2;
                        }
                        if(box->y2 > dstBox.y2){
                                OS_DEBUG("clipBox %d y2 needs clip", rectloop);
                                box->y2 = dstBox.y2;
                        }
                        if(box->x1 > box->x2){
                                OS_DEBUG("clipBox %d x1 > x2", rectloop);
                                box->x1 = box->x2 - 1;
                        }
                        if(box->y1 > box->y2){
                                OS_DEBUG("clipBox %d y1 > y2", rectloop);
                                box->y1 = box->y2 - 1;
                        }

                }
        }

        *top = y1 >> 16;
        *left = (x1 >> 16) & ~1;
        *npixels = ALIGN(((x2 + 0xffff) >> 16), 2) - *left;
        if(is_planar_fourcc(id)) {
                *top &= ~1;
                *nlines = ALIGN(((y2 + 0xffff) >> 16), 2) - *top;
        }else
                *nlines = ((y2 + 0xffff) >> 16) - *top;

        return 0;
}



/*
 * Return the requested information about the format.
 */
static int emgd_query_image_attriabutes(
	ScrnInfoPtr scrn,
	int id,
	unsigned short *w, unsigned short *h,
	int *pitches, int *offsets)
{
	int size = 0;
	int offset[3];  /* at most 3 planes */
	int pitch[3];

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	memset(offset, 0, sizeof(offset));
	memset(pitch, 0, sizeof(pitch));

	/*
	 * If the requested source image size is bigger than the overlay engine
	 * can handle, reset it
	 */

	if(*w > IMAGE_SUGGESTED_MAX_WIDTH) {
		*w = IMAGE_SUGGESTED_MAX_WIDTH;
	}
	if(*h > IMAGE_SUGGESTED_MAX_HEIGHT) {
		*h = IMAGE_SUGGESTED_MAX_HEIGHT;
	}

	switch(id){
	case FOURCC_YUY2:
		*w = (*w + 1) & ~1;
		pitch[0] = *w * 2;
		size = pitch[0] * *h;
		break;
	case FOURCC_UYVY:
		*w = (*w + 1) & ~1;
		pitch[0] = *w * 2;
		size = pitch[0] * *h;
		break;
	case FOURCC_YV12:
		*w = (*w + 3) & ~3;
		*h = (*h + 1) & ~1;

		/* Planar format would need multiple offsets for Y,V,U */
		pitch[0] = *w;
		offset[0] = 0;
		pitch[1] = ((*w + 7) & ~7) >> 1;
		offset[1] = *w * *h;
		pitch[2] = pitch[1];
		offset[2] = (*w * *h) + pitch[2] * (*h >> 1);
		size = offset[2] + pitch[1] * (*h >> 1);
		break;
	case FOURCC_I420:
		*w = (*w + 3) & ~3;
		*h = (*h + 1) & ~1;

		/* Planar format would need multiple offsets for Y,V,U */
		pitch[0] = *w;
		offset[0] = 0;
		pitch[1] = ((*w + 7) & ~7) >> 1;
		offset[1] = (*w * *h) + pitch[1] * (*h >> 1);
		pitch[2] = pitch[1];
		offset[2] = *w * *h;
		size = offset[1] + pitch[1] * (*h >> 1);
		break;
#ifdef FOURCC_VAXV
	case FOURCC_VAXV:
		*h = (*h + 1) & ~1;
		size = sizeof(va_xv_put_image_t);
		if (pitches)
			pitches[0] = size;
		break;
#endif

	default:
		OS_ERROR("Invalid incoming format: %x.", id);
		break;
	}

	/* If the caller application asks for offsets, tell it the
	 * offset for each component in planar format */
	if (offsets) {
		switch(id){
		case FOURCC_YUY2:
		case FOURCC_UYVY:
			/* Packed format would need only one offset */
			offsets[0] = offset[0];
			break;
		case FOURCC_YV12:
		case FOURCC_I420:
			offsets[0] = offset[0];
			offsets[1] = offset[1];
			offsets[2] = offset[2];
			break;
		default:
			OS_ERROR("Invalid incoming format: %x.", id);
		}
	}

	/*If the caller application ask for the pitches, tell it
	 *for each of the component if it is in planar format*/
	if(pitches){
		switch(id){
		case FOURCC_YUY2:
		case FOURCC_UYVY:
			pitches[0] = pitch[0];
			break;
		case FOURCC_YV12:
		case FOURCC_I420:
			pitches[0] = pitch[0];
			pitches[1] = pitch[1];
			pitches[2] = pitch[2];
			break;
		default:
			OS_ERROR("Invalid incoming format: %x.", id);
			break;
		}
	}

	OS_DEBUG("our size is %d", size);

	OS_TRACE_EXIT;
	return size;
}

/*
 * Stop Video
 */
static void emgd_stop_video(
	ScrnInfoPtr scrn,
	pointer data, Bool cleanup)
{
	emgd_xv_t * xv_priv = (emgd_xv_t *)data;
	emgd_priv_t *iptr = EMGDPTR(scrn);
	int i;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	REGION_EMPTY(scrn->pScreen, &xv_priv->clip);

	if(cleanup){
		if (xv_priv->video_status & CLIENT_VIDEO_ON) {
			if(NULL != xv_priv->ovlplane[0]) {
				emgd_sprite_disable_core(iptr, xv_priv->ovlplane[0]);
				emgd_sprite_ungrab(scrn->pScreen, iptr, xv_priv->ovlplane[0], NULL, IGD_GRAB_FOR_XV_OFF);
				xv_priv->ovlplane[0] = NULL;
			}
			if (NULL != xv_priv->ovlplane[1]) {
				emgd_sprite_disable_core(iptr, xv_priv->ovlplane[1]);
				emgd_sprite_ungrab(scrn->pScreen, iptr,
					xv_priv->ovlplane[1], NULL, IGD_GRAB_FOR_XV_OFF);
				xv_priv->ovlplane[1] = NULL;
			}

            /* Free the scaled pixmap buffers and framebuffers. */
            for (i = 0; i < 3; ++i) {
                    if (xv_priv->fb_ids[i]) {
                            drmModeRmFB(iptr->drm_fd, xv_priv->fb_ids[i]);
                            xv_priv->fb_ids[i] = 0;
                    }

                    if (xv_priv->pixmaps[i]) {
                            scrn->pScreen->DestroyPixmap(xv_priv->pixmaps[i]);
                            xv_priv->pixmaps[i] = NULL;
                    }
            }
		}
		xv_priv->video_status = 0;
	} else {
		if (xv_priv->video_status & CLIENT_VIDEO_ON) {
			xv_priv->video_status |= OFF_TIMER;
			xv_priv->off_time = currentTime.milliseconds + OFF_DELAY;
		}
	}

	OS_TRACE_EXIT;
	return;
}

static void emgd_query_best_size(ScrnInfoPtr scrn, Bool motion,
	short vid_w, short vid_h, short draw_w, short draw_h,
	unsigned int *p_w, unsigned int *p_h, pointer data){

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	if(vid_w > (draw_w << 1))
		draw_w = vid_w >> 1;
	if(vid_h > (draw_h << 1))
		draw_h = vid_h >> 1;

	*p_w = draw_w;
	*p_h = draw_h;

	OS_TRACE_EXIT;
	return;
}

void emgd_destroy_video(ScreenPtr screen){
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrn);
	XF86VideoAdaptorPtr overlay_adapt = iptr->xvOverlayAdaptor;
	XF86VideoAdaptorPtr blend_adapt = iptr->xvBlendAdaptor;
	emgd_xv_t * priv = NULL;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	if ((!overlay_adapt) && (!blend_adapt)) {
		/* No adaptor to free */
		return;
	}

	if(overlay_adapt){
		priv = (emgd_xv_t *) overlay_adapt->pPortPrivates[0].ptr;

		free(iptr->xvOverlayAdaptor);
		iptr->xvOverlayAdaptor = NULL;
	}

	if(blend_adapt){
		priv = (emgd_xv_t *) blend_adapt->pPortPrivates[0].ptr;

		if (priv->buf) {
			drm_intel_bo_unreference(priv->buf);
			priv->buf = NULL;
		}
		free(iptr->xvBlendAdaptor);
		iptr->xvBlendAdaptor = NULL;
	}

	OS_TRACE_EXIT;
	return;
}


/**
 * Intialiazes the hardware for the 3D pipeline use in the 2D driver.
 *
 * Some state caching is performed to avoid redundant state emits.  This
 * function is also responsible for marking the state as clobbered for DRI
 * clients.
 */
void IntelEmitInvarientState(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/*
	 * If we've emitted our state since the last clobber by another client,
	 * skip it.
	 */
	if (intel->last_3d != LAST_3D_OTHER)
		return;
}

