/*
 *-----------------------------------------------------------------------------
 * Filename: emgd_dri2.h
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
 *  XFree86 DRI .h
 *-----------------------------------------------------------------------------
 */

#ifndef __EMGD_DRI2_H__
#define __EMGD_DRI2_H__

#include "emgd.h"

/* MSC wraparound detection threshold */
#define WRAP_THRESH 10



typedef struct emgd_dri2_private {
	DRI2Buffer2Ptr buffer;
	unsigned long refcnt;
	PixmapPtr pixmap;
} emgd_dri2_private_t;


/*
 * Record of scheduled DRI2 swap request.
 */
typedef struct emgd_dri2_swap_record {
	emgd_priv_t *iptr;
	/* Client that submitted the swap and XID of drawable */
	ClientPtr client;
	XID drawable_id;

	/* Swap type: blit or flip */
	enum {
		SWAP_BLIT,
		SWAP_FLIP,
		SWAP_WAIT
	} type;

	/* CRTC number that we're using for vblank */
	int crtc;

	/* Last frame number seen */
	int framenum;

	/* Swap completion callback */
	DRI2SwapEventPtr callback;
	void* callback_data;

	/* Buffers to swap */
	DRI2BufferPtr front;
	DRI2BufferPtr back;

	/* Allows linking into the resource list for clients and drawable */
	struct LIST client_resource;
	struct LIST drawable_resource;
	void *(plane_pointer[2]);
} emgd_dri2_swap_record_t;


struct emgd_resource {
	XID id;
	RESTYPE type;
	struct LIST list;
};

void emgd_dri2_flip_complete(unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	emgd_dri2_swap_record_t *swapinfo);
void emgd_dri2_vblank_handler(unsigned int frame,
	unsigned int tv_sec,
	unsigned int tv_usec,
	emgd_dri2_swap_record_t *swapinfo);
void emgd_dri2_destroy_buffer(DrawablePtr drawable,
	DRI2Buffer2Ptr buffer);

int crtc_for_drawable(ScrnInfoPtr scrn, DrawablePtr drawable);

#endif /* __EMGD_DRI2_H__ */
