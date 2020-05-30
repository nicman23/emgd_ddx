/*
 * -----------------------------------------------------------------------------
 *  Filename: emgd_ddx.c
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
 *   Main driver file for the EMGD DDX X server driver.
 *-----------------------------------------------------------------------------
 */

#define PER_MODULE_DEBUG
#define MODULE_NAME ial.driver

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <xorg-server.h>
#include <fcntl.h>
#include <errno.h>
#include <xf86drm.h>
#include <micmap.h>
#include <xf86cmap.h>
#include <xf86.h>
#include <sys/stat.h>

#include "emgd.h"
#include "emgd_dri2.h"
#include "emgd_crtc.h"
#include "emgd_drm_bo.h"
#include "emgd_uxa.h"
#include "emgd_sprite.h"
#include "intel_batchbuffer.h"
#include "ddx_version.h"

/*
 * Define the debug flags used by the debug logging facility
 */
static igd_drv_debug_t debug_flag = {
	{
		CONFIG_DEBUG_IAL_FLAGS
	}
};
igd_drv_debug_t *igd_debug = &debug_flag;
unsigned long iegd_dropped_debug_messages = 0;
unsigned long *dropped_debug_messages = &iegd_dropped_debug_messages;
pthread_mutex_t iegd_debug_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t *debug_log_mutex = &iegd_debug_log_mutex;


#ifndef SUSPEND_SLEEP
#define SUSPEND_SLEEP 0
#endif
#ifndef RESUME_SLEEP
#define RESUME_SLEEP 0
#endif


/*
 * Function prototypes
 */
extern void emgd_parse_options(ScrnInfoPtr scrn, const char *chipset);
extern const OptionInfoRec *emgd_available_options(int chipid, int busid);
extern Bool emgd_dri2_screen_init(ScreenPtr screen);
extern void emgd_extension_init(ScrnInfoPtr scrn);
extern Bool intel_uxa_create_screen_resources(ScreenPtr screen);
extern Bool emgd_init_video(ScreenPtr screen);
void i965_free_video(ScrnInfoPtr scrn);
extern Bool emgd_has_multiplane_drm(emgd_priv_t *iptr);

/*
 * Define the chipset info that this driver supports.
 */
#define EMGD_SNB  1
#define EMGD_IVB  2
#define EMGD_VLV1 3
#define EMGD_VLV2 4
#define EMGD_VLV3 5
#define EMGD_VLV4 6
#define EMGD_CDV0 7
#define EMGD_CDV1 8
#define EMGD_CDV2 9
#define EMGD_CDV3 10

static SymTabRec emgd_ChipsetFamilies[] = {
	{EMGD_SNB , "Intel SandyBridge"},
	{EMGD_IVB , "Intel IvyBridge"},
	{EMGD_VLV1, "Intel ValleyView Class"},
	{EMGD_VLV2, "Intel ValleyView 2 Class"},
	{EMGD_VLV3, "Intel ValleyView 3 Class"},
	{EMGD_VLV4, "Intel ValleyView 4 Class"},
	{-1, NULL}
};

SymTabRec emgd_Chipsets[] = {
	{EMGD_SNB , "Intel SandyBridge"},
	{EMGD_IVB , "Intel IvyBridge"},
	{EMGD_VLV1, "Intel ValleyView Classic"},
	{EMGD_VLV2, "Intel ValleyView"},
	{-1, NULL}
};

static struct intel_device_info *chipset_info;

static const struct intel_device_info emgd_sandybridge_info = {
	.gen = 60,
};

static const struct intel_device_info emgd_ivybridge_info = {
	.gen = 70,
};

static const struct intel_device_info emgd_valleyview_info = {
	.gen = 70,
};

PciChipsets emgd_PCIChipsets[] = {
	{EMGD_SNB , PCI_CHIP_SNB,  RES_SHARED_VGA},
	{EMGD_IVB , PCI_CHIP_IVB,  RES_SHARED_VGA},
	{EMGD_VLV1, PCI_CHIP_VLV1, RES_SHARED_VGA},
	{EMGD_VLV2, PCI_CHIP_VLV2, RES_SHARED_VGA},
	{EMGD_VLV3, PCI_CHIP_VLV3, RES_SHARED_VGA},
	{EMGD_VLV4, PCI_CHIP_VLV4, RES_SHARED_VGA},
	{EMGD_CDV0, PCI_CHIP_CDV0, RES_SHARED_VGA},
	{EMGD_CDV1, PCI_CHIP_CDV1, RES_SHARED_VGA},
	{EMGD_CDV2, PCI_CHIP_CDV2, RES_SHARED_VGA},
	{EMGD_CDV3, PCI_CHIP_CDV3, RES_SHARED_VGA},
	{-1, -1, RES_UNDEFINED},
};

#define INTEL_DEVICE_MATCH(d,i) \
	{ 0x8086, (d), PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, (intptr_t)(i) }

