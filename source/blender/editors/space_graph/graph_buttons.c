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
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
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

#include "graph_intern.h"	// own include


/* ******************* view3d space & buttons ************** */
#define B_NOP		1
#define B_REDR		2

static void do_graph_region_buttons(bContext *C, void *arg, int event)
{
	//Scene *scene= CTX_data_scene(C);
	
	switch(event) {

	}
	
	/* default for now */
	//WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
}


static void graph_panel_properties(const bContext *C, ARegion *ar, short cntrl, bAnimListElem *ale)	// GRAPH_HANDLER_SETTINGS
{
	uiBlock *block;
	char name[128];

	block= uiBeginBlock(C, ar, "graph_panel_properties", UI_EMBOSS, UI_HELV);
	if(uiNewPanel(C, ar, block, "Properties", "Graph", 340, 30, 318, 254)==0) return;
	uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL);

	/* to force height */
	uiNewPanelHeight(block, 204);

	// XXX testing buttons
	uiDefBut(block, LABEL, 1, "Active F-Curve:",					10, 200, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	getname_anim_fcurve(name, ale->id, (FCurve *)ale->data);
	uiDefBut(block, LABEL, 1, name,					30, 180, 300, 19, NULL, 0.0, 0.0, 0, 0, "Name of Active F-Curve");
	
#if 0
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_REDR, "Spacing:",		10, 200, 140, 19, &v3d->grid, 0.001, 100.0, 10, 0, "Set the distance between grid lines");
	uiDefButS(block, NUM, B_REDR, "Lines:",		10, 180, 140, 19, &v3d->gridlines, 0.0, 100.0, 100, 0, "Set the number of grid lines in perspective view");
	uiDefButS(block, NUM, B_REDR, "Divisions:",		10, 160, 140, 19, &v3d->gridsubdiv, 1.0, 100.0, 100, 0, "Set the number of grid lines");
	uiBlockEndAlign(block);
#endif
}

/* Find 'active' F-Curve. It must be editable, since that's the purpose of these buttons (subject to change).  
 * We return the 'wrapper' since it contains valuable context info (about hierarchy), which will need to be freed 
 * when the caller is done with it.
 */
// TODO: move this to anim api with another name?
static bAnimListElem *get_active_fcurve_channel (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	int filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_ACTIVE | ANIMFILTER_CURVESONLY);
	int items = ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* We take the first F-Curve only, since some other ones may have had 'active' flag set
	 * if they were from linked data.
	 */
	if (items) {
		bAnimListElem *ale= (bAnimListElem *)anim_data.first;
		
		/* remove first item from list, then free the rest of the list and return the stored one */
		BLI_remlink(&anim_data, ale);
		BLI_freelistN(&anim_data);
		
		return ale;
	}
	
	/* no active F-Curve */
	return NULL;
}

void graph_region_buttons(const bContext *C, ARegion *ar)
{
	SpaceIpo *sipo= (SpaceIpo *)CTX_wm_space_data(C);
	bAnimContext ac;
	bAnimListElem *ale= NULL;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return;
	
	
	/* try to find 'active' F-Curve */
	ale= get_active_fcurve_channel(&ac);
	if (ale == NULL) 
		return;	
		
		// XXX temp panel for testing
	graph_panel_properties(C, ar, 0, ale);
	
	/* driver settings for active F-Curve (only for 'Drivers' mode) */
	if (sipo->mode == SIPO_MODE_DRIVERS) {
		//graph_panel_drivers(C, ar, 0);
	}

	uiDrawPanels(C, 1);		/* 1 = align */
	uiMatchPanelsView2d(ar); /* sets v2d->totrct */
	
	/* free temp data */
	MEM_freeN(ale);
}


static int graph_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= graph_has_buttons_region(sa);
	
	if(ar) {
		ar->flag ^= RGN_FLAG_HIDDEN;
		ar->v2d.flag &= ~V2D_IS_INITIALISED; /* XXX should become hide/unhide api? */
		
		ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
		ED_area_tag_redraw(sa);
	}
	return OPERATOR_FINISHED;
}

void GRAPHEDIT_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "GRAPHEDIT_OT_properties";
	
	ot->exec= graph_properties;
	ot->poll= ED_operator_ipo_active; // xxx
 	
	/* flags */
	ot->flag= 0;
}
