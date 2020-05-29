/*
* -----------------------------------------------------------------------------
*  Filename: emgd_sprite.c
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
*   EMGD Sprite DDX implementation.
*-----------------------------------------------------------------------------
*/
#define PER_MODULE_DEBUG
#define MODULE_NAME ial.overlay

#include <xf86Crtc.h>
#include <xvdix.h>

#include "emgd_sprite.h"
#include "intel_bufmgr.h"

#define U642VOID(x) ((void *)(unsigned long)(x))
#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

#define XV_IN_USE_ID 0xa5a5fff0

/* From emgd_video.c */
int get_src_dest_rect(
		ScrnInfoPtr pScrn,
		short src_x, short src_y,
		short drw_x, short drw_y,
		short src_w, short src_h,
		short drw_w, short drw_h,
		short width, short height,
		unsigned char calc_4_blend,
		unsigned int *src_render_ops,
		igd_rect_t *src_rect,
		igd_rect_t *dest_rect,
		RegionPtr clipBoxes);

/*
 * Util functions
 */

void* drmAllocCpy(void *array, int count, int entry_size)
{
	char *r;
	int i;

	if (!count || !array || !entry_size)
		return 0;

	if (!(r = drmMalloc(count*entry_size)))
		return 0;

	for (i = 0; i < count; i++)
		memcpy(r+(entry_size*i), array+(entry_size*i), entry_size);

	return r;
}

int intel_drm_add_fb2(int fd, uint32_t width, uint32_t height,
		uint32_t pixel_format, uint32_t bo_handles[4],
		uint32_t pitches[4], uint32_t offsets[4],
		uint32_t *buf_id)
{
	struct drm_mode_fb_cmd2 f;
	int ret;

	OS_TRACE_ENTER

	f.width = width;
	f.height = height;
	f.pixel_format = pixel_format;
	memcpy(f.handles, bo_handles, 4 * sizeof(bo_handles[0]));	
	memcpy(f.pitches, pitches, 4 * sizeof(pitches[0]));
	memcpy(f.offsets, offsets, 4 * sizeof(offsets[0]));

	/* Calling the private driver IOCTL instead of the DRM IOCTL */
	if ((ret = drmIoctl(fd, DRM_IOCTL_IGD_MODE_ADDFB2, &f))) {
		OS_ERROR("add_fb2 returned non-zero failed!\n");
		return ret;
	}
	*buf_id = f.fb_id;

	OS_TRACE_EXIT
	return 0;
}

drmModePlaneResPtr intel_drm_get_plane_resources(int fd)
{
	struct drm_mode_get_plane_res res, counts;
	drmModePlaneResPtr r = 0;

	OS_TRACE_ENTER

retry:
	memset(&res, 0, sizeof(struct drm_mode_get_plane_res));
	if (drmIoctl(fd, DRM_IOCTL_IGD_MODE_GETPLANERESOURCES, &res))
	{
		OS_TRACE_EXIT
		return 0;
	}

	counts = res;

	if (res.count_planes) {
		res.plane_id_ptr = VOID2U64(drmMalloc(res.count_planes *
					sizeof(uint32_t)));
		if (!res.plane_id_ptr)
			goto err_allocs;
	}

	if (drmIoctl(fd, DRM_IOCTL_IGD_MODE_GETPLANERESOURCES, &res))
		goto err_allocs;

	if (counts.count_planes < res.count_planes) {
		drmFree(U642VOID(res.plane_id_ptr));
		goto retry;
	}

	if (!(r = drmMalloc(sizeof(*r))))
		goto err_allocs;

	r->count_planes = res.count_planes;
	r->planes = drmAllocCpy(U642VOID(res.plane_id_ptr),
			res.count_planes, sizeof(uint32_t));
	if (res.count_planes && !r->planes) {
		drmFree(r->planes);
		r = 0;
	}

err_allocs:
	drmFree(U642VOID(res.plane_id_ptr));


	return r;
}

drmModePlanePtr intel_drm_get_plane(int fd, uint32_t plane_id)
{
	struct drm_mode_get_plane ovr, counts;
	drmModePlanePtr r = 0;

retry:
	memset(&ovr, 0, sizeof(struct drm_mode_get_plane));
	ovr.plane_id = plane_id;
	if (drmIoctl(fd, DRM_IOCTL_IGD_MODE_GETPLANE, &ovr))
		return 0;

	counts = ovr;

	if (ovr.count_format_types) {
		ovr.format_type_ptr = VOID2U64(drmMalloc(ovr.count_format_types *
					sizeof(uint32_t)));
		if (!ovr.format_type_ptr)
			goto err_allocs;
	}

	if (drmIoctl(fd, DRM_IOCTL_IGD_MODE_GETPLANE, &ovr))
		goto err_allocs;

	if (counts.count_format_types < ovr.count_format_types) {
		drmFree(U642VOID(ovr.format_type_ptr));
		goto retry;
	}

	if (!(r = drmMalloc(sizeof(*r))))
		goto err_allocs;

	r->count_formats = ovr.count_format_types;
	r->plane_id = ovr.plane_id;
	r->crtc_id = ovr.crtc_id;
	r->fb_id = ovr.fb_id;
	r->possible_crtcs = ovr.possible_crtcs;
	r->gamma_size = ovr.gamma_size;
	r->formats = drmAllocCpy(U642VOID(ovr.format_type_ptr),
			ovr.count_format_types, sizeof(uint32_t));
	if (ovr.count_format_types && !r->formats) {
		drmFree(r->formats);
		r = 0;
	}

err_allocs:
	drmFree(U642VOID(ovr.format_type_ptr));

	return r;
}

