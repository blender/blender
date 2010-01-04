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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
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

/* -------------- */

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

/* -------------- */

/* Graph Editor View Settings */
static void graph_panel_view(const bContext *C, Panel *pa)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	Scene *scene= CTX_data_scene(C);
	PointerRNA spaceptr, sceneptr;
	uiLayout *col, *subcol;
	
	/* get RNA pointers for use when creating the UI elements */
	RNA_id_pointer_create(&scene->id, &sceneptr);
	RNA_pointer_create(&sc->id, &RNA_SpaceGraphEditor, sipo, &spaceptr);

	/* 2D-Cursor */
	col= uiLayoutColumn(pa->layout, 0);
		uiItemR(col, NULL, 0, &spaceptr, "show_cursor", 0);
		
		subcol= uiLayoutColumn(col, 1);
		uiLayoutSetActive(subcol, RNA_boolean_get(&spaceptr, "show_cursor")); 
			uiItemR(subcol, "Cursor X", 0, &sceneptr, "current_frame", 0);
			uiItemR(subcol, "Cursor Y", 0, &spaceptr, "cursor_value", 0);
			
		subcol= uiLayoutColumn(col, 1);
		uiLayoutSetActive(subcol, RNA_boolean_get(&spaceptr, "show_cursor")); 
			uiItemO(subcol, "Cursor from Selection", 0, "GRAPH_OT_frame_jump");
}

/* ******************* active F-Curve ************** */

static void graph_panel_properties(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	PointerRNA fcu_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *col, *row, *subrow;
	uiBlock *block;
	char name[256];
	int icon = 0;

	if (!graph_panel_context(C, &ale, &fcu))
		return;
	
	block= uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL);
	
	/* F-Curve pointer */
	RNA_pointer_create(ale->id, &RNA_FCurve, fcu, &fcu_ptr);
	
	/* user-friendly 'name' for F-Curve */
	// TODO: only show the path if this is invalid?
	col= uiLayoutColumn(layout, 0);
		icon= getname_anim_fcurve(name, ale->id, fcu);
		uiItemL(col, name, icon);
		
	/* RNA-Path Editing - only really should be enabled when things aren't working */
	col= uiLayoutColumn(layout, 1);
		uiLayoutSetEnabled(col, (fcu->flag & FCURVE_DISABLED)); 
		uiItemR(col, "", ICON_RNA, &fcu_ptr, "data_path", 0);
		uiItemR(col, NULL, 0, &fcu_ptr, "array_index", 0);
		
	/* color settings */
	col= uiLayoutColumn(layout, 1);
		uiItemL(col, "Display Color:", 0);
		
		row= uiLayoutRow(col, 1);
			uiItemR(row, "", 0, &fcu_ptr, "color_mode", 0);
			
			subrow= uiLayoutRow(row, 1);
				uiLayoutSetEnabled(subrow, (fcu->color_mode==FCURVE_COLOR_CUSTOM));
				uiItemR(subrow, "", 0, &fcu_ptr, "color", 0);
	
	/* TODO: the following settings could be added here
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
			DAG_ids_flush_update(0);
		}
			break;
	}
	
	/* default for now */
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene); // XXX could use better notifier
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
	
	/* add a new variable */
	driver_add_new_variable(driver);
}

/* callback to remove target variable from active driver */
static void driver_delete_var_cb (bContext *C, void *driver_v, void *dvar_v)
{
	ChannelDriver *driver= (ChannelDriver *)driver_v;
	DriverVar *dvar= (DriverVar *)dvar_v;
	
	/* remove the active variable */
	driver_free_variable(driver, dvar);
}

