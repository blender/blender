/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "RNA_define.hh"

/* #include "BLI_sys_types.h" */

#include "rna_internal.hh" /* own include */

using namespace blender;

#ifdef RNA_RUNTIME

#  include "BKE_animsys.h"
#  include "BKE_armature.hh"
#  include "BKE_context.hh"
#  include "BKE_pose_backup.h"

#  include "DNA_action_types.h"
#  include "DNA_anim_types.h"

#  include "BLI_ghash.h"
#  include "BLI_math_matrix.h"

#  include "ANIM_action.hh"
#  include "ANIM_pose.hh"

static float rna_PoseBone_do_envelope(bPoseChannel *chan, const float vec[3])
{
  Bone *bone = chan->bone;

  float scale = (bone->flag & BONE_MULT_VG_ENV) == BONE_MULT_VG_ENV ? bone->weight : 1.0f;

  return distfactor_to_bone(vec,
                            chan->pose_head,
                            chan->pose_tail,
                            bone->rad_head * scale,
                            bone->rad_tail * scale,
                            bone->dist * scale);
}

static void rna_PoseBone_bbone_segment_index(
    bPoseChannel *pchan, ReportList *reports, const float pt[3], int *r_index, float *r_blend_next)
{
  if (!pchan->bone || pchan->bone->segments <= 1) {
    BKE_reportf(reports, RPT_ERROR, "Bone '%s' is not a B-Bone!", pchan->name);
    return;
  }
  if (pchan->runtime.bbone_segments != pchan->bone->segments) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Bone '%s' has out of date B-Bone segment data - depsgraph update required!",
                pchan->name);
    return;
  }

  BKE_pchan_bbone_deform_segment_index(pchan, pt, r_index, r_blend_next);
}

static void rna_PoseBone_bbone_segment_matrix(
    bPoseChannel *pchan, ReportList *reports, float mat_ret[16], int index, bool rest)
{
  if (!pchan->bone || pchan->bone->segments <= 1) {
    BKE_reportf(reports, RPT_ERROR, "Bone '%s' is not a B-Bone!", pchan->name);
    return;
  }
  if (pchan->runtime.bbone_segments != pchan->bone->segments) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Bone '%s' has out of date B-Bone segment data - depsgraph update required!",
                pchan->name);
    return;
  }
  if (index < 0 || index > pchan->runtime.bbone_segments) {
    BKE_reportf(
        reports, RPT_ERROR, "Invalid index %d for B-Bone segments of '%s'!", index, pchan->name);
    return;
  }

  if (rest) {
    copy_m4_m4((float (*)[4])mat_ret, pchan->runtime.bbone_rest_mats[index].mat);
  }
  else {
    copy_m4_m4((float (*)[4])mat_ret, pchan->runtime.bbone_pose_mats[index].mat);
  }
}

static void rna_PoseBone_compute_bbone_handles(bPoseChannel *pchan,
                                               ReportList *reports,
                                               float ret_h1[3],
                                               float *ret_roll1,
                                               float ret_h2[3],
                                               float *ret_roll2,
                                               bool rest,
                                               bool ease,
                                               bool offsets)
{
  if (!pchan->bone || pchan->bone->segments <= 1) {
    BKE_reportf(reports, RPT_ERROR, "Bone '%s' is not a B-Bone!", pchan->name);
    return;
  }

  BBoneSplineParameters params;

  BKE_pchan_bbone_spline_params_get(pchan, rest, &params);
  BKE_pchan_bbone_handles_compute(
      &params, ret_h1, ret_roll1, ret_h2, ret_roll2, ease || offsets, offsets);
}

static void rna_Pose_apply_pose_from_action(ID *pose_owner,
                                            bContext *C,
                                            bAction *action,
                                            const float evaluation_time)
{
  BLI_assert(GS(pose_owner->name) == ID_OB);
  Object *pose_owner_ob = (Object *)pose_owner;

  AnimationEvalContext anim_eval_context = {CTX_data_depsgraph_pointer(C), evaluation_time};
  animrig::pose_apply_action({pose_owner_ob}, action->wrap(), &anim_eval_context, 1.0);

  /* Do NOT tag with ID_RECALC_ANIMATION, as that would overwrite the just-applied pose. */
  DEG_id_tag_update(pose_owner, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pose_owner);
}

