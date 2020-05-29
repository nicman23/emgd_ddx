/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_api.c
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
 *  This is an implimentation of a protocol extension to X that allows
 *  client applications to communication with driver.
 *
 *  The following custom APIs are included here:
 *     Get / Set sprite plane color correction
 *     Set sprite plane z-order
 *     Get driver info
 *     Get port info
 *     SplashScreen
 *
 *  Most of these may move to DRM IOCTL commands.  The exceptions would
 *  be z-order control, splashscreen control, and possibly X ddx driver info.
 *
 *-----------------------------------------------------------------------------
 */

#define PER_MODULE_DEBUG
#define MODULE_NAME ial.escape

#include <unistd.h>
#include <string.h>
#include <xf86.h>
#include <xf86_OSproc.h>
#include <xf86xv.h>
#include <scrnintstr.h>
#include <windowstr.h>
#include <pixmapstr.h>
#include <extnsionst.h>
#include <dixstruct.h>
#include "emgd.h"
#define _EMGD_SERVER_
#include <emgd_apistr.h>
#include "ddx_version.h"

/* External function protypes */
extern int emgd_drm_query_ovl(void * kms_display_handle, int fd, unsigned int flags);

/* Function prototypes for all API implemenation functions */
static int driver_info(int *size, void **output);
static int set_ovl_color_params(void *color_to_set);
static int get_ovl_color_params(int *size, void **color_returned);
static int get_debug(int *size, void **output);
#if DEBUG
static int set_debug(void *input);
#endif

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,11,99,901,0)
#define SWAPL(x, n) swapl(x, n)
#define SWAPS(x, n) swaps(x, n)
#else
#define SWAPL(x, n) swapl(x)
#define SWAPS(x, n) swaps(x)
#endif

static DEV_PRIVATE_KEY_TYPE emgd_client_private_index;

/* For driver info data */
extern SymTabRec   emgd_Chipsets[];             /* from emgd_ddx.c */
extern PciChipsets emgd_PCIChipsets[];          /* from emgd_ddx.c */

int emgd_init_generation = 0;

static int emgd_error_base;
static int emgd_req_code;
static int emgd_screen[2];


/* This holds the client's version information */
typedef struct {
	int		major;
	int		minor;
} emgd_priv_rec_t;


/*
 * This is the implementation of the X protocol extension.  Most of
 * this is standard for any extension. Only the emgd_api is unique
 * to this extension.
 */
static DISPATCH_PROC(emgd_dispatch);
static DISPATCH_PROC(emgd_query_version);
static DISPATCH_PROC(emgd_set_client_version);
static DISPATCH_PROC(emgd_api);

static DISPATCH_PROC(s_emgd_dispatch);
/*
static DISPATCH_PROC(s_emgd_query_version);
static DISPATCH_PROC(s_emgd_set_client_version);
static DISPATCH_PROC(s_emgd_api);
*/


/* Dispatch the incoming request to the right function */
static int emgd_dispatch(ClientPtr client)
{
	REQUEST(xReq);
	switch (stuff->data) {
	case X_EMGDQueryVersion:
		return emgd_query_version(client);
	case X_EMGDSetClientVersion:
		return emgd_set_client_version(client);
	case X_EMGDControl:
		return emgd_api(client);
	default:
		return BadRequest;
	}
}

/*
 * This is suppose to do byte order swaping but since our driver
 * will never by used on a system with a different byte order,
 * don't bother.
 */
static int s_emgd_dispatch(ClientPtr client)
{
	REQUEST(xReq);
	switch (stuff->data) {
	case X_EMGDQueryVersion:
		return emgd_query_version(client);
	case X_EMGDSetClientVersion:
		return emgd_set_client_version(client);
	case X_EMGDControl:
		return emgd_api(client);
	default:
		return BadRequest;
	}
}


/*
 * emgd_api
 *
 * The client library has one interface. This allows us to keep the
 * client library simple and static but at the same time allows us
 * to add new capabilities to the driver.
 *
 * Requests to the driver are formated as follows:
 *
 * API_FUNCTION_ID, (void *)arguments, (void *)results
 *
 * The actual sturcture of the arguments and results depends on the
 * API_FUNCTION_ID.
 *
 * The emgd_api function acts as a big dispatch function. Based on the
 * API_FUNCTION_ID
 */
