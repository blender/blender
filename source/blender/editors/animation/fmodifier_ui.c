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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/fmodifier_ui.c
 *  \ingroup edanimation
 */


/* User-Interface Stuff for F-Modifiers:
 * This file defines the (C-Coded) templates + editing callbacks needed 
 * by the interface stuff or F-Modifiers, as used by F-Curves in the Graph Editor,
 * and NLA-Strips in the NLA Editor.
 *
 * Copy/Paste Buffer for F-Modifiers:
 * For now, this is also defined in this file so that it can be shared between the 
 */
 
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_anim_api.h"
#include "ED_util.h"

/* ********************************************** */
/* UI STUFF */

// XXX! --------------------------------
/* temporary definition for limits of float number buttons (FLT_MAX tends to infinity with old system) */
#define UI_FLT_MAX  10000.0f

#define B_REDR                  1
#define B_FMODIFIER_REDRAW      20

/* callback to verify modifier data */
static void validate_fmodifier_cb(bContext *UNUSED(C), void *fcm_v, void *UNUSED(arg))
{
	FModifier *fcm = (FModifier *)fcm_v;
	const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);
	
	/* call the verify callback on the modifier if applicable */
	if (fmi && fmi->verify_data)
		fmi->verify_data(fcm);
}

/* callback to remove the given modifier  */
static void delete_fmodifier_cb(bContext *C, void *fmods_v, void *fcm_v)
{
	ListBase *modifiers = (ListBase *)fmods_v;
	FModifier *fcm = (FModifier *)fcm_v;
	
	/* remove the given F-Modifier from the active modifier-stack */
	remove_fmodifier(modifiers, fcm);

	ED_undo_push(C, "Delete F-Curve Modifier");
	
	/* send notifiers */
	// XXX for now, this is the only way to get updates in all the right places... but would be nice to have a special one in this case 
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

/* --------------- */
	
/* draw settings for generator modifier */
static void draw_modifier__generator(uiLayout *layout, ID *id, FModifier *fcm, short width)
{
	FMod_Generator *data = (FMod_Generator *)fcm->data;
	uiLayout /* *col, */ /* UNUSED */ *row;
	uiBlock *block;
	uiBut *but;
	PointerRNA ptr;
	short bwidth = width - 1.5 * UI_UNIT_X; /* max button width */
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierFunctionGenerator, fcm, &ptr);
	
	/* basic settings (backdrop + mode selector + some padding) */
	/* col = uiLayoutColumn(layout, true); */ /* UNUSED */
	block = uiLayoutGetBlock(layout);
	UI_block_align_begin(block);
	but = uiDefButR(block, UI_BTYPE_MENU, B_FMODIFIER_REDRAW, NULL, 0, 0, bwidth, UI_UNIT_Y, &ptr, "mode", -1, 0, 0, -1, -1, NULL);
	UI_but_func_set(but, validate_fmodifier_cb, fcm, NULL);
	
	uiDefButR(block, UI_BTYPE_TOGGLE, B_FMODIFIER_REDRAW, NULL, 0, 0, bwidth, UI_UNIT_Y, &ptr, "use_additive", -1, 0, 0, -1, -1, NULL);
	UI_block_align_end(block);
	
	/* now add settings for individual modes */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
		{
			const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
			float *cp = NULL;
			char xval[32];
			unsigned int i;
			int maxXWidth;
			
			/* draw polynomial order selector */
			row = uiLayoutRow(layout, false);
			block = uiLayoutGetBlock(row);
			but = uiDefButI(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, IFACE_("Poly Order:"), 0.5f * UI_UNIT_X, 0, bwidth, UI_UNIT_Y,
			                &data->poly_order, 1, 100, 0, 0,
			                TIP_("'Order' of the Polynomial (for a polynomial with n terms, 'order' is n-1)"));
			UI_but_func_set(but, validate_fmodifier_cb, fcm, NULL);
			
			
			/* calculate maximum width of label for "x^n" labels */
			if (data->arraysize > 2) {
				BLI_snprintf(xval, sizeof(xval), "x^%u", data->arraysize);
				/* XXX: UI_fontstyle_string_width is not accurate */
				maxXWidth = UI_fontstyle_string_width(fstyle, xval) + 0.5 * UI_UNIT_X;
			}
			else {
				/* basic size (just "x") */
				maxXWidth = UI_fontstyle_string_width(fstyle, "x") + 0.5 * UI_UNIT_X;
			}
			
			/* draw controls for each coefficient and a + sign at end of row */
			row = uiLayoutRow(layout, true);
			block = uiLayoutGetBlock(row);
			
			cp = data->coefficients;
			for (i = 0; (i < data->arraysize) && (cp); i++, cp++) {
				/* To align with first line... */
				if (i)
					uiDefBut(block, UI_BTYPE_LABEL, 1, "   ", 0, 0, 2 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				else
					uiDefBut(block, UI_BTYPE_LABEL, 1, "y =", 0, 0, 2 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				
				/* coefficient */
				uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, "", 0, 0, bwidth / 2, UI_UNIT_Y, cp, -UI_FLT_MAX, UI_FLT_MAX,
				          10, 3, TIP_("Coefficient for polynomial"));
				
				/* 'x' param (and '+' if necessary) */
				if (i == 0)
					BLI_strncpy(xval, "", sizeof(xval));
				else if (i == 1)
					BLI_strncpy(xval, "x", sizeof(xval));
				else
					BLI_snprintf(xval, sizeof(xval), "x^%u", i);
				uiDefBut(block, UI_BTYPE_LABEL, 1, xval, 0, 0, maxXWidth, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, TIP_("Power of x"));
				
				if ( (i != (data->arraysize - 1)) || ((i == 0) && data->arraysize == 2) ) {
					uiDefBut(block, UI_BTYPE_LABEL, 1, "+", 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
					
					/* next coefficient on a new row */
					row = uiLayoutRow(layout, true);
					block = uiLayoutGetBlock(row);
				}
				else {
					/* For alignment in UI! */
					uiDefBut(block, UI_BTYPE_LABEL, 1, " ", 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				}
			}
			break;
		}
		
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* Factorized polynomial expression */
		{
			float *cp = NULL;
			unsigned int i;
			
			/* draw polynomial order selector */
			row = uiLayoutRow(layout, false);
			block = uiLayoutGetBlock(row);
			but = uiDefButI(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, IFACE_("Poly Order:"), 0, 0, width - 1.5 * UI_UNIT_X, UI_UNIT_Y,
			                &data->poly_order, 1, 100, 0, 0,
			                TIP_("'Order' of the Polynomial (for a polynomial with n terms, 'order' is n-1)"));
			UI_but_func_set(but, validate_fmodifier_cb, fcm, NULL);
			
			
			/* draw controls for each pair of coefficients */
			row = uiLayoutRow(layout, true);
			block = uiLayoutGetBlock(row);
			
			cp = data->coefficients;
			for (i = 0; (i < data->poly_order) && (cp); i++, cp += 2) {
				/* To align with first line */
				if (i)
					uiDefBut(block, UI_BTYPE_LABEL, 1, "   ", 0, 0, 2.5 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				else
					uiDefBut(block, UI_BTYPE_LABEL, 1, "y =", 0, 0, 2.5 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				/* opening bracket */
				uiDefBut(block, UI_BTYPE_LABEL, 1, "(", 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				
				/* coefficients */
				uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, "", 0, 0, 5 * UI_UNIT_X, UI_UNIT_Y, cp, -UI_FLT_MAX, UI_FLT_MAX,
				          10, 3, TIP_("Coefficient of x"));
				
				uiDefBut(block, UI_BTYPE_LABEL, 1, "x +", 0, 0, 2 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
				
				uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, "", 0, 0, 5 * UI_UNIT_X, UI_UNIT_Y, cp + 1, -UI_FLT_MAX, UI_FLT_MAX,
				          10, 3, TIP_("Second coefficient"));
				
				/* closing bracket and multiplication sign */
				if ( (i != (data->poly_order - 1)) || ((i == 0) && data->poly_order == 2) ) {
					uiDefBut(block, UI_BTYPE_LABEL, 1, ") \xc3\x97", 0, 0, 2 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
					
					/* set up new row for the next pair of coefficients */
					row = uiLayoutRow(layout, true);
					block = uiLayoutGetBlock(row);
				}
				else 
					uiDefBut(block, UI_BTYPE_LABEL, 1, ")  ", 0, 0, 2 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
			}
			break;
		}
	}
}

/* --------------- */

/* draw settings for generator modifier */
static void draw_modifier__fn_generator(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	uiLayout *col;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierFunctionGenerator, fcm, &ptr);
	
	/* add the settings */
	col = uiLayoutColumn(layout, true);
	uiItemR(col, &ptr, "function_type", 0, "", ICON_NONE);
	uiItemR(col, &ptr, "use_additive", UI_ITEM_R_TOGGLE, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false); // no grouping for now
	uiItemR(col, &ptr, "amplitude", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "phase_multiplier", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "phase_offset", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "value_offset", 0, NULL, ICON_NONE);
}

/* --------------- */

/* draw settings for cycles modifier */
static void draw_modifier__cycles(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	uiLayout *split, *col;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierCycles, fcm, &ptr);
	
	/* split into 2 columns 
	 * NOTE: the mode comboboxes shouldn't get labels, otherwise there isn't enough room
	 */
	split = uiLayoutSplit(layout, 0.5f, false);
	
	/* before range */
	col = uiLayoutColumn(split, true);
	uiItemL(col, IFACE_("Before:"), ICON_NONE);
	uiItemR(col, &ptr, "mode_before", 0, "", ICON_NONE);
	uiItemR(col, &ptr, "cycles_before", 0, NULL, ICON_NONE);
		
	/* after range */
	col = uiLayoutColumn(split, true);
	uiItemL(col, IFACE_("After:"), ICON_NONE);
	uiItemR(col, &ptr, "mode_after", 0, "", ICON_NONE);
	uiItemR(col, &ptr, "cycles_after", 0, NULL, ICON_NONE);
}

/* --------------- */

/* draw settings for noise modifier */
static void draw_modifier__noise(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	uiLayout *split, *col;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierNoise, fcm, &ptr);
	
	/* blending mode */
	uiItemR(layout, &ptr, "blend_type", 0, NULL, ICON_NONE);
	
	/* split into 2 columns */
	split = uiLayoutSplit(layout, 0.5f, false);
	
	/* col 1 */
	col = uiLayoutColumn(split, false);
	uiItemR(col, &ptr, "scale", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "strength", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "offset", 0, NULL, ICON_NONE);
	
	/* col 2 */
	col = uiLayoutColumn(split, false);
	uiItemR(col, &ptr, "phase", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "depth", 0, NULL, ICON_NONE);
}

/* callback to add new envelope data point */
static void fmod_envelope_addpoint_cb(bContext *C, void *fcm_dv, void *UNUSED(arg))
{
	Scene *scene = CTX_data_scene(C);
	FMod_Envelope *env = (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	FCM_EnvelopeData fed;
	
	/* init template data */
	fed.min = -1.0f;
	fed.max = 1.0f;
	fed.time = (float)scene->r.cfra; // XXX make this int for ease of use?
	fed.f1 = fed.f2 = 0;
	
	/* check that no data exists for the current frame... */
	if (env->data) {
		bool exists;
		int i = BKE_fcm_envelope_find_index(env->data, (float)(scene->r.cfra), env->totvert, &exists);
		
		/* binarysearch_...() will set exists by default to 0, so if it is non-zero, that means that the point exists already */
		if (exists)
			return;
			
		/* add new */
		fedn = MEM_callocN((env->totvert + 1) * sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		
		/* add the points that should occur before the point to be pasted */
		if (i > 0)
			memcpy(fedn, env->data, i * sizeof(FCM_EnvelopeData));
		
		/* add point to paste at index i */
		*(fedn + i) = fed;
		
		/* add the points that occur after the point to be pasted */
		if (i < env->totvert) 
			memcpy(fedn + i + 1, env->data + i, (env->totvert - i) * sizeof(FCM_EnvelopeData));
		
		/* replace (+ free) old with new */
		MEM_freeN(env->data);
		env->data = fedn;
		
		env->totvert++;
	}
	else {
		env->data = MEM_callocN(sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		*(env->data) = fed;
		
		env->totvert = 1;
	}
}

/* callback to remove envelope data point */
// TODO: should we have a separate file for things like this?
static void fmod_envelope_deletepoint_cb(bContext *UNUSED(C), void *fcm_dv, void *ind_v)
{
	FMod_Envelope *env = (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	int index = GET_INT_FROM_POINTER(ind_v);
	
	/* check that no data exists for the current frame... */
	if (env->totvert > 1) {
		/* allocate a new smaller array */
		fedn = MEM_callocN(sizeof(FCM_EnvelopeData) * (env->totvert - 1), "FCM_EnvelopeData");

		memcpy(fedn, env->data, sizeof(FCM_EnvelopeData) * (index));
		memcpy(fedn + index, env->data + (index + 1), sizeof(FCM_EnvelopeData) * ((env->totvert - index) - 1));
		
		/* free old array, and set the new */
		MEM_freeN(env->data);
		env->data = fedn;
		env->totvert--;
	}
	else {
		/* just free array, since the only vert was deleted */
		if (env->data) {
			MEM_freeN(env->data);
			env->data = NULL;
		}
		env->totvert = 0;
	}
}

/* draw settings for envelope modifier */
static void draw_modifier__envelope(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	FMod_Envelope *env = (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed;
	uiLayout *col, *row;
	uiBlock *block;
	uiBut *but;
	PointerRNA ptr;
	int i;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierEnvelope, fcm, &ptr);
	
	/* general settings */
	col = uiLayoutColumn(layout, true);
	uiItemL(col, IFACE_("Envelope:"), ICON_NONE);
	uiItemR(col, &ptr, "reference_value", 0, NULL, ICON_NONE);

	row = uiLayoutRow(col, true);
	uiItemR(row, &ptr, "default_min", 0, IFACE_("Min"), ICON_NONE);
	uiItemR(row, &ptr, "default_max", 0, IFACE_("Max"), ICON_NONE);

	/* control points header */
	/* TODO: move this control-point control stuff to using the new special widgets for lists
	 * the current way is far too cramped */
	row = uiLayoutRow(layout, false);
	block = uiLayoutGetBlock(row);
		
	uiDefBut(block, UI_BTYPE_LABEL, 1, IFACE_("Control Points:"), 0, 0, 7.5 * UI_UNIT_X, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
		
	but = uiDefBut(block, UI_BTYPE_BUT, B_FMODIFIER_REDRAW, IFACE_("Add Point"), 0, 0, 7.5 * UI_UNIT_X, UI_UNIT_Y,
	               NULL, 0, 0, 0, 0, TIP_("Add a new control-point to the envelope on the current frame"));
	UI_but_func_set(but, fmod_envelope_addpoint_cb, env, NULL);
		
	/* control points list */
	for (i = 0, fed = env->data; i < env->totvert; i++, fed++) {
		/* get a new row to operate on */
		row = uiLayoutRow(layout, true);
		block = uiLayoutGetBlock(row);
		
		UI_block_align_begin(block);
		but = uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, IFACE_("Fra:"), 0, 0, 4.5 * UI_UNIT_X, UI_UNIT_Y,
		                &fed->time, -MAXFRAMEF, MAXFRAMEF, 10, 1, TIP_("Frame that envelope point occurs"));
		UI_but_func_set(but, validate_fmodifier_cb, fcm, NULL);
			
		uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, IFACE_("Min:"), 0, 0, 5 * UI_UNIT_X, UI_UNIT_Y,
		          &fed->min, -UI_FLT_MAX, UI_FLT_MAX, 10, 2, TIP_("Minimum bound of envelope at this point"));
		uiDefButF(block, UI_BTYPE_NUM, B_FMODIFIER_REDRAW, IFACE_("Max:"), 0, 0, 5 * UI_UNIT_X, UI_UNIT_Y,
		          &fed->max, -UI_FLT_MAX, UI_FLT_MAX, 10, 2, TIP_("Maximum bound of envelope at this point"));

		but = uiDefIconBut(block, UI_BTYPE_BUT, B_FMODIFIER_REDRAW, ICON_X, 0, 0, 0.9 * UI_UNIT_X, UI_UNIT_Y,
		                   NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Delete envelope control point"));
		UI_but_func_set(but, fmod_envelope_deletepoint_cb, env, SET_INT_IN_POINTER(i));
		UI_block_align_begin(block);
	}
}

/* --------------- */

/* draw settings for limits modifier */
static void draw_modifier__limits(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	uiLayout *split, *col /* , *row */ /* UNUSED */;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierLimits, fcm, &ptr);
	
	/* row 1: minimum */
	{
		/* row = uiLayoutRow(layout, false); */ /* UNUSED */
		
		/* split into 2 columns */
		split = uiLayoutSplit(layout, 0.5f, false);
		
		/* x-minimum */
		col = uiLayoutColumn(split, true);
		uiItemR(col, &ptr, "use_min_x", 0, NULL, ICON_NONE);
		uiItemR(col, &ptr, "min_x", 0, NULL, ICON_NONE);
			
		/* y-minimum*/
		col = uiLayoutColumn(split, true);
		uiItemR(col, &ptr, "use_min_y", 0, NULL, ICON_NONE);
		uiItemR(col, &ptr, "min_y", 0, NULL, ICON_NONE);
	}
	
	/* row 2: maximum */
	{
		/* row = uiLayoutRow(layout, false); */ /* UNUSED */
		
		/* split into 2 columns */
		split = uiLayoutSplit(layout, 0.5f, false);
		
		/* x-minimum */
		col = uiLayoutColumn(split, true);
		uiItemR(col, &ptr, "use_max_x", 0, NULL, ICON_NONE);
		uiItemR(col, &ptr, "max_x", 0, NULL, ICON_NONE);
			
		/* y-minimum*/
		col = uiLayoutColumn(split, true);
		uiItemR(col, &ptr, "use_max_y", 0, NULL, ICON_NONE);
		uiItemR(col, &ptr, "max_y", 0, NULL, ICON_NONE);
	}
}

/* --------------- */

/* draw settings for stepped interpolation modifier */
static void draw_modifier__stepped(uiLayout *layout, ID *id, FModifier *fcm, short UNUSED(width))
{
	uiLayout *col, *sub;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifierStepped, fcm, &ptr);
	
	/* block 1: "stepping" settings */
	col = uiLayoutColumn(layout, false);
	uiItemR(col, &ptr, "frame_step", 0, NULL, ICON_NONE);
	uiItemR(col, &ptr, "frame_offset", 0, NULL, ICON_NONE);
		
	/* block 2: start range settings */
	col = uiLayoutColumn(layout, true);
	uiItemR(col, &ptr, "use_frame_start", 0, NULL, ICON_NONE);
		
	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_frame_start"));
	uiItemR(sub, &ptr, "frame_start", 0, NULL, ICON_NONE);
			
	/* block 3: end range settings */
	col = uiLayoutColumn(layout, true);
	uiItemR(col, &ptr, "use_frame_end", 0, NULL, ICON_NONE);
		
	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_frame_end"));
	uiItemR(sub, &ptr, "frame_end", 0, NULL, ICON_NONE);
}

/* --------------- */

void ANIM_uiTemplate_fmodifier_draw(uiLayout *layout, ID *id, ListBase *modifiers, FModifier *fcm)
{
	const FModifierTypeInfo *fmi = fmodifier_get_typeinfo(fcm);
	uiLayout *box, *row, *sub, *col;
	uiBlock *block;
	uiBut *but;
	short width = 314;
	PointerRNA ptr;
	
	/* init the RNA-pointer */
	RNA_pointer_create(id, &RNA_FModifier, fcm, &ptr);
	
	/* draw header */
	{
		/* get layout-row + UI-block for this */
		box = uiLayoutBox(layout);
		
		row = uiLayoutRow(box, false);
		block = uiLayoutGetBlock(row); // err...
		
		/* left-align -------------------------------------------- */
		sub = uiLayoutRow(row, true);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_LEFT);
		
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		
		/* expand */
		uiItemR(sub, &ptr, "show_expanded", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		
		/* checkbox for 'active' status (for now) */
		uiItemR(sub, &ptr, "active", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		
		/* name */
		if (fmi)
			uiItemL(sub, IFACE_(fmi->name), ICON_NONE);
		else
			uiItemL(sub, IFACE_("<Unknown Modifier>"), ICON_NONE);
		
		/* right-align ------------------------------------------- */
		sub = uiLayoutRow(row, true);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);
		
		
		/* 'mute' button */
		uiItemR(sub, &ptr, "mute", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		
		UI_block_emboss_set(block, UI_EMBOSS_NONE);
		
		/* delete button */
		but = uiDefIconBut(block, UI_BTYPE_BUT, B_REDR, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y,
		                   NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Delete F-Curve Modifier"));
		UI_but_func_set(but, delete_fmodifier_cb, modifiers, fcm);
		
		UI_block_emboss_set(block, UI_EMBOSS);
	}
	
	/* when modifier is expanded, draw settings */
	if (fcm->flag & FMODIFIER_FLAG_EXPANDED) {
		/* set up the flexible-box layout which acts as the backdrop for the modifier settings */
		box = uiLayoutBox(layout);
		
		/* draw settings for individual modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR: /* Generator */
				draw_modifier__generator(box, id, fcm, width);
				break;
				
			case FMODIFIER_TYPE_FN_GENERATOR: /* Built-In Function Generator */
				draw_modifier__fn_generator(box, id, fcm, width);
				break;
				
			case FMODIFIER_TYPE_CYCLES: /* Cycles */
				draw_modifier__cycles(box, id, fcm, width);
				break;
				
			case FMODIFIER_TYPE_ENVELOPE: /* Envelope */
				draw_modifier__envelope(box, id, fcm, width);
				break;
				
			case FMODIFIER_TYPE_LIMITS: /* Limits */
				draw_modifier__limits(box, id, fcm, width);
				break;
			
			case FMODIFIER_TYPE_NOISE: /* Noise */
				draw_modifier__noise(box, id, fcm, width);
				break;
				
			case FMODIFIER_TYPE_STEPPED: /* Stepped */
				draw_modifier__stepped(box, id, fcm, width);
				break;
			
			default: /* unknown type */
				break;
		}
		
		/* one last panel below this: FModifier range */
		// TODO: experiment with placement of this
		{
			box = uiLayoutBox(layout);
			
			/* restricted range ----------------------------------------------------- */
			col = uiLayoutColumn(box, true);
			
			/* top row: use restricted range */
			row = uiLayoutRow(col, true);
			uiItemR(row, &ptr, "use_restricted_range", 0, NULL, ICON_NONE);
			
			if (fcm->flag & FMODIFIER_FLAG_RANGERESTRICT) {
				/* second row: settings */
				row = uiLayoutRow(col, true);
				
				uiItemR(row, &ptr, "frame_start", 0, IFACE_("Start"), ICON_NONE);
				uiItemR(row, &ptr, "frame_end", 0, IFACE_("End"), ICON_NONE);
				
				/* third row: blending influence */
				row = uiLayoutRow(col, true);
				
				uiItemR(row, &ptr, "blend_in", 0, IFACE_("In"), ICON_NONE);
				uiItemR(row, &ptr, "blend_out", 0, IFACE_("Out"), ICON_NONE);
			}
			
			/* influence -------------------------------------------------------------- */
			col = uiLayoutColumn(box, true);
			
			/* top row: use influence */
			uiItemR(col, &ptr, "use_influence", 0, NULL, ICON_NONE);
			
			if (fcm->flag & FMODIFIER_FLAG_USEINFLUENCE) {
				/* second row: influence value */
				uiItemR(col, &ptr, "influence", 0, NULL, ICON_NONE);
			}
		}
	}
}

/* ********************************************** */
/* COPY/PASTE BUFFER STUFF */

/* Copy/Paste Buffer itself (list of FModifier 's) */
static ListBase fmodifier_copypaste_buf = {NULL, NULL};

/* ---------- */

/* free the copy/paste buffer */
void ANIM_fmodifiers_copybuf_free(void)
{
	/* just free the whole buffer */
	free_fmodifiers(&fmodifier_copypaste_buf);
}

/* copy the given F-Modifiers to the buffer, returning whether anything was copied or not
 * assuming that the buffer has been cleared already with ANIM_fmodifiers_copybuf_free()
 *	- active: only copy the active modifier
 */
bool ANIM_fmodifiers_copy_to_buf(ListBase *modifiers, bool active)
{
	bool ok = true;
	
	/* sanity checks */
	if (ELEM(NULL, modifiers, modifiers->first))
		return 0;
		
	/* copy the whole list, or just the active one? */
	if (active) {
		FModifier *fcm = find_active_fmodifier(modifiers);
		
		if (fcm) {
			FModifier *fcmN = copy_fmodifier(fcm);
			BLI_addtail(&fmodifier_copypaste_buf, fcmN);
		}
		else
			ok = 0;
	}
	else
		copy_fmodifiers(&fmodifier_copypaste_buf, modifiers);
		
	/* did we succeed? */
	return ok;
}

/* 'Paste' the F-Modifier(s) from the buffer to the specified list 
 *	- replace: free all the existing modifiers to leave only the pasted ones 
 */
bool ANIM_fmodifiers_paste_from_buf(ListBase *modifiers, bool replace)
{
	FModifier *fcm;
	bool ok = false;
	
	/* sanity checks */
	if (modifiers == NULL)
		return 0;
		
	/* if replacing the list, free the existing modifiers */
	if (replace)
		free_fmodifiers(modifiers);
		
	/* now copy over all the modifiers in the buffer to the end of the list */
	for (fcm = fmodifier_copypaste_buf.first; fcm; fcm = fcm->next) {
		/* make a copy of it */
		FModifier *fcmN = copy_fmodifier(fcm);
		
		/* make sure the new one isn't active, otherwise the list may get several actives */
		fcmN->flag &= ~FMODIFIER_FLAG_ACTIVE;
		
		/* now add it to the end of the list */
		BLI_addtail(modifiers, fcmN);
		ok = 1;
	}
	
	/* did we succeed? */
	return ok;
}

/* ********************************************** */
