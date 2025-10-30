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

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include <cstddef>

#  include "DNA_armature_types.h"

#  include "BLI_math_matrix.h"
#  include "BLI_math_vector.h"

#  include "BKE_armature.hh"
#  include "BKE_report.hh"

#  include "ED_armature.hh"

#  include "ANIM_bone_collections.hh"

#  include "WM_api.hh"

static void rna_EditBone_align_roll(EditBone *ebo, const float no[3])
{
  ebo->roll = ED_armature_ebone_roll_to_vector(ebo, no, false);
  WM_main_add_notifier(NC_GEOM | ND_DATA, nullptr);
}

static float rna_Bone_do_envelope(Bone *bone, const float vec[3])
{
  float scale = (bone->flag & BONE_MULT_VG_ENV) == BONE_MULT_VG_ENV ? bone->weight : 1.0f;
  return distfactor_to_bone(vec,
                            bone->arm_head,
                            bone->arm_tail,
                            bone->rad_head * scale,
                            bone->rad_tail * scale,
                            bone->dist * scale);
}

static void rna_Bone_convert_local_to_pose(Bone *bone,
                                           float r_matrix[16],
                                           const float matrix[16],
                                           const float matrix_local[16],
                                           const float parent_matrix[16],
                                           const float parent_matrix_local[16],
                                           bool invert)
{
  BoneParentTransform bpt;
  float offs_bone[4][4];
  float (*bone_arm_mat)[4] = (float (*)[4])matrix_local;
  float (*parent_pose_mat)[4] = (float (*)[4])parent_matrix;
  float (*parent_arm_mat)[4] = (float (*)[4])parent_matrix_local;

  if (is_zero_m4(parent_pose_mat) || is_zero_m4(parent_arm_mat)) {
    /* No parent case. */
    BKE_bone_parent_transform_calc_from_matrices(
        bone->flag, bone->inherit_scale_mode, bone_arm_mat, nullptr, nullptr, &bpt);
  }
  else {
    invert_m4_m4(offs_bone, parent_arm_mat);
    mul_m4_m4m4(offs_bone, offs_bone, bone_arm_mat);

    BKE_bone_parent_transform_calc_from_matrices(
        bone->flag, bone->inherit_scale_mode, offs_bone, parent_arm_mat, parent_pose_mat, &bpt);
  }

  if (invert) {
    BKE_bone_parent_transform_invert(&bpt);
  }

  BKE_bone_parent_transform_apply(&bpt, (float (*)[4])matrix, (float (*)[4])r_matrix);
}

static void rna_Bone_MatrixFromAxisRoll(const float axis[3], float roll, float r_matrix[9])
{
  vec_roll_to_mat3(axis, roll, (float (*)[3])r_matrix);
}

static void rna_Bone_AxisRollFromMatrix(const float matrix[9],
                                        const float axis_override[3],
                                        float r_axis[3],
                                        float *r_roll)
{
  float mat[3][3];

  normalize_m3_m3(mat, (float (*)[3])matrix);

  if (normalize_v3_v3(r_axis, axis_override) != 0.0f) {
    mat3_vec_to_roll(mat, r_axis, r_roll);
  }
  else {
    mat3_to_vec_roll(mat, r_axis, r_roll);
  }
}

using bonecoll_assign_func_bone = bool (*)(BoneCollection *, Bone *);
using bonecoll_assign_func_ebone = bool (*)(BoneCollection *, EditBone *);

static bool rna_BoneCollection_assign_abstract(BoneCollection *bcoll,
                                               bContext *C,
                                               ReportList *reports,
                                               PointerRNA *bone_ptr,
                                               bonecoll_assign_func_bone assign_bone,
                                               bonecoll_assign_func_ebone assign_ebone)

{
  if (RNA_pointer_is_null(bone_ptr)) {
    return false;
  }

  if (RNA_struct_is_a(bone_ptr->type, &RNA_PoseBone)) {
    bPoseChannel *pchan = static_cast<bPoseChannel *>(bone_ptr->data);
    const bool made_any_change = assign_bone(bcoll, pchan->bone);
    if (made_any_change) {
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
    }
    return made_any_change;
  }

  if (RNA_struct_is_a(bone_ptr->type, &RNA_Bone)) {
    Bone *bone = static_cast<Bone *>(bone_ptr->data);
    const bool made_any_change = assign_bone(bcoll, bone);
    if (made_any_change) {
      WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
    }
    return made_any_change;
  }

  if (RNA_struct_is_a(bone_ptr->type, &RNA_EditBone)) {
    EditBone *ebone = static_cast<EditBone *>(bone_ptr->data);
    const bool made_any_change = assign_ebone(bcoll, ebone);
    if (made_any_change) {
      WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, nullptr);
    }
    return made_any_change;
  }
  BKE_reportf(reports,
              RPT_ERROR,
              "%s is not supported, pass a Bone, PoseBone, or EditBone",
              RNA_struct_identifier(bone_ptr->type));
  return false;
}

