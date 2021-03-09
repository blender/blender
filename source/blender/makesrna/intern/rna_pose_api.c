/*
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_object_types.h"

/* #include "BLI_sys_types.h" */

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_animsys.h"
#  include "BKE_armature.h"
#  include "BKE_context.h"

#  include "DNA_action_types.h"
#  include "DNA_anim_types.h"

#  include "BLI_ghash.h"

static float rna_PoseBone_do_envelope(bPoseChannel *chan, float *vec)
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

static void rna_PoseBone_bbone_segment_matrix(
    bPoseChannel *pchan, ReportList *reports, float mat_ret[16], int index, bool rest)
{
  if (!pchan->bone || pchan->bone->segments <= 1) {
    BKE_reportf(reports, RPT_ERROR, "Bone '%s' is not a B-Bone!", pchan->name);
    return;
  }
  if (pchan->runtime.bbone_segments != pchan->bone->segments) {
    BKE_reportf(reports, RPT_ERROR, "Bone '%s' has out of date B-Bone segment data!", pchan->name);
    return;
  }
  if (index < 0 || index > pchan->runtime.bbone_segments) {
    BKE_reportf(
        reports, RPT_ERROR, "Invalid index %d for B-Bone segments of '%s'!", index, pchan->name);
    return;
  }

  if (rest) {
    copy_m4_m4((float(*)[4])mat_ret, pchan->runtime.bbone_rest_mats[index].mat);
  }
  else {
    copy_m4_m4((float(*)[4])mat_ret, pchan->runtime.bbone_pose_mats[index].mat);
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
  BKE_pose_apply_action(pose_owner_ob, action, &anim_eval_context);

  /* Do NOT tag with ID_RECALC_ANIMATION, as that would overwrite the just-applied pose. */
  DEG_id_tag_update(pose_owner, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_POSE, pose_owner);
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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  parm = RNA_def_float(func,
                       "evaluation_time",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Evaluation Time",
                       "Time at which the given action is evaluated to obtain the pose",
                       -FLT_MAX,
                       FLT_MAX);
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
                                  NULL,
                                  -FLT_MAX,
                                  FLT_MAX,
                                  "Point",
                                  "Position in 3d space to evaluate",
                                  -FLT_MAX,
                                  FLT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return value */
  parm = RNA_def_float(
      func, "factor", 0, -FLT_MAX, FLT_MAX, "Factor", "Envelope factor", -FLT_MAX, FLT_MAX);
  RNA_def_function_return(func, parm);

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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
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
