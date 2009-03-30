/**
 * $Id: drawaction.c 17746 2008-12-08 11:19:44Z aligorith $
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
#include "DNA_ipo_types.h"
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
#include "UI_text.h"
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

#if 0  // disabled, as some intel cards have problems with this
/* Draw a simple diamond shape with a filled in center (in screen space) */
static void draw_key_but(int x, int y, short w, short h, int sel)
{
	int xmin= x, ymin= y;
	int xmax= x+w-1, ymax= y+h-1;
	int xc= (xmin+xmax)/2, yc= (ymin+ymax)/2;
	
	/* interior - hardcoded colors (for selected and unselected only) */
	if (sel) glColor3ub(0xF1, 0xCA, 0x13);
	else glColor3ub(0xE9, 0xE9, 0xE9);
	
	glBegin(GL_QUADS);
	glVertex2i(xc, ymin);
	glVertex2i(xmax, yc);
	glVertex2i(xc, ymax);
	glVertex2i(xmin, yc);
	glEnd();
	
	
	/* outline */
	glColor3ub(0, 0, 0);
	
	glBegin(GL_LINE_LOOP);
	glVertex2i(xc, ymin);
	glVertex2i(xmax, yc);
	glVertex2i(xc, ymax);
	glVertex2i(xmin, yc);
	glEnd();
}
#endif

static void draw_keylist(gla2DDrawInfo *di, ListBase *keys, ListBase *blocks, float ypos)
{
	ActKeyColumn *ak;
	ActKeyBlock *ab;
	
	glEnable(GL_BLEND);
	
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
				int sc_xa, sc_xb, sc_ya, sc_yb;
				
				/* get co-ordinates of block */
				gla2DDrawTranslatePt(di, ab->start, ypos, &sc_xa, &sc_ya);
				gla2DDrawTranslatePt(di, ab->end, ypos, &sc_xb, &sc_yb);
				
				/* draw block */
				if (ab->sel)
					UI_ThemeColor4(TH_STRIP_SELECT);
				else
					UI_ThemeColor4(TH_STRIP);
				glRectf((float)sc_xa, (float)sc_ya-3, (float)sc_xb, (float)sc_yb+5);
			}
		}
	}
	
	/* draw keys */
	if (keys) {
		for (ak= keys->first; ak; ak= ak->next) {
			int sc_x, sc_y;
			
			/* get co-ordinate to draw at */
			gla2DDrawTranslatePt(di, ak->cfra, ypos, &sc_x, &sc_y);
			
			/* draw using icons - old way which is slower but more proven */
			if (ak->sel & SELECT) UI_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE2, 1.0f);
			else UI_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE3, 1.0f);
			
			/* draw using OpenGL - slightly uglier but faster */
			// 	NOTE: disabled for now, as some intel cards seem to have problems with this
			//draw_key_but(sc_x-5, sc_y-4, 11, 11, (ak->sel & SELECT));
		}	
	}
	
	glDisable(GL_BLEND);
}

/* *************************** Channel Drawing Funcs *************************** */

