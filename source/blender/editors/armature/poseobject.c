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
 *				 Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 * support for animation modes - Reevan McKay
 */

/** \file blender/editors/armature/poseobject.c
 *  \ingroup edarmature
 */


#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_animsys.h"
#include "BKE_anim.h"
#include "BKE_idprop.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"


#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "armature_intern.h"

/* This function is used to process the necessary updates for */
void ED_armature_enter_posemode(bContext *C, Base *base)
{
	ReportList *reports= CTX_wm_reports(C);
	Object *ob= base->object;
	
	if (ob->id.lib) {
		BKE_report(reports, RPT_WARNING, "Can't pose libdata");
		return;
	}
	
	switch (ob->type) {
		case OB_ARMATURE:
			ob->restore_mode = ob->mode;
			ob->mode |= OB_MODE_POSE;
			
			WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_POSE, NULL);
			
			break;
		default:
			return;
	}
	
	// XXX: disabled as this would otherwise cause a nasty loop...
	//ED_object_toggle_modes(C, ob->mode);
}

void ED_armature_exit_posemode(bContext *C, Base *base)
{
	if (base) {
		Object *ob= base->object;
		
		ob->restore_mode = ob->mode;
		ob->mode &= ~OB_MODE_POSE;
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, NULL);
	}	
}

/* if a selected or active bone is protected, throw error (oonly if warn==1) and return 1 */
/* only_selected==1 : the active bone is allowed to be protected */
#if 0 /* UNUSED 2.5 */
static short pose_has_protected_selected(Object *ob, short warn)
{
	/* check protection */
	if (ob->proxy) {
		bPoseChannel *pchan;
		bArmature *arm= ob->data;

		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
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

/* only for real IK, not for auto-IK */
static int pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan, int level)
{
	bConstraint *con;
	Bone *bone;
	
	/* No need to check if constraint is active (has influence),
	 * since all constraints with CONSTRAINT_IK_AUTO are active */
	for (con= pchan->constraints.first; con; con= con->next) {
		if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			if (data->rootbone == 0 || data->rootbone > level) {
				if ((data->flag & CONSTRAINT_IK_AUTO)==0)
					return 1;
			}
		}
	}
	for (bone= pchan->bone->childbase.first; bone; bone= bone->next) {
		pchan= get_pose_channel(ob->pose, bone->name);
		if (pchan && pose_channel_in_IK_chain(ob, pchan, level + 1))
			return 1;
	}
	return 0;
}

int ED_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
	return pose_channel_in_IK_chain(ob, pchan, 0);
}

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

/* For the object with pose/action: create path curves for selected bones 
 * This recalculates the WHOLE path within the pchan->pathsf and pchan->pathef range
 */
