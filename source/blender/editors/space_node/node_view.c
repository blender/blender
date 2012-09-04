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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_view.c
 *  \ingroup spnode
 */

#include "DNA_node_types.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_screen.h"
#include "BKE_node.h"

#include "ED_node.h"  /* own include */
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_image.h"

#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "node_intern.h"  /* own include */


/* **************** View All Operator ************** */

static int space_node_view_flag(bContext *C, SpaceNode *snode, ARegion *ar, const int node_flag)
{
	bNode *node;
	rctf cur_new;
	float oldwidth, oldheight, width, height;
	int tot = 0;
	int has_frame = FALSE;
	
	oldwidth  = BLI_RCT_SIZE_X(&ar->v2d.cur);
	oldheight = BLI_RCT_SIZE_Y(&ar->v2d.cur);

	BLI_rctf_init_minmax(&cur_new);

	if (snode->edittree) {
		for (node = snode->edittree->nodes.first; node; node = node->next) {
			if ((node->flag & node_flag) == node_flag) {
				BLI_rctf_union(&cur_new, &node->totr);
				tot++;

				if (node->type == NODE_FRAME) {
					has_frame = TRUE;
				}
			}
		}
	}

	if (tot) {
		width  = BLI_RCT_SIZE_X(&cur_new);
		height = BLI_RCT_SIZE_Y(&cur_new);

		/* for single non-frame nodes, don't zoom in, just pan view,
		 * but do allow zooming out, this allows for big nodes to be zoomed out */
		if ((tot == 1) &&
		    (has_frame == FALSE) &&
		    ((oldwidth * oldheight) > (width * height)))
		{
			/* center, don't zoom */
			BLI_rctf_resize(&cur_new, oldwidth, oldheight);
		}
		else {
			if (width > height) {
				float newheight;
				newheight = oldheight * width / oldwidth;
				cur_new.ymin = cur_new.ymin - newheight / 4;
				cur_new.ymax = cur_new.ymax + newheight / 4;
			}
			else {
				float newwidth;
				newwidth = oldwidth * height / oldheight;
				cur_new.xmin = cur_new.xmin - newwidth / 4;
				cur_new.xmax = cur_new.xmax + newwidth / 4;
			}
		}

		UI_view2d_smooth_view(C, ar, &cur_new);
	}

	return (tot != 0);
}

static int node_view_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	ARegion *ar = CTX_wm_region(C);
	SpaceNode *snode = CTX_wm_space_node(C);

	/* is this really needed? */
	snode->xof = 0;
	snode->yof = 0;

	if (space_node_view_flag(C, snode, ar, 0)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void NODE_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "NODE_OT_view_all";
	ot->description = "Resize view so you can see all nodes";
	
	/* api callbacks */
	ot->exec = node_view_all_exec;
	ot->poll = ED_operator_node_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int node_view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	ARegion *ar = CTX_wm_region(C);
	SpaceNode *snode = CTX_wm_space_node(C);

	if (space_node_view_flag(C, snode, ar, NODE_SELECT)) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void NODE_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Selected";
	ot->idname = "NODE_OT_view_selected";
	ot->description = "Resize view so you can see selected nodes";

	/* api callbacks */
	ot->exec = node_view_selected_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************** Backround Image Operators ************** */

typedef struct NodeViewMove {
	int mvalo[2];
	int xmin, ymin, xmax, ymax;
} NodeViewMove;

static int snode_bg_viewmove_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	NodeViewMove *nvm = op->customdata;

	switch (event->type) {
		case MOUSEMOVE:

			snode->xof -= (nvm->mvalo[0] - event->mval[0]);
			snode->yof -= (nvm->mvalo[1] - event->mval[1]);
			nvm->mvalo[0] = event->mval[0];
			nvm->mvalo[1] = event->mval[1];

			/* prevent dragging image outside of the window and losing it! */
			CLAMP(snode->xof, nvm->xmin, nvm->xmax);
			CLAMP(snode->yof, nvm->ymin, nvm->ymax);

			ED_region_tag_redraw(ar);

			break;

		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:

			MEM_freeN(nvm);
			op->customdata = NULL;

			return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int snode_bg_viewmove_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	NodeViewMove *nvm;
	Image *ima;
	ImBuf *ibuf;
	const float pad = 32.0f; /* better be bigger then scrollbars */

	void *lock;

	ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf == NULL) {
		BKE_image_release_ibuf(ima, lock);
		return OPERATOR_CANCELLED;
	}

	nvm = MEM_callocN(sizeof(NodeViewMove), "NodeViewMove struct");
	op->customdata = nvm;
	nvm->mvalo[0] = event->mval[0];
	nvm->mvalo[1] = event->mval[1];

	nvm->xmin = -(ar->winx / 2) - (ibuf->x * (0.5f * snode->zoom)) + pad;
	nvm->xmax =  (ar->winx / 2) + (ibuf->x * (0.5f * snode->zoom)) - pad;
	nvm->ymin = -(ar->winy / 2) - (ibuf->y * (0.5f * snode->zoom)) + pad;
	nvm->ymax =  (ar->winy / 2) + (ibuf->y * (0.5f * snode->zoom)) - pad;

	BKE_image_release_ibuf(ima, lock);

	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int snode_bg_viewmove_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata = NULL;

	return OPERATOR_CANCELLED;
}

