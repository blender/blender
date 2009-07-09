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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
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
#include "BLI_arithb.h"

/* Types --------------------------------------------------------------- */

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
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
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

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

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

/* *************************** Keyframe Drawing *************************** */

static void add_bezt_to_keycolumnslist(ListBase *keys, BezTriple *bezt)
{
	/* The equivalent of add_to_cfra_elem except this version 
	 * makes ActKeyColumns - one of the two datatypes required
	 * for action editor drawing.
	 */
	ActKeyColumn *ak, *akn;
	
	if (ELEM(NULL, keys, bezt)) return;
	
	/* try to any existing key to replace, or where to insert after */
	for (ak= keys->last; ak; ak= ak->prev) {
		/* do because of double keys */
		if (ak->cfra == bezt->vec[1][0]) {			
			/* set selection status and 'touched' status */
			if (BEZSELECTED(bezt)) ak->sel = SELECT;
			ak->modified += 1;
			
			return;
		}
		else if (ak->cfra < bezt->vec[1][0]) break;
	}
	
	/* add new block */
	akn= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
	if (ak) BLI_insertlinkafter(keys, ak, akn);
	else BLI_addtail(keys, akn);
	
	akn->cfra= bezt->vec[1][0];
	akn->modified += 1;
	
	// TODO: handle type = bezt->h1 or bezt->h2
	akn->handle_type= 0; 
	
	if (BEZSELECTED(bezt))
		akn->sel = SELECT;
	else
		akn->sel = 0;
}

static void add_bezt_to_keyblockslist(ListBase *blocks, FCurve *fcu, int index)
{
	/* The equivalent of add_to_cfra_elem except this version 
	 * makes ActKeyBlocks - one of the two datatypes required
	 * for action editor drawing.
	 */
	ActKeyBlock *ab, *abn;
	BezTriple *beztn=NULL, *prev=NULL;
	BezTriple *bezt;
	int v;
	
	/* get beztriples */
	beztn= (fcu->bezt + index);
	
	/* we need to go through all beztriples, as they may not be in order (i.e. during transform) */
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
	
	/* try to find a keyblock that starts on the previous beztriple 
	 * Note: we can't search from end to try to optimise this as it causes errors there's
	 * 		an A ___ B |---| B situation
	 */
	// FIXME: here there is a bug where we are trying to get the summary for the following channels
	//		A|--------------|A ______________ B|--------------|B
	//		A|------------------------------------------------|A
	//		A|----|A|---|A|-----------------------------------|A
	for (ab= blocks->first; ab; ab= ab->next) {
		/* check if alter existing block or add new block */
		if (ab->start == prev->vec[1][0]) {			
			/* set selection status and 'touched' status */
			if (BEZSELECTED(beztn)) ab->sel = SELECT;
			ab->modified += 1;
			
			return;
		}
		else if (ab->start < prev->vec[1][0]) break;
	}
	
	/* add new block */
	abn= MEM_callocN(sizeof(ActKeyBlock), "ActKeyBlock");
	if (ab) BLI_insertlinkbefore(blocks, ab, abn);
	else BLI_addtail(blocks, abn);
	
	abn->start= prev->vec[1][0];
	abn->end= beztn->vec[1][0];
	abn->val= beztn->vec[1][1];
	
	if (BEZSELECTED(prev) || BEZSELECTED(beztn))
		abn->sel = SELECT;
	else
		abn->sel = 0;
	abn->modified = 1;
}

/* helper function - find actkeycolumn that occurs on cframe */
static ActKeyColumn *cfra_find_actkeycolumn (ListBase *keys, float cframe)
{
	ActKeyColumn *ak, *ak2;
	
	if (keys==NULL) 
		return NULL;
	 
	/* search from both ends at the same time, and stop if we find match or if both ends meet */ 
	for (ak=keys->first, ak2=keys->last; ak && ak2; ak=ak->next, ak2=ak2->prev) {
		/* return whichever end encounters the frame */
		if (ak->cfra == cframe)
			return ak;
		if (ak2->cfra == cframe)
			return ak2;
		
		/* no matches on either end, so return NULL */
		if (ak == ak2)
			return NULL;
	}
	
	return NULL;
}

