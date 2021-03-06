/* -*- c-basic-offset: 4 -*- */
/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "xf86.h"
#include "emgd.h"
#include "emgd_uxa.h"
#include "intel_batchbuffer.h"
#include "i830_reg.h"
#include "i915_drm.h"
#include "i965_reg.h"

#define DUMP_BATCHBUFFERS NULL // "/tmp/i915-batchbuffers.dump"

static void intel_end_vertex(intel_screen_private *intel)
{
	if (intel->vertex_bo) {
		if (intel->vertex_used) {
			dri_bo_subdata(intel->vertex_bo, 0, intel->vertex_used*4, intel->vertex_ptr);
			intel->vertex_used = 0;
		}

		dri_bo_unreference(intel->vertex_bo);
		intel->vertex_bo = NULL;
	}

	intel->vertex_id = 0;
}

void intel_next_vertex(intel_screen_private *intel)
{
	intel_end_vertex(intel);

	intel->vertex_bo =
		dri_bo_alloc(intel->bufmgr, "vertex", sizeof (intel->vertex_ptr), 4096);
}

static void intel_next_batch(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	intel->batch_bo =
	    dri_bo_alloc(intel->bufmgr, "batch", 4096 * 4, 4096);

	intel->batch_used = 0;

	/* We don't know when another client has executed, so we have
	 * to reinitialize our 3D state per batch.
	 */
	intel->last_3d = LAST_3D_OTHER;
}

void intel_batch_init(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	intel->batch_emit_start = 0;
	intel->batch_emitting = 0;
	intel->vertex_id = 0;

	intel_next_batch(scrn);
}

void intel_batch_teardown(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (intel->batch_bo != NULL) {
		dri_bo_unreference(intel->batch_bo);
		intel->batch_bo = NULL;
	}

	if (intel->vertex_bo) {
		dri_bo_unreference(intel->vertex_bo);
		intel->vertex_bo = NULL;
	}

	while (!LIST_IS_EMPTY(&intel->batch_pixmaps))
		LIST_DEL(intel->batch_pixmaps.next);

	while (!LIST_IS_EMPTY(&intel->flush_pixmaps))
		LIST_DEL(intel->flush_pixmaps.next);

	while (!LIST_IS_EMPTY(&intel->in_flight)) {
		struct intel_pixmap *entry;

		entry = LIST_FIRST_ENTRY(&intel->in_flight,
					 struct intel_pixmap,
					 in_flight);

		dri_bo_unreference(entry->bo);
		LIST_DEL(&entry->in_flight);
		free(entry);
	}
}

void intel_batch_do_flush(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	while (!LIST_IS_EMPTY(&intel->flush_pixmaps))
		LIST_DEL(intel->flush_pixmaps.next);
}

static void intel_emit_post_sync_nonzero_flush(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* keep this entire sequence of 3 PIPE_CONTROL cmds in one batch to
	 * avoid upsetting the gpu. */
	BEGIN_BATCH(3*4);
	OUT_BATCH(BRW_PIPE_CONTROL | (4 - 2));
	OUT_BATCH(BRW_PIPE_CONTROL_CS_STALL |
		  BRW_PIPE_CONTROL_STALL_AT_SCOREBOARD);
	OUT_BATCH(0); /* address */
	OUT_BATCH(0); /* write data */

	OUT_BATCH(BRW_PIPE_CONTROL | (4 - 2));
	OUT_BATCH(BRW_PIPE_CONTROL_WRITE_QWORD);
	OUT_RELOC(intel->wa_scratch_bo,
		  I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION, 0);
	OUT_BATCH(0); /* write data */

	/* now finally the _real flush */
	OUT_BATCH(BRW_PIPE_CONTROL | (4 - 2));
	OUT_BATCH(BRW_PIPE_CONTROL_WC_FLUSH |
		  BRW_PIPE_CONTROL_TC_FLUSH |
		  BRW_PIPE_CONTROL_NOWRITE);
	OUT_BATCH(0); /* write address */
	OUT_BATCH(0); /* write data */
	ADVANCE_BATCH();
}

