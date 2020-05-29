/*
 * -----------------------------------------------------------------------------
 *  Filename: emgd_output.c
 * -----------------------------------------------------------------------------
 *  Copyright (c) 2002-2013, Intel Corporation.
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
 *   Support for CTRC/KMS based interfaces.
 *-----------------------------------------------------------------------------
 */
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.output

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <xf86.h>
#include <X11/Xatom.h>
#include <X11/extensions/dpmsconst.h>
#include <xf86DDC.h>

#include "emgd.h"
#include "emgd_dri2.h"
#include "emgd_crtc.h" /* Use this for all typedefs */


/*
 * The list of output names is currently hard coded here. This needs to
 * be in sync with the DRM modules's list of output types.
 */
static const char *emgd_output_names[] =  {
	"None",
	"VGA",
	"DVII",
	"DVID",
	"DVIA",
	"Composite",
	"SVIDEO",
	"LVDS",
	"Component",
	"9PinDIN",
	"DisplayPort",
	"HDMIA",
	"HDMIB",
	"HDMID",
	"TV",
	"eDisplayPort",
};


/* subpixel converstion table.  Convert from KMS to XRandR. */
static int subpixel_conversion_table[7] = {
	0,
	SubPixelUnknown,
	SubPixelHorizontalRGB,
	SubPixelHorizontalBGR,
	SubPixelVerticalRGB,
	SubPixelVerticalBGR,
	SubPixelNone
};

/* Backlight support */
#define BACKLIGHT_NAME             "Backlight"
#define BACKLIGHT_DEPRECATED_NAME  "BACKLIGHT"
static Atom backlight_atom, backlight_deprecated_atom;


/*
 * Convert a kernel mode record into a X mode record.
 * Inputs:
 *    kernel mode record.
 *    pre-allocated memory for X mode record.
 */
static void emgd_convert_from_kmode(ScrnInfoPtr scrn,
		drmModeModeInfoPtr kmode,
		DisplayModePtr mode)
{
	memset(mode, 0, sizeof(DisplayModeRec));

	switch(kmode->type) {
		case DRM_MODE_TYPE_DRIVER:
			mode->type = M_T_DRIVER;
			break;
		case DRM_MODE_TYPE_PREFERRED:
			mode->type = M_T_PREFERRED;
			break;
	}

	mode->Clock      = kmode->clock;

	mode->HDisplay   = kmode->hdisplay;
	mode->HSyncStart = kmode->hsync_start;
	mode->HSyncEnd   = kmode->hsync_end;
	mode->HTotal     = kmode->htotal;
	mode->HSkew      = kmode->hskew;

	mode->VDisplay   = kmode->vdisplay;
	mode->VSyncStart = kmode->vsync_start;
	mode->VSyncEnd   = kmode->vsync_end;
	mode->VTotal     = kmode->vtotal;
	mode->VScan      = kmode->vscan;

	mode->Flags      = kmode->flags;
	mode->name       = strdup(kmode->name);

	mode->status     = MODE_OK;

	xf86SetModeCrtc (mode, scrn->adjustFlags);
}

/*
 * emgd_backlight_get
 *
 * Get the current backlight level.
 */
static int emgd_backlight_get(xf86OutputPtr output)
{
	OS_TRACE_ENTER;
	return -1;
	OS_TRACE_EXIT;
}

static Bool ignore_property(drmModePropertyPtr prop)
{
	OS_TRACE_ENTER;
	if (!prop) {
		return TRUE;
	}

	/* ignore blob type properites */
	if (prop->flags & DRM_MODE_PROP_BLOB) {
		return TRUE;
	}

	/*
	 * if it's not edid or dpms, ignore it
	 */
	if (!strcmp(prop->name, "EDID") ||
			!strcmp(prop->name, "DPMS")) {
		/*
		if ((strcmp(prop->name, "EDID") != 0) ||
		(strcmp(prop->name, "DPMS") != 0)) {
		*/
		return TRUE;
	}

	OS_TRACE_EXIT;
	return FALSE;
}