/* coordinates for diamond shape */
static const float _unit_diamond_shape[4][2] = {
	{0.0f, 1.0f},	/* top vert */
	{1.0f, 0.0f},	/* mid-right */
	{0.0f, -1.0f},	/* bottom vert */
	{-1.0f, 0.0f}	/* mid-left */
}; 

/* draw a simple diamond shape with OpenGL */
static void draw_keyframe_shape (float x, float y, float xscale, float hsize, short sel)
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
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f/xscale*hsize, hsize, 1.0f);
	
	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);
	
	/* draw! ---------------------------- */
	
	/* interior - hardcoded colors (for selected and unselected only) */
	if (sel) UI_ThemeColorShade(TH_STRIP_SELECT, 50);//glColor3ub(0xF1, 0xCA, 0x13);
	else glColor3ub(0xE9, 0xE9, 0xE9);
	glCallList(displist2);
	
	/* exterior - black frame */
	glColor3ub(0, 0, 0);
	glCallList(displist1);
	
	glDisable(GL_LINE_SMOOTH);
	
	/* restore view transform */
	glScalef(xscale/hsize, 1.0f/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

static void draw_keylist(View2D *v2d, ListBase *keys, ListBase *blocks, float ypos)
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
			ak= cfra_find_actkeycolumn(keys, ab->start);
			startCurves = (ak)? ak->totcurve: 0;
			
			ak= cfra_find_actkeycolumn(keys, ab->end);
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
			/* draw using OpenGL - uglier but faster */
			// NOTE: a previous version of this didn't work nice for some intel cards
			draw_keyframe_shape(ak->cfra, ypos, xscale, 5.0f, (ak->sel & SELECT));
			
#if 0 // OLD CODE
			//int sc_x, sc_y;
			
			/* get co-ordinate to draw at */
			//gla2DDrawTranslatePt(di, ak->cfra, ypos, &sc_x, &sc_y);
			
			/* draw using icons - old way which is slower but more proven */
			//if (ak->sel & SELECT) UI_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE2, 1.0f);
			//else UI_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE3, 1.0f);
#endif // OLD CODE
#if 0 // NEW NON-WORKING CODE 
			/* draw icon */
			// FIXME: this draws slightly wrong, as we need to apply some offset for icon, but that depends on scaling
			// so for now disabled
			//int icon = (ak->sel & SELECT) ? ICON_SPACE2 : ICON_SPACE3;
			//UI_icon_draw_aspect(ak->cfra, ypos-6, icon, 1.0f);
#endif // NEW NON-WORKING CODE
			
		}	
	}
	
	glDisable(GL_BLEND);
}

/* *************************** Channel Drawing Funcs *************************** */

