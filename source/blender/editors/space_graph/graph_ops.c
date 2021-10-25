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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_graph/graph_ops.c
 *  \ingroup spgraph
 */


#include <stdlib.h>
#include <math.h>

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "graph_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** view-based operators **********************************/
// XXX should these really be here?

/* Set Cursor --------------------------------------------------------------------- */
/* The 'cursor' in the Graph Editor consists of two parts:
 *	1) Current Frame Indicator (as per ANIM_OT_change_frame)
 *	2) Value Indicator (stored per Graph Editor instance)
 */

static int graphview_cursor_poll(bContext *C)
{
	/* prevent changes during render */
	if (G.is_rendering)
		return 0;

	return ED_operator_graphedit_active(C);
}

/* Set the new frame number */
static void graphview_cursor_apply(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceIpo *sipo = CTX_wm_space_graph(C);
	float frame = RNA_float_get(op->ptr, "frame"); /* this isn't technically "frame", but it'll do... */
	
	/* adjust the frame or the cursor x-value */
	if (sipo->mode == SIPO_MODE_DRIVERS) {
		/* adjust cursor x-value */
		sipo->cursorTime = frame;
	}
	else {
		/* adjust the frame 
		 * NOTE: sync this part of the code with ANIM_OT_change_frame
		 */
		/* 1) frame is rounded to the nearest int, since frames are ints */
		CFRA = iroundf(frame);
		
		if (scene->r.flag & SCER_LOCK_FRAME_SELECTION) {
			/* Clip to preview range
			 * NOTE: Preview range won't go into negative values,
			 *       so only clamping once should be fine.
			 */
			CLAMP(CFRA, PSFRA, PEFRA);
		}
		else {
			/* Prevent negative frames */
			FRAMENUMBER_MIN_CLAMP(CFRA);
		}
		
		SUBFRA = 0.0f;
		BKE_sound_seek_scene(bmain, scene);
	}
	
	/* set the cursor value */
	sipo->cursorVal = RNA_float_get(op->ptr, "value");
	
	/* send notifiers - notifiers for frame should force an update for both vars ok... */
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

/* ... */

/* Non-modal callback for running operator without user input */
static int graphview_cursor_exec(bContext *C, wmOperator *op)
{
	graphview_cursor_apply(C, op);
	return OPERATOR_FINISHED;
}

/* ... */

/* set the operator properties from the initial event */
static void graphview_cursor_setprops(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float viewx, viewy;
	
	/* abort if not active region (should not really be possible) */
	if (ar == NULL)
		return;
	
	/* convert from region coordinates to View2D 'tot' space */
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &viewx, &viewy);
	
	/* store the values in the operator properties */
	/* NOTE: we don't clamp frame here, as it might be used for the drivers cursor */
	RNA_float_set(op->ptr, "frame", viewx);
	RNA_float_set(op->ptr, "value", viewy);
}

/* Modal Operator init */
static int graphview_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bScreen *screen = CTX_wm_screen(C);
	
	/* Change to frame that mouse is over before adding modal handler,
	 * as user could click on a single frame (jump to frame) as well as
	 * click-dragging over a range (modal scrubbing). Apply this change.
	 */
	graphview_cursor_setprops(C, op, event);
	graphview_cursor_apply(C, op);
	
	/* Signal that a scrubbing operating is starting */
	if (screen)
		screen->scrubbing = true;
	
	/* add temp handler */
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