static int emgd_api(ClientPtr client)
{
	REQUEST(xEMGDControlReq);
	xEMGDControlReply rep;
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,11,99,0,0)
	register int n;
#endif
	emgd_priv_t *iptr;
	ScrnInfoPtr scrn;
	unsigned long reply_size = 0;
	int status;
	int out_size = 0;
	void *output = NULL;

	oal_screen = emgd_screen[0];
	OS_TRACE_ENTER;
	REQUEST_FIXED_SIZE(xEMGDControlReq, stuff->in_size);

	scrn = xf86Screens[emgd_screen[0]];
	iptr = EMGDPTR(scrn);

	/*
	 * Check if the x server has ben suspended. If so, we no longer control
	 * the grahpics device, so ignore any escape requests.  However, we
	 * must still generate a valid reply or we'll corrupt the protocol
	 * stream.
	 */
	if (iptr->suspended) {
		/* If a reply is expected, Create an empty reply */
		if (stuff->do_reply) {
			rep.type = X_Reply;
			rep.length = SIZEOF(xEMGDControlReply) - SIZEOF(xGenericReply);
			rep.length >>= 2;
			rep.sequenceNumber = client->sequence;
			rep.status = EMGD_CONTROL_ERROR;
			rep.out_length = 0;
			if (client->swapped) {
				SWAPS(&rep.sequenceNumber, n);
				SWAPL(&rep.length, n);
				SWAPL(&rep.status, n);
				SWAPL(&rep.out_length, n);
			}

			WriteToClient(client, sizeof(xEMGDControlReply), (char *)&rep);
		}
		return 0;
	}

	/*
	 * Incoming protocol has 3 data items:
	 *   Escape code             = stuff->api_function
	 *   size of incoming data
	 *   incoming data block     = stuff[1]
	 *
	 * Reply will have:
	 *   status                  = status
	 *   size of outgoing data   = out_size
	 *   outging data packet     = output
	 *
	 * For outputing data back to the client:
	 *   1. set status to success or fail
	 *   2. set output to point at the data to be returned. This data
	 *      block must be dynamiclly allocated as it will be freed after
	 *      it is sent.
	 *   3. set the output size to the size of the data block.
	 *
	 */

	OS_DEBUG("Process API function 0x%lx", (unsigned long)stuff->api_function);
	switch (stuff->api_function) {
	case EMGD_CONTROL_GET_DRIVER_INFO:
		OS_DEBUG("  -> GET_DRIVER_INFO");
		status = driver_info(&out_size, &output);
		break;
	case EMGD_CONTROL_GET_DEBUG:
		OS_DEBUG("  -> ESCAPE_GET_DEBUG");
		status = get_debug(&out_size, &output);
		break;
	case EMGD_CONTROL_SET_DEBUG:
		OS_DEBUG("  -> ESCAPE_SET_DEBUG");
#if DEBUG
		/* Only allow setting of debug if this is a debug driver */
		status = set_debug(&stuff[1]);
#else
		status = EMGD_CONTROL_SUCCESS;
#endif
		out_size = 0L;
		output   = (void *)NULL;
		break;
	case EMGD_CONTROL_SET_OVL_COLOR_PARAMS:
		OS_DEBUG("  -> ESCAPE_SET_OVL_COLOR_PARAMS");
		status = set_ovl_color_params(&stuff[1]);
		out_size = 0;
		output = (void *)NULL;
		break;
	case EMGD_CONTROL_GET_OVL_COLOR_PARAMS:
		OS_DEBUG("  -> ESCAPE_GET_OVL_COLOR_PARAMS");
		status = get_ovl_color_params(&out_size, &output);
		break;
	default:
		OS_DEBUG("  -> ESCAPE UNKNOWN");
		output = (void *)NULL;
		out_size = 0;
		status = EMGD_CONTROL_ERROR;
		break;
	}

	if (stuff->do_reply) {
		/* OPTIMIZE:
		 *   If we make sure all the iegd_esc*_t structures are all
		 *   4 byte aligned, we can get rid of this check. For OGL
		 *   processing the speed improvement may be worth the risk.
		 */
		reply_size = (out_size + 0x03) & ~ 0x03;
		if (reply_size != out_size) {
			/* need to pad the output buffer */
			output = realloc(output, reply_size);
			if (output == NULL) {
				OS_ERROR("Resizing output data buffer failed.");
				reply_size = 0;
			}
		}
		OS_DEBUG("  -> Sending reply: size = %lu", reply_size);

		/* Create reply */
		rep.type = X_Reply;
		rep.length = SIZEOF(xEMGDControlReply) - SIZEOF(xGenericReply);
		rep.length += reply_size;
		rep.length >>= 2;
		rep.sequenceNumber = client->sequence;
		rep.status = status;
		rep.out_length = reply_size;
		if (client->swapped) {
			SWAPS(&rep.sequenceNumber, n);
			SWAPL(&rep.length, n);
			SWAPL(&rep.status, n);
			SWAPL(&rep.out_length, n);
		}
		OS_DEBUG("  -> rep.length = %lu", (unsigned long)rep.length);

		WriteToClient(client, sizeof(xEMGDControlReply), (char *)&rep);

		if (output != NULL) {
			OS_DEBUG("  -> Sending back escape reply data of size %lu",
					reply_size);
			WriteToClient(client, reply_size, (char *)output);
		}
	}

	free(output);

	OS_TRACE_EXIT;
	return 0;
}