void draw_scene_channel(View2D *v2d, bDopeSheet *ads, Scene *sce, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	scene_to_keylist(ads, sce, &keys, &blocks);
	draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_object_channel(View2D *v2d, bDopeSheet *ads, Object *ob, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	ob_to_keylist(ads, ob, &keys, &blocks);
	draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_fcurve_channel(View2D *v2d, AnimData *adt, FCurve *fcu, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	fcurve_to_keylist(adt, fcu, &keys, &blocks);
	draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_agroup_channel(View2D *v2d, AnimData *adt, bActionGroup *agrp, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	agroup_to_keylist(adt, agrp, &keys, &blocks);
	draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_action_channel(View2D *v2d, AnimData *adt, bAction *act, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	action_to_keylist(adt, act, &keys, &blocks);
	draw_keylist(v2d, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_gpl_channel(View2D *v2d, bDopeSheet *ads, bGPDlayer *gpl, float ypos)
{
	ListBase keys = {0, 0};
	
	gpl_to_keylist(ads, gpl, &keys, NULL);
	draw_keylist(v2d, &keys, NULL, ypos);
	BLI_freelistN(&keys);
}

/* *************************** Keyframe List Conversions *************************** */

void scene_to_keylist(bDopeSheet *ads, Scene *sce, ListBase *keys, ListBase *blocks)
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
	}
}

void ob_to_keylist(bDopeSheet *ads, Object *ob, ListBase *keys, ListBase *blocks)
{
	Key *key= ob_get_key(ob);

	if (ob) {
		int filterflag;
		
		/* get filterflag */
		if (ads)
			filterflag= ads->filterflag;
		else
			filterflag= 0;
		
		/* Add action keyframes */
		if (ob->adt && ob->adt->action)
			action_to_keylist(ob->adt, ob->adt->action, keys, blocks);
		
		/* Add shapekey keyframes (only if dopesheet allows, if it is available) */
		if ((key && key->adt && key->adt->action) && !(filterflag & ADS_FILTER_NOSHAPEKEYS))
			action_to_keylist(key->adt, key->adt->action, keys, blocks);
			
		// TODO: restore materials, and object data, etc.
	}
}


void fcurve_to_keylist(AnimData *adt, FCurve *fcu, ListBase *keys, ListBase *blocks)
{
	BezTriple *bezt;
	ActKeyColumn *ak, *ak2;
	ActKeyBlock *ab, *ab2;
	int v;
	
	if (fcu && fcu->totvert && fcu->bezt) {
		/* apply NLA-mapping (if applicable) */
		if (adt)	
			ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 1);
		
		/* loop through beztriples, making ActKeys and ActKeyBlocks */
		bezt= fcu->bezt;
		
		for (v=0; v < fcu->totvert; v++, bezt++) {
			/* only if keyframe is in range (optimisation) */
			add_bezt_to_keycolumnslist(keys, bezt);
			if (blocks) add_bezt_to_keyblockslist(blocks, fcu, v);
		}
		
		/* update the number of curves that elements have appeared in  */
		if (keys) {
			for (ak=keys->first, ak2=keys->last; ak && ak2; ak=ak->next, ak2=ak2->prev) {
				if (ak->modified) {
					ak->modified = 0;
					ak->totcurve += 1;
				}
				
				if (ak == ak2)
					break;
				
				if (ak2->modified) {
					ak2->modified = 0;
					ak2->totcurve += 1;
				}
			}
		}
		if (blocks) {
			for (ab=blocks->first, ab2=blocks->last; ab && ab2; ab=ab->next, ab2=ab2->prev) {
				if (ab->modified) {
					ab->modified = 0;
					ab->totcurve += 1;
				}
				
				if (ab == ab2)
					break;
				
				if (ab2->modified) {
					ab2->modified = 0;
					ab2->totcurve += 1;
				}
			}
		}
		
		/* unapply NLA-mapping if applicable */
		ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 1);
	}
}

void agroup_to_keylist(AnimData *adt, bActionGroup *agrp, ListBase *keys, ListBase *blocks)
{
	FCurve *fcu;

	if (agrp) {
		/* loop through F-Curves */
		for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}

void action_to_keylist(AnimData *adt, bAction *act, ListBase *keys, ListBase *blocks)
{
	FCurve *fcu;

	if (act) {
		/* loop through F-Curves */
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {
			fcurve_to_keylist(adt, fcu, keys, blocks);
		}
	}
}


void gpl_to_keylist(bDopeSheet *ads, bGPDlayer *gpl, ListBase *keys, ListBase *blocks)
{
	bGPDframe *gpf;
	ActKeyColumn *ak;
	
	if (gpl && keys) {
		/* loop over frames, converting directly to 'keyframes' (should be in order too) */
		for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
			ak= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
			BLI_addtail(keys, ak);
			
			ak->cfra= (float)gpf->framenum;
			ak->modified = 1;
			ak->handle_type= 0; 
			
			if (gpf->flag & GP_FRAME_SELECT)
				ak->sel = SELECT;
			else
				ak->sel = 0;
		}
	}
}