static bool rna_BoneCollection_assign(BoneCollection *bcoll,
                                      bContext *C,
                                      ReportList *reports,
                                      PointerRNA *bone_ptr)
{
  return rna_BoneCollection_assign_abstract(bcoll,
                                            C,
                                            reports,
                                            bone_ptr,
                                            ANIM_armature_bonecoll_assign,
                                            ANIM_armature_bonecoll_assign_editbone);
}

static bool rna_BoneCollection_unassign(BoneCollection *bcoll,
                                        bContext *C,
                                        ReportList *reports,
                                        PointerRNA *bone_ptr)
{
  return rna_BoneCollection_assign_abstract(bcoll,
                                            C,
                                            reports,
                                            bone_ptr,
                                            ANIM_armature_bonecoll_unassign,
                                            ANIM_armature_bonecoll_unassign_editbone);
}

#else

void RNA_api_armature_edit_bone(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "align_roll", "rna_EditBone_align_roll");
  RNA_def_function_ui_description(func,
                                  "Align the bone to a local-space roll so the Z axis "
                                  "points in the direction of the vector given");
  parm = RNA_def_float_vector(
      func, "vector", 3, nullptr, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_api_bone(StructRNA *srna)
{
  PropertyRNA *parm;
  FunctionRNA *func;

  func = RNA_def_function(srna, "evaluate_envelope", "rna_Bone_do_envelope");
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

  func = RNA_def_function(srna, "convert_local_to_pose", "rna_Bone_convert_local_to_pose");
  RNA_def_function_ui_description(func,
                                  "Transform a matrix from Local to Pose space (or back), taking "
                                  "into account options like Inherit Scale and Local Location. "
                                  "Unlike Object.convert_space, this uses custom rest and pose "
                                  "matrices provided by the caller. If the parent matrices are "
                                  "omitted, the bone is assumed to have no parent.");
  parm = RNA_def_property(func, "matrix_return", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The transformed matrix");
  RNA_def_function_output(func, parm);
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The matrix to transform");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "matrix_local", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The custom rest matrix of this bone (Bone.matrix_local)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "parent_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      parm, "", "The custom pose matrix of the parent bone (PoseBone.matrix)");
  parm = RNA_def_property(func, "parent_matrix_local", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      parm, "", "The custom rest matrix of the parent bone (Bone.matrix_local)");
  RNA_def_boolean(func, "invert", false, "", "Convert from Pose to Local space");

  /* Conversions between Matrix and Axis + Roll representations. */
  func = RNA_def_function(srna, "MatrixFromAxisRoll", "rna_Bone_MatrixFromAxisRoll");
  RNA_def_function_ui_description(func, "Convert the axis + roll representation to a matrix");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_property(func, "axis", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(parm, "", "The main axis of the bone (tail - head)");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "roll", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(parm, "", "The roll of the bone");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "result_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_3x3);
  RNA_def_property_ui_text(parm, "", "The resulting orientation matrix");
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "AxisRollFromMatrix", "rna_Bone_AxisRollFromMatrix");
  RNA_def_function_ui_description(func,
                                  "Convert a rotational matrix to the axis + roll representation. "
                                  "Note that the resulting value of the roll may not be as "
                                  "expected if the matrix has shear or negative determinant.");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_3x3);
  RNA_def_property_ui_text(parm, "", "The orientation matrix of the bone");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "axis", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(
      parm, "", "The optional override for the axis (finds closest approximation for the matrix)");
  parm = RNA_def_property(func, "result_axis", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(parm, "", "The main axis of the bone");
  RNA_def_function_output(func, parm);
  parm = RNA_def_property(func, "result_roll", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(parm, "", "The roll of the bone");
  RNA_def_function_output(func, parm);
}

void RNA_api_bonecollection(StructRNA *srna)
{
  PropertyRNA *parm;
  FunctionRNA *func;

  func = RNA_def_function(srna, "assign", "rna_BoneCollection_assign");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Assign the given bone to this collection");
  parm = RNA_def_pointer(
      func, "bone", "AnyType", "", "Bone, PoseBone, or EditBone to assign to this collection");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  /* return value */
  parm = RNA_def_boolean(func,
                         "assigned",
                         false,
                         "Assigned",
                         "Whether the bone was actually assigned; will be false if the bone was "
                         "already member of the collection");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "unassign", "rna_BoneCollection_unassign");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the given bone from this collection");
  parm = RNA_def_pointer(
      func, "bone", "AnyType", "", "Bone, PoseBone, or EditBone to remove from this collection");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  /* return value */
  parm = RNA_def_boolean(func,
                         "assigned",
                         false,
                         "Unassigned",
                         "Whether the bone was actually removed; will be false if the bone was "
                         "not a member of the collection to begin with");
  RNA_def_function_return(func, parm);
}

#endif