void NODE_OT_backimage_move(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Background Image Move";
	ot->description = "Move Node backdrop";
	ot->idname = "NODE_OT_backimage_move";

	/* api callbacks */
	ot->invoke = snode_bg_viewmove_invoke;
	ot->modal = snode_bg_viewmove_modal;
	ot->poll = composite_node_active;
	ot->cancel = snode_bg_viewmove_cancel;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_POINTER;
}

static int backimage_zoom(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	float fac = RNA_float_get(op->ptr, "factor");

	snode->zoom *= fac;
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}


void NODE_OT_backimage_zoom(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Background Image Zoom";
	ot->idname = "NODE_OT_backimage_zoom";
	ot->description = "Zoom in/out the background image";

	/* api callbacks */
	ot->exec = backimage_zoom;
	ot->poll = composite_node_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;

	/* internal */
	RNA_def_float(ot->srna, "factor", 1.2f, 0.0f, 10.0f, "Factor", "", 0.0f, 10.0f);
}

/******************** sample backdrop operator ********************/

typedef struct ImageSampleInfo {
	ARegionType *art;
	void *draw_handle;
	int x, y;
	int channels;

	unsigned char col[4];
	float colf[4];

	int draw;
	int color_manage;
} ImageSampleInfo;

static void sample_draw(const bContext *C, ARegion *ar, void *arg_info)
{
	Scene *scene = CTX_data_scene(C);
	ImageSampleInfo *info = arg_info;

	if (info->draw) {
		ED_image_draw_info(scene, ar, info->color_manage, info->channels,
		                   info->x, info->y, info->col, info->colf,
		                   NULL, NULL /* zbuf - unused for nodes */
		                   );
	}
}

/* returns color in SRGB */
/* matching ED_space_image_color_sample() */
int ED_space_node_color_sample(SpaceNode *snode, ARegion *ar, int mval[2], float r_col[3])
{
	void *lock;
	Image *ima;
	ImBuf *ibuf;
	float fx, fy, bufx, bufy;
	int ret = FALSE;

	ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);
	if (!ibuf) {
		return FALSE;
	}

	/* map the mouse coords to the backdrop image space */
	bufx = ibuf->x * snode->zoom;
	bufy = ibuf->y * snode->zoom;
	fx = (bufx > 0.0f ? ((float)mval[0] - 0.5f * ar->winx - snode->xof) / bufx + 0.5f : 0.0f);
	fy = (bufy > 0.0f ? ((float)mval[1] - 0.5f * ar->winy - snode->yof) / bufy + 0.5f : 0.0f);

	if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
		float *fp;
		unsigned char *cp;
		int x = (int)(fx * ibuf->x), y = (int)(fy * ibuf->y);

		CLAMP(x, 0, ibuf->x - 1);
		CLAMP(y, 0, ibuf->y - 1);

		if (ibuf->rect_float) {
			fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));
			/* IB_PROFILE_NONE is default but infact its linear */
			if (ELEM(ibuf->profile, IB_PROFILE_LINEAR_RGB, IB_PROFILE_NONE)) {
				linearrgb_to_srgb_v3_v3(r_col, fp);
			}
			else {
				copy_v3_v3(r_col, fp);
			}
			ret = TRUE;
		}
		else if (ibuf->rect) {
			cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);
			rgb_uchar_to_float(r_col, cp);
			ret = TRUE;
		}
	}

	BKE_image_release_ibuf(ima, lock);

	return ret;
}

