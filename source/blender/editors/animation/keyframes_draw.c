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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/keyframes_draw.c
 *  \ingroup edanimation
 */


/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_dlrbTree.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_brush_types.h"
#include "DNA_mask_types.h"

#include "BKE_fcurve.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_state.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

/* *************************** Keyframe Processing *************************** */

/* ActKeyColumns (Keyframe Columns) ------------------------------------------ */

BLI_INLINE bool is_cfra_eq(float a, float b)
{
	return IS_EQT(a, b, BEZT_BINARYSEARCH_THRESH);
}

BLI_INLINE bool is_cfra_lt(float a, float b)
{
	return (b - a) > BEZT_BINARYSEARCH_THRESH;
}

/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
/* NOTE: this is exported to other modules that use the ActKeyColumns for finding keyframes */
short compare_ak_cfraPtr(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	const float *cframe = data;
	float val = *cframe;

	if (is_cfra_eq(val, ak->cfra))
		return 0;

	if (val < ak->cfra)
		return -1;
	else
		return 1;
}

/* --------------- */

/* Comparator callback used for ActKeyColumns and BezTriple */
static short compare_ak_bezt(void *node, void *data)
{
	BezTriple *bezt = (BezTriple *)data;

	return compare_ak_cfraPtr(node, &bezt->vec[1][0]);
}

