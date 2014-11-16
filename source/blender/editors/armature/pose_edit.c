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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Ton Roosendaal, Blender Foundation '05, full recode.
 *                 Joshua Leung
 *                 Reevan McKay (original NaN code)
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Pose Mode API's and Operators for Pose Mode armatures
 */

/** \file blender/editors/armature/pose_edit.c
 *  \ingroup edarmature
 */

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "UI_interface.h"

#include "armature_intern.h"

/* matches logic with ED_operator_posemode_context() */
Object *ED_pose_object_from_context(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	Object *ob;

	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa && sa->spacetype == SPACE_BUTS) {
		ob = ED_object_context(C);
	}
	else {
		ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	}

	return ob;
}

/* This function is used to process the necessary updates for */
void ED_armature_enter_posemode(bContext *C, Base *base)
{
	ReportList *reports = CTX_wm_reports(C);
	Object *ob = base->object;
	
	if (ob->id.lib) {
		BKE_report(reports, RPT_WARNING, "Cannot pose libdata");
		return;
	}
	
	switch (ob->type) {
		case OB_ARMATURE:
			ob->restore_mode = ob->mode;
			ob->mode |= OB_MODE_POSE;
			
			WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_POSE, NULL);
			
			break;
		default:
			return;
	}
	
	/* XXX: disabled as this would otherwise cause a nasty loop... */
	//ED_object_toggle_modes(C, ob->mode);
}

void ED_armature_exit_posemode(bContext *C, Base *base)
{
	if (base) {
		Object *ob = base->object;
		
		ob->restore_mode = ob->mode;
		ob->mode &= ~OB_MODE_POSE;
		
		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, NULL);
	}
}

/* if a selected or active bone is protected, throw error (oonly if warn == 1) and return 1 */
/* only_selected == 1: the active bone is allowed to be protected */
#if 0 /* UNUSED 2.5 */
static bool pose_has_protected_selected(Object *ob, short warn)
{
	/* check protection */
	if (ob->proxy) {
		bPoseChannel *pchan;
		bArmature *arm = ob->data;

		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if (pchan->bone && (pchan->bone->layer & arm->layer)) {
				if (pchan->bone->layer & arm->layer_protected) {
					if (pchan->bone->flag & BONE_SELECTED)
						break;
				}
			}
		}
		if (pchan) {
			if (warn) error("Cannot change Proxy protected bones");
			return 1;
		}
	}
	return 0;
}
#endif

/* ********************************************** */
/* Motion Paths */

/* For the object with pose/action: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates 
 */
void ED_pose_recalculate_paths(Scene *scene, Object *ob)
{
	ListBase targets = {NULL, NULL};
	
	/* set flag to force recalc, then grab the relevant bones to target */
	ob->pose->avs.recalc |= ANIMVIZ_RECALC_PATHS;
	animviz_get_object_motionpaths(ob, &targets);
	
	/* recalculate paths, then free */
	animviz_calc_motionpaths(scene, &targets);
	BLI_freelistN(&targets);
}


/* show popup to determine settings */
static int pose_calculate_paths_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{	
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* set default settings from existing/stored settings */
	{
		bAnimVizSettings *avs = &ob->pose->avs;
		PointerRNA avs_ptr;
		
		RNA_int_set(op->ptr, "start_frame", avs->path_sf);
		RNA_int_set(op->ptr, "end_frame", avs->path_ef);
		
		RNA_pointer_create(NULL, &RNA_AnimVizMotionPaths, avs, &avs_ptr);
		RNA_enum_set(op->ptr, "bake_location", RNA_enum_get(&avs_ptr, "bake_location"));
	}
	
	/* show popup dialog to allow editing of range... */
	// FIXME: hardcoded dimensions here are just arbitrary
	return WM_operator_props_dialog_popup(C, op, 10 * UI_UNIT_X, 10 * UI_UNIT_Y);
}

/* For the object with pose/action: create path curves for selected bones 
 * This recalculates the WHOLE path within the pchan->pathsf and pchan->pathef range
 */
