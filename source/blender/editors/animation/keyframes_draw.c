/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"
#include "DNA_view2d_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_global.h" 	// XXX remove me!
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_screen.h"
#include "ED_space_api.h"

/* *************************** Keyframe Processing *************************** */

/* Create a ActKeyColumn from a BezTriple */
static ActKeyColumn *bezt_to_new_actkeycolumn(BezTriple *bezt)
{
	ActKeyColumn *ak= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
	
	/* store settings based on state of BezTriple */
	ak->cfra= bezt->vec[1][0];
	ak->sel= BEZSELECTED(bezt) ? SELECT : 0;
	ak->key_type= BEZKEYTYPE(bezt); 
	
	/* set 'modified', since this is used to identify long keyframes */
	ak->modified = 1;
	
	return ak;
}

/* Add the given BezTriple to the given 'list' of Keyframes */
static void add_bezt_to_keycolumns_list(DLRBT_Tree *keys, BezTriple *bezt)
{
	ActKeyColumn *new_ak=NULL;
	
	if ELEM(NULL, keys, bezt) return;
	
	/* if there are no keys already, just add as root */
	if (keys->root == NULL) {
		/* just add this as the root, then call the tree-balancing functions to validate */
		new_ak= bezt_to_new_actkeycolumn(bezt);
		keys->root= (DLRBT_Node *)new_ak;
	}
	else {
		ActKeyColumn *ak, *akp=NULL, *akn=NULL;
		
		/* traverse tree to find an existing entry to update the status of, 
		 * or a suitable point to add at
		 */
		for (ak= keys->root; ak; akp= ak, ak= akn) {
			/* check if this is a match, or whether we go left or right */
			if (ak->cfra == bezt->vec[1][0]) {
				/* set selection status and 'touched' status */
				if (BEZSELECTED(bezt)) ak->sel = SELECT;
				ak->modified += 1;
				
				/* for keyframe type, 'proper' keyframes have priority over breakdowns (and other types for now) */
				if (BEZKEYTYPE(bezt) == BEZT_KEYTYPE_KEYFRAME)
					ak->key_type= BEZT_KEYTYPE_KEYFRAME;
				
				/* done... no need to insert */
				return;
			}
			else {
				ActKeyColumn **aknp= NULL; 
				
				/* check if go left or right, but if not available, add new node */
				if (ak->cfra < bezt->vec[1][0]) 
					aknp= &ak->right;
				else
					aknp= &ak->left;
					
				/* if this does not exist, add a new node, otherwise continue... */
				if (*aknp == NULL) {
					/* add a new node representing this, and attach it to the relevant place */
					new_ak= bezt_to_new_actkeycolumn(bezt);
					new_ak->parent= ak;
					*aknp= new_ak;
					break;
				}
				else
					akn= *aknp;
			}
		}
	}
	
	/* now, balance the tree taking into account this newly added node */
	BLI_dlrbTree_insert(keys, (DLRBT_Node *)new_ak);
}


/* Create a ActKeyColumn for a pair of BezTriples */
static ActKeyBlock *bezts_to_new_actkeyblock(BezTriple *prev, BezTriple *beztn)
{
	ActKeyBlock *ab= MEM_callocN(sizeof(ActKeyBlock), "ActKeyBlock");
	
	ab->start= prev->vec[1][0];
	ab->end= beztn->vec[1][0];
	ab->val= beztn->vec[1][1];
	
	ab->sel= (BEZSELECTED(prev) || BEZSELECTED(beztn)) ? SELECT : 0;
	ab->modified = 1;
	
	return ab;
}

