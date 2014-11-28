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
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"

#include "BKE_fcurve.h"

#include "BIF_gl.h"

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
	ak->sel = BEZSELECTED(bezt) ? SELECT : 0;
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
	if (BEZSELECTED(bezt)) ak->sel = SELECT;
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
	
	ab->sel = (BEZSELECTED(prev) || BEZSELECTED(beztn)) ? SELECT : 0;
	ab->modified = 1;
	
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
	
	
	/* check if block needed - same value(s)?
	 *	-> firstly, handles must have same central value as each other
	 *	-> secondly, handles which control that section of the curve must be constant
	 */
	if (prev == NULL) return;
	if (IS_EQF(beztn->vec[1][1], prev->vec[1][1]) == 0) return;
	
	if (IS_EQF(beztn->vec[1][1], beztn->vec[0][1]) == 0) return;
	if (IS_EQF(prev->vec[1][1], prev->vec[2][1]) == 0) return;
	
	
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
				if (BEZSELECTED(beztn)) ab->sel = SELECT;
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

/* coordinates for diamond shape */
static const float _unit_diamond_shape[4][2] = {
	{0.0f, 1.0f},   /* top vert */
	{1.0f, 0.0f},   /* mid-right */
	{0.0f, -1.0f},  /* bottom vert */
	{-1.0f, 0.0f}   /* mid-left */
}; 

/* draw a simple diamond shape with OpenGL */
void draw_keyframe_shape(float x, float y, float xscale, float hsize, short sel, short key_type, short mode, float alpha)
{
	static GLuint displist1 = 0;
	static GLuint displist2 = 0;
	
	/* initialize 2 display lists for diamond shape - one empty, one filled */
	if (displist1 == 0) {
		displist1 = glGenLists(1);
		glNewList(displist1, GL_COMPILE);
			
		glBegin(GL_LINE_LOOP);
		glVertex2fv(_unit_diamond_shape[0]);
		glVertex2fv(_unit_diamond_shape[1]);
		glVertex2fv(_unit_diamond_shape[2]);
		glVertex2fv(_unit_diamond_shape[3]);
		glEnd();

		glEndList();
	}
	if (displist2 == 0) {
		displist2 = glGenLists(1);
		glNewList(displist2, GL_COMPILE);
			
		glBegin(GL_QUADS);
		glVertex2fv(_unit_diamond_shape[0]);
		glVertex2fv(_unit_diamond_shape[1]);
		glVertex2fv(_unit_diamond_shape[2]);
		glVertex2fv(_unit_diamond_shape[3]);
		glEnd();

		glEndList();
	}
	
	/* tweak size of keyframe shape according to type of keyframe 
	 * - 'proper' keyframes have key_type = 0, so get drawn at full size
	 */
	hsize -= 0.5f * key_type;
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f / xscale * hsize, hsize, 1.0f);
	
	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);
	
	/* draw! */
	if (ELEM(mode, KEYFRAME_SHAPE_INSIDE, KEYFRAME_SHAPE_BOTH)) {
		float inner_col[4];
		
		/* get interior colors from theme (for selected and unselected only) */
		switch (key_type) {
			case BEZT_KEYTYPE_BREAKDOWN: /* bluish frames (default theme) */
			{
				if (sel)  UI_GetThemeColor4fv(TH_KEYTYPE_BREAKDOWN_SELECT, inner_col);
				else UI_GetThemeColor4fv(TH_KEYTYPE_BREAKDOWN, inner_col);
				break;
			}
			case BEZT_KEYTYPE_EXTREME: /* reddish frames (default theme) */
			{
				if (sel) UI_GetThemeColor4fv(TH_KEYTYPE_EXTREME_SELECT, inner_col);
				else UI_GetThemeColor4fv(TH_KEYTYPE_EXTREME, inner_col);
				break;
			}
			case BEZT_KEYTYPE_JITTER: /* greenish frames (default theme) */
			{
				if (sel) UI_GetThemeColor4fv(TH_KEYTYPE_JITTER_SELECT, inner_col);
				else UI_GetThemeColor4fv(TH_KEYTYPE_JITTER, inner_col);
				break;
			}
			case BEZT_KEYTYPE_KEYFRAME: /* traditional yellowish frames (default theme) */
			default:
			{
				if (sel) UI_GetThemeColor4fv(TH_KEYTYPE_KEYFRAME_SELECT, inner_col);
				else UI_GetThemeColor4fv(TH_KEYTYPE_KEYFRAME, inner_col);
				break;
			}
		}
		
		/* NOTE: we don't use the straight alpha from the theme, or else effects such as 
		 * greying out protected/muted channels doesn't work correctly! 
		 */
		inner_col[3] *= alpha;
		glColor4fv(inner_col);
		
		/* draw the "filled in" interior poly now */
		glCallList(displist2);
	}
	
	if (ELEM(mode, KEYFRAME_SHAPE_FRAME, KEYFRAME_SHAPE_BOTH)) {
		float border_col[4];
		
		/* exterior - black frame */
		if (sel)  UI_GetThemeColor4fv(TH_KEYBORDER_SELECT, border_col);
		else  UI_GetThemeColor4fv(TH_KEYBORDER, border_col);
		
		border_col[3] *= alpha;
		glColor4fv(border_col);
		
		glCallList(displist1);
	}
	
	glDisable(GL_LINE_SMOOTH);
	
	/* restore view transform */
	glScalef(xscale / hsize, 1.0f / hsize, 1.0f);
	glTranslatef(-x, -y, 0.0f);
}