void intel_drm_free_plane(drmModePlanePtr ptr)
{
	if (!ptr)
		return;

	free(ptr->formats);
	free(ptr);
}

int intel_drm_set_plane(int fd, uint32_t plane_id, uint32_t crtc_id,
	uint32_t fb_id, uint32_t crtc_x, uint32_t crtc_y,
	uint32_t crtc_w, uint32_t crtc_h,
	uint32_t src_x, uint32_t src_y,
	uint32_t src_w, uint32_t src_h)
{
	struct drm_mode_set_plane s;
	s.plane_id = plane_id;
	s.crtc_id = crtc_id;
	s.fb_id = fb_id;
	s.crtc_x = crtc_x;
	s.crtc_y = crtc_y;
	s.crtc_w = crtc_w;
	s.crtc_h = crtc_h;
	s.src_x = src_x;
	s.src_y = src_y;
	s.src_w = src_w;
	s.src_h = src_h;

	return drmIoctl(fd, DRM_IOCTL_IGD_MODE_SETPLANE, &s);
}

Bool emgd_has_multiplane_drm(emgd_priv_t *iptr)
{
        struct drm_i915_getparam param;
        int overlay = 0;
        int ret;

	/* 
	 * Value 30 is a private definition in EMGD that allows 
	 * checking whether multiplane drm is available or not
	 */
        param.param = I915_PARAM_HAS_MULTIPLANE_DRM;
        param.value = &overlay;

        ret = drmCommandWriteRead(iptr->drm_fd, DRM_I915_GETPARAM, &param,
                        sizeof(param));
        if (ret) {
                OS_ERROR("DRM reports no overlay support!\n");
                return FALSE;
        }

        return overlay;
}

#define GET_PLANE_RESOURCES(drm_fd) iptr->private_multiplane_drm? 	\
		 intel_drm_get_plane_resources(drm_fd):drmModeGetPlaneResources(drm_fd)
#define GET_PLANE(drm_fd, ovl)      iptr->private_multiplane_drm?	\
		intel_drm_get_plane(drm_fd, ovl):drmModeGetPlane(drm_fd, ovl)
#define ADD_OVLFB(drm_fd, w, h, pf, bo, pitches, offsets, fb_id) iptr->private_multiplane_drm?       \
		intel_drm_add_fb2(drm_fd, w, h, pf, bo, pitches, offsets, fb_id):	\
			drmModeAddFB2(drm_fd, w, h, pf, bo, pitches, offsets, fb_id, 0)
#define SET_PLANE(drm_fd, ovl, crtc, fb, dx, dy, dw, dh, sx, sy, sw, sh)	\
		 iptr->private_multiplane_drm?	\
			intel_drm_set_plane(drm_fd, ovl, crtc, fb, dx, dy, dw, dh, sx, sy, sw, sh):	\
				drmModeSetPlane(drm_fd, ovl, crtc, fb, 0, \
					dx, dy, dw, dh, sx, sy, sw, sh)
#define FREE_PLANE(ovl)	iptr->private_multiplane_drm?intel_drm_free_plane(ovl):drmModeFreePlane(ovl)


/* emgd_sprite_grab
 * pass in the drawable, function will determine the crtc_id
 * and based on any pre-defined sprite assignment rules, will
 * grab an available sprite/ovl plane.
 * Returns the plane-id of that sprite/ovl plane
 */
emgd_sprite_t * emgd_sprite_grab(emgd_priv_t *iptr, DrawablePtr drawable,
			int crtc_id, uint32_t assignment, ClientPtr client, Bool grab_for_xv)
{
	emgd_sprite_t *emgdsprite = NULL;
	ScreenPtr pScreen;
	int i, avail_sprite=0;
	XvScreenPtr pxvs;
	XvAdaptorPtr adaptors;
	int cnt;
	DevPrivateKey XvScreenIndex;
	emgd_sprite_t *tmp_sprite = NULL;

	OS_TRACE_ENTER

	/* Go thru the entire list of known sprites */
	LIST_FOR_EACH_ENTRY(emgdsprite, &iptr->sprite_planes, link) {
		if(emgdsprite->possible_crtcs == crtc_id) {
			if(!emgdsprite->curr_state.locked) {
				if( (emgdsprite->curr_state.assignment ==
						SPRITE_PREASSIGNED_NONE) ||
					(emgdsprite->curr_state.assignment & assignment) ) {

					if(tmp_sprite == NULL){
						emgdsprite->curr_state.locked = 1;
						emgdsprite->curr_state.drawable = drawable;
						emgdsprite->crtc_id = crtc_id;
					
						tmp_sprite = emgdsprite;
					}else{
						++avail_sprite;
					}
				}
			}
		}
	}

	if (grab_for_xv == IGD_GRAB_FOR_XV_ON && !avail_sprite) {
 

		pScreen = drawable->pScreen;
	
		/* Get the Xv private screen index */
		XvScreenIndex = XvGetScreenKey();

		if(dixLookupPrivate(&pScreen->devPrivates, XvScreenIndex)){
			/* Get the Xv ScreenInfo pointer */
			pxvs = dixLookupPrivate(&pScreen->devPrivates, XvScreenIndex);

			/* Query for adaptor list */
			pxvs->ddQueryAdaptors(pScreen, &adaptors, &cnt);

			for(i = 0; i < cnt; i++){
				if(strcmp(adaptors[i].name, "EMGD Video Using Overlay") == 0){
					if(adaptors[i].pPorts->grab.client == NULL && 
							adaptors[i].pPorts->grab.id != XV_IN_USE_ID) {
						adaptors[i].pPorts->grab.client = client;
						adaptors[i].pPorts->grab.id = XV_IN_USE_ID;
					}
				}

			}
		}
	
	}

	
	OS_TRACE_EXIT
	return tmp_sprite;
}

