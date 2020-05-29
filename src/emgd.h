/* -*- pse-c -*-
 *-----------------------------------------------------------------------------
 * Filename: emgd.h
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
 */

#ifndef _EMGD_H_
#define _EMGD_H_

/* Xorg headers */
#include <xorg-server.h>
#include <xf86.h>
#include <xf86xv.h>
#include <xf86Crtc.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>
#include <dri2.h>
#include <damage.h>
#include <list.h>

/* EMGD headers */
#include "emgd_io.h"
#include "uxa.h"

/* Xorg list structure changed name in 1.12 */
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,11,99,0,0)
#define LIST list
#define LIST_INIT list_init
#define LIST_IS_EMPTY list_is_empty
#define LIST_ADD list_add
#define LIST_DEL list_del
#define LIST_FIRST_ENTRY list_first_entry
#define LIST_FOR_EACH_ENTRY list_for_each_entry
#define LIST_FOR_EACH_ENTRY_SAFE list_for_each_entry_safe
#else
#define LIST xorg_list
#define LIST_INIT xorg_list_init
#define LIST_IS_EMPTY xorg_list_is_empty
#define LIST_ADD xorg_list_add
#define LIST_DEL xorg_list_del
#define LIST_FIRST_ENTRY xorg_list_first_entry
#define LIST_FOR_EACH_ENTRY xorg_list_for_each_entry
#define LIST_FOR_EACH_ENTRY_SAFE xorg_list_for_each_entry_safe
#endif

#ifndef XF86_HAS_SCRN_CONV
#define xf86ScreenToScrn(s) xf86Screens[(s)->myNum]
#endif

#define SCREEN_ARG_TYPE ScreenPtr
#define SCREEN_PTR(arg1) ScreenPtr screen = (arg1)

#define BLOCKHANDLER_ARGS_DECL ScreenPtr arg, pointer timeout, pointer read_mask
#define BLOCKHANDLER_ARGS arg, timeout, read_mask

/*
 * ScrnInfo Global array goes away in xserver 1.13.  Several functions
 * also lost "flags" parameters.
 */
#ifdef XF86_SCRN_INTERFACE
	/* Functions take ScrnInfoPtr directly instead of integer index now */
	#define SCRN_ARG_TYPE ScrnInfoPtr
	/* Declaration of 'scrn' ScreenInfoPtr for top of functions */
	#define DECLARE_SCREENINFOPTR(arg) ScrnInfoPtr scrn = (arg)

	/* ScreenInit */
	#define SCREEN_INIT_ARGS_DECL ScreenPtr screen, int argc, char **argv

	/* CloseScreen */
	#define CLOSE_SCREEN_ARGS_DECL ScreenPtr screen
	#define CLOSE_SCREEN_ARGS      scrn->pScreen

	/* EnterVT/LeaveVT */
	#define VT_FUNC_ARGS_DECL   ScrnInfoPtr arg
	#define VT_FUNC_ARGS(flags) scrn

	/* FreeScreen */
	#define FREE_SCREEN_ARGS_DECL ScrnInfoPtr arg

	/* AdjustFrame */
	#define ADJUST_FRAME_ARGS_DECL ScrnInfoPtr arg, int x, int y

	/* xf86DeleteScreen */
	#define XF86_DELETE_SCREEN_ARGS(flags) scrn
#else
	/* Integer indices into global array were passed around on older servers */
	#define SCRN_ARG_TYPE int
	/* Declaration of 'scrn' ScreenInfoPtr for top of functions */
	#define DECLARE_SCREENINFOPTR(arg) ScrnInfoPtr scrn = xf86Screens[(arg)]

	/* ScreenInit */
	#define SCREEN_INIT_ARGS_DECL int scrn_index, ScreenPtr screen, \
		int argc, char **argv

	/* CloseScreen */
	#define CLOSE_SCREEN_ARGS_DECL int arg, ScreenPtr screen
	#define CLOSE_SCREEN_ARGS      scrn->scrnIndex, scrn->pScreen

	/* EnterVT/LeaveVT function args */
	#define VT_FUNC_ARGS_DECL   int arg, int flags
	#define VT_FUNC_ARGS(flags) scrn->scrnIndex, (flags)

	/* FreeScreen */
	#define FREE_SCREEN_ARGS_DECL   int arg, int flags

	/* AdjustFrame */
	#define ADJUST_FRAME_ARGS_DECL int arg, int x, int y, int flags

	/* xf86DeleteScreen */
	#define XF86_DELETE_SCREEN_ARGS(flags) scrn->scrnIndex, flags