static void draw_keylist(View2D *v2d, DLRBT_Tree *keys, DLRBT_Tree *blocks, float ypos, short channelLocked)
{
	ActKeyColumn *ak;
	ActKeyBlock *ab;
	float alpha;
	float xscale;
	float iconsize = U.widget_unit / 4.0f;
	glEnable(GL_BLEND);
	
	/* get View2D scaling factor */
	UI_view2d_scale_get(v2d, &xscale, NULL);
	
	/* locked channels are less strongly shown, as feedback for locked channels in DopeSheet */
	/* TODO: allow this opacity factor to be themed? */
	alpha = (channelLocked) ? 0.25f : 1.0f;
	
	/* draw keyblocks */
	if (blocks) {
		float sel_color[4], unsel_color[4];
		
		/* cache colours first */
		UI_GetThemeColor4fv(TH_STRIP_SELECT, sel_color);
		UI_GetThemeColor4fv(TH_STRIP, unsel_color);
		
		sel_color[3]   *= alpha;
		unsel_color[3] *= alpha;
		
		/* NOTE: the tradeoff for changing colors between each draw is dwarfed by the cost of checking validity */
		for (ab = blocks->first; ab; ab = ab->next) {
			if (actkeyblock_is_valid(ab, keys)) {
				/* draw block */
				if (ab->sel)
					glColor4fv(sel_color);
				else
					glColor4fv(unsel_color);
				
				glRectf(ab->start, ypos - iconsize, ab->end, ypos + iconsize);
			}
		}
	}
	
	/* draw keys */
	if (keys) {
		for (ak = keys->first; ak; ak = ak->next) {
			/* optimization: if keyframe doesn't appear within 5 units (screenspace) in visible area, don't draw
			 *	- this might give some improvements, since we current have to flip between view/region matrices
			 */
			if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax) == 0)
				continue;
			
			/* draw using OpenGL - uglier but faster */
			/* NOTE1: a previous version of this didn't work nice for some intel cards
			 * NOTE2: if we wanted to go back to icons, these are  icon = (ak->sel & SELECT) ? ICON_SPACE2 : ICON_SPACE3; */
			draw_keyframe_shape(ak->cfra, ypos, xscale, iconsize, (ak->sel & SELECT), ak->key_type, KEYFRAME_SHAPE_BOTH, alpha);
		}
	}

	glDisable(GL_BLEND);
}

/* *************************** Channel Drawing Funcs *************************** */

void draw_summary_channel(View2D *v2d, bAnimContext *ac, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	summary_to_keylist(ac, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, 0);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_scene_channel(View2D *v2d, bDopeSheet *ads, Scene *sce, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	scene_to_keylist(ads, sce, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, 0);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_object_channel(View2D *v2d, bDopeSheet *ads, Object *ob, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	ob_to_keylist(ads, ob, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, 0);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_fcurve_channel(View2D *v2d, AnimData *adt, FCurve *fcu, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	short locked = (fcu->flag & FCURVE_PROTECTED) ||
	               ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) ||
	               ((adt && adt->action) && (adt->action->id.lib));
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	fcurve_to_keylist(adt, fcu, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, locked);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_agroup_channel(View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	short locked = (agrp->flag & AGRP_PROTECTED) ||
	               ((adt && adt->action) && (adt->action->id.lib));
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	agroup_to_keylist(adt, agrp, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, locked);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_action_channel(View2D *v2d, AnimData *adt, bAction *act, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	short locked = (act && act->id.lib != NULL);
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
	action_to_keylist(adt, act, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
	draw_keylist(v2d, &keys, &blocks, ypos, locked);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_gpl_channel(View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos)
{
	DLRBT_Tree keys;
	
	BLI_dlrbTree_init(&keys);
	
	gpl_to_keylist(ads, gpl, &keys);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	
	draw_keylist(v2d, &keys, NULL, ypos, (gpl->flag & GP_LAYER_LOCKED));
	
	BLI_dlrbTree_free(&keys);
}

void draw_masklay_channel(View2D *v2d, bDopeSheet *ads, MaskLayer *masklay, float ypos)
{
	DLRBT_Tree keys;

	BLI_dlrbTree_init(&keys);

	mask_to_keylist(ads, masklay, &keys);

	BLI_dlrbTree_linkedlist_sync(&keys);

	draw_keylist(v2d, &keys, NULL, ypos, (masklay->flag & MASK_LAYERFLAG_LOCKED));

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