/* Modal event handling of cursor changing */
static int graphview_cursor_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	bScreen *screen = CTX_wm_screen(C);
	Scene *scene = CTX_data_scene(C);
	
	/* execute the events */
	switch (event->type) {
		case ESCKEY:
			if (screen)
				screen->scrubbing = false;
			
			WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
			return OPERATOR_FINISHED;
		
		case MOUSEMOVE:
			/* set the new values */
			graphview_cursor_setprops(C, op, event);
			graphview_cursor_apply(C, op);
			break;
		
		case LEFTMOUSE: 
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			/* we check for either mouse-button to end, as checking for ACTIONMOUSE (which is used to init 
			 * the modal op) doesn't work for some reason
			 */
			if (event->val == KM_RELEASE) {
				if (screen)
					screen->scrubbing = false;
				
				WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void GRAPH_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Cursor";
	ot->idname = "GRAPH_OT_cursor_set";
	ot->description = "Interactively set the current frame and value cursor";
	
	/* api callbacks */
	ot->exec = graphview_cursor_exec;
	ot->invoke = graphview_cursor_invoke;
	ot->modal = graphview_cursor_modal;
	ot->poll = graphview_cursor_poll;
	
	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

	/* rna */
	RNA_def_float(ot->srna, "frame", 0, MINAFRAMEF, MAXFRAMEF, "Frame", "", MINAFRAMEF, MAXFRAMEF);
	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Value", "", -100.0f, 100.0f);
}

/* Hide/Reveal ------------------------------------------------------------ */

static int graphview_curves_hide_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	ListBase all_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	const bool unselected = RNA_boolean_get(op->ptr, "unselected");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get list of all channels that selection may need to be flushed to 
	 * - hierarchy must not affect what we have access to here...
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);
	
	/* filter data
	 * - of the remaining visible curves, we want to hide the ones that are 
	 *   selected/unselected (depending on "unselected" prop) 
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
	if (unselected)
		filter |= ANIMFILTER_UNSEL;
	else
		filter |= ANIMFILTER_SEL;
	
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		/* hack: skip object channels for now, since flushing those will always flush everything, but they are always included */
		/* TODO: find out why this is the case, and fix that */
		if (ale->type == ANIMTYPE_OBJECT)
			continue;
		
		/* change the hide setting, and unselect it... */
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_CLEAR);
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_SELECT,  ACHANNEL_SETFLAG_CLEAR);
		
		/* now, also flush selection status up/down as appropriate */
		ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_CLEAR);
	}

	/* cleanup */
	ANIM_animdata_freelist(&anim_data);
	BLI_freelistN(&all_data);
	
	/* unhide selected */
	if (unselected) {
		/* turn off requirement for visible */
		filter = ANIMFILTER_SEL | ANIMFILTER_NODUPLIS | ANIMFILTER_LIST_CHANNELS;

		/* flushing has been done */
		ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

		for (ale = anim_data.first; ale; ale = ale->next) {
			/* hack: skip object channels for now, since flushing those will always flush everything, but they are always included */
			/* TODO: find out why this is the case, and fix that */
			if (ale->type == ANIMTYPE_OBJECT)
				continue;
			
			/* change the hide setting, and unselect it... */
			ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
			ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_SELECT,  ACHANNEL_SETFLAG_ADD);
			
			/* now, also flush selection status up/down as appropriate */
			ANIM_flush_setting_anim_channels(&ac, &anim_data, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
		}
		ANIM_animdata_freelist(&anim_data);	
	}
	
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void GRAPH_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Curves";
	ot->idname = "GRAPH_OT_hide";
	ot->description = "Hide selected curves from Graph Editor view";
	
	/* api callbacks */
	ot->exec = graphview_curves_hide_exec;
	ot->poll = ED_operator_graphedit_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected curves");
}

/* ........ */

static int graphview_curves_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	ListBase all_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get list of all channels that selection may need to be flushed to 
	 * - hierarchy must not affect what we have access to here...
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);
	
	/* filter data
	 * - just go through all visible channels, ensuring that everything is set to be curve-visible
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		/* hack: skip object channels for now, since flushing those will always flush everything, but they are always included */
		/* TODO: find out why this is the case, and fix that */
		if (ale->type == ANIMTYPE_OBJECT)
			continue;
		
		/* select if it is not visible */
		if (ANIM_channel_setting_get(&ac, ale, ACHANNEL_SETTING_VISIBLE) == 0)
			ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_SELECT, ACHANNEL_SETFLAG_ADD);
		
		/* change the visibility setting */
		ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
		
		/* now, also flush selection status up/down as appropriate */
		ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, true);
	}
	
	/* cleanup */
	ANIM_animdata_freelist(&anim_data);
	BLI_freelistN(&all_data);
	
	/* send notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}

static void GRAPH_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Curves";
	ot->idname = "GRAPH_OT_reveal";
	ot->description = "Make previously hidden curves visible again in Graph Editor view";
	
	/* api callbacks */
	ot->exec = graphview_curves_reveal_exec;
	ot->poll = ED_operator_graphedit_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************** registration - operator types **********************************/