static struct pci_id_match emgd_device_match[] = {
	INTEL_DEVICE_MATCH(PCI_CHIP_SNB, &emgd_sandybridge_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_IVB, &emgd_ivybridge_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_VLV1, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_VLV2, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_VLV3, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_VLV4, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_CDV0, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_CDV1, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_CDV2, &emgd_valleyview_info ),
	INTEL_DEVICE_MATCH(PCI_CHIP_CDV3, &emgd_valleyview_info ),

	/* {0, 0, PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, 0}, */
	{0, 0, 0, 0, 0, 0, 0}
};

static int emgd_entity_index = -1;

/*
 * Support functions
 */
static int emgd_get_module_parameter(char *param)
{
	char path[50];
	char buf[50];
	int fd;

	if (strlen(param) > 20) {
		OS_ERROR("Unable to process %s, string length must be less than 20.",
				param);
		return -1;
	}

	sprintf(path, "/sys/module/emgd/parameters/%s", param);
	fd = open(path, O_RDONLY);
	if (fd > 0) {
		ssize_t bytes_read;
		bytes_read = read (fd, buf, 50);
		if(!bytes_read){
			OS_ERROR("read failed !");
		}
		close (fd);
		return (atoi(buf));
	} else {
		OS_ERROR("Failed to read parameter %s (%d)", param, fd);
		return -1;
	}
}


/*
 * Entry points called by the X server
 *
 *   emgd_pre_init
 *   emgd_screen_init;
 *   emgd_adjust_frame;
 *   emgd_enter_vt;
 *   emgd_leave_vt;
 *   emgd_free_screen;
 *   emgd_valid_mode;
 */

/*
 * emgd_enter_vt
 *
 * Called when the X server is becoming active
 */
static Bool emgd_enter_vt(VT_FUNC_ARGS_DECL)
{
	DECLARE_SCREENINFOPTR(arg);
	emgd_priv_t *iptr;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	iptr = EMGDPTR(scrn);

	if (iptr->primary_display) {
		if (drmSetMaster(iptr->drm_fd)) {
			OS_ERROR("drmSetMaster failed: %s", strerror(errno));
			return FALSE;
		}
	}

	if (!xf86SetDesiredModes(scrn)) {
		return FALSE;
	}

	OS_TRACE_EXIT;
	return TRUE;
}


/*
 * emgd_leave_vt
 *
 * Called with the X server is giving up control of the graphics device
 */
static void emgd_leave_vt(VT_FUNC_ARGS_DECL)
{
	DECLARE_SCREENINFOPTR(arg);
	emgd_priv_t *iptr = EMGDPTR(scrn);

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;
	
	/* turn off the sprite and ungrab it */
	emgd_sprite_leave_vt(iptr);

	/* Unrotate if necessary and turn off cursors */
	xf86RotateFreeShadow(scrn);
	xf86_hide_cursors(scrn);

	/* Drop DRM master */
	drmDropMaster(iptr->drm_fd);

	OS_TRACE_EXIT;
	return;
}


/*
 * emgd_free_screen
 *
 * Called when the screen is being destroyed.  This should only happen during
 * X shutdown or error conditions.  See emgd_close_screen for regular cleanup
 * operations that happen at the end of each server generation.
 */
static void emgd_free_screen(FREE_SCREEN_ARGS_DECL)
{
	DECLARE_SCREENINFOPTR(arg);
	emgd_priv_t *iptr;

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	iptr = EMGDPTR(scrn);
	if (iptr != NULL) {
		drmmode_shutdown(iptr);
		emgd_buffer_manager_destroy(iptr);
		drmClose(iptr->drm_fd);
		free(iptr);
		scrn->driverPrivate = NULL;
	}
}


/*
 * emgd_valid_mode
 *
 */
static ModeStatus emgd_valid_mode(
		SCRN_ARG_TYPE arg,
		DisplayModePtr mode,
		Bool verbose,
		int flags)
{
	DECLARE_SCREENINFOPTR(arg);

	if (mode->Flags & V_INTERLACE) {
		if (verbose) {
			xf86DrvMsg(scrn->scrnIndex, X_PROBED,
					"Removing interlaced mode \"%s\"\n",
					mode->name);
		}
		return MODE_BAD;
	}

	return MODE_OK;
}


/*
 * emgd_adjust_frame
 *
 *  Move the display around in the framebuffer. This will position the
 *  viewport somewhere in the virtual coordiate space.
 */
static void emgd_adjust_frame(ADJUST_FRAME_ARGS_DECL)
{
	DECLARE_SCREENINFOPTR(arg);

	oal_screen = scrn->scrnIndex;
	OS_TRACE_ENTER;

	/*
	 * This deprecated and replaced by the xrandr code.
	 */

	OS_TRACE_EXIT;
	return;
}


/*
 * emgd_load_palette
 *
 * This loads the palette registers with the values specified. It is used
 * for 8 and 16 bit depth displays.
 */
