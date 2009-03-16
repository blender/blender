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

static void graph_panel_properties(const bContext *C, ARegion *ar, short cntrl, bAnimListElem *ale)	
{
	FCurve *fcu= (FCurve *)ale->data;
	uiBlock *block;
	char name[128];

	block= uiBeginBlock(C, ar, "graph_panel_properties", UI_EMBOSS, UI_HELV);
	if (uiNewPanel(C, ar, block, "Properties", "Graph", 340, 30, 318, 254)==0) return;
	uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL);

	/* to force height */
	uiNewPanelHeight(block, 204);

	/* Info - Active F-Curve */
	uiDefBut(block, LABEL, 1, "Active F-Curve:",					10, 200, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	if (ale->id) { 
		// icon of active blocktype - is this really necessary?
		int icon= geticon_anim_blocktype(GS(ale->id->name));
		
		// xxx type of icon-but is currently "LABEL", as that one is plain...
		uiDefIconBut(block, LABEL, 1, icon, 10, 180, 20, 19, NULL, 0, 0, 0, 0, "ID-type that F-Curve belongs to");
	}
	
	getname_anim_fcurve(name, ale->id, fcu);
	uiDefBut(block, LABEL, 1, name,	30, 180, 300, 19, NULL, 0.0, 0.0, 0, 0, "Name of Active F-Curve");
	
	/* TODO: the following settings could be added here
	 *	- F-Curve coloring mode - mode selector + color selector
	 *	- Access details (ID-block + RNA-Path + Array Index)
	 *	- ...
	 */
}

/* -------------- */

#define B_IPO_DEPCHANGE 	10

static void do_graph_region_driver_buttons(bContext *C, void *arg, int event)
{
	Scene *scene= CTX_data_scene(C);
	
	switch(event) {
		case B_IPO_DEPCHANGE:
		{
			/* rebuild depsgraph for the new deps */
			DAG_scene_sort(scene);
			
			/* TODO: which one? we need some way of sending these updates since curves from non-active ob could be being edited */
			//DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			//DAG_object_flush_update(scene, ob, OB_RECALC_OB);
		}
			break;
	}
	
	/* default for now */
	//WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
}