/* emgd_sprite_plane_from_drawable
 * pass in the drawable and this function checks sprite link list to find
 * the sprite that was already allocated and locked by this drawable and
 * returns that plane
 */
emgd_sprite_t * emgd_sprite_plane_from_drawable(emgd_priv_t *iptr,
					DrawablePtr drawable, int crtc_id)
{
	emgd_sprite_t *emgdsprite = NULL;

	OS_TRACE_ENTER

	/* Go thru the entire list of known sprites */
	LIST_FOR_EACH_ENTRY(emgdsprite, &iptr->sprite_planes, link) {
		if((emgdsprite->curr_state.drawable == drawable) &&
			(emgdsprite->curr_state.locked) &&
			(emgdsprite->crtc_id == crtc_id)) {
			return emgdsprite;
		}
	}
	return NULL;
}


/* emgd_sprite_ungrab
 * pass in the sprite / ovl plane id and it frees it back to the DDX
 * sprite management link list
 */


void emgd_sprite_ungrab(ScreenPtr screen, emgd_priv_t *iptr, 
			emgd_sprite_t * oldplane, ClientPtr client, Bool grab_for_xv)
{
	emgd_sprite_t *emgdsprite = NULL;
	ScreenPtr pScreen;
	DevPrivateKey XvScreenIndex;
	XvScreenPtr pxvs;
	XvAdaptorPtr adaptors;
	int i, cnt;
	

	OS_TRACE_ENTER

	/* Go thru the entire list of known sprites */
	LIST_FOR_EACH_ENTRY(emgdsprite, &iptr->sprite_planes, link) {
		if(emgdsprite == oldplane) {
			memset(&emgdsprite->curr_state, 0, sizeof(emgdsprite->curr_state));
			break;
		}
	}

	if(grab_for_xv == 1){
		pScreen = screen;
		/* Get the Xv private screen index */
		XvScreenIndex = XvGetScreenKey();
	
		if(XvScreenIndex != NULL){
			if(dixLookupPrivate(&pScreen->devPrivates, XvScreenIndex)){
				/* Get the Xv ScreenInfo pointer */
				pxvs = dixLookupPrivate(&pScreen->devPrivates, XvScreenIndex);
			
				/* Query for adaptor list */
				pxvs->ddQueryAdaptors(pScreen, &adaptors, &cnt);
			
				/* Search list for our overlay adaptor */
				for(i=0; i < cnt; i++){
					/* This is bad, looking for it by name. But how else? */
					if(strcmp(adaptors[i].name, "EMGD Video Using Overlay") == 0){
						if((adaptors[i].pPorts->grab.client != NULL) &&
						   (client != NULL) &&
						   (adaptors[i].pPorts->grab.id == XV_IN_USE_ID)){
							adaptors[i].pPorts->grab.id = 0;
							adaptors[i].pPorts->grab.client = NULL;
						}	 
					}
				}
			}
		}	
	}



	return;
}

/* emgd_sprite_disable_core
 * call to turn of the sprite for this sprite/ovl plane */
int emgd_sprite_disable_core(emgd_priv_t *iptr, emgd_sprite_t * plane)
{
	OS_TRACE_ENTER;

	if(iptr && plane){
		if (SET_PLANE(iptr->drm_fd, plane->plane_id, plane->crtc_id, 0,
				0, 0, 0, 0, 0, 0, 0, 0)) {
			OS_ERROR("Failed to turn off plane id %d, crtc id %d", plane->plane_id, plane->crtc_id);
		}
	}

	OS_TRACE_EXIT;
	return TRUE;
}

/* emgd_sprite_update_attr
 * pass in the type of attribute and the value for it.
 * this might probably be some kind of structure like the one
 * used in DRM - igd_ovl_info perhaps
 */
int emgd_sprite_update_attr(emgd_priv_t *iptr,
		struct drm_intel_sprite_colorkey * ckey,
		struct drm_intel_sprite_colorcorrect * ccorrect,
		struct drm_intel_sprite_pipeblend *pipeblend,
		uint32_t flags)
{
	OS_TRACE_ENTER;

	if(ckey){
		OS_ERROR("DDX Sprite- color key NOT-DONE! Kernel user_config is workaround!\n");
	}

	if(ccorrect){
		OS_ERROR("DDX Sprite- color correct NOT-DONE! Kernel user_config is workaround!\n");
	}

	if(pipeblend){
		OS_ERROR("DDX Sprite- pipe_blend NOT-DONE! Kernel user_config is workaround!\n");
	}

	OS_TRACE_EXIT;

	return FALSE;
}

static Bool emgd_sprite_rect_changed(uint32_t x,
		uint32_t y,
		uint32_t w,
		uint32_t h,
		igd_rect_t new_rect)
{
	uint32_t new_w = new_rect.x2 - new_rect.x1;
	uint32_t new_h = new_rect.y2 - new_rect.y1;
	if (new_rect.x1 != x ||
		new_rect.y1 != y ||
		new_w != w ||
		new_h != h) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int emgd_sprite_create_flip_bo(DrawablePtr drawable,
	DRI2BufferPtr buffer, 
	emgd_sprite_buffer_t *flip_buffer)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	emgd_priv_t *iptr = EMGDPTR(scrninfo);
	emgd_dri2_private_t* buf_priv = buffer->driverPrivate;
	struct intel_pixmap *pixmap_priv = NULL;
	drm_intel_bo *bo;
	unsigned int tiling, name, size;
	int stride;

	pixmap_priv = intel_get_pixmap_private(buf_priv->pixmap);
	if ((NULL == pixmap_priv) || !pixmap_priv->bo) {
		OS_ERROR("No EMGD private data for buffer pixmap");
		return FALSE;
	}

	tiling = pixmap_priv->tiling;
	stride = pixmap_priv->stride;
	size = pixmap_priv->bo->size;
	
	bo = drm_intel_bo_alloc_for_render(iptr->bufmgr,
				"pixmap", size, 0);
	if (NULL == bo) {
		OS_ERROR("Failed to allocate bo");
		return FALSE;
	}

	if (tiling !=  I915_TILING_NONE) {
		drm_intel_bo_set_tiling(bo, &tiling, stride);
	}

	/* I think we need this to make sure the bo will not be kept in-flight for re-use */
	drm_intel_bo_disable_reuse(bo); 
	drm_intel_bo_flink(bo, &name);

	flip_buffer->bo = bo;
	flip_buffer->name = name;

	return TRUE;
}