static int pose_calculate_paths_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
		
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* set up path data for bones being calculated */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) 
	{
		/* verify makes sure that the selected bone has a bone with the appropriate settings */
		animviz_verify_motionpaths(op->reports, scene, ob, pchan);
	}
	CTX_DATA_END;
	
	/* calculate the bones that now have motionpaths... */
	// TODO: only make for the selected bones?
	ED_pose_recalculate_paths(scene, ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_calculate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Calculate Bone Paths";
	ot->idname = "POSE_OT_paths_calculate";
	ot->description = "Calculate paths for the selected bones";
	
	/* api callbacks */
	ot->exec = pose_calculate_paths_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
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
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->mpath) {
			if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
				animviz_free_motionpath(pchan->mpath);
				pchan->mpath= NULL;
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
static int pose_clear_paths_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
		
	/* only continue if there's an object */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* use the backend function for this */
	ED_pose_clear_paths(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_clear (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Bone Paths";
	ot->idname = "POSE_OT_paths_clear";
	ot->description = "Clear path caches for selected bones";
	
	/* api callbacks */
	ot->exec = pose_clear_paths_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************* Select Constraint Target Operator ************* */

static int pose_select_constraint_target_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bConstraint *con;
	int found= 0;
	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
	{
		if (pchan->bone->flag & BONE_SELECTED) {
			for (con= pchan->constraints.first; con; con= con->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						if ((ct->tar == ob) && (ct->subtarget[0])) {
							bPoseChannel *pchanc= get_pose_channel(ob->pose, ct->subtarget);
							if ((pchanc) && !(pchanc->bone->flag & BONE_UNSELECTABLE)) {
								pchanc->bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
								found= 1;
							}
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 1);
				}
			}
		}
	}
	CTX_DATA_END;

	if (!found)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_select_constraint_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Constraint Target";
	ot->idname = "POSE_OT_select_constraint_target";
	ot->description = "Select bones used as targets for the currently selected bones";
	
	/* api callbacks */
	ot->exec = pose_select_constraint_target_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************* select hierarchy operator ************* */

static int pose_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm= ob->data;
	Bone *curbone, *pabone, *chbone;
	int direction = RNA_enum_get(op->ptr, "direction");
	int add_to_sel = RNA_boolean_get(op->ptr, "extend");
	int found= 0;
	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
	{
		curbone= pchan->bone;
		
		if ((curbone->flag & BONE_UNSELECTABLE)==0) {
			if (curbone == arm->act_bone) {
				if (direction == BONE_SELECT_PARENT) {
					if (pchan->parent == NULL) continue;
					else pabone= pchan->parent->bone;
					
					if (PBONE_VISIBLE(arm, pabone)) {
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						pabone->flag |= BONE_SELECTED;
						arm->act_bone= pabone;
						
						found= 1;
						break;
					}
				} 
				else { /* direction == BONE_SELECT_CHILD */

					/* the child member is only assigned to connected bones, see [#30340] */
#if 0
					if (pchan->child == NULL) continue;
					else chbone = pchan->child->bone;
#else
					/* instead. find _any_ visible child bone, using the first one is a little arbitrary  - campbell */
					chbone = pchan->child ? pchan->child->bone : NULL;
					if (chbone == NULL) {
						bPoseChannel *pchan_child;

						for (pchan_child = ob->pose->chanbase.first; pchan_child; pchan_child = pchan_child->next) {
							/* possible we have multiple children, some invisible */
							if (PBONE_VISIBLE(arm, pchan_child->bone)) {
								if (pchan_child->parent == pchan) {
									chbone = pchan_child->bone;
									break;
								}
							}
						}
					}

					if (chbone == NULL) continue;
#endif
					
					if (PBONE_VISIBLE(arm, chbone)) {
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						chbone->flag |= BONE_SELECTED;
						arm->act_bone= chbone;
						
						found= 1;
						break;
					}
				}
			}
		}
	}
	CTX_DATA_END;

	if (found == 0)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_select_hierarchy(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[]= {
		{BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
		{BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Select Hierarchy";
	ot->idname = "POSE_OT_select_hierarchy";
	ot->description = "Select immediate parent/children of selected bones";
	
	/* api callbacks */
	ot->exec = pose_select_hierarchy_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_enum(ot->srna, "direction", direction_items, BONE_SELECT_PARENT, "Direction", "");
	RNA_def_boolean(ot->srna, "extend", 0, "Add to Selection", "");
	
}

/* ******************* select grouped operator ************* */

static short pose_select_same_group (bContext *C, Object *ob, short extend)
{
	bArmature *arm= (ob)? ob->data : NULL;
	bPose *pose= (ob)? ob->pose : NULL;
	char *group_flags;
	int numGroups = 0;
	short changed=0, tagged=0;
	
	/* sanity checks */
	if (ELEM3(NULL, ob, pose, arm))
		return 0;
		
	/* count the number of groups */
	numGroups= BLI_countlist(&pose->agroups);
	if (numGroups == 0)
		return 0;
		
	/* alloc a small array to keep track of the groups to use 
	 * 	- each cell stores on/off state for whether group should be used
	 *	- size is numGroups + 1, since index=0 is used for no-group
	 */
	group_flags= MEM_callocN(numGroups+1, "pose_select_same_group");
	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
	{
		/* keep track of group as group to use later? */
		if (pchan->bone->flag & BONE_SELECTED) {
			group_flags[pchan->agrp_index] = 1;
			tagged= 1;
		}
		
		/* deselect all bones before selecting new ones? */
		if ((extend == 0) && (pchan->bone->flag & BONE_UNSELECTABLE)==0)
			pchan->bone->flag &= ~BONE_SELECTED;
	}
	CTX_DATA_END;
	
	/* small optimization: only loop through bones a second time if there are any groups tagged */
	if (tagged) {
		/* only if group matches (and is not selected or current bone) */
		CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
		{
			if ((pchan->bone->flag & BONE_UNSELECTABLE)==0) {
				/* check if the group used by this bone is counted */
				if (group_flags[pchan->agrp_index]) {
					pchan->bone->flag |= BONE_SELECTED;
					changed= 1;
				}
			}
		}
		CTX_DATA_END;
	}
	
	/* free temp info */
	MEM_freeN(group_flags);
	
	return changed;
}

static short pose_select_same_layer (bContext *C, Object *ob, short extend)
{
	bPose *pose= (ob)? ob->pose : NULL;
	bArmature *arm= (ob)? ob->data : NULL;
	short changed= 0;
	int layers= 0;
	
	if (ELEM3(NULL, ob, pose, arm))
		return 0;
	
	/* figure out what bones are selected */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
	{
		/* keep track of layers to use later? */
		if (pchan->bone->flag & BONE_SELECTED)
			layers |= pchan->bone->layer;
			
		/* deselect all bones before selecting new ones? */
		if ((extend == 0) && (pchan->bone->flag & BONE_UNSELECTABLE)==0)
			pchan->bone->flag &= ~BONE_SELECTED;
	}
	CTX_DATA_END;
	if (layers == 0) 
		return 0;
		
	/* select bones that are on same layers as layers flag */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
	{
		/* if bone is on a suitable layer, and the bone can have its selection changed, select it */
		if ((layers & pchan->bone->layer) && (pchan->bone->flag & BONE_UNSELECTABLE)==0) {
			pchan->bone->flag |= BONE_SELECTED;
			changed= 1;
		}
	}
	CTX_DATA_END;
	
	return changed;
}

static int pose_select_same_keyingset(bContext *C, Object *ob, short extend)
{
	KeyingSet *ks = ANIM_scene_get_active_keyingset(CTX_data_scene(C));
	KS_Path *ksp;
	
	bArmature *arm = (ob)? ob->data : NULL;
	bPose *pose= (ob)? ob->pose : NULL;
	short changed= 0;
	
	/* sanity checks: validate Keying Set and object */
	if ((ks == NULL) || (ANIM_validate_keyingset(C, NULL, ks) != 0))
		return 0;
		
	if (ELEM3(NULL, ob, pose, arm))
		return 0;
		
	/* if not extending selection, deselect all selected first */
	if (extend == 0) {
		CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) 
		{
			if ((pchan->bone->flag & BONE_UNSELECTABLE)==0)
				pchan->bone->flag &= ~BONE_SELECTED;
		}
		CTX_DATA_END;
	}
		
	/* iterate over elements in the Keying Set, setting selection depending on whether 
	 * that bone is visible or not...
	 */
	for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
		/* only items related to this object will be relevant */
		if ((ksp->id == &ob->id) && (ksp->rna_path != NULL)) {
			if (strstr(ksp->rna_path, "bones")) {
				char *boneName = BLI_getQuotedStr(ksp->rna_path, "bones[");
				
				if (boneName) {
					bPoseChannel *pchan = get_pose_channel(pose, boneName);
					
					if (pchan) {
						/* select if bone is visible and can be affected */
						if ((PBONE_VISIBLE(arm, pchan->bone)) && 
							(pchan->bone->flag & BONE_UNSELECTABLE)==0)
						{
							pchan->bone->flag |= BONE_SELECTED;
							changed = 1;
						}
					}
					
					/* free temp memory */
					MEM_freeN(boneName);
				}
			}
		}
	}
	
	return changed;
}

static int pose_select_grouped_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	short extend= RNA_boolean_get(op->ptr, "extend");
	short changed = 0;
	
	/* sanity check */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
		
	/* selection types 
	 * NOTE: for the order of these, see the enum in POSE_OT_select_grouped()
	 */
	switch (RNA_enum_get(op->ptr, "type")) {
		case 1: /* group */
			changed= pose_select_same_group(C, ob, extend);
			break;
		case 2: /* Keying Set */
			changed= pose_select_same_keyingset(C, ob, extend);
			break;
		default: /* layer */
			changed= pose_select_same_layer(C, ob, extend);
			break;
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* report done status */
	if (changed)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_select_grouped (wmOperatorType *ot)
{
	static EnumPropertyItem prop_select_grouped_types[] = {
		{0, "LAYER", 0, "Layer", "Shared layers"},
		{1, "GROUP", 0, "Group", "Shared group"},
		{2, "KEYINGSET", 0, "Keying Set", "All bones affected by active Keying Set"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Select Grouped";
	ot->description = "Select all visible bones grouped by similar properties";
	ot->idname = "POSE_OT_select_grouped";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = pose_select_grouped_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}


/* ********************************************** */

/* context active object, or weightpainted object with armature in posemode */
static int pose_bone_flip_active_exec (bContext *C, wmOperator *UNUSED(op))
{
	Object *ob_act= CTX_data_active_object(C);
	Object *ob= object_pose_armature_get(ob_act);

	if (ob && (ob->mode & OB_MODE_POSE)) {
		bArmature *arm= ob->data;

		if (arm->act_bone) {
			bPoseChannel *pchanf;
			char name[MAXBONENAME];
			flip_side_name(name, arm->act_bone->name, TRUE);

			pchanf= get_pose_channel(ob->pose, name);
			if (pchanf && pchanf->bone != arm->act_bone) {
				arm->act_bone->flag &= ~BONE_SELECTED;
				pchanf->bone->flag |= BONE_SELECTED;

				arm->act_bone= pchanf->bone;

				/* in weightpaint we select the associated vertex group too */
				if (ob_act->mode & OB_MODE_WEIGHT_PAINT) {
					ED_vgroup_select_by_name(ob_act, name);
					DAG_id_tag_update(&ob_act->id, OB_RECALC_DATA);
				}

				WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

				return OPERATOR_FINISHED;
			}
		}
	}

	return OPERATOR_CANCELLED;
}

void POSE_OT_select_flip_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Selected Active Bone";
	ot->idname = "POSE_OT_select_flip_active";
	ot->description = "Activate the bone with a flipped name";
	
	/* api callbacks */
	ot->exec = pose_bone_flip_active_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* ********************************************** */
#if 0 /* UNUSED 2.5 */
static void pose_copy_menu(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX context
	Object *ob= OBACT;
	bArmature *arm;
	bPoseChannel *pchan, *pchanact;
	short nr=0;
	int i=0;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) return;
	if ((ob==obedit) || (ob->mode & OB_MODE_POSE)==0) return;
	
	pchan= get_active_posechannel(ob);
	
	if (pchan==NULL) return;
	pchanact= pchan;
	arm= ob->data;

	/* if proxy-protected bones selected, some things (such as locks + displays) shouldn't be changeable,
	 * but for constraints (just add local constraints)
	 */
	if (pose_has_protected_selected(ob, 0)) {
		i= BLI_countlist(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|Constraints...%x5");
		else
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4");
	}
	else {
		i= BLI_countlist(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|Constraints...%x5|%l|Transform Locks%x6|IK Limits%x7|Bone Shape%x8");
		else
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|%l|Transform Locks%x6|IK Limits%x7|Bone Shape%x8");
	}
	
	if (nr <= 0) 
		return;
	
	if (nr != 5) {
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ( (arm->layer & pchan->bone->layer) &&
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
						copy_constraints(&tmp_constraints, &pchanact->constraints, TRUE);
						if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {
							bConstraint *con;
							
							/* add proxy-local tags */
							for (con= tmp_constraints.first; con; con= con->next)
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
						pchan->ikstretch= pchanact->ikstretch;
						pchan->ikrotweight= pchanact->ikrotweight;
						pchan->iklinweight= pchanact->iklinweight;
					}
						break;
					case 8: /* Custom Bone Shape */
						pchan->custom = pchanact->custom;
						break;
					case 9: /* Visual Location */
						armature_loc_pose_to_bone(pchan, pchanact->pose_mat[3], pchan->loc);
						break;
					case 10: /* Visual Rotation */
					{
						float delta_mat[4][4];
						
						armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						
						if (pchan->rotmode == ROT_MODE_AXISANGLE) {
							float tmp_quat[4];
							
							/* need to convert to quat first (in temp var)... */
							mat4_to_quat( tmp_quat,delta_mat);
							quat_to_axis_angle( pchan->rotAxis, &pchan->rotAngle,tmp_quat);
						}
						else if (pchan->rotmode == ROT_MODE_QUAT)
							mat4_to_quat( pchan->quat,delta_mat);
						else
							mat4_to_eulO( pchan->eul, pchan->rotmode,delta_mat);
					}
						break;
					case 11: /* Visual Size */
					{
						float delta_mat[4][4], size[4];
						
						armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						mat4_to_size( size,delta_mat);
						copy_v3_v3(pchan->size, size);
					}
				}
			}
		}
	} 
	else { /* constraints, optional (note: max we can have is 24 constraints) */
		bConstraint *con, *con_back;
		int const_toggle[24]= {0}; /* XXX, initialize as 0 to quiet errors */
		ListBase const_copy = {NULL, NULL};
		
		BLI_duplicatelist(&const_copy, &(pchanact->constraints));
		
		/* build the puplist of constraints */
		for (con = pchanact->constraints.first, i=0; con; con=con->next, i++) {
			const_toggle[i]= 1;
//			add_numbut(i, TOG|INT, con->name, 0, 0, &(const_toggle[i]), "");
		}
		
//		if (!do_clever_numbuts("Select Constraints", i, REDRAW)) {
//			BLI_freelistN(&const_copy);
//			return;
//		}
		
		/* now build a new listbase from the options selected */
		for (i=0, con=const_copy.first; con; i++) {
			/* if not selected, free/remove it from the list */
			if (!const_toggle[i]) {
				con_back= con->next;
				BLI_freelinkN(&const_copy, con);
				con= con_back;
			} 
			else
				con= con->next;
		}
		
		/* Copy the temo listbase to the selected posebones */
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ( (arm->layer & pchan->bone->layer) &&
				 (pchan->bone->flag & BONE_SELECTED) &&
				 (pchan!=pchanact) ) 
			{
				ListBase tmp_constraints = {NULL, NULL};
				
				/* copy constraints to tmpbase and apply 'local' tags before 
				 * appending to list of constraints for this channel
				 */
				copy_constraints(&tmp_constraints, &const_copy, TRUE);
				if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {					
					/* add proxy-local tags */
					for (con= tmp_constraints.first; con; con= con->next)
						con->flag |= CONSTRAINT_PROXY_LOCAL;
				}
				BLI_movelisttolist(&pchan->constraints, &tmp_constraints);
				
				/* update flags (need to add here, not just copy) */
				pchan->constflag |= pchanact->constflag;
			}
		}
		BLI_freelistN(&const_copy);
		update_pose_constraint_flags(ob->pose); /* we could work out the flags but its simpler to do this */
		
		if (ob->pose)
			ob->pose->flag |= POSE_RECALC;
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);	// and all its relations
	
	BIF_undo_push("Copy Pose Attributes");
	
}
#endif

/* ******************** copy/paste pose ********************** */

/* Global copy/paste buffer for pose - cleared on start/end session + before every copy operation */
static bPose *g_posebuf = NULL;

void free_posebuf(void) 
{
	if (g_posebuf) {
		bPoseChannel *pchan;
		
		for (pchan= g_posebuf->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->prop) {
				IDP_FreeProperty(pchan->prop);
				MEM_freeN(pchan->prop);
			}
		}
		
		/* was copied without constraints */
		BLI_freelistN(&g_posebuf->chanbase);
		MEM_freeN(g_posebuf);
	}
	
	g_posebuf=NULL;
}

/* This function is used to indicate that a bone is selected 
 * and needs to be included in copy buffer (used to be for inserting keys)
 */
static void set_pose_keys (Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *chan;

	if (ob->pose) {
		for (chan=ob->pose->chanbase.first; chan; chan=chan->next) {
			Bone *bone= chan->bone;
			if ((bone) && (bone->flag & BONE_SELECTED) && (arm->layer & bone->layer))
				chan->flag |= POSE_KEY;	
			else
				chan->flag &= ~POSE_KEY;
		}
	}
}

/* perform paste pose, for a single bone 
 * < ob: object where bone to paste to lives
 * < chan: bone that pose to paste comes from
 * < selOnly: only paste on selected bones
 * < flip: flip on x-axis
 *
 * > returns: whether the bone that we pasted to if we succeeded
 */
static bPoseChannel *pose_bone_do_paste (Object *ob, bPoseChannel *chan, short selOnly, short flip)
{
	bPoseChannel *pchan;
	char name[MAXBONENAME];
	short paste_ok;
	
	/* get the name - if flipping, we must flip this first */
	if (flip)
		flip_side_name(name, chan->name, 0);		/* 0 = don't strip off number extensions */
	else
		BLI_strncpy(name, chan->name, sizeof(name));
	
	/* only copy when:
	 * 	1) channel exists - poses are not meant to add random channels to anymore
	 * 	2) if selection-masking is on, channel is selected - only selected bones get pasted on, allowing making both sides symmetrical
	 */
	pchan= get_pose_channel(ob->pose, name);
	
	if (selOnly)
		paste_ok= ((pchan) && (pchan->bone->flag & BONE_SELECTED));
	else
		paste_ok= ((pchan != NULL));
	
	/* continue? */
	if (paste_ok) {
		/* only loc rot size 
		 *	- only copies transform info for the pose 
		 */
		copy_v3_v3(pchan->loc, chan->loc);
		copy_v3_v3(pchan->size, chan->size);
		pchan->flag= chan->flag;
		
		/* check if rotation modes are compatible (i.e. do they need any conversions) */
		if (pchan->rotmode == chan->rotmode) {
			/* copy the type of rotation in use */
			if (pchan->rotmode > 0) {
				copy_v3_v3(pchan->eul, chan->eul);
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				copy_v3_v3(pchan->rotAxis, chan->rotAxis);
				pchan->rotAngle = chan->rotAngle;
			}
			else {
				copy_qt_qt(pchan->quat, chan->quat);
			}
		}
		else if (pchan->rotmode > 0) {
			/* quat/axis-angle to euler */
			if (chan->rotmode == ROT_MODE_AXISANGLE)
				axis_angle_to_eulO( pchan->eul, pchan->rotmode,chan->rotAxis, chan->rotAngle);
			else
				quat_to_eulO( pchan->eul, pchan->rotmode,chan->quat);
		}
		else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
			/* quat/euler to axis angle */
			if (chan->rotmode > 0)
				eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->eul, chan->rotmode);
			else	
				quat_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->quat);
		}
		else {
			/* euler/axis-angle to quat */
			if (chan->rotmode > 0)
				eulO_to_quat(pchan->quat, chan->eul, chan->rotmode);
			else
				axis_angle_to_quat(pchan->quat, chan->rotAxis, pchan->rotAngle);
		}
		
		/* paste flipped pose? */
		if (flip) {
			pchan->loc[0]*= -1;
			
			/* has to be done as eulers... */
			if (pchan->rotmode > 0) {
				pchan->eul[1] *= -1;
				pchan->eul[2] *= -1;
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				float eul[3];
				
				axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, pchan->rotAxis, pchan->rotAngle);
				eul[1]*= -1;
				eul[2]*= -1;
				eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, eul, EULER_ORDER_DEFAULT);
			}
			else {
				float eul[3];
				
				normalize_qt(pchan->quat);
				quat_to_eul(eul, pchan->quat);
				eul[1]*= -1;
				eul[2]*= -1;
				eul_to_quat(pchan->quat, eul);
			}
		}
		
		/* ID properties */
		if (chan->prop) {
			if (pchan->prop) {
				/* if we have existing properties on a bone, just copy over the values of 
				 * matching properties (i.e. ones which will have some impact) on to the 
				 * target instead of just blinding replacing all [
				 */
				IDP_SyncGroupValues(pchan->prop, chan->prop);
			}
			else {
				/* no existing properties, so assume that we want copies too? */
				pchan->prop= IDP_CopyProperty(chan->prop);	
			}
		}
	}
	
	/* return whether paste went ahead */
	return pchan;
}

