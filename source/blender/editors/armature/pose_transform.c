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

/** \file blender/editors/armature/pose_transform.c
 *  \ingroup edarmature
 */

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "armature_intern.h"


/* ********************************************** */
/* Pose Apply */

/* helper for apply_armature_pose2bones - fixes parenting of objects that are bone-parented to armature */
static void applyarmature_fix_boneparents(Scene *scene, Object *armob)
{
	Object workob, *ob;
	
	/* go through all objects in database */
	for (ob = G.main->object.first; ob; ob = ob->id.next) {
		/* if parent is bone in this armature, apply corrections */
		if ((ob->parent == armob) && (ob->partype == PARBONE)) {
			/* apply current transform from parent (not yet destroyed), 
			 * then calculate new parent inverse matrix
			 */
			BKE_object_apply_mat4(ob, ob->obmat, false, false);
			
			BKE_object_workob_calc_parent(scene, ob, &workob);
			invert_m4_m4(ob->parentinv, workob.obmat);
		}
	}
}

/* set the current pose as the restpose */
static int apply_armature_pose2bones_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C)); // must be active object, not edit-object
	bArmature *arm = BKE_armature_from_object(ob);
	bPose *pose;
	bPoseChannel *pchan;
	EditBone *curbone;
	
	/* don't check if editmode (should be done by caller) */
	if (ob->type != OB_ARMATURE)
		return OPERATOR_CANCELLED;
	if (BKE_object_obdata_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot apply pose to lib-linked armature"); /* error_libdata(); */
		return OPERATOR_CANCELLED;
	}

	/* helpful warnings... */
	/* TODO: add warnings to be careful about actions, applying deforms first, etc. */
	if (ob->adt && ob->adt->action)
		BKE_report(op->reports, RPT_WARNING,
		           "Actions on this armature will be destroyed by this new rest pose as the "
		           "transforms stored are relative to the old rest pose");

	/* Get editbones of active armature to alter */
	ED_armature_to_edit(arm);
	
	/* get pose of active object and move it out of posemode */
	pose = ob->pose;
	
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		curbone = ED_armature_bone_find_name(arm->edbo, pchan->name);
		
		/* simply copy the head/tail values from pchan over to curbone */
		copy_v3_v3(curbone->head, pchan->pose_head);
		copy_v3_v3(curbone->tail, pchan->pose_tail);
		
		/* fix roll:
		 *	1. find auto-calculated roll value for this bone now
		 *	2. remove this from the 'visual' y-rotation
		 */
		{
			float premat[3][3], imat[3][3], pmat[3][3], tmat[3][3];
			float delta[3], eul[3];
			
			/* obtain new auto y-rotation */
			sub_v3_v3v3(delta, curbone->tail, curbone->head);
			vec_roll_to_mat3(delta, 0.0f, premat);
			invert_m3_m3(imat, premat);
			
			/* get pchan 'visual' matrix */
			copy_m3_m4(pmat, pchan->pose_mat);
			
			/* remove auto from visual and get euler rotation */
			mul_m3_m3m3(tmat, imat, pmat);
			mat3_to_eul(eul, tmat);
			
			/* just use this euler-y as new roll value */
			curbone->roll = eul[1];
		}
		
		/* clear transform values for pchan */
		zero_v3(pchan->loc);
		zero_v3(pchan->eul);
		unit_qt(pchan->quat);
		unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
		pchan->size[0] = pchan->size[1] = pchan->size[2] = 1.0f;
		
		/* set anim lock */
		curbone->flag |= BONE_UNKEYED;
	}
	
	/* convert editbones back to bones, and then free the edit-data */
	ED_armature_from_edit(arm);
	ED_armature_edit_free(arm);
	
	/* flush positions of posebones */
	BKE_pose_where_is(scene, ob);
	
	/* fix parenting of objects which are bone-parented */
	applyarmature_fix_boneparents(scene, ob);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_armature_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Apply Pose as Rest Pose";
	ot->idname = "POSE_OT_armature_apply";
	ot->description = "Apply the current pose as the new rest pose";
	
	/* callbacks */
	ot->exec = apply_armature_pose2bones_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* set the current pose as the restpose */