/****************************************************************************
 *  Output functions
 */

/*
 * emgd_output_create_resources
 *
 * Query all the properties from KMS and add them to the XRandR
 * interface.
 */
static void emgd_output_create_resources(xf86OutputPtr output)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	drmmode_t *drmmode = emgd_output->drmmode;
	drmModeConnectorPtr k_output = emgd_output->output;
	drmModePropertyPtr prop;
	int i;
	int j;
	int err;

	OS_TRACE_ENTER;
	emgd_output->props = calloc(k_output->count_props,
			sizeof(drmmode_prop_t));
	if (!emgd_output->props) {
		return;
	}

	/* Get all the output's properties and save them */
	emgd_output->num_props = 0;
	for (i = 0, j = 0; i < k_output->count_props; i++) {
		prop = drmModeGetProperty(drmmode->fd, k_output->props[i]);
		if (ignore_property(prop)) {
			drmModeFreeProperty(prop);
		} else {
			emgd_output->props[j].k_prop = prop;
			emgd_output->props[j].value = k_output->prop_values[i];
			emgd_output->num_props++;
			j++;
		}
	}

	/* For each property saved ..... */
	for (i = 0; i < emgd_output->num_props; i++) {
		drmmode_prop_t *p = &emgd_output->props[i];

		prop = p->k_prop;

		if (prop->flags & DRM_MODE_PROP_RANGE) {  /* Range properties */
			int32_t range[2];

			range[0] = prop->values[0];
			range[1] = prop->values[1];

			p->num_atoms = 1;
			if (!(p->atoms = calloc(p->num_atoms, sizeof(Atom)))) {
				continue;
			}

			p->atoms[0] = MakeAtom(prop->name, strlen(prop->name), TRUE);

			err = RRConfigureOutputProperty(output->randr_output, p->atoms[0],
					FALSE, TRUE,
					(prop->flags & DRM_MODE_PROP_IMMUTABLE) ? TRUE : FALSE,
					2, (INT32*)range);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
						"RRConfigureOutputProperty error, %d\n", err);
			}

			err = RRChangeOutputProperty(output->randr_output, p->atoms[0],
					XA_INTEGER, 32, PropModeReplace, 1, &p->value, FALSE, TRUE);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
						"RRChangeOutputProperty range %s error, %d\n",
						prop->name, err);
			}
		} else if (prop->flags & DRM_MODE_PROP_ENUM) { /* Enum properties */
			struct drm_mode_property_enum *e;

			p->num_atoms = prop->count_enums + 1;
			if (!(p->atoms = calloc(p->num_atoms, sizeof(Atom)))) {
				continue;
			}

			p->atoms[0] = MakeAtom(prop->name, strlen(prop->name), TRUE);

			for (j = 1; j < prop->count_enums; j++) {
				e = &prop->enums[j-1];
				p->atoms[j] = MakeAtom(e->name, strlen(e->name), TRUE);
			}

			err = RRConfigureOutputProperty(output->randr_output, p->atoms[0],
					FALSE, FALSE,
					(prop->flags & DRM_MODE_PROP_IMMUTABLE) ? TRUE : FALSE,
					p->num_atoms - 1, (INT32 *)&p->atoms[1]);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
						"RRConfigureOutputProperty error, %d\n", err);
			}

			for (j = 0; j < prop->count_enums; j++) {
				if (prop->enums[j].value == p->value) {
					break;
				}

				err = RRChangeOutputProperty(output->randr_output, p->atoms[0],
						XA_INTEGER, 32, PropModeReplace, 1, &p->atoms[j+1],
						FALSE, TRUE);
				if (err != 0) {
					xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
							"RRChangeOutputProperty enum error, %d\n", err);
				}
			}
		}
	}

	if (emgd_output->backlight_iface) {

		/*
		 * The backlight properties are unique. Code should be added
		 * here to handle setting these properties.
		 */
	}
	OS_TRACE_EXIT;
}


/*
 * emgd_output_set_property
 *
 */