/* New node callback used for building ActKeyColumns from BezTriples */
static DLRBT_Node *nalloc_ak_bezt(void *data)
{
	ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
	BezTriple *bezt = (BezTriple *)data;

	/* store settings based on state of BezTriple */
	ak->cfra = bezt->vec[1][0];
	ak->sel = BEZT_ISSEL_ANY(bezt) ? SELECT : 0;
	ak->key_type = BEZKEYTYPE(bezt);

	/* count keyframes in this column */
	ak->totkey = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from BezTriples */
static void nupdate_ak_bezt(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	BezTriple *bezt = (BezTriple *)data;

	/* set selection status and 'touched' status */
	if (BEZT_ISSEL_ANY(bezt)) ak->sel = SELECT;

	/* count keyframes in this column */
	ak->totkey++;

	/* for keyframe type, 'proper' keyframes have priority over breakdowns (and other types for now) */
	if (BEZKEYTYPE(bezt) == BEZT_KEYTYPE_KEYFRAME)
		ak->key_type = BEZT_KEYTYPE_KEYFRAME;
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_gpframe(void *node, void *data)
{
	bGPDframe *gpf = (bGPDframe *)data;

	return compare_ak_cfraPtr(node, &gpf->framenum);
}

/* New node callback used for building ActKeyColumns from GPencil frames */
static DLRBT_Node *nalloc_ak_gpframe(void *data)
{
	ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumnGPF");
	bGPDframe *gpf = (bGPDframe *)data;

	/* store settings based on state of BezTriple */
	ak->cfra = gpf->framenum;
	ak->sel = (gpf->flag & GP_FRAME_SELECT) ? SELECT : 0;
	ak->key_type = gpf->key_type;

	/* count keyframes in this column */
	ak->totkey = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_gpframe(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	bGPDframe *gpf = (bGPDframe *)data;

	/* set selection status and 'touched' status */
	if (gpf->flag & GP_FRAME_SELECT) ak->sel = SELECT;

	/* count keyframes in this column */
	ak->totkey++;

	/* for keyframe type, 'proper' keyframes have priority over breakdowns (and other types for now) */
	if (gpf->key_type == BEZT_KEYTYPE_KEYFRAME)
		ak->key_type = BEZT_KEYTYPE_KEYFRAME;
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_masklayshape(void *node, void *data)
{
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	return compare_ak_cfraPtr(node, &masklay_shape->frame);
}

/* New node callback used for building ActKeyColumns from GPencil frames */
static DLRBT_Node *nalloc_ak_masklayshape(void *data)
{
	ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumnGPF");
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	/* store settings based on state of BezTriple */
	ak->cfra = masklay_shape->frame;
	ak->sel = (masklay_shape->flag & MASK_SHAPE_SELECT) ? SELECT : 0;

	/* count keyframes in this column */
	ak->totkey = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_masklayshape(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	/* set selection status and 'touched' status */
	if (masklay_shape->flag & MASK_SHAPE_SELECT) ak->sel = SELECT;

	/* count keyframes in this column */
	ak->totkey++;
}


/* --------------- */

/* Add the given BezTriple to the given 'list' of Keyframes */
static void add_bezt_to_keycolumns_list(DLRBT_Tree *keys, BezTriple *bezt)
{
	if (ELEM(NULL, keys, bezt))
		return;
	else
		BLI_dlrbTree_add(keys, compare_ak_bezt, nalloc_ak_bezt, nupdate_ak_bezt, bezt);
}

/* Add the given GPencil Frame to the given 'list' of Keyframes */
static void add_gpframe_to_keycolumns_list(DLRBT_Tree *keys, bGPDframe *gpf)
{
	if (ELEM(NULL, keys, gpf))
		return;
	else
		BLI_dlrbTree_add(keys, compare_ak_gpframe, nalloc_ak_gpframe, nupdate_ak_gpframe, gpf);
}

/* Add the given MaskLayerShape Frame to the given 'list' of Keyframes */
static void add_masklay_to_keycolumns_list(DLRBT_Tree *keys, MaskLayerShape *masklay_shape)
{
	if (ELEM(NULL, keys, masklay_shape))
		return;
	else
		BLI_dlrbTree_add(keys, compare_ak_masklayshape, nalloc_ak_masklayshape, nupdate_ak_masklayshape, masklay_shape);
}

/* ActKeyBlocks (Long Keyframes) ------------------------------------------ */

static const ActKeyBlockInfo dummy_keyblock = { 0 };

static void compute_keyblock_data(ActKeyBlockInfo *info, BezTriple *prev, BezTriple *beztn)
{
	memset(info, 0, sizeof(ActKeyBlockInfo));

	if (BEZKEYTYPE(beztn) == BEZT_KEYTYPE_MOVEHOLD) {
		/* Animator tagged a "moving hold"
		 *   - Previous key must also be tagged as a moving hold, otherwise
		 *     we're just dealing with the first of a pair, and we don't
		 *     want to be creating any phantom holds...
		 */
		if (BEZKEYTYPE(prev) == BEZT_KEYTYPE_MOVEHOLD) {
			info->flag |= ACTKEYBLOCK_FLAG_MOVING_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD;
		}
	}

	/* Check for same values...
	 *  - Handles must have same central value as each other
	 *  - Handles which control that section of the curve must be constant
	 */
	if (IS_EQF(beztn->vec[1][1], prev->vec[1][1])) {
		bool hold;

		/* Only check handles in case of actual bezier interpolation. */
		if (prev->ipo == BEZT_IPO_BEZ) {
			hold = IS_EQF(beztn->vec[1][1], beztn->vec[0][1]) && IS_EQF(prev->vec[1][1], prev->vec[2][1]);
		}
		/* This interpolation type induces movement even between identical keys. */
		else {
			hold = !ELEM(prev->ipo, BEZT_IPO_ELASTIC);
		}

		if (hold) {
			info->flag |= ACTKEYBLOCK_FLAG_STATIC_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD;
		}
	}

	info->sel = BEZT_ISSEL_ANY(prev) || BEZT_ISSEL_ANY(beztn);
}

static void add_keyblock_info(ActKeyColumn *col, const ActKeyBlockInfo *block)
{
	/* New curve and block. */
	if (col->totcurve <= 1 && col->totblock == 0) {
		memcpy(&col->block, block, sizeof(ActKeyBlockInfo));
	}
	/* Existing curve. */
	else {
		col->block.conflict |= (col->block.flag ^ block->flag);
		col->block.flag |= block->flag;
		col->block.sel |= block->sel;
	}

	if (block->flag) {
		col->totblock++;
	}
}

static void add_bezt_to_keyblocks_list(DLRBT_Tree *keys, BezTriple *bezt, int bezt_len)
{
	ActKeyColumn *col = keys->first;

	if (bezt && bezt_len >= 2) {
		ActKeyBlockInfo block;

		/* Find the first key column while inserting dummy blocks. */
		for (; col != NULL && is_cfra_lt(col->cfra, bezt[0].vec[1][0]); col = col->next) {
			add_keyblock_info(col, &dummy_keyblock);
		}

		BLI_assert(col != NULL);

		/* Insert real blocks. */
		for (int v = 1; col != NULL && v < bezt_len; v++, bezt++) {
			/* Wrong order of bezier keys: resync position. */
			if (is_cfra_lt(bezt[1].vec[1][0], bezt[0].vec[1][0])) {
				/* Backtrack to find the right location. */
				if (is_cfra_lt(bezt[1].vec[1][0], col->cfra)) {
					ActKeyColumn *newcol = (ActKeyColumn *)BLI_dlrbTree_search_exact(
					        keys, compare_ak_cfraPtr, &bezt[1].vec[1][0]);

					if (newcol != NULL) {
						col = newcol;

						/* The previous keyblock is garbage too. */
						if (col->prev != NULL) {
							add_keyblock_info(col->prev, &dummy_keyblock);
						}
					}
					else {
						BLI_assert(false);
					}
				}

				continue;
			}

			/* Normal sequence */
			BLI_assert(is_cfra_eq(col->cfra, bezt[0].vec[1][0]));

			compute_keyblock_data(&block, bezt, bezt + 1);

			for (; col != NULL && is_cfra_lt(col->cfra, bezt[1].vec[1][0]); col = col->next) {
				add_keyblock_info(col, &block);
			}

			BLI_assert(col != NULL);
		}
	}

	/* Insert dummy blocks at the end. */
	for (; col != NULL; col = col->next) {
		add_keyblock_info(col, &dummy_keyblock);
	}
}

/* Walk through columns and propagate blocks and totcurve.
 *
 * This must be called even by animation sources that don't generate
 * keyblocks to keep the data structure consistent after adding columns.
 */
static void update_keyblocks(DLRBT_Tree *keys, BezTriple *bezt, int bezt_len)
{
	/* Recompute the prev/next linked list. */
	BLI_dlrbTree_linkedlist_sync(keys);

	/* Find the curve count */
	int max_curve = 0;

	for (ActKeyColumn *col = keys->first; col; col = col->next) {
		max_curve = MAX2(max_curve, col->totcurve);
	}

	/* Propagate blocks to inserted keys */
	ActKeyColumn *prev_ready = NULL;

	for (ActKeyColumn *col = keys->first; col; col = col->next) {
		/* Pre-existing column. */
		if (col->totcurve > 0) {
			prev_ready = col;
		}
		/* Newly inserted column, so copy block data from previous. */
		else if (prev_ready != NULL) {
			col->totblock = prev_ready->totblock;
			memcpy(&col->block, &prev_ready->block, sizeof(ActKeyBlockInfo));
		}

		col->totcurve = max_curve + 1;
	}

	/* Add blocks on top */
	add_bezt_to_keyblocks_list(keys, bezt, bezt_len);
}

/* --------- */

/* Checks if ActKeyBlock should exist... */
int actkeyblock_get_valid_hold(ActKeyColumn *ac)
{
	/* check that block is valid */
	if (ac == NULL || ac->next == NULL || ac->totblock == 0)
		return 0;

	const int hold_mask = (ACTKEYBLOCK_FLAG_ANY_HOLD | ACTKEYBLOCK_FLAG_STATIC_HOLD | ACTKEYBLOCK_FLAG_ANY_HOLD);
	return (ac->block.flag & ~ac->block.conflict) & hold_mask;
}

/* *************************** Keyframe Drawing *************************** */

void draw_keyframe_shape(float x, float y, float size, bool sel, short key_type, short mode, float alpha,
                         unsigned int pos_id, unsigned int size_id, unsigned int color_id, unsigned int outline_color_id)
{
	bool draw_fill = ELEM(mode, KEYFRAME_SHAPE_INSIDE, KEYFRAME_SHAPE_BOTH);
	bool draw_outline = ELEM(mode, KEYFRAME_SHAPE_FRAME, KEYFRAME_SHAPE_BOTH);

	BLI_assert(draw_fill || draw_outline);

	/* tweak size of keyframe shape according to type of keyframe
	 * - 'proper' keyframes have key_type = 0, so get drawn at full size
	 */
	switch (key_type) {
		case BEZT_KEYTYPE_KEYFRAME:  /* must be full size */
			break;

		case BEZT_KEYTYPE_BREAKDOWN: /* slightly smaller than normal keyframe */
			size *= 0.85f;
			break;

		case BEZT_KEYTYPE_MOVEHOLD:  /* slightly smaller than normal keyframes (but by less than for breakdowns) */
			size *= 0.925f;
			break;

		case BEZT_KEYTYPE_EXTREME:   /* slightly larger */
			size *= 1.2f;
			break;

		default:
			size -= 0.8f * key_type;
	}

	unsigned char fill_col[4];
	unsigned char outline_col[4];

	/* draw! */
	if (draw_fill) {
		/* get interior colors from theme (for selected and unselected only) */
		switch (key_type) {
			case BEZT_KEYTYPE_BREAKDOWN: /* bluish frames (default theme) */
				UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_BREAKDOWN_SELECT : TH_KEYTYPE_BREAKDOWN, fill_col);
				break;
			case BEZT_KEYTYPE_EXTREME: /* reddish frames (default theme) */
				UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_EXTREME_SELECT : TH_KEYTYPE_EXTREME, fill_col);
				break;
			case BEZT_KEYTYPE_JITTER: /* greenish frames (default theme) */
				UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_JITTER_SELECT : TH_KEYTYPE_JITTER, fill_col);
				break;
			case BEZT_KEYTYPE_MOVEHOLD: /* similar to traditional keyframes, but different... */
				UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_MOVEHOLD_SELECT : TH_KEYTYPE_MOVEHOLD, fill_col);
				break;
			case BEZT_KEYTYPE_KEYFRAME: /* traditional yellowish frames (default theme) */
			default:
				UI_GetThemeColor4ubv(sel ? TH_KEYTYPE_KEYFRAME_SELECT : TH_KEYTYPE_KEYFRAME, fill_col);
		}

		/* NOTE: we don't use the straight alpha from the theme, or else effects such as
		 * graying out protected/muted channels doesn't work correctly!
		 */
		fill_col[3] *= alpha;

		if (!draw_outline) {
			/* force outline color to match */
			outline_col[0] = fill_col[0];
			outline_col[1] = fill_col[1];
			outline_col[2] = fill_col[2];
			outline_col[3] = fill_col[3];
		}
	}

	if (draw_outline) {
		/* exterior - black frame */
		UI_GetThemeColor4ubv(sel ? TH_KEYBORDER_SELECT : TH_KEYBORDER, outline_col);
		outline_col[3] *= alpha;

		if (!draw_fill) {
			/* fill color needs to be (outline.rgb, 0) */
			fill_col[0] = outline_col[0];
			fill_col[1] = outline_col[1];
			fill_col[2] = outline_col[2];
			fill_col[3] = 0;
		}
	}

	immAttr1f(size_id, size);
	immAttr4ubv(color_id, fill_col);
	immAttr4ubv(outline_color_id, outline_col);
	immVertex2f(pos_id, x, y);
}

static void draw_keylist(View2D *v2d, DLRBT_Tree *keys, float ypos, float yscale_fac, bool channelLocked)
{
	const float icon_sz = U.widget_unit * 0.5f * yscale_fac;
	const float half_icon_sz = 0.5f * icon_sz;
	const float smaller_sz = 0.35f * icon_sz;

	GPU_blend(true);

	/* locked channels are less strongly shown, as feedback for locked channels in DopeSheet */
	/* TODO: allow this opacity factor to be themed? */
	float alpha = channelLocked ? 0.25f : 1.0f;

	/* draw keyblocks */
	if (keys) {
		float sel_color[4], unsel_color[4];
		float sel_mhcol[4], unsel_mhcol[4];

		/* cache colours first */
		UI_GetThemeColor4fv(TH_STRIP_SELECT, sel_color);
		UI_GetThemeColor4fv(TH_STRIP, unsel_color);

		sel_color[3]   *= alpha;
		unsel_color[3] *= alpha;

		copy_v4_v4(sel_mhcol, sel_color);
		sel_mhcol[3]   *= 0.8f;
		copy_v4_v4(unsel_mhcol, unsel_color);
		unsel_mhcol[3] *= 0.8f;

		uint block_len = 0;
		for (ActKeyColumn *ab = keys->first; ab; ab = ab->next) {
			if (actkeyblock_get_valid_hold(ab)) {
				block_len++;
			}
		}

		if (block_len > 0) {
			GPUVertFormat *format = immVertexFormat();
			uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			uint color_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

			immBegin(GPU_PRIM_TRIS, 6 * block_len);
			for (ActKeyColumn *ab = keys->first; ab; ab = ab->next) {
				int valid_hold = actkeyblock_get_valid_hold(ab);
				if (valid_hold != 0) {
					if ((valid_hold & ACTKEYBLOCK_FLAG_STATIC_HOLD) == 0) {
						/* draw "moving hold" long-keyframe block - slightly smaller */
						immRectf_fast_with_color(pos_id, color_id,
						                         ab->cfra, ypos - smaller_sz, ab->next->cfra, ypos + smaller_sz,
						                         (ab->block.sel) ? sel_mhcol : unsel_mhcol);
					}
					else {
						/* draw standard long-keyframe block */
						immRectf_fast_with_color(pos_id, color_id,
						                         ab->cfra, ypos - half_icon_sz, ab->next->cfra, ypos + half_icon_sz,
						                         (ab->block.sel) ? sel_color : unsel_color);
					}
				}
			}
			immEnd();
			immUnbindProgram();
		}
	}

	if (keys) {
		/* count keys */
		uint key_len = 0;
		for (ActKeyColumn *ak = keys->first; ak; ak = ak->next) {
			/* optimization: if keyframe doesn't appear within 5 units (screenspace) in visible area, don't draw
			 *	- this might give some improvements, since we current have to flip between view/region matrices
			 */
			if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax))
				key_len++;
		}

		if (key_len > 0) {
			/* draw keys */
			GPUVertFormat *format = immVertexFormat();
			uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			uint size_id = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
			uint color_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
			uint outline_color_id = GPU_vertformat_attr_add(format, "outlineColor", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
			immBindBuiltinProgram(GPU_SHADER_KEYFRAME_DIAMOND);
			GPU_enable_program_point_size();
			immBegin(GPU_PRIM_POINTS, key_len);

			for (ActKeyColumn *ak = keys->first; ak; ak = ak->next) {
				if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax)) {
					draw_keyframe_shape(ak->cfra, ypos, icon_sz, (ak->sel & SELECT), ak->key_type, KEYFRAME_SHAPE_BOTH, alpha,
					                    pos_id, size_id, color_id, outline_color_id);
				}
			}

			immEnd();
			GPU_disable_program_point_size();
			immUnbindProgram();
		}
	}

	GPU_blend(false);
}