/* ---- */

static int pose_copy_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* sanity checking */
	if (ELEM(NULL, ob, ob->pose)) {
		BKE_report(op->reports, RPT_ERROR, "No Pose to Copy");
		return OPERATOR_CANCELLED;
	}

	/* free existing pose buffer */
	free_posebuf();
	
	/* sets chan->flag to POSE_KEY if bone selected, then copy those bones to the buffer */
	set_pose_keys(ob);  
	copy_pose(&g_posebuf, ob->pose, 0);
	
	
	return OPERATOR_FINISHED;
}

void POSE_OT_copy (wmOperatorType *ot) 
{
	/* identifiers */
	ot->name = "Copy Pose";
	ot->idname = "POSE_OT_copy";
	ot->description = "Copies the current pose of the selected bones to copy/paste buffer";
	
	/* api callbacks */
	ot->exec = pose_copy_exec;
	ot->poll = ED_operator_posemode;
	
	/* flag */
	ot->flag = OPTYPE_REGISTER;
}

/* ---- */

static int pose_paste_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	Scene *scene= CTX_data_scene(C);
	bPoseChannel *chan;
	int flip= RNA_boolean_get(op->ptr, "flipped");
	int selOnly= RNA_boolean_get(op->ptr, "selected_mask");

	/* get KeyingSet to use */
	KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOC_ROT_SCALE_ID);

	/* sanity checks */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;

	if (g_posebuf == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Copy buffer is empty");
		return OPERATOR_CANCELLED;
	}
	
	/* if selOnly option is enabled, if user hasn't selected any bones, 
	 * just go back to default behavior to be more in line with other pose tools
	 */
	if (selOnly) {
		if (CTX_DATA_COUNT(C, selected_pose_bones) == 0)
			selOnly = 0;
	}

	/* Safely merge all of the channels in the buffer pose into any existing pose */
	for (chan= g_posebuf->chanbase.first; chan; chan=chan->next) {
		if (chan->flag & POSE_KEY) {
			/* try to perform paste on this bone */
			bPoseChannel *pchan = pose_bone_do_paste(ob, chan, selOnly, flip);
			
			if (pchan) {
				/* keyframing tagging for successful paste */
				ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
			}
		}
	}
	
	/* Update event for pose and deformation children */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_paste (wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Paste Pose";
	ot->idname = "POSE_OT_paste";
	ot->description = "Paste the stored pose on to the current pose";
	
	/* api callbacks */
	ot->exec = pose_paste_exec;
	ot->poll = ED_operator_posemode;
	
	/* flag */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "flipped", FALSE, "Flipped on X-Axis", "Paste the stored pose flipped on to current pose");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_boolean(ot->srna, "selected_mask", FALSE, "On Selected Only", "Only paste the stored pose on to selected bones in the current pose");
}