void graphedit_operatortypes(void)
{
	/* view */
	WM_operatortype_append(GRAPH_OT_cursor_set);
	
	WM_operatortype_append(GRAPH_OT_previewrange_set);
	WM_operatortype_append(GRAPH_OT_view_all);
	WM_operatortype_append(GRAPH_OT_view_selected);
	WM_operatortype_append(GRAPH_OT_properties);
	WM_operatortype_append(GRAPH_OT_view_frame);
	
	WM_operatortype_append(GRAPH_OT_ghost_curves_create);
	WM_operatortype_append(GRAPH_OT_ghost_curves_clear);
	
	WM_operatortype_append(GRAPH_OT_hide);
	WM_operatortype_append(GRAPH_OT_reveal);
	
	/* keyframes */
	/* selection */
	WM_operatortype_append(GRAPH_OT_clickselect);
	WM_operatortype_append(GRAPH_OT_select_all_toggle);
	WM_operatortype_append(GRAPH_OT_select_border);
	WM_operatortype_append(GRAPH_OT_select_lasso);
	WM_operatortype_append(GRAPH_OT_select_circle);
	WM_operatortype_append(GRAPH_OT_select_column);
	WM_operatortype_append(GRAPH_OT_select_linked);
	WM_operatortype_append(GRAPH_OT_select_more);
	WM_operatortype_append(GRAPH_OT_select_less);
	WM_operatortype_append(GRAPH_OT_select_leftright);
	
	/* editing */
	WM_operatortype_append(GRAPH_OT_snap);
	WM_operatortype_append(GRAPH_OT_mirror);
	WM_operatortype_append(GRAPH_OT_frame_jump);
	WM_operatortype_append(GRAPH_OT_handle_type);
	WM_operatortype_append(GRAPH_OT_interpolation_type);
	WM_operatortype_append(GRAPH_OT_extrapolation_type);
	WM_operatortype_append(GRAPH_OT_easing_type);
	WM_operatortype_append(GRAPH_OT_sample);
	WM_operatortype_append(GRAPH_OT_bake);
	WM_operatortype_append(GRAPH_OT_sound_bake);
	WM_operatortype_append(GRAPH_OT_smooth);
	WM_operatortype_append(GRAPH_OT_clean);
	WM_operatortype_append(GRAPH_OT_euler_filter);
	WM_operatortype_append(GRAPH_OT_delete);
	WM_operatortype_append(GRAPH_OT_duplicate);
	
	WM_operatortype_append(GRAPH_OT_copy);
	WM_operatortype_append(GRAPH_OT_paste);
	
	WM_operatortype_append(GRAPH_OT_keyframe_insert);
	WM_operatortype_append(GRAPH_OT_click_insert);
	
	/* F-Curve Modifiers */
	WM_operatortype_append(GRAPH_OT_fmodifier_add);
	WM_operatortype_append(GRAPH_OT_fmodifier_copy);
	WM_operatortype_append(GRAPH_OT_fmodifier_paste);
	
	/* Drivers */
	WM_operatortype_append(GRAPH_OT_driver_variables_copy);
	WM_operatortype_append(GRAPH_OT_driver_variables_paste);
}

void ED_operatormacros_graph(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;
	
	ot = WM_operatortype_append_macro("GRAPH_OT_duplicate_move", "Duplicate",
	                                  "Make a copy of all selected keyframes and move them",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "GRAPH_OT_duplicate");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_transform");
	RNA_enum_set(otmacro->ptr, "mode", TFM_TIME_DUPLICATE);
	RNA_enum_set(otmacro->ptr, "proportional", PROP_EDIT_OFF);
}


/* ************************** registration - keymaps **********************************/

