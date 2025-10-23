/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 */

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_appdir.hh"
#include "BKE_armature.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_keyframing.hh"
#include "ED_screen.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "armature_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void pose_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer_pose.blend");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Pose as Rest Pose
 * \{ */

/* helper for apply_armature_pose2bones - fixes parenting of objects
 * that are bone-parented to armature */
static void applyarmature_fix_boneparents(const bContext *C, Scene *scene, Object *armob)
{
  /* Depsgraph has been ensured to be evaluated at the beginning of the operator.
   *
   * Must not evaluate depsgraph here yet, since this will ruin object matrix which we want to
   * preserve after other changes has been done in the operator.
   *
   * TODO(sergey): This seems very similar to `ignore_parent_tx()`, which was now ensured to work
   * quite reliably. Can we de-duplicate the code? Or at least verify we don't need an extra logic
   * in this function. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Main *bmain = CTX_data_main(C);

  /* go through all objects in database */
  for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    /* if parent is bone in this armature, apply corrections */
    if ((ob->parent == armob) && (ob->partype == PARBONE)) {
      /* apply current transform from parent (not yet destroyed),
       * then calculate new parent inverse matrix
       */
      BKE_object_apply_mat4(ob, ob->object_to_world().ptr(), false, false);

      invert_m4_m4(ob->parentinv, BKE_object_calc_parent(depsgraph, scene, ob).ptr());
    }
  }
}

/* Sets the bone head, tail and roll to match the supplied parameters. */
static void applyarmature_set_edit_position(EditBone *curbone,
                                            const float pose_mat[4][4],
                                            const float new_tail[3],
                                            float r_new_arm_mat[4][4])
{
  /* Simply copy the head/tail values from pchan over to curbone. */
  copy_v3_v3(curbone->head, pose_mat[3]);
  copy_v3_v3(curbone->tail, new_tail);

  /* Fix roll:
   * 1. find auto-calculated roll value for this bone now
   * 2. remove this from the 'visual' y-rotation
   */
  {
    float premat[3][3], pmat[3][3];
    float delta[3];

    /* Obtain new auto y-rotation. */
    sub_v3_v3v3(delta, curbone->tail, curbone->head);

    copy_m3_m4(pmat, pose_mat);
    mat3_vec_to_roll(pmat, delta, &curbone->roll);

    /* Compute new rest pose matrix if requested. */
    if (r_new_arm_mat) {
      vec_roll_to_mat3(delta, curbone->roll, premat);
      copy_m4_m3(r_new_arm_mat, premat);
      copy_v3_v3(r_new_arm_mat[3], pose_mat[3]);
    }
  }
}

/* Copy properties over from pchan to curbone and reset channels. */
static void applyarmature_transfer_properties(EditBone *curbone,
                                              bPoseChannel *pchan,
                                              const bPoseChannel *pchan_eval)
{
  /* Combine pose and rest values for bendy bone settings,
   * then clear the pchan values (so we don't get a double-up).
   */
  if (pchan->bone->segments > 1) {
    /* Combine rest/pose values. */
    curbone->curve_in_x += pchan_eval->curve_in_x;
    curbone->curve_in_z += pchan_eval->curve_in_z;
    curbone->curve_out_x += pchan_eval->curve_out_x;
    curbone->curve_out_z += pchan_eval->curve_out_z;
    curbone->roll1 += pchan_eval->roll1;
    curbone->roll2 += pchan_eval->roll2;
    curbone->ease1 += pchan_eval->ease1;
    curbone->ease2 += pchan_eval->ease2;

    mul_v3_v3(curbone->scale_in, pchan_eval->scale_in);
    mul_v3_v3(curbone->scale_out, pchan_eval->scale_out);

    /* Reset pose values. */
    pchan->curve_in_x = pchan->curve_out_x = 0.0f;
    pchan->curve_in_z = pchan->curve_out_z = 0.0f;
    pchan->roll1 = pchan->roll2 = 0.0f;
    pchan->ease1 = pchan->ease2 = 0.0f;

    copy_v3_fl(pchan->scale_in, 1.0f);
    copy_v3_fl(pchan->scale_out, 1.0f);
  }

  /* Clear transform values for pchan. */
  zero_v3(pchan->loc);
  zero_v3(pchan->eul);
  unit_qt(pchan->quat);
  unit_axis_angle(pchan->rotAxis, &pchan->rotAngle);
  pchan->scale[0] = pchan->scale[1] = pchan->scale[2] = 1.0f;
}