/*
 * emgd_query_version
 *
 * Return the extension version to the client application.
 */
static int emgd_query_version(ClientPtr client)
{
	xEMGDQueryVersionReply rep;
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,11,99,0,0)
	register int n;
#endif

	OS_DEBUG("emgd_query_version: enter");

	REQUEST_SIZE_MATCH(xEMGDQueryVersionReq);
	rep.type = X_Reply;
	rep.length = 0;
	rep.sequenceNumber = client->sequence;
	rep.major_version = EGD_ESC_MAJOR_VERSION;
	rep.minor_version = EGD_ESC_MINOR_VERSION;
	if (client->swapped) {
		SWAPS(&rep.sequenceNumber, n);
		SWAPL(&rep.length, n);
		SWAPS(&rep.major_version, n);
		SWAPS(&rep.minor_version, n);
	}
	WriteToClient(client, sizeof(xEMGDQueryVersionReply), (char *)&rep);
	return (client->noClientException);
}


/*
 * emgd_set_client_version
 *
 * Set the version of the extension library that the client application was
 * compiled with.  This is so that future releases can be backward compatable.
 */
static int emgd_set_client_version(ClientPtr client)
{
	REQUEST(xEMGDSetClientVersionReq);
	emgd_priv_rec_t *emgd_client_version = NULL;


	OS_DEBUG("iegd_set_client_version: enter");

	REQUEST_SIZE_MATCH(xEMGDSetClientVersionReq);

	emgd_client_version = dixLookupPrivate(&client->devPrivates,
		&emgd_client_private_index);
	if (emgd_client_version == NULL) {
		OS_DEBUG("iegd_set_client_version: allocating new private struct");
		emgd_client_version = malloc(sizeof(emgd_priv_rec_t));
		if (!emgd_client_version) {
			return BadAlloc;
		}
		dixSetPrivate(&client->devPrivates, &emgd_client_private_index,
			emgd_client_version);
	}
	emgd_client_version->major = stuff->major;
	emgd_client_version->minor = stuff->minor;

	OS_DEBUG("iegd_set_client_version: exit");
	return (client->noClientException);
}





/**************************************************************************
 *
 * Functions below this point implement the escape functions.
 * Functions above this point implement the extension protocol.
 *
 *************************************************************************/


static int driver_info(int *size, void **output)
{
	iegd_esc_driver_info_t *driver_info;

	driver_info =
			(iegd_esc_driver_info_t *)malloc(sizeof(iegd_esc_driver_info_t));
	*output = (void *)driver_info;

	if (driver_info == NULL) {
		*size = 0;
		return EMGD_CONTROL_ERROR;
	} else {
		driver_info->major     = EMGD_MAJOR_VERSION;
		driver_info->minor     = EMGD_MINOR_VERSION;
		driver_info->build     = EMGD_PATCHLEVEL;
		driver_info->device_id = emgd_PCIChipsets[0].PCIid;
		strncpy(driver_info->date, __DATE__, MAX_NAME_LEN);
		strncpy(driver_info->name, EMGD_DRIVER_TITLE, MAX_NAME_LEN);
		strncpy(driver_info->chipset, emgd_Chipsets[0].name, MAX_NAME_LEN);
		*size = sizeof(iegd_esc_driver_info_t);
		return EMGD_CONTROL_SUCCESS;
	}
}