void draw_scene_channel(gla2DDrawInfo *di, ActKeysInc *aki, Scene *sce, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	scene_to_keylist(sce, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_object_channel(gla2DDrawInfo *di, ActKeysInc *aki, Object *ob, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	ob_to_keylist(ob, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_fcurve_channel(gla2DDrawInfo *di, ActKeysInc *aki, FCurve *fcu, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	fcurve_to_keylist(fcu, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_agroup_channel(gla2DDrawInfo *di, ActKeysInc *aki, bActionGroup *agrp, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	agroup_to_keylist(agrp, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_action_channel(gla2DDrawInfo *di, ActKeysInc *aki, bAction *act, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	action_to_keylist(act, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_gpl_channel(gla2DDrawInfo *di, ActKeysInc *aki, bGPDlayer *gpl, float ypos)
{
	ListBase keys = {0, 0};
	
	gpl_to_keylist(gpl, &keys, NULL, aki);
	draw_keylist(di, &keys, NULL, ypos);
	BLI_freelistN(&keys);
}

/* *************************** Keyframe List Conversions *************************** */

void scene_to_keylist(Scene *sce, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	if (sce) {
		bDopeSheet *ads= (aki)? (aki->ads) : NULL;
		AnimData *adt;
		int filterflag;
		
		/* get filterflag */
		if (ads)
			filterflag= ads->filterflag;
		else if ((aki) && (aki->actmode == -1)) /* only set like this by NLA */
			filterflag= ADS_FILTER_NLADUMMY;
		else
			filterflag= 0;
			
		/* scene animdata */
		if ((sce->adt) && !(filterflag & ADS_FILTER_NOSCE)) {
			adt= sce->adt;
			
			// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
			if (adt->action) 
				action_to_keylist(adt->action, keys, blocks, aki);
		}
		
		/* world animdata */
		if ((sce->world) && (sce->world->adt) && !(filterflag & ADS_FILTER_NOWOR)) {
			adt= sce->world->adt;
			
			// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
			if (adt->action) 
				action_to_keylist(adt->action, keys, blocks, aki);
		}
	}
}

void ob_to_keylist(Object *ob, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	Key *key= ob_get_key(ob);

	if (ob) {
		bDopeSheet *ads= (aki)? (aki->ads) : NULL;
		int filterflag;
		
		/* get filterflag */
		if (ads)
			filterflag= ads->filterflag;
		else if ((aki) && (aki->actmode == -1)) /* only set like this by NLA */
			filterflag= ADS_FILTER_NLADUMMY;
		else
			filterflag= 0;
		
		/* Add action keyframes */
		if (ob->adt && ob->adt->action)
			action_nlascaled_to_keylist(ob, ob->adt->action, keys, blocks, aki);
		
		/* Add shapekey keyframes (only if dopesheet allows, if it is available) */
		// TODO: when we adapt NLA system, this needs to be the NLA-scaled version
		if ((key && key->adt && key->adt->action) && !(filterflag & ADS_FILTER_NOSHAPEKEYS))
			action_to_keylist(key->adt->action, keys, blocks, aki);
			
#if 0 // XXX old animation system
		/* Add material keyframes (only if dopesheet allows, if it is available) */
		if ((ob->totcol) && !(filterflag & ADS_FILTER_NOMAT)) {
			short a;
			
			for (a=0; a<ob->totcol; a++) {
				Material *ma= give_current_material(ob, a);
				
				if (ELEM(NULL, ma, ma->ipo) == 0)
					ipo_to_keylist(ma->ipo, keys, blocks, aki);
			}
		}
			
		/* Add object data keyframes */
		switch (ob->type) {
			case OB_CAMERA: /* ------- Camera ------------ */
			{
				Camera *ca= (Camera *)ob->data;
				if ((ca->ipo) && !(filterflag & ADS_FILTER_NOCAM))
					ipo_to_keylist(ca->ipo, keys, blocks, aki);
			}
				break;
			case OB_LAMP: /* ---------- Lamp ----------- */
			{
				Lamp *la= (Lamp *)ob->data;
				if ((la->ipo) && !(filterflag & ADS_FILTER_NOLAM))
					ipo_to_keylist(la->ipo, keys, blocks, aki);
			}
				break;
			case OB_CURVE: /* ------- Curve ---------- */
			{
				Curve *cu= (Curve *)ob->data;
				if ((cu->ipo) && !(filterflag & ADS_FILTER_NOCUR))
					ipo_to_keylist(cu->ipo, keys, blocks, aki);
			}
				break;
		}
#endif // XXX old animation system
	}
}

static short bezt_in_aki_range (ActKeysInc *aki, BezTriple *bezt)
{
	/* when aki == NULL, we don't care about range */
	if (aki == NULL) 
		return 1;
		
	/* if start and end are both 0, then don't care about range */
	if (IS_EQ(aki->start, 0) && IS_EQ(aki->end, 0))
		return 1;
		
	/* if nla-scaling is in effect, apply appropriate scaling adjustments */
#if 0 // XXX this was from some buggy code... do not port for now
	if (aki->ob) {
		float frame= get_action_frame_inv(aki->ob, bezt->vec[1][0]);
		return IN_RANGE(frame, aki->start, aki->end);
	}
	else {
		/* check if in range */
		return IN_RANGE(bezt->vec[1][0], aki->start, aki->end);
	}
#endif // XXX this was from some buggy code... do not port for now
	return 1;
}

void fcurve_to_keylist(FCurve *fcu, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	BezTriple *bezt;
	ActKeyColumn *ak, *ak2;
	ActKeyBlock *ab, *ab2;
	int v;
	
	if (fcu && fcu->totvert && fcu->bezt) {
		/* loop through beztriples, making ActKeys and ActKeyBlocks */
		bezt= fcu->bezt;
		
		for (v=0; v < fcu->totvert; v++, bezt++) {
			/* only if keyframe is in range (optimisation) */
			if (bezt_in_aki_range(aki, bezt)) {
				add_bezt_to_keycolumnslist(keys, bezt);
				if (blocks) add_bezt_to_keyblockslist(blocks, fcu, v);
			}
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
	}
}

void agroup_to_keylist(bActionGroup *agrp, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	FCurve *fcu;

	if (agrp) {
		/* loop through F-Curves */
		for (fcu= agrp->channels.first; fcu && fcu->grp==agrp; fcu= fcu->next) {
			fcurve_to_keylist(fcu, keys, blocks, aki);
		}
	}
}

void action_to_keylist(bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	FCurve *fcu;

	if (act) {
		/* loop through F-Curves */
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {
			fcurve_to_keylist(fcu, keys, blocks, aki);
		}
	}
}

void action_nlascaled_to_keylist(Object *ob, bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	FCurve *fcu;
	Object *oldob= NULL;
	
	/* although apply and clearing NLA-scaling pre-post creating keylist does impact on performance,
	 * the effects should be fairly minimal, as we're already going through the keyframes multiple times 
	 * already for blocks too...
	 */
	if (act) {	
		/* if 'aki' is provided, store it's current ob to restore later as it might not be the same */
		if (aki) {
			oldob= aki->ob;
			aki->ob= ob;
		}
		
		/* loop through F-Curves 
		 *	- scaling correction only does times for center-points, so should be faster
		 */
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {	
			ANIM_nla_mapping_apply_fcurve(ob, fcu, 0, 1);
			fcurve_to_keylist(fcu, keys, blocks, aki);
			ANIM_nla_mapping_apply_fcurve(ob, fcu, 1, 1);
		}
		
		/* if 'aki' is provided, restore ob */
		if (aki)
			aki->ob= oldob;
	}
}

void gpl_to_keylist(bGPDlayer *gpl, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
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