void emgd_sprite_destroy_flip_bo(emgd_sprite_buffer_t *flip_buffer)
{
	drm_intel_bo_unreference(flip_buffer->bo);
	flip_buffer->name = 0;
}	

int emgd_sprite_check_flip_bo(DrawablePtr drawable, 
	DRI2BufferPtr buffer, 
	emgd_sprite_buffer_t *flip_buffer)
{
	emgd_dri2_private_t* buf_priv = buffer->driverPrivate;
	struct intel_pixmap *pixmap_priv = NULL;

	pixmap_priv = intel_get_pixmap_private(buf_priv->pixmap);
	if ((NULL == pixmap_priv) || !pixmap_priv->bo) {
		OS_ERROR("No EMGD private data for buffer pixmap");
		return FALSE;
	}

	/* Check against the size for now */
	if ((unsigned long) flip_buffer->bo->size != (unsigned long) pixmap_priv->bo->size) {
		return FALSE;
	}

	return TRUE;
}

void emgd_sprite_exchange_flip_bo(DrawablePtr drawable,
	DRI2BufferPtr buffer,
	emgd_sprite_buffer_t *flip_buffer1,
	emgd_sprite_buffer_t *flip_buffer2)
{
	emgd_dri2_private_t* buf_priv = buffer->driverPrivate;
	struct intel_pixmap *pixmap_priv = NULL;
	drm_intel_bo *old_back;
	unsigned int old_back_name;

	pixmap_priv = intel_get_pixmap_private(buf_priv->pixmap);
	
	old_back = pixmap_priv->bo;
	old_back_name =  buffer->name;
	
	/* First set the buffer pixmap with flip_bufer1 */
	drm_intel_bo_reference(pixmap_priv->bo);
	intel_set_pixmap_bo(buf_priv->pixmap, flip_buffer1->bo);
	buffer->name = flip_buffer1->name;
	/* Unreference this GEM bo since it is no longer being scanout? */
	drm_intel_bo_unreference(flip_buffer1->bo);

	flip_buffer1->bo = flip_buffer2->bo;
	flip_buffer1->name = flip_buffer2->name;

	flip_buffer2->bo = old_back;
	flip_buffer2->name = old_back_name;
}

