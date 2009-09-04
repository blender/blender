/**
 * $Id$
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

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
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

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_types.h"

#include "action_intern.h"

/* ************************************************************************** */
/* KEYFRAME-RANGE STUFF */

/* *************************** Calculate Range ************************** */

/* Get the min/max keyframes*/
static void get_keyframe_extents (bAnimContext *ac, float *min, float *max)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get data to filter, from Action or Dopesheet */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* set large values to try to override */
	*min= 999999999.0f;
	*max= -999999999.0f;
	
	/* check if any channels to set range with */
	if (anim_data.first) {
		/* go through channels, finding max extents */
		for (ale= anim_data.first; ale; ale= ale->next) {
			AnimData *adt= ANIM_nla_mapping_get(ac, ale);
			FCurve *fcu= (FCurve *)ale->key_data;
			float tmin, tmax;
			
			/* get range and apply necessary scaling before */
			calc_fcurve_range(fcu, &tmin, &tmax);
			
			if (adt) {
				tmin= BKE_nla_tweakedit_remap(adt, tmin, NLATIME_CONVERT_MAP);
				tmax= BKE_nla_tweakedit_remap(adt, tmax, NLATIME_CONVERT_MAP);
			}
			
			/* try to set cur using these values, if they're more extreme than previously set values */
			*min= MIN2(*min, tmin);
			*max= MAX2(*max, tmax);
		}
		
		/* free memory */
		BLI_freelistN(&anim_data);
	}
	else {
		/* set default range */
		if (ac->scene) {
			*min= (float)ac->scene->r.sfra;
			*max= (float)ac->scene->r.efra;
		}
		else {
			*min= -5;
			*max= 100;
		}
	}
}

/* ****************** Automatic Preview-Range Operator ****************** */

static int actkeys_previewrange_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	Scene *scene;
	float min, max;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.scene == NULL)
		return OPERATOR_CANCELLED;
	else
		scene= ac.scene;
	
	/* set the range directly */
	get_keyframe_extents(&ac, &min, &max);
	scene->r.psfra= (int)floor(min + 0.5f);
	scene->r.pefra= (int)floor(max + 0.5f);
	
	/* set notifier that things have changed */
	// XXX err... there's nothing for frame ranges yet, but this should do fine too
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, ac.scene); 
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_previewrange_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Auto-Set Preview Range";
	ot->idname= "ACT_OT_previewrange_set";
	ot->description= "Set Preview Range based on extents of selected Keyframes.";
	
	/* api callbacks */
	ot->exec= actkeys_previewrange_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ****************** View-All Operator ****************** */

static int actkeys_viewall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	View2D *v2d;
	float extra;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	v2d= &ac.ar->v2d;
	
	/* set the horizontal range, with an extra offset so that the extreme keys will be in view */
	get_keyframe_extents(&ac, &v2d->cur.xmin, &v2d->cur.xmax);
	
	extra= 0.1f * (v2d->cur.xmax - v2d->cur.xmin);
	v2d->cur.xmin -= extra;
	v2d->cur.xmax += extra;
	
	/* set vertical range */
	v2d->cur.ymax= 0.0f;
	v2d->cur.ymin= (float)-(v2d->mask.ymax - v2d->mask.ymin);
	
	/* do View2D syncing */
	UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
	
	/* just redraw this view */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_view_all (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "View All";
	ot->idname= "ACT_OT_view_all";
	ot->description= "Reset viewable area to show full keyframe range.";
	
	/* api callbacks */
	ot->exec= actkeys_viewall_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************************************************** */
/* GENERAL STUFF */

/* ******************** Copy/Paste Keyframes Operator ************************* */
/* NOTE: the backend code for this is shared with the graph editor */

static short copy_action_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok=0;
	
	/* clear buffer first */
	free_anim_copybuf();
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* copy keyframes */
	ok= copy_animedit_keys(ac, &anim_data);
	
	/* clean up */
	BLI_freelistN(&anim_data);

	return ok;
}


static short paste_action_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok=0;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_SEL | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* paste keyframes */
	ok= paste_animedit_keys(ac, &anim_data);
	
	/* clean up */
	BLI_freelistN(&anim_data);

	return ok;
}

/* ------------------- */