/* callback to reset the driver's flags */
static void driver_update_flags_cb (bContext *C, void *fcu_v, void *dummy_v)
{
	FCurve *fcu= (FCurve *)fcu_v;
	ChannelDriver *driver= fcu->driver;
	
	/* clear invalid flags */
	fcu->flag &= ~FCURVE_DISABLED; // XXX?
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


/* settings for 'single property' driver variable type */
static void graph_panel_driverVar__singleProp(const bContext *C, uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar= &dvar->targets[0];
	PointerRNA dtar_ptr;
	uiLayout *row, *col;
	uiBlock *block;
	
	/* initialise RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	
	/* Target ID */
	row= uiLayoutRow(layout, 0);
		uiTemplateAnyID(row, (bContext *)C, &dtar_ptr, "id", "id_type", "Value:");
	
	/* Target Property */
	// TODO: make this less technical...
	if (dtar->id) {
		PointerRNA root_ptr;
		
		/* get pointer for resolving the property selected */
		RNA_id_pointer_create(dtar->id, &root_ptr);
		
		col= uiLayoutColumn(layout, 1);
		block= uiLayoutGetBlock(col);
			/* rna path */
			uiTemplatePathBuilder(col, (bContext *)C, &dtar_ptr, "data_path", &root_ptr, "Path");
	}
}

/* settings for 'rotation difference' driver variable type */
static void graph_panel_driverVar__rotDiff(const bContext *C, uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar= &dvar->targets[0];
	DriverTarget *dtar2= &dvar->targets[1];
	Object *ob1 = (Object *)dtar->id;
	Object *ob2 = (Object *)dtar2->id;
	PointerRNA dtar_ptr, dtar2_ptr;
	uiLayout *col;
	
	/* initialise RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	RNA_pointer_create(id, &RNA_DriverTarget, dtar2, &dtar2_ptr); 
	
	/* Bone 1 */
	col= uiLayoutColumn(layout, 1);
		uiTemplateAnyID(col, (bContext *)C, &dtar_ptr, "id", "id_type", "Bone 1:");
		
		if (dtar->id && ob1->pose) {
			PointerRNA tar_ptr;
			
			RNA_pointer_create(dtar2->id, &RNA_Pose, ob1->pose, &tar_ptr);
			uiItemPointerR(col, "", ICON_BONE_DATA, &dtar_ptr, "bone_target", &tar_ptr, "bones");
		}
	
	col= uiLayoutColumn(layout, 1);
		uiTemplateAnyID(col, (bContext *)C, &dtar2_ptr, "id", "id_type", "Bone 2:");
		
		if (dtar2->id && ob2->pose) {
			PointerRNA tar_ptr;
			
			RNA_pointer_create(dtar2->id, &RNA_Pose, ob2->pose, &tar_ptr);
			uiItemPointerR(col, "", ICON_BONE_DATA, &dtar2_ptr, "bone_target", &tar_ptr, "bones");
		}
}

/* settings for 'rotation difference' driver variable type */
static void graph_panel_driverVar__locDiff(const bContext *C, uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar= &dvar->targets[0];
	DriverTarget *dtar2= &dvar->targets[1];
	Object *ob1 = (Object *)dtar->id;
	Object *ob2 = (Object *)dtar2->id;
	PointerRNA dtar_ptr, dtar2_ptr;
	uiLayout *col;
	
	/* initialise RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	RNA_pointer_create(id, &RNA_DriverTarget, dtar2, &dtar2_ptr); 
	
	/* Bone 1 */
	col= uiLayoutColumn(layout, 1);
		uiTemplateAnyID(col, (bContext *)C, &dtar_ptr, "id", "id_type", "Ob/Bone 1:");
		
		if (dtar->id && ob1->pose) {
			PointerRNA tar_ptr;
			
			RNA_pointer_create(dtar2->id, &RNA_Pose, ob1->pose, &tar_ptr);
			uiItemPointerR(col, "", ICON_BONE_DATA, &dtar_ptr, "bone_target", &tar_ptr, "bones");
		}
	
	col= uiLayoutColumn(layout, 1);
		uiTemplateAnyID(col, (bContext *)C, &dtar2_ptr, "id", "id_type", "Ob/Bone 2:");
		
		if (dtar2->id && ob2->pose) {
			PointerRNA tar_ptr;
			
			RNA_pointer_create(dtar2->id, &RNA_Pose, ob2->pose, &tar_ptr);
			uiItemPointerR(col, "", ICON_BONE_DATA, &dtar2_ptr, "bone_target", &tar_ptr, "bones");
		}
}

/* driver settings for active F-Curve (only for 'Drivers' mode) */
static void graph_panel_drivers(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	ChannelDriver *driver;
	DriverVar *dvar;
	
	PointerRNA driver_ptr;
	uiLayout *col;
	uiBlock *block;
	uiBut *but;
	
	/* Get settings from context */
	if (!graph_panel_context(C, &ale, &fcu))
		return;
	driver= fcu->driver;
	
	/* set event handler for panel */
	block= uiLayoutGetBlock(pa->layout); // xxx?
	uiBlockSetHandleFunc(block, do_graph_region_driver_buttons, NULL);
	
	/* general actions - management */
	col= uiLayoutColumn(pa->layout, 0);
	block= uiLayoutGetBlock(col);
		but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Update Dependencies", 0, 0, 10*UI_UNIT_X, 22, NULL, 0.0, 0.0, 0, 0, "Force updates of dependencies");
		uiButSetFunc(but, driver_update_flags_cb, fcu, NULL);
		
		but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Remove Driver", 0, 0, 10*UI_UNIT_X, 18, NULL, 0.0, 0.0, 0, 0, "Remove this driver");
		uiButSetNFunc(but, driver_remove_cb, MEM_dupallocN(ale), NULL);
		
	/* driver-level settings - type, expressions, and errors */
	RNA_pointer_create(ale->id, &RNA_Driver, driver, &driver_ptr);
	
	col= uiLayoutColumn(pa->layout, 1);
	block= uiLayoutGetBlock(col);
		uiItemR(col, NULL, 0, &driver_ptr, "type", 0);
		
		/* show expression box if doing scripted drivers, and/or error messages when invalid drivers exist */
		if (driver->type == DRIVER_TYPE_PYTHON) {
			/* expression */
			uiItemR(col, "Expr", 0, &driver_ptr, "expression", 0);
			
			/* errors? */
			if (driver->flag & DRIVER_FLAG_INVALID)
				uiItemL(col, "ERROR: invalid Python expression", ICON_ERROR);
		}
		else {
			/* errors? */
			if (driver->flag & DRIVER_FLAG_INVALID)
				uiItemL(col, "ERROR: invalid target channel(s)", ICON_ERROR);
		}
	
	/* add driver variables */
	col= uiLayoutColumn(pa->layout, 0);
	block= uiLayoutGetBlock(col);
		but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Add Variable", 0, 0, 10*UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "Add a new target variable for this Driver");
		uiButSetFunc(but, driver_add_var_cb, driver, NULL);
	
	/* loop over targets, drawing them */
	for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
		PointerRNA dvar_ptr;
		uiLayout *box, *row;
		
		/* sub-layout column for this variable's settings */
		col= uiLayoutColumn(pa->layout, 1);
		
		/* header panel */
		box= uiLayoutBox(col);
			/* first row context info for driver */
			RNA_pointer_create(ale->id, &RNA_DriverVariable, dvar, &dvar_ptr);
			
			row= uiLayoutRow(box, 0);
			block= uiLayoutGetBlock(row);
				/* variable name */
				uiItemR(row, "", 0, &dvar_ptr, "name", 0);
				
				/* remove button */
				uiBlockSetEmboss(block, UI_EMBOSSN);
					but= uiDefIconBut(block, BUT, B_IPO_DEPCHANGE, ICON_X, 290, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0.0, 0.0, "Delete target variable.");
					uiButSetFunc(but, driver_delete_var_cb, driver, dvar);
				uiBlockSetEmboss(block, UI_EMBOSS);
			
			/* variable type */
			row= uiLayoutRow(box, 0);
				uiItemR(row, "", 0, &dvar_ptr, "type", 0);
				
		/* variable type settings */
		box= uiLayoutBox(col);
			/* controls to draw depends on the type of variable */
			switch (dvar->type) {
				case DVAR_TYPE_SINGLE_PROP:	/* single property */
					graph_panel_driverVar__singleProp(C, box, ale->id, dvar);
					break;
				case DVAR_TYPE_ROT_DIFF: /* rotational difference */
					graph_panel_driverVar__rotDiff(C, box, ale->id, dvar);
					break;
				case DVAR_TYPE_LOC_DIFF: /* location difference */
					graph_panel_driverVar__locDiff(C, box, ale->id, dvar);
					break;
			}
	}
	
	/* cleanup */
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
			WM_event_add_notifier(C, NC_ANIMATION, NULL); // XXX need a notifier specially for F-Modifiers
			break;
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

	pt= MEM_callocN(sizeof(PanelType), "spacetype graph panel view");
	strcpy(pt->idname, "GRAPH_PT_view");
	strcpy(pt->label, "View Properties");
	pt->draw= graph_panel_view;
	BLI_addtail(&art->paneltypes, pt);
	
	pt= MEM_callocN(sizeof(PanelType), "spacetype graph panel properties");
	strcpy(pt->idname, "GRAPH_PT_properties");
	strcpy(pt->label, "Active F-Curve");
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
