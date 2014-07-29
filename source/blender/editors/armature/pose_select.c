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
 * Contributor(s): Blender Foundation, 2002-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/pose_select.c
 *  \ingroup edarmature
 */

#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* utility macros fro storing a temp int in the bone (selection flag) */
#define PBONE_PREV_FLAG_GET(pchan) ((void)0, (GET_INT_FROM_POINTER((pchan)->temp)))
#define PBONE_PREV_FLAG_SET(pchan, val) ((pchan)->temp = SET_INT_IN_POINTER(val))


/* ***************** Pose Select Utilities ********************* */

/* Note: SEL_TOGGLE is assumed to have already been handled! */
static void pose_do_bone_select(bPoseChannel *pchan, const int select_mode) {
	/* select pchan only if selectable, but deselect works always */
	switch (select_mode) {
		case SEL_SELECT:
			if (!(pchan->bone->flag & BONE_UNSELECTABLE))
				pchan->bone->flag |= BONE_SELECTED;
			break;
		case SEL_DESELECT:
			pchan->bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			break;
		case SEL_INVERT:
			if (pchan->bone->flag & BONE_SELECTED) {
				pchan->bone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			}
			else if (!(pchan->bone->flag & BONE_UNSELECTABLE)) {
				pchan->bone->flag |= BONE_SELECTED;
			}
			break;
	}
}

/* Utility method for changing the selection status of a bone */
void ED_pose_bone_select(Object *ob, bPoseChannel *pchan, bool select)
{
	bArmature *arm;

	/* sanity checks */
	// XXX: actually, we can probably still get away with no object - at most we have no updates
	if (ELEM(NULL, ob, ob->pose, pchan, pchan->bone))
		return;
	
	arm = ob->data;
	
	/* can only change selection state if bone can be modified */
	if (PBONE_SELECTABLE(arm, pchan->bone)) {
		/* change selection state - activate too if selected */
		if (select) {
			pchan->bone->flag |= BONE_SELECTED;
			arm->act_bone = pchan->bone;
		}
		else {
			pchan->bone->flag &= ~BONE_SELECTED;
			arm->act_bone = NULL;
		}
		
		// TODO: select and activate corresponding vgroup?
		
		/* tag necessary depsgraph updates 
		 * (see rna_Bone_select_update() in rna_armature.c for details)
		 */
		if (arm->flag & ARM_HAS_VIZ_DEPS) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
		
		/* send necessary notifiers */
		WM_main_add_notifier(NC_GEOM | ND_DATA, ob);
	}
}