#endif

/* Per-module debugging flags */
#define CONFIG_DEBUG_IAL_FLAGS	  \
	0, /* Core Driver */		  \
    0, /* DRM buffer management */ \
	0, /* Escape Module */		  \
	0, /* Accel Module */		  \
	0, /* DRI Module */			  \
	0, /* Native kernel API */    \
	0, /* Options Module */		  \
	0, /* Overlay Module */		  \
	0, /* Video Module */		  \
	0, /* XRandR/KMS CRTC */      \
	0, /* XRandR/KMS Output */    \
	\
	0,	/* Global Tracing */	  \
	0	/* Global Debug */		  \


#define DEFAULT_CONFIG_ID                      0

/* Globals */
#ifndef MODULEVENDORSTRING
#define MODULEVENDORSTRING "Intel(R) Corporation"
#endif

#define EMGD_NAME "EMGD"
#define EMGD_DRIVER_NAME "emgd"
#define DRI_DRIVER_NAME "i965" /* Which Mesa DRI driver do we use? */
#define DRM_DRIVER_NAME "emgd" /* Kernel DRM driver to modprobe if necessary */
#define EMGD_VERSION 1100
#define EMGD_DRIVER_TITLE "Intel Embedded Media and Graphics Driver"
#define EMGD_TOTAL_SURFACES 9
#define EMGD_MAX_DISPLAYS 4
#define EMGD_MAX_PIXMAP_REGION 2
#define EMGD_MAJOR_VERSION IGD_MAJOR_NUM
#define EMGD_MINOR_VERSION IGD_MINOR_NUM
#define EMGD_PATCHLEVEL IGD_BUILD_NUM
#ifdef IGD_MAX_PIPE_DISPLAYS
#define EMGD_MAX_DEVICES IGD_MAX_PIPE_DISPLAYS
#else
#define EMGD_MAX_DEVICES 4
#endif

/*
 * Chipsets PCI device ID.
 */
#define PCI_CHIP_PSB1         0x8108
#define PCI_CHIP_PSB2         0x8109
#define PCI_CHIP_TNC          0x4108
#define PCI_CHIP_SNB          0x0102
#define PCI_CHIP_IVB          0x0152
//#ifdef CONFIG_VLVONCDV
/* CDV device ID */
//#define PCI_CHIP_VLV1         0x0BE2
//#else
#define PCI_CHIP_VLV1         0x0f30
//#endif
#define PCI_CHIP_VLV2         0x0f31
#define PCI_CHIP_VLV3         0x0f32
#define PCI_CHIP_VLV4         0x0f33
#define PCI_CHIP_CDV0         0x0BE0
#define PCI_CHIP_CDV1         0x0BE1
#define PCI_CHIP_CDV2         0x0BE2
#define PCI_CHIP_CDV3         0x0BE3


#define INTEL_INFO(intel) ((intel)->chipset.info)
#define IS_GENx(intel, X) \
	(INTEL_INFO(intel)->gen >= 10*(X) && INTEL_INFO(intel)->gen < 10*((X)+1))
#define IS_GEN4(intel) IS_GENx(intel, 4)
#define IS_GEN5(intel) IS_GENx(intel, 5)
#define IS_GEN6(intel) IS_GENx(intel, 6)
#define IS_GEN7(intel) IS_GENx(intel, 7)