static int actkeys_copy_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* copy keyframes */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		// FIXME...
	}
	else {
		if (copy_action_keys(&ac)) {	
			BKE_report(op->reports, RPT_ERROR, "No keyframes copied to keyframes copy/paste buffer");
			return OPERATOR_CANCELLED;
		}
	}
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_copy (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Copy Keyframes";
	ot->idname= "ACT_OT_copy";
	ot->description= "Copy selected keyframes to the copy/paste buffer.";
	
	/* api callbacks */
	ot->exec= actkeys_copy_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



static int actkeys_paste_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* paste keyframes */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		// FIXME...
	}
	else {
		if (paste_action_keys(&ac)) {
			BKE_report(op->reports, RPT_ERROR, "No keyframes to paste");
			return OPERATOR_CANCELLED;
		}
	}
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_paste (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Keyframes";
	ot->idname= "ACT_OT_paste";
	ot->description= "Paste keyframes from copy/paste buffer for the selected channels, starting on the current frame.";
	
	/* api callbacks */
	ot->exec= actkeys_paste_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Insert Keyframes Operator ************************* */

/* defines for insert keyframes tool */
EnumPropertyItem prop_actkeys_insertkey_types[] = {
	{1, "ALL", 0, "All Channels", ""},
	{2, "SEL", 0, "Only Selected Channels", ""},
	{3, "GROUP", 0, "In Active Group", ""}, // xxx not in all cases
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void insert_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene= ac->scene;
	float cfra= (float)CFRA;
	short flag = 0;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	if (mode == 2) 			filter |= ANIMFILTER_SEL;
	else if (mode == 3) 	filter |= ANIMFILTER_ACTGROUPED;
	
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init keyframing flag */
	if (IS_AUTOKEY_FLAG(AUTOMATKEY)) flag |= INSERTKEY_MATRIX;
	if (IS_AUTOKEY_FLAG(INSERTNEEDED)) flag |= INSERTKEY_NEEDED;
	if (IS_AUTOKEY_MODE(scene, EDITKEYS)) flag |= INSERTKEY_REPLACE;
	
	/* insert keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* adjust current frame for NLA-scaling */
		if (adt)
			cfra= BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else 
			cfra= (float)CFRA;
			
		/* if there's an id */
		if (ale->id)
			insert_keyframe(ale->id, NULL, ((fcu->grp)?(fcu->grp->name):(NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
		else
			insert_vert_fcurve(fcu, cfra, fcu->curval, 0);
	}
	
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_insertkey_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL)
		return OPERATOR_CANCELLED;
		
	/* what channels to affect? */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* insert keyframes */
	insert_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}

void ACT_OT_insert_keyframe (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Insert Keyframes";
	ot->idname= "ACT_OT_insert_keyframe";
	ot->description= "Insert keyframes for the specified channels.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_insertkey_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_actkeys_insertkey_types, 0, "Type", "");
}

/* ******************** Duplicate Keyframes Operator ************************* */

static void duplicate_action_keys (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale= anim_data.first; ale; ale= ale->next) {
		//if (ale->type == ANIMTYPE_GPLAYER)
		//	delete_gplayer_frames((bGPDlayer *)ale->data);
		//else
			duplicate_fcurve_keys((FCurve *)ale->key_data);
	}
	
	/* free filtered list */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_duplicate_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* duplicate keyframes */
	duplicate_action_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED; // xxx - start transform
}

static int actkeys_duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	actkeys_duplicate_exec(C, op);
	
	RNA_int_set(op->ptr, "mode", TFM_TIME_TRANSLATE);
	WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}
 
void ACT_OT_duplicate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Keyframes";
	ot->idname= "ACT_OT_duplicate";
	ot->description= "Make a copy of all selected keyframes.";
	
	/* api callbacks */
	ot->invoke= actkeys_duplicate_invoke;
	ot->exec= actkeys_duplicate_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_int(ot->srna, "mode", TFM_TIME_TRANSLATE, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* ******************** Delete Keyframes Operator ************************* */

static void delete_action_keys (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale= anim_data.first; ale; ale= ale->next) {
		//if (ale->type == ANIMTYPE_GPLAYER)
		//	delete_gplayer_frames((bGPDlayer *)ale->data);
		//else
			delete_fcurve_keys((FCurve *)ale->key_data); // XXX... this doesn't delete empty curves anymore
	}
	
	/* free filtered list */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_delete_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* delete keyframes */
	delete_action_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Keyframes";
	ot->idname= "ACT_OT_delete";
	ot->description= "Remove all selected keyframes.";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= actkeys_delete_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Clean Keyframes Operator ************************* */

static void clean_action_keys (bAnimContext *ac, float thresh)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_SEL | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and clean curves */
	for (ale= anim_data.first; ale; ale= ale->next)
		clean_fcurve((FCurve *)ale->key_data, thresh);
	
	/* free temp data */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_clean_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	float thresh;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL)
		return OPERATOR_PASS_THROUGH;
		
	/* get cleaning threshold */
	thresh= RNA_float_get(op->ptr, "threshold");
	
	/* clean keyframes */
	clean_action_keys(&ac, thresh);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_clean (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clean Keyframes";
	ot->idname= "ACT_OT_clean";
	ot->description= "Simplify F-Curves by removing closely spaced keyframes.";
	
	/* api callbacks */
	//ot->invoke=  // XXX we need that number popup for this! 
	ot->exec= actkeys_clean_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
}