/* ********************************************** */
/* Bone Groups */

static int pose_group_add_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
		
	/* only continue if there's an object */
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* for now, just call the API function for this */
	pose_add_group(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bone Group";
	ot->idname = "POSE_OT_group_add";
	ot->description = "Add a new bone group";
	
	/* api callbacks */
	ot->exec = pose_group_add_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int pose_group_remove_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object */
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* for now, just call the API function for this */
	pose_remove_group(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Bone Group";
	ot->idname = "POSE_OT_group_remove";
	ot->description = "Removes the active bone group";
	
	/* api callbacks */
	ot->exec = pose_group_remove_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------ */

/* invoke callback which presents a list of bone-groups for the user to choose from */
static int pose_groups_menu_invoke (bContext *C, wmOperator *op, wmEvent *UNUSED(evt))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	bPose *pose;
	
	uiPopupMenu *pup;
	uiLayout *layout;
	bActionGroup *grp;
	int i;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	pose= ob->pose;
	
	/* if there's no active group (or active is invalid), create a new menu to find it */
	if (pose->active_group <= 0) {
		/* create a new menu, and start populating it with group names */
		pup= uiPupMenuBegin(C, op->type->name, ICON_NONE);
		layout= uiPupMenuLayout(pup);
		
		/* special entry - allow to create new group, then use that 
		 *	(not to be used for removing though)
		 */
		if (strstr(op->idname, "assign")) {
			uiItemIntO(layout, "New Group", ICON_NONE, op->idname, "type", 0);
			uiItemS(layout);
		}
		
		/* add entries for each group */
		for (grp= pose->agroups.first, i=1; grp; grp=grp->next, i++)
			uiItemIntO(layout, grp->name, ICON_NONE, op->idname, "type", i);
			
		/* finish building the menu, and process it (should result in calling self again) */
		uiPupMenuEnd(C, pup);
		
		return OPERATOR_CANCELLED;
	}
	else {
		/* just use the active group index, and call the exec callback for the calling operator */
		RNA_int_set(op->ptr, "type", pose->active_group);
		return op->type->exec(C, op);
	}
}

/* Assign selected pchans to the bone group that the user selects */
static int pose_group_assign_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	bPose *pose;
	short done= 0;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;

	pose= ob->pose;
	
	/* set the active group number to the one from operator props 
	 * 	- if 0 after this, make a new group...
	 */
	pose->active_group= RNA_int_get(op->ptr, "type");
	if (pose->active_group == 0)
		pose_add_group(ob);
	
	/* add selected bones to group then */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
	{
		pchan->agrp_index= pose->active_group;
		done= 1;
	}
	CTX_DATA_END;

	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* report done status */
	if (done)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_group_assign (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Selected to Bone Group";
	ot->idname = "POSE_OT_group_assign";
	ot->description = "Add selected bones to the chosen bone group";
	
	/* api callbacks */
	ot->invoke = pose_groups_menu_invoke;
	ot->exec = pose_group_assign_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "type", 0, 0, 10, "Bone Group Index", "", 0, INT_MAX);
}