static void add_bezt_to_keyblocks_list(DLRBT_Tree *blocks, FCurve *fcu, int index)
{
	ActKeyBlock *new_ab= NULL;
	BezTriple *beztn=NULL, *prev=NULL;
	BezTriple *bezt;
	int v;
	
	/* get beztriples */
	beztn= (fcu->bezt + index);
	
	/* we need to go through all beztriples, as they may not be in order (i.e. during transform) */
	// TODO: this seems to be a bit of a bottleneck
	for (v=0, bezt=fcu->bezt; v < fcu->totvert; v++, bezt++) {
		/* skip if beztriple is current */
		if (v != index) {
			/* check if beztriple is immediately before */
			if (beztn->vec[1][0] > bezt->vec[1][0]) {
				/* check if closer than previous was */
				if (prev) {
					if (prev->vec[1][0] < bezt->vec[1][0])
						prev= bezt;
				}
				else {
					prev= bezt;
				}
			}
		}
	}
	
	/* check if block needed - same value(s)?
	 *	-> firstly, handles must have same central value as each other
	 *	-> secondly, handles which control that section of the curve must be constant
	 */
	if ((!prev) || (!beztn)) return;
	if (IS_EQ(beztn->vec[1][1], prev->vec[1][1])==0) return;
	if (IS_EQ(beztn->vec[1][1], beztn->vec[0][1])==0) return;
	if (IS_EQ(prev->vec[1][1], prev->vec[2][1])==0) return;
	
	
	/* if there are no blocks already, just add as root */
	if (blocks->root == NULL) {
		/* just add this as the root, then call the tree-balancing functions to validate */
		new_ab= bezts_to_new_actkeyblock(prev, beztn);
		blocks->root= (DLRBT_Node *)new_ab;
	}
	else {
		ActKeyBlock *ab, *abp=NULL, *abn=NULL;
		
		/* try to find a keyblock that starts on the previous beztriple, and add a new one if none start there
		 * Note: we can't search from end to try to optimise this as it causes errors there's
		 * 		an A ___ B |---| B situation
		 */
		// FIXME: here there is a bug where we are trying to get the summary for the following channels
		//		A|--------------|A ______________ B|--------------|B
		//		A|------------------------------------------------|A
		//		A|----|A|---|A|-----------------------------------|A
		for (ab= blocks->root; ab; abp= ab, ab= abn) {
			/* check if this is a match, or whether we go left or right */
			if (ab->start == prev->vec[1][0]) {
				/* set selection status and 'touched' status */
				if (BEZSELECTED(beztn)) ab->sel = SELECT;
				ab->modified += 1;
				
				/* done... no need to insert */
				return;
			}
			else {
				ActKeyBlock **abnp= NULL; 
				
				/* check if go left or right, but if not available, add new node */
				if (ab->start < prev->vec[1][0]) 
					abnp= &ab->right;
				else
					abnp= &ab->left;
					
				/* if this does not exist, add a new node, otherwise continue... */
				if (*abnp == NULL) {
					/* add a new node representing this, and attach it to the relevant place */
					new_ab= bezts_to_new_actkeyblock(prev, beztn);
					new_ab->parent= ab;
					*abnp= new_ab;
					break;
				}
				else
					abn= *abnp;
			}
		}
	}
	
	/* now, balance the tree taking into account this newly added node */
	BLI_dlrbTree_insert(blocks, (DLRBT_Node *)new_ab);
}

/* --------- */

/* Handle the 'touched' status of ActKeyColumn tree nodes */
static void set_touched_actkeycolumn (ActKeyColumn *ak)
{
	/* sanity check */
	if (ak == NULL)
		return;
		
	/* deal with self first */
	if (ak->modified) {
		ak->modified= 0;
		ak->totcurve++;
	}
	
	/* children */
	set_touched_actkeycolumn(ak->left);
	set_touched_actkeycolumn(ak->right);
}

/* Handle the 'touched' status of ActKeyBlock tree nodes */
static void set_touched_actkeyblock (ActKeyBlock *ab)
{
	/* sanity check */
	if (ab == NULL)
		return;
		
	/* deal with self first */
	if (ab->modified) {
		ab->modified= 0;
		ab->totcurve++;
	}
	
	/* children */
	set_touched_actkeyblock(ab->left);
	set_touched_actkeyblock(ab->right);
}

/* *************************** Keyframe Drawing *************************** */

/* helper function - find actkeycolumn that occurs on cframe */
ActKeyColumn *cfra_find_actkeycolumn (ActKeyColumn *ak, float cframe)
{
	/* sanity checks */
	if (ak == NULL)
		return NULL;
	
	/* check if this is a match, or whether it is in some subtree */
	if (cframe < ak->cfra)
		return cfra_find_actkeycolumn(ak->left, cframe);
	else if (cframe > ak->cfra)
		return cfra_find_actkeycolumn(ak->right, cframe);
	else
		return ak; /* match */
}