static void rna_Pose_blend_pose_from_action(ID *pose_owner,
                                            bContext *C,
                                            bAction *action,
                                            const float blend_factor,
                                            const float evaluation_time)
{
  BLI_assert(GS(pose_owner->name) == ID_OB);
  Object *pose_owner_ob = (Object *)pose_owner;

  AnimationEvalContext anim_eval_context = {CTX_data_depsgraph_pointer(C), evaluation_time};
  animrig::pose_apply_action({pose_owner_ob}, action->wrap(), &anim_eval_context, blend_factor);

  /* Do NOT tag with ID_RECALC_ANIMATION, as that would overwrite the just-applied pose. */
  DEG_id_tag_update(pose_owner, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pose_owner);
}

static void rna_Pose_backup_create(ID *pose_owner, bAction *action)
{
  BLI_assert(GS(pose_owner->name) == ID_OB);
  if (!action || action->wrap().slot_array_num == 0) {
    /* A pose asset without slots has no data, this usually doesn't happen but can happen by
     * tagging an empty action as a pose asset. */
    return;
  }
  Object *pose_owner_ob = (Object *)pose_owner;
  BKE_pose_backup_create_on_object(pose_owner_ob, action);
}

static bool rna_Pose_backup_restore(ID *pose_owner, bContext *C)
{
  BLI_assert(GS(pose_owner->name) == ID_OB);
  Object *pose_owner_ob = (Object *)pose_owner;

  const bool success = BKE_pose_backup_restore_on_object(pose_owner_ob);
  if (!success) {
    return false;
  }

  /* Do NOT tag with ID_RECALC_ANIMATION, as that would overwrite the just-applied pose. */
  DEG_id_tag_update(pose_owner, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pose_owner);

  return true;
}

static void rna_Pose_backup_clear(ID *pose_owner)
{
  BLI_assert(GS(pose_owner->name) == ID_OB);
  Object *pose_owner_ob = (Object *)pose_owner;

  BKE_pose_backup_clear(pose_owner_ob);
}

#else

void RNA_api_pose(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "apply_pose_from_action", "rna_Pose_apply_pose_from_action");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(
      func,
      "Apply the given action to this pose by evaluating it at a specific time. Only updates the "
      "pose of selected bones, or all bones if none are selected.");
  parm = RNA_def_pointer(func, "action", "Action", "Action", "The Action containing the pose");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "evaluation_time",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Evaluation Time",
                       "Time at which the given action is evaluated to obtain the pose",
                       -FLT_MAX,
                       FLT_MAX);

  func = RNA_def_function(srna, "blend_pose_from_action", "rna_Pose_blend_pose_from_action");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func,
                                  "Blend the given action into this pose by evaluating it at a "
                                  "specific time. Only updates the "
                                  "pose of selected bones, or all bones if none are selected.");
  parm = RNA_def_pointer(func, "action", "Action", "Action", "The Action containing the pose");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float(func,
                "blend_factor",
                1.0f,
                0.0f,
                1.0f,
                "Blend Factor",
                "How much the given Action affects the final pose",
                0.0f,
                1.0f);
  RNA_def_float(func,
                "evaluation_time",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Evaluation Time",
                "Time at which the given action is evaluated to obtain the pose",
                -FLT_MAX,
                FLT_MAX);

  func = RNA_def_function(srna, "backup_create", "rna_Pose_backup_create");
  RNA_def_function_ui_description(
      func,
      "Create a backup of the current pose. Only those bones that are animated in the Action are "
      "backed up. The object owns the backup, and each object can have only one backup at a time. "
      "When you no longer need it, it must be freed use ``backup_clear()``.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);
  parm = RNA_def_pointer(func,
                         "action",
                         "Action",
                         "Action",
                         "An Action with animation data for the bones. "
                         "Only the animated bones will be included in the backup.");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "backup_restore", "rna_Pose_backup_restore");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(
      func,
      "Restore the previously made pose backup. "
      "This can be called multiple times. See ``Pose.backup_create()`` for more info.");
  /* return value */
  parm = RNA_def_boolean(
      func,
      "success",
      false,
      "",
      "``True`` when the backup was restored, ``False`` if there was no backup to restore");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "backup_clear", "rna_Pose_backup_clear");
  RNA_def_function_ui_description(
      func, "Free a previously made pose backup. See ``Pose.backup_create()`` for more info.");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);
}