int emgd_sprite_flip_core(
		emgd_sprite_t *(planes[2]),
		DrawablePtr drawable, uint32_t fb_id,
		igd_rect_t src_rect, igd_rect_t dst_rect, 
		uint32_t pixel_format
		)
{
	ScreenPtr screen;
	ScrnInfoPtr scrninfo;
	xf86CrtcConfigPtr xf86_config;
	emgd_priv_t *iptr;
	RegionRec region;
	WindowPtr wp;
	BoxPtr pBox;
	int num_dirty_rect;
	uint32_t old_fb_id = 0;
	uint32_t src_w, src_h;
	uint32_t src_x, src_y;
	uint32_t dst_w, dst_h;
	uint32_t dst_x, dst_y;
	struct drm_intel_sprite_colorkey colorkey;
	unsigned long color_check = 0;
	int ret, i;
	int num_planes, mode, crtcnum;

	OS_TRACE_ENTER

	if (NULL == drawable) {
		OS_ERROR("Bad drawable detected");
		return FALSE;
	}

	if (NULL == drawable->pScreen) {
		OS_ERROR("NULL screen detected");
		return FALSE;
	}

	if (NULL == planes[0]) {
		OS_ERROR("Invalid planes given");
		return FALSE;
	}

	emgd_sprite_check_mode(drawable, &mode, &num_planes);

	if (NULL == planes[1]) {
		num_planes = 1;
	} else {
		num_planes = 2;
	}

	screen = drawable->pScreen;
	scrninfo = xf86Screens[screen->myNum];
	iptr = EMGDPTR(scrninfo);
	xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);

	/*
	 * Call get_src_dest_rect to calculate the src_rect
	 * and dst_rect
	 */
	wp = (WindowPtr)drawable;

	/* see if the caller is asking us to check the src and dest rects and
	 * correct them according to virtual coordinates vs screen coordinates.
	 * And of course get the bounding clip rect of the dest drawable
	 * (and the get_src_dest_rect could also check for things like rotation
	 * and render-scaling).
	 */
	REGION_INIT(screen, &region, REGION_RECTS(&wp->clipList),
			REGION_NUM_RECTS(&wp->clipList) + 0);
	REGION_COPY(screen, &region, &wp->clipList);
	pBox = REGION_RECTS(&region);
	num_dirty_rect = REGION_NUM_RECTS(&region);

	src_x = src_rect.x1 << 16;
	src_y = src_rect.y1 << 16;
	src_w = src_rect.x2 - src_rect.x1;
	src_h = src_rect.y2 - src_rect.y1;

	/* Adjust the destination X and Y in extended mode */
	if (mode == SPRITE_MODE_EXTENDED) {
		crtcnum = crtc_for_drawable(scrninfo, drawable);
		if(crtcnum < 0){
			OS_ERROR("invalid crtcnum detected");
			return FALSE;
		}
		dst_x = dst_rect.x1 - xf86_config->crtc[crtcnum]->x;
		dst_y = dst_rect.y1 - xf86_config->crtc[crtcnum]->y;
	} else {
		dst_x = dst_rect.x1;
		dst_y = dst_rect.y1;
	}
	dst_w = dst_rect.x2 - dst_rect.x1;
	dst_h = dst_rect.y2 - dst_rect.y1;

	old_fb_id = planes[0]->curr_state.fb_id;

	for (i = 0; i < num_planes; i++) {
		/* Keep this for now as update attribute is not available yet */
		/* Set the destination colorkey */
		colorkey.plane_id = planes[i]->plane_id;
		colorkey.min_value = iptr->cfg.video_key;
		colorkey.flags = I915_SET_COLORKEY_DESTINATION;

		ret = drmIoctl(iptr->drm_fd, DRM_IOCTL_I915_SET_SPRITE_COLORKEY, &colorkey);
		if (ret) {
			OS_ERROR("Failed to set sprite colorkey");
			REGION_UNINIT(scrninfo, &region);
			return FALSE;
		}

		OS_DEBUG("src.x1 = %d", src_rect.x1);
	        OS_DEBUG("src.x2 = %d", src_rect.x2);
	        OS_DEBUG("src.y1 = %d", src_rect.y1);
	        OS_DEBUG("src.y2 = %d", src_rect.y2);
	        OS_DEBUG("dst.x1 = %d", dst_rect.x1);
	        OS_DEBUG("dst.x2 = %d", dst_rect.x2);
	        OS_DEBUG("dst.y1 = %d", dst_rect.y1);
	        OS_DEBUG("dst.y2 = %d", dst_rect.y2);
	        OS_DEBUG("region.x1 = %d", region.extents.x1);
	        OS_DEBUG("region.x2 = %d", region.extents.x2);
	        OS_DEBUG("region.y1 = %d", region.extents.y1);
	        OS_DEBUG("region.y2 = %d", region.extents.y2);

		planes[i]->curr_state.fb_id = fb_id;
		planes[i]->curr_state.old_fb_id = old_fb_id;

		/* Check if source position has changed */
		if (emgd_sprite_rect_changed(planes[i]->curr_state.src_x,
			planes[i]->curr_state.src_y,
			planes[i]->curr_state.src_w,
			planes[i]->curr_state.src_h,
			src_rect)) {
			/* Update source position */
			planes[i]->curr_state.src_x = src_rect.x1;
			planes[i]->curr_state.src_y = src_rect.y1;
			planes[i]->curr_state.src_w = src_rect.x2 - src_rect.x1;
			planes[i]->curr_state.src_h = src_rect.y2 - src_rect.y1;
		}

		/* Check if destination position has changed */
		if (emgd_sprite_rect_changed(planes[i]->curr_state.crtc_x,
			planes[i]->curr_state.crtc_y,
			planes[i]->curr_state.crtc_w,
			planes[i]->curr_state.crtc_h,
			dst_rect)) {
			/* Update destination position */
			planes[i]->curr_state.crtc_x = dst_rect.x1;
			planes[i]->curr_state.crtc_y = dst_rect.y1;
			planes[i]->curr_state.crtc_w = dst_rect.x2 - dst_rect.x1;
			planes[i]->curr_state.crtc_h = dst_rect.y2 - dst_rect.y1;
		}

		/*
		 * I seem to have plane_id, crtc_id, fb_id, and all the crtc coords
		 * to call drmModeSetPlane now
		 */
		ret = SET_PLANE(iptr->drm_fd, planes[i]->plane_id, planes[i]->crtc_id,
			planes[i]->curr_state.fb_id,
			dst_x, dst_y, dst_w, dst_h,
			src_x, src_y,
			(src_w<<16), (src_h<<16) ); /*srcx/srcy is 16.16 fixed pt*/
		if (ret) {
			/* FIXME: I think we need to make sure the sprite is turned off? */
			OS_ERROR("Failed to set the sprite plane");
			if (planes[i]->curr_state.enabled) {
				planes[i]->curr_state.enabled = 0;
			}
			REGION_UNINIT(scrninfo, &region);
			return FALSE;
		}

		if (!planes[i]->curr_state.enabled) {
			planes[i]->curr_state.enabled = 1;
		}
	}

	/*
	 * Paint the colorkey and return
	 */
	if (!REGION_EQUAL(scrninfo->pScreen, &planes[0]->curr_state.region, &region)) {
		REGION_COPY(scrninfo->pScreen, &planes[0]->curr_state.region, &region);
		xf86XVFillKeyHelperDrawable(drawable, iptr->cfg.video_key,
			&region);
	} else {
		if (drm_intel_gem_bo_map_gtt(iptr->front_buffer)) {
			OS_DEBUG("Failed to map front_buffer, no color check");
			REGION_UNINIT(scrninfo, &region);
			return TRUE;
		}

		for (i = 0; i < num_dirty_rect; ++i) {
			/* Check frame buffer pixel data. If it is not the same as
			 * the color key, draw the color key again.
			 */
			color_check = (unsigned long) *(((uint32_t *)iptr->front_buffer->virtual) + 
					((pBox[i].y1) * iptr->fb_pitch / 4) + (pBox[i].x1));
			if (color_check != iptr->cfg.video_key){
				xf86XVFillKeyHelperDrawable(drawable, iptr->cfg.video_key,
					&region);
			}
		}
		drm_intel_gem_bo_unmap_gtt(iptr->front_buffer);
	}

	REGION_UNINIT(scrninfo, &region);
	return TRUE;
}