static void emgd_load_palette(
		ScrnInfoPtr scrn,
		int num_colors,
		int *indices,
		LOCO *colors,
		VisualPtr visual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr crtc;
	uint16_t lut_r[256], lut_g[256], lut_b[256];
	int index;
	int c;
	int i;
	int j;

	OS_TRACE_ENTER;

	/* Set palette for all crtcs in use by this screen */
	for (c = 0; c < xf86_config->num_crtc; c++) {
		crtc = xf86_config->crtc[c];

		switch (scrn->depth) {
		case 15:
			for (i = 0; i < num_colors; i++) {
				index = indices[i];
				if (index <= 31) {
					for (j = 0; j < 8; j++) {
						lut_r[index * 8 + j] = colors[index].red << 8;
						lut_g[index * 8 + j] = colors[index].green << 8;
						lut_b[index * 8 + j] = colors[index].blue << 8;
					}
				}
			}
			break;
		case 16:
			for (i = 0; i < num_colors; i++) {
				index = indices[i];
				if (index <= 31) {
					for (j = 0; j < 8; j++) {
						lut_r[index * 8 + j] = colors[index].red << 8;
						lut_b[index * 8 + j] = colors[index].blue << 8;
					}
				}
				for (j = 0; j < 4; j++) {
					lut_g[index * 4 + j] = colors[index].green << 8;
				}
			}
			break;
		default:
			for (i = 0; i < num_colors; i++) {
				index = indices[i];
				if (index <= 255) {
					lut_r[index] = colors[index].red << 8;
					lut_g[index] = colors[index].green << 8;
					lut_b[index] = colors[index].blue << 8;
				}
			}
			break;
		}

#ifdef RANDR_12_INTERFACE
		RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
#else
		crtc->funcs->gamma_set(crtc, lut_r, lut_g, lut_b, 256);
#endif
	}
	OS_TRACE_EXIT;
}



/*
 * Handle power management events.
 *
 * The undo flag tells us to reverse the state.  How is is this different
 * from a resume?
 */
static Bool emgd_power_event(SCRN_ARG_TYPE arg, pmEvent event, Bool undo)
{
	DECLARE_SCREENINFOPTR(arg);
	emgd_priv_t *iptr = EMGDPTR(scrn);

	OS_TRACE_ENTER;
	switch (event) {
		case XF86_APM_SYS_SUSPEND:
		case XF86_APM_CRITICAL_SUSPEND:
		case XF86_APM_USER_SUSPEND:
		case XF86_APM_SYS_STANDBY:
		case XF86_APM_USER_STANDBY:
			/*
			 * Normal case is that undo is false and we're not
			 * suspended, so start the suspend.
			 *
			 * Alternate case is that we are suspended and undo
			 * is true, this is the same as resume?
			 */
			if (!undo && !iptr->suspended) {
				scrn->LeaveVT(VT_FUNC_ARGS(0));
				iptr->suspended = TRUE;
				sleep(SUSPEND_SLEEP);
			} else if (undo && iptr->suspended) {
				sleep(RESUME_SLEEP);
				scrn->EnterVT(VT_FUNC_ARGS(0));
				iptr->suspended = FALSE;
			}
			break;
		case XF86_APM_STANDBY_RESUME:
		case XF86_APM_NORMAL_RESUME:
		case XF86_APM_CRITICAL_RESUME:
			if (iptr->suspended) {
				sleep(RESUME_SLEEP);
				scrn->EnterVT(VT_FUNC_ARGS(0));
				iptr->suspended = FALSE;
				/*
				 * Turn the screen saver off when resuming.  This seems to be
				 * needed to stop xscreensaver kicking in (when used).
				 */
				SaveScreens(SCREEN_SAVER_FORCER, ScreenSaverReset);
			}
			break;
			/* This is currently used for ACPI */
		case XF86_APM_CAPABILITY_CHANGED:
			OS_ERROR("Capability change event.");

			SaveScreens(SCREEN_SAVER_FORCER, ScreenSaverReset);
			break;
		default:
			OS_PRINT("Unhandled APM event %d", event);
			break;
	}
	OS_TRACE_EXIT;
	return TRUE;
}



static Bool emgd_create_screen_resources(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrn);
	Bool ret;
	OS_TRACE_ENTER;

	screen->CreateScreenResources = iptr->create_screen_resources;
	if (!(*screen->CreateScreenResources)(screen)) {
		OS_TRACE_EXIT;
		return FALSE;
	}

	ret = intel_uxa_create_screen_resources(screen);
	OS_TRACE_EXIT;
	return ret;
}


/*
 * emgd_drm_wakeup_handler()
 *
 * This is the EMGD wakeup handler, which will get run every time the
 * X server completes a potentially-blocking system call (e.g.,
 * select() or poll()).  The wakeup may have been caused by a
 * pageflip or vblank event being passed on the DRM filehandle, so
 * we need to check the status of our filehandle and process the
 * event if necessary.
 */
