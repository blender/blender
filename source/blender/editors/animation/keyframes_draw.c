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

/* Comparator callback used for ActKeyColumns and cframe float-value pointer */
/* NOTE: this is exported to other modules that use the ActKeyColumns for finding keyframes */
short compare_ak_cfraPtr(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	const float *cframe = data;
	float val = *cframe;

	if (IS_EQT(val, ak->cfra, BEZT_BINARYSEARCH_THRESH))
		return 0;

	if (val < ak->cfra)
		return -1;
	else if (val > ak->cfra)
		return 1;
	else
		return 0;
}

/* --------------- */

/* Comparator callback used for ActKeyColumns and BezTriple */
static short compare_ak_bezt(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	BezTriple *bezt = (BezTriple *)data;

	if (bezt->vec[1][0] < ak->cfra)
		return -1;
	else if (bezt->vec[1][0] > ak->cfra)
		return 1;
	else
		return 0;
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

	/* set 'modified', since this is used to identify long keyframes */
	ak->modified = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from BezTriples */
static void nupdate_ak_bezt(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	BezTriple *bezt = (BezTriple *)data;

	/* set selection status and 'touched' status */
	if (BEZT_ISSEL_ANY(bezt)) ak->sel = SELECT;
	ak->modified += 1;

	/* for keyframe type, 'proper' keyframes have priority over breakdowns (and other types for now) */
	if (BEZKEYTYPE(bezt) == BEZT_KEYTYPE_KEYFRAME)
		ak->key_type = BEZT_KEYTYPE_KEYFRAME;
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_gpframe(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	bGPDframe *gpf = (bGPDframe *)data;

	if (gpf->framenum < ak->cfra)
		return -1;
	else if (gpf->framenum > ak->cfra)
		return 1;
	else
		return 0;
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

	/* set 'modified', since this is used to identify long keyframes */
	ak->modified = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_gpframe(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	bGPDframe *gpf = (bGPDframe *)data;

	/* set selection status and 'touched' status */
	if (gpf->flag & GP_FRAME_SELECT) ak->sel = SELECT;
	ak->modified += 1;

	/* for keyframe type, 'proper' keyframes have priority over breakdowns (and other types for now) */
	if (gpf->key_type == BEZT_KEYTYPE_KEYFRAME)
		ak->key_type = BEZT_KEYTYPE_KEYFRAME;
}

/* ......... */

/* Comparator callback used for ActKeyColumns and GPencil frame */
static short compare_ak_masklayshape(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	if (masklay_shape->frame < ak->cfra)
		return -1;
	else if (masklay_shape->frame > ak->cfra)
		return 1;
	else
		return 0;
}

/* New node callback used for building ActKeyColumns from GPencil frames */
static DLRBT_Node *nalloc_ak_masklayshape(void *data)
{
	ActKeyColumn *ak = MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumnGPF");
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	/* store settings based on state of BezTriple */
	ak->cfra = masklay_shape->frame;
	ak->sel = (masklay_shape->flag & MASK_SHAPE_SELECT) ? SELECT : 0;

	/* set 'modified', since this is used to identify long keyframes */
	ak->modified = 1;

	return (DLRBT_Node *)ak;
}

/* Node updater callback used for building ActKeyColumns from GPencil frames */
static void nupdate_ak_masklayshape(void *node, void *data)
{
	ActKeyColumn *ak = (ActKeyColumn *)node;
	MaskLayerShape *masklay_shape = (MaskLayerShape *)data;

	/* set selection status and 'touched' status */
	if (masklay_shape->flag & MASK_SHAPE_SELECT) ak->sel = SELECT;
	ak->modified += 1;
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

/* Comparator callback used for ActKeyBlock and cframe float-value pointer */
/* NOTE: this is exported to other modules that use the ActKeyBlocks for finding long-keyframes */
short compare_ab_cfraPtr(void *node, void *data)
{
	ActKeyBlock *ab = (ActKeyBlock *)node;
	const float *cframe = data;
	float val = *cframe;

	if (val < ab->start)
		return -1;
	else if (val > ab->start)
		return 1;
	else
		return 0;
}

/* --------------- */

/* Create a ActKeyColumn for a pair of BezTriples */
static ActKeyBlock *bezts_to_new_actkeyblock(BezTriple *prev, BezTriple *beztn)
{
	ActKeyBlock *ab = MEM_callocN(sizeof(ActKeyBlock), "ActKeyBlock");

	ab->start = prev->vec[1][0];
	ab->end = beztn->vec[1][0];
	ab->val = beztn->vec[1][1];

	ab->sel = (BEZT_ISSEL_ANY(prev) || BEZT_ISSEL_ANY(beztn)) ? SELECT : 0;
	ab->modified = 1;

	if (BEZKEYTYPE(beztn) == BEZT_KEYTYPE_MOVEHOLD)
		ab->flag |= ACTKEYBLOCK_FLAG_MOVING_HOLD;

	return ab;
}

static void add_bezt_to_keyblocks_list(DLRBT_Tree *blocks, BezTriple *first_bezt, BezTriple *beztn)
{
	ActKeyBlock *new_ab = NULL;
	BezTriple *prev = NULL;

	/* get the BezTriple immediately before the given one which has the same value */
	if (beztn != first_bezt) {
		/* XXX: Unless I'm overlooking some details from the past, this should be sufficient?
		 *      The old code did some elaborate stuff trying to find keyframe columns for
		 *      the given BezTriple, then step backwards to the column before that, and find
		 *      an appropriate BezTriple with matching values there. Maybe that was warranted
		 *      in the past, but now, that list is only ever filled with keyframes from the
		 *      current FCurve.
		 *
		 *      -- Aligorith (20140415)
		 */
		prev = beztn - 1;
	}


	/* check if block needed */
	if (prev == NULL) return;

	if (BEZKEYTYPE(beztn) == BEZT_KEYTYPE_MOVEHOLD) {
		/* Animator tagged a "moving hold"
		 *   - Previous key must also be tagged as a moving hold, otherwise
		 *     we're just dealing with the first of a pair, and we don't
		 *     want to be creating any phantom holds...
		 */
		if (BEZKEYTYPE(prev) != BEZT_KEYTYPE_MOVEHOLD)
			return;
	}
	else {
		/* Check for same values...
		 *  - Handles must have same central value as each other
		 *  - Handles which control that section of the curve must be constant
		 */
		if (IS_EQF(beztn->vec[1][1], prev->vec[1][1]) == 0) return;

		if (IS_EQF(beztn->vec[1][1], beztn->vec[0][1]) == 0) return;
		if (IS_EQF(prev->vec[1][1], prev->vec[2][1]) == 0) return;
	}

	/* if there are no blocks already, just add as root */
	if (blocks->root == NULL) {
		/* just add this as the root, then call the tree-balancing functions to validate */
		new_ab = bezts_to_new_actkeyblock(prev, beztn);
		blocks->root = (DLRBT_Node *)new_ab;
	}
	else {
		ActKeyBlock *ab, *abn = NULL;

		/* try to find a keyblock that starts on the previous beztriple, and add a new one if none start there
		 * Note: we perform a tree traversal here NOT a standard linked-list traversal...
		 * Note: we can't search from end to try to optimize this as it causes errors there's
		 *      an A ___ B |---| B situation
		 */
		// FIXME: here there is a bug where we are trying to get the summary for the following channels
		//		A|--------------|A ______________ B|--------------|B
		//		A|------------------------------------------------|A
		//		A|----|A|---|A|-----------------------------------|A
		for (ab = blocks->root; ab; ab = abn) {
			/* check if this is a match, or whether we go left or right
			 * NOTE: we now use a float threshold to prevent precision errors causing problems with summaries
			 */
			if (IS_EQT(ab->start, prev->vec[1][0], BEZT_BINARYSEARCH_THRESH)) {
				/* set selection status and 'touched' status */
				if (BEZT_ISSEL_ANY(beztn))
					ab->sel = SELECT;

				/* XXX: only when the first one was a moving hold? */
				if (BEZKEYTYPE(beztn) == BEZT_KEYTYPE_MOVEHOLD)
					ab->flag |= ACTKEYBLOCK_FLAG_MOVING_HOLD;

				ab->modified++;

				/* done... no need to insert */
				return;
			}
			else {
				ActKeyBlock **abnp = NULL; /* branch to go down - used to hook new blocks to parents */

				/* check if go left or right, but if not available, add new node */
				if (ab->start < prev->vec[1][0])
					abnp = &ab->right;
				else
					abnp = &ab->left;

				/* if this does not exist, add a new node, otherwise continue... */
				if (*abnp == NULL) {
					/* add a new node representing this, and attach it to the relevant place */
					new_ab = bezts_to_new_actkeyblock(prev, beztn);
					new_ab->parent = ab;
					*abnp = new_ab;
					break;
				}
				else
					abn = *abnp;
			}
		}
	}

	/* now, balance the tree taking into account this newly added node */
	BLI_dlrbTree_insert(blocks, (DLRBT_Node *)new_ab);
}

/* --------- */

/* Handle the 'touched' status of ActKeyColumn tree nodes */
static void set_touched_actkeycolumn(ActKeyColumn *ak)
{
	/* sanity check */
	if (ak == NULL)
		return;

	/* deal with self first */
	if (ak->modified) {
		ak->modified = 0;
		ak->totcurve++;
	}

	/* children */
	set_touched_actkeycolumn(ak->left);
	set_touched_actkeycolumn(ak->right);
}

/* Handle the 'touched' status of ActKeyBlock tree nodes */
static void set_touched_actkeyblock(ActKeyBlock *ab)
{
	/* sanity check */
	if (ab == NULL)
		return;

	/* deal with self first */
	if (ab->modified) {
		ab->modified = 0;
		ab->totcurve++;
	}

	/* children */
	set_touched_actkeyblock(ab->left);
	set_touched_actkeyblock(ab->right);
}

/* --------- */

/* Checks if ActKeyBlock should exist... */
bool actkeyblock_is_valid(ActKeyBlock *ab, DLRBT_Tree *keys)
{
	ActKeyColumn *ak;
	short startCurves, endCurves, totCurves;

	/* check that block is valid */
	if (ab == NULL)
		return 0;

	/* find out how many curves occur at each keyframe */
	ak = (ActKeyColumn *)BLI_dlrbTree_search_exact(keys, compare_ak_cfraPtr, &ab->start);
	startCurves = (ak) ? ak->totcurve : 0;

	ak = (ActKeyColumn *)BLI_dlrbTree_search_exact(keys, compare_ak_cfraPtr, &ab->end);
	endCurves = (ak) ? ak->totcurve : 0;

	/* only draw keyblock if it appears in at all of the keyframes at lowest end */
	if (!startCurves && !endCurves)
		return 0;

	totCurves = (startCurves > endCurves) ? endCurves : startCurves;
	return (ab->totcurve >= totCurves);
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
				/* XXX: Should these get their own theme options instead? */
				if (sel) UI_GetThemeColorShade3ubv(TH_STRIP_SELECT, 35, fill_col);
				else UI_GetThemeColorShade3ubv(TH_STRIP, 50, fill_col);
				fill_col[3] = 255; /* full opacity, to avoid problems with visual glitches */
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

	immAttrib1f(size_id, size);
	immAttrib4ubv(color_id, fill_col);
	immAttrib4ubv(outline_color_id, outline_col);
	immVertex2f(pos_id, x, y);
}

static void draw_keylist(View2D *v2d, DLRBT_Tree *keys, DLRBT_Tree *blocks, float ypos, float yscale_fac, bool channelLocked)
{
	const float icon_sz = U.widget_unit * 0.5f * yscale_fac;
	const float half_icon_sz = 0.5f * icon_sz;

	GPU_blend(true);

	/* locked channels are less strongly shown, as feedback for locked channels in DopeSheet */
	/* TODO: allow this opacity factor to be themed? */
	float alpha = channelLocked ? 0.25f : 1.0f;

	/* draw keyblocks */
	if (blocks) {
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
		for (ActKeyBlock *ab = blocks->first; ab; ab = ab->next) {
			if (actkeyblock_is_valid(ab, keys)) {
				block_len++;
			}
		}

		if (block_len > 0) {
			GPUVertFormat *format = immVertexFormat();
			uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			uint color_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

			immBegin(GPU_PRIM_TRIS, 6 * block_len);
			for (ActKeyBlock *ab = blocks->first; ab; ab = ab->next) {
				if (actkeyblock_is_valid(ab, keys)) {
					if (ab->flag & ACTKEYBLOCK_FLAG_MOVING_HOLD) {
						/* draw "moving hold" long-keyframe block - slightly smaller */
						immRectf_fast_with_color(pos_id, color_id,
						                         ab->start, ypos - half_icon_sz, ab->end, ypos + half_icon_sz,
						                         (ab->sel) ? sel_mhcol : unsel_mhcol);
					}
					else {
						/* draw standard long-keyframe block */
						immRectf_fast_with_color(pos_id, color_id,
						                         ab->start, ypos - half_icon_sz, ab->end, ypos + half_icon_sz,
						                         (ab->sel) ? sel_color : unsel_color);
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
	DLRBT_Tree keys, blocks;

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	summary_to_keylist(ac, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_scene_channel(View2D *v2d, bDopeSheet *ads, Scene *sce, float ypos, float yscale_fac)
{
	DLRBT_Tree keys, blocks;

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	scene_to_keylist(ads, sce, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_object_channel(View2D *v2d, bDopeSheet *ads, Object *ob, float ypos, float yscale_fac)
{
	DLRBT_Tree keys, blocks;

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	ob_to_keylist(ads, ob, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_fcurve_channel(View2D *v2d, AnimData *adt, FCurve *fcu, float ypos, float yscale_fac)
{
	DLRBT_Tree keys, blocks;

	bool locked = (fcu->flag & FCURVE_PROTECTED) ||
	              ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
	              ((adt && adt->action) && ID_IS_LINKED(adt->action));

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	fcurve_to_keylist(adt, fcu, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_agroup_channel(View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos, float yscale_fac)
{
	DLRBT_Tree keys, blocks;

	bool locked = (agrp->flag & AGRP_PROTECTED) ||
	              ((adt && adt->action) && ID_IS_LINKED(adt->action));

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	agroup_to_keylist(adt, agrp, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_action_channel(View2D *v2d, AnimData *adt, bAction *act, float ypos, float yscale_fac)
{
	DLRBT_Tree keys, blocks;

	bool locked = (act && ID_IS_LINKED(act));

	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);

	action_to_keylist(adt, act, &keys, &blocks);

	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);

	draw_keylist(v2d, &keys, &blocks, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_gpencil_channel(View2D *v2d, bDopeSheet *ads, bGPdata *gpd, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	gpencil_to_keylist(ads, gpd, &keys);

	BLI_dlrbTree_linkedlist_sync(&keys);

	draw_keylist(v2d, &keys, NULL, ypos, yscale_fac, false);

	BLI_dlrbTree_free(&keys);
}

void draw_gpl_channel(View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (gpl->flag & GP_LAYER_LOCKED) != 0;

	BLI_dlrbTree_init(&keys);

	gpl_to_keylist(ads, gpl, &keys);

	BLI_dlrbTree_linkedlist_sync(&keys);

	draw_keylist(v2d, &keys, NULL, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

void draw_masklay_channel(View2D *v2d, bDopeSheet *ads, MaskLayer *masklay, float ypos, float yscale_fac)
{
	DLRBT_Tree keys;

	bool locked = (masklay->flag & MASK_LAYERFLAG_LOCKED) != 0;

	BLI_dlrbTree_init(&keys);

	mask_to_keylist(ads, masklay, &keys);

	BLI_dlrbTree_linkedlist_sync(&keys);

	draw_keylist(v2d, &keys, NULL, ypos, yscale_fac, locked);

	BLI_dlrbTree_free(&keys);
}

/* *************************** Keyframe List Conversions *************************** */

void summary_to_keylist(bAnimContext *ac, DLRBT_Tree *keys, DLRBT_Tree *blocks)
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
					fcurve_to_keylist(ale->adt, ale->data, keys, blocks);
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

void scene_to_keylist(bDopeSheet *ads, Scene *sce, DLRBT_Tree *keys, DLRBT_Tree *blocks)
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
		fcurve_to_keylist(ale->adt, ale->data, keys, blocks);

	ANIM_animdata_freelist(&anim_data);
}

void ob_to_keylist(bDopeSheet *ads, Object *ob, DLRBT_Tree *keys, DLRBT_Tree *blocks)
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
		fcurve_to_keylist(ale->adt, ale->data, keys, blocks);

	ANIM_animdata_freelist(&anim_data);
}

void cachefile_to_keylist(bDopeSheet *ads, CacheFile *cache_file, DLRBT_Tree *keys, DLRBT_Tree *blocks)
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
		fcurve_to_keylist(ale->adt, ale->data, keys, blocks);
	}

	ANIM_animdata_freelist(&anim_data);
}

void fcurve_to_keylist(AnimData *adt, FCurve *fcu, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	BezTriple *bezt;
	unsigned int v;

	if (fcu && fcu->totvert && fcu->bezt) {
		/* apply NLA-mapping (if applicable) */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 0);

		/* loop through beztriples, making ActKeysColumns and ActKeyBlocks */
		for (v = 0, bezt = fcu->bezt; v < fcu->totvert; v++, bezt++) {
			add_bezt_to_keycolumns_list(keys, bezt);
			if (blocks) add_bezt_to_keyblocks_list(blocks, fcu->bezt, bezt);
		}

		/* update the number of curves that elements have appeared in  */
		if (keys)
			set_touched_actkeycolumn(keys->root);
		if (blocks)
			set_touched_actkeyblock(blocks->root);

		/* unapply NLA-mapping if applicable */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
	}
}

void agroup_to_keylist(AnimData *adt, bActionGroup *agrp, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	FCurve *fcu;

	if (agrp) {
		/* loop through F-Curves */
		for (fcu = agrp->channels.first; fcu && fcu->grp == agrp; fcu = fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}

void action_to_keylist(AnimData *adt, bAction *act, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	FCurve *fcu;

	if (act) {
		/* loop through F-Curves */
		for (fcu = act->curves.first; fcu; fcu = fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}


void gpencil_to_keylist(bDopeSheet *ads, bGPdata *gpd, DLRBT_Tree *keys)
{
	bGPDlayer *gpl;

	if (gpd && keys) {
		/* for now, just aggregate out all the frames, but only for visible layers */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			if ((gpl->flag & GP_LAYER_HIDE) == 0) {
				gpl_to_keylist(ads, gpl, keys);
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
	}
}