/* helper function - find actkeycolumn that occurs on cframe, or the nearest one if not found */
// FIXME: this is buggy... next() is ignored completely...
ActKeyColumn *cfra_find_nearest_next_ak (ActKeyColumn *ak, float cframe, short next)
{
	ActKeyColumn *akn= NULL;
	
	/* sanity checks */
	if (ak == NULL)
		return NULL;
	
	/* check if this is a match, or whether it is in some subtree */
	if (cframe < ak->cfra)
		akn= cfra_find_nearest_next_ak(ak->left, cframe, next);
	else if (cframe > ak->cfra)
		akn= cfra_find_nearest_next_ak(ak->right, cframe, next);
		
	/* if no match found (or found match), just use the current one */
	if (akn == NULL)
		return ak;
	else
		return akn;
}

/* -------- */

/* coordinates for diamond shape */
static const float _unit_diamond_shape[4][2] = {
	{0.0f, 1.0f},	/* top vert */
	{1.0f, 0.0f},	/* mid-right */
	{0.0f, -1.0f},	/* bottom vert */
	{-1.0f, 0.0f}	/* mid-left */
}; 

/* draw a simple diamond shape with OpenGL */
void draw_keyframe_shape (float x, float y, float xscale, float hsize, short sel, short key_type, short mode)
{
	static GLuint displist1=0;
	static GLuint displist2=0;
	
	/* initialise 2 display lists for diamond shape - one empty, one filled */
	if (displist1 == 0) {
		displist1= glGenLists(1);
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
		displist2= glGenLists(1);
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
	 * 	- 'proper' keyframes have key_type=0, so get drawn at full size
	 */
	hsize -= 0.5f*key_type;
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f/xscale*hsize, hsize, 1.0f);
	
	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);
	
	/* draw! */
	if ELEM(mode, KEYFRAME_SHAPE_INSIDE, KEYFRAME_SHAPE_BOTH) {
		/* interior - hardcoded colors (for selected and unselected only) */
		switch (key_type) {
			case BEZT_KEYTYPE_BREAKDOWN: /* bluish frames for now */
			{
				if (sel) glColor3f(0.33f, 0.75f, 0.93f);
				else glColor3f(0.70f, 0.86f, 0.91f);
			}
				break;
				
			case BEZT_KEYTYPE_EXTREME: /* redish frames for now */
			{
				if (sel) glColor3f(95.0f, 0.5f, 0.5f);
				else glColor3f(0.91f, 0.70f, 0.80f);
			}
				break;
				
			case BEZT_KEYTYPE_KEYFRAME: /* traditional yellowish frames for now */
			default:
			{
				if (sel) UI_ThemeColorShade(TH_STRIP_SELECT, 50);
				else glColor3f(0.91f, 0.91f, 0.91f);
			}
				break;
		}
		
		glCallList(displist2);
	}
	
	if ELEM(mode, KEYFRAME_SHAPE_FRAME, KEYFRAME_SHAPE_BOTH) {
		/* exterior - black frame */
		glColor3ub(0, 0, 0);
		
		glCallList(displist1);
	}
	
	glDisable(GL_LINE_SMOOTH);
	
	/* restore view transform */
	glScalef(xscale/hsize, 1.0f/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

static void draw_keylist(View2D *v2d, DLRBT_Tree *keys, DLRBT_Tree *blocks, float ypos)
{
	ActKeyColumn *ak;
	ActKeyBlock *ab;
	float xscale;
	
	glEnable(GL_BLEND);
	
	/* get View2D scaling factor */
	UI_view2d_getscale(v2d, &xscale, NULL);
	
	/* draw keyblocks */
	if (blocks) {
		for (ab= blocks->first; ab; ab= ab->next) {
			short startCurves, endCurves, totCurves;
			
			/* find out how many curves occur at each keyframe */
			ak= cfra_find_actkeycolumn((ActKeyColumn *)keys->root, ab->start);
			startCurves = (ak)? ak->totcurve: 0;
			
			ak= cfra_find_actkeycolumn((ActKeyColumn *)keys->root, ab->end);
			endCurves = (ak)? ak->totcurve: 0;
			
			/* only draw keyblock if it appears in at all of the keyframes at lowest end */
			if (!startCurves && !endCurves) 
				continue;
			else
				totCurves = (startCurves>endCurves)? endCurves: startCurves;
				
			if (ab->totcurve >= totCurves) {
				/* draw block */
				if (ab->sel)
					UI_ThemeColor4(TH_STRIP_SELECT);
				else
					UI_ThemeColor4(TH_STRIP);
				
				glRectf(ab->start, ypos-5, ab->end, ypos+5);
			}
		}
	}
	
	/* draw keys */
	if (keys) {
		for (ak= keys->first; ak; ak= ak->next) {
			/* optimisation: if keyframe doesn't appear within 5 units (screenspace) in visible area, don't draw 
			 *	- this might give some improvements, since we current have to flip between view/region matrices
			 */
			if (IN_RANGE_INCL(ak->cfra, v2d->cur.xmin, v2d->cur.xmax) == 0)
				continue;
			
			/* draw using OpenGL - uglier but faster */
			// NOTE1: a previous version of this didn't work nice for some intel cards
			// NOTE2: if we wanted to go back to icons, these are  icon = (ak->sel & SELECT) ? ICON_SPACE2 : ICON_SPACE3;
			draw_keyframe_shape(ak->cfra, ypos, xscale, 5.0f, (ak->sel & SELECT), ak->key_type, KEYFRAME_SHAPE_BOTH);
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
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
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
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
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
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_fcurve_channel(View2D *v2d, AnimData *adt, FCurve *fcu, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
		fcurve_to_keylist(adt, fcu, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_agroup_channel(View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
		agroup_to_keylist(adt, agrp, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_action_channel(View2D *v2d, AnimData *adt, bAction *act, float ypos)
{
	DLRBT_Tree keys, blocks;
	
	BLI_dlrbTree_init(&keys);
	BLI_dlrbTree_init(&blocks);
	
		action_to_keylist(adt, act, &keys, &blocks);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	BLI_dlrbTree_linkedlist_sync(&blocks);
	
		draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_dlrbTree_free(&keys);
	BLI_dlrbTree_free(&blocks);
}

void draw_gpl_channel(View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos)
{
	DLRBT_Tree keys;
	
	BLI_dlrbTree_init(&keys);
	
		gpl_to_keylist(ads, gpl, &keys, NULL);
	
	BLI_dlrbTree_linkedlist_sync(&keys);
	
		draw_keylist(v2d, &keys, NULL, ypos);
	
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
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVESONLY);
		ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
		/* loop through each F-Curve, grabbing the keyframes */
		for (ale= anim_data.first; ale; ale= ale->next)
			fcurve_to_keylist(ale->adt, ale->data, keys, blocks);
		
		BLI_freelistN(&anim_data);
	}
}

void scene_to_keylist(bDopeSheet *ads, Scene *sce, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	if (sce) {
		AnimData *adt;
		int filterflag;
		
		/* get filterflag */
		if (ads)
			filterflag= ads->filterflag;
		else
			filterflag= 0;
			
		/* scene animdata */
		if ((sce->adt) && !(filterflag & ADS_FILTER_NOSCE)) {
			adt= sce->adt;
			
			// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
			if (adt->action) 
				action_to_keylist(adt, adt->action, keys, blocks);
		}
		
		/* world animdata */
		if ((sce->world) && (sce->world->adt) && !(filterflag & ADS_FILTER_NOWOR)) {
			adt= sce->world->adt;
			
			// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
			if (adt->action) 
				action_to_keylist(adt, adt->action, keys, blocks);
		}
		
		/* nodetree animdata */
		if ((sce->nodetree) && (sce->nodetree->adt) && !(filterflag & ADS_FILTER_NONTREE)) {
			adt= sce->nodetree->adt;
			
			// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
			if (adt->action) 
				action_to_keylist(adt, adt->action, keys, blocks);
		}
	}
}

void ob_to_keylist(bDopeSheet *ads, Object *ob, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	Key *key= ob_get_key(ob);
	int filterflag= (ads)? ads->filterflag : 0;
	
	/* sanity check */
	if (ob == NULL)
		return;
		
	/* Add action keyframes */
	if (ob->adt && ob->adt->action)
		action_to_keylist(ob->adt, ob->adt->action, keys, blocks);
	
	/* Add shapekey keyframes (only if dopesheet allows, if it is available) */
	if ((key && key->adt && key->adt->action) && !(filterflag & ADS_FILTER_NOSHAPEKEYS))
		action_to_keylist(key->adt, key->adt->action, keys, blocks);
	
	/* Add material keyframes */
	if ((ob->totcol) && !(filterflag & ADS_FILTER_NOMAT)) {
		int a;
		
		for (a=0; a < ob->totcol; a++) {
			Material *ma= give_current_material(ob, a);
			
			/* there might not be a material */
			if (ELEM(NULL, ma, ma->adt)) 
				continue;
			
			/* add material's data */
			action_to_keylist(ma->adt, ma->adt->action, keys, blocks);
		}
	}
	
	/* Add object data keyframes */
	switch (ob->type) {
		case OB_CAMERA: /* ------- Camera ------------ */
		{
			Camera *ca= (Camera *)ob->data;
			
			if ((ca->adt) && !(filterflag & ADS_FILTER_NOCAM)) 
				action_to_keylist(ca->adt, ca->adt->action, keys, blocks);
		}
			break;
		case OB_LAMP: /* ---------- Lamp ----------- */
		{
			Lamp *la= (Lamp *)ob->data;
			
			if ((la->adt) && !(filterflag & ADS_FILTER_NOLAM)) 
				action_to_keylist(la->adt, la->adt->action, keys, blocks);
		}
			break;
		case OB_CURVE: /* ------- Curve ---------- */
		{
			Curve *cu= (Curve *)ob->data;
			
			if ((cu->adt) && !(filterflag & ADS_FILTER_NOCUR)) 
				action_to_keylist(cu->adt, cu->adt->action, keys, blocks);
		}
			break;
		case OB_MBALL: /* ------- MetaBall ---------- */
		{
			MetaBall *mb= (MetaBall *)ob->data;
			
			if ((mb->adt) && !(filterflag & ADS_FILTER_NOMBA)) 
				action_to_keylist(mb->adt, mb->adt->action, keys, blocks);
		}
			break;
		case OB_ARMATURE: /* ------- Armature ---------- */
		{
			bArmature *arm= (bArmature *)ob->data;
			
			if ((arm->adt) && !(filterflag & ADS_FILTER_NOARM)) 
				action_to_keylist(arm->adt, arm->adt->action, keys, blocks);
		}
			break;
	}
	
	/* Add Particle System Keyframes */
	if ((ob->particlesystem.first) && !(filterflag & ADS_FILTER_NOPART)) {
		ParticleSystem *psys = ob->particlesystem.first;
		
		for(; psys; psys=psys->next) {
			if (ELEM(NULL, psys->part, psys->part->adt))
				continue;
			else
				action_to_keylist(psys->part->adt, psys->part->adt->action, keys, blocks);
		}
	}
}

void fcurve_to_keylist(AnimData *adt, FCurve *fcu, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	BezTriple *bezt;
	int v;
	
	if (fcu && fcu->totvert && fcu->bezt) {
		/* apply NLA-mapping (if applicable) */
		if (adt)	
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 1);
		
		/* loop through beztriples, making ActKeys and ActKeyBlocks */
		bezt= fcu->bezt;
		
		for (v=0; v < fcu->totvert; v++, bezt++) {
			add_bezt_to_keycolumns_list(keys, bezt);
			if (blocks) add_bezt_to_keyblocks_list(blocks, fcu, v);
		}
		
		/* update the number of curves that elements have appeared in  */
		// FIXME: this is broken with the new tree structure for now...
		if (keys)
			set_touched_actkeycolumn(keys->root);
		if (blocks)
			set_touched_actkeyblock(blocks->root);
		
		/* unapply NLA-mapping if applicable */
		ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 1);
	}
}

void agroup_to_keylist(AnimData *adt, bActionGroup *agrp, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	FCurve *fcu;

	if (agrp) {
		/* loop through F-Curves */
		for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}

void action_to_keylist(AnimData *adt, bAction *act, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	FCurve *fcu;

	if (act) {
		/* loop through F-Curves */
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}


void gpl_to_keylist(bDopeSheet *ads, bGPDlayer *gpl, DLRBT_Tree *keys, DLRBT_Tree *blocks)
{
	bGPDframe *gpf;
	ActKeyColumn *ak;
	
	if (gpl && keys) {
		/* loop over frames, converting directly to 'keyframes' (should be in order too) */
		for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
			ak= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
			BLI_addtail((ListBase *)keys, ak);
			
			ak->cfra= (float)gpf->framenum;
			ak->modified = 1;
			ak->key_type= 0; 
			
			if (gpf->flag & GP_FRAME_SELECT)
				ak->sel = SELECT;
			else
				ak->sel = 0;
		}
	}
}