static void emgd_drm_wakeup_handler(pointer data, int err, pointer p)
{
	drmmode_t *kms = data;
	fd_set *read_mask = p;

	/*
	 * If we didn't get our KMS data passed, or if the blocking
	 * system call returned with an error, don't try to do anything.
	 */
	if (!kms || err < 0) {
		return;
	}

	/*
	 * Is there new data on our DRM filehandle?  If so, ask libdrm to
	 * figure out which kind of event it was and pick the appropriate
	 * handler for the type of event we received (the event_context
	 * structure contains pointers to our vblank and pageflip handlers).
	 */
	if (FD_ISSET(kms->fd, read_mask)) {
		drmHandleEvent(kms->fd, &kms->event_context);
	}
}


/*
 * emgd_flush_callback()
 *
 * The flush callback gets called any time the X server decides to
 * flush pending output for clients.  Generally this happens when
 * we're at a good stopping point, so it also makes sense for us
 * to dispatch any batch buffers we've been building to the kernel
 * at this point so that they can get processed rather than sitting
 * in userspace memory forever.
 */
static void emgd_flush_callback(CallbackListPtr *list,
	pointer userdata,
	pointer calldata)
{
	ScrnInfoPtr scrn = userdata;

	/* Only submit the batchbuffer if our VT is actually active. */
	if (scrn->vtSema) {
		intel_batch_submit(scrn);
	}
}


/*
 * emgd_pre_init
 *
 * Called by the X server to start up the driver. The driver will need
 * to detect the hardware capabilities and report these back to the X
 * server.
 */
static Bool emgd_pre_init(ScrnInfoPtr scrn, int flags)
{
	emgd_priv_t *iptr;
	emgd_entity_t *eptr;
	int pf_support;
	Gamma zero_gamma = { 0.0, 0.0, 0.0 };
	rgb defaultWeight = { 0, 0, 0 };
	rgb defaultMask = { 0, 0, 0 };
	int param;
	int sysfsdir;

	oal_screen = scrn->scrnIndex;  /* For logging */
	OS_TRACE_ENTER;

	/* If X was started with -configure */
	if (scrn->confScreen == NULL) {
		return FALSE;
	}

	iptr = calloc(1, sizeof(*iptr));
	if (iptr == NULL) {
		return FALSE;
	}
	scrn->driverPrivate = iptr;

	/* Initialize the EMGD driver priviate */
	iptr->scrn = scrn;

	/* Get the entity info and determine which driver instance this is. */
	iptr->pEnt = xf86GetEntityInfo(scrn->entityList[0]);
	eptr = xf86GetEntityPrivate(scrn->entityList[0],emgd_entity_index)->ptr;
	iptr->entity = eptr;
	iptr->PciInfo = xf86GetPciInfoForEntity(iptr->pEnt->index);

	if (scrn->scrnIndex == eptr->primary_screen) {
		/* Primary driver instance */
		iptr->primary_display = TRUE;
		eptr->primary = iptr;
		OS_DEBUG("Instance %d is primary.", scrn->entityInstanceList[0]);
	} else {
		/* Secondary driver instance */
		iptr->primary_display = FALSE;
		eptr->secondary = iptr;
		OS_DEBUG("Instance %d is secondary.", scrn->entityInstanceList[0]);
	}

	/* Open connection to DRM driver */
	if (iptr->primary_display) {
		iptr->drm_fd = drmOpen(DRM_DRIVER_NAME, "PCI:00:02:00");
		if (iptr->drm_fd == -1) {
			OS_PRINT("Failed to open connection to DRM");
			free(iptr);
			return FALSE;
		}
	} else {
		iptr->drm_fd = iptr->entity->primary->drm_fd;
	}

	/*
	 * Look at the DRM configuration via the sysfs interface
	 * and configure X to match.
	 */
	sysfsdir = open("/sys/module/emgd", O_RDONLY|O_DIRECTORY);
	if (sysfsdir == -1) {
		OS_PRINT("No EMGD sysfs interface found.  Assuming non-EMGD DRM driver.");
	} else {
		close(sysfsdir);
		if ((param = emgd_get_module_parameter("width")) > 0) {
			scrn->virtualX = param;
		}
		if ((param = emgd_get_module_parameter("height")) > 0) {
			scrn->virtualY = param;
		}
		if ((param = emgd_get_module_parameter("depth")) > 0) {
			scrn->depth = param;
		}
		if ((param = emgd_get_module_parameter("bpp")) > 0) {
			scrn->bitsPerPixel = param;
		}
	}

	/* Set some initial driver capabilities */
	if (scrn->depth < 32) {
		pf_support = Support32bppFb | SupportConvert24to32 |
				SupportConvert32to24 | PreferConvert24to32;
	} else {
		pf_support = Support32bppFb | Support24bppFb | SupportConvert24to32 |
				SupportConvert32to24 | PreferConvert32to24;
	}

	scrn->monitor = scrn->confScreen->monitor;
	scrn->rgbBits = 8;

	if(!xf86SetDepthBpp(scrn, scrn->depth, 0, scrn->bitsPerPixel, pf_support)) {
		OS_PRINT("Failed to enable support for screen depth of %d with %d bpp.",
			scrn->depth, scrn->bitsPerPixel);
		free(iptr);
		return FALSE;
	}
	xf86PrintDepthBpp(scrn);

	/* Set the weight */
	if (!xf86SetWeight(scrn, defaultWeight, defaultMask)) {
		/* This will fail for depth = 32. Do the configuration here */
		scrn->weight.red = scrn->weight.green = scrn->weight.blue = 8;
		scrn->rgbBits = 8;
		scrn->offset.red = 16;
		scrn->offset.green = 8;
		scrn->offset.blue = 0;
		scrn->mask.red = 0x00ff0000;
		scrn->mask.green = 0x0000ff00;
		scrn->mask.blue = 0x000000ff;
	}

	/* Set the default visual */
	if (!xf86SetDefaultVisual(scrn, -1)) {
		free(iptr);
		return FALSE;
	}

	/* Gamma correction */
	if (!xf86SetGamma(scrn, zero_gamma) ) {
		OS_PRINT("Set Gamma values failed.");
		free(iptr);
		return FALSE;
	}

	/* Parse options from xorg.conf */
	xf86CollectOptions(scrn, NULL);
	iptr->chipset.info = chipset_info;
	emgd_parse_options(scrn, iptr->chipset.name);

	/* Enabled off screen rendering */
	if (iptr->cfg.render_offscreen) {
		int fd;
		int crtc = 0;
		/*
		 * Update the DRM driver to lock changes to the display
		 * plane address. Note that this must be done before
		 * drmmode_pre_init since that will try to change the
		 * plane address to X's framebuffer.
		 *
		 * Try to do this for all active CRTCs.
		 *
		 * Something external to this driver will need to unlock
		 * the plane by writing "0" to the crtc's lock_plane control.
		 */

		do {
			char path[50];

			sprintf(path, "/sys/module/emgd/control/crtc%d/lock_plane", crtc);
			fd = open(path, O_WRONLY);
			if (fd > 0) {
				ssize_t bytes_write;
				bytes_write = write (fd, "1\n", 3);
				if(!bytes_write){
					OS_ERROR("write failed !");
				}
				close (fd);
				crtc++;   /* try next CRTC */
			} else {
				/* CRTC doesn't exist so finish up */
				crtc = -1;
			}
		} while ((crtc != -1) && (crtc < 999));
	}


	/* KMS initialization */
	if (drmmode_pre_init(scrn, iptr->drm_fd) == FALSE) {
		OS_PRINT("Kernel modesetting setup failed.");
		free(iptr);
		return FALSE;
	}

	/* Set display resolution */
	scrn->currentMode = scrn->modes;
	xf86SetDpi(scrn, 0, 0);

	/* Load the required submodules */
	if (!xf86LoadSubModule(scrn, "fb")) {
		free(iptr);
		return FALSE;
	}

	if(emgd_has_multiplane_drm(iptr)){
		iptr->private_multiplane_drm = 0;
	} else {
		iptr->private_multiplane_drm = 1;
	}
	/* Overlay/Sprite Plane initialization */
	LIST_INIT(&iptr->sprite_planes);
	emgd_sprite_init(iptr);


	OS_TRACE_EXIT;
	return TRUE;
}