static int pose_visual_transform_apply_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C)); // must be active object, not edit-object

	/* don't check if editmode (should be done by caller) */
	if (ob->type != OB_ARMATURE)
		return OPERATOR_CANCELLED;

	/* loop over all selected pchans
	 *
	 * TODO, loop over children before parents if multiple bones
	 * at once are to be predictable*/
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones)
	{
		float delta_mat[4][4];
		
		/* chan_mat already contains the delta transform from rest pose to pose-mode pose
		 * as that is baked into there so that B-Bones will work. Once we've set this as the
		 * new raw-transform components, don't recalc the poses yet, otherwise IK result will 
		 * change, thus changing the result we may be trying to record.
		 */
		/* XXX For some reason, we can't use pchan->chan_mat here, gives odd rotation/offset (see T38251).
		 *     Using pchan->pose_mat and bringing it back in bone space seems to work as expected!
		 */
		BKE_armature_mat_pose_to_bone(pchan, pchan->pose_mat, delta_mat);
		
		BKE_pchan_apply_mat4(pchan, delta_mat, true);
	}
	CTX_DATA_END;
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_visual_transform_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Apply Visual Transform to Pose";
	ot->idname = "POSE_OT_visual_transform_apply";
	ot->description = "Apply final constrained position of pose bones to their transform";
	
	/* callbacks */
	ot->exec = pose_visual_transform_apply_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Copy/Paste */

/* Global copy/paste buffer for pose - cleared on start/end session + before every copy operation */
static bPose *g_posebuf = NULL;

void ED_clipboard_posebuf_free(void)
{
	if (g_posebuf) {
		bPoseChannel *pchan;
		
		for (pchan = g_posebuf->chanbase.first; pchan; pchan = pchan->next) {
			if (pchan->prop) {
				IDP_FreeProperty(pchan->prop);
				MEM_freeN(pchan->prop);
			}
		}
		
		/* was copied without constraints */
		BLI_freelistN(&g_posebuf->chanbase);
		MEM_freeN(g_posebuf);
	}
	
	g_posebuf = NULL;
}

/* This function is used to indicate that a bone is selected 
 * and needs to be included in copy buffer (used to be for inserting keys)
 */
static void set_pose_keys(Object *ob)
{
	bArmature *arm = ob->data;
	bPoseChannel *chan;

	if (ob->pose) {
		for (chan = ob->pose->chanbase.first; chan; chan = chan->next) {
			Bone *bone = chan->bone;
			if ((bone) && (bone->flag & BONE_SELECTED) && (arm->layer & bone->layer))
				chan->flag |= POSE_KEY;
			else
				chan->flag &= ~POSE_KEY;
		}
	}
}

/**
 * Perform paste pose, for a single bone.
 *
 * \param ob Object where bone to paste to lives
 * \param chan Bone that pose to paste comes from
 * \param selOnly Only paste on selected bones
 * \param flip Flip on x-axis
 * \return Whether the bone that we pasted to if we succeeded
 */
static bPoseChannel *pose_bone_do_paste(Object *ob, bPoseChannel *chan, const bool selOnly, const bool flip)
{
	bPoseChannel *pchan;
	char name[MAXBONENAME];
	short paste_ok;
	
	/* get the name - if flipping, we must flip this first */
	if (flip)
		BKE_deform_flip_side_name(name, chan->name, false);
	else
		BLI_strncpy(name, chan->name, sizeof(name));
	
	/* only copy when:
	 *  1) channel exists - poses are not meant to add random channels to anymore
	 *  2) if selection-masking is on, channel is selected - only selected bones get pasted on, allowing making both sides symmetrical
	 */
	pchan = BKE_pose_channel_find_name(ob->pose, name);
	
	if (selOnly)
		paste_ok = ((pchan) && (pchan->bone->flag & BONE_SELECTED));
	else
		paste_ok = ((pchan != NULL));
	
	/* continue? */
	if (paste_ok) {
		/* only loc rot size 
		 *	- only copies transform info for the pose 
		 */
		copy_v3_v3(pchan->loc, chan->loc);
		copy_v3_v3(pchan->size, chan->size);
		pchan->flag = chan->flag;
		
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
				axis_angle_to_eulO(pchan->eul, pchan->rotmode, chan->rotAxis, chan->rotAngle);
			else
				quat_to_eulO(pchan->eul, pchan->rotmode, chan->quat);
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
			pchan->loc[0] *= -1;
			
			/* has to be done as eulers... */
			if (pchan->rotmode > 0) {
				pchan->eul[1] *= -1;
				pchan->eul[2] *= -1;
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				float eul[3];
				
				axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, pchan->rotAxis, pchan->rotAngle);
				eul[1] *= -1;
				eul[2] *= -1;
				eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, eul, EULER_ORDER_DEFAULT);
			}
			else {
				float eul[3];
				
				normalize_qt(pchan->quat);
				quat_to_eul(eul, pchan->quat);
				eul[1] *= -1;
				eul[2] *= -1;
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
				pchan->prop = IDP_CopyProperty(chan->prop);
			}
		}
	}
	
	/* return whether paste went ahead */
	return pchan;
}