/* *************************** Channel Drawing Funcs *************************** */

void draw_summary_channel(View2D *v2d, bAnimContext *ac, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	summary_to_keylist(ac, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
}

void draw_scene_channel(View2D *v2d, bDopeSheet *ads, Scene *sce, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	scene_to_keylist(ads, sce, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
}

void draw_object_channel(View2D *v2d, bDopeSheet *ads, Object *ob, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	ob_to_keylist(ads, ob, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
}

void draw_fcurve_channel(View2D *v2d, AnimData *adt, FCurve *fcu, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (fcu->flag & FCURVE_PROTECTED) ||
	              ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
	              ((adt && adt->action) && ID_IS_LINKED(adt->action));

	BLI_dlrbTree_init(&keys);

	fcurve_to_keylist(adt, fcu, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

void draw_agroup_channel(View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (agrp->flag & AGRP_PROTECTED) ||
	              ((adt && adt->action) && ID_IS_LINKED(adt->action));

	BLI_dlrbTree_init(&keys);

	agroup_to_keylist(adt, agrp, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

void draw_action_channel(View2D *v2d, AnimData *adt, bAction *act, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (act && ID_IS_LINKED(act));

	BLI_dlrbTree_init(&keys);

	action_to_keylist(adt, act, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

void draw_gpencil_channel(View2D *v2d, bDopeSheet *ads, bGPdata *gpd, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	gpencil_to_keylist(ads, gpd, &keys, false);

	draw_keylist(v2d, &keys, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
}

void draw_gpl_channel(View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (gpl->flag & GP_LAYER_LOCKED) != 0;

	BLI_dlrbTree_init(&keys);

	gpl_to_keylist(ads, gpl, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

void draw_masklay_channel(View2D *v2d, bDopeSheet *ads, MaskLayer *masklay, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (masklay->flag & MASK_LAYERFLAG_LOCKED) != 0;

	BLI_dlrbTree_init(&keys);

	mask_to_keylist(ads, masklay, &keys);

	draw_keylist(v2d, &keys, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

/* *************************** Keyframe List Conversions *************************** */

void summary_to_keylist(bAnimContext *ac, DLRBT_Tree *keys)
{
	if (ac) {
		ListBase anim_data = {NULL, NULL};
		bAnimListElem *ale;
		int filter;

		/* get F-Curves to take keyframes from */
		filter = ANIMFILTER_DATA_VISIBLE;
		ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

		/* loop through each F-Curve, grabbing the keyframes */
		for (ale = anim_data.first; ale; ale = ale->next) {
			/* Why not use all #eAnim_KeyType here?
			 * All of the other key types are actually "summaries" themselves, and will just end up duplicating stuff
			 * that comes up through standard filtering of just F-Curves.
			 * Given the way that these work, there isn't really any benefit at all from including them. - Aligorith */

			switch (ale->datatype) {
				case ALE_FCURVE:
					fcurve_to_keylist(ale->adt, ale->data, keys);
					break;
				case ALE_MASKLAY:
					mask_to_keylist(ac->ads, ale->data, keys);
					break;
				case ALE_GPFRAME:
					gpl_to_keylist(ac->ads, ale->data, keys);
					break;
				default:
					// printf("%s: datatype %d unhandled\n", __func__, ale->datatype);
					break;
			}
		}

		ANIM_animdata_freelist(&anim_data);
	}
}

void scene_to_keylist(bDopeSheet *ads, Scene *sce, DLRBT_Tree *keys)
{
	bAnimContext ac = {NULL};
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	bAnimListElem dummychan = {NULL};

	if (sce == NULL)
		return;

	/* create a dummy wrapper data to work with */
	dummychan.type = ANIMTYPE_SCENE;
	dummychan.data = sce;
	dummychan.id = &sce->id;
	dummychan.adt = sce->adt;

	ac.ads = ads;
	ac.data = &dummychan;
	ac.datatype = ANIMCONT_CHANNEL;

	/* get F-Curves to take keyframes from */
	filter = ANIMFILTER_DATA_VISIBLE; // curves only
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

	/* loop through each F-Curve, grabbing the keyframes */
	for (ale = anim_data.first; ale; ale = ale->next)
		fcurve_to_keylist(ale->adt, ale->data, keys);

	ANIM_animdata_freelist(&anim_data);
}

void ob_to_keylist(bDopeSheet *ads, Object *ob, DLRBT_Tree *keys)
{
	bAnimContext ac = {NULL};
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	bAnimListElem dummychan = {NULL};
	Base dummybase = {NULL};

	if (ob == NULL)
		return;

	/* create a dummy wrapper data to work with */
	dummybase.object = ob;

	dummychan.type = ANIMTYPE_OBJECT;
	dummychan.data = &dummybase;
	dummychan.id = &ob->id;
	dummychan.adt = ob->adt;

	ac.ads = ads;
	ac.data = &dummychan;
	ac.datatype = ANIMCONT_CHANNEL;

	/* get F-Curves to take keyframes from */
	filter = ANIMFILTER_DATA_VISIBLE; // curves only
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

	/* loop through each F-Curve, grabbing the keyframes */
	for (ale = anim_data.first; ale; ale = ale->next)
		fcurve_to_keylist(ale->adt, ale->data, keys);

	ANIM_animdata_freelist(&anim_data);
}

void cachefile_to_keylist(bDopeSheet *ads, CacheFile *cache_file, DLRBT_Tree *keys)
{
	if (cache_file == NULL) {
		return;
	}

	/* create a dummy wrapper data to work with */
	bAnimListElem dummychan = {NULL};
	dummychan.type = ANIMTYPE_DSCACHEFILE;
	dummychan.data = cache_file;
	dummychan.id = &cache_file->id;
	dummychan.adt = cache_file->adt;

	bAnimContext ac = {NULL};
	ac.ads = ads;
	ac.data = &dummychan;
	ac.datatype = ANIMCONT_CHANNEL;

	/* get F-Curves to take keyframes from */
	ListBase anim_data = { NULL, NULL };
	int filter = ANIMFILTER_DATA_VISIBLE; // curves only
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

	/* loop through each F-Curve, grabbing the keyframes */
	for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
		fcurve_to_keylist(ale->adt, ale->data, keys);
	}

	ANIM_animdata_freelist(&anim_data);
}

void fcurve_to_keylist(AnimData *adt, FCurve *fcu, DLRBT_Tree *keys)
{
	BezTriple *bezt;
	unsigned int v;

	if (fcu && fcu->totvert && fcu->bezt) {
		/* apply NLA-mapping (if applicable) */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 0);

		/* loop through beztriples, making ActKeysColumns */
		for (v = 0, bezt = fcu->bezt; v < fcu->totvert; v++, bezt++) {
			add_bezt_to_keycolumns_list(keys, bezt);
		}

		/* Update keyblocks. */
		update_keyblocks(keys, fcu->bezt, fcu->totvert);

		/* unapply NLA-mapping if applicable */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
	}
}

void agroup_to_keylist(AnimData *adt, bActionGroup *agrp, DLRBT_Tree *keys)
{
	FCurve *fcu;

	if (agrp) {
		/* loop through F-Curves */
		for (fcu = agrp->channels.first; fcu && fcu->grp == agrp; fcu = fcu->next) {
			fcurve_to_keylist(adt, fcu, keys);
		}
	}
}

void action_to_keylist(AnimData *adt, bAction *act, DLRBT_Tree *keys)
{
	FCurve *fcu;

	if (act) {
		/* loop through F-Curves */
		for (fcu = act->curves.first; fcu; fcu = fcu->next) {
			fcurve_to_keylist(adt, fcu, keys);
		}
	}
}


void gpencil_to_keylist(bDopeSheet *ads, bGPdata *gpd, DLRBT_Tree *keys, const bool active)
{
	bGPDlayer *gpl;

	if (gpd && keys) {
		/* for now, just aggregate out all the frames, but only for visible layers */
		for (gpl = gpd->layers.last; gpl; gpl = gpl->prev) {
			if ((gpl->flag & GP_LAYER_HIDE) == 0) {
				if ((!active) || ((active) && (gpl->flag & GP_LAYER_SELECT))) {
					gpl_to_keylist(ads, gpl, keys);
				}
			}
		}
	}
}

void gpl_to_keylist(bDopeSheet *UNUSED(ads), bGPDlayer *gpl, DLRBT_Tree *keys)
{
	bGPDframe *gpf;

	if (gpl && keys) {
		/* although the frames should already be in an ordered list, they are not suitable for displaying yet */
		for (gpf = gpl->frames.first; gpf; gpf = gpf->next)
			add_gpframe_to_keycolumns_list(keys, gpf);

		update_keyblocks(keys, NULL, 0);
	}
}

void mask_to_keylist(bDopeSheet *UNUSED(ads), MaskLayer *masklay, DLRBT_Tree *keys)
{
	MaskLayerShape *masklay_shape;

	if (masklay && keys) {
		for (masklay_shape = masklay->splines_shapes.first;
		     masklay_shape;
		     masklay_shape = masklay_shape->next)
		{
			add_masklay_to_keycolumns_list(keys, masklay_shape);
		}

		update_keyblocks(keys, NULL, 0);
	}
}