void intel_batch_emit_flush(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int flags;

	assert (!intel->in_batch_atomic);

	/* Big hammer, look to the pipelined flushes in future. */
	if ((INTEL_INFO(intel)->gen >= 60)) {
		if (intel->current_batch == BLT_BATCH) {
			BEGIN_BATCH_BLT(4);
			OUT_BATCH(MI_FLUSH_DW | 2);
			OUT_BATCH(0);
			OUT_BATCH(0);
			OUT_BATCH(0);
			ADVANCE_BATCH();
		} else  {
			if ((INTEL_INFO(intel)->gen == 60)) {
				/* HW-Workaround for Sandybdrige */
				intel_emit_post_sync_nonzero_flush(scrn);
			} else {
				BEGIN_BATCH(4);
				OUT_BATCH(BRW_PIPE_CONTROL | (4 - 2));
				OUT_BATCH(BRW_PIPE_CONTROL_WC_FLUSH |
					  BRW_PIPE_CONTROL_TC_FLUSH |
					  BRW_PIPE_CONTROL_NOWRITE);
				OUT_BATCH(0); /* write address */
				OUT_BATCH(0); /* write data */
				ADVANCE_BATCH();
			}
		}
	} else {
		flags = MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE;
		if (INTEL_INFO(intel)->gen >= 40)
			flags = 0;

		BEGIN_BATCH(1);
		OUT_BATCH(MI_FLUSH | flags);
		ADVANCE_BATCH();
	}
	intel_batch_do_flush(scrn);
}

void intel_batch_submit(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int ret;

	assert (!intel->in_batch_atomic);

	if (intel->vertex_flush)
		intel->vertex_flush(intel);
	intel_end_vertex(intel);

	if (intel->batch_flush)
		intel->batch_flush(intel);

	if (intel->batch_used == 0)
		return;

	/* Mark the end of the batchbuffer. */
	OUT_BATCH(MI_BATCH_BUFFER_END);
	/* Emit a padding dword if we aren't going to be quad-word aligned. */
	if (intel->batch_used & 1)
		OUT_BATCH(MI_NOOP);

	if (DUMP_BATCHBUFFERS) {
	    FILE *file = fopen(DUMP_BATCHBUFFERS, "a");
	    if (file) {
		size_t bytes_fwrite;
		bytes_fwrite = fwrite (intel->batch_ptr, intel->batch_used*4, 1, file);
		if(!bytes_fwrite){
			OS_ERROR("fwrite failed !");
		}
		fclose(file);
	    }
	}

	ret = dri_bo_subdata(intel->batch_bo, 0, intel->batch_used*4, intel->batch_ptr);
	if (ret == 0) {
		ret = drm_intel_bo_mrb_exec(intel->batch_bo,
				intel->batch_used*4,
				NULL, 0, 0xffffffff, intel->current_batch);
	}

	if (ret != 0) {
		if (ret == -EIO) {
			static int once;

			/* The GPU has hung and unlikely to recover by this point. */
			if (!once) {
				xf86DrvMsg(scrn->scrnIndex, X_ERROR, "Detected a hung GPU, disabling acceleration.\n");
				xf86DrvMsg(scrn->scrnIndex, X_ERROR, "When reporting this, please include i915_error_state from debugfs and the full dmesg.\n");
				uxa_set_force_fallback(screenInfo.screens[scrn->scrnIndex], TRUE);
				intel->force_fallback = TRUE;
				once = 1;
			}
		} else {
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Failed to submit batch buffer, expect rendering corruption "
				   "or even a frozen display: %s.\n",
				   strerror(-ret));
		}
	}

	while (!LIST_IS_EMPTY(&intel->batch_pixmaps)) {
		struct intel_pixmap *entry;

		entry = LIST_FIRST_ENTRY(&intel->batch_pixmaps,
					 struct intel_pixmap,
					 batch);

		entry->busy = -1;
		entry->batch_write = 0;
		LIST_DEL(&entry->batch);
	}

	while (!LIST_IS_EMPTY(&intel->flush_pixmaps))
		LIST_DEL(intel->flush_pixmaps.next);

	while (!LIST_IS_EMPTY(&intel->in_flight)) {
		struct intel_pixmap *entry;

		entry = LIST_FIRST_ENTRY(&intel->in_flight,
					 struct intel_pixmap,
					 in_flight);

		dri_bo_unreference(entry->bo);
		LIST_DEL(&entry->in_flight);
		free(entry);
	}

	if (intel->debug_flush & DEBUG_FLUSH_WAIT)
		drm_intel_bo_wait_rendering(intel->batch_bo);

	dri_bo_unreference(intel->batch_bo);
	intel_next_batch(scrn);

	if (intel->batch_commit_notify)
		intel->batch_commit_notify(intel);

	intel->current_batch = 0;
}

void intel_debug_flush(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (intel->debug_flush & DEBUG_FLUSH_CACHES)
		intel_batch_emit_flush(scrn);

	if (intel->debug_flush & DEBUG_FLUSH_BATCHES)
		intel_batch_submit(scrn);
}