/* Adjust the current edit position of the bone using the pose space matrix. */
static void applyarmature_adjust_edit_position(bArmature *arm,
                                               bPoseChannel *pchan,
                                               const float delta_mat[4][4],
                                               float r_new_arm_mat[4][4])
{
  EditBone *curbone = ED_armature_ebone_find_name(arm->edbo, pchan->name);
  float delta[3], new_tail[3], premat[3][3], new_pose[4][4];

  /* Current orientation matrix. */
  sub_v3_v3v3(delta, curbone->tail, curbone->head);
  vec_roll_to_mat3(delta, curbone->roll, premat);

  /* New location and orientation. */
  mul_m4_m4m3(new_pose, delta_mat, premat);
  mul_v3_m4v3(new_pose[3], delta_mat, curbone->head);
  mul_v3_m4v3(new_tail, delta_mat, curbone->tail);

  applyarmature_set_edit_position(curbone, new_pose, new_tail, r_new_arm_mat);
}

/* Data about parent position for Apply To Selected mode. */
struct ApplyArmature_ParentState {
  Bone *bone;

  /* New rest position of the bone with scale included. */
  float new_rest_mat[4][4];
  /* New arm_mat of the bone == new_rest_mat without scale. */
  float new_arm_mat[4][4];
};

/* Recursive walk for Apply To Selected mode; pstate nullptr unless child of an applied bone. */
static void applyarmature_process_selected_recursive(bArmature *arm,
                                                     bPose *pose,
                                                     bPose *pose_eval,
                                                     Bone *bone,
                                                     blender::Span<PointerRNA> selected,
                                                     ApplyArmature_ParentState *pstate)
{
  bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone->name);
  const bPoseChannel *pchan_eval = BKE_pose_channel_find_name(pose_eval, bone->name);

  if (!pchan || !pchan_eval) {
    return;
  }

  ApplyArmature_ParentState new_pstate{};
  new_pstate.bone = bone;

  if (std::find_if(selected.begin(), selected.end(), [&](const PointerRNA &ptr) {
        return ptr.data == pchan;
      }) != selected.end())
  {
    /* SELECTED BONE: Snap to final pose transform minus un-applied parent effects.
     *
     * I.e. bone position with accumulated parent effects but no local
     * transformation will match the original final pose_mat.
     *
     * Pose channels are reset as expected.
     */
    EditBone *curbone = ED_armature_ebone_find_name(arm->edbo, pchan->name);
    BoneParentTransform invparent;
    float new_tail[3];

    if (pchan->parent) {
      BoneParentTransform old_bpt, new_bpt;
      float offs_bone[4][4];

      /* Parent effects on the bone transform that have to be removed. */
      BKE_bone_offset_matrix_get(bone, offs_bone);
      BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                   bone->inherit_scale_mode,
                                                   offs_bone,
                                                   bone->parent->arm_mat,
                                                   pchan_eval->parent->pose_mat,
                                                   &old_bpt);

      /* Applied parent effects that have to be kept, if any. */
      float (*new_parent_pose)[4] = pstate ? pstate->new_rest_mat : bone->parent->arm_mat;
      BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                   bone->inherit_scale_mode,
                                                   offs_bone,
                                                   bone->parent->arm_mat,
                                                   new_parent_pose,
                                                   &new_bpt);

      BKE_bone_parent_transform_invert(&old_bpt);
      BKE_bone_parent_transform_combine(&new_bpt, &old_bpt, &invparent);
    }
    else {
      BKE_bone_parent_transform_clear(&invparent);
    }

    /* Apply change without inherited unapplied parent transformations. */
    BKE_bone_parent_transform_apply(&invparent, pchan_eval->pose_mat, new_pstate.new_rest_mat);

    copy_v3_fl3(new_tail, 0.0, bone->length, 0.0);
    mul_m4_v3(new_pstate.new_rest_mat, new_tail);

    applyarmature_set_edit_position(
        curbone, new_pstate.new_rest_mat, new_tail, new_pstate.new_arm_mat);
    applyarmature_transfer_properties(curbone, pchan, pchan_eval);

    pstate = &new_pstate;
  }
  else if (pstate) {
    /* UNSELECTED CHILD OF SELECTED: Include applied parent effects.
     *
     * The inherited transform of applied (selected) bones is baked
     * into the rest pose so that the final bone position doesn't
     * change.
     *
     * Pose channels are not changed, with the exception of the inherited
     * applied parent scale being baked into the location pose channel.
     */
    BoneParentTransform bpt;
    float offs_bone[4][4], delta[4][4], old_chan_loc[3];

    /* Include applied parent effects. */
    BKE_bone_offset_matrix_get(bone, offs_bone);
    BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                 bone->inherit_scale_mode,
                                                 offs_bone,
                                                 pstate->bone->arm_mat,
                                                 pstate->new_rest_mat,
                                                 &bpt);

    unit_m4(new_pstate.new_rest_mat);
    BKE_bone_parent_transform_apply(&bpt, new_pstate.new_rest_mat, new_pstate.new_rest_mat);

    /* Bone location channel in pose space relative to bone head. */
    mul_v3_mat3_m4v3(old_chan_loc, bpt.loc_mat, pchan_eval->loc);

    /* Apply the change to the rest bone position. */
    invert_m4_m4(delta, bone->arm_mat);
    mul_m4_m4m4(delta, new_pstate.new_rest_mat, delta);

    applyarmature_adjust_edit_position(arm, pchan, delta, new_pstate.new_arm_mat);

    /* Location pose channel has to be updated, because it is affected
     * by parent scaling, and the rest pose has no scale by definition. */
    if (!(bone->flag & BONE_CONNECTED) && !is_zero_v3(old_chan_loc)) {
      float inv_parent_arm[4][4];

      /* Compute the channel coordinate space matrices for the new rest state. */
      invert_m4_m4(inv_parent_arm, pstate->new_arm_mat);
      mul_m4_m4m4(offs_bone, inv_parent_arm, new_pstate.new_arm_mat);
      BKE_bone_parent_transform_calc_from_matrices(bone->flag,
                                                   bone->inherit_scale_mode,
                                                   offs_bone,
                                                   pstate->new_arm_mat,
                                                   pstate->new_arm_mat,
                                                   &bpt);

      /* Re-apply the location to keep the final effect. */
      invert_m4(bpt.loc_mat);
      mul_v3_mat3_m4v3(pchan->loc, bpt.loc_mat, old_chan_loc);
    }

    pstate = &new_pstate;
  }

  LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
    applyarmature_process_selected_recursive(arm, pose, pose_eval, child, selected, pstate);
  }
}