static void sample_apply(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	ImageSampleInfo *info = op->customdata;
	void *lock;
	Image *ima;
	ImBuf *ibuf;
	float fx, fy, bufx, bufy;

	ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);
	if (!ibuf) {
		info->draw = 0;
		return;
	}

	if (!ibuf->rect) {
		if (info->color_manage)
			ibuf->profile = IB_PROFILE_LINEAR_RGB;
		else
			ibuf->profile = IB_PROFILE_NONE;
		IMB_rect_from_float(ibuf);
	}

	/* map the mouse coords to the backdrop image space */
	bufx = ibuf->x * snode->zoom;
	bufy = ibuf->y * snode->zoom;
	fx = (bufx > 0.0f ? ((float)event->mval[0] - 0.5f * ar->winx - snode->xof) / bufx + 0.5f : 0.0f);
	fy = (bufy > 0.0f ? ((float)event->mval[1] - 0.5f * ar->winy - snode->yof) / bufy + 0.5f : 0.0f);

	if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
		float *fp;
		unsigned char *cp;
		int x = (int)(fx * ibuf->x), y = (int)(fy * ibuf->y);

		CLAMP(x, 0, ibuf->x - 1);
		CLAMP(y, 0, ibuf->y - 1);

		info->x = x;
		info->y = y;
		info->draw = 1;
		info->channels = ibuf->channels;

		if (ibuf->rect) {
			cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);

			info->col[0] = cp[0];
			info->col[1] = cp[1];
			info->col[2] = cp[2];
			info->col[3] = cp[3];

			info->colf[0] = (float)cp[0] / 255.0f;
			info->colf[1] = (float)cp[1] / 255.0f;
			info->colf[2] = (float)cp[2] / 255.0f;
			info->colf[3] = (float)cp[3] / 255.0f;

			info->color_manage = FALSE;
		}
		if (ibuf->rect_float) {
			fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));

			info->colf[0] = fp[0];
			info->colf[1] = fp[1];
			info->colf[2] = fp[2];
			info->colf[3] = fp[3];

			info->color_manage = TRUE;
		}

		ED_node_sample_set(info->colf);
	}
	else {
		info->draw = 0;
		ED_node_sample_set(NULL);
	}

	BKE_image_release_ibuf(ima, lock);

	ED_area_tag_redraw(CTX_wm_area(C));
}

static void sample_exit(bContext *C, wmOperator *op)
{
	ImageSampleInfo *info = op->customdata;

	ED_node_sample_set(NULL);
	ED_region_draw_cb_exit(info->art, info->draw_handle);
	ED_area_tag_redraw(CTX_wm_area(C));
	MEM_freeN(info);
}

static int sample_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	ARegion *ar = CTX_wm_region(C);
	ImageSampleInfo *info;

	if (snode->treetype != NTREE_COMPOSIT || !(snode->flag & SNODE_BACKDRAW))
		return OPERATOR_CANCELLED;

	info = MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");
	info->art = ar->type;
	info->draw_handle = ED_region_draw_cb_activate(ar->type, sample_draw, info, REGION_DRAW_POST_PIXEL);
	op->customdata = info;

	sample_apply(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int sample_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch (event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			sample_exit(C, op);
			return OPERATOR_CANCELLED;
		case MOUSEMOVE:
			sample_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int sample_cancel(bContext *C, wmOperator *op)
{
	sample_exit(C, op);
	return OPERATOR_CANCELLED;
}

void NODE_OT_backimage_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Backimage Sample";
	ot->idname = "NODE_OT_backimage_sample";
	ot->description = "Use mouse to sample background image";

	/* api callbacks */
	ot->invoke = sample_invoke;
	ot->modal = sample_modal;
	ot->cancel = sample_cancel;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}