/*
 * emgd_close_screen()
 *
 * Performs per-generation cleanup of an X screen.  This function will be
 * called during normal operation of the driver, whereas emgd_free_screen
 * is only called during exceptional conditions where the screen must
 * be destroyed for reasons beyond the driver's control.
 */
static Bool emgd_close_screen(CLOSE_SCREEN_ARGS_DECL)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
	emgd_priv_t *iptr = EMGDPTR(scrn);
	drmmode_t *kms = iptr->kms;

	/* Shut down Sprite */
	emgd_sprite_shutdown(iptr);

	/* Restore screen state */
	if (scrn->vtSema == TRUE) {
		emgd_leave_vt(VT_FUNC_ARGS(0));
	}

	/* Unregister our flush callback */
	DeleteCallback(&FlushCallback, emgd_flush_callback, scrn);

	/* Release bo cache cleanup timer */
	TimerFree(iptr->cache_expire);
	iptr->cache_expire = NULL;

	/* Cleanup UXA */
	if (iptr->uxa_driver) {
		uxa_driver_fini(scrn->pScreen);
		free(iptr->uxa_driver);
		iptr->uxa_driver = NULL;
	}

	/* Release front buffer and unregister DRM framebuffer */
	intel_set_pixmap_bo(scrn->pScreen->GetScreenPixmap(scrn->pScreen), NULL);
	if (kms->fb_id) {
		drmModeRmFB(iptr->drm_fd, kms->fb_id);
		kms->fb_id = 0;
	}

	/* Clean up render engine */
	intel_batch_teardown(scrn);
	gen4_render_state_cleanup(scrn);

	/* Clean up cursor */
	xf86_cursors_fini(scrn->pScreen);

	/* Release buffers used by OTC video shaders */
	i965_free_video(scrn);

	/* Shut down DRI2 */
	if (iptr->dri2_inuse) {
		DRI2CloseScreen(scrn->pScreen);
		free(iptr->dev_dri_name);
		iptr->dev_dri_name = NULL;
		iptr->dri2_inuse = 0;
	}

	/* Unwrap and call CloseScreen */
	scrn->pScreen->CloseScreen = iptr->CloseScreen;
	(*scrn->pScreen->CloseScreen)(CLOSE_SCREEN_ARGS);

	scrn->vtSema = FALSE;

	return TRUE;
}

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,13,0,0,0)
static void emgd_block_handler(BLOCKHANDLER_ARGS_DECL)
{
        return;
}
#endif