static void graphedit_keymap_keyframes(wmKeyConfig *keyconf, wmKeyMap *keymap)
{
	wmKeyMapItem *kmi;
	
	/* view */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", HKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_handles");

	/* NOTE: 'ACTIONMOUSE' not 'LEFTMOUSE', as user may have swapped mouse-buttons
	 * This keymap is supposed to override ANIM_OT_change_frame, which does the same except it doesn't do y-values
	 */
	WM_keymap_add_item(keymap, "GRAPH_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);
	
	
	/* graph_select.c - selection tools */
	/* click-select: keyframe (replace)  */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "curves", false);
	RNA_boolean_set(kmi->ptr, "column", false);
	/* click-select: all keyframes on same frame (replace) */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "curves", false);
	RNA_boolean_set(kmi->ptr, "column", true);
	/* click-select: keyframe (add) */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "curves", false);
	RNA_boolean_set(kmi->ptr, "column", false);
	/* click-select: all keyframes on same frame (add) */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "curves", false);
	RNA_boolean_set(kmi->ptr, "column", true);
	/* click-select: all keyframes in same curve (replace) */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "curves", true);
	RNA_boolean_set(kmi->ptr, "column", false);
	/* click-select: all keyframes in same curve (add) */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "curves", true);
	RNA_boolean_set(kmi->ptr, "column", false);
	
	/* click-select left/right */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", GRAPHKEYS_LRSEL_TEST);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_leftright", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_enum_set(kmi->ptr, "mode", GRAPHKEYS_LRSEL_TEST);
	
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_leftright", LEFTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", GRAPHKEYS_LRSEL_LEFT);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_leftright", RIGHTBRACKETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_enum_set(kmi->ptr, "mode", GRAPHKEYS_LRSEL_RIGHT);
	
	/* deselect all */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "invert", false);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_all_toggle", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "invert", true);
	
	/* borderselect */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", false);
	RNA_boolean_set(kmi->ptr, "include_handles", false);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_border", BKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", true);
	RNA_boolean_set(kmi->ptr, "include_handles", false);
	
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_border", BKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", false);
	RNA_boolean_set(kmi->ptr, "include_handles", true);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_border", BKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "axis_range", true);
	RNA_boolean_set(kmi->ptr, "include_handles", true);
	
	/* region select */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", true);
	
	WM_keymap_add_item(keymap, "GRAPH_OT_select_circle", CKEY, KM_PRESS, 0, 0);
	
	/* column select */
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPH_OT_select_column", KKEY, KM_PRESS, 0, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_KEYS);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPH_OT_select_column", KKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_CFRA);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPH_OT_select_column", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPH_OT_select_column", KKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN);
	
	/* select more/less */
	WM_keymap_add_item(keymap, "GRAPH_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	
	/* select linked */
	WM_keymap_add_item(keymap, "GRAPH_OT_select_linked", LKEY, KM_PRESS, 0, 0);
	
	
	/* graph_edit.c */
	/* jump to selected keyframes */
	WM_keymap_add_item(keymap, "GRAPH_OT_frame_jump", GKEY, KM_PRESS, KM_CTRL, 0);
		
	/* menu + single-step transform */
	WM_keymap_add_item(keymap, "GRAPH_OT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_mirror", MKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "GRAPH_OT_handle_type", VKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "GRAPH_OT_interpolation_type", TKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_easing_type", EKEY, KM_PRESS, KM_CTRL, 0);
	
	/* destructive */
	WM_keymap_add_item(keymap, "GRAPH_OT_smooth", OKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_sample", OKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "GRAPH_OT_bake", CKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_menu(keymap, "GRAPH_MT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "GRAPH_MT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "GRAPH_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* insertkey */
	WM_keymap_add_item(keymap, "GRAPH_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_click_insert", ACTIONMOUSE, KM_CLICK, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_click_insert", ACTIONMOUSE, KM_CLICK, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	
	/* copy/paste */
	WM_keymap_add_item(keymap, "GRAPH_OT_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_paste", VKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "flipped", true);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "GRAPH_OT_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_paste", VKEY, KM_PRESS, KM_OSKEY | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "flipped", true);
#endif

	/* auto-set range */
	WM_keymap_add_item(keymap, "GRAPH_OT_previewrange_set", PKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
#ifdef WITH_INPUT_NDOF
	WM_keymap_add_item(keymap, "GRAPH_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
#endif
	WM_keymap_add_item(keymap, "GRAPH_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_view_frame", PAD0, KM_PRESS, 0, 0);

	/* F-Modifiers */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_fmodifier_add", MKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "only_active", false);
	
	/* animation module */
	/* channels list
	 * NOTE: these operators were originally for the channels list, but are added here too for convenience...
	 */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_editable_toggle", TABKEY, KM_PRESS, 0, 0);
	
	/* transform system */
	transform_keymap_for_space(keyconf, keymap, SPACE_IPO);
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", OKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_proportional_fcurve");

	/* pivot point settings */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "BOUNDING_BOX_CENTER");
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "CURSOR");
	
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.pivot_point");
	RNA_string_set(kmi->ptr, "value", "INDIVIDUAL_ORIGINS");
		
	/* special markers hotkeys for anim editors: see note in definition of this function */
	ED_marker_keymap_animedit_conflictfree(keymap);
}

/* --------------- */

void graphedit_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* keymap for all regions */
	keymap = WM_keymap_find(keyconf, "Graph Editor Generic", SPACE_IPO, 0);
	WM_keymap_add_item(keymap, "GRAPH_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	/* extrapolation works on channels, not keys */
	WM_keymap_add_item(keymap, "GRAPH_OT_extrapolation_type", EKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* find (i.e. a shortcut for setting the name filter) */
	WM_keymap_add_item(keymap, "ANIM_OT_channels_find", FKEY, KM_PRESS, KM_CTRL, 0);
	
	/* hide/reveal selected curves */
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	
	kmi = WM_keymap_add_item(keymap, "GRAPH_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);
	
	WM_keymap_add_item(keymap, "GRAPH_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);
	
	
	/* channels */
	/* Channels are not directly handled by the Graph Editor module, but are inherited from the Animation module. 
	 * All the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as these
	 * are all used for the Graph Editor too.
	 */
	
	/* keyframes */
	keymap = WM_keymap_find(keyconf, "Graph Editor", SPACE_IPO, 0);
	graphedit_keymap_keyframes(keyconf, keymap);
}