static int pose_calculate_paths_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	Scene *scene = CTX_data_scene(C);
	
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* grab baking settings from operator settings */
	{
		bAnimVizSettings *avs = &ob->pose->avs;
		PointerRNA avs_ptr;
		
		avs->path_sf = RNA_int_get(op->ptr, "start_frame");
		avs->path_ef = RNA_int_get(op->ptr, "end_frame");
		
		RNA_pointer_create(NULL, &RNA_AnimVizMotionPaths, avs, &avs_ptr);
		RNA_enum_set(&avs_ptr, "bake_location", RNA_enum_get(op->ptr, "bake_location"));
	}
	
	/* set up path data for bones being calculated */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		/* verify makes sure that the selected bone has a bone with the appropriate settings */
		animviz_verify_motionpaths(op->reports, scene, ob, pchan);
	}
	CTX_DATA_END;

	/* calculate the bones that now have motionpaths... */
	/* TODO: only make for the selected bones? */
	ED_pose_recalculate_paths(scene, ob);

	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_calculate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Calculate Bone Paths";
	ot->idname = "POSE_OT_paths_calculate";
	ot->description = "Calculate paths for the selected bones";
	
	/* api callbacks */
	ot->invoke = pose_calculate_paths_invoke;
	ot->exec = pose_calculate_paths_exec;
	ot->poll = ED_operator_posemode_exclusive;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "start_frame", 1, MINAFRAME, MAXFRAME, "Start", 
	            "First frame to calculate bone paths on", MINFRAME, MAXFRAME / 2.0);
	RNA_def_int(ot->srna, "end_frame", 250, MINAFRAME, MAXFRAME, "End", 
	            "Last frame to calculate bone paths on", MINFRAME, MAXFRAME / 2.0);
	
	RNA_def_enum(ot->srna, "bake_location", motionpath_bake_location_items, 0, 
	             "Bake Location", 
	             "Which point on the bones is used when calculating paths");
}

/* --------- */

static int pose_update_paths_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	Scene *scene = CTX_data_scene(C);
	
	if (ELEM(NULL, ob, scene))
		return OPERATOR_CANCELLED;

	/* calculate the bones that now have motionpaths... */
	/* TODO: only make for the selected bones? */
	ED_pose_recalculate_paths(scene, ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_paths_update(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Update Bone Paths";
	ot->idname = "POSE_OT_paths_update";
	ot->description = "Recalculate paths for bones that already have them";
	
	/* api callbakcs */
	ot->exec = pose_update_paths_exec;
	ot->poll = ED_operator_posemode_exclusive; /* TODO: this should probably check for active bone and/or existing paths */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------- */

/* for the object with pose/action: clear path curves for selected bones only */
static void ED_pose_clear_paths(Object *ob)
{
	bPoseChannel *pchan;
	short skipped = 0;
	
	if (ELEM(NULL, ob, ob->pose))
		return;
	
	/* free the motionpath blocks, but also take note of whether we skipped some... */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (pchan->mpath) {
			if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
				animviz_free_motionpath(pchan->mpath);
				pchan->mpath = NULL;
			}
			else 
				skipped = 1;
		}
	}
	
	/* if we didn't skip any, we shouldn't have any paths left */
	if (skipped == 0)
		ob->pose->avs.path_bakeflag &= ~MOTIONPATH_BAKE_HAS_PATHS;
}