static int pose_group_unassign_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	short done= 0;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	/* find selected bones to remove from all bone groups */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
	{
		if (pchan->agrp_index) {
			pchan->agrp_index= 0;
			done= 1;
		}
	}
	CTX_DATA_END;
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* report done status */
	if (done)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_group_unassign (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Selected from Bone Groups";
	ot->idname = "POSE_OT_group_unassign";
	ot->description = "Remove selected bones from all bone groups";
	
	/* api callbacks */
	ot->exec = pose_group_unassign_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int group_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	bActionGroup *grp;
	int dir= RNA_enum_get(op->ptr, "direction");
	int grpIndexA, grpIndexB;

	if (ELEM(NULL, ob, pose))
		return OPERATOR_CANCELLED;
	if (pose->active_group <= 0)
		return OPERATOR_CANCELLED;

	/* get group to move */
	grp= BLI_findlink(&pose->agroups, pose->active_group-1);
	if (grp == NULL)
		return OPERATOR_CANCELLED;

	/* move bone group */
	grpIndexA = pose->active_group;
	if (dir == 1) { /* up */
		void *prev = grp->prev;
		
		if (prev == NULL)
			return OPERATOR_FINISHED;
			
		BLI_remlink(&pose->agroups, grp);
		BLI_insertlinkbefore(&pose->agroups, prev, grp);
		
		grpIndexB = grpIndexA - 1;
		pose->active_group--;
	}
	else { /* down */
		void *next = grp->next;
		
		if (next == NULL)
			return OPERATOR_FINISHED;
			
		BLI_remlink(&pose->agroups, grp);
		BLI_insertlinkafter(&pose->agroups, next, grp);
		
		grpIndexB = grpIndexA + 1;
		pose->active_group++;
	}

	/* fix changed bone group indices in bones (swap grpIndexA with grpIndexB) */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->agrp_index == grpIndexB)
			pchan->agrp_index= grpIndexA;
		else if (pchan->agrp_index == grpIndexA)
			pchan->agrp_index= grpIndexB;
	}

	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_group_move(wmOperatorType *ot)
{
	static EnumPropertyItem group_slot_move[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Bone Group";
	ot->idname = "POSE_OT_group_move";
	ot->description = "Change position of active Bone Group in list of Bone Groups";

	/* api callbacks */
	ot->exec = group_move_exec;
	ot->poll = ED_operator_posemode;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "direction", group_slot_move, 0, "Direction", "Direction to move, UP or DOWN");
}