/* Reset bone constraint so that it is correct after the pose has been applied. */
static void applyarmature_reset_bone_constraint(const bConstraint *constraint)
{
  /* TODO(Sybren): This function needs too much knowledge of the internals of specific constraints.
   * When it is extended with one or two more constraints, move the functionality into a
   * bConstraintTypeInfo callback function. */
  switch (constraint->type) {
    case CONSTRAINT_TYPE_STRETCHTO: {
      bStretchToConstraint *stretch_to = static_cast<bStretchToConstraint *>(constraint->data);
      stretch_to->orglength = 0.0f; /* Force recalculation on next evaluation. */
      break;
    }
    default:
      /* Most constraints don't need resetting. */
      break;
  }
}

/* Reset bone constraints of the given pose channel so that they are correct after the pose has
 * been applied. */
static void applyarmature_reset_bone_constraints(const bPoseChannel *pchan)
{
  LISTBASE_FOREACH (bConstraint *, constraint, &pchan->constraints) {
    applyarmature_reset_bone_constraint(constraint);
  }
}

/* Reset all (or only selected) bone constraints so that they are correct after the pose has been
 * applied. */
static void applyarmature_reset_constraints(bPose *pose, const bool use_selected)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    BLI_assert(pchan->bone != nullptr);
    if (use_selected && (pchan->flag & POSE_SELECTED) == 0) {
      continue;
    }
    applyarmature_reset_bone_constraints(pchan);
  }
}

/* Set the current pose as the rest-pose. */
static wmOperatorStatus apply_armature_pose2bones_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  /* must be active object, not edit-object */
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  bArmature *arm = BKE_armature_from_object(ob);
  bPose *pose;
  blender::Vector<PointerRNA> selected_bones;

  const bool use_selected = RNA_boolean_get(op->ptr, "selected");

  /* don't check if editmode (should be done by caller) */
  if (ob->type != OB_ARMATURE) {
    return OPERATOR_CANCELLED;
  }
  if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot apply pose to lib-linked armature");
    return OPERATOR_CANCELLED;
  }

  /* helpful warnings... */
  /* TODO: add warnings to be careful about actions, applying deforms first, etc. */
  if (ob->adt && ob->adt->action) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Actions on this armature will be destroyed by this new rest pose as the "
               "transforms stored are relative to the old rest pose");
  }

  /* Find selected bones before switching to edit mode. */
  if (use_selected) {
    CTX_data_selected_pose_bones(C, &selected_bones);

    if (selected_bones.is_empty()) {
      return OPERATOR_CANCELLED;
    }
  }

  /* Get edit-bones of active armature to alter. */
  ED_armature_to_edit(arm);

  /* Get pose of active object and move it out of pose-mode. */
  pose = ob->pose;

  if (use_selected) {
    /* The selected only mode requires a recursive walk to handle parent-child relations. */
    LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
      applyarmature_process_selected_recursive(
          arm, pose, ob_eval->pose, bone, selected_bones, nullptr);
    }
  }
  else {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
      const bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
      EditBone *curbone = ED_armature_ebone_find_name(arm->edbo, pchan->name);

      applyarmature_set_edit_position(
          curbone, pchan_eval->pose_mat, pchan_eval->pose_tail, nullptr);
      applyarmature_transfer_properties(curbone, pchan, pchan_eval);
    }
  }

  /* Convert edit-bones back to bones, and then free the edit-data. */
  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  /* Flush positions of pose-bones. */
  BKE_pose_where_is(depsgraph, scene, ob);

  /* fix parenting of objects which are bone-parented */
  applyarmature_fix_boneparents(C, scene, ob);

  /* For the affected bones, reset specific constraints that are now known to be invalid. */
  applyarmature_reset_constraints(pose, use_selected);

  /* NOTE: notifier might evolve. */
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);

  return OPERATOR_FINISHED;
}