void RNA_api_pose_channel(StructRNA *srna)
{
  PropertyRNA *parm;
  FunctionRNA *func;

  func = RNA_def_function(srna, "evaluate_envelope", "rna_PoseBone_do_envelope");
  RNA_def_function_ui_description(func, "Calculate bone envelope at given point");
  parm = RNA_def_float_vector_xyz(func,
                                  "point",
                                  3,
                                  nullptr,
                                  -FLT_MAX,
                                  FLT_MAX,
                                  "Point",
                                  "Position in 3d space to evaluate",
                                  -FLT_MAX,
                                  FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_float(
      func, "factor", 0, -FLT_MAX, FLT_MAX, "Factor", "Envelope factor", -FLT_MAX, FLT_MAX);
  RNA_def_function_return(func, parm);

  /* B-Bone segment index from point */
  func = RNA_def_function(srna, "bbone_segment_index", "rna_PoseBone_bbone_segment_index");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func, "Retrieve the index and blend factor of the B-Bone segments based on vertex position");
  parm = RNA_def_float_vector_xyz(func,
                                  "point",
                                  3,
                                  nullptr,
                                  -FLT_MAX,
                                  FLT_MAX,
                                  "Point",
                                  "Vertex position in armature pose space",
                                  -FLT_MAX,
                                  FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* outputs */
  parm = RNA_def_property(func, "index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(parm, "", "The index of the first segment joint affecting the point");
  RNA_def_function_output(func, parm);
  parm = RNA_def_property(func, "blend_next", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(parm, "", "The blend factor between the given and the following joint");
  RNA_def_function_output(func, parm);

  /* B-Bone segment matrices */
  func = RNA_def_function(srna, "bbone_segment_matrix", "rna_PoseBone_bbone_segment_matrix");
  RNA_def_function_ui_description(
      func, "Retrieve the matrix of the joint between B-Bone segments if available");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_property(func, "matrix_return", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The resulting matrix in bone local space");
  RNA_def_function_output(func, parm);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "", "Index of the segment endpoint", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "rest", false, "", "Return the rest pose matrix");

  /* B-Bone custom handle positions */
  func = RNA_def_function(srna, "compute_bbone_handles", "rna_PoseBone_compute_bbone_handles");
  RNA_def_function_ui_description(
      func, "Retrieve the vectors and rolls coming from B-Bone custom handles");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_property(func, "handle1", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(
      parm, "", "The direction vector of the start handle in bone local space");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float(
      func, "roll1", 0, -FLT_MAX, FLT_MAX, "", "Roll of the start handle", -FLT_MAX, FLT_MAX);
  RNA_def_function_output(func, parm);
  parm = RNA_def_property(func, "handle2", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(parm, "", "The direction vector of the end handle in bone local space");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float(
      func, "roll2", 0, -FLT_MAX, FLT_MAX, "", "Roll of the end handle", -FLT_MAX, FLT_MAX);
  RNA_def_function_output(func, parm);
  parm = RNA_def_boolean(func, "rest", false, "", "Return the rest pose state");
  parm = RNA_def_boolean(func, "ease", false, "", "Apply scale from ease values");
  parm = RNA_def_boolean(
      func, "offsets", false, "", "Apply roll and curve offsets from bone properties");
}

#endif
