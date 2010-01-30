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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"
#include "BKE_sound.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_markers.h"
#include "ED_screen.h"

#include "anim_intern.h"

/* ********************** frame change operator ***************************/

/* Check if the operator can be run from the current context */
static int change_frame_poll(bContext *C)
{
	ScrArea *curarea= CTX_wm_area(C);
	
	/* as long as there is an active area, and it isn't a Graph Editor 
	 * (since the Graph Editor has its own version which does extra stuff),
	 * we're fine
	 */
	return ((curarea) && (curarea->spacetype != SPACE_IPO));
}

/* Set the new frame number */
static void change_frame_apply(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	
	/* set the new frame number */
	CFRA= RNA_int_get(op->ptr, "frame");
	
	/* do updates */
	sound_scrub(C);
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
}

/* ---- */

/* Non-modal callback for running operator without user input */
static int change_frame_exec(bContext *C, wmOperator *op)
{
	change_frame_apply(C, op);

	return OPERATOR_FINISHED;
}

/* ---- */

/* Get frame from mouse coordinates */
static int frame_from_event(bContext *C, wmEvent *event)
{
	ARegion *region= CTX_wm_region(C);
	float viewx;
	int x, y;
	
	/* convert screen coordinates to region coordinates */
	x= event->x - region->winrct.xmin;
	y= event->y - region->winrct.ymin;
	
	/* convert from region coordinates to View2D 'tot' space */
	UI_view2d_region_to_view(&region->v2d, x, y, &viewx, NULL);
	
	/* round result to nearest int (frames are ints!) */
	return (int)floor(viewx+0.5f);
}

/* Modal Operator init */
static int change_frame_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* Change to frame that mouse is over before adding modal handler,
	 * as user could click on a single frame (jump to frame) as well as
	 * click-dragging over a range (modal scrubbing).
	 */
	RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
	
	change_frame_apply(C, op);
	
	/* add temp handler */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* Modal event handling of frame changing */
static int change_frame_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	/* execute the events */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_FINISHED;
		
		case MOUSEMOVE:
			RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
			change_frame_apply(C, op);
			break;
		
		case LEFTMOUSE: 
		case RIGHTMOUSE:
			/* we check for either mouse-button to end, as checking for ACTIONMOUSE (which is used to init 
			 * the modal op) doesn't work for some reason
			 */
			if (event->val==KM_RELEASE)
				return OPERATOR_FINISHED;
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

void ANIM_OT_change_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change frame";
	ot->idname= "ANIM_OT_change_frame";
	ot->description= "Interactively change the current frame number.";
	
	/* api callbacks */
	ot->exec= change_frame_exec;
	ot->invoke= change_frame_invoke;
	ot->modal= change_frame_modal;
	ot->poll= change_frame_poll;
	
	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* rna */
	RNA_def_int(ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
}

/* ****************** set preview range operator ****************************/

static int previewrange_define_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	float sfra, efra;
	int xmin, xmax;
	
	/* get min/max values from border select rect (already in region coordinates, not screen) */
	xmin= RNA_int_get(op->ptr, "xmin");
	xmax= RNA_int_get(op->ptr, "xmax");
	
	/* convert min/max values to frames (i.e. region to 'tot' rect) */
	UI_view2d_region_to_view(&ar->v2d, xmin, 0, &sfra, NULL);
	UI_view2d_region_to_view(&ar->v2d, xmax, 0, &efra, NULL);
	
	/* set start/end frames for preview-range 
	 *	- must clamp within allowable limits
	 *	- end must not be before start (though this won't occur most of the time)
	 */
	if (sfra < 1) sfra = 1.0f;
	if (efra < 1) efra = 1.0f;
	if (efra < sfra) efra= sfra;
	
	scene->r.flag |= SCER_PRV_RANGE;
	scene->r.psfra= (int)floor(sfra + 0.5f);
	scene->r.pefra= (int)floor(efra + 0.5f);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
	
	return OPERATOR_FINISHED;
} 

void ANIM_OT_previewrange_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Preview Range";
	ot->idname= "ANIM_OT_previewrange_set";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= previewrange_define_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* rna */
		/* used to define frame range */
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
		/* these are not used, but are needed by borderselect gesture operator stuff */
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}

/* ****************** clear preview range operator ****************************/

static int previewrange_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *curarea= CTX_wm_area(C);
	
	/* sanity checks */
	if (ELEM(NULL, scene, curarea))
		return OPERATOR_CANCELLED;
	
	/* simply clear values */
	scene->r.flag &= ~SCER_PRV_RANGE;
	scene->r.psfra= 0;
	scene->r.pefra= 0;
	
	ED_area_tag_redraw(curarea);
	
	return OPERATOR_FINISHED;
} 