/*
 * emgd_screen_init
 *
 * Called when the actual X server screen resource is being initialized.
 */
static Bool emgd_screen_init(SCREEN_INIT_ARGS_DECL)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,13,0,0,0)
        intel_screen_private *intel = intel_get_screen_private(scrn);
#endif
	emgd_priv_t *iptr;
	VisualPtr visual;
	int i;
	xf86CrtcConfigPtr xf86_config;
	emgd_crtc_priv_t *emgd_crtc, *emgd_crtc_scnd;
	drmmode_t *kms;

	oal_screen = scrn->scrnIndex; /* For debug & logging */
	OS_TRACE_ENTER;

	iptr = EMGDPTR(scrn);

	iptr->cpp = scrn->bitsPerPixel / 8;

	if (!emgd_buffer_manager_init(iptr)) {
		OS_ERROR("Failed to initialize buffer management.");
		return FALSE;
	}

	/* DRI2 */
	if (!emgd_dri2_screen_init(screen)) {
		OS_PRINT("DRI2 initialization failed (disabled).");
	} else {
		OS_PRINT("DRI2 initialization complete.");
	}

	/* Initialize front buffer */
	if (!emgd_init_front_buffer(scrn, scrn->virtualX, scrn->virtualY,
				&iptr->fb_pitch)) {
		return FALSE;
	}

	/* Initialize batch buffer and rendering infrastructure */
	intel_batch_init(scrn);
	gen4_render_state_init(scrn);

	/* Set up the visuals */
	miClearVisualTypes();

	/*
	 * If the depth passed in is 32, the number of visuals available
	 * shrinks to just 32 bit visuals (2).  This causes a number of
	 * OpenGL/ES problems.  By limiting the maximum depth passed
	 * into the miSetVisualTypes to 24, the full range of visuals
	 * remain available.
	 */
	if (!miSetVisualTypes((scrn->depth > 24) ? 24 : scrn->depth,
				miGetDefaultVisualMask(scrn->depth),
				scrn->rgbBits, scrn->defaultVisual)) {

		xf86DeleteScreen(XF86_DELETE_SCREEN_ARGS(0));
		return FALSE;
	}

	if (!miSetPixmapDepths()) {
		xf86DeleteScreen(XF86_DELETE_SCREEN_ARGS(0));
		return FALSE;
	}

	scrn->displayWidth = iptr->fb_pitch / iptr->cpp;
	OS_DEBUG("displayWidth = 0x%x  (%d)", scrn->displayWidth,
			scrn->bitsPerPixel);

	OS_DEBUG("Calling fbScreenInit.");
	fbScreenInit(screen, NULL,
			scrn->virtualX, scrn->virtualY, scrn->xDpi, scrn->yDpi,
			scrn->displayWidth, scrn->bitsPerPixel);

	/* Fixup RGB ordering */
	if (scrn->bitsPerPixel > 8) {
		visual = screen->visuals;
		for ( i = 0; i < screen->numVisuals; i++ ) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed = scrn->offset.red;
				visual->offsetGreen = scrn->offset.green;
				visual->offsetBlue = scrn->offset.blue;
				visual->redMask = scrn->mask.red;
				visual->greenMask = scrn->mask.green;
				visual->blueMask = scrn->mask.blue;
			}
			visual++;
		}
	}

	fbPictureInit(screen, 0, 0);
	xf86SetBlackWhitePixels(screen);

	/* Initialize UXA */
	if (!intel_uxa_init(screen)) {
		OS_ERROR("UXA Initialization failed.");
		xf86DeleteScreen(XF86_DELETE_SCREEN_ARGS(0));
		return FALSE;
	}

	/* XV Init */
	OS_PRINT("Initializing XVideo support.");
	if (!emgd_init_video(screen)) {
		OS_ERROR("XV Initialization failed.");
	}

	miInitializeBackingStore(screen);
	xf86SetBackingStore(screen);

	miDCInitialize(screen, xf86GetPointerScreenFuncs());

	/* Hardware cursor init */
	if (iptr->cfg.hw_cursor) {
		OS_PRINT("Initializing HW cursor.");
		if (!xf86_cursors_init(screen, 64, 64,
					(HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
					HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
					HARDWARE_CURSOR_INVERT_MASK |
					HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
					HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
					HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED |
					HARDWARE_CURSOR_UPDATE_UNHIDDEN |
					HARDWARE_CURSOR_ARGB))) {
			OS_ERROR("Hardware cursor initialization failed\n");
		}
	} else {
		OS_PRINT("Using SW cursor.");
	}

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,13,0,0,0)
         intel->BlockHandler = screen->BlockHandler;
         screen->BlockHandler = emgd_block_handler;