/*
 * Get and set debug
 *
 * These api's allow control of the debug flags via run-time control.
 */
static int get_debug(int *size, void **output)
{
	iegd_esc_debug_info_t *debug_info;

	debug_info =
			(iegd_esc_debug_info_t *)malloc(sizeof(iegd_esc_debug_info_t));
	*output = (void *)debug_info;

	if (debug_info == NULL) {
		*size = 0;
		return  EMGD_CONTROL_ERROR;
	} else {
		memcpy(&debug_info->ial_flags, &igd_debug->ial, sizeof(unsigned long));
		*size = sizeof(iegd_esc_debug_info_t);
		return EMGD_CONTROL_SUCCESS;
	}
}

#if DEBUG
static int set_debug(void *input)
{
	iegd_esc_debug_info_t *debug_info;

	debug_info = (iegd_esc_debug_info_t *)input;

	memcpy(&igd_debug->ial, &debug_info->ial_flags, sizeof(long));
	OS_PRINT("IAL Debugging level set to 0x%" PRIx32, debug_info->ial_flags);

	return EMGD_CONTROL_SUCCESS;
}
#endif


/*
 * set_ovl_color_params
 *
 * This function sets the color correction parameters for the overlay.
 *
 * This sets the information in the iptr->cfg and then the intel_video.c
 * passes this information into the actual igd_ovl_info.  The reason for
 * this is the igd_ovl_info is private data only available during a call
 * to the overlay (such as intel_OverlayPutImage).
 *
 * This may be replaced by a DRM IOCTL command in the future.
 */
static int set_ovl_color_params(void *input)
{
	emgd_priv_t *iptr;
	ScrnInfoPtr scrn;
	iegd_esc_color_params_t *color_params;
	int ret;

	/*
	 * There is only 1 set of video quality and gamma settings, so
	 * always use the primary.
	 */
	scrn = xf86Screens[emgd_screen[0]];
	iptr = EMGDPTR(scrn);

	color_params = (iegd_esc_color_params_t *)input;

	/* Check if color correction is supported */
	ret = emgd_drm_query_ovl(NULL, iptr->drm_fd,
				IGD_OVL_QUERY_IS_GAMMA_SUPPORTED);
	if (!ret) {
		/* Just return */
		return EMGD_CONTROL_SUCCESS;
	}

	if (color_params->flag & OVL_COLOR_FLAG) {
		OS_DEBUG("intel_set_ovl_color_params");

		/* set gamma */
		if (color_params->flag & GAMMA_FLAG) {
			/* Convert the input R-G-B gamma (which is in  3i.5f
			 * format) to 24i.8f format */
			iptr->cfg.ovl_gamma_r =
				((color_params->gamma >> 16) & 0xFF) << 3;

			iptr->cfg.ovl_gamma_g =
				((color_params->gamma >> 8) & 0xFF) << 3;

			iptr->cfg.ovl_gamma_b =
				(color_params->gamma & 0xFF) << 3;

			OS_DEBUG("New gamma values %u %u %u",
					iptr->cfg.ovl_gamma_r,
					iptr->cfg.ovl_gamma_g,
					iptr->cfg.ovl_gamma_b);
		}

		/* set brightness */
		if (color_params->flag & BRIGHTNESS_FLAG) {
			iptr->cfg.ovl_brightness = color_params->brightness;
		}

		/* set contrast */
		if (color_params->flag & CONTRAST_FLAG) {
			iptr->cfg.ovl_contrast = color_params->contrast;
		}

		/* set saturation */
		if (color_params->flag & SATURATION_FLAG) {
			iptr->cfg.ovl_saturation = color_params->saturation;
		}

		if(color_params->flag & COLORKEY_FLAG) {
			iptr->cfg.video_key = color_params->colorkey;
		}
	}
	else {
		OS_DEBUG("intel_set_ovl_color_params Error");
		return EMGD_CONTROL_ERROR;
	}

	return EMGD_CONTROL_SUCCESS;
}