/* bone group sort element */
typedef struct tSortActionGroup {
	bActionGroup *agrp;
	int          index;
} tSortActionGroup;

/* compare bone groups by name */
static int compare_agroup(const void *sgrp_a_ptr, const void *sgrp_b_ptr)
{
	tSortActionGroup *sgrp_a= (tSortActionGroup *)sgrp_a_ptr;
	tSortActionGroup *sgrp_b= (tSortActionGroup *)sgrp_b_ptr;

	return strcmp(sgrp_a->agrp->name, sgrp_b->agrp->name);
}

static int group_sort_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	tSortActionGroup *agrp_array;
	bActionGroup *agrp;
	int agrp_count;
	int i;

	if (ELEM(NULL, ob, pose))
		return OPERATOR_CANCELLED;
	if (pose->active_group <= 0)
		return OPERATOR_CANCELLED;

	/* create temporary array with bone groups and indices */
	agrp_count = BLI_countlist(&pose->agroups);
	agrp_array = MEM_mallocN(sizeof(tSortActionGroup) * agrp_count, "sort bone groups");
	for (agrp= pose->agroups.first, i= 0; agrp; agrp= agrp->next, i++) {
		BLI_assert(i < agrp_count);
		agrp_array[i].agrp = agrp;
		agrp_array[i].index = i+1;
	}

	/* sort bone groups by name */
	qsort(agrp_array, agrp_count, sizeof(tSortActionGroup), compare_agroup);

	/* create sorted bone group list from sorted array */
	pose->agroups.first= pose->agroups.last= NULL;
	for (i= 0; i < agrp_count; i++) {
		BLI_addtail(&pose->agroups, agrp_array[i].agrp);
	}

	/* fix changed bone group indizes in bones */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		for (i= 0; i < agrp_count; i++) {
			if (pchan->agrp_index == agrp_array[i].index) {
				pchan->agrp_index= i+1;
				break;
			}
		}
	}

	/* free temp resources */
	MEM_freeN(agrp_array);

	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_group_sort(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sort Bone Groups";
	ot->idname = "POSE_OT_group_sort";
	ot->description = "Sort Bone Groups by their names in ascending order";

	/* api callbacks */
	ot->exec = group_sort_exec;
	ot->poll = ED_operator_posemode;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static void pose_group_select(bContext *C, Object *ob, int select)
{
	bPose *pose= ob->pose;
	
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, visible_pose_bones)
	{
		if ((pchan->bone->flag & BONE_UNSELECTABLE)==0) {
			if (select) {
				if (pchan->agrp_index == pose->active_group) 
					pchan->bone->flag |= BONE_SELECTED;
			}
			else {
				if (pchan->agrp_index == pose->active_group) 
					pchan->bone->flag &= ~BONE_SELECTED;
			}
		}
	}
	CTX_DATA_END;
}