/* ---- */

static int pose_copy_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	
	/* sanity checking */
	if (ELEM(NULL, ob, ob->pose)) {
		BKE_report(op->reports, RPT_ERROR, "No pose to copy");
		return OPERATOR_CANCELLED;
	}

	/* free existing pose buffer */
	ED_clipboard_posebuf_free();
	
	/* sets chan->flag to POSE_KEY if bone selected, then copy those bones to the buffer */
	set_pose_keys(ob);  
	BKE_pose_copy_data(&g_posebuf, ob->pose, 0);
	
	
	return OPERATOR_FINISHED;
}

void POSE_OT_copy(wmOperatorType *ot)
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

static int pose_paste_exec(bContext *C, wmOperator *op)
{
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	Scene *scene = CTX_data_scene(C);
	bPoseChannel *chan;
	const bool flip = RNA_boolean_get(op->ptr, "flipped");
	bool selOnly = RNA_boolean_get(op->ptr, "selected_mask");

	/* get KeyingSet to use */
	KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);

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
	for (chan = g_posebuf->chanbase.first; chan; chan = chan->next) {
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
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_paste(wmOperatorType *ot)
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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "flipped", false, "Flipped on X-Axis", "Paste the stored pose flipped on to current pose");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_boolean(ot->srna, "selected_mask", false, "On Selected Only", "Only paste the stored pose on to selected bones in the current pose");
}

/* ********************************************** */
/* Clear Pose Transforms */

/* clear scale of pose-channel */
static void pchan_clear_scale(bPoseChannel *pchan)
{
	if ((pchan->protectflag & OB_LOCK_SCALEX) == 0)
		pchan->size[0] = 1.0f;
	if ((pchan->protectflag & OB_LOCK_SCALEY) == 0)
		pchan->size[1] = 1.0f;
	if ((pchan->protectflag & OB_LOCK_SCALEZ) == 0)
		pchan->size[2] = 1.0f;
}

/* clear location of pose-channel */
static void pchan_clear_loc(bPoseChannel *pchan)
{
	if ((pchan->protectflag & OB_LOCK_LOCX) == 0)
		pchan->loc[0] = 0.0f;
	if ((pchan->protectflag & OB_LOCK_LOCY) == 0)
		pchan->loc[1] = 0.0f;
	if ((pchan->protectflag & OB_LOCK_LOCZ) == 0)
		pchan->loc[2] = 0.0f;
}