static Bool emgd_output_set_property(xf86OutputPtr output,
		Atom property,
		RRPropertyValuePtr value)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	drmmode_t *drmmode = emgd_output->drmmode;
	drmmode_prop_t *p;
	int i;
	int j;
	long val;
	Atom atom;
	char *name;

	OS_TRACE_ENTER;
	/* Handle setting backlighting properties, they're special */
	if (property == backlight_atom || property == backlight_deprecated_atom) {
		OS_DEBUG(" - Handle backlight property.");
		if ((value->type != XA_INTEGER) ||
			(value->format != 32) ||
			(value->size != 1)) {
			OS_TRACE_EXIT;
			return FALSE;
		}

		val = *(long *)value->data;
		if ((val < 0) || (val > emgd_output->backlight_max)) {
			OS_TRACE_EXIT;
			return FALSE;
		}

		if (emgd_output->dpms_mode == DPMSModeOn) {
			/* Set backlight */
			/* emgd_backlight_set(output, val); */
		}

		emgd_output->backlight_active_level = val;
		OS_TRACE_EXIT;
		return TRUE;
	}


	/* Not a backlight property.  Look up the property in our list */
	for (i = 0; i < emgd_output->num_props; i++) {
		OS_DEBUG(" - Handle non-backlight property %d.", i);
		p = &emgd_output->props[i];

		if (p->atoms[0] == property) {
			if (p->k_prop->flags & DRM_MODE_PROP_RANGE) {
				if ((value->type != XA_INTEGER) ||
					(value->format != 32) ||
					(value->size != 1)) {
					OS_TRACE_EXIT;
					return FALSE;
				}
				val = *(long *)value->data;

				drmModeConnectorSetProperty(drmmode->fd, emgd_output->output_id,
						p->k_prop->prop_id, (unsigned long long)val);
				return TRUE;
			} else if (p->k_prop->flags & DRM_MODE_PROP_ENUM) {
				if ((value->type != XA_ATOM) ||
					(value->format != 32) ||
					(value->size != 1)) {
					OS_TRACE_EXIT;
					return TRUE;
				}

				memcpy(&atom, value->data, 4);
				name = (char *)NameForAtom(atom);

				/* Search the list for a matching name */
				for (j = 0; j < p->k_prop->count_enums; j++) {
					if (!strcmp(p->k_prop->enums[j].name, name)) {
						drmModeConnectorSetProperty(drmmode->fd,
								emgd_output->output_id,
								p->k_prop->prop_id,
								p->k_prop->enums[j].value);
						OS_TRACE_EXIT;
						return TRUE;
					}
				}

				OS_DEBUG("ENUM property without matching name");
				OS_TRACE_EXIT;
				return FALSE;
			}
		}
	}

	OS_DEBUG("No matching property, return true.");
	OS_TRACE_EXIT;
	return TRUE;
}


/*
 * emgd_output_get_property
 *
 * Get property from kernel and set in XRandR.
 */
static Bool emgd_output_get_property(xf86OutputPtr output, Atom property)
{
	/* emgd_output_priv_t *emgd_output = output->driver_private; */
	int err;
	long val;

	OS_TRACE_ENTER;
	/* Handle setting backlighting properties, they're special */
	if (property == backlight_atom || property == backlight_deprecated_atom) {

		if ((val = emgd_backlight_get(output)) < 0) {
			return FALSE;
		}

		err = RRChangeOutputProperty(output->randr_output, property,
				XA_INTEGER, 32, PropModeReplace, 1, &val, FALSE, TRUE);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
					"RRChangeOutputProperty error, %d\n", err);
			return FALSE;
		}

		OS_TRACE_EXIT;
		return TRUE;
	}

	/*
	 * Non-backlight properties would be handled here.
	 */
	OS_TRACE_EXIT;
	return TRUE;
}


/*
 * emgd_output_dpms
 *
 * Set the DPMS mode in the kernel to the requested value.
 *
 * Note: This is called from emgd_crtc.c, hence not static.
 */
