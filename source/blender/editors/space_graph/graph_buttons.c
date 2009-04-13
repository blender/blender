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
	//WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
}

/* callback to copy over RNA-Paths accordingly */
static void driver_rnapath_copy_cb (bContext *C, void *driver_v, void *strbuf_v)
{
	ChannelDriver *driver= (ChannelDriver *)driver_v;
	char *stringBuf= (char *)strbuf_v;
	
	/* copy over string */
	if (driver->rna_path)
		MEM_freeN(driver->rna_path);
	driver->rna_path= BLI_strdupn(stringBuf, strlen(stringBuf));
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

/* callback to reset the driver's flags */
static void driver_update_flags_cb (bContext *C, void *fcu_v, void *dummy_v)
{
	FCurve *fcu= (FCurve *)fcu_v;
	ChannelDriver *driver= fcu->driver;
	
	/* clear invalid flags */
	driver->flag &= ~DRIVER_FLAG_INVALID;
}

static void graph_panel_drivers(const bContext *C, ARegion *ar, short cntrl, bAnimListElem *ale)	
{
	FCurve *fcu= (FCurve *)ale->data;
	ChannelDriver *driver= fcu->driver;
	uiBlock *block;
	uiBut *but;

	block= uiBeginBlock(C, ar, "graph_panel_drivers", UI_EMBOSS, UI_HELV);
	if (uiNewPanel(C, ar, block, "Drivers", "Graph", 340, 30, 318, 254)==0) return;
	uiBlockSetHandleFunc(block, do_graph_region_driver_buttons, NULL);

	/* to force height */
	uiNewPanelHeight(block, 204);
	
	/* general actions */
	but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Update Dependencies", 10, 200, 180, 22, NULL, 0.0, 0.0, 0, 0, "Force updates of dependencies");
	uiButSetFunc(but, driver_update_flags_cb, fcu, NULL);
	
	but= uiDefBut(block, BUT, B_IPO_DEPCHANGE, "Remove Driver", 200, 200, 110, 18, NULL, 0.0, 0.0, 0, 0, "Remove this driver");
	uiButSetFunc(but, driver_remove_cb, ale, NULL);
	
	/* type */
	uiDefBut(block, LABEL, 1, "Type:",					10, 170, 60, 20, NULL, 0.0, 0.0, 0, 0, "");
	uiDefButI(block, MENU, B_IPO_DEPCHANGE,
					"Driver Type%t|Transform Channel%x0|Scripted Expression%x1|Rotational Difference%x2", 
					70,170,230,20, &driver->type, 0, 0, 0, 0, "Driver type");
					
	/* buttons to draw depends on type of driver */
	if (driver->type == DRIVER_TYPE_PYTHON) { /* PyDriver */
		uiDefBut(block, TEX, B_REDR, "Expr: ", 10,130,300,20, driver->expression, 0, 255, 0, 0, "One-liner Python Expression to use as Scripted Expression");
		
		/* errors */
		if (driver->flag & DRIVER_FLAG_INVALID) {
			uiDefIconBut(block, LABEL, 1, ICON_ERROR, 10, 110, 32, 32, NULL, 0, 0, 0, 0, ""); // a bit larger
			uiDefBut(block, LABEL, 0, "Error: invalid Python expression",
					30,110,230,19, NULL, 0, 0, 0, 0, "");
		}
	}
	else { /* Channel or RotDiff - RotDiff just has extra settings */
		/* Driver Object */
		uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_REDR,
			               "Ob: ", 10, 130, 150, 20, &driver->id, "Object to use as Driver target");
		
		// XXX should we hide these technical details?
		if (driver->id) {
			static char pathBuf[512]; 	/* bad... evil... */
		
			/* Array Index */
			// XXX ideally this is grouped with the path, but that can get quite long...
			uiDefButI(block, NUM, B_REDR, "Index: ", 170,130,140,20, &driver->array_index, 0, INT_MAX, 0, 0, "Index to the specific property used as Driver if applicable.");
			
			/* RNA Path */
			if (driver->rna_path == NULL)
				pathBuf[0]= '\0';
			else
				BLI_snprintf(pathBuf, 512, driver->rna_path);
			
			but= uiDefButC(block, TEX, B_REDR, "Path: ", 10,100,300,20, pathBuf, 0, 511, 0, 0, "RNA Path (from Driver Object) to property used as Driver.");
			uiButSetFunc(but, driver_rnapath_copy_cb, driver, pathBuf);
		}
		
		/* for rotational difference, show second target... */
		if (driver->type == DRIVER_TYPE_ROTDIFF) {
			// TODO...
		}
		
		/* errors */
		if (driver->flag & DRIVER_FLAG_INVALID) {
			uiDefIconBut(block, LABEL, 1, ICON_ERROR, 10, 70, 32, 32, NULL, 0, 0, 0, 0, ""); // a bit larger
			uiDefBut(block, LABEL, 0, "Error: invalid target channel",
					30,70,230,19, NULL, 0, 0, 0, 0, "");
		}
	}
}