#endif

	/* Wrap CloseScreen for driver-specific per-generation cleanup */
	iptr->CloseScreen = screen->CloseScreen;
	screen->CloseScreen = emgd_close_screen;

	/* Control API protocol init */
	OS_PRINT("Initializing custome control API extension.");
	emgd_extension_init(scrn);

	scrn->vtSema = TRUE;

	if (!emgd_enter_vt(VT_FUNC_ARGS(0))) {
		return FALSE;
	}

	/* Use the standard 'SaveScreen' handler to set screen DPMS */
	screen->SaveScreen = xf86SaveScreen;

	/* Create Screen Resources */
	iptr->create_screen_resources = screen->CreateScreenResources;
	screen->CreateScreenResources = emgd_create_screen_resources;

	if (!xf86CrtcScreenInit(screen)) {
		return FALSE;
	}

	miCreateDefColormap(screen);
	xf86HandleColormaps(screen, 256, 8, emgd_load_palette, 0,
			CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR);

	xf86DPMSInit(screen, xf86DPMSSet, 0);

	iptr->suspended   = FALSE;

	/* Grab our KMS data */
	xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	emgd_crtc = xf86_config->crtc[0]->driver_private;
	emgd_crtc_scnd = xf86_config->crtc[1]->driver_private;
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,13,0,0,0)
	/*Check for the num of crtcs*/
       if(xf86_config->num_crtc == 2){ /* Two crtcs*/
               if(emgd_crtc->shadow_fb_id || emgd_crtc_scnd->shadow_fb_id){
                       xf86_config->BlockHandler = emgd_block_handler;
               }
       }
       else{   /* One crtc*/
               if(emgd_crtc->shadow_fb_id){
                        xf86_config->BlockHandler = emgd_block_handler;
                }
       }
#endif
	kms = emgd_crtc->drmmode;

	/*
	 * Add our DRM filehandle to the X server's select()/poll() filehandle set.
	 * This will ensure that the X server process is woken up when the DRM
	 * sends pageflip or vblank events to userspace.
	 */
	AddGeneralSocket(iptr->drm_fd);

	/*
	 * Register block and wakeup handlers.  A BlockHandler is a callback that
	 * gets called immediately before the X server does something that might
	 * cause it to block (e.g., by calling select()/poll() system calls).  The
	 * WakeupHandler is a callback that gets called immediately after one of
	 * those blocking syscalls completes.  Our driver doesn't have anything to
	 * do in the BlockHandler (so we use the generic "Don't Do Anything"
	 * handler), but a wakeup might have been caused by a DRM event (which
	 * we registered above), so we need to process vblank or pageflip
	 * events.
	 *
	 * See doc/Xserver-spec.xml in the Xorg source tree for more details.
	 */
	RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
		emgd_drm_wakeup_handler, kms);

	/*
	 * Register a flush callback.  When the X server decides it's time to
	 * flush output to clients generally means it's also a good time to
	 * submit any batchbuffers we've been building to the kernel so that
	 * they get processed in a timely manner.
	 */
	AddCallback(&FlushCallback, emgd_flush_callback, scrn);

	OS_TRACE_EXIT;
	return TRUE;
}


/*
 * Entry points called by the X server when initializing the driver:
 *
 *   emgd_identify - Prints a message to the X server log that identifes
 *                   the driver.  Should be user friendly.
 *
 *   emgd_available_options - Proces the xorg.conf file for driver options.
 *
 *   emgd_pci_probe - Match the PCI device to the driver entity.
 */

/*
 * emgd_identify
 *
 * Entry point that the X server will call to get the driver's description.
 * This is output to the X server's log file.
 *
 * Make sure we note if this is a debug driver.
 */
static void emgd_identify(int flags)
{
	char version_str[180];

#if DEBUG
	sprintf (version_str,
		"** DEBUG ** Intel(R) Embedded Media and Graphics Driver ** DEBUG **, "
		"debug level 0x%x "
		"(Not For Release), "
		"version %d.%d.%d for",
		DEBUG, EMGD_MAJOR_VERSION, EMGD_MINOR_VERSION, EMGD_PATCHLEVEL);
#else
	sprintf (version_str,
		"Intel(R) Embedded Media and Graphics Driver version %d.%d.%d for",
		EMGD_MAJOR_VERSION, EMGD_MINOR_VERSION, EMGD_PATCHLEVEL);
#endif
	xf86PrintChipsets(EMGD_NAME, version_str, emgd_ChipsetFamilies );
}