/* operator callback for this */
static int pose_clear_paths_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
		
	/* only continue if there's an object */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* use the backend function for this */
	ED_pose_clear_paths(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Bone Paths";
	ot->idname = "POSE_OT_paths_clear";
	ot->description = "Clear path caches for selected bones";
	
	/* api callbacks */
	ot->exec = pose_clear_paths_exec;
	ot->poll = ED_operator_posemode_exclusive;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
#if 0 /* UNUSED 2.5 */
static void pose_copy_menu(Scene *scene)
{
	Object *obedit = scene->obedit; // XXX context
	Object *ob = OBACT;
	bArmature *arm;
	bPoseChannel *pchan, *pchanact;
	short nr = 0;
	int i = 0;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) return;
	if ((ob == obedit) || (ob->mode & OB_MODE_POSE) == 0) return;
	
	pchan = BKE_pose_channel_active(ob);
	
	if (pchan == NULL) return;
	pchanact = pchan;
	arm = ob->data;

	/* if proxy-protected bones selected, some things (such as locks + displays) shouldn't be changeable,
	 * but for constraints (just add local constraints)
	 */
	if (pose_has_protected_selected(ob, 0)) {
		i = BLI_listbase_count(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr = pupmenu("Copy Pose Attributes %t|Local Location %x1|Local Rotation %x2|Local Size %x3|%l|Visual Location %x9|Visual Rotation %x10|Visual Size %x11|%l|Constraints (All) %x4|Constraints... %x5");
		else
			nr = pupmenu("Copy Pose Attributes %t|Local Location %x1|Local Rotation %x2|Local Size %x3|%l|Visual Location %x9|Visual Rotation %x10|Visual Size %x11|%l|Constraints (All) %x4");
	}
	else {
		i = BLI_listbase_count(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr = pupmenu("Copy Pose Attributes %t|Local Location %x1|Local Rotation %x2|Local Size %x3|%l|Visual Location %x9|Visual Rotation %x10|Visual Size %x11|%l|Constraints (All) %x4|Constraints... %x5|%l|Transform Locks %x6|IK Limits %x7|Bone Shape %x8");
		else
			nr = pupmenu("Copy Pose Attributes %t|Local Location %x1|Local Rotation %x2|Local Size %x3|%l|Visual Location %x9|Visual Rotation %x10|Visual Size %x11|%l|Constraints (All) %x4|%l|Transform Locks %x6|IK Limits %x7|Bone Shape %x8");
	}
	
	if (nr <= 0) 
		return;
	
	if (nr != 5) {
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if ((arm->layer & pchan->bone->layer) &&
			    (pchan->bone->flag & BONE_SELECTED) &&
			    (pchan != pchanact) )
			{
				switch (nr) {
					case 1: /* Local Location */
						copy_v3_v3(pchan->loc, pchanact->loc);
						break;
					case 2: /* Local Rotation */
						copy_qt_qt(pchan->quat, pchanact->quat);
						copy_v3_v3(pchan->eul, pchanact->eul);
						break;
					case 3: /* Local Size */
						copy_v3_v3(pchan->size, pchanact->size);
						break;
					case 4: /* All Constraints */
					{
						ListBase tmp_constraints = {NULL, NULL};
						
						/* copy constraints to tmpbase and apply 'local' tags before 
						 * appending to list of constraints for this channel
						 */
						BKE_constraints_copy(&tmp_constraints, &pchanact->constraints, true);
						if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {
							bConstraint *con;
							
							/* add proxy-local tags */
							for (con = tmp_constraints.first; con; con = con->next)
								con->flag |= CONSTRAINT_PROXY_LOCAL;
						}
						BLI_movelisttolist(&pchan->constraints, &tmp_constraints);
						
						/* update flags (need to add here, not just copy) */
						pchan->constflag |= pchanact->constflag;
						
						if (ob->pose)
							ob->pose->flag |= POSE_RECALC;
					}
					break;
					case 6: /* Transform Locks */
						pchan->protectflag = pchanact->protectflag;
						break;
					case 7: /* IK (DOF) settings */
					{
						pchan->ikflag = pchanact->ikflag;
						copy_v3_v3(pchan->limitmin, pchanact->limitmin);
						copy_v3_v3(pchan->limitmax, pchanact->limitmax);
						copy_v3_v3(pchan->stiffness, pchanact->stiffness);
						pchan->ikstretch = pchanact->ikstretch;
						pchan->ikrotweight = pchanact->ikrotweight;
						pchan->iklinweight = pchanact->iklinweight;
					}
					break;
					case 8: /* Custom Bone Shape */
						pchan->custom = pchanact->custom;
						if (pchan->custom) {
							id_us_plus(&pchan->custom->id);
						}
						break;
					case 9: /* Visual Location */
						BKE_armature_loc_pose_to_bone(pchan, pchanact->pose_mat[3], pchan->loc);
						break;
					case 10: /* Visual Rotation */
					{
						float delta_mat[4][4];
						
						BKE_armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						
						if (pchan->rotmode == ROT_MODE_AXISANGLE) {
							float tmp_quat[4];
							
							/* need to convert to quat first (in temp var)... */
							mat4_to_quat(tmp_quat, delta_mat);
							quat_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, tmp_quat);
						}
						else if (pchan->rotmode == ROT_MODE_QUAT)
							mat4_to_quat(pchan->quat, delta_mat);
						else
							mat4_to_eulO(pchan->eul, pchan->rotmode, delta_mat);
					}
					break;
					case 11: /* Visual Size */
					{
						float delta_mat[4][4], size[4];
						
						BKE_armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						mat4_to_size(size, delta_mat);
						copy_v3_v3(pchan->size, size);
					}
				}
			}
		}
	}
	else { /* constraints, optional (note: max we can have is 24 constraints) */
		bConstraint *con, *con_back;
		int const_toggle[24] = {0}; /* XXX, initialize as 0 to quiet errors */
		ListBase const_copy = {NULL, NULL};
		
		BLI_duplicatelist(&const_copy, &(pchanact->constraints));
		
		/* build the puplist of constraints */
		for (con = pchanact->constraints.first, i = 0; con; con = con->next, i++) {
			const_toggle[i] = 1;
//			add_numbut(i, UI_BTYPE_TOGGLE|INT, con->name, 0, 0, &(const_toggle[i]), "");
		}
		
//		if (!do_clever_numbuts("Select Constraints", i, REDRAW)) {
//			BLI_freelistN(&const_copy);
//			return;
//		}
		
		/* now build a new listbase from the options selected */
		for (i = 0, con = const_copy.first; con; i++) {
			/* if not selected, free/remove it from the list */
			if (!const_toggle[i]) {
				con_back = con->next;
				BLI_freelinkN(&const_copy, con);
				con = con_back;
			}
			else
				con = con->next;
		}
		
		/* Copy the temo listbase to the selected posebones */
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if ((arm->layer & pchan->bone->layer) &&
			    (pchan->bone->flag & BONE_SELECTED) &&
			    (pchan != pchanact) )
			{
				ListBase tmp_constraints = {NULL, NULL};
				
				/* copy constraints to tmpbase and apply 'local' tags before 
				 * appending to list of constraints for this channel
				 */
				BKE_constraints_copy(&tmp_constraints, &const_copy, true);
				if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {
					/* add proxy-local tags */
					for (con = tmp_constraints.first; con; con = con->next)
						con->flag |= CONSTRAINT_PROXY_LOCAL;
				}
				BLI_movelisttolist(&pchan->constraints, &tmp_constraints);
				
				/* update flags (need to add here, not just copy) */
				pchan->constflag |= pchanact->constflag;
			}
		}
		BLI_freelistN(&const_copy);
		BKE_pose_update_constraint_flags(ob->pose); /* we could work out the flags but its simpler to do this */
		
		if (ob->pose)
			ob->pose->flag |= POSE_RECALC;
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA); // and all its relations
	
	BIF_undo_push("Copy Pose Attributes");
	
}
#endif