/* called from editview.c, for mode-less pose selection */
/* assumes scene obact and basact is still on old situation */
int ED_do_pose_selectbuffer(Scene *scene, Base *base, unsigned int *buffer, short hits,
                            bool extend, bool deselect, bool toggle)
{
	Object *ob = base->object;
	Bone *nearBone;
	
	if (!ob || !ob->pose) return 0;

	nearBone = get_bone_from_selectbuffer(scene, base, buffer, hits, 1);
	
	/* if the bone cannot be affected, don't do anything */
	if ((nearBone) && !(nearBone->flag & BONE_UNSELECTABLE)) {
		Object *ob_act = OBACT;
		bArmature *arm = ob->data;
		
		/* since we do unified select, we don't shift+select a bone if the
		 * armature object was not active yet.
		 * note, special exception for armature mode so we can do multi-select
		 * we could check for multi-select explicitly but think its fine to
		 * always give predictable behavior in weight paint mode - campbell */
		if ((ob_act == NULL) || ((ob_act != ob) && (ob_act->mode & OB_MODE_WEIGHT_PAINT) == 0)) {
			/* when we are entering into posemode via toggle-select,
			 * frop another active object - always select the bone. */
			if (!extend && !deselect && toggle) {
				/* re-select below */
				nearBone->flag &= ~BONE_SELECTED;
			}
		}

		if (!extend && !deselect && !toggle) {
			ED_pose_de_selectall(ob, SEL_DESELECT, true);
			nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			arm->act_bone = nearBone;
		}
		else {
			if (extend) {
				nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				arm->act_bone = nearBone;
			}
			else if (deselect) {
				nearBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			}
			else if (toggle) {
				if (nearBone->flag & BONE_SELECTED) {
					/* if not active, we make it active */
					if (nearBone != arm->act_bone) {
						arm->act_bone = nearBone;
					}
					else {
						nearBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					}
				}
				else {
					nearBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					arm->act_bone = nearBone;
				}
			}
		}
		
		if (ob_act) {
			/* in weightpaint we select the associated vertex group too */
			if (ob_act->mode & OB_MODE_WEIGHT_PAINT) {
				if (nearBone == arm->act_bone) {
					ED_vgroup_select_by_name(ob_act, nearBone->name);
					DAG_id_tag_update(&ob_act->id, OB_RECALC_DATA);
				}
			}
			/* if there are some dependencies for visualizing armature state 
			 * (e.g. Mask Modifier in 'Armature' mode), force update 
			 */
			else if (arm->flag & ARM_HAS_VIZ_DEPS) {
				/* NOTE: ob not ob_act here is intentional - it's the source of the 
				 *       bones being selected  [T37247]
				 */
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
		}
	}
	
	return nearBone != NULL;
}

/* 'select_mode' is usual SEL_SELECT/SEL_DESELECT/SEL_TOGGLE/SEL_INVERT.
 * When true, 'ignore_visibility' makes this func also affect invisible bones (hidden or on hidden layers). */
void ED_pose_de_selectall(Object *ob, int select_mode, const bool ignore_visibility)
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	
	/* we call this from outliner too */
	if (ob->pose == NULL) {
		return;
	}
	
	/*	Determine if we're selecting or deselecting	*/
	if (select_mode == SEL_TOGGLE) {
		select_mode = SEL_SELECT;
		for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if (ignore_visibility || PBONE_VISIBLE(arm, pchan->bone)) {
				if (pchan->bone->flag & BONE_SELECTED) {
					select_mode = SEL_DESELECT;
					break;
				}
			}
		}
	}
	
	/* Set the flags accordingly */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		/* ignore the pchan if it isn't visible or if its selection cannot be changed */
		if (ignore_visibility || PBONE_VISIBLE(arm, pchan->bone)) {
			pose_do_bone_select(pchan, select_mode);
		}
	}
}

/* ***************** Selections ********************** */

static void selectconnected_posebonechildren(Object *ob, Bone *bone, int extend)
{
	Bone *curBone;
	
	/* stop when unconnected child is encountered, or when unselectable bone is encountered */
	if (!(bone->flag & BONE_CONNECTED) || (bone->flag & BONE_UNSELECTABLE))
		return;
	
	/* XXX old cruft! use notifiers instead */
	//select_actionchannel_by_name (ob->action, bone->name, !(shift));
	
	if (extend)
		bone->flag &= ~BONE_SELECTED;
	else
		bone->flag |= BONE_SELECTED;
	
	for (curBone = bone->childbase.first; curBone; curBone = curBone->next)
		selectconnected_posebonechildren(ob, curBone, extend);
}

/* within active object context */
/* previously known as "selectconnected_posearmature" */
static int pose_select_connected_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = (bArmature *)ob->data;
	Bone *bone, *curBone, *next = NULL;
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	view3d_operator_needs_opengl(C);
	
	if (extend)
		bone = get_nearest_bone(C, 0, event->mval[0], event->mval[1]);
	else
		bone = get_nearest_bone(C, 1, event->mval[0], event->mval[1]);
	
	if (!bone)
		return OPERATOR_CANCELLED;
	
	/* Select parents */
	for (curBone = bone; curBone; curBone = next) {
		/* ignore bone if cannot be selected */
		if ((curBone->flag & BONE_UNSELECTABLE) == 0) {
			if (extend)
				curBone->flag &= ~BONE_SELECTED;
			else
				curBone->flag |= BONE_SELECTED;
			
			if (curBone->flag & BONE_CONNECTED)
				next = curBone->parent;
			else
				next = NULL;
		}
		else
			next = NULL;
	}
	
	/* Select children */
	for (curBone = bone->childbase.first; curBone; curBone = next)
		selectconnected_posebonechildren(ob, curBone, extend);
	
	/* updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	if (arm->flag & ARM_HAS_VIZ_DEPS) {
		/* mask modifier ('armature' mode), etc. */
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

static int pose_select_linked_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && ED_operator_posemode(C));
}

void POSE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Connected";
	ot->idname = "POSE_OT_select_linked";
	ot->description = "Select bones related to selected ones by parent/child relationships";
	
	/* api callbacks */
	/* leave 'exec' unset */
	ot->invoke = pose_select_connected_invoke;
	ot->poll = pose_select_linked_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
}