static int pose_group_select_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	pose_group_select(C, ob, 1);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_select (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Bones of Bone Group";
	ot->idname = "POSE_OT_group_select";
	ot->description = "Select bones in active Bone Group";
	
	/* api callbacks */
	ot->exec = pose_group_select_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int pose_group_deselect_exec (bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= ED_object_context(C);
	else
		ob= object_pose_armature_get(CTX_data_active_object(C));
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	
	pose_group_select(C, ob, 0);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_deselect (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Bone Group";
	ot->idname = "POSE_OT_group_deselect";
	ot->description = "Deselect bones of active Bone Group";
	
	/* api callbacks */
	ot->exec = pose_group_deselect_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************************************** */

static int pose_flip_names_exec (bContext *C, wmOperator *UNUSED(op))
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
	{
		char newname[MAXBONENAME];
		flip_side_name(newname, pchan->name, TRUE);
		ED_armature_bone_rename(arm, pchan->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_flip_names (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Names";
	ot->idname = "POSE_OT_flip_names";
	ot->description = "Flips (and corrects) the axis suffixes of the the names of selected bones";
	
	/* api callbacks */
	ot->exec = pose_flip_names_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------------ */

static int pose_autoside_names_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm;
	char newname[MAXBONENAME];
	short axis= RNA_enum_get(op->ptr, "axis");
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
	{
		BLI_strncpy(newname, pchan->name, sizeof(newname));
		if (bone_autoside_name(newname, 1, axis, pchan->bone->head[axis], pchan->bone->tail[axis]))
			ED_armature_bone_rename(arm, pchan->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_autoside_names (wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[]= {
		{0, "XAXIS", 0, "X-Axis", "Left/Right"},
		{1, "YAXIS", 0, "Y-Axis", "Front/Back"},
		{2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name = "AutoName by Axis";
	ot->idname = "POSE_OT_autoside_names";
	ot->description = "Automatically renames the selected bones according to which side of the target axis they fall on";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = pose_autoside_names_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* settings */
	ot->prop = RNA_def_enum(ot->srna, "axis", axis_items, 0, "Axis", "Axis tag names with");
}

/* ********************************************** */

static int pose_bone_rotmode_exec (bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	int mode = RNA_enum_get(op->ptr, "type");
	
	/* set rotation mode of selected bones  */	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones) 
	{
		pchan->rotmode = mode;
	}
	CTX_DATA_END;
	
	/* notifiers and updates */
	DAG_id_tag_update((ID *)ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_rotation_mode_set (wmOperatorType *ot)
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
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", posebone_rotmode_items, 0, "Rotation Mode", "");
}

/* ********************************************** */

/* Show all armature layers */
static int pose_armature_layers_showall_poll (bContext *C)
{
	/* this single operator can be used in posemode OR editmode for armatures */
	return ED_operator_posemode(C) || ED_operator_editarmature(C);
}

static int pose_armature_layers_showall_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = (ob)? ob->data : NULL;
	PointerRNA ptr;
	int maxLayers = (RNA_boolean_get(op->ptr, "all"))? 32 : 16;
	int layers[32] = {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	int i;
	
	/* sanity checking */
	if (arm == NULL)
		return OPERATOR_CANCELLED;
	
	/* use RNA to set the layers
	 * 	although it would be faster to just set directly using bitflags, we still
	 *	need to setup a RNA pointer so that we get the "update" callbacks for free...
	 */
	RNA_id_pointer_create(&arm->id, &ptr);
	
	for (i = 0; i < maxLayers; i++)
		layers[i] = 1;
	
	RNA_boolean_set_array(&ptr, "layers", layers);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* done */
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_layers_show_all (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show All Layers";
	ot->idname = "ARMATURE_OT_layers_show_all";
	ot->description = "Make all armature layers visible";
	
	/* callbacks */
	ot->exec = pose_armature_layers_showall_exec;
	ot->poll = pose_armature_layers_showall_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_boolean(ot->srna, "all", 1, "All Layers", "Enable all layers or just the first 16 (top row)");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int pose_armature_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm= (ob)? ob->data : NULL;
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
	return WM_operator_props_popup(C, op, evt);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int pose_armature_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */

	if (ELEM(NULL, ob, ob->data)) {
		return OPERATOR_CANCELLED;
	}

	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);

	/* get pointer for armature, and write data there... */
	RNA_id_pointer_create((ID *)ob->data, &ptr);
	RNA_boolean_set_array(&ptr, "layers", layers);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);

	return OPERATOR_FINISHED;
}


void POSE_OT_armature_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Armature Layers";
	ot->idname = "POSE_OT_armature_layers";
	ot->description = "Change the visible armature layers";
	
	/* callbacks */
	ot->invoke = pose_armature_layers_invoke;
	ot->exec = pose_armature_layers_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers to make visible");
}

void ARMATURE_OT_armature_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Armature Layers";
	ot->idname = "ARMATURE_OT_armature_layers";
	ot->description = "Change the visible armature layers";
	
	/* callbacks */
	ot->invoke = pose_armature_layers_invoke;
	ot->exec = pose_armature_layers_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers to make visible");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int pose_bone_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	int layers[32]= {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get layers that are active already */	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones) 
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit= 0; bit < 32; bit++) {
			if (pchan->bone->layer & (1<<bit))
				layers[bit]= 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
		/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, evt);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int pose_bone_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */

	if (ob==NULL || ob->data==NULL) {
		return OPERATOR_CANCELLED;
	}

	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);

	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones)
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)ob->data, &RNA_Bone, pchan->bone, &ptr);
		RNA_boolean_set_array(&ptr, "layers", layers);
	}
	CTX_DATA_END;

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_bone_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Bone Layers";
	ot->idname = "POSE_OT_bone_layers";
	ot->description = "Change the layers that the selected bones belong to";
	
	/* callbacks */
	ot->invoke = pose_bone_layers_invoke;
	ot->exec = pose_bone_layers_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers that bone belongs to");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int armature_bone_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	int layers[32]= {0}; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get layers that are active already */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) 
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit= 0; bit < 32; bit++) {
			if (ebone->layer & (1<<bit))
				layers[bit]= 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
		/* part to sync with other similar operators... */
	return WM_operator_props_popup(C, op, evt);
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int armature_bone_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (ob)? ob->data : NULL;
	PointerRNA ptr;
	int layers[32]; /* hardcoded for now - we can only have 32 armature layers, so this should be fine... */
	
	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);
	
	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) 
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)arm, &RNA_EditBone, ebone, &ptr);
		RNA_boolean_set_array(&ptr, "layers", layers);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_layers (wmOperatorType *ot)
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
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 32, NULL, "Layer", "Armature layers that bone belongs to");
}