/* ********************************************** */

static int pose_flip_names_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm = ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		char name_flip[MAXBONENAME];
		BKE_deform_flip_side_name(name_flip, pchan->name, true);
		ED_armature_bone_rename(arm, pchan->name, name_flip);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_flip_names(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Names";
	ot->idname = "POSE_OT_flip_names";
	ot->description = "Flips (and corrects) the axis suffixes of the names of selected bones";
	
	/* api callbacks */
	ot->exec = pose_flip_names_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ------------------ */

static int pose_autoside_names_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm;
	char newname[MAXBONENAME];
	short axis = RNA_enum_get(op->ptr, "axis");
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm = ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		BLI_strncpy(newname, pchan->name, sizeof(newname));
		if (bone_autoside_name(newname, 1, axis, pchan->bone->head[axis], pchan->bone->tail[axis]))
			ED_armature_bone_rename(arm, pchan->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_autoside_names(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{0, "XAXIS", 0, "X-Axis", "Left/Right"},
		{1, "YAXIS", 0, "Y-Axis", "Front/Back"},
		{2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "AutoName by Axis";
	ot->idname = "POSE_OT_autoside_names";
	ot->description = "Automatically renames the selected bones according to which side of the target axis they fall on";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = pose_autoside_names_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* settings */
	ot->prop = RNA_def_enum(ot->srna, "axis", axis_items, 0, "Axis", "Axis tag names with");
}

/* ********************************************** */

static int pose_bone_rotmode_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	int mode = RNA_enum_get(op->ptr, "type");
	
	/* set rotation mode of selected bones  */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		pchan->rotmode = mode;
	}
	CTX_DATA_END;
	
	/* notifiers and updates */
	DAG_id_tag_update((ID *)ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_rotation_mode_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Rotation Mode";
	ot->idname = "POSE_OT_rotation_mode_set";
	ot->description = "Set the rotation representation used by selected bones";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = pose_bone_rotmode_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", posebone_rotmode_items, 0, "Rotation Mode", "");
}

/* ********************************************** */

static int armature_layers_poll(bContext *C)
{
	/* Armature layers operators can be used in posemode OR editmode for armatures */
	return ED_operator_posemode(C) || ED_operator_editarmature(C);
}

static bArmature *armature_layers_get_data(Object **ob)
{
	bArmature *arm = NULL;

	/* Sanity checking and handling of posemode. */
	if (*ob) {
		Object *tob = BKE_object_pose_armature_get(*ob);
		if (tob) {
			*ob = tob;
			arm = (*ob)->data;
		}
		else if ((*ob)->type == OB_ARMATURE) {
			arm = (*ob)->data;
		}
	}

	return arm;
}

/* Show all armature layers */

static int pose_armature_layers_showall_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	bArmature *arm = armature_layers_get_data(&ob);
	PointerRNA ptr;
	int maxLayers = (RNA_boolean_get(op->ptr, "all")) ? 32 : 16;
	int layers[32] = {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	int i;
	
	/* sanity checking */
	if (arm == NULL)
		return OPERATOR_CANCELLED;
	
	/* use RNA to set the layers
	 *  although it would be faster to just set directly using bitflags, we still
	 *	need to setup a RNA pointer so that we get the "update" callbacks for free...
	 */
	RNA_id_pointer_create(&arm->id, &ptr);
	
	for (i = 0; i < maxLayers; i++)
		layers[i] = 1;
	
	RNA_boolean_set_array(&ptr, "layers", layers);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	/* done */
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_layers_show_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show All Layers";
	ot->idname = "ARMATURE_OT_layers_show_all";
	ot->description = "Make all armature layers visible";
	
	/* callbacks */
	ot->exec = pose_armature_layers_showall_exec;
	ot->poll = armature_layers_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_boolean(ot->srna, "all", 1, "All Layers", "Enable all layers or just the first 16 (top row)");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int armature_layers_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *ob = CTX_data_active_object(C);
	bArmature *arm = armature_layers_get_data(&ob);
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* sanity checking */
	if (arm == NULL)
		return OPERATOR_CANCELLED;
		
	/* get RNA pointer to armature data to use that to retrieve the layers as ints to init the operator */
	RNA_id_pointer_create((ID *)arm, &ptr);
	RNA_boolean_get_array(&ptr, "layers", layers);
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
	/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, event);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int armature_layers_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	bArmature *arm = armature_layers_get_data(&ob);
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */

	if (arm == NULL) {
		return OPERATOR_CANCELLED;
	}

	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);

	/* get pointer for armature, and write data there... */
	RNA_id_pointer_create((ID *)arm, &ptr);
	RNA_boolean_set_array(&ptr, "layers", layers);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_armature_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Armature Layers";
	ot->idname = "ARMATURE_OT_armature_layers";
	ot->description = "Change the visible armature layers";
	
	/* callbacks */
	ot->invoke = armature_layers_invoke;
	ot->exec = armature_layers_exec;
	ot->poll = armature_layers_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers to make visible");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int pose_bone_layers_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int layers[32] = {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get layers that are active already */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit = 0; bit < 32; bit++) {
			if (pchan->bone->layer & (1 << bit))
				layers[bit] = 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
	/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, event);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int pose_bone_layers_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */

	if (ob == NULL || ob->data == NULL) {
		return OPERATOR_CANCELLED;
	}

	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);

	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)ob->data, &RNA_Bone, pchan->bone, &ptr);
		RNA_boolean_set_array(&ptr, "layers", layers);
	}
	CTX_DATA_END;

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_bone_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Bone Layers";
	ot->idname = "POSE_OT_bone_layers";
	ot->description = "Change the layers that the selected bones belong to";
	
	/* callbacks */
	ot->invoke = pose_bone_layers_invoke;
	ot->exec = pose_bone_layers_exec;
	ot->poll = ED_operator_posemode_exclusive;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers that bone belongs to");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int armature_bone_layers_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int layers[32] = {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get layers that are active already */
	CTX_DATA_BEGIN (C, EditBone *, ebone, selected_editable_bones)
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit = 0; bit < 32; bit++) {
			if (ebone->layer & (1 << bit))
				layers[bit] = 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
	/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, event);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int armature_bone_layers_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (ob) ? ob->data : NULL;
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);
	
	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN (C, EditBone *, ebone, selected_editable_bones)
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)arm, &RNA_EditBone, ebone, &ptr);
		RNA_boolean_set_array(&ptr, "layers", layers);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Bone Layers";
	ot->idname = "ARMATURE_OT_bone_layers";
	ot->description = "Change the layers that the selected bones belong to";
	
	/* callbacks */
	ot->invoke = armature_bone_layers_invoke;
	ot->exec = armature_bone_layers_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers that bone belongs to");
}