/* clear rotation of pose-channel */
static void pchan_clear_rot(bPoseChannel *pchan)
{
	if (pchan->protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) {
		/* check if convert to eulers for locking... */
		if (pchan->protectflag & OB_LOCK_ROT4D) {
			/* perform clamping on a component by component basis */
			if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				if ((pchan->protectflag & OB_LOCK_ROTW) == 0)
					pchan->rotAngle = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
					pchan->rotAxis[0] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
					pchan->rotAxis[1] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
					pchan->rotAxis[2] = 0.0f;
					
				/* check validity of axis - axis should never be 0,0,0 (if so, then we make it rotate about y) */
				if (IS_EQF(pchan->rotAxis[0], pchan->rotAxis[1]) && IS_EQF(pchan->rotAxis[1], pchan->rotAxis[2]))
					pchan->rotAxis[1] = 1.0f;
			}
			else if (pchan->rotmode == ROT_MODE_QUAT) {
				if ((pchan->protectflag & OB_LOCK_ROTW) == 0)
					pchan->quat[0] = 1.0f;
				if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
					pchan->quat[1] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
					pchan->quat[2] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
					pchan->quat[3] = 0.0f;
			}
			else {
				/* the flag may have been set for the other modes, so just ignore the extra flag... */
				if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
					pchan->eul[0] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
					pchan->eul[1] = 0.0f;
				if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
					pchan->eul[2] = 0.0f;
			}
		}
		else {
			/* perform clamping using euler form (3-components) */
			float eul[3], oldeul[3], quat1[4] = {0};
			float qlen = 0.0f;
			
			if (pchan->rotmode == ROT_MODE_QUAT) {
				qlen = normalize_qt_qt(quat1, pchan->quat);
				quat_to_eul(oldeul, quat1);
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, pchan->rotAxis, pchan->rotAngle);
			}
			else {
				copy_v3_v3(oldeul, pchan->eul);
			}
			
			eul[0] = eul[1] = eul[2] = 0.0f;
			
			if (pchan->protectflag & OB_LOCK_ROTX)
				eul[0] = oldeul[0];
			if (pchan->protectflag & OB_LOCK_ROTY)
				eul[1] = oldeul[1];
			if (pchan->protectflag & OB_LOCK_ROTZ)
				eul[2] = oldeul[2];
			
			if (pchan->rotmode == ROT_MODE_QUAT) {
				eul_to_quat(pchan->quat, eul);
				
				/* restore original quat size */
				mul_qt_fl(pchan->quat, qlen);
				
				/* quaternions flip w sign to accumulate rotations correctly */
				if ((quat1[0] < 0.0f && pchan->quat[0] > 0.0f) || (quat1[0] > 0.0f && pchan->quat[0] < 0.0f)) {
					mul_qt_fl(pchan->quat, -1.0f);
				}
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, eul, EULER_ORDER_DEFAULT);
			}
			else {
				copy_v3_v3(pchan->eul, eul);
			}
		}
	}       /* Duplicated in source/blender/editors/object/object_transform.c */
	else {
		if (pchan->rotmode == ROT_MODE_QUAT) {
			unit_qt(pchan->quat);
		}
		else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
			/* by default, make rotation of 0 radians around y-axis (roll) */
			unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
		}
		else {
			zero_v3(pchan->eul);
		}
	}
}

/* clear loc/rot/scale of pose-channel */
static void pchan_clear_transforms(bPoseChannel *pchan)
{
	pchan_clear_loc(pchan);
	pchan_clear_rot(pchan);
	pchan_clear_scale(pchan);
}

/* --------------- */

