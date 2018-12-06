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
#include "BLI_string_utils.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_appdir.h"
#include "BKE_armature.h"
#include "BKE_blender_copybuffer.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

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
static void applyarmature_fix_boneparents(const bContext *C, Scene *scene, Object *armob)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Main *bmain = CTX_data_main(C);
	Object workob, *ob;

	/* go through all objects in database */
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		/* if parent is bone in this armature, apply corrections */
		if ((ob->parent == armob) && (ob->partype == PARBONE)) {
			/* apply current transform from parent (not yet destroyed),
			 * then calculate new parent inverse matrix
			 */
			BKE_object_apply_mat4(ob, ob->obmat, false, false);

			BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
			invert_m4_m4(ob->parentinv, workob.obmat);
		}
	}
}

/* set the current pose as the restpose */
static int apply_armature_pose2bones_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C)); // must be active object, not edit-object
	const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
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
		const bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
		curbone = ED_armature_ebone_find_name(arm->edbo, pchan->name);

		/* simply copy the head/tail values from pchan over to curbone */
		copy_v3_v3(curbone->head, pchan_eval->pose_head);
		copy_v3_v3(curbone->tail, pchan_eval->pose_tail);

		/* fix roll:
		 * 1. find auto-calculated roll value for this bone now
		 * 2. remove this from the 'visual' y-rotation
		 */
		{
			float premat[3][3], imat[3][3], pmat[3][3], tmat[3][3];
			float delta[3], eul[3];

			/* obtain new auto y-rotation */
			sub_v3_v3v3(delta, curbone->tail, curbone->head);
			vec_roll_to_mat3(delta, 0.0f, premat);
			invert_m3_m3(imat, premat);

			/* get pchan 'visual' matrix */
			copy_m3_m4(pmat, pchan_eval->pose_mat);

			/* remove auto from visual and get euler rotation */
			mul_m3_m3m3(tmat, imat, pmat);
			mat3_to_eul(eul, tmat);

			/* just use this euler-y as new roll value */
			curbone->roll = eul[1];
		}

		/* combine pose and rest values for bendy bone settings,
		 * then clear the pchan values (so we don't get a double-up)
		 */
		if (pchan->bone->segments > 1) {
			/* combine rest/pose values  */
			curbone->curveInX += pchan_eval->curveInX;
			curbone->curveInY += pchan_eval->curveInY;
			curbone->curveOutX += pchan_eval->curveOutX;
			curbone->curveOutY += pchan_eval->curveOutY;
			curbone->roll1 += pchan_eval->roll1;
			curbone->roll2 += pchan_eval->roll2;
			curbone->ease1 += pchan_eval->ease1;
			curbone->ease2 += pchan_eval->ease2;
			curbone->scaleIn += pchan_eval->scaleIn;
			curbone->scaleOut += pchan_eval->scaleOut;

			/* reset pose values */
			pchan->curveInX = pchan->curveOutX = 0.0f;
			pchan->curveInY = pchan->curveOutY = 0.0f;
			pchan->roll1 = pchan->roll2 = 0.0f;
			pchan->ease1 = pchan->ease2 = 0.0f;
			pchan->scaleIn = pchan->scaleOut = 1.0f;
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
	ED_armature_from_edit(bmain, arm);
	ED_armature_edit_free(arm);

	/* flush positions of posebones */
	BKE_pose_where_is(depsgraph, scene, ob);

	/* fix parenting of objects which are bone-parented */
	applyarmature_fix_boneparents(C, scene, ob);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

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
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);

	FOREACH_OBJECT_IN_MODE_BEGIN(view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob)
	{
		/* loop over all selected pchans
		 *
		 * TODO, loop over children before parents if multiple bones
		 * at once are to be predictable*/
		FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob, pchan)
		{
			const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
			bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
			float delta_mat[4][4];

			/* chan_mat already contains the delta transform from rest pose to pose-mode pose
			 * as that is baked into there so that B-Bones will work. Once we've set this as the
			 * new raw-transform components, don't recalc the poses yet, otherwise IK result will
			 * change, thus changing the result we may be trying to record.
			 */
			/* XXX For some reason, we can't use pchan->chan_mat here, gives odd rotation/offset (see T38251).
			 *     Using pchan->pose_mat and bringing it back in bone space seems to work as expected!
			 */
			BKE_armature_mat_pose_to_bone(pchan_eval, pchan_eval->pose_mat, delta_mat);

			BKE_pchan_apply_mat4(pchan, delta_mat, true);
		}
		FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

		DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

		/* note, notifier might evolve */
		WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	}
	FOREACH_OBJECT_IN_MODE_END;

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
		BLI_string_flip_side_name(name, chan->name, false, sizeof(name));
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
		paste_ok = (pchan != NULL);

	/* continue? */
	if (paste_ok) {
		/* only loc rot size
		 * - only copies transform info for the pose
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

		/* B-Bone posing options should also be included... */
		pchan->curveInX = chan->curveInX;
		pchan->curveInY = chan->curveInY;
		pchan->curveOutX = chan->curveOutX;
		pchan->curveOutY = chan->curveOutY;

		pchan->roll1 = chan->roll1;
		pchan->roll2 = chan->roll2;
		pchan->ease1 = chan->ease1;
		pchan->ease2 = chan->ease2;
		pchan->scaleIn = chan->scaleIn;
		pchan->scaleOut = chan->scaleOut;

		/* paste flipped pose? */
		if (flip) {
			pchan->loc[0] *= -1;

			pchan->curveInX *= -1;
			pchan->curveOutX *= -1;
			pchan->roll1 *= -1; // XXX?
			pchan->roll2 *= -1; // XXX?

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
	char str[FILE_MAX];
	/* Sanity checking. */
	if (ELEM(NULL, ob, ob->pose)) {
		BKE_report(op->reports, RPT_ERROR, "No pose to copy");
		return OPERATOR_CANCELLED;
	}
	/* Sets chan->flag to POSE_KEY if bone selected. */
	set_pose_keys(ob);
	/* Construct a local bmain and only put object and it's data into it,
	 * o this way we don't expand any other objects into the copy buffer
	 * file.
	 *
	 * TODO(sergey): Find an easier way to tell copy buffer to only store
	 * data we are actually interested in. Maybe pass it a flag to skip
	 * any datablock expansion?
	 */
	Main *temp_bmain = BKE_main_new();
	Object ob_copy = *ob;
	bArmature arm_copy = *((bArmature *)ob->data);
	ob_copy.data = &arm_copy;
	BLI_addtail(&temp_bmain->object, &ob_copy);
	BLI_addtail(&temp_bmain->armature, &arm_copy);
	/* begin copy buffer on a temp bmain. */
	BKE_copybuffer_begin(temp_bmain);
	/* Store the whole object to the copy buffer because pose can't be
	 * existing on it's own.
	 */
	BKE_copybuffer_tag_ID(&ob_copy.id);
	BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer_pose.blend");
	BKE_copybuffer_save(temp_bmain, str, op->reports);
	/* We clear the lists so no datablocks gets freed,
	 * This is required because objects in temp bmain shares same pointers
	 * as the real ones.
	 */
	BLI_listbase_clear(&temp_bmain->object);
	BLI_listbase_clear(&temp_bmain->armature);
	BKE_main_free(temp_bmain);
	/* We are all done! */
	BKE_report(op->reports, RPT_INFO, "Copied pose to buffer");
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

	/* Get KeyingSet to use. */
	KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_WHOLE_CHARACTER_ID);

	/* Sanity checks. */
	if (ELEM(NULL, ob, ob->pose)) {
		return OPERATOR_CANCELLED;
	}

	/* Read copy buffer .blend file. */
	char str[FILE_MAX];
	Main *tmp_bmain = BKE_main_new();
	BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer_pose.blend");
	if (!BKE_copybuffer_read(tmp_bmain, str, op->reports)) {
		BKE_report(op->reports, RPT_ERROR, "Copy buffer is empty");
		BKE_main_free(tmp_bmain);
		return OPERATOR_CANCELLED;
	}
	/* Make sure data from this file is usable for pose paste. */
	if (BLI_listbase_count_at_most(&tmp_bmain->object, 2) != 1) {
		BKE_report(op->reports, RPT_ERROR, "Copy buffer is not from pose mode");
		BKE_main_free(tmp_bmain);
		return OPERATOR_CANCELLED;
	}

	Object *object_from = tmp_bmain->object.first;
	bPose *pose_from = object_from->pose;
	if (pose_from == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Copy buffer has no pose");
		BKE_main_free(tmp_bmain);
		return OPERATOR_CANCELLED;
	}

	/* If selOnly option is enabled, if user hasn't selected any bones,
	 * just go back to default behavior to be more in line with other
	 * pose tools.
	 */
	if (selOnly) {
		if (CTX_DATA_COUNT(C, selected_pose_bones) == 0) {
			selOnly = false;
		}
	}

	/* Safely merge all of the channels in the buffer pose into any
	 * existing pose.
	 */
	for (chan = pose_from->chanbase.first; chan; chan = chan->next) {
		if (chan->flag & POSE_KEY) {
			/* Try to perform paste on this bone. */
			bPoseChannel *pchan = pose_bone_do_paste(ob, chan, selOnly, flip);
			if (pchan != NULL) {
				/* Keyframing tagging for successful paste, */
				ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
			}
		}
	}
	BKE_main_free(tmp_bmain);

	/* Update event for pose and deformation children. */
	DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

	/* Recalculate paths if any of the bones have paths... */
	if ((ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS)) {
		ED_pose_recalculate_paths(C, scene, ob, false);
	}

	/* Notifiers for updates, */
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

	pchan->ease1 = 0.0f;
	pchan->ease2 = 0.0f;
	pchan->scaleIn = 1.0f;
	pchan->scaleOut = 1.0f;
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

	/* Clear also Bendy Bone stuff - Roll is obvious, but Curve X/Y stuff is also kindof rotational in nature... */
	pchan->roll1 = 0.0f;
	pchan->roll2 = 0.0f;

	pchan->curveInX = 0.0f;
	pchan->curveInY = 0.0f;
	pchan->curveOutX = 0.0f;
	pchan->curveOutY = 0.0f;
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
	bool changed_multi = false;

	/* sanity checks */
	if (ELEM(NULL, clear_func, default_ksName)) {
		BKE_report(op->reports, RPT_ERROR, "Programming error: missing clear transform function or keying set name");
		return OPERATOR_CANCELLED;
	}

	/* only clear relevant transforms for selected bones */
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter)
	{
		Object *ob_eval = DEG_get_evaluated_object(CTX_data_depsgraph(C), ob_iter); // XXX: UGLY HACK (for autokey + clear transforms)
		ListBase dsources = {NULL, NULL};
		bool changed = false;

		FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob_iter, pchan)
		{
			/* run provided clearing function */
			clear_func(pchan);
			changed = true;

			/* do auto-keyframing as appropriate */
			if (autokeyframe_cfra_can_key(scene, &ob_iter->id)) {
				/* clear any unkeyed tags */
				if (pchan->bone) {
					pchan->bone->flag &= ~BONE_UNKEYED;
				}
				/* tag for autokeying later */
				ANIM_relative_keyingset_add_source(&dsources, &ob_iter->id, &RNA_PoseBone, pchan);

#if 1			/* XXX: Ugly Hack - Run clearing function on evaluated copy of pchan */
				bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
				clear_func(pchan_eval);
#endif
			}
			else {
				/* add unkeyed tags */
				if (pchan->bone) {
					pchan->bone->flag |= BONE_UNKEYED;
				}
			}
		}
		FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

		if (changed) {
			changed_multi = true;

			/* perform autokeying on the bones if needed */
			if (!BLI_listbase_is_empty(&dsources)) {
				/* get KeyingSet to use */
				KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, default_ksName);

				/* insert keyframes */
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);

				/* now recalculate paths */
				if ((ob_iter->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS)) {
					ED_pose_recalculate_paths(C, scene, ob_iter, false);
				}

				BLI_freelistN(&dsources);
			}

			DEG_id_tag_update(&ob_iter->id, ID_RECALC_GEOMETRY);

			/* note, notifier might evolve */
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob_iter);
		}
	}
	FOREACH_OBJECT_IN_MODE_END;

	return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
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
	ViewLayer *view_layer = CTX_data_view_layer(C);
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	float cframe = (float)CFRA;
	const bool only_select = RNA_boolean_get(op->ptr, "only_selected");

	FOREACH_OBJECT_IN_MODE_BEGIN (view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob)
	{
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

			BKE_animsys_evaluate_animdata(NULL, scene, &workob.id, workob.adt, cframe, ADT_RECALC_ANIM);

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
		DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
		WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	}
	FOREACH_OBJECT_IN_MODE_END;

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
