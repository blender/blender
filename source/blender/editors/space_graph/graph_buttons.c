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

#include "graph_intern.h"	// own include

/* XXX */

/* temporary definition for limits of float number buttons (FLT_MAX tends to infinity with old system) */
#define UI_FLT_MAX 	10000.0f


/* ******************* graph editor space & buttons ************** */

#define B_NOP		1
#define B_REDR		2

/* -------------- */

static void do_graph_region_buttons(bContext *C, void *arg, int event)
{
	//Scene *scene= CTX_data_scene(C);
	
	switch(event) {

	}
	
	/* default for now */
	//WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
}

static int graph_panel_context(const bContext *C, bAnimListElem **ale, FCurve **fcu)
{
	bAnimContext ac;
	bAnimListElem *elem= NULL;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return 0;
	
	/* try to find 'active' F-Curve */
	elem= get_active_fcurve_channel(&ac);
	if(elem == NULL) 
		return 0;
	
	if(fcu)
		*fcu= (FCurve*)elem->data;
	if(ale)
		*ale= elem;
	else
		MEM_freeN(elem);
	
	return 1;
}

static int graph_panel_poll(const bContext *C, PanelType *pt)
{
	return graph_panel_context(C, NULL, NULL);
}

static void graph_panel_properties(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	uiBlock *block;
	char name[128];

	if(!graph_panel_context(C, &ale, &fcu))
		return;

	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL);

	/* Info - Active F-Curve */
	uiDefBut(block, LABEL, 1, "Active F-Curve:",					10, 200, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	if (ale->id) { 
		// icon of active blocktype - is this really necessary?
		int icon= geticon_anim_blocktype(GS(ale->id->name));
		
		// xxx type of icon-but is currently "LABEL", as that one is plain...
		uiDefIconBut(block, LABEL, 1, icon, 10, 180, 20, 19, NULL, 0, 0, 0, 0, "ID-type that F-Curve belongs to");
	}
	
	getname_anim_fcurve(name, ale->id, fcu);
	uiDefBut(block, LABEL, 1, name,	40, 180, 300, 19, NULL, 0.0, 0.0, 0, 0, "Name of Active F-Curve");
	
	/* TODO: the following settings could be added here
	 *	- F-Curve coloring mode - mode selector + color selector
	 *	- Access details (ID-block + RNA-Path + Array Index)
	 *	- ...
	 */

	MEM_freeN(ale);
}

/* ******************* drivers ******************************** */

#define B_IPO_DEPCHANGE 	10

static void do_graph_region_driver_buttons(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	
	switch (event) {
		case B_IPO_DEPCHANGE:
		{
			/* rebuild depsgraph for the new deps */
			DAG_scene_sort(scene);
			
			/* force an update of depsgraph */
			ED_anim_dag_flush_update(C);
		}
			break;
	}
	
	/* default for now */
	WM_event_add_notifier(C, NC_SCENE, scene);
}

/* callback to remove the active driver */
static void driver_remove_cb (bContext *C, void *ale_v, void *dummy_v)
{
	bAnimListElem *ale= (bAnimListElem *)ale_v;
	ID *id= ale->id;
	FCurve *fcu= ale->data;
	
	/* try to get F-Curve that driver lives on, and ID block which has this AnimData */
	if (ELEM(NULL, id, fcu))
		return;
	
	/* call API method to remove this driver  */	
	ANIM_remove_driver(id, fcu->rna_path, fcu->array_index, 0);
}

/* callback to add a target variable to the active driver */
static void driver_add_var_cb (bContext *C, void *driver_v, void *dummy_v)
{
	ChannelDriver *driver= (ChannelDriver *)driver_v;
	
	/* add a new var */
	driver_add_new_target(driver);
}

/* callback to remove target variable from active driver */
static void driver_delete_var_cb (bContext *C, void *driver_v, void *dtar_v)
{
	ChannelDriver *driver= (ChannelDriver *)driver_v;
	DriverTarget *dtar= (DriverTarget *)dtar_v;
	
	/* add a new var */
	driver_free_target(driver, dtar);
}