static int get_ovl_color_params(int *size, void **output)
{
	emgd_priv_t *iptr;
	ScrnInfoPtr  scrn;
	int gamma_r, gamma_g, gamma_b;
	int ret;
	iegd_esc_color_params_t *color_params;

	/*
	 * There is only 1 set of video quality and gamma settings, so
	 * always use the primary.
	 */
	scrn = xf86Screens[emgd_screen[0]];
	iptr  = EMGDPTR(scrn);

	/* allocate memory to hold the results returned */
	*size = sizeof(iegd_esc_color_params_t);
	color_params = (iegd_esc_color_params_t *)malloc(*size);
	*output = (void *)color_params;

	if ( NULL == color_params ) {
		OS_DEBUG("intel_get_ovl_color_params Error");
		*size = 0;
		return EMGD_CONTROL_ERROR;
	}

	memset(color_params, 0, sizeof(iegd_esc_color_params_t));

	/* Check if gamma correction is supported. We only fill in Gamma params. */
	ret = emgd_drm_query_ovl(NULL, iptr->drm_fd,
			IGD_OVL_QUERY_IS_GAMMA_SUPPORTED);
	if(ret){
		/* The gamma is in 24i.8f format, convert it to 3i.5f format.
		 * This works because gamma is suppose to be in the range of 0.6 - 6 */
		gamma_r = (iptr->cfg.ovl_gamma_r >> 3) & 0xFF;
		gamma_g = (iptr->cfg.ovl_gamma_g >> 3) & 0xFF;
		gamma_b = (iptr->cfg.ovl_gamma_b >> 3) & 0xFF;

		color_params->gamma = (gamma_r << 16) | (gamma_g << 8) | (gamma_b);
	}

	/* Check if other video params are supported. */
	ret = emgd_drm_query_ovl(NULL, iptr->drm_fd,
			IGD_OVL_QUERY_IS_VIDEO_PARAM_SUPPORTED);
	if(ret){
		/* Get brightness, contrast and saturation */
		color_params->brightness = iptr->cfg.ovl_brightness;
		color_params->contrast   = iptr->cfg.ovl_contrast;
		color_params->saturation = iptr->cfg.ovl_saturation;
		color_params->colorkey   = iptr->cfg.video_key;
	}

	return EMGD_CONTROL_SUCCESS;
}



/****************************************************************************
 *
 *  Extension initialization
 *
 ***************************************************************************/

/*
 * emgd_reset_proc
 *
 * This will get called when the extension is removed. Most likey when
 * the server is shutting down.
 */
static void emgd_reset_proc (ExtensionEntry *ext_entry)
{
	OS_DEBUG("EMGD control extension resetting...");
}

/*
 * emgd_extension_init
 *
 * Called to initialize the extension. This must be called by the
 * driver to make the extension available to both the client and the
 * driver.
 */
void emgd_extension_init(ScrnInfoPtr scrn)
{
	ExtensionEntry* ext_entry;
	emgd_priv_t *iptr;
	/*
	ScreenPtr screen;
	emgd_screen_priv_t *sptr;
	*/

	OS_DEBUG("emgd_extension_init: enter");
	OS_DEBUG("emgd_extension: serverGeneration = %lu, extension = %u",
		serverGeneration, emgd_init_generation);

	if (!DIX_REGISTER_PRIVATE(&emgd_client_private_index, PRIVATE_CLIENT, 0)) {
		OS_ERROR("Failed to register emgdapi DevPrivate");
		return;
	}

	if ((ext_entry = AddExtension(EMGDNAME, 0, 0,
			emgd_dispatch,
			s_emgd_dispatch,
			emgd_reset_proc,
			StandardMinorOpcode))) {
		emgd_req_code = (unsigned char)ext_entry->base;
		emgd_error_base = ext_entry->errorBase;
	} else {
		OS_DEBUG("emgd_extension_init: failed to initialize extension");
		return;
	}

	/*
	 *  Need to check any escape function that passes in a screen to
	 *  make sure it matches at least one of the screen indexes that
	 *  initialized this extension (iegd_screen may need to be a list
	 *  of screen indexes instead of a single value).  This is critical
	 *  for any function that tries to access the driver private data
	 *  because it will crash X if we end up accessing some other drivers
	 *  private data!
	 */
	iptr = EMGDPTR(scrn);
	if (iptr->primary_display) {
		emgd_screen[0] = scrn->scrnIndex;
	} else {
		emgd_screen[1] = scrn->scrnIndex;
	}
}


/**************************************************************************
 *
 * Functions below this point are wrappers to the kernel module IOCTLs
 *
 *************************************************************************/