static void graph_panel_drivers(const bContext *C, ARegion *ar, short cntrl, bAnimListElem *ale)	
{
	FCurve *fcu= (FCurve *)ale->data;
	ChannelDriver *driver= fcu->driver;
	uiBlock *block;
	uiBut *but;
	int len;

	block= uiBeginBlock(C, ar, "graph_panel_drivers", UI_EMBOSS, UI_HELV);
	if (uiNewPanel(C, ar, block, "Drivers", "Graph", 340, 30, 318, 254)==0) return;
	uiBlockSetHandleFunc(block, do_graph_region_driver_buttons, NULL);

	/* to force height */
	uiNewPanelHeight(block, 204);
	
	/* type */
	uiDefBut(block, LABEL, 1, "Type:",					10, 200, 120, 20, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButI(block, MENU, B_IPO_DEPCHANGE,
					"Driver Type%t|Transform Channel%x0|Scripted Expression%x1|Rotational Difference%x2", 
					130,200,180,20, &driver->type, 0, 0, 0, 0, "Driver type");
					
	/* buttons to draw depends on type of driver */
	if (driver->type == DRIVER_TYPE_PYTHON) { /* PyDriver */
		uiDefBut(block, TEX, B_REDR, "Expr: ", 10,160,300,20, driver->expression, 0, 255, 0, 0, "One-liner Python Expression to use as Scripted Expression");
		
		if (driver->flag & DRIVER_FLAG_INVALID) {
			uiDefIconBut(block, LABEL, 1, ICON_ERROR, 10, 140, 20, 19, NULL, 0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "Error: invalid Python expression",
					30,140,230,19, NULL, 0, 0, 0, 0, "");
		}
	}
	else { /* Channel or RotDiff - RotDiff just has extra settings */
		/* Driver Object */
		but= uiDefBut(block, TEX, B_IPO_DEPCHANGE, "OB: ",	10,160,150,20, driver->id->name+2, 0.0, 21.0, 0, 0, "Object that controls this Driver.");
		uiButSetFunc(but, test_idbutton_cb, driver->id->name, NULL);
		
		// XXX should we hide these technical details?
		if (driver->id) {
			/* Array Index */
			// XXX ideally this is grouped with the path, but that can get quite long...
			uiDefButI(block, NUM, B_IPO_DEPCHANGE, "Index: ", 170,160,140,20, &driver->array_index, 0, INT_MAX, 0, 0, "Index to the specific property used as Driver if applicable.");
			
			/* RNA-Path - allocate if non-existant */
			if (driver->rna_path == NULL) {
				driver->rna_path= MEM_callocN(256, "Driver RNA-Path");
				len= 255;
			}
			else
				len= strlen(driver->rna_path);
			uiDefBut(block, TEX, B_IPO_DEPCHANGE, "Path: ", 10,130,300,20, driver->rna_path, 0, len, 0, 0, "RNA Path (from Driver Object) to property used as Driver.");
		}
		
		/* for rotational difference, show second target... */
		if (driver->type == DRIVER_TYPE_ROTDIFF) {
			// TODO...
		}
	}
}

/* -------------- */

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

/* macro for use here to draw background box and set height */
// XXX for now, roundbox has it's callback func set to NULL to not intercept events
#define DRAW_BACKDROP(height) \
	{ \
		if (active) uiBlockSetCol(block, TH_BUT_ACTION); \
			but= uiDefBut(block, ROUNDBOX, B_REDR, "", 10-8, *yco-height, width, height-1, NULL, 5.0, 0.0, 12.0, (float)rb_col, ""); \
			uiButSetFunc(but, NULL, NULL, NULL); \
		if (active) uiBlockSetCol(block, TH_AUTO); \
	}

/* callback to verify modifier data */
static void validate_fmodifier_cb (bContext *C, void *fcu_v, void *fcm_v)
{
	FModifier *fcm= (FModifier *)fcm_v;
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	
	/* call the verify callback on the modifier if applicable */
	if (fmi && fmi->verify_data)
		fmi->verify_data(fcm);
}
	
/* draw settings for generator modifier */
static void _draw_modifier__generator(uiBlock *block, FCurve *fcu, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	char gen_mode[]="Generator Type%t|Expanded Polynomial%x0|Factorised Polynomial%x1|Built-In Function%x2|Expression%x3";
	//char fn_type[]="Built-In Function%t|Sin%x0|Cos%x1|Tan%x2|Square Root%x3|Natural Log%x4";
	int cy= *yco - 30;
	uiBut *but;
	
	/* set the height */
	(*height) = 70;
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
			(*height) += 20*(data->poly_order+1) + 35;
			break;
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial */
			(*height) += 25 * data->poly_order;
			break;
		case FCM_GENERATOR_FUNCTION: /* builtin function */
			(*height) += 50; // xxx
			break;
		case FCM_GENERATOR_EXPRESSION: /* py-expression */
			// xxx nothing to draw 
			break;
	}
	
	/* basic settings (backdrop + mode selector + some padding) */
	//DRAW_BACKDROP((*height)); // XXX buggy...
	but= uiDefButS(block, MENU, /*B_FMODIFIER_REDRAW*/B_REDR, gen_mode, 10,cy,width-30,19, &data->mode, 0, 0, 0, 0, "Selects type of generator algorithm.");
	uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
	cy -= 35;
	
	/* now add settings for individual modifiers */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
		{
			float *cp = NULL;
			char xval[32];
			unsigned int i;
			
			/* draw polynomial order selector */
				// XXX this needs validation!
			but= uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Poly Order: ", 10,cy,width-30,19, &data->poly_order, 1, 100, 0, 0, "'Order' of the Polynomial - for a polynomial with n terms, 'order' is n-1");
			uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
			cy -= 35;
			
			/* draw controls for each coefficient and a + sign at end of row */
			cp= data->coefficients;
			for (i=0; (i < data->arraysize) && (cp); i++, cp++) {
				/* coefficient */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 50, cy, 150, 20, cp, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient for polynomial");
				
				/* 'x' param (and '+' if necessary) */
				if (i == 0)
					strcpy(xval, "");
				else if (i == 1)
					strcpy(xval, "x");
				else
					sprintf(xval, "x^%d", i);
				uiDefBut(block, LABEL, 1, xval, 200, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "Power of x");
				
				if ( (i != (data->arraysize - 1)) || ((i==0) && data->arraysize==2) )
					uiDefBut(block, LABEL, 1, "+", 300, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "Power of x");
				
				cy -= 20;
			}
		}
			break;
		
		case FCM_GENERATOR_EXPRESSION: /* py-expression */
			// TODO...
			break;
	}
}