#define IS_VALLEYVIEW(iptr_intel) ( (iptr_intel->PciInfo->device_id==PCI_CHIP_VLV1) || \
									(iptr_intel->PciInfo->device_id==PCI_CHIP_VLV2) || \
									(iptr_intel->PciInfo->device_id==PCI_CHIP_VLV3) || \
									(iptr_intel->PciInfo->device_id==PCI_CHIP_VLV4) )

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * KB(1024))

#define GTT_PAGE_SIZE			KB(4)
#define HWCURSOR_SIZE			GTT_PAGE_SIZE
#define HWCURSOR_SIZE_ARGB		GTT_PAGE_SIZE * 4

#define EMGD_BATCH_SIZE         KB(16)

/*
 * Definitions required for render-scaling or centering
 * when either one intends to add modes to the IAL modelist.
 */

#define MAX_STANDARD_MODES                 4
#define STD_MODE_WIDTH_IDX                 0
#define STD_MODE_HEIGHT_IDX                1

#if HAS_DEVPRIVATEKEYREC
#define DEV_PRIVATE_KEY_TYPE DevPrivateKeyRec
#else
#define DEV_PRIVATE_KEY_TYPE int
#endif

extern DEV_PRIVATE_KEY_TYPE uxa_pixmap_index;

#if HAS_DIXREGISTERPRIVATEKEY
#define DIX_REGISTER_PRIVATE(key, type, size) \
	dixRegisterPrivateKey(key, type, size)
#else
#define DIX_REGISTER_PRIVATE(key, size) \
	dixRequestPrivate(key, size)
#endif

#ifdef XvMCExtension
#ifdef ENABLE_XVMC
#define INTEL_XVMC 1
#endif
#endif

#define PITCH_NONE 0

/*----------------------------------------------------------------------------
 * Overlay Query Flags
 *--------------------------------------------------------------------------*/
/*!
 * @name Query Overlay Flags
 * @anchor query_ovl_flags
 *
 * Flags for use with _igd_dispatch::query_ovl()
 * These flags ARE EXCLUSIVE - ONLY ONE AT A TIME can be used during a
 *    single call to igd_query_ovl
 *
 * - IGD_OVL_QUERY_IS_HW_SUPPORTED: Can the hardware support an overlay for
 *   this display_h?  This will return the same value no matter if the overlay
 *   is on or off, so the IAL must use some other method to determine if the
 *   overlay is in use.
 * - IGD_OVL_QUERY_IS_LAST_FLIP_DONE: Has the last flip completed?
 * - IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE: Wait until the last flip is complete.
 *   Returns TRUE if the last flip was successfully completed.  Returns FALSE
 *   if there was a HW issue where the last flip did not occur.
 * - IGD_OVL_QUERY_IS_ON: Is the hardware overlay currently on?  This does not
 *   include the secondary overlay, only the hardware overlay.
 * - IGD_OVL_QUERY_IS_GAMMA_SUPPORTED: Is the hardware supports GAMMA
 *   correction?
 * - IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED: Is the hardware supports video
 *   parameters (brightness, contrast, saturation) correction?
 * - IGD_OVL_QUERY_NUM_PLANES: Returns the number of hardware planes. Doesnt
 *   need a display (crtc) handle when calling with this flag
 * @{
 */
#define IGD_OVL_QUERY_IS_HW_SUPPORTED                     0x00000001
#define IGD_OVL_QUERY_IS_LAST_FLIP_DONE                   0x00000002
#define IGD_OVL_QUERY_WAIT_LAST_FLIP_DONE                 0x00000003
#define IGD_OVL_QUERY_IS_ON                               0x00000004
#define IGD_OVL_QUERY_IS_GAMMA_SUPPORTED                  0x00000005
#define IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED            0x00000006
#define IGD_OVL_QUERY_NUM_PLANES                          0x00000007
#define IGD_OVL_QUERY_MASK                                0x0000000F
	/* Ensure no bits conflict with IGD_OVL_FORCE_USE_DISP */
/*! @} */

#define IGD_GRAB_FOR_XV_ON 0x1
#define IGD_GRAB_FOR_XV_OFF 0x0