/* callback to reset the driver's flags */
static void driver_update_flags_cb (bContext *C, void *fcu_v, void *dummy_v)
{
	FCurve *fcu= (FCurve *)fcu_v;
	ChannelDriver *driver= fcu->driver;
	
	/* clear invalid flags */
	driver->flag &= ~DRIVER_FLAG_INVALID;
}

/* drivers panel poll */
static int graph_panel_drivers_poll(const bContext *C, PanelType *pt)
{
	SpaceIpo *sipo= CTX_wm_space_graph(C);

	if(sipo->mode != SIPO_MODE_DRIVERS)
		return 0;

	return graph_panel_context(C, NULL, NULL);
}

/* driver settings for active F-Curve (only for 'Drivers' mode) */
static void graph_panel_drivers(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	ChannelDriver *driver;
	DriverTarget *dtar;
	
	PointerRNA rna_ptr;
	uiBlock *block;
	uiBut *but;
	int yco=85, i=0;

	if(!graph_panel_context(C, &ale, &fcu))
		return;

	driver= fcu->driver;
	
	block= uiLayoutFreeBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_graph_region_driver_buttons, NULL);
	
	/* general actions */
	but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Update Dependencies", 10, 200, 180, 22, NULL, 0.0, 0.0, 0, 0, "Force updates of dependencies");
	uiButSetFunc(but, driver_update_flags_cb, fcu, NULL);
	
	but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Remove Driver", 200, 200, 110, 18, NULL, 0.0, 0.0, 0, 0, "Remove this driver");
	uiButSetFunc(but, driver_remove_cb, ale, NULL);
	
	/* type */
	uiDefBut(block, LABEL, 1, "Type:",					10, 170, 60, 20, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButI(block, MENU, B_IPO_DEPCHANGE,
					"Driver Type%t|Normal%x0|Scripted Expression%x1|Rotational Difference%x2", 
					70,170,240,20, &driver->type, 0, 0, 0, 0, "Driver type");
	
	/* show expression box if doing scripted drivers */
	if (driver->type == DRIVER_TYPE_PYTHON) {
		uiDefBut(block, TEX, B_REDR, "Expr: ", 10,150,300,20, driver->expression, 0, 255, 0, 0, "One-liner Python Expression to use as Scripted Expression");
		
		/* errors */
		if (driver->flag & DRIVER_FLAG_INVALID) {
			uiDefIconBut(block, LABEL, 1, ICON_ERROR, 10, 130, 48, 48, NULL, 0, 0, 0, 0, ""); // a bit larger
			uiDefBut(block, LABEL, 0, "Error: invalid Python expression",
					50,110,230,19, NULL, 0, 0, 0, 0, "");
		}
	}
	else {
		/* errors */
		if (driver->flag & DRIVER_FLAG_INVALID) {
			uiDefIconBut(block, LABEL, 1, ICON_ERROR, 10, 130, 48, 48, NULL, 0, 0, 0, 0, ""); // a bit larger
			uiDefBut(block, LABEL, 0, "Error: invalid target channel(s)",
					50,130,230,19, NULL, 0, 0, 0, 0, "");
		}
	}
	
	but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Add Variable", 10, 110, 300, 20, NULL, 0.0, 0.0, 0, 0, "Add a new target variable for this Driver");
	uiButSetFunc(but, driver_add_var_cb, driver, NULL);
	
	/* loop over targets, drawing them */
	for (dtar= driver->targets.first; dtar; dtar= dtar->next) {
		short height = (dtar->id) ? 80 : 60;
		
		/* panel behind buttons */
		uiDefBut(block, ROUNDBOX, B_REDR, "", 5, yco-height+25, 310, height, NULL, 5.0, 0.0, 12.0, 0, "");
		
		/* variable name */
		uiDefButC(block, TEX, B_REDR, "Name: ", 10,yco,280,20, dtar->name, 0, 63, 0, 0, "Name of target variable (No spaces or dots are allowed. Also, must not start with a symbol or digit).");
		
		/* remove button */
		but= uiDefIconBut(block, BUT, B_REDR, ICON_X, 290, yco, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Delete target variable.");
		uiButSetFunc(but, driver_delete_var_cb, driver, dtar);
		
		
		/* Target Object */
		uiDefBut(block, LABEL, 1, "Value:",	10, yco-30, 60, 20, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_REDR, "Ob: ", 70, yco-30, 240, 20, &dtar->id, "Object to use as Driver target");
		
		// XXX should we hide these technical details?
		if (dtar->id) {
			uiBlockBeginAlign(block);
				/* RNA Path */
				RNA_pointer_create(ale->id, &RNA_DriverTarget, dtar, &rna_ptr);
				uiDefButR(block, TEX, 0, "Path: ", 10, yco-50, 250, 20, &rna_ptr, "rna_path", 0, 0, 0, -1, -1, "RNA Path (from Driver Object) to property used as Driver.");
					
				/* Array Index */
				uiDefButI(block, NUM, B_REDR, "", 260,yco-50,50,20, &dtar->array_index, 0, INT_MAX, 0, 0, "Index to the specific property used as Driver if applicable.");
			uiBlockEndAlign(block);
		}
		
		/* adjust y-coordinate for next target */
		yco -= height;
		i++;
	}

	MEM_freeN(ale);
}

/* ******************* f-modifiers ******************************** */
/* all the drawing code is in editors/animation/fmodifier_ui.c */

#define B_FMODIFIER_REDRAW		20

static void do_graph_region_modifier_buttons(bContext *C, void *arg, int event)
{
	switch (event) {
		case B_REDR:
		case B_FMODIFIER_REDRAW: // XXX this should send depsgraph updates too
			ED_area_tag_redraw(CTX_wm_area(C));
			return; /* no notifier! */
	}
}

static void graph_panel_modifiers(const bContext *C, Panel *pa)	
{
	bAnimListElem *ale;
	FCurve *fcu;
	FModifier *fcm;
	uiLayout *col, *row;
	uiBlock *block;
	
	if (!graph_panel_context(C, &ale, &fcu))
		return;
	
	block= uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_graph_region_modifier_buttons, NULL);
	
	/* 'add modifier' button at top of panel */
	{
		row= uiLayoutRow(pa->layout, 0);
		block= uiLayoutGetBlock(row);
		
		// XXX for now, this will be a operator button which calls a temporary 'add modifier' operator
		uiDefButO(block, BUT, "GRAPH_OT_fmodifier_add", WM_OP_INVOKE_REGION_WIN, "Add Modifier", 10, 0, 150, 20, "Adds a new F-Curve Modifier for the active F-Curve");
	}
	
	/* draw each modifier */
	for (fcm= fcu->modifiers.first; fcm; fcm= fcm->next) {
		col= uiLayoutColumn(pa->layout, 1);
		
		ANIM_uiTemplate_fmodifier_draw(col, ale->id, &fcu->modifiers, fcm);
	}

	MEM_freeN(ale);
}

/* ******************* general ******************************** */

void graph_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt= MEM_callocN(sizeof(PanelType), "spacetype graph panel properties");
	strcpy(pt->idname, "GRAPH_PT_properties");
	strcpy(pt->label, "Properties");
	pt->draw= graph_panel_properties;
	pt->poll= graph_panel_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype graph panel drivers");
	strcpy(pt->idname, "GRAPH_PT_drivers");
	strcpy(pt->label, "Drivers");
	pt->draw= graph_panel_drivers;
	pt->poll= graph_panel_drivers_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt= MEM_callocN(sizeof(PanelType), "spacetype graph panel modifiers");
	strcpy(pt->idname, "GRAPH_PT_modifiers");
	strcpy(pt->label, "Modifiers");
	pt->draw= graph_panel_modifiers;
	pt->poll= graph_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int graph_properties(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= graph_has_buttons_region(sa);
	
	if(ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void GRAPH_OT_properties(wmOperatorType *ot)
{
	ot->name= "Properties";
	ot->idname= "GRAPH_OT_properties";
	
	ot->exec= graph_properties;
	ot->poll= ED_operator_ipo_active; // xxx
 	
	/* flags */
	ot->flag= 0;
}