/* ********************************************** */
/* Flip Quats */

static int pose_flip_quats_exec (bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= object_pose_armature_get(CTX_data_active_object(C));
	KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
	
	/* loop through all selected pchans, flipping and keying (as needed) */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
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
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_quaternions_flip (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Quats";
	ot->idname = "POSE_OT_quaternions_flip";
	ot->description = "Flip quaternion values to achieve desired rotations, while maintaining the same orientations";
	
	/* callbacks */
	ot->exec = pose_flip_quats_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********************************************** */
/* Clear User Transforms */

static int pose_clear_user_transforms_exec (bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	float cframe = (float)CFRA;
	
	if ((ob->adt) && (ob->adt->action)) {
		/* XXX: this is just like this to avoid contaminating anything else; 
		 * just pose values should change, so this should be fine 
		 */
		bPose *dummyPose = NULL;
		Object workob = {{0}}; 
		bPoseChannel *pchan;
		
		/* execute animation step for current frame using a dummy copy of the pose */		
		copy_pose(&dummyPose, ob->pose, 0);
		
		BLI_strncpy(workob.id.name, "OB<ClearTfmWorkOb>", sizeof(workob.id.name));
		workob.type = OB_ARMATURE;
		workob.data = ob->data;
		workob.adt = ob->adt;
		workob.pose = dummyPose;
		
		BKE_animsys_evaluate_animdata(scene, &workob.id, workob.adt, cframe, ADT_RECALC_ANIM);
		
		/* copy back values, but on selected bones only  */
		for (pchan = dummyPose->chanbase.first; pchan; pchan = pchan->next) {
			pose_bone_do_paste(ob, pchan, 1, 0);
		}
		
		/* free temp data - free manually as was copied without constraints */
		if (dummyPose) {
			for (pchan= dummyPose->chanbase.first; pchan; pchan= pchan->next) {
				if (pchan->prop) {
					IDP_FreeProperty(pchan->prop);
					MEM_freeN(pchan->prop);
				}
			}
			
			/* was copied without constraints */
			BLI_freelistN(&dummyPose->chanbase);
			MEM_freeN(dummyPose);
		}
	}
	else {
		/* no animation, so just reset whole pose to rest pose 
		 * (cannot just restore for selected though)
		 */
		rest_pose(ob->pose);
	}
	
	/* notifiers and updates */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_user_transforms_clear (wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear User Transforms";
	ot->idname = "POSE_OT_user_transforms_clear";
	ot->description = "Reset pose on selected bones to keyframed state";
	
	/* callbacks */
	ot->exec = pose_clear_user_transforms_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

