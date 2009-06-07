/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include


/* ******************* nla editor space & buttons ************** */

#define B_NOP		1
#define B_REDR		2

/* -------------- */

static void do_nla_region_buttons(bContext *C, void *arg, int event)
{
	//Scene *scene= CTX_data_scene(C);
	
	switch(event) {

	}
	
	/* default for now */
	//WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
}

static int nla_panel_context(const bContext *C, bAnimListElem **ale, NlaTrack **nlt)
{
	bAnimContext ac;
	bAnimListElem *elem= NULL;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return 0;
	
	// XXX
	return 1;
	
	/* try to find 'active' F-Curve */
	//elem= get_active_fcurve_channel(&ac);
	if(elem == NULL) 
		return 0;
	
	if(nlt)
		*nlt= (NlaTrack*)elem->data;
	if(ale)
		*ale= elem;
	else
		MEM_freeN(elem);
	
	return 1;
}

static int nla_panel_poll(const bContext *C, PanelType *pt)
{
	return nla_panel_context(C, NULL, NULL);
}

static void nla_panel_properties(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	NlaTrack *nlt;
	uiBlock *block;
	char name[128];

	if(!nla_panel_context(C, &ale, &nlt))
		return;

	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);

	/* Info - Active F-Curve */
	uiDefBut(block, LABEL, 1, "Active NLA Strip:",					10, 200, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	

	//MEM_freeN(ale);
}


/* ******************* general ******************************** */


void nla_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
	strcpy(pt->idname, "NLA_PT_properties");
	strcpy(pt->label, "Properties");
	pt->draw= nla_panel_properties;
	pt->poll= nla_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int nla_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= nla_has_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void NLAEDIT_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "NLAEDIT_OT_properties";
	
	ot->exec= nla_properties;
	ot->poll= ED_operator_nla_active;
 	
	/* flags */
	ot->flag= 0;
}