Bool emgd_dri2_sprite_flip_wrapper(ClientPtr client,
		DrawablePtr drawable,
		DRI2BufferPtr src_buf,
		emgd_sprite_t *(planes[2]))
{
	ScreenPtr screen;
	ScrnInfoPtr scrninfo;
	emgd_priv_t *iptr;
	emgd_dri2_private_t* src_priv;
	struct _emgd_pixmap *back_pixmap;
	WindowPtr wp;
	RegionRec region;
	uint32_t src_w, src_h;
	uint32_t src_x, src_y;
	uint32_t drw_w, drw_h;
	uint32_t drw_x, drw_y;
	igd_rect_t src_rect, dst_rect;
	uint32_t pixel_format;
	uint32_t pitches[4] = { 0 };
	uint32_t bo_handles[4] = { 0 };
	uint32_t offsets[4] = { 0 };
	uint32_t fb_id;
	struct drm_intel_sprite_pipeblend pipeblend;
	int ret;

	if (NULL == client) {
		OS_ERROR("NULL client detected");
		return FALSE;
	}

	if (NULL == src_buf) {
		OS_ERROR("NULL src_buf detected");
		return FALSE;
	}

	if (NULL == drawable) {
		OS_ERROR("NULL drawable detected");
		return FALSE;
	}

	if (NULL == drawable->pScreen) {
		OS_ERROR("NULL screen detected");
		return FALSE;
	}

	src_priv = src_buf->driverPrivate;
	screen = drawable->pScreen;
	scrninfo = xf86Screens[screen->myNum];
	iptr = EMGDPTR(scrninfo);

	wp = (WindowPtr)drawable;

	REGION_INIT(screen, &region, REGION_RECTS(&wp->clipList),
		REGION_NUM_RECTS(&wp->clipList) + 0);
	REGION_COPY(screen, &region, &wp->clipList);

	/* Lets request sprite configuration values...*/
	memset(&pipeblend, 0, sizeof(pipeblend));
	pipeblend.plane_id = planes[0]->plane_id;
	pipeblend.crtc_id = planes[0]->crtc_id;
	ret = drmIoctl(iptr->drm_fd, DRM_IOCTL_IGD_GET_PIPEBLEND, &pipeblend);
	if (ret) {
		OS_ERROR("Failed to get the sprite pipe blend info");
		REGION_UNINIT(scrninfo, &region);
		return FALSE;
	}

	if (pipeblend.fb_blend_ovl != 1) {
		if (!(region.extents.x2 - region.extents.x1) || !(region.extents.y2 - region.extents.y1)) {
			/* No region to flip, this happens when a window is minimized */
			REGION_COPY(scrninfo->pScreen, &planes[0]->curr_state.region, &region);

			/* In case where the sprite plane is in front of display plane , we need to 
			 * disable the sprite plane so that it is not visible
			 */
			if(planes[0]->curr_state.enabled) {
				emgd_sprite_disable_core(iptr, planes[0]);
				planes[0]->curr_state.enabled = 0;
				if (planes[1] != NULL) {
					emgd_sprite_disable_core(iptr, planes[1]);
					planes[1]->curr_state.enabled = 0;
				}
			}

		REGION_UNINIT(scrninfo, &region);
		return TRUE;
		}
	}

	/* Calculate the source and dest rects */
	src_w = src_priv->pixmap->drawable.width;
	src_h = src_priv->pixmap->drawable.height;
	src_x = src_priv->pixmap->drawable.x;
	src_y = src_priv->pixmap->drawable.y;

	drw_w = src_w; /* Use src_w since we are not supporting hardware scaling */
	drw_h = src_h; /* Use src_h since we are not supporting hardware scaling */
	drw_x = drawable->x;
	drw_y = drawable->y;

    /* Get drm gem buffer object for back buffer we're flipping from*/
    back_pixmap = intel_get_pixmap_private(src_priv->pixmap);
    if (!back_pixmap || !back_pixmap->bo) {
        OS_ERROR("No EMGD private data for back buffer pixmap");
        REGION_UNINIT(scrninfo, &region);
        return FALSE;
    }

    bo_handles[0] = back_pixmap->bo->handle;
    bo_handles[1] = bo_handles[2] = bo_handles[3] = 0;

    /* Determine the pitch */
    pitches[0] = src_buf->pitch;
    pitches[1] = pitches[2] = pitches[3] = 0;

    /* Determine the pixel format */
    if (src_buf->cpp == 2) {
        pixel_format = DRM_FORMAT_RGB565;
    } else if (src_buf->cpp == 4) {
        pixel_format = DRM_FORMAT_XRGB8888;
    } else {
        OS_ERROR("Unsupported pixel format, source cpp = %d", src_buf->cpp);
        REGION_UNINIT(scrninfo, &region);
        return FALSE;
    }

    if (ADD_OVLFB(iptr->drm_fd, src_w, src_h,
        pixel_format, bo_handles,
        pitches, offsets, &fb_id)) {
        OS_ERROR("Failed to add fb2");
        REGION_UNINIT(scrninfo, &region);
        return FALSE;
    }
    
	if (pipeblend.fb_blend_ovl != 1) {
		get_src_dest_rect(scrninfo,
			src_x, src_y, drw_x, drw_y,
			src_w, src_h, drw_w, drw_h,
			src_w, src_h,
			0, NULL, &src_rect, &dst_rect, &region);
	} else {
		/* If FbBlendOvl is ON, we always flip the whole 
		 * surface */
                src_rect.x1 = src_x;
                src_rect.x2 = src_x + src_w;
                src_rect.y1 = src_y;
                src_rect.y2 = src_y + src_h;

                dst_rect.x1 = drw_x;
                dst_rect.y1 = drw_y;
                dst_rect.x2 = drw_x + drw_w;
                dst_rect.y2 = drw_y + drw_h;
	}		

	ret = emgd_sprite_flip_core(
			planes,
			drawable, fb_id,
			src_rect, dst_rect,
			pixel_format);
	if(!ret){
		OS_ERROR("emgd_sprite_flip_core failed to succeed\n");
		REGION_UNINIT(scrninfo, &region);
		return FALSE;
	}

	REGION_UNINIT(scrninfo, &region);
	return TRUE;
}