/* generic exec for clear-pose operators */
static int pose_clear_transform_generic_exec(bContext *C, wmOperator *op, 
                                             void (*clear_func)(bPoseChannel *), const char default_ksName[])
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
	short autokey = 0;
	
	/* sanity checks */
	if (ELEM(NULL, clear_func, default_ksName)) {
		BKE_report(op->reports, RPT_ERROR, "Programming error: missing clear transform function or keying set name");
		return OPERATOR_CANCELLED;
	}
	
	/* only clear relevant transforms for selected bones */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones)
	{
		/* run provided clearing function */
		clear_func(pchan);
		
		/* do auto-keyframing as appropriate */
		if (autokeyframe_cfra_can_key(scene, &ob->id)) {
			/* clear any unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag &= ~BONE_UNKEYED;
				
			/* tag for autokeying later */
			autokey = 1;
		}
		else {
			/* add unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag |= BONE_UNKEYED;
		}
	}
	CTX_DATA_END;
	
	/* perform autokeying on the bones if needed */
	if (autokey) {
		/* get KeyingSet to use */
		KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, default_ksName);
		
		/* insert keyframes */
		ANIM_apply_keyingset(C, NULL, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
		
		/* now recalculate paths */
		if ((ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS))
			ED_pose_recalculate_paths(scene, ob);
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

/* --------------- */

static int pose_clear_scale_exec(bContext *C, wmOperator *op) 
{
	return pose_clear_transform_generic_exec(C, op, pchan_clear_scale, ANIM_KS_SCALING_ID);
}

void POSE_OT_scale_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Pose Scale";
	ot->idname = "POSE_OT_scale_clear";
	ot->description = "Reset scaling of selected bones to their default values";
	
	/* api callbacks */
	ot->exec = pose_clear_scale_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int pose_clear_rot_exec(bContext *C, wmOperator *op) 
{
	return pose_clear_transform_generic_exec(C, op, pchan_clear_rot, ANIM_KS_ROTATION_ID);
}

void POSE_OT_rot_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Pose Rotation";
	ot->idname = "POSE_OT_rot_clear";
	ot->description = "Reset rotations of selected bones to their default values";
	
	/* api callbacks */
	ot->exec = pose_clear_rot_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int pose_clear_loc_exec(bContext *C, wmOperator *op) 
{
	return pose_clear_transform_generic_exec(C, op, pchan_clear_loc, ANIM_KS_LOCATION_ID);
}

void POSE_OT_loc_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Pose Location";
	ot->idname = "POSE_OT_loc_clear";
	ot->description = "Reset locations of selected bones to their default values";
	
	/* api callbacks */
	ot->exec = pose_clear_loc_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int pose_clear_transforms_exec(bContext *C, wmOperator *op) 
{
	return pose_clear_transform_generic_exec(C, op, pchan_clear_transforms, ANIM_KS_LOC_ROT_SCALE_ID);
}

void POSE_OT_transforms_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Pose Transforms";
	ot->idname = "POSE_OT_transforms_clear";
	ot->description = "Reset location, rotation, and scaling of selected bones to their default values";
	
	/* api callbacks */
	ot->exec = pose_clear_transforms_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Clear User Transforms */

static int pose_clear_user_transforms_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	float cframe = (float)CFRA;
	const bool only_select = RNA_boolean_get(op->ptr, "only_selected");
	
	if ((ob->adt) && (ob->adt->action)) {
		/* XXX: this is just like this to avoid contaminating anything else; 
		 * just pose values should change, so this should be fine 
		 */
		bPose *dummyPose = NULL;
		Object workob = {{NULL}};
		bPoseChannel *pchan;
		
		/* execute animation step for current frame using a dummy copy of the pose */
		BKE_pose_copy_data(&dummyPose, ob->pose, 0);
		
		BLI_strncpy(workob.id.name, "OB<ClearTfmWorkOb>", sizeof(workob.id.name));
		workob.type = OB_ARMATURE;
		workob.data = ob->data;
		workob.adt = ob->adt;
		workob.pose = dummyPose;
		
		BKE_animsys_evaluate_animdata(scene, &workob.id, workob.adt, cframe, ADT_RECALC_ANIM);
		
		/* copy back values, but on selected bones only  */
		for (pchan = dummyPose->chanbase.first; pchan; pchan = pchan->next) {
			pose_bone_do_paste(ob, pchan, only_select, 0);
		}
		
		/* free temp data - free manually as was copied without constraints */
		for (pchan = dummyPose->chanbase.first; pchan; pchan = pchan->next) {
			if (pchan->prop) {
				IDP_FreeProperty(pchan->prop);
				MEM_freeN(pchan->prop);
			}
		}
		
		/* was copied without constraints */
		BLI_freelistN(&dummyPose->chanbase);
		MEM_freeN(dummyPose);
	}
	else {
		/* no animation, so just reset whole pose to rest pose 
		 * (cannot just restore for selected though)
		 */
		BKE_pose_rest(ob->pose);
	}
	
	/* notifiers and updates */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_user_transforms_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear User Transforms";
	ot->idname = "POSE_OT_user_transforms_clear";
	ot->description = "Reset pose on selected bones to keyframed state";
	
	/* callbacks */
	ot->exec = pose_clear_user_transforms_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "only_selected", true, "Only Selected", "Only visible/selected bones");
}