/* -------------------------------------- */

static int pose_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");
	
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	bArmature *arm = ob->data;
	int multipaint = scene->toolsettings->multipaint;

	if (action == SEL_TOGGLE) {
		action = CTX_DATA_COUNT(C, selected_pose_bones) ? SEL_DESELECT : SEL_SELECT;
	}
	
	/*	Set the flags */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones)
	{
		pose_do_bone_select(pchan, action);
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, NULL);
	
	/* weightpaint or mask modifiers need depsgraph updates */
	if (multipaint || (arm->flag & ARM_HAS_VIZ_DEPS)) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}

	return OPERATOR_FINISHED;
}

void POSE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "POSE_OT_select_all";
	ot->description = "Toggle selection status of all bones";
	
	/* api callbacks */
	ot->exec = pose_de_select_all_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_select_all(ot);
}

/* -------------------------------------- */

static int pose_select_parent_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = (bArmature *)ob->data;
	bPoseChannel *pchan, *parent;

	/* Determine if there is an active bone */
	pchan = CTX_data_active_pose_bone(C);
	if (pchan) {
		parent = pchan->parent;
		if ((parent) && !(parent->bone->flag & (BONE_HIDDEN_P | BONE_UNSELECTABLE))) {
			parent->bone->flag |= BONE_SELECTED;
			arm->act_bone = parent->bone;
		}
		else {
			return OPERATOR_CANCELLED;
		}
	}
	else {
		return OPERATOR_CANCELLED;
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	if (arm->flag & ARM_HAS_VIZ_DEPS) {
		/* mask modifier ('armature' mode), etc. */
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	
	return OPERATOR_FINISHED;
}

void POSE_OT_select_parent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Parent Bone";
	ot->idname = "POSE_OT_select_parent";
	ot->description = "Select bones that are parents of the currently selected bones";

	/* api callbacks */
	ot->exec = pose_select_parent_exec;
	ot->poll = ED_operator_posemode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

static int pose_select_constraint_target_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = (bArmature *)ob->data;
	bConstraint *con;
	int found = 0;
	
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
	{
		if (pchan->bone->flag & BONE_SELECTED) {
			for (con = pchan->constraints.first; con; con = con->next) {
				bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct = targets.first; ct; ct = ct->next) {
						if ((ct->tar == ob) && (ct->subtarget[0])) {
							bPoseChannel *pchanc = BKE_pose_channel_find_name(ob->pose, ct->subtarget);
							if ((pchanc) && !(pchanc->bone->flag & BONE_UNSELECTABLE)) {
								pchanc->bone->flag |= BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
								found = 1;
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
	
	/* updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	if (arm->flag & ARM_HAS_VIZ_DEPS) {
		/* mask modifier ('armature' mode), etc. */
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------- */

static int pose_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = ob->data;
	Bone *curbone, *pabone, *chbone;
	int direction = RNA_enum_get(op->ptr, "direction");
	const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
	bool found = false;
	
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
	{
		curbone = pchan->bone;
		
		if ((curbone->flag & BONE_UNSELECTABLE) == 0) {
			if (curbone == arm->act_bone) {
				if (direction == BONE_SELECT_PARENT) {
					if (pchan->parent == NULL) continue;
					else pabone = pchan->parent->bone;
					
					if (PBONE_SELECTABLE(arm, pabone)) {
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						pabone->flag |= BONE_SELECTED;
						arm->act_bone = pabone;
						
						found = 1;
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
							if (PBONE_SELECTABLE(arm, pchan_child->bone)) {
								if (pchan_child->parent == pchan) {
									chbone = pchan_child->bone;
									break;
								}
							}
						}
					}

					if (chbone == NULL) continue;
#endif
					
					if (PBONE_SELECTABLE(arm, chbone)) {
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						chbone->flag |= BONE_SELECTED;
						arm->act_bone = chbone;
						
						found = 1;
						break;
					}
				}
			}
		}
	}
	CTX_DATA_END;

	if (found == 0)
		return OPERATOR_CANCELLED;
	
	/* updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	if (arm->flag & ARM_HAS_VIZ_DEPS) {
		/* mask modifier ('armature' mode), etc. */
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	
	return OPERATOR_FINISHED;
}

void POSE_OT_select_hierarchy(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	ot->prop = RNA_def_enum(ot->srna, "direction", direction_items, BONE_SELECT_PARENT, "Direction", "");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/* -------------------------------------- */

/* modes for select same */
typedef enum ePose_SelectSame_Mode {
	POSE_SEL_SAME_LAYER      = 0,
	POSE_SEL_SAME_GROUP      = 1,
	POSE_SEL_SAME_KEYINGSET  = 2,
} ePose_SelectSame_Mode;

static bool pose_select_same_group(bContext *C, Object *ob, bool extend)
{
	bArmature *arm = (ob) ? ob->data : NULL;
	bPose *pose = (ob) ? ob->pose : NULL;
	char *group_flags;
	int numGroups = 0;
	bool changed = false, tagged = false;
	
	/* sanity checks */
	if (ELEM(NULL, ob, pose, arm))
		return 0;
		
	/* count the number of groups */
	numGroups = BLI_countlist(&pose->agroups);
	if (numGroups == 0)
		return 0;
		
	/* alloc a small array to keep track of the groups to use 
	 *  - each cell stores on/off state for whether group should be used
	 *	- size is (numGroups + 1), since (index = 0) is used for no-group
	 */
	group_flags = MEM_callocN(numGroups + 1, "pose_select_same_group");
	
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
	{
		/* keep track of group as group to use later? */
		if (pchan->bone->flag & BONE_SELECTED) {
			group_flags[pchan->agrp_index] = 1;
			tagged = true;
		}
		
		/* deselect all bones before selecting new ones? */
		if ((extend == false) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0)
			pchan->bone->flag &= ~BONE_SELECTED;
	}
	CTX_DATA_END;
	
	/* small optimization: only loop through bones a second time if there are any groups tagged */
	if (tagged) {
		/* only if group matches (and is not selected or current bone) */
		CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
		{
			if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
				/* check if the group used by this bone is counted */
				if (group_flags[pchan->agrp_index]) {
					pchan->bone->flag |= BONE_SELECTED;
					changed = true;
				}
			}
		}
		CTX_DATA_END;
	}
	
	/* free temp info */
	MEM_freeN(group_flags);
	
	return changed;
}

static bool pose_select_same_layer(bContext *C, Object *ob, bool extend)
{
	bPose *pose = (ob) ? ob->pose : NULL;
	bArmature *arm = (ob) ? ob->data : NULL;
	bool changed = false;
	int layers = 0;
	
	if (ELEM(NULL, ob, pose, arm))
		return 0;
	
	/* figure out what bones are selected */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
	{
		/* keep track of layers to use later? */
		if (pchan->bone->flag & BONE_SELECTED)
			layers |= pchan->bone->layer;
			
		/* deselect all bones before selecting new ones? */
		if ((extend == false) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0)
			pchan->bone->flag &= ~BONE_SELECTED;
	}
	CTX_DATA_END;
	if (layers == 0) 
		return 0;
		
	/* select bones that are on same layers as layers flag */
	CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
	{
		/* if bone is on a suitable layer, and the bone can have its selection changed, select it */
		if ((layers & pchan->bone->layer) && (pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
			pchan->bone->flag |= BONE_SELECTED;
			changed = true;
		}
	}
	CTX_DATA_END;
	
	return changed;
}

static bool pose_select_same_keyingset(bContext *C, ReportList *reports, Object *ob, bool extend)
{
	KeyingSet *ks = ANIM_scene_get_active_keyingset(CTX_data_scene(C));
	KS_Path *ksp;
	
	bArmature *arm = (ob) ? ob->data : NULL;
	bPose *pose = (ob) ? ob->pose : NULL;
	bool changed = false;
	
	/* sanity checks: validate Keying Set and object */
	if (ks == NULL) {
		BKE_report(reports, RPT_ERROR, "No active Keying Set to use");
		return false;
	}
	else if (ANIM_validate_keyingset(C, NULL, ks) != 0) {
		if (ks->paths.first == NULL) {
			if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
				BKE_report(reports, RPT_ERROR, 
				           "Use another Keying Set, as the active one depends on the currently "
				           "selected items or cannot find any targets due to unsuitable context");
			}
			else {
				BKE_report(reports, RPT_ERROR, "Keying Set does not contain any paths");
			}
		}
		return false;
	}
		
	if (ELEM(NULL, ob, pose, arm))
		return false;
		
	/* if not extending selection, deselect all selected first */
	if (extend == false) {
		CTX_DATA_BEGIN (C, bPoseChannel *, pchan, visible_pose_bones)
		{
			if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0)
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
				char *boneName = BLI_str_quoted_substrN(ksp->rna_path, "bones[");
				
				if (boneName) {
					bPoseChannel *pchan = BKE_pose_channel_find_name(pose, boneName);
					
					if (pchan) {
						/* select if bone is visible and can be affected */
						if (PBONE_SELECTABLE(arm, pchan->bone)) {
							pchan->bone->flag |= BONE_SELECTED;
							changed = true;
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

static int pose_select_grouped_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	bArmature *arm = (bArmature *)ob->data;
	const ePose_SelectSame_Mode type = RNA_enum_get(op->ptr, "type");
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	bool changed = false;
	
	/* sanity check */
	if (ob->pose == NULL)
		return OPERATOR_CANCELLED;
		
	/* selection types */
	switch (type) {
		case POSE_SEL_SAME_LAYER: /* layer */
			changed = pose_select_same_layer(C, ob, extend);
			break;
		
		case POSE_SEL_SAME_GROUP: /* group */
			changed = pose_select_same_group(C, ob, extend);
			break;
			
		case POSE_SEL_SAME_KEYINGSET: /* Keying Set */
			changed = pose_select_same_keyingset(C, op->reports, ob, extend);
			break;
		
		default:
			printf("pose_select_grouped() - Unknown selection type %d\n", type);
			break;
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	if (arm->flag & ARM_HAS_VIZ_DEPS) {
		/* mask modifier ('armature' mode), etc. */
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	
	/* report done status */
	if (changed)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_select_grouped(wmOperatorType *ot)
{
	static EnumPropertyItem prop_select_grouped_types[] = {
		{POSE_SEL_SAME_LAYER, "LAYER", 0, "Layer", "Shared layers"},
		{POSE_SEL_SAME_GROUP, "GROUP", 0, "Group", "Shared group"},
		{POSE_SEL_SAME_KEYINGSET, "KEYINGSET", 0, "Keying Set", "All bones affected by active Keying Set"},
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
	ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

/* -------------------------------------- */

/**
 * \note clone of #armature_select_mirror_exec keep in sync
 */
static int pose_select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *ob_act = CTX_data_active_object(C);
	Object *ob = BKE_object_pose_armature_get(ob_act);
	bArmature *arm;
	bPoseChannel *pchan, *pchan_mirror_act = NULL;
	const bool active_only = RNA_boolean_get(op->ptr, "only_active");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	if ((ob && (ob->mode & OB_MODE_POSE)) == 0) {
		return OPERATOR_CANCELLED;
	}

	arm = ob->data;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		const int flag = (pchan->bone->flag & BONE_SELECTED);
		PBONE_PREV_FLAG_SET(pchan, flag);
	}

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (PBONE_SELECTABLE(arm, pchan->bone)) {
			bPoseChannel *pchan_mirror;
			int flag_new = extend ? PBONE_PREV_FLAG_GET(pchan) : 0;

			if ((pchan_mirror = BKE_pose_channel_get_mirrored(ob->pose, pchan->name)) &&
			    (PBONE_VISIBLE(arm, pchan_mirror->bone)))
			{
				const int flag_mirror = PBONE_PREV_FLAG_GET(pchan_mirror);
				flag_new |= flag_mirror;

				if (pchan->bone == arm->act_bone) {
					pchan_mirror_act = pchan_mirror;
				}

				/* skip all but the active or its mirror */
				if (active_only && !ELEM(arm->act_bone, pchan->bone, pchan_mirror->bone)) {
					continue;
				}
			}

			pchan->bone->flag = (pchan->bone->flag & ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) | flag_new;
		}
	}

	if (pchan_mirror_act) {
		arm->act_bone = pchan_mirror_act->bone;

		/* in weightpaint we select the associated vertex group too */
		if (ob_act->mode & OB_MODE_WEIGHT_PAINT) {
			ED_vgroup_select_by_name(ob_act, pchan_mirror_act->name);
			DAG_id_tag_update(&ob_act->id, OB_RECALC_DATA);
		}
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Active/Selected Bone";
	ot->idname = "POSE_OT_select_mirror";
	ot->description = "Mirror the bone selection";
	
	/* api callbacks */
	ot->exec = pose_select_mirror_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "only_active", false, "Active Only", "Only operate on the active bone");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