void emgd_sprite_check_mode(DrawablePtr drawable, int * mode, int * num_planes) 
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrninfo = xf86Screens[screen->myNum];
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);

	/* Check the crtcs number and also whether their are enabled */
	if (xf86_config->num_crtc == 2 &&
		xf86_config->crtc[0]->enabled &&
		xf86_config->crtc[1]->enabled) {
		if ((xf86_config->crtc[0]->x == xf86_config->crtc[1]->x) &&
			(xf86_config->crtc[0]->y == xf86_config->crtc[1]->y)) {
			/* Clone mode, is there better way to identify this? */
			*num_planes = 2;	
			*mode = SPRITE_MODE_CLONE;
		} else { 
			/* Extended mode */
			*num_planes = 1;
			*mode = SPRITE_MODE_EXTENDED;
		}
	} else {
		/* Single mode */
		*num_planes = 1;
		*mode = SPRITE_MODE_SINGLE;
	}
}

Bool emgd_sprite_get_plane(emgd_priv_t *iptr, DrawablePtr drawable, int assignment,
	emgd_sprite_t *(planes[2]), ClientPtr client, Bool grab_for_xv) 
{
	ScreenPtr screen;
	ScrnInfoPtr scrninfo;
	xf86CrtcConfigPtr xf86_config;
	emgd_crtc_priv_t *emgd_crtc;
	int mode, num_planes;
	int crtcnum;
	int i;

	if (NULL == iptr) {
		OS_ERROR("NULL drawable detected");
		return FALSE;
	}

	if (NULL == drawable) {
		OS_ERROR("NULL drawable detected");
		return FALSE;
	}

	if(NULL == drawable->pScreen) {
		OS_ERROR("NULL screen pointer detected");
		return FALSE;
	}

	if (NULL == planes) {
		OS_ERROR("NULL planes pointer detected");
		return FALSE;
	}

	screen = drawable->pScreen;
	scrninfo = xf86Screens[screen->myNum];
	xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);

	emgd_sprite_check_mode(drawable, &mode, &num_planes);	

	if (num_planes == 1) {
		/* Set NULL to second element in planes array */
		planes[1] = NULL;
	}

	crtcnum = crtc_for_drawable(scrninfo, drawable);
	if (crtcnum < 0) {
		OS_ERROR("invalid crtcnum detected");
		return FALSE;
	}

	for (i = 0; i < num_planes; i++) {
		if ((mode == SPRITE_MODE_CLONE) && (i == 1)) {
			/* For clone mode, we need to grab a plane tied to 
			 * another crtc */
			crtcnum = crtcnum ? 0:1;
		}

		emgd_crtc = xf86_config->crtc[crtcnum]->driver_private;
		
		/* Check if drawable has already grabbed plane(s) previously */
		planes[i] = emgd_sprite_plane_from_drawable(iptr, drawable, 
				emgd_crtc->crtc->crtc_id);
		if (NULL == planes[i]) {
			/* No plane has been previously grabbed yet */
			planes[i] = emgd_sprite_grab(iptr, drawable,
					emgd_crtc->crtc->crtc_id, assignment, client, grab_for_xv);
			if (NULL == planes[i]) {
				OS_ERROR("No sprite plane available");
				return FALSE;
			}
		}
	}
	
	return TRUE;
}

Bool emgd_sprite_allocate_tripple_buffer(DrawablePtr drawable, DRI2BufferPtr dri_buffer,
		emgd_sprite_t *plane) 
{
	emgd_sprite_buffer_t *flip_buff = NULL;
	int ret;

	/* Allocate the flip_buffers for tripple buffering if it's not done yet 
	 * please note flip_buufers are only attached to plane[0] for SPRITE_CLONE_MODE */
	if (NULL == plane->flip_buffer1) {
		flip_buff = calloc(1, sizeof(*flip_buff));
		if (NULL == flip_buff) {
			OS_ERROR("Failed to allocate EMGD sprite flip_buffer1");
			return FALSE;
		}

		plane->flip_buffer1 = flip_buff;

		ret = emgd_sprite_create_flip_bo(drawable, dri_buffer, plane->flip_buffer1);
		if (!ret) {
			free(plane->flip_buffer1);
			plane->flip_buffer1 = NULL;
			OS_ERROR("Failed to create bo for flip_buffer1");
			return FALSE;
		}
	} else {
		/* Flip buffer object has previously been allocated,
		 * check if we can re-use this bo */
		ret = emgd_sprite_check_flip_bo(drawable, dri_buffer, plane->flip_buffer1);
		if (!ret) {
			/* Flip buffer object cannot be reused,
			 * let's destroy it and allocate one */
			emgd_sprite_destroy_flip_bo(plane->flip_buffer1);
			emgd_sprite_create_flip_bo(drawable, dri_buffer, plane->flip_buffer1);
		}
	}

	if (NULL == plane->flip_buffer2) {
		flip_buff = calloc(1, sizeof(*flip_buff));
		if (NULL == flip_buff) {
			OS_ERROR("Failed to allocate EMGD sprite flip_buffer2");
			return FALSE;
		}       
        
		plane->flip_buffer2 = flip_buff;
        
		ret = emgd_sprite_create_flip_bo(drawable, dri_buffer, plane->flip_buffer2);
		if (!ret) {
			free(plane->flip_buffer2);
				plane->flip_buffer2 = NULL;
				OS_ERROR("Failed to create bo for flip_buffer2");
				return FALSE;
		}
	} else {
		/* Flip buffer object has previously been allocated,
		* check if we can re-use this bo */
		ret = emgd_sprite_check_flip_bo(drawable, dri_buffer, plane->flip_buffer2);
		if (!ret) {
			/* Flip buffer object cannot be reused,
			* let's destroy it and allocate one */
			emgd_sprite_destroy_flip_bo(plane->flip_buffer2);
			emgd_sprite_create_flip_bo(drawable, dri_buffer, plane->flip_buffer2);
		}       
	}       

	return TRUE;
}	
		
	
int emgd_sprite_plane_init(emgd_priv_t *iptr, drmModePlane *ovr, int i) {
	emgd_sprite_t *plane;

	plane = calloc(1, sizeof *plane);

	if (!plane) {
		OS_ERROR("Failed to allocate memory for sprite structure");
		return -1;
	}

	plane->possible_crtcs = ovr->possible_crtcs;
	plane->plane_id = ovr->plane_id;
	plane->curr_state.assignment = iptr->cfg.sprite_assignment[i];

	LIST_ADD(&plane->link, &iptr->sprite_planes);

	return 0;
}