/** enumeration of 3d consumers so some can maintain invariant state. */
enum last_3d {
	LAST_3D_OTHER,
	LAST_3D_VIDEO,
	LAST_3D_RENDER,
	LAST_3D_ROTATION
};

struct intel_chipset {
	const char *name;
	int variant;
	const struct intel_device_info {
		int gen;
	} *info;
};

/*
 * Store the information read from the Xorg.conf config file.
 * This strucutre should represent the requested configuration state,
 * the actual state may be different.  These values should not be
 * changed by the driver.
 */
typedef struct _config_info {
	Bool shadow_fb;
	Bool tear_fb;

	/* Framebuffer orientation options. */
	/* NOTE:
	 *  Rotate and flip are controlled through standard X monitor config
	 *  options.  Create a "Monitor" section and add the option:
	 *      Option "Rotate" "right" | "left" | "normal" | "inverted"
	 *
	 *  then add an option to the EMGD "Device" section:
	 *      Option "Monitor-<output_name>"  "monitor section name"
	 *
	 */

	/* Hardware accelerated options. */
	Bool accel_2d;
	Bool hw_cursor;
	Bool punt_uxa_offscreen_pixmaps;
	Bool punt_uxa_solid;
	Bool punt_uxa_copy;
	Bool punt_uxa_composite;
	Bool punt_uxa_composite_1x1_mask;
	Bool punt_uxa_composite_mask;

	/* XVideo options. */
	/* FIXME: these overlay specific attrs should go away.
	 * Eventually the emgd_sprite_flip DDX function will become
	 * the common function that will service all following 4:
	 *
	 *    1. XVideo
	 *    2. DRI2SwapBuffer for OGL
	 *    3. DRI2SwapBuffer (or new OTC escape?) for VA driver
	 *    4. SplashVideo or DMA2Overlay
	 *
	 * When this happens, that function will maintain a link list
	 * of all the planes available on this system and will maintain
	 * which XDrawables and "current" gem buffer objects are currently
	 * being displayed by that overlay. It will also maintain
	 * the overlay attributes like below for each plane (seperately).
	 *
	 * At that point such a  function would probably have a companion
	 * function like emgd_sprite_set_attribute which will take in
	 * below parameters and update it into that link list.
	 */

	Bool xv_overlay;
	Bool xv_blend;
	Bool xv_mc;
	unsigned int video_key;
	unsigned long videokey_autopaint;
	int ovl_gamma_r;
	int ovl_gamma_g;
	int ovl_gamma_b;
	int ovl_brightness;
	int ovl_contrast;
	int ovl_saturation;

	/* Embedded features */
	Bool qb_splash;
	Bool render_offscreen;
	unsigned long dih_clone_enable;
	Bool fb_blend_ovl;
	int sprite_assignment[4];
	char *sprite_zorder;
} emgd_config_info_t;