/* ******************* f-modifiers ******************************** */

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

/* callback to set the active modifier */
static void activate_fmodifier_cb (bContext *C, void *fcu_v, void *fcm_v)
{
	FCurve *fcu= (FCurve *)fcu_v;
	FModifier *fcm= (FModifier *)fcm_v;
	
	/* call API function to set the active modifier for active F-Curve */
	fcurve_set_active_modifier(fcu, fcm);
}

/* callback to remove the given modifier  */
static void delete_fmodifier_cb (bContext *C, void *fcu_v, void *fcm_v)
{
	FCurve *fcu= (FCurve *)fcu_v;
	FModifier *fcm= (FModifier *)fcm_v;
	
	/* remove the given F-Modifier from the F-Curve */
	fcurve_remove_modifier(fcu, fcm);
}

/* --------------- */
	
/* draw settings for generator modifier */
static void draw_modifier__generator(uiBlock *block, FCurve *fcu, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	char gen_mode[]="Generator Type%t|Expanded Polynomial%x0|Factorised Polynomial%x1|Built-In Function%x2|Expression%x3";
	char fn_type[]="Built-In Function%t|Sin%x0|Cos%x1|Tan%x2|Square Root%x3|Natural Log%x4";
	int cy= *yco - 30;
	uiBut *but;
	
	/* set the height */
	(*height) = 90;
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
			(*height) += 20*(data->poly_order+1) + 35;
			break;
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial */
			(*height) += 20 * data->poly_order;
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
	uiBlockBeginAlign(block);
		but= uiDefButS(block, MENU, B_FMODIFIER_REDRAW, gen_mode, 10,cy,width-30,19, &data->mode, 0, 0, 0, 0, "Selects type of generator algorithm.");
		uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
		cy -= 20;
		
		uiDefButBitS(block, TOG, FCM_GENERATOR_ADDITIVE, B_FMODIFIER_REDRAW, "Additive", 10,cy,width-30,19, &data->flag, 0, 0, 0, 0, "Values generated by this modifier are applied on top of the existing values instead of overwriting them");
		cy -= 35;
	uiBlockEndAlign(block);
	
	/* now add settings for individual modes */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
		{
			float *cp = NULL;
			char xval[32];
			unsigned int i;
			
			/* draw polynomial order selector */
			but= uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Poly Order: ", 10,cy,width-30,19, &data->poly_order, 1, 100, 0, 0, "'Order' of the Polynomial - for a polynomial with n terms, 'order' is n-1");
			uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
			cy -= 35;
			
			/* draw controls for each coefficient and a + sign at end of row */
			uiDefBut(block, LABEL, 1, "y = ", 0, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
			
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
					uiDefBut(block, LABEL, 1, "+", 250, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				cy -= 20;
			}
		}
			break;
		
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial expression */
		{
			float *cp = NULL;
			unsigned int i;
			
			/* draw polynomial order selector */
			but= uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Poly Order: ", 10,cy,width-30,19, &data->poly_order, 1, 100, 0, 0, "'Order' of the Polynomial - for a polynomial with n terms, 'order' is n-1");
			uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
			cy -= 35;
			
			/* draw controls for each pair of coefficients */
			uiDefBut(block, LABEL, 1, "y = ", 0, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
			
			cp= data->coefficients;
			for (i=0; (i < data->poly_order) && (cp); i++, cp+=2) {
				/* opening bracket */
				uiDefBut(block, LABEL, 1, "(", 40, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				/* coefficients */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 50, cy, 100, 20, cp, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient of x");
				
				uiDefBut(block, LABEL, 1, "x + ", 150, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 180, cy, 100, 20, cp+1, -FLT_MAX, FLT_MAX, 10, 3, "Second coefficient");
				
				/* closing bracket and '+' sign */
				if ( (i != (data->poly_order - 1)) || ((i==0) && data->poly_order==2) )
					uiDefBut(block, LABEL, 1, ") ×", 280, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				else
					uiDefBut(block, LABEL, 1, ")", 280, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				cy -= 20;
			}
		}
			break;
		
		case FCM_GENERATOR_FUNCTION: /* built-in function */
		{
			float *cp= data->coefficients;
			
			/* draw function selector */
			but= uiDefButS(block, MENU, B_FMODIFIER_REDRAW, fn_type, 10,cy,width-30,19, &data->func_type, 0, 0, 0, 0, "Built-In Function to use");
			uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
			cy -= 35;
			
			/* draw controls for equation of coefficients */
			/* row 1 */
			{
				uiDefBut(block, LABEL, 1, "y = ", 0, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 50, cy, 150, 20, cp+3, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient (D) for function");
				uiDefBut(block, LABEL, 1, "+", 200, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				cy -= 20;
			}
			
			/* row 2 */
			{
				char func_name[32];
				
				/* coefficient outside bracket */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 0, cy, 80, 20, cp, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient (A) for function");
				
				/* opening bracket */
				switch (data->func_type)
				{		
					case FCM_GENERATOR_FN_SIN: /* sine wave */
						sprintf(func_name, "sin(");
						break;
					case FCM_GENERATOR_FN_COS: /* cosine wave */
						sprintf(func_name, "cos(");
						break;
					case FCM_GENERATOR_FN_TAN: /* tangent wave */
						sprintf(func_name, "tan(");
						break;
					case FCM_GENERATOR_FN_LN: /* natural log */
						sprintf(func_name, "ln(");
						break;
					case FCM_GENERATOR_FN_SQRT: /* square root */
						sprintf(func_name, "sqrt(");
						break;
					default: /* unknown */
						sprintf(func_name, "<fn?>(");
						break;
				}
				uiDefBut(block, LABEL, 1, func_name, 80, cy, 40, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				/* coefficients inside bracket */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 115, cy, 75, 20, cp+1, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient (B) of x");
				
				uiDefBut(block, LABEL, 1, "x+", 190, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 220, cy, 80, 20, cp+2, -FLT_MAX, FLT_MAX, 10, 3, "Coefficient (C) of function");
				
				/* closing bracket */
					uiDefBut(block, LABEL, 1, ")", 300, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				cy -= 20;
			}
		}
			break;
		
		case FCM_GENERATOR_EXPRESSION: /* py-expression */
			// TODO...
			break;
	}
}

/* --------------- */

/* draw settings for cycles modifier */
static void draw_modifier__cycles(uiBlock *block, FCurve *fcu, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Cycles *data= (FMod_Cycles *)fcm->data;
	char cyc_mode[]="Cycling Mode%t|No Cycles%x0|Repeat Motion%x1|Repeat with Offset%x2";
	int cy= (*yco - 30), cy1= (*yco - 50), cy2= (*yco - 70);
	
	/* set the height */
	(*height) = 90;
	
	/* basic settings (backdrop + some padding) */
	//DRAW_BACKDROP((*height)); // XXX buggy...
	
	/* 'before' range */
	uiDefBut(block, LABEL, 1, "Before:", 10, cy, 80, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling before first keyframe");
	uiBlockBeginAlign(block);
		uiDefButS(block, MENU, B_FMODIFIER_REDRAW, cyc_mode, 10,cy1,150,20, &data->before_mode, 0, 0, 0, 0, "Cycling mode to use before first keyframe");
		uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Max Cycles:", 10, cy2, 150, 20, &data->before_cycles, 0, 10000, 10, 3, "Maximum number of cycles to allow (0 = infinite)");
	uiBlockEndAlign(block);
	
	/* 'after' range */
	uiDefBut(block, LABEL, 1, "After:", 160, cy, 80, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling after last keyframe");
	uiBlockBeginAlign(block);
		uiDefButS(block, MENU, B_FMODIFIER_REDRAW, cyc_mode, 170,cy1,150,20, &data->after_mode, 0, 0, 0, 0, "Cycling mode to use after first keyframe");
		uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Max Cycles:", 170, cy2, 150, 20, &data->after_cycles, 0, 10000, 10, 3, "Maximum number of cycles to allow (0 = infinite)");
	uiBlockEndAlign(block);
}

/* --------------- */

#define BINARYSEARCH_FRAMEEQ_THRESH	0.0001

/* Binary search algorithm for finding where to insert Envelope Data Point.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int binarysearch_fcm_envelopedata_index (FCM_EnvelopeData array[], float frame, int arraylen, short *exists)
{
	int start=0, end=arraylen;
	int loopbreaker= 0, maxloop= arraylen * 2;
	
	/* initialise exists-flag first */
	*exists= 0;
	
	/* sneaky optimisations (don't go through searching process if...):
	 *	- keyframe to be added is to be added out of current bounds
	 *	- keyframe to be added would replace one of the existing ones on bounds
	 */
	if ((arraylen <= 0) || (array == NULL)) {
		printf("Warning: binarysearch_fcm_envelopedata_index() encountered invalid array \n");
		return 0;
	}
	else {
		/* check whether to add before/after/on */
		float framenum;
		
		/* 'First' Point (when only one point, this case is used) */
		framenum= array[0].time;
		if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists = 1;
			return 0;
		}
		else if (frame < framenum)
			return 0;
			
		/* 'Last' Point */
		framenum= array[(arraylen-1)].time;
		if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists= 1;
			return (arraylen - 1);
		}
		else if (frame > framenum)
			return arraylen;
	}
	
	
	/* most of the time, this loop is just to find where to put it
	 * 	- 'loopbreaker' is just here to prevent infinite loops 
	 */
	for (loopbreaker=0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
		/* compute and get midpoint */
		int mid = start + ((end - start) / 2);	/* we calculate the midpoint this way to avoid int overflows... */
		float midfra= array[mid].time;
		
		/* check if exactly equal to midpoint */
		if (IS_EQT(frame, midfra, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists = 1;
			return mid;
		}
		
		/* repeat in upper/lower half */
		if (frame > midfra)
			start= mid + 1;
		else if (frame < midfra)
			end= mid - 1;
	}
	
	/* print error if loop-limit exceeded */
	if (loopbreaker == (maxloop-1)) {
		printf("Error: binarysearch_fcm_envelopedata_index() was taking too long \n");
		
		// include debug info 
		printf("\tround = %d: start = %d, end = %d, arraylen = %d \n", loopbreaker, start, end, arraylen);
	}
	
	/* not found, so return where to place it */
	return start;
}

/* callback to add new envelope data point */
// TODO: should we have a separate file for things like this?
static void fmod_envelope_addpoint_cb (bContext *C, void *fcm_dv, void *dummy)
{
	Scene *scene= CTX_data_scene(C);
	FMod_Envelope *env= (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	FCM_EnvelopeData fed;
	
	/* init template data */
	fed.min= -1.0f;
	fed.max= 1.0f;
	fed.time= (float)scene->r.cfra; // XXX make this int for ease of use?
	fed.f1= fed.f2= 0;
	
	/* check that no data exists for the current frame... */
	if (env->data) {
		short exists = -1;
		int i= binarysearch_fcm_envelopedata_index(env->data, (float)(scene->r.cfra), env->totvert, &exists);
		
		/* binarysearch_...() will set exists by default to 0, so if it is non-zero, that means that the point exists already */
		if (exists)
			return;
			
		/* add new */
		fedn= MEM_callocN((env->totvert+1)*sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		
		/* add the points that should occur before the point to be pasted */
		if (i > 0)
			memcpy(fedn, env->data, i*sizeof(FCM_EnvelopeData));
		
		/* add point to paste at index i */
		*(fedn + i)= fed;
		
		/* add the points that occur after the point to be pasted */
		if (i < env->totvert) 
			memcpy(fedn+i+1, env->data+i, (env->totvert-i)*sizeof(FCM_EnvelopeData));
		
		/* replace (+ free) old with new */
		MEM_freeN(env->data);
		env->data= fedn;
		
		env->totvert++;
	}
	else {
		env->data= MEM_callocN(sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		*(env->data)= fed;
		
		env->totvert= 1;
	}
}

/* callback to remove envelope data point */
// TODO: should we have a separate file for things like this?
static void fmod_envelope_deletepoint_cb (bContext *C, void *fcm_dv, void *ind_v)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	int index= GET_INT_FROM_POINTER(ind_v);
	
	/* check that no data exists for the current frame... */
	if (env->totvert > 1) {
		/* allocate a new smaller array */
		fedn= MEM_callocN(sizeof(FCM_EnvelopeData)*(env->totvert-1), "FCM_EnvelopeData");
		
		memcpy(fedn, &env->data, sizeof(FCM_EnvelopeData)*(index));
		memcpy(&fedn[index], &env->data[index+1], sizeof(FCM_EnvelopeData)*(env->totvert-index-1));
		
		/* free old array, and set the new */
		MEM_freeN(env->data);
		env->data= fedn;
		env->totvert--;
	}
	else {
		/* just free array, since the only vert was deleted */
		if (env->data) 
			MEM_freeN(env->data);
		env->totvert= 0;
	}
}

/* draw settings for envelope modifier */
static void draw_modifier__envelope(uiBlock *block, FCurve *fcu, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed;
	uiBut *but;
	int cy= (*yco - 30);
	int i;
	
	/* set the height:
	 *	- basic settings + variable height from envelope controls
	 */
	(*height) = 96 + (25 * env->totvert);
	
	/* basic settings (backdrop + general settings + some padding) */
	//DRAW_BACKDROP((*height)); // XXX buggy...
	
	/* General Settings */
	uiDefBut(block, LABEL, 1, "Envelope:", 10, cy, 100, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling before first keyframe");
	cy -= 20;
	
	uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Reference Val:", 10, cy, 300, 20, &env->midval, -FLT_MAX, FLT_MAX, 10, 3, "");
		cy -= 20;
		
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Min:", 10, cy, 150, 20, &env->min, -FLT_MAX, env->max, 10, 3, "Minimum value (relative to Reference Value) that is used as the 'normal' minimum value");
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Max:", 160, cy, 150, 20, &env->max, env->min, FLT_MAX, 10, 3, "Maximum value (relative to Reference Value) that is used as the 'normal' maximum value");
		cy -= 35;
	uiBlockEndAlign(block);
	
	
	/* Points header */
	uiDefBut(block, LABEL, 1, "Control Points:", 10, cy, 150, 20, NULL, 0.0, 0.0, 0, 0, "");
	
	but= uiDefBut(block, BUT, B_FMODIFIER_REDRAW, "Add Point", 160,cy,150,19, NULL, 0, 0, 0, 0, "Adds a new control-point to the envelope on the current frame");
	uiButSetFunc(but, fmod_envelope_addpoint_cb, env, NULL);
	cy -= 35;
	
	/* Points List */
	for (i=0, fed=env->data; i < env->totvert; i++, fed++) {
		uiBlockBeginAlign(block);
			but=uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Fra:", 5, cy, 100, 20, &fed->time, -FLT_MAX, FLT_MAX, 10, 3, "Frame that envelope point occurs");
			uiButSetFunc(but, validate_fmodifier_cb, fcu, fcm);
			
			uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Min:", 105, cy, 100, 20, &fed->min, -FLT_MAX, FLT_MAX, 10, 3, "Minimum bound of envelope at this point");
			uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Max:", 205, cy, 100, 20, &fed->max, -FLT_MAX, FLT_MAX, 10, 3, "Maximum bound of envelope at this point");
			
			but= uiDefIconBut(block, BUT, B_FMODIFIER_REDRAW, ICON_X, 305, cy, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Delete envelope control point");
			uiButSetFunc(but, fmod_envelope_deletepoint_cb, env, SET_INT_IN_POINTER(i));
		uiBlockBeginAlign(block);
		cy -= 25;
	}
}

/* --------------- */

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
		uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_EXPANDED, B_REDR, ICON_TRIA_RIGHT,	5, *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is expanded");
		
		/* checkbox for 'active' status (for now) */
		but= uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_ACTIVE, B_REDR, ICON_RADIOBUT_OFF,	25, *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is active one");
		uiButSetFunc(but, activate_fmodifier_cb, fcu, fcm);
		
		/* name */
		if (fmi)
			but= uiDefBut(block, LABEL, 1, fmi->name,	10+40, *yco, 240, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type. Click to make modifier active one.");
		else
			but= uiDefBut(block, LABEL, 1, "<Unknown Modifier>",	10+40, *yco, 240, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type. Click to make modifier active one.");
		
		/* delete button */
		but= uiDefIconBut(block, BUT, B_REDR, ICON_X, 10+(width-30), *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete F-Curve Modifier.");
		uiButSetFunc(but, delete_fmodifier_cb, fcu, fcm);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* when modifier is expanded, draw settings */
	if (fcm->flag & FMODIFIER_FLAG_EXPANDED) {
		/* draw settings for individual modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR: /* Generator */
				draw_modifier__generator(block, fcu, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_CYCLES: /* Cycles */
				draw_modifier__cycles(block, fcu, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_ENVELOPE: /* Envelope */
				draw_modifier__envelope(block, fcu, fcm, yco, &height, width, active, rb_col);
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

/* ******************* general ******************************** */

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
		
	uiBeginPanels(C, ar);

	/* for now, the properties panel displays info about the selected channels */
	graph_panel_properties(C, ar, 0, ale);
	
	/* driver settings for active F-Curve (only for 'Drivers' mode) */
	if (sipo->mode == SIPO_MODE_DRIVERS)
		graph_panel_drivers(C, ar, 0, ale);
	
	/* modifiers */
	graph_panel_modifiers(C, ar, 0, ale);
	

	uiEndPanels(C, ar);
	
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