void ANIM_OT_previewrange_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Preview Range";
	ot->idname= "ANIM_OT_previewrange_clear";
	
	/* api callbacks */
	ot->exec= previewrange_clear_exec;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** time display toggle operator ****************************/

static int toggle_time_exec(bContext *C, wmOperator *op)
{
	ScrArea *curarea= CTX_wm_area(C);
	
	if (curarea == NULL)
		return OPERATOR_CANCELLED;
	
	/* simply toggle draw frames flag in applicable spaces */
	// XXX or should relevant spaces define their own version of this?
	switch (curarea->spacetype) {
		case SPACE_TIME: /* TimeLine */
		{
			SpaceTime *stime= CTX_wm_space_time(C);
			stime->flag ^= TIME_DRAWFRAMES;
		}
			break;
		case SPACE_ACTION: /* Action Editor */
		{
			SpaceAction *saction= CTX_wm_space_action(C);
			saction->flag ^= SACTION_DRAWTIME;
		}
			break;
		case SPACE_IPO: /* Graph Editor */
		{
			SpaceIpo *sipo= CTX_wm_space_graph(C);
			sipo->flag ^= SIPO_DRAWTIME;
		}
			break;
		case SPACE_NLA: /* NLA Editor */
		{
			SpaceNla *snla= CTX_wm_space_nla(C);
			snla->flag ^= SNLA_DRAWTIME;
		}
			break;
		case SPACE_SEQ: /* Sequencer */
		{
			SpaceSeq *sseq= CTX_wm_space_seq(C);
			sseq->flag ^= SEQ_DRAWFRAMES;
		}
			break;
			
		default: /* editor doesn't show frames */
			return OPERATOR_CANCELLED; // XXX or should we pass through instead?
	}
	
	ED_area_tag_redraw(curarea);
	
	return OPERATOR_FINISHED;
}

void ANIM_OT_time_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Frames/Seconds";
	ot->idname= "ANIM_OT_time_toggle";
	
	/* api callbacks */
	ot->exec= toggle_time_exec;
	
	ot->poll= ED_operator_areaactive;
}

/* ************************** registration **********************************/

void ED_operatortypes_anim(void)
{
	/* Animation Editors only -------------------------- */
	WM_operatortype_append(ANIM_OT_change_frame);
	WM_operatortype_append(ANIM_OT_time_toggle);
	
	WM_operatortype_append(ANIM_OT_previewrange_set);
	WM_operatortype_append(ANIM_OT_previewrange_clear);
	
	/* Entire UI --------------------------------------- */
	WM_operatortype_append(ANIM_OT_keyframe_insert);
	WM_operatortype_append(ANIM_OT_keyframe_delete);
	WM_operatortype_append(ANIM_OT_keyframe_insert_menu);
	WM_operatortype_append(ANIM_OT_keyframe_delete_v3d);
	WM_operatortype_append(ANIM_OT_keyframe_insert_button);
	WM_operatortype_append(ANIM_OT_keyframe_delete_button);
	
	
	WM_operatortype_append(ANIM_OT_driver_button_add);
	WM_operatortype_append(ANIM_OT_driver_button_remove);
	WM_operatortype_append(ANIM_OT_copy_driver_button);
	WM_operatortype_append(ANIM_OT_paste_driver_button);

	
	WM_operatortype_append(ANIM_OT_keyingset_button_add);
	WM_operatortype_append(ANIM_OT_keyingset_button_remove);
	
	WM_operatortype_append(ANIM_OT_keying_set_add);
	WM_operatortype_append(ANIM_OT_keying_set_remove);
	WM_operatortype_append(ANIM_OT_keying_set_path_add);
	WM_operatortype_append(ANIM_OT_keying_set_path_remove);
}

void ED_keymap_anim(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Animation", 0, 0);
	
	/* frame management */
		/* NOTE: 'ACTIONMOUSE' not 'LEFTMOUSE', as user may have swapped mouse-buttons */
	WM_keymap_add_item(keymap, "ANIM_OT_change_frame", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_time_toggle", TKEY, KM_PRESS, KM_CTRL, 0);
	
	/* preview range */
	WM_keymap_verify_item(keymap, "ANIM_OT_previewrange_set", PKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_previewrange_clear", PKEY, KM_PRESS, KM_ALT, 0);
}