typedef struct emgd_priv_t {
	ScrnInfoPtr scrn;
	int cpp;

#define RENDER_BATCH			I915_EXEC_RENDER
#define BLT_BATCH			I915_EXEC_BLT
	unsigned int current_batch;

	void *modes;
	drm_intel_bo *front_buffer;
	long front_tiling;

	/* Not currently used by EMGD, but checked by OTC UXA code */
	DamagePtr shadow_damage;

	dri_bufmgr *bufmgr;

	uint32_t batch_ptr[4096];
	/** Byte offset in batch_ptr for the next dword to be emitted. */
	unsigned int batch_used;
	/** Position in batch_ptr at the start of the current BEGIN_BATCH */
	unsigned int batch_emit_start;
	/** Number of bytes to be emitted in the current BEGIN_BATCH. */
	uint32_t batch_emitting;
	dri_bo *batch_bo;
	/** Whether we're in a section of code that can't tolerate flushing */
	Bool in_batch_atomic;
	/** Ending batch_used that was verified by intel_start_batch_atomic() */
	int batch_atomic_limit;
	struct LIST batch_pixmaps;
	struct LIST flush_pixmaps;
	struct LIST in_flight;
	drm_intel_bo *wa_scratch_bo;
	OsTimerPtr cache_expire;

	/* For Xvideo */
	Bool use_overlay;
#ifdef INTEL_XVMC
	/* For XvMC */
	Bool XvMCEnabled;
#endif

	CreateScreenResourcesProcPtr CreateScreenResources;

	Bool shadow_present;

	unsigned int tiling;
#define INTEL_TILING_FB		0x1
#define INTEL_TILING_2D		0x2
#define INTEL_TILING_3D		0x4
#define INTEL_TILING_ALL (~0)

	Bool swapbuffers_wait;
	Bool has_relaxed_fencing;

	int Chipset;
	EntityInfoPtr pEnt;
	struct pci_device *PciInfo;
	struct intel_chipset chipset;

	unsigned int BR[20];

	CloseScreenProcPtr CloseScreen;

	void (*context_switch) (struct emgd_priv_t *intel,
				int new_mode);
	void (*vertex_flush) (struct emgd_priv_t *intel);
	void (*batch_flush) (struct emgd_priv_t *intel);
	void (*batch_commit_notify) (struct emgd_priv_t *intel);

	uxa_driver_t *uxa_driver;
	Bool need_sync;
	int accel_pixmap_offset_alignment;
	int accel_max_x;
	int accel_max_y;
	int max_bo_size;
	int max_gtt_map_size;
	int max_tiling_size;

	struct LIST sprite_planes; /* list of emgd_sprite_t */
	Bool private_multiplane_drm;

	Bool XvDisabled;	/* Xv disabled in PreInit. */
	Bool XvEnabled;		/* Xv enabled for this generation. */
	Bool XvPreferOverlay;

	int colorKey;
	XF86VideoAdaptorPtr xvOverlayAdaptor;
	XF86VideoAdaptorPtr xvBlendAdaptor;
	ScreenBlockHandlerProcPtr BlockHandler;
	Bool overlayOn;

	struct {
		drm_intel_bo *gen4_vs_bo;
		drm_intel_bo *gen4_sf_bo;
		drm_intel_bo *gen4_wm_packed_bo;
		drm_intel_bo *gen4_wm_planar_bo;
		drm_intel_bo *gen4_cc_bo;
		drm_intel_bo *gen4_cc_vp_bo;
		drm_intel_bo *gen4_sampler_bo;
		drm_intel_bo *gen4_sip_kernel_bo;
		drm_intel_bo *wm_prog_packed_bo;
		drm_intel_bo *wm_prog_planar_bo;
		drm_intel_bo *gen6_blend_bo;
		drm_intel_bo *gen6_depth_stencil_bo;
	} video;

	/* Video object for YUV output */
	struct {
		drm_intel_bo *gen4_cc_bo;
		drm_intel_bo *gen4_cc_vp_bo;
		drm_intel_bo *gen4_sampler_bo;
		drm_intel_bo *wm_prog_packed_bo;
		drm_intel_bo *wm_prog_planar_bo;
		drm_intel_bo *gen6_blend_bo;
		drm_intel_bo *gen6_depth_stencil_bo;
	} video_yuv;

	/* Render accel state */
	float scale_units[2][2];
	/** Transform pointers for src/mask, or NULL if identity */
	PictTransform *transform[2];

	PixmapPtr render_source, render_mask, render_dest;
	PicturePtr render_source_picture, render_mask_picture, render_dest_picture;
	CARD32 render_source_solid;
	CARD32 render_mask_solid;
	PixmapPtr render_current_dest;
	Bool render_source_is_solid;
	Bool render_mask_is_solid;
	Bool needs_3d_invariant;
	Bool needs_render_state_emit;
	Bool needs_render_vertex_emit;
	Bool needs_render_ca_pass;

	/* i830 render accel state */
	uint32_t render_dest_format;
	uint32_t cblend, ablend, s8_blendctl;

	/* i915 render accel state */
	PixmapPtr texture[2];
	uint32_t mapstate[6];
	uint32_t samplerstate[6];

	struct {
		int op;
		uint32_t dst_format;
	} i915_render_state;

	struct {
		int num_sf_outputs;
		int drawrect;
		uint32_t blend;
		dri_bo *samplers;
		dri_bo *kernel;
	} gen6_render_state;

	uint32_t prim_offset;
	void (*prim_emit)(struct emgd_priv_t *intel,
			  int srcX, int srcY,
			  int maskX, int maskY,
			  int dstX, int dstY,
			  int w, int h);
	int floats_per_vertex;
	int last_floats_per_vertex;
	uint16_t vertex_offset;
	uint16_t vertex_count;
	uint16_t vertex_index;
	uint16_t vertex_used;
	uint32_t vertex_id;
	float vertex_ptr[4*1024];
	dri_bo *vertex_bo;

	uint8_t surface_data[16*1024];
	uint16_t surface_used;
	uint16_t surface_table;
	uint32_t surface_reloc;
	dri_bo *surface_bo;

	/* 965 render acceleration state */
	struct gen4_render_state *gen4_render_state;

	Bool use_pageflipping;
	Bool use_triple_buffer;
	Bool force_fallback;
	Bool can_blt;
	Bool has_kernel_flush;
	Bool needs_flush;
	Bool use_shadow;

	struct _DRI2FrameEvent *pending_flip[2];

	enum last_3d last_3d;

	/**
	 * User option to print acceleration fallback info to the server log.
	 */
	Bool fallback_debug;
	unsigned debug_flush;
#if HAVE_UDEV
	struct udev_monitor *uevent_monitor;
	InputHandlerProc uevent_handler;
#endif

	/* EMGD Specific */
	union {
		unsigned long fb_pitch;     /* EMGD code */
		unsigned long front_pitch;  /* OTC UXA */
	};

	Bool primary_display;      /* This screen is the primary */
	Bool dri2_inuse;           /* DRI2 loaded and in use */
	int drm_fd;                /* DRM file handle */
	char *dev_dri_name;        /* DRM device node (e.g., /dev/dri/card0) */

	/* Wrap CreateScreenResources function with our own */
	CreateScreenResourcesProcPtr create_screen_resources;

	/* pointer to entity private data */
	struct _emgd_entity *entity;

	/* Broken-out options. */
	emgd_config_info_t cfg;

	/* Driver phase/state information */
	Bool suspended;

	/* KMS context */
	void *kms;

	/* End of EMGD Specific */

} emgd_priv_t;