static void apply_armature_pose2bones_ui(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  wmWindowManager *wm = CTX_wm_manager(C);

  PointerRNA ptr = RNA_pointer_create_discrete(&wm->id, op->type->srna, op->properties);

  layout->prop(&ptr, "selected", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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
  ot->ui = apply_armature_pose2bones_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "selected",
                  false,
                  "Selected Only",
                  "Only apply the selected bones (with propagation to children)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Visual Transform Operator
 *
 * Set the current pose as the rest-pose.
 * \{ */

static wmOperatorStatus pose_visual_transform_apply_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  /* Needed to ensure #bPoseChannel.pose_mat are up to date. */
  CTX_data_ensure_evaluated_depsgraph(C);

  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    const bArmature *arm = static_cast<const bArmature *>(ob->data);

    int chanbase_len = BLI_listbase_count(&ob->pose->chanbase);
    /* Storage for the calculated matrices to prevent reading from modified values.
     * NOTE: this could be avoided if children were always calculated before parents
     * however ensuring this is involved and doesn't give any significant advantage. */
    struct XFormArray {
      float matrix[4][4];
      bool is_set;
    } *pchan_xform_array = MEM_malloc_arrayN<XFormArray>(chanbase_len, __func__);
    bool changed = false;

    int i;
    LISTBASE_FOREACH_INDEX (bPoseChannel *, pchan, &ob->pose->chanbase, i) {
      if (!blender::animrig::bone_is_selected(arm, pchan)) {
        pchan_xform_array[i].is_set = false;
        continue;
      }

      /* `chan_mat` already contains the delta transform from rest pose to pose-mode pose
       * as that is baked into there so that B-Bones will work. Once we've set this as the
       * new raw-transform components, don't recalculate the poses yet, otherwise IK result will
       * change, thus changing the result we may be trying to record. */

      /* NOTE: For some reason `pchan->chan_mat` can't be used here as it gives odd
       * rotation/offset, see #38251.
       * Using `pchan->pose_mat` and bringing it back in bone space seems to work as expected!
       * This matches how visual key-framing works. */
      BKE_armature_mat_pose_to_bone(pchan, pchan->pose_mat, pchan_xform_array[i].matrix);
      pchan_xform_array[i].is_set = true;
      changed = true;
    }

    if (changed) {
      /* Perform separately to prevent feedback loop. */
      LISTBASE_FOREACH_INDEX (bPoseChannel *, pchan, &ob->pose->chanbase, i) {
        if (!pchan_xform_array[i].is_set) {
          continue;
        }
        BKE_pchan_apply_mat4(pchan, pchan_xform_array[i].matrix, true);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
    }

    MEM_freeN(pchan_xform_array);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy/Paste Utilities
 * \{ */

/**
 * Perform paste pose, for a single bone.
 *
 * \param ob: Object where bone to paste to lives
 * \param chan: Bone that pose to paste comes from
 * \param selOnly: Only paste on selected bones
 * \param flip: Flip on x-axis
 * \return The channel of the bone that was pasted to, or nullptr if no paste was performed.
 */
static bPoseChannel *pose_bone_do_paste(Object *ob,
                                        bPoseChannel *chan,
                                        const bool selOnly,
                                        const bool flip)
{
  char name[MAXBONENAME];

  /* get the name - if flipping, we must flip this first */
  if (flip) {
    BLI_string_flip_side_name(name, chan->name, false, sizeof(name));
  }
  else {
    STRNCPY_UTF8(name, chan->name);
  }

  /* only copy when:
   *  1) channel exists - poses are not meant to add random channels to anymore
   *  2) if selection-masking is on, channel is selected -
   *     only selected bones get pasted on, allowing making both sides symmetrical.
   */
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, name);
  if (pchan == nullptr) {
    return nullptr;
  }
  if (selOnly && (pchan->flag & POSE_SELECTED) == 0) {
    return nullptr;
  }

  /* only loc rot size
   * - only copies transform info for the pose
   */
  copy_v3_v3(pchan->loc, chan->loc);
  copy_v3_v3(pchan->scale, chan->scale);

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
    if (chan->rotmode == ROT_MODE_AXISANGLE) {
      axis_angle_to_eulO(pchan->eul, pchan->rotmode, chan->rotAxis, chan->rotAngle);
    }
    else {
      quat_to_eulO(pchan->eul, pchan->rotmode, chan->quat);
    }
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    /* quat/euler to axis angle */
    if (chan->rotmode > 0) {
      eulO_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->eul, chan->rotmode);
    }
    else {
      quat_to_axis_angle(pchan->rotAxis, &pchan->rotAngle, chan->quat);
    }
  }
  else {
    /* euler/axis-angle to quat */
    if (chan->rotmode > 0) {
      eulO_to_quat(pchan->quat, chan->eul, chan->rotmode);
    }
    else {
      axis_angle_to_quat(pchan->quat, chan->rotAxis, pchan->rotAngle);
    }
  }

  /* B-Bone posing options should also be included... */
  pchan->curve_in_x = chan->curve_in_x;
  pchan->curve_in_z = chan->curve_in_z;
  pchan->curve_out_x = chan->curve_out_x;
  pchan->curve_out_z = chan->curve_out_z;

  pchan->roll1 = chan->roll1;
  pchan->roll2 = chan->roll2;
  pchan->ease1 = chan->ease1;
  pchan->ease2 = chan->ease2;

  copy_v3_v3(pchan->scale_in, chan->scale_in);
  copy_v3_v3(pchan->scale_out, chan->scale_out);

  /* paste flipped pose? */
  if (flip) {
    pchan->loc[0] *= -1;

    pchan->curve_in_x *= -1;
    pchan->curve_out_x *= -1;
    pchan->roll1 *= -1; /* XXX? */
    pchan->roll2 *= -1; /* XXX? */

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
  if (chan->system_properties) {
    /* Same logic as above for system IDProperties, for now. */
    if (pchan->system_properties) {
      IDP_SyncGroupValues(pchan->system_properties, chan->system_properties);
    }
    else {
      pchan->system_properties = IDP_CopyProperty(chan->system_properties);
    }
  }

  return pchan;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Pose Operator
 * \{ */

static wmOperatorStatus pose_copy_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::blendfile;

  Main *bmain = CTX_data_main(C);
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));

  /* Sanity checking. */
  if (ELEM(nullptr, ob, ob->pose)) {
    BKE_report(op->reports, RPT_ERROR, "No pose to copy");
    return OPERATOR_CANCELLED;
  }
  if (ID_IS_PACKED(&ob->id)) {
    /* Direct link/append of packed IDs is not supported currently, so neither is their
     * copy/pasting. */
    BKE_report(op->reports, RPT_ERROR, "Cannot copy/paste packed data");
    return OPERATOR_CANCELLED;
  }

  bArmature *armature = static_cast<bArmature *>(ob->data);
  BLI_assert_msg(armature, "If an armature object has a pose, it should have armature data");
  /* Taking off the selection flag in case bones are hidden so they are not
   * applied when pasting.  */
  LISTBASE_FOREACH (bPoseChannel *, pose_bone, &ob->pose->chanbase) {
    if (!blender::animrig::bone_is_visible(armature, pose_bone)) {
      pose_bone->flag &= ~POSE_SELECTED;
    }
  }

  PartialWriteContext copybuffer{*bmain};
  copybuffer.id_add(
      &ob->id,
      PartialWriteContext::IDAddOptions{
          (PartialWriteContext::IDAddOperations::MAKE_LOCAL |
           PartialWriteContext::IDAddOperations::SET_FAKE_USER |
           PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK)},
      [ob](LibraryIDLinkCallbackData *cb_data,
           PartialWriteContext::IDAddOptions /*options*/) -> PartialWriteContext::IDAddOperations {
        /* Only include `ob->data` (i.e. the Armature) dependency. */
        if (*(cb_data->id_pointer) == ob->data) {
          return (PartialWriteContext::IDAddOperations::MAKE_LOCAL |
                  PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES);
        }
        return PartialWriteContext::IDAddOperations::CLEAR_DEPENDENCIES;
      });

  char filepath[FILE_MAX];
  pose_copybuffer_filepath_get(filepath, sizeof(filepath));
  copybuffer.write(filepath, *op->reports);

  /* We are all done! */
  BKE_report(op->reports, RPT_INFO, "Copied pose to internal clipboard");
  return OPERATOR_FINISHED;
}

void POSE_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Pose";
  ot->idname = "POSE_OT_copy";
  ot->description = "Copy the current pose of the selected bones to the internal clipboard";

  /* API callbacks. */
  ot->exec = pose_copy_exec;
  ot->poll = ED_operator_posemode;

  /* flag */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Pose Operator
 * \{ */

static wmOperatorStatus pose_paste_exec(bContext *C, wmOperator *op)
{
  Object *ob = BKE_object_pose_armature_get(CTX_data_active_object(C));
  Scene *scene = CTX_data_scene(C);
  const bool flip = RNA_boolean_get(op->ptr, "flipped");
  bool selOnly = RNA_boolean_get(op->ptr, "selected_mask");

  /* Get KeyingSet to use. */
  KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene,
                                                                 ANIM_KS_WHOLE_CHARACTER_ID);

  /* Sanity checks. */
  if (ELEM(nullptr, ob, ob->pose)) {
    return OPERATOR_CANCELLED;
  }

  /* Read copy buffer .blend file. */
  char filepath[FILE_MAX];
  Main *temp_bmain = BKE_main_new();
  STRNCPY(temp_bmain->filepath, BKE_main_blendfile_path_from_global());

  pose_copybuffer_filepath_get(filepath, sizeof(filepath));
  if (!BKE_copybuffer_read(temp_bmain, filepath, op->reports, FILTER_ID_OB)) {
    BKE_report(op->reports, RPT_ERROR, "Internal clipboard is empty");
    BKE_main_free(temp_bmain);
    return OPERATOR_CANCELLED;
  }
  /* Make sure data from this file is usable for pose paste. */
  if (!BLI_listbase_is_single(&temp_bmain->objects)) {
    BKE_report(op->reports, RPT_ERROR, "Internal clipboard is not from pose mode");
    BKE_main_free(temp_bmain);
    return OPERATOR_CANCELLED;
  }

  Object *object_from = static_cast<Object *>(temp_bmain->objects.first);
  bPose *pose_from = object_from->pose;
  if (pose_from == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Internal clipboard has no pose");
    BKE_main_free(temp_bmain);
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
  LISTBASE_FOREACH (bPoseChannel *, chan, &pose_from->chanbase) {
    if (chan->flag & POSE_SELECTED) {
      /* Try to perform paste on this bone. */
      bPoseChannel *pchan = pose_bone_do_paste(ob, chan, selOnly, flip);
      if (pchan != nullptr) {
        /* Keyframing tagging for successful paste, */
        blender::animrig::autokeyframe_pchan(C, scene, ob, pchan, ks);
      }
    }
  }
  BKE_main_free(temp_bmain);

  /* Update event for pose and deformation children. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  /* Recalculate paths if any of the bones have paths... */
  if (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
    ED_pose_recalculate_paths(C, scene, ob, POSE_PATH_CALC_RANGE_FULL);
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

  /* API callbacks. */
  ot->exec = pose_paste_exec;
  ot->poll = ED_operator_posemode;

  /* flag */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "flipped",
                         false,
                         "Flipped on X-Axis",
                         "Paste the stored pose flipped on to current pose");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_boolean(ot->srna,
                  "selected_mask",
                  false,
                  "On Selected Only",
                  "Only paste the stored pose on to selected bones in the current pose");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Pose Transforms Utilities
 * \{ */

/* clear scale of pose-channel */
static void pchan_clear_scale(bPoseChannel *pchan)
{
  if ((pchan->protectflag & OB_LOCK_SCALEX) == 0) {
    pchan->scale[0] = 1.0f;
  }
  if ((pchan->protectflag & OB_LOCK_SCALEY) == 0) {
    pchan->scale[1] = 1.0f;
  }
  if ((pchan->protectflag & OB_LOCK_SCALEZ) == 0) {
    pchan->scale[2] = 1.0f;
  }

  pchan->ease1 = 0.0f;
  pchan->ease2 = 0.0f;

  copy_v3_fl(pchan->scale_in, 1.0f);
  copy_v3_fl(pchan->scale_out, 1.0f);
}
/* Clear the scale. When X-mirror is enabled,
 * also clear the scale of the mirrored pose channel. */
static void pchan_clear_scale_with_mirrored(const bPose *pose, bPoseChannel *pchan)
{
  if (pose->flag & POSE_MIRROR_EDIT) {
    bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(pose, pchan->name);
    if (pchan_mirror != nullptr) {
      pchan_clear_scale(pchan_mirror);
    }
  }
  pchan_clear_scale(pchan);
}

/* clear location of pose-channel */
static void pchan_clear_loc(bPoseChannel *pchan)
{
  if ((pchan->protectflag & OB_LOCK_LOCX) == 0) {
    pchan->loc[0] = 0.0f;
  }
  if ((pchan->protectflag & OB_LOCK_LOCY) == 0) {
    pchan->loc[1] = 0.0f;
  }
  if ((pchan->protectflag & OB_LOCK_LOCZ) == 0) {
    pchan->loc[2] = 0.0f;
  }
}
/* Clear the Location. When X-mirror is enabled,
 * also clear the location of the mirrored pose channel. */
static void pchan_clear_loc_with_mirrored(const bPose *pose, bPoseChannel *pchan)
{
  if (pose->flag & POSE_MIRROR_EDIT) {
    bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(pose, pchan->name);
    if (pchan_mirror != nullptr) {
      pchan_clear_loc(pchan_mirror);
    }
  }
  pchan_clear_loc(pchan);
}

/* clear rotation of pose-channel */
static void pchan_clear_rot(bPoseChannel *pchan)
{
  if (pchan->protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) {
    /* check if convert to eulers for locking... */
    if (pchan->protectflag & OB_LOCK_ROT4D) {
      /* perform clamping on a component by component basis */
      if (pchan->rotmode == ROT_MODE_AXISANGLE) {
        if ((pchan->protectflag & OB_LOCK_ROTW) == 0) {
          pchan->rotAngle = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->rotAxis[0] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->rotAxis[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->rotAxis[2] = 0.0f;
        }

        /* check validity of axis - axis should never be 0,0,0
         * (if so, then we make it rotate about y). */
        if (IS_EQF(pchan->rotAxis[0], pchan->rotAxis[1]) &&
            IS_EQF(pchan->rotAxis[1], pchan->rotAxis[2]))
        {
          pchan->rotAxis[1] = 1.0f;
        }
      }
      else if (pchan->rotmode == ROT_MODE_QUAT) {
        if ((pchan->protectflag & OB_LOCK_ROTW) == 0) {
          pchan->quat[0] = 1.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->quat[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->quat[2] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->quat[3] = 0.0f;
        }
      }
      else {
        /* the flag may have been set for the other modes, so just ignore the extra flag... */
        if ((pchan->protectflag & OB_LOCK_ROTX) == 0) {
          pchan->eul[0] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTY) == 0) {
          pchan->eul[1] = 0.0f;
        }
        if ((pchan->protectflag & OB_LOCK_ROTZ) == 0) {
          pchan->eul[2] = 0.0f;
        }
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

      if (pchan->protectflag & OB_LOCK_ROTX) {
        eul[0] = oldeul[0];
      }
      if (pchan->protectflag & OB_LOCK_ROTY) {
        eul[1] = oldeul[1];
      }
      if (pchan->protectflag & OB_LOCK_ROTZ) {
        eul[2] = oldeul[2];
      }

      if (pchan->rotmode == ROT_MODE_QUAT) {
        eul_to_quat(pchan->quat, eul);

        /* restore original quat size */
        mul_qt_fl(pchan->quat, qlen);

        /* quaternions flip w sign to accumulate rotations correctly */
        if ((quat1[0] < 0.0f && pchan->quat[0] > 0.0f) ||
            (quat1[0] > 0.0f && pchan->quat[0] < 0.0f))
        {
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
  } /* Duplicated in source/blender/editors/object/object_transform.cc */
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

  /* Clear also Bendy Bone stuff - Roll is obvious,
   * but Curve X/Y stuff is also kind of rotational in nature... */
  pchan->roll1 = 0.0f;
  pchan->roll2 = 0.0f;

  pchan->curve_in_x = 0.0f;
  pchan->curve_in_z = 0.0f;
  pchan->curve_out_x = 0.0f;
  pchan->curve_out_z = 0.0f;
}
/* Clear the rotation. When X-mirror is enabled,
 * also clear the rotation of the mirrored pose channel. */
static void pchan_clear_rot_with_mirrored(const bPose *pose, bPoseChannel *pchan)
{
  if (pose->flag & POSE_MIRROR_EDIT) {
    bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(pose, pchan->name);
    if (pchan_mirror != nullptr) {
      pchan_clear_rot(pchan_mirror);
    }
  }
  pchan_clear_rot(pchan);
}

/* clear loc/rot/scale of pose-channel */
static void pchan_clear_transforms(const bPose *pose, bPoseChannel *pchan)
{
  pchan_clear_loc_with_mirrored(pose, pchan);
  pchan_clear_rot_with_mirrored(pose, pchan);
  pchan_clear_scale_with_mirrored(pose, pchan);
}

/* --------------- */

/* generic exec for clear-pose operators */
static wmOperatorStatus pose_clear_transform_generic_exec(bContext *C,
                                                          wmOperator *op,
                                                          void (*clear_func)(const bPose *,
                                                                             bPoseChannel *),
                                                          const char default_ksName[])
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  bool changed_multi = false;

  /* sanity checks */
  if (ELEM(nullptr, clear_func, default_ksName)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Programming error: missing clear transform function or keying set name");
    return OPERATOR_CANCELLED;
  }

  /* only clear relevant transforms for selected bones */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob_iter) {
    /* XXX: UGLY HACK (for auto-key + clear transforms). */
    Object *ob_eval = DEG_get_evaluated(depsgraph, ob_iter);
    blender::Vector<PointerRNA> sources;
    bool changed = false;

    FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN (ob_iter, pchan) {
      /* run provided clearing function */
      clear_func(ob_iter->pose, pchan);
      changed = true;

      /* do auto-keyframing as appropriate */
      if (blender::animrig::autokeyframe_cfra_can_key(scene, &ob_iter->id)) {
        /* tag for autokeying later */
        blender::animrig::relative_keyingset_add_source(
            sources, &ob_iter->id, &RNA_PoseBone, pchan);

#if 1 /* XXX: Ugly Hack - Run clearing function on evaluated copy of pchan */
        bPoseChannel *pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, pchan->name);
        clear_func(ob_iter->pose, pchan_eval);
#endif
      }
    }
    FOREACH_PCHAN_SELECTED_IN_OBJECT_END;

    if (changed) {
      changed_multi = true;

      /* perform autokeying on the bones if needed */
      if (!sources.is_empty()) {
        /* get KeyingSet to use */
        KeyingSet *ks = blender::animrig::get_keyingset_for_autokeying(scene, default_ksName);

        /* insert keyframes */
        blender::animrig::apply_keyingset(
            C, &sources, ks, blender::animrig::ModifyKeyMode::INSERT, float(scene->r.cfra));

        /* now recalculate paths */
        if (ob_iter->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) {
          ED_pose_recalculate_paths(C, scene, ob_iter, POSE_PATH_CALC_RANGE_FULL);
        }
      }

      DEG_id_tag_update(&ob_iter->id, ID_RECALC_GEOMETRY);

      /* NOTE: notifier might evolve. */
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob_iter);
    }
  }
  FOREACH_OBJECT_IN_MODE_END;

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Pose Scale Operator
 * \{ */

static wmOperatorStatus pose_clear_scale_exec(bContext *C, wmOperator *op)
{
  return pose_clear_transform_generic_exec(
      C, op, pchan_clear_scale_with_mirrored, ANIM_KS_SCALING_ID);
}

void POSE_OT_scale_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Scale";
  ot->idname = "POSE_OT_scale_clear";
  ot->description = "Reset scaling of selected bones to their default values";

  /* API callbacks. */
  ot->exec = pose_clear_scale_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Pose Rotation Operator
 * \{ */

static wmOperatorStatus pose_clear_rot_exec(bContext *C, wmOperator *op)
{
  return pose_clear_transform_generic_exec(
      C, op, pchan_clear_rot_with_mirrored, ANIM_KS_ROTATION_ID);
}

void POSE_OT_rot_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Rotation";
  ot->idname = "POSE_OT_rot_clear";
  ot->description = "Reset rotations of selected bones to their default values";

  /* API callbacks. */
  ot->exec = pose_clear_rot_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Pose Location Operator
 * \{ */

static wmOperatorStatus pose_clear_loc_exec(bContext *C, wmOperator *op)
{
  return pose_clear_transform_generic_exec(
      C, op, pchan_clear_loc_with_mirrored, ANIM_KS_LOCATION_ID);
}

void POSE_OT_loc_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Location";
  ot->idname = "POSE_OT_loc_clear";
  ot->description = "Reset locations of selected bones to their default values";

  /* API callbacks. */
  ot->exec = pose_clear_loc_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Pose Transforms Operator
 * \{ */

static wmOperatorStatus pose_clear_transforms_exec(bContext *C, wmOperator *op)
{
  return pose_clear_transform_generic_exec(
      C, op, pchan_clear_transforms, ANIM_KS_LOC_ROT_SCALE_ID);
}

void POSE_OT_transforms_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Pose Transforms";
  ot->idname = "POSE_OT_transforms_clear";
  ot->description =
      "Reset location, rotation, and scaling of selected bones to their default values";

  /* API callbacks. */
  ot->exec = pose_clear_transforms_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear User Transforms Operator
 * \{ */

static wmOperatorStatus pose_clear_user_transforms_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, float(scene->r.cfra));
  const bool only_select = RNA_boolean_get(op->ptr, "only_selected");

  FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, OB_ARMATURE, OB_MODE_POSE, ob) {
    if ((ob->adt) && (ob->adt->action)) {
      /* XXX: this is just like this to avoid contaminating anything else;
       * just pose values should change, so this should be fine
       */
      bPose *dummyPose = nullptr;
      Object workob{};

      /* execute animation step for current frame using a dummy copy of the pose */
      BKE_pose_copy_data(&dummyPose, ob->pose, false);

      STRNCPY_UTF8(workob.id.name, "OB<ClearTfmWorkOb>");
      workob.type = OB_ARMATURE;
      workob.data = ob->data;
      workob.adt = ob->adt;
      workob.pose = dummyPose;

      BKE_animsys_evaluate_animdata(
          &workob.id, workob.adt, &anim_eval_context, ADT_RECALC_ANIM, false);

      /* Copy back values, but on selected bones only. */
      LISTBASE_FOREACH (bPoseChannel *, pchan, &dummyPose->chanbase) {
        pose_bone_do_paste(ob, pchan, only_select, false);
      }

      /* free temp data - free manually as was copied without constraints */
      LISTBASE_FOREACH (bPoseChannel *, pchan, &dummyPose->chanbase) {
        if (pchan->prop) {
          IDP_FreeProperty(pchan->prop);
        }
        if (pchan->system_properties) {
          IDP_FreeProperty(pchan->system_properties);
        }
      }

      /* was copied without constraints */
      BLI_freelistN(&dummyPose->chanbase);
      MEM_freeN(dummyPose);
    }
    else {
      /* No animation, so just reset to the rest pose. */
      BKE_pose_rest(ob->pose, only_select);
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
  ot->description = "Reset pose bone transforms to keyframed state";

  /* callbacks */
  ot->exec = pose_clear_user_transforms_exec;
  ot->poll = ED_operator_posemode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "only_selected", true, "Only Selected", "Only visible/selected bones");
}

/** \} */