/*
 * emgd_pci_probe
 */

static Bool emgd_pci_probe(
	DriverPtr driver,
	int entity_num,
	struct pci_device *device,
	intptr_t match_data)
{
	ScrnInfoPtr p_scrn = NULL;
	EntityInfoPtr e_ptr;
	Bool found_screen = FALSE;
	DevUnion *pPriv;
	emgd_entity_t *eptr;
	/*
	 * The below code will turn on full debugging for init functions.
	 *
	 *  unsigned char debug_level[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	 *      0xff, 0xff};
	 * memcpy(igd_debug, &debug_level, 8);
	 */

	oal_screen = -1;
	OS_TRACE_ENTER;

	chipset_info = (void *)match_data;

	OS_DEBUG("Configuring PCI Entity");
	p_scrn = xf86ConfigPciEntity(p_scrn, 0, entity_num, emgd_PCIChipsets, NULL,
			NULL, NULL, NULL, NULL);

	if (p_scrn != NULL) {
		OS_DEBUG("Configure screen/entity.");
		p_scrn->driverVersion = EMGD_VERSION;
		p_scrn->driverName = EMGD_DRIVER_NAME;
		p_scrn->name = EMGD_NAME;
		p_scrn->Probe = NULL;
		p_scrn->PreInit = emgd_pre_init;
		p_scrn->ScreenInit = emgd_screen_init;
		/* p_scrn->SwitchMode = emgd_switch_mode; */
		p_scrn->AdjustFrame= emgd_adjust_frame;
		p_scrn->EnterVT = emgd_enter_vt;
		p_scrn->LeaveVT = emgd_leave_vt;
		p_scrn->FreeScreen = emgd_free_screen;
		p_scrn->ValidMode = emgd_valid_mode;
		p_scrn->PMEvent = emgd_power_event;
		found_screen = TRUE;

		OS_DEBUG("Get Entity Info");
		e_ptr = xf86GetEntityInfo(entity_num);
		xf86SetEntitySharable(entity_num);

		if ( e_ptr ) {

			OS_DEBUG("Get/Create Entity Private data.");
			if (emgd_entity_index < 0) {
				emgd_entity_index = xf86AllocateEntityPrivateIndex();
			}
			pPriv = xf86GetEntityPrivate(p_scrn->entityList[0],
					emgd_entity_index);
			if ( !pPriv->ptr ) {
				pPriv->ptr = xnfcalloc(sizeof(emgd_entity_t), 1);
				eptr = (emgd_entity_t *)pPriv->ptr;
				eptr->lastInstance = -1;
				eptr->num_devices = 0;
				eptr->primary_screen = p_scrn->scrnIndex;
				eptr->chipset = NULL;
			} else {
				eptr = (emgd_entity_t *)pPriv->ptr;
			}
			eptr->num_devices++;
			eptr->lastInstance++;

			OS_DEBUG("Set Entity Instance for Screen");
			xf86SetEntityInstanceForScreen(p_scrn, p_scrn->entityList[0],
					eptr->lastInstance);
		}

		free(e_ptr);
	}

	OS_TRACE_EXIT;
	return found_screen;
}








/*
 * DDX driver module initialization
 */
static Bool emgd_ddx_driver_func(ScrnInfoPtr scrn,
		xorgDriverFuncOp op,
		pointer ptr)
{
	switch (op) {
		case GET_REQUIRED_HW_INTERFACES:
			*((CARD32*)ptr) = 0;
			return TRUE;
		default: /* Unknown function */
			return FALSE;
	}
}

_X_EXPORT DriverRec emgd_ddx = {
	EMGD_VERSION,
	EMGD_DRIVER_NAME,
	emgd_identify,
	NULL,  /* old probe function, no longer used */
	emgd_available_options,
	NULL,
	0,
	emgd_ddx_driver_func,
#ifdef XSERVER_LIBPCIACCESS
	emgd_device_match,
	emgd_pci_probe
#endif
};


/*
 *  emgd_ddx_setup
 *
 *  Called to add this driver to the X server's driver list. This should
 *  be called only once.
 */
static pointer emgd_ddx_setup(pointer module,
		pointer opts,
		int *errmaj,
		int *errmin)
{
	static Bool finished_setup = 0;

	/* Make sure this is only executed once */
	if (!finished_setup) {
		xf86AddDriver(&emgd_ddx, module, HaveDriverFuncs);
		finished_setup = 1;
		return (pointer)1;
	} else {
		if (errmaj) {
			*errmaj = LDR_ONCEONLY;
		}
		return NULL;
	}
}


static XF86ModuleVersionInfo emgd_ddx_version_info = {
	"emgd",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	EMGD_MAJOR_VERSION,
	EMGD_MINOR_VERSION,
	EMGD_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData emgdModuleData = {
	&emgd_ddx_version_info,
	emgd_ddx_setup,
	NULL
};