/* ********************************************** */
/* Show/Hide Bones */

static int hide_selected_pose_bone_cb(Object *ob, Bone *bone, void *UNUSED(ptr)) 
{
	bArmature *arm = ob->data;
	
	if (arm->layer & bone->layer) {
		if (bone->flag & BONE_SELECTED) {
			bone->flag |= BONE_HIDDEN_P;
			bone->flag &= ~BONE_SELECTED;
			if (arm->act_bone == bone)
				arm->act_bone = NULL;
		}
	}
	return 0;
}

static int hide_unselected_pose_bone_cb(Object *ob, Bone *bone, void *UNUSED(ptr)) 
{
	bArmature *arm = ob->data;
	
	if (arm->layer & bone->layer) {
		/* hrm... typo here? */
		if ((bone->flag & BONE_SELECTED) == 0) {
			bone->flag |= BONE_HIDDEN_P;
			if (arm->act_bone == bone)
				arm->act_bone = NULL;
		}
	}
	return 0;
}

/* active object is armature in posemode, poll checked */
static int pose_hide_exec(bContext *C, wmOperator *op) 
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = ob->data;

	if (ob->proxy != NULL) {
		BKE_report(op->reports, RPT_INFO, "Undo of hiding can only be done with Reveal Selected");
	}

	if (RNA_boolean_get(op->ptr, "unselected"))
		bone_looper(ob, arm->bonebase.first, NULL, hide_unselected_pose_bone_cb);
	else
		bone_looper(ob, arm->bonebase.first, NULL, hide_selected_pose_bone_cb);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selected";
	ot->idname = "POSE_OT_hide";
	ot->description = "Tag selected bones to not be visible in Pose Mode";
	
	/* api callbacks */
	ot->exec = pose_hide_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "");
}

