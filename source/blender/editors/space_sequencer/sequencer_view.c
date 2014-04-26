/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/editors/space_sequencer/sequencer_view.c
 *  \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_sequencer.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "UI_view2d.h"

/* own include */
#include "sequencer_intern.h"

/******************** sample backdrop operator ********************/

typedef struct ImageSampleInfo {
	ARegionType *art;
	void *draw_handle;
	int x, y;
	int channels;

	unsigned char col[4];
	float colf[4];
	float linearcol[4];

	unsigned char *colp;
	const float *colfp;

	int draw;
int color_manage;
} ImageSampleInfo;

static void sample_draw(const bContext *C, ARegion *ar, void *arg_info)
{
	Scene *scene = CTX_data_scene(C);
	ImageSampleInfo *info = arg_info;

	if (info->draw) {
		ED_image_draw_info(scene, ar, info->color_manage, false, info->channels,
		                   info->x, info->y, info->colp, info->colfp,
		                   info->linearcol, NULL, NULL);
	}
}

static void sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceSeq *sseq = (SpaceSeq *) CTX_wm_space_data(C);
	ARegion *ar = CTX_wm_region(C);
	ImBuf *ibuf = sequencer_ibuf_get(bmain, scene, sseq, CFRA, 0);
	ImageSampleInfo *info = op->customdata;
	float fx, fy;
	
	if (ibuf == NULL) {
		IMB_freeImBuf(ibuf);
		info->draw = 0;
		return;
	}

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fx, &fy);

	fx += (float) ibuf->x / 2.0f;
	fy += (float) ibuf->y / 2.0f;

	if (fx >= 0.0f && fy >= 0.0f && fx < ibuf->x && fy < ibuf->y) {
		const float *fp;
		unsigned char *cp;
		int x = (int) fx, y = (int) fy;

		info->x = x;
		info->y = y;
		info->draw = 1;
		info->channels = ibuf->channels;

		info->colp = NULL;
		info->colfp = NULL;
		
		if (ibuf->rect) {
			cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);

			info->col[0] = cp[0];
			info->col[1] = cp[1];
			info->col[2] = cp[2];
			info->col[3] = cp[3];
			info->colp = info->col;

			info->colf[0] = (float)cp[0] / 255.0f;
			info->colf[1] = (float)cp[1] / 255.0f;
			info->colf[2] = (float)cp[2] / 255.0f;
			info->colf[3] = (float)cp[3] / 255.0f;
			info->colfp = info->colf;

			copy_v4_v4(info->linearcol, info->colf);
			IMB_colormanagement_colorspace_to_scene_linear_v4(info->linearcol, false, ibuf->rect_colorspace);

			info->color_manage = true;
		}
		if (ibuf->rect_float) {
			fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));

			info->colf[0] = fp[0];
			info->colf[1] = fp[1];
			info->colf[2] = fp[2];
			info->colf[3] = fp[3];
			info->colfp = info->colf;

			/* sequencer's image buffers are in non-linear space, need to make them linear */
			copy_v4_v4(info->linearcol, info->colf);
			BKE_sequencer_pixel_from_sequencer_space_v4(scene, info->linearcol);

			info->color_manage = true;
		}
	}
	else {
		info->draw = 0;
	}

	IMB_freeImBuf(ibuf);
	ED_area_tag_redraw(CTX_wm_area(C));
}

static void sample_exit(bContext *C, wmOperator *op)
{
	ImageSampleInfo *info = op->customdata;

	ED_region_draw_cb_exit(info->art, info->draw_handle);
	ED_area_tag_redraw(CTX_wm_area(C));
	MEM_freeN(info);
}

static int sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceSeq *sseq = CTX_wm_space_seq(C);
	ImageSampleInfo *info;

	if (sseq->mainb != SEQ_DRAW_IMG_IMBUF)
		return OPERATOR_CANCELLED;

	info = MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");
	info->art = ar->type;
	info->draw_handle = ED_region_draw_cb_activate(ar->type, sample_draw, info, REGION_DRAW_POST_PIXEL);
	op->customdata = info;

	sample_apply(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	switch (event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: /* XXX hardcoded */
			sample_exit(C, op);
			return OPERATOR_CANCELLED;
		case MOUSEMOVE:
			sample_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void sample_cancel(bContext *C, wmOperator *op)
{
	sample_exit(C, op);
}

static int sample_poll(bContext *C)
{
	SpaceSeq *sseq = CTX_wm_space_seq(C);
	return sseq && BKE_sequencer_editing_get(CTX_data_scene(C), false) != NULL;
}

void SEQUENCER_OT_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Color";
	ot->idname = "SEQUENCER_OT_sample";
	ot->description = "Use mouse to sample color in current frame";

	/* api callbacks */
	ot->invoke = sample_invoke;
	ot->modal = sample_modal;
	ot->cancel = sample_cancel;
	ot->poll = sample_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}