/*
 * NOTE: Most of this file contains implementations of user space interface
 * to the EMGD HAL API that resides in the kernel mode driver.
 *
 * The naming convention is:  emgd_drm_<HAL-procedure-pointer-name>()
 */

/*
 * The next set of procedures are for use during the X driver's PreInit()
 * timeframe.  They provide the functionality that used to be provided by
 * igd_module_init() and igd_get_config_info().
 */

int emgd_drm_query_ovl(void * kms_display_handle,
		int drm_fd,
		unsigned int flags)
{
	OS_ERROR("emgd_drm_query_ovl unsupported! OVERLAY module still WIP!");
	return -1;
}

#if 0

 	 THESE ARE COMMENTED OUT - BUT KEPT HERE FOR REFERENCE ONLY.
 	 ALL OF THEM ARE MANAGING FOR OVERLAY PLANES FROM X DDX-DRIVER
 	 FOR EITHER XVIDEO / OGL-ON-SPRITE FEATURES (NEW) / VA-PUT-SURFACE
 	 ON SPRITE FEATURE (NEW). ONCE THE MECHANICS ARE DETAILED
 	 USING THE NEW KMS APIS, THESE BELOW MIGHT START GETTING RESURRECTED
 	 BUT WITH DIFFERENT IOCTL CODES / STRUCTURES.


/*!
 * User-space bridge version of the HAL get_ovl_init_params() procedure.
 */
int emgd_drm_get_ovl_init_params(igd_driver_h driver_handle,
		int drm_fd,
		ovl_um_context_t *ovl_um_context)
{
	emgd_drm_get_ovl_init_params_t drm_data;

	OS_TRACE_ENTER;
	OS_DEBUG("emgd_drm_get_ovl_init_params entry.\n");
	memset(&drm_data, 0, sizeof(emgd_drm_get_ovl_init_params_t));
	drm_data.ovl_um_context = ovl_um_context;


	/* Call the ioctl: */
	OS_DEBUG("emgd_drm_get_ovl_init_params() calling IOCTL\n");
	if (drmCommandWriteRead(drm_fd,
				DRM_IGD_GET_OVL_INIT_PARAMS,
				&drm_data,
				sizeof(emgd_drm_get_ovl_init_params_t))) {
		OS_PRINT("[EMGD] Communication With DRM Failed\n");
		return -1;
	}

	/* No parameters to copy.
	 * KM filled in the the user mode overlay context */

	/* Return the "return value" returned in the DRM ioctl struct: */
	OS_DEBUG("emgd_drm_get_ovl_init_params() return value:%d\n",
			drm_data.rtn);
	OS_TRACE_EXIT;
	return drm_data.rtn;

} /* emgd_drm_get_ovl_init_params() */


/*!
 * User-space bridge version of the HAL query_ovl() procedure.
 */
int emgd_drm_query_ovl(igd_display_h display_handle,
		int drm_fd,
		unsigned int flags)
{
	emgd_drm_query_ovl_t drm_data;


	/* Copy parameters into the DRM ioctl struct: */
	OS_TRACE_ENTER;
	memset(&drm_data, 0, sizeof(emgd_drm_query_ovl_t));
	drm_data.display_handle = display_handle;
	drm_data.flags = flags;


	/* Call the ioctl: */
	if (drmCommandWriteRead(drm_fd, DRM_IGD_QUERY_OVL,
		&drm_data, sizeof(emgd_drm_query_ovl_t))) {
		OS_PRINT("[EMGD] Communication With DRM Failed\n");
		return -1;
	}


	/* Return the "return value" returned in the DRM ioctl struct: */
	OS_DEBUG("emgd_drm_query_ovl() return value:%d\n", drm_data.rtn);
	OS_TRACE_EXIT;
	return drm_data.rtn;
} /* emgd_drm_query_ovl() */


/*!
 * User-space bridge version of the HAL query_max_size_ovl() procedure.
 */