static int show_pose_bone_cb(Object *ob, Bone *bone, void *UNUSED(ptr)) 
{
	bArmature *arm = ob->data;
	
	if (arm->layer & bone->layer) {
		if (bone->flag & BONE_HIDDEN_P) {
			bone->flag &= ~BONE_HIDDEN_P;
			if (!(bone->flag & BONE_UNSELECTABLE)) {
				bone->flag |= BONE_SELECTED;
			}
		}
	}
	
	return 0;
}

/* active object is armature in posemode, poll checked */
static int pose_reveal_exec(bContext *C, wmOperator *UNUSED(op)) 
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = ob->data;
	
	bone_looper(ob, arm->bonebase.first, NULL, show_pose_bone_cb);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Selected";
	ot->idname = "POSE_OT_reveal";
	ot->description = "Unhide all bones that have been tagged to be hidden in Pose Mode";
	
	/* api callbacks */
	ot->exec = pose_reveal_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Flip Quats */

static int pose_flip_quats_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
	
	/* loop through all selected pchans, flipping and keying (as needed) */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, selected_pose_bones)
	{
		/* only if bone is using quaternion rotation */
		if (pchan->rotmode == ROT_MODE_QUAT) {
			/* quaternions have 720 degree range */
			negate_v4(pchan->quat);

			ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
		}
	}
	CTX_DATA_END;
	
	/* notifiers and updates */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_quaternions_flip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Quats";
	ot->idname = "POSE_OT_quaternions_flip";
	ot->description = "Flip quaternion values to achieve desired rotations, while maintaining the same orientations";
	
	/* callbacks */
	ot->exec = pose_flip_quats_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