void emgd_output_dpms(xf86OutputPtr output, int mode)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	drmmode_t *drmmode = emgd_output->drmmode;
	drmModeConnectorPtr k_output = emgd_output->output;
	int i;
	drmModePropertyPtr props;

	OS_TRACE_ENTER;
	/* Loop through the properties looking for the DPMS property */
	for (i = 0; i < k_output->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, k_output->props[i]);
		if (!props) {
			continue;
		}

		if (strcmp(props->name, "DPMS") == 0) {

			/* Set the property */
			drmModeConnectorSetProperty(drmmode->fd,
					emgd_output->output_id,
					props->prop_id,
					mode);

			/* Change the backlight state */
			if (mode == DPMSModeOn) {
				if (emgd_output->dpms_mode != DPMSModeOn) {
					/*
					emgd_backlight_set(output,
							emgd_output->backlight_active_level);
					*/
				}
			} else {
				if (emgd_output->dpms_mode == DPMSModeOn) {
					/*
					emgd_output->backlight_active_level =
						emgd_backlight_get(output);

					emgd_backlight_set(output, 0);
					*/
				}
			}

			emgd_output->dpms_mode = mode;
			drmModeFreeProperty(props);
			OS_TRACE_EXIT;
			return;
		}
		drmModeFreeProperty(props);
	}
	OS_TRACE_EXIT;
}


/*
 * emgd_output_detect
 *
 * Get the output's status from the kernel and return the XRandR equivelent.
 */
static xf86OutputStatus emgd_output_detect(xf86OutputPtr output)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	drmmode_t *drmmode = emgd_output->drmmode;
	xf86OutputStatus status;

	OS_TRACE_ENTER;
	drmModeFreeConnector(emgd_output->output);

	emgd_output->output =
		drmModeGetConnector(drmmode->fd, emgd_output->output_id);

	switch (emgd_output->output->connection) {
		case DRM_MODE_CONNECTED:
			status = XF86OutputStatusConnected;
			break;
		case DRM_MODE_DISCONNECTED:
			status = XF86OutputStatusDisconnected;
			break;
		case DRM_MODE_UNKNOWNCONNECTION:
		default:
			status = XF86OutputStatusUnknown;
			break;
	}
	OS_TRACE_EXIT;
	return status;
}


/*
 * emgd_output_mode_valid
 *
 * Check a mode to make sure the output can handle it.
 */
static Bool emgd_output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	/* emgd_output_priv_t *emgd_output = output->driver_private; */
	/* drmmode_t *drmmode = emgd_output->drmmode; */

	/*
	 * No checking is performed on the mode. It is assumed to be a valid
	 * mode.
	 */
	OS_TRACE_ENTER;
	OS_TRACE_EXIT;
	return MODE_OK;
}


/*
 * emgd_output_get_modes
 *
 * Get the mode list from the kernel and convert it to XRandR style.
 */
static DisplayModePtr emgd_output_get_modes(xf86OutputPtr output)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	drmmode_t *drmmode = emgd_output->drmmode;
	drmModeConnectorPtr k_output = emgd_output->output;
	drmModePropertyPtr props;
	DisplayModePtr mode_list, mode;
	int i;

	OS_TRACE_ENTER;
	/*
	 * If an EDID block is available for the output, make sure
	 * XRandR knows about it.
	 */
	for (i = 0; i < k_output->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, k_output->props[i]);

		if (!props) {
			continue;
		}

		if ((props->flags & DRM_MODE_PROP_BLOB) &&
				(strcmp(props->name, "EDID") == 0) ) {
			emgd_output->edid = drmModeGetPropertyBlob(drmmode->fd,
					k_output->prop_values[i]);
			continue;  /* pick the first and bail early */
		}
		drmModeFreeProperty(props);
	}

	/* If we found EDID block, make sure XRandR uses it */
	if (emgd_output->edid) {
		xf86OutputSetEDID(output,
				xf86InterpretEDID(output->scrn->scrnIndex,
					emgd_output->edid->data));
	} else {
		xf86OutputSetEDID(output,
				xf86InterpretEDID(output->scrn->scrnIndex, NULL));
	}

	/*
	 * Get all of the output's modes from the kernel and insert them
	 * into the XRandR's output mode list.
	 */
	mode_list = NULL;
	for (i = 0; i < k_output->count_modes; i++) {
		mode = xnfalloc(sizeof(DisplayModeRec));
		emgd_convert_from_kmode(output->scrn, &k_output->modes[i], mode);
		mode_list = xf86ModesAdd(mode_list, mode);
	}

	OS_TRACE_EXIT;
	return mode_list;
}