int emgd_drm_query_max_size_ovl(igd_display_h display_handle,
		int drm_fd,
		unsigned long pf,
		unsigned int *max_width,
		unsigned int *max_height)
{
	emgd_drm_query_max_size_ovl_t drm_data;


	/* Copy parameters into the DRM ioctl struct: */
	OS_TRACE_ENTER;
	memset(&drm_data, 0, sizeof(emgd_drm_query_max_size_ovl_t));
	drm_data.display_handle = display_handle;
	drm_data.pf = pf;


	/* Call the ioctl: */
	if (drmCommandWriteRead(drm_fd, DRM_IGD_QUERY_MAX_SIZE_OVL,
		&drm_data, sizeof(emgd_drm_query_max_size_ovl_t))) {
		OS_PRINT("[EMGD] Communication With DRM Failed\n");
		return -1;
	}


	/* Copy returned parameters from the DRM ioctl struct: */
	*max_width = drm_data.max_width;
	*max_height = drm_data.max_height;


	/* Return the "return value" returned in the DRM ioctl struct: */
	OS_DEBUG("emgd_drm_query_max_size_ovl() return value:%d\n",
		drm_data.rtn);
	OS_TRACE_EXIT;
	return drm_data.rtn;
} /* emgd_drm_query_max_size_ovl() */


/*!
 * User-space bridge version of the HAL alter_ovl() procedure.
 */

int emgd_drm_alter_ovl(igd_display_h display_handle,
		int drm_fd,
		igd_surface_t       *src_surf,
		igd_rect_t          *src_rect,
		igd_rect_t          *dst_rect,
		igd_ovl_info_t      *ovl_info,
		unsigned int         flags)
{
	emgd_drm_alter_ovl2_t drm_data;

	/* Copy parameters into the DRM ioctl struct: */
	OS_TRACE_ENTER;

	memset(&drm_data, 0, sizeof(emgd_drm_alter_ovl2_t));
	drm_data.display_handle = display_handle;
	drm_data.flags = flags;
	drm_data.cmd = CMD_ALTER_OVL2;

	if (src_surf){
		memcpy(&(drm_data.src_surf), src_surf, sizeof(igd_surface_t));
	}
	if (src_rect) {
		memcpy(&(drm_data.src_rect), src_rect, sizeof(igd_rect_t));
	}
	if (dst_rect) {
		memcpy(&(drm_data.dst_rect), dst_rect, sizeof(igd_rect_t));
	}
	if (ovl_info) {
		memcpy(&(drm_data.ovl_info), ovl_info, sizeof(igd_ovl_info_t));
	}


	/* Call the ioctl: */
	if (drmCommandWriteRead(drm_fd, DRM_IGD_ALTER_OVL2,
				&drm_data, sizeof(emgd_drm_alter_ovl2_t))) {
		OS_PRINT("[EMGD] Communication With DRM Failed\n");
		return -1;
	}


	/* Return the "return value" returned in the DRM ioctl struct: */
	OS_TRACE_EXIT;
	return drm_data.rtn;
} /* emgd_drm_alter_ovl2() */


int emgd_drm_alter_ovl_osd(igd_display_h display_handle,
		int drm_fd,
		igd_surface_t       *src_surf,
		igd_rect_t          *src_rect,
		igd_rect_t          *dst_rect,
		igd_ovl_info_t      *ovl_info,
		unsigned int         flags)
{
	emgd_drm_alter_ovl2_t drm_data;

	/* Copy parameters into the DRM ioctl struct: */
	OS_TRACE_ENTER;

	memset(&drm_data, 0, sizeof(emgd_drm_alter_ovl2_t));
	drm_data.display_handle = display_handle;
	drm_data.flags = flags;

	if (src_surf){
		memcpy(&(drm_data.src_surf), src_surf, sizeof(igd_surface_t));
	}
	if (src_rect) {
		memcpy(&(drm_data.src_rect), src_rect, sizeof(igd_rect_t));
	}
	if (dst_rect) {
		memcpy(&(drm_data.dst_rect), dst_rect, sizeof(igd_rect_t));
	}
	if (ovl_info) {
		memcpy(&(drm_data.ovl_info), ovl_info, sizeof(igd_ovl_info_t));
	}

	drm_data.cmd = CMD_ALTER_OVL2_OSD;

	/* Call the ioctl: */
	if (drmCommandWriteRead(drm_fd, DRM_IGD_ALTER_OVL2,
				&drm_data, sizeof(emgd_drm_alter_ovl2_t))) {
		OS_PRINT("[EMGD] Communication With DRM Failed\n");
		return -1;
	}


	/* Return the "return value" returned in the DRM ioctl struct: */
	OS_TRACE_EXIT;
	return drm_data.rtn;
} /* emgd_drm_alter_ovl2_osd() */
#endif