int emgd_sprite_init(emgd_priv_t *iptr)
{
	drmModePlaneRes *plane_resources;
	drmModePlane *ovr;
	int i;
	int ret;
	unsigned int plane_count;

	OS_TRACE_ENTER;

	plane_resources = GET_PLANE_RESOURCES(iptr->drm_fd);
	if (!plane_resources) {
		OS_ERROR("Failed to get plane resources");
		return -1;
	}

	plane_count = plane_resources->count_planes;

	/* Start the iteration from plane_count - 1 so that the linked list's 
	 * populated in the order of Sprite A -> B -> C -> D.
	 */
	for (i = plane_count - 1; i >= 0; i--) {
		OS_DEBUG("count_planes=%d", plane_resources->count_planes);
		ovr = GET_PLANE(iptr->drm_fd,
				plane_resources->planes[i]);
		if (!ovr) {
			continue;
		}

		/* We have received a plane that has a possible crtc_id
		 * Now let's create a sprite structure and add it to a
		 * linked list that tracks the plane resources.
		 */
		ret = emgd_sprite_plane_init(iptr, ovr, i);
		if (ret) {
			OS_ERROR("Failed to initialize sprite plane");
			return -1;
		}

		FREE_PLANE(ovr);
	}

	free(plane_resources);

	OS_TRACE_EXIT;
	return 0;
}

int emgd_sprite_shutdown(emgd_priv_t *iptr) {
	emgd_sprite_t *plane;

	OS_TRACE_ENTER;

	while (!LIST_IS_EMPTY(&iptr->sprite_planes)) {
		plane = LIST_FIRST_ENTRY(&iptr->sprite_planes,
					emgd_sprite_t, link);
		if (plane->curr_state.enabled) {
			/* Turn off the plane if it is still ON */
			emgd_sprite_disable_core(iptr, plane);
			plane->curr_state.enabled = 0;
		}

		/* Remove sprite from the list */
		LIST_DEL(&plane->link);
		free(plane);
	}

	OS_TRACE_EXIT;
	return 0;
}

void emgd_sprite_leave_vt(emgd_priv_t *iptr){
	emgd_sprite_t *plane = NULL;

	OS_TRACE_ENTER;
        LIST_FOR_EACH_ENTRY(plane, &iptr->sprite_planes, link){
		if(NULL != plane){
			if(plane->curr_state.locked){
				emgd_sprite_disable_core(iptr, plane);
				emgd_sprite_ungrab(NULL, iptr, plane, NULL, IGD_GRAB_FOR_XV_OFF);
			}
		}
        }

	OS_TRACE_EXIT;
}

/*
 * emgd_sprite_check_display_rotation_status function is using to check what is current display 
 * rotation status on the display in single, clone or extended mode for sprite usage whether
 * sprite should be fallback to blend/blit.
 */
int emgd_sprite_check_display_rotation_status(DrawablePtr drawable, emgd_priv_t *iptr, int crtcnum, int mode) {
	ScreenPtr screen;
	ScrnInfoPtr scrninfo;
	xf86CrtcConfigPtr xf86_config;
	xf86CrtcPtr crtc;
	int rotation_status = RR_Rotate_0;
	int i;
	OS_TRACE_ENTER;

	screen = drawable->pScreen;
	scrninfo = xf86Screens[screen->myNum];

	xf86_config = XF86_CRTC_CONFIG_PTR(scrninfo);

	if(xf86_config != NULL) {
		if((mode == SPRITE_MODE_SINGLE) || (mode == SPRITE_MODE_EXTENDED)) {
			crtc = xf86_config->crtc[crtcnum];
			rotation_status = crtc->rotation;
		} else if (mode == SPRITE_MODE_CLONE) {
			/* 
 			 * For clone mode, we have to read every single crtc display 
 			 * because the crtc_for_drawable only return 0 ever you in
 			 * first or secondary display. Our driver consider it as a
 			 * one frame buffer for clone mode.	
  			 */
			for(i=0; i < xf86_config->num_crtc; i++) {
				crtc = xf86_config->crtc[i];
				if(xf86_config->crtc[i] != NULL){
					if(crtc->rotation != RR_Rotate_0){
						rotation_status = crtc->rotation;
					}
				} 
            }
		} 
	}
	OS_TRACE_EXIT;
	return rotation_status;
}