/*
 * emgd_output_destroy
 *
 * Clean up and free the output
 */
static void emgd_output_destroy(xf86OutputPtr output)
{
	emgd_output_priv_t *emgd_output = output->driver_private;
	int i;

	OS_TRACE_ENTER;
	/* Free any edid property blob */
	if (emgd_output->edid) {
		drmModeFreePropertyBlob(emgd_output->edid);
	}

	/* Free any properties */
	for (i = 0; i < emgd_output->num_props; i++) {
		drmModeFreeProperty(emgd_output->props[i].k_prop);
		free(emgd_output->props[i].atoms);
	}
	free(emgd_output->props);

	/* Free the output connector */
	drmModeFreeConnector(emgd_output->output);

	/* Free the private data */
	free(emgd_output);
	OS_TRACE_EXIT;
}




/****************************************************************************
 * Output function tables.
 *
 * These are the function tables that are given to the the X server as
 * part of the KMS/XRandR implementation.
 */

static const xf86OutputFuncsRec drmmode_output_funcs = {
	.create_resources = emgd_output_create_resources,
	.set_property = emgd_output_set_property,
	.get_property = emgd_output_get_property,
	.dpms = emgd_output_dpms,
	.detect = emgd_output_detect,
	.mode_valid = emgd_output_mode_valid,
	.get_modes = emgd_output_get_modes,
	.destroy = emgd_output_destroy
};


/****************************************************************************/


/*
 * drmmode_output_init
 *
 * Initialize the xf86 output.
 */
void drmmode_output_init(ScrnInfoPtr scrn, drmmode_t *drmmode, int num)
{
	xf86OutputPtr output;
	drmModeConnectorPtr k_output;
	drmModeEncoderPtr k_encoder;
	emgd_output_priv_t *priv;
	char name[32];

	/* Get output from KMS */
	k_output = drmModeGetConnector(drmmode->fd,
			drmmode->mode_res->connectors[num]);
	if (!k_output) {
		return;
	}

	/* Get default encoder from KMS */
	k_encoder = drmModeGetEncoder(drmmode->fd, k_output->encoders[0]);
	if (!k_encoder) {
		drmModeFreeConnector(k_output);
		return;
	}

	/* Create the Output object */
	snprintf(name, 32, "%s%d", emgd_output_names[k_output->connector_type],
			k_output->connector_type_id);

	output = xf86OutputCreate(scrn, &drmmode_output_funcs, name);
	if (!output) {
		drmModeFreeEncoder(k_encoder);
		drmModeFreeConnector(k_output);
		return;
	}

	priv = calloc(sizeof(emgd_output_priv_t), 1);
	if (!priv) {
		xf86OutputDestroy(output);
		drmModeFreeEncoder(k_encoder);
		drmModeFreeConnector(k_output);
		return;
	}

	priv->private_data = NULL;

	/*
	 * If any private data needs to be maintained. Then this is the place
	 * to allocate and initialize it
	 */

	priv->output_id = drmmode->mode_res->connectors[num];
	priv->output = k_output;
	priv->encoder = k_encoder;
	priv->drmmode = drmmode;

	output->mm_width = k_output->mmWidth;
	output->mm_height = k_output->mmHeight;
	output->subpixel_order = subpixel_conversion_table[k_output->subpixel];
	output->driver_private = priv;
	output->possible_crtcs = k_encoder->possible_crtcs;
	output->possible_clones = k_encoder->possible_clones;

	return;
}