/* ******************** Sample Keyframes Operator *********************** */

/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
static void sample_action_keys (bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale= anim_data.first; ale; ale= ale->next)
		sample_fcurve((FCurve *)ale->key_data);
	
	/* admin and redraws */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_sample_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL)
		return OPERATOR_PASS_THROUGH;
	
	/* sample keyframes */
	sample_action_keys(&ac);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_sample (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sample Keyframes";
	ot->idname= "ACT_OT_sample";
	ot->description= "Add keyframes on every frame between the selected keyframes.";
	
	/* api callbacks */
	ot->exec= actkeys_sample_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************************************************************************** */
/* SETTINGS STUFF */

/* ******************** Set Extrapolation-Type Operator *********************** */

/* defines for set extrapolation-type for selected keyframes tool */
EnumPropertyItem prop_actkeys_expo_types[] = {
	{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant Extrapolation", ""},
	{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear Extrapolation", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for setting extrapolation mode for keyframes */
static void setexpo_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting mode per F-Curve */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->data;
		fcu->extend= mode;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_expo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL) 
		return OPERATOR_PASS_THROUGH;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setexpo_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_extrapolation_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Extrapolation";
	ot->idname= "ACT_OT_extrapolation_type";
	ot->description= "Set extrapolation mode for selected F-Curves.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_expo_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_actkeys_expo_types, 0, "Type", "");
}

/* ******************** Set Interpolation-Type Operator *********************** */

/* this function is responsible for setting interpolation mode for keyframes */
static void setipo_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditFunc set_cb= ANIM_editkeyframes_ipo(mode);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply BeztEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale= anim_data.first; ale; ale= ale->next)
		ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_ipo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL) 
		return OPERATOR_PASS_THROUGH;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setipo_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_interpolation_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Interpolation";
	ot->idname= "ACT_OT_interpolation_type";
	ot->description= "Set interpolation mode for the F-Curve segments starting from the selected keyframes.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_ipo_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", beztriple_interpolation_mode_items, 0, "Type", "");
}

/* ******************** Set Handle-Type Operator *********************** */

/* this function is responsible for setting handle-type of selected keyframes */
static void sethandles_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditFunc set_cb= ANIM_editkeyframes_handles(mode);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting flags for handles 
	 * Note: we do not supply BeztEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (mode == -1) {	
			BeztEditFunc toggle_cb;
			
			/* check which type of handle to set (free or aligned) 
			 *	- check here checks for handles with free alignment already
			 */
			if (ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, NULL))
				toggle_cb= ANIM_editkeyframes_handles(HD_FREE);
			else
				toggle_cb= ANIM_editkeyframes_handles(HD_ALIGN);
				
			/* set handle-type */
			ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, toggle_cb, calchandles_fcurve);
		}
		else {
			/* directly set handle-type */
			ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);
		}
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_handletype_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL) 
		return OPERATOR_PASS_THROUGH;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	sethandles_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_handle_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Handle Type";
	ot->idname= "ACT_OT_handle_type";
	ot->description= "Set type of handle for selected keyframes.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_handletype_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", beztriple_handle_type_items, 0, "Type", "");
}

/* ******************** Set Keyframe-Type Operator *********************** */