static void graph_panel_modifier_draw(uiBlock *block, FCurve *fcu, FModifier *fcm, int *yco)
{
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	uiBut *but;
	short active= (fcm->flag & FMODIFIER_FLAG_ACTIVE);
	short width= 314;
	short height = 0; 
	int rb_col;
	
	/* draw header */
	{
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* rounded header */
#if 0 // XXX buggy...
		if (active) uiBlockSetCol(block, TH_BUT_ACTION);
			rb_col= (active)?-20:20;
			but= uiDefBut(block, ROUNDBOX, B_REDR, "", 10-8, *yco-2, width, 24, NULL, 5.0, 0.0, 15.0, (float)(rb_col-20), ""); 
		if (active) uiBlockSetCol(block, TH_AUTO);
#endif // XXX buggy
		
		/* expand */
		uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_EXPANDED, B_REDR, ICON_TRIA_RIGHT,	10-7, *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is expanded");
		
		/* name */
		if (fmi)
			uiDefBut(block, LABEL, 1, fmi->name,	10+35, *yco, 240, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type");
		else
			uiDefBut(block, LABEL, 1, "<Unknown Modifier>",	10+35, *yco, 240, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type");
		
		/* delete button */
		but= uiDefIconBut(block, BUT, B_REDR, ICON_X, 10+(width-30), *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete layer");
		//uiButSetFunc(but, gp_ui_dellayer_cb, gpd, NULL);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* when modifier is expanded, draw settings */
	if (fcm->flag & FMODIFIER_FLAG_EXPANDED) {
		/* draw settings for individual modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR: /* Generator */
				_draw_modifier__generator(block, fcu, fcm, yco, &height, width, active, rb_col);
				break;
			
			default: /* unknown type */
				height= 96;
				//DRAW_BACKDROP(height); // XXX buggy...
				break;
		}
	}
	
	/* adjust height for new to start */
	(*yco) -= (height + 27); 
}

static void graph_panel_modifiers(const bContext *C, ARegion *ar, short cntrl, bAnimListElem *ale)	
{
	FCurve *fcu= (FCurve *)ale->data;
	FModifier *fcm;
	uiBlock *block;
	int yco= 190;
	
	block= uiBeginBlock(C, ar, "graph_panel_modifiers", UI_EMBOSS, UI_HELV);
	if (uiNewPanel(C, ar, block, "Modifiers", "Graph", 340, 30, 318, 254)==0) return;
	uiBlockSetHandleFunc(block, do_graph_region_modifier_buttons, NULL);
	
	uiNewPanelHeight(block, 204);
	
	/* 'add modifier' button at top of panel */
	// XXX for now, this will be a operator button which calls a temporary 'add modifier' operator
	uiDefButO(block, BUT, "GRAPHEDIT_OT_fmodifier_add", WM_OP_INVOKE_REGION_WIN, "Add Modifier", 10, 225, 150, 20, "Adds a new F-Curve Modifier for the active F-Curve");
	
	/* draw each modifier */
	for (fcm= fcu->modifiers.first; fcm; fcm= fcm->next)
		graph_panel_modifier_draw(block, fcu, fcm, &yco);
	
	/* since these buttons can have variable height */
	if (yco < 0)
		uiNewPanelHeight(block, (204 - yco));
	else
		uiNewPanelHeight(block, 204);
}

/* -------------- */

/* Find 'active' F-Curve. It must be editable, since that's the purpose of these buttons (subject to change).  
 * We return the 'wrapper' since it contains valuable context info (about hierarchy), which will need to be freed 
 * when the caller is done with it.
 */
// TODO: move this to anim api with another name?
bAnimListElem *get_active_fcurve_channel (bAnimContext *ac)
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
		
	/* for now, the properties panel displays info about the selected channels */
	graph_panel_properties(C, ar, 0, ale);
	
	/* driver settings for active F-Curve (only for 'Drivers' mode) */
	if (sipo->mode == SIPO_MODE_DRIVERS)
		graph_panel_drivers(C, ar, 0, ale);
	
	/* modifiers */
	graph_panel_modifiers(C, ar, 0, ale);
	

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