/*
 * Screen private lookup.  EMGD code uses EMGDPTR, but
 * intel_get_screen_private is defined for compatibility with OTC UXA
 * code.
 */
#define EMGDPTR(p)                   ((emgd_priv_t *)((p)->driverPrivate))

/*
 * Entity is used to link the driver instances when the hardware is being
 * used by multiple screens.
 */
typedef struct _emgd_entity {
	int lastInstance;
	int num_devices;
	int primary_screen;
	char *chipname;
	char *chipset;
	unsigned long pipes;
	unsigned long ports;
	emgd_priv_t *primary;   /* pointer to screen 1 intel private */
	emgd_priv_t *secondary; /* pointer to screen 2 intel private */

	/* Shared Memory Limits */
	unsigned int max_mem;
	unsigned int max_video_mem;
	unsigned int video_mem;
	unsigned int total_mem;
} emgd_entity_t;


/* Xserver 1.13 adds its own copy of this macro */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define ALIGN(i,m)	(((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

/**
 * Hints to CreatePixmap to tell the driver how the pixmap is going to be
 * used.
 *
 * Compare to CREATE_PIXMAP_USAGE_* in the server.
 */
enum {
	INTEL_CREATE_PIXMAP_TILING_X	= 0x10000000,
	INTEL_CREATE_PIXMAP_TILING_Y	= 0x20000000,
	INTEL_CREATE_PIXMAP_TILING_NONE	= 0x40000000,
	INTEL_CREATE_PIXMAP_DRI2	= 0x80000000,
};


#endif /* _EMGD_H_ */