/* this function is responsible for setting interpolation mode for keyframes */
static void setkeytype_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditFunc set_cb= ANIM_editkeyframes_keytype(mode);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply BeztEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale= anim_data.first; ale; ale= ale->next)
		ANIM_fcurve_keys_bezier_loop(NULL, ale->key_data, NULL, set_cb, NULL);
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_keytype_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.datatype == ANIMCONT_GPENCIL) 
		return OPERATOR_PASS_THROUGH;
		
	/* get handle setting mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setkeytype_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframe_type (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Keyframe Type";
	ot->idname= "ACT_OT_keyframe_type";
	ot->description= "Set type of keyframe for the seleced keyframes.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_keytype_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", beztriple_keyframe_type_items, 0, "Type", "");
}

/* ************************************************************************** */
/* TRANSFORM STUFF */

/* ***************** Jump to Selected Frames Operator *********************** */

/* snap current-frame indicator to 'average time' of selected keyframe */
static int actkeys_framejump_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* init edit data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* loop over action data, averaging values */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_calc_average, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_calc_average, NULL);
	}
	
	BLI_freelistN(&anim_data);
	
	/* set the new current frame value, based on the average time */
	if (bed.i1) {
		Scene *scene= ac.scene;
		CFRA= (int)floor((bed.f1 / bed.i1) + 0.5f);
	}
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}

void ACT_OT_frame_jump (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Jump to Frame";
	ot->idname= "ACT_OT_frame_jump";
	ot->description= "Set the current frame to the average frame of the selected keyframes.";
	
	/* api callbacks */
	ot->exec= actkeys_framejump_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************** Snap Keyframes Operator *********************** */

/* defines for snap keyframes tool */
EnumPropertyItem prop_actkeys_snap_types[] = {
	{ACTKEYS_SNAP_CFRA, "CFRA", 0, "Current frame", ""},
	{ACTKEYS_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Nearest Frame", ""}, // XXX as single entry?
	{ACTKEYS_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Nearest Second", ""}, // XXX as single entry?
	{ACTKEYS_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Nearest Marker", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_snap(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	if (mode == ACTKEYS_SNAP_NEAREST_MARKER) {
		bed.list.first= (ac->markers) ? ac->markers->first : NULL;
		bed.list.last= (ac->markers) ? ac->markers->last : NULL;
	}
	
	/* snap keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		//else if (ale->type == ACTTYPE_GPLAYER)
		//	snap_gplayer_frames(ale->data, mode);
		else 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_snap_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get snapping mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* snap keyframes */
	snap_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_snap (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Snap Keys";
	ot->idname= "ACT_OT_snap";
	ot->description= "Snap selected keyframes to the times specified.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_snap_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_actkeys_snap_types, 0, "Type", "");
}

/* ******************** Mirror Keyframes Operator *********************** */

/* defines for mirror keyframes tool */
EnumPropertyItem prop_actkeys_mirror_types[] = {
	{ACTKEYS_MIRROR_CFRA, "CFRA", 0, "By Times over Current frame", ""},
	{ACTKEYS_MIRROR_YAXIS, "YAXIS", 0, "By Times over Time=0", ""},
	{ACTKEYS_MIRROR_XAXIS, "XAXIS", 0, "By Values over Value=0", ""},
	{ACTKEYS_MIRROR_MARKER, "MARKER", 0, "By Times over First Selected Marker", ""},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for mirroring keyframes */
static void mirror_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_mirror(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	
	/* for 'first selected marker' mode, need to find first selected marker first! */
	// XXX should this be made into a helper func in the API?
	if (mode == ACTKEYS_MIRROR_MARKER) {
		TimeMarker *marker= NULL;
		
		/* find first selected marker */
		if (ac->markers) {
			for (marker= ac->markers->first; marker; marker=marker->next) {
				if (marker->flag & SELECT) {
					break;
				}
			}
		}
		
		/* store marker's time (if available) */
		if (marker)
			bed.f1= (float)marker->frame;
		else
			return;
	}
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* mirror keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		//else if (ale->type == ACTTYPE_GPLAYER)
		//	snap_gplayer_frames(ale->data, mode);
		else 
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_fcurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_mirror_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get mirroring mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* mirror keyframes */
	mirror_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_EDIT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_mirror (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mirror Keys";
	ot->idname= "ACT_OT_mirror";
	ot->description= "Flip selected keyframes over the selected mirror line.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_mirror_exec;
	ot->poll= ED_operator_action_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* id-props */
	RNA_def_enum(ot->srna, "type", prop_actkeys_mirror_types, 0, "Type", "");
}

/* ************************************************************************** */
