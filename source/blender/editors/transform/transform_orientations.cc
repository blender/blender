/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include <cctype>
#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLT_translation.hh"

#include "ED_armature.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "SEQ_select.hh"

#include "SEQ_transform.hh"
#include "transform.hh"
#include "transform_orientations.hh"

namespace blender::ed::transform {

/* *********************** TransSpace ************************** */

void BIF_clearTransformOrientation(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;

  BLI_freelistN(transform_orientations);

  for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
    TransformOrientationSlot *orient_slot = &scene->orientation_slots[i];
    if (orient_slot->type == V3D_ORIENT_CUSTOM) {
      orient_slot->type = V3D_ORIENT_GLOBAL; /* Fallback to global. */
      orient_slot->index_custom = -1;
    }
  }
}

static TransformOrientation *findOrientationName(ListBase *lb, const char *name)
{
  return static_cast<TransformOrientation *>(
      BLI_findstring(lb, name, offsetof(TransformOrientation, name)));
}

static void uniqueOrientationName(ListBase *lb, char *name)
{
  BLI_uniquename_cb(
      [&](const StringRefNull check_name) {
        return findOrientationName(lb, check_name.c_str()) != nullptr;
      },
      CTX_DATA_(BLT_I18NCONTEXT_ID_SCENE, "Space"),
      '.',
      name,
      sizeof(TransformOrientation::name));
}

static TransformOrientation *createViewSpace(bContext *C,
                                             ReportList * /*reports*/,
                                             const char *name,
                                             const bool overwrite)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  float mat[3][3];

  if (!rv3d) {
    return nullptr;
  }

  copy_m3_m4(mat, rv3d->viewinv);
  normalize_m3(mat);

  if (name[0] == 0) {
    View3D *v3d = CTX_wm_view3d(C);
    if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
      /* If an object is used as camera, then this space is the same as object space! */
      name = v3d->camera->id.name + 2;
    }
    else {
      name = DATA_("Custom View");
    }
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createObjectSpace(bContext *C,
                                               ReportList * /*reports*/,
                                               const char *name,
                                               const bool overwrite)
{
  Base *base = CTX_data_active_base(C);
  Object *ob;
  float mat[3][3];

  if (base == nullptr) {
    return nullptr;
  }

  ob = base->object;

  copy_m3_m4(mat, ob->object_to_world().ptr());
  normalize_m3(mat);

  /* Use object name if no name is given. */
  if (name[0] == 0) {
    name = ob->id.name + 2;
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createBoneSpace(bContext *C,
                                             ReportList *reports,
                                             const char *name,
                                             const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];

  getTransformOrientation(C, normal, plane);

  if (createSpaceNormalTangent(mat, normal, plane) == 0) {
    BKE_reports_prepend(reports, "Cannot use zero-length bone");
    return nullptr;
  }

  if (name[0] == 0) {
    name = DATA_("Bone");
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createCurveSpace(bContext *C,
                                              ReportList *reports,
                                              const char *name,
                                              const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];

  getTransformOrientation(C, normal, plane);

  if (createSpaceNormalTangent(mat, normal, plane) == 0) {
    BKE_reports_prepend(reports, "Cannot use zero-length curve");
    return nullptr;
  }

  if (name[0] == 0) {
    name = DATA_("Curve");
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static TransformOrientation *createMeshSpace(bContext *C,
                                             ReportList *reports,
                                             const char *name,
                                             const bool overwrite)
{
  float mat[3][3];
  float normal[3], plane[3];
  int type;

  type = getTransformOrientation(C, normal, plane);

  switch (type) {
    case ORIENTATION_VERT:
      if (createSpaceNormal(mat, normal) == 0) {
        BKE_reports_prepend(reports, "Cannot use vertex with zero-length normal");
        return nullptr;
      }

      if (name[0] == 0) {
        name = DATA_("Vertex");
      }
      break;
    case ORIENTATION_EDGE:
      if (createSpaceNormalTangent(mat, normal, plane) == 0) {
        BKE_reports_prepend(reports, "Cannot use zero-length edge");
        return nullptr;
      }

      if (name[0] == 0) {
        name = DATA_("Edge");
      }
      break;
    case ORIENTATION_FACE:
      if (createSpaceNormalTangent(mat, normal, plane) == 0) {
        BKE_reports_prepend(reports, "Cannot use zero-area face");
        return nullptr;
      }

      if (name[0] == 0) {
        name = DATA_("Face");
      }
      break;
    default:
      return nullptr;
  }

  return addMatrixSpace(C, mat, name, overwrite);
}

static bool test_rotmode_euler(short rotmode)
{
  return ELEM(rotmode, ROT_MODE_AXISANGLE, ROT_MODE_QUAT) ? false : true;
}

/**
 * Could move into BLI_math_rotation.h however this is only useful for display/editing purposes.
 */
static void axis_angle_to_gimbal_axis(float gmat[3][3], const float axis[3], const float angle)
{
  /* X/Y are arbitrary axes, most importantly Z is the axis of rotation. */

  float cross_vec[3];
  float quat[4];

  /* This is an un-scientific method to get a vector to cross with XYZ intentionally YZX. */
  cross_vec[0] = axis[1];
  cross_vec[1] = axis[2];
  cross_vec[2] = axis[0];

  /* X-axis. */
  cross_v3_v3v3(gmat[0], cross_vec, axis);
  normalize_v3(gmat[0]);
  axis_angle_to_quat(quat, axis, angle);
  mul_qt_v3(quat, gmat[0]);

  /* Y-axis. */
  axis_angle_to_quat(quat, axis, M_PI_2);
  copy_v3_v3(gmat[1], gmat[0]);
  mul_qt_v3(quat, gmat[1]);

  /* Z-axis. */
  copy_v3_v3(gmat[2], axis);

  normalize_m3(gmat);
}

bool gimbal_axis_pose(Object *ob, const bPoseChannel *pchan, float gmat[3][3])
{
  float mat[3][3], tmat[3][3], obmat[3][3];
  if (test_rotmode_euler(pchan->rotmode)) {
    eulO_to_gimbal_axis(mat, pchan->eul, pchan->rotmode);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    axis_angle_to_gimbal_axis(mat, pchan->rotAxis, pchan->rotAngle);
  }
  else { /* Quaternion. */
    return false;
  }

  /* Apply bone transformation. */
  mul_m3_m3m3(tmat, pchan->bone->bone_mat, mat);

  if (pchan->parent) {
    float parent_mat[3][3];

    copy_m3_m4(parent_mat,
               (pchan->bone->flag & BONE_HINGE) ? pchan->parent->bone->arm_mat :
                                                  pchan->parent->pose_mat);
    mul_m3_m3m3(mat, parent_mat, tmat);

    /* Needed if object transformation isn't identity. */
    copy_m3_m4(obmat, ob->object_to_world().ptr());
    mul_m3_m3m3(gmat, obmat, mat);
  }
  else {
    /* Needed if object transformation isn't identity. */
    copy_m3_m4(obmat, ob->object_to_world().ptr());
    mul_m3_m3m3(gmat, obmat, tmat);
  }

  normalize_m3(gmat);
  return true;
}

bool gimbal_axis_object(Object *ob, float gmat[3][3])
{
  if (test_rotmode_euler(ob->rotmode)) {
    eulO_to_gimbal_axis(gmat, ob->rot, ob->rotmode);
  }
  else if (ob->rotmode == ROT_MODE_AXISANGLE) {
    axis_angle_to_gimbal_axis(gmat, ob->rotAxis, ob->rotAngle);
  }
  else { /* Quaternion. */
    return false;
  }

  if (ob->parent) {
    float parent_mat[3][3];
    copy_m3_m4(parent_mat, ob->parent->object_to_world().ptr());
    normalize_m3(parent_mat);
    mul_m3_m3m3(gmat, parent_mat, gmat);
  }
  return true;
}

bool transform_orientations_create_from_axis(float mat[3][3],
                                             const float x[3],
                                             const float y[3],
                                             const float z[3])
{
  bool is_zero[3] = {true, true, true};
  zero_m3(mat);
  if (x) {
    is_zero[0] = normalize_v3_v3(mat[0], x) == 0.0f;
  }
  if (y) {
    is_zero[1] = normalize_v3_v3(mat[1], y) == 0.0f;
  }
  if (z) {
    is_zero[2] = normalize_v3_v3(mat[2], z) == 0.0f;
  }

  int zero_axis = is_zero[0] + is_zero[1] + is_zero[2];
  if (zero_axis == 0) {
    return true;
  }

  if (zero_axis == 1) {
    int axis = is_zero[0] ? 0 : is_zero[1] ? 1 : 2;
    cross_v3_v3v3(mat[axis], mat[(axis + 1) % 3], mat[(axis + 2) % 3]);
    if (normalize_v3(mat[axis]) != 0.0f) {
      return true;
    }
  }
  else if (zero_axis == 2) {
    int axis, a, b;
    axis = !is_zero[0] ? 0 : !is_zero[1] ? 1 : 2;
    a = (axis + 1) % 3;
    b = (axis + 2) % 3;

    mat[a][a] = 1.0f;
    mat[b][b] = 1.0f;
    project_plane_v3_v3v3(mat[a], mat[a], mat[axis]);
    project_plane_v3_v3v3(mat[b], mat[b], mat[axis]);
    if ((normalize_v3(mat[a]) != 0.0f) && (normalize_v3(mat[b]) != 0.0f)) {
      return true;
    }
  }

  unit_m3(mat);
  return false;
}

bool createSpaceNormal(float mat[3][3], const float normal[3])
{
  float tangent[3] = {0.0f, 0.0f, 1.0f};

  copy_v3_v3(mat[2], normal);
  if (normalize_v3(mat[2]) == 0.0f) {
    return false; /* Error return. */
  }

  cross_v3_v3v3(mat[0], mat[2], tangent);
  if (is_zero_v3(mat[0])) {
    tangent[0] = 1.0f;
    tangent[1] = tangent[2] = 0.0f;
    cross_v3_v3v3(mat[0], tangent, mat[2]);
  }

  cross_v3_v3v3(mat[1], mat[2], mat[0]);

  normalize_m3(mat);

  return true;
}

bool createSpaceNormalTangent(float mat[3][3], const float normal[3], const float tangent[3])
{
  BLI_ASSERT_UNIT_V3(normal);
  BLI_ASSERT_UNIT_V3(tangent);

  if (UNLIKELY(is_zero_v3(normal))) {
    /* Error return. */
    return false;
  }
  copy_v3_v3(mat[2], normal);

  /* Negate so we can use values from the matrix as input. */
  negate_v3_v3(mat[1], tangent);

  /* Preempt zero length tangent from causing trouble. */
  if (UNLIKELY(is_zero_v3(mat[1]))) {
    mat[1][2] = 1.0f;
  }

  cross_v3_v3v3(mat[0], mat[2], mat[1]);
  if (UNLIKELY(normalize_v3(mat[0]) == 0.0f)) {
    /* Error return from co-linear normal & tangent. */
    return false;
  }

  /* Make the tangent orthogonal. */
  cross_v3_v3v3(mat[1], mat[2], mat[0]);

  if (UNLIKELY(normalize_v3(mat[1]) == 0.0f)) {
    /* Error return as it's possible making the tangent orthogonal to the normal
     * causes it to be zero length. */
    return false;
  }

  /* Final matrix must be normalized, do inline. */
  // normalize_m3(mat);

  return true;
}

void createSpaceNormalTangent_or_fallback(float mat[3][3],
                                          const float normal[3],
                                          const float tangent[3])
{
  if (createSpaceNormalTangent(mat, normal, tangent)) {
    return;
  }
  if (!is_zero_v3(normal)) {
    axis_dominant_v3_to_m3(mat, normal);
    invert_m3(mat);
    return;
  }
  /* Last resort. */
  unit_m3(mat);
}

bool BIF_createTransformOrientation(bContext *C,
                                    ReportList *reports,
                                    const char *name,
                                    const bool use_view,
                                    const bool activate,
                                    const bool overwrite)
{
  TransformOrientation *ts = nullptr;

  if (use_view) {
    ts = createViewSpace(C, reports, name, overwrite);
  }
  else {
    Object *obedit = CTX_data_edit_object(C);
    Object *ob = CTX_data_active_object(C);
    if (obedit) {
      if (obedit->type == OB_MESH) {
        ts = createMeshSpace(C, reports, name, overwrite);
      }
      else if (obedit->type == OB_ARMATURE) {
        ts = createBoneSpace(C, reports, name, overwrite);
      }
      else if (obedit->type == OB_CURVES_LEGACY) {
        ts = createCurveSpace(C, reports, name, overwrite);
      }
    }
    else if (ob && (ob->mode & OB_MODE_POSE)) {
      ts = createBoneSpace(C, reports, name, overwrite);
    }
    else {
      ts = createObjectSpace(C, reports, name, overwrite);
    }
  }

  if (activate && ts != nullptr) {
    BIF_selectTransformOrientation(C, ts);
  }
  return (ts != nullptr);
}

TransformOrientation *addMatrixSpace(bContext *C,
                                     float mat[3][3],
                                     const char *name,
                                     const bool overwrite)
{
  TransformOrientation *ts = nullptr;
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;
  char name_unique[sizeof(ts->name)];

  if (overwrite) {
    ts = findOrientationName(transform_orientations, name);
  }
  else {
    STRNCPY_UTF8(name_unique, name);
    uniqueOrientationName(transform_orientations, name_unique);
    name = name_unique;
  }

  /* If not, create a new one. */
  if (ts == nullptr) {
    ts = MEM_callocN<TransformOrientation>("UserTransSpace from matrix");
    BLI_addtail(transform_orientations, ts);
    STRNCPY_UTF8(ts->name, name);
  }

  /* Copy matrix into transform space. */
  copy_m3_m3(ts->mat, mat);

  return ts;
}

void BIF_removeTransformOrientation(bContext *C, TransformOrientation *target)
{
  BKE_scene_transform_orientation_remove(CTX_data_scene(C), target);
}

void BIF_removeTransformOrientationIndex(bContext *C, int index)
{
  TransformOrientation *target = BKE_scene_transform_orientation_find(CTX_data_scene(C), index);
  BIF_removeTransformOrientation(C, target);
}

void BIF_selectTransformOrientation(bContext *C, TransformOrientation *target)
{
  Scene *scene = CTX_data_scene(C);
  int index = BKE_scene_transform_orientation_get_index(scene, target);

  BLI_assert(index != -1);

  scene->orientation_slots[SCE_ORIENT_DEFAULT].type = V3D_ORIENT_CUSTOM;
  scene->orientation_slots[SCE_ORIENT_DEFAULT].index_custom = index;
}

int BIF_countTransformOrientation(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *transform_orientations = &scene->transform_spaces;
  return BLI_listbase_count(transform_orientations);
}

void applyTransformOrientation(const TransformOrientation *ts, float r_mat[3][3], char r_name[64])
{
  if (r_name) {
    BLI_strncpy_utf8(r_name, ts->name, MAX_NAME);
  }
  copy_m3_m3(r_mat, ts->mat);
}

static int bone_children_clear_transflag(bPose &pose, bPoseChannel &pose_bone)
{
  int cleared = 0;
  animrig::pose_bone_descendent_iterator(pose, pose_bone, [&](bPoseChannel &child) {
    if (&child == &pose_bone) {
      return;
    }
    if (child.runtime.flag & POSE_RUNTIME_TRANSFORM) {
      child.runtime.flag &= ~POSE_RUNTIME_TRANSFORM;
      cleared++;
    }
  });
  return cleared;
}

/* Updates all `POSE_RUNTIME_TRANSFORM` flags.
 * Returns total number of bones with `POSE_RUNTIME_TRANSFORM`.
 * NOTE: `transform_convert_pose_transflags_update` has a similar logic. */
static int armature_bone_transflags_update(Object &ob,
                                           bArmature *arm,
                                           ListBase /* bPoseChannel */ *lb)
{
  int total = 0;

  LISTBASE_FOREACH (bPoseChannel *, pchan, lb) {
    pchan->runtime.flag &= ~POSE_RUNTIME_TRANSFORM;
    if (!ANIM_bone_in_visible_collection(arm, pchan->bone)) {
      continue;
    }
    if (pchan->flag & POSE_SELECTED) {
      pchan->runtime.flag |= POSE_RUNTIME_TRANSFORM;
      total++;
    }
  }

  /* No transform on children if any parent bone is selected. */
  LISTBASE_FOREACH (bPoseChannel *, pchan, lb) {
    if (pchan->runtime.flag & POSE_RUNTIME_TRANSFORM) {
      total -= bone_children_clear_transflag(*ob.pose, *pchan);
    }
  }
  return total;
}

void calc_orientation_from_type(const bContext *C, float r_mat[3][3])
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  const short orient_index = BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);
  const int pivot_point = scene->toolsettings->transform_pivot_point;

  calc_orientation_from_type_ex(
      scene, view_layer, v3d, rv3d, ob, obedit, orient_index, pivot_point, r_mat);
}

static void handle_armature_parent_orientation(Object *ob, float r_mat[3][3])
{
  bPoseChannel *active_pchan = BKE_pose_channel_active(ob, false);

  /* Check if target bone is a child. */
  if (active_pchan && active_pchan->parent) {
    /* For child, show parent local regardless if "local location" is set for parent bone. */
    transform_orientations_create_from_axis(r_mat, UNPACK3(active_pchan->parent->pose_mat));
    float ob_orientations_mat[3][3];
    transform_orientations_create_from_axis(ob_orientations_mat,
                                            UNPACK3(ob->object_to_world().ptr()));
    mul_m3_m3_pre(r_mat, ob_orientations_mat);
    return;
  }

  /* For root, use local transform of armature object. */
  transform_orientations_create_from_axis(r_mat, UNPACK3(ob->object_to_world().ptr()));
}

static void handle_object_parent_orientation(Object *ob, float r_mat[3][3])
{
  /* If object has parent, then orient to parent. */
  if (ob->parent) {
    transform_orientations_create_from_axis(r_mat, UNPACK3(ob->parent->object_to_world().ptr()));
  }
  else {
    /* If object doesn't have parent, then orient to world. */
    unit_m3(r_mat);
  }
}

short calc_orientation_from_type_ex(const Scene *scene,
                                    ViewLayer *view_layer,
                                    const View3D *v3d,
                                    const RegionView3D *rv3d,
                                    Object *ob,
                                    Object *obedit,
                                    const short orientation_index,
                                    const int pivot_point,
                                    float r_mat[3][3])
{
  switch (orientation_index) {
    case V3D_ORIENT_GIMBAL: {

      if (ob) {
        if (ob->mode & OB_MODE_POSE) {
          const bPoseChannel *pchan = BKE_pose_channel_active_if_bonecoll_visible(ob);

          if (pchan && gimbal_axis_pose(ob, pchan, r_mat)) {
            break;
          }
        }
        else {
          if (gimbal_axis_object(ob, r_mat)) {
            break;
          }
        }
      }
      /* If not gimbal, fall through to normal. */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_PARENT: {
      if (ob) {
        if (ob->mode & OB_MODE_POSE) {
          handle_armature_parent_orientation(ob, r_mat);
          break;
        }
        handle_object_parent_orientation(ob, r_mat);
        break;
      }
      /* No break; we define 'parent' as 'normal' otherwise. */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_NORMAL: {
      if (obedit || (ob && ob->mode & OB_MODE_POSE)) {
        ED_getTransformOrientationMatrix(scene, view_layer, v3d, ob, obedit, pivot_point, r_mat);
        break;
      }
      /* No break we define 'normal' as 'local' in Object mode. */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_LOCAL: {
      if (ob) {
        if (ob->mode & OB_MODE_POSE) {
          /* Each bone moves on its own local axis, but to avoid confusion,
           * use the active bone's axis for display #33575, this works as expected on a single
           * bone and users who select many bones will understand what's going on and what local
           * means when they start transforming. */
          ED_getTransformOrientationMatrix(scene, view_layer, v3d, ob, obedit, pivot_point, r_mat);
        }
        else {
          transform_orientations_create_from_axis(r_mat, UNPACK3(ob->object_to_world().ptr()));
        }
        break;
      }
      /* If not local, fall through to global. */
      ATTR_FALLTHROUGH;
    }
    case V3D_ORIENT_GLOBAL: {
      unit_m3(r_mat);
      break;
    }
    case V3D_ORIENT_VIEW: {
      if (rv3d != nullptr) {
        copy_m3_m4(r_mat, rv3d->viewinv);
        normalize_m3(r_mat);
      }
      else {
        unit_m3(r_mat);
      }
      break;
    }
    case V3D_ORIENT_CURSOR: {
      copy_m3_m3(r_mat, scene->cursor.matrix<float3x3>().ptr());
      break;
    }
    case V3D_ORIENT_CUSTOM_MATRIX: {
      /* Do nothing. */;
      break;
    }
    case V3D_ORIENT_CUSTOM:
    default: {
      BLI_assert(orientation_index >= V3D_ORIENT_CUSTOM);
      int orientation_index_custom = orientation_index - V3D_ORIENT_CUSTOM;
      TransformOrientation *custom_orientation = BKE_scene_transform_orientation_find(
          scene, orientation_index_custom);
      applyTransformOrientation(custom_orientation, r_mat, nullptr);
      break;
    }
  }

  return orientation_index;
}

short transform_orientation_matrix_get(bContext *C,
                                       TransInfo *t,
                                       short orient_index,
                                       const float custom[3][3],
                                       float r_spacemtx[3][3])
{
  if (orient_index == V3D_ORIENT_CUSTOM_MATRIX) {
    copy_m3_m3(r_spacemtx, custom);
    return V3D_ORIENT_CUSTOM_MATRIX;
  }

  if (t->spacetype == SPACE_SEQ && t->options & CTX_SEQUENCER_IMAGE) {
    Scene *scene = t->scene;
    Strip *strip = seq::select_active_get(scene);
    if (strip && strip->data->transform && orient_index == V3D_ORIENT_LOCAL) {
      const float2 mirror = seq::image_transform_mirror_factor_get(strip);
      axis_angle_to_mat3_single(
          r_spacemtx, 'Z', strip->data->transform->rotation * mirror[0] * mirror[1]);
      return orient_index;
    }
  }

  Object *ob = CTX_data_active_object(C);
  Object *obedit = CTX_data_edit_object(C);
  Scene *scene = t->scene;
  View3D *v3d = nullptr;
  RegionView3D *rv3d = nullptr;

  if ((t->spacetype == SPACE_VIEW3D) && t->region && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    v3d = static_cast<View3D *>(t->view);
    rv3d = static_cast<RegionView3D *>(t->region->regiondata);

    if (ob && (ob->mode & OB_MODE_ALL_WEIGHT_PAINT) && !(t->options & CTX_PAINT_CURVE)) {
      Object *ob_armature = transform_object_deform_pose_armature_get(t, ob);
      if (ob_armature) {
        /* The armature matrix is used for GIMBAL, NORMAL and LOCAL orientations. */
        ob = ob_armature;
      }
    }
  }

  const short orient_index_result = calc_orientation_from_type_ex(
      scene, t->view_layer, v3d, rv3d, ob, obedit, orient_index, t->around, r_spacemtx);

  if (rv3d && (t->options & CTX_PAINT_CURVE)) {
    /* Screen space in the 3d region. */
    if (orient_index_result == V3D_ORIENT_VIEW) {
      unit_m3(r_spacemtx);
    }
    else {
      mul_m3_m4m3(r_spacemtx, rv3d->viewmat, r_spacemtx);
      normalize_m3(r_spacemtx);
    }
  }

  return orient_index_result;
}

const char *transform_orientations_spacename_get(TransInfo *t, const short orient_type)
{
  switch (orient_type) {
    case V3D_ORIENT_GLOBAL:
      return RPT_("global");
    case V3D_ORIENT_GIMBAL:
      return RPT_("gimbal");
    case V3D_ORIENT_NORMAL:
      return RPT_("normal");
    case V3D_ORIENT_LOCAL:
      return RPT_("local");
    case V3D_ORIENT_VIEW:
      return RPT_("view");
    case V3D_ORIENT_CURSOR:
      return RPT_("cursor");
    case V3D_ORIENT_PARENT:
      return RPT_("parent");
    case V3D_ORIENT_CUSTOM_MATRIX:
      return RPT_("custom");
    case V3D_ORIENT_CUSTOM:
    default:
      BLI_assert(orient_type >= V3D_ORIENT_CUSTOM);
      TransformOrientation *ts = BKE_scene_transform_orientation_find(
          t->scene, orient_type - V3D_ORIENT_CUSTOM);
      return ts->name;
  }
}

void transform_orientations_current_set(TransInfo *t, const short orient_index)
{
  const short orientation = t->orient[orient_index].type;
  const char *spacename = transform_orientations_spacename_get(t, orientation);

  STRNCPY_UTF8(t->spacename, spacename);
  copy_m3_m3(t->spacemtx, t->orient[orient_index].matrix);
  invert_m3_m3_safe_ortho(t->spacemtx_inv, t->spacemtx);
  t->orient_curr = eTOType(orient_index);
}

/**
 * Utility function - get first n, selected vert/edge/faces.
 */
static uint bm_mesh_elems_select_get_n__internal(
    BMesh *bm, BMElem **elems, const uint n, const BMIterType itype, const char htype)
{
  BMIter iter;
  BMElem *ele;
  uint i;

  BLI_assert(ELEM(htype, BM_VERT, BM_EDGE, BM_FACE));
  BLI_assert(ELEM(itype, BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH));

  if (!BLI_listbase_is_empty(&bm->selected)) {
    /* Quick check. */
    i = 0;
    LISTBASE_FOREACH_BACKWARD (BMEditSelection *, ese, &bm->selected) {
      /* Shouldn't need this check. */
      if (BM_elem_flag_test(ese->ele, BM_ELEM_SELECT)) {

        /* Only use contiguous selection. */
        if (ese->htype != htype) {
          i = 0;
          break;
        }

        elems[i++] = ese->ele;
        if (n == i) {
          break;
        }
      }
      else {
        BLI_assert(0);
      }
    }

    if (i == 0) {
      /* Pass. */
    }
    else if (i == n) {
      return i;
    }
  }

  i = 0;
  BM_ITER_MESH (ele, &iter, bm, itype) {
    BLI_assert(ele->head.htype == htype);
    if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
      elems[i++] = ele;
      if (n == i) {
        break;
      }
    }
  }

  return i;
}

static uint bm_mesh_verts_select_get_n(BMesh *bm, BMVert **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totvertsel), BM_VERTS_OF_MESH, BM_VERT);
}
static uint bm_mesh_edges_select_get_n(BMesh *bm, BMEdge **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totedgesel), BM_EDGES_OF_MESH, BM_EDGE);
}
#if 0
static uint bm_mesh_faces_select_get_n(BMesh *bm, BMVert **elems, const uint n)
{
  return bm_mesh_elems_select_get_n__internal(
      bm, (BMElem **)elems, min_ii(n, bm->totfacesel), BM_FACES_OF_MESH, BM_FACE);
}
#endif

int getTransformOrientation_ex(const Scene *scene,
                               ViewLayer *view_layer,
                               const View3D *v3d,
                               Object *ob,
                               Object *obedit,
                               const short around,
                               float r_normal[3],
                               float r_plane[3])
{
  int result = ORIENTATION_NONE;
  const bool activeOnly = (around == V3D_AROUND_ACTIVE);

  zero_v3(r_normal);
  zero_v3(r_plane);

  if (obedit) {
    float imat[3][3], mat[3][3];

    /* We need the transpose of the inverse for a normal... */
    copy_m3_m4(imat, ob->object_to_world().ptr());

    invert_m3_m3(mat, imat);
    transpose_m3(mat);

    ob = obedit;

    if (ob->type == OB_MESH) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      BMEditSelection ese;

      /* Use last selected with active. */
      if (activeOnly && BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_normal(&ese, r_normal);
        BM_editselection_plane(&ese, r_plane);

        switch (ese.htype) {
          case BM_VERT:
            result = ORIENTATION_VERT;
            break;
          case BM_EDGE:
            result = ORIENTATION_EDGE;
            break;
          case BM_FACE:
            result = ORIENTATION_FACE;
            break;
        }
      }
      else {
        if (em->bm->totfacesel >= 1) {
          BMFace *efa;
          BMIter iter;

          float normal[3] = {0.0f};
          float plane_pair[2][3] = {{0.0f}};
          int face_count = 0;

          BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
            if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
              float tangent_pair[2][3];
              BM_face_calc_tangent_pair_auto(efa, tangent_pair[0], tangent_pair[1]);
              add_v3_v3(normal, efa->no);
              add_v3_v3(plane_pair[0], tangent_pair[0]);
              add_v3_v3(plane_pair[1], tangent_pair[1]);
              face_count++;
            }
          }

          /* Pick the best plane (least likely to be co-linear),
           * since this can result in failure to construct a usable matrix, see: #96535. */
          int plane_index;
          if (face_count == 1) {
            /* Special case so a single face always matches
             * the active-element orientation, see: #134948. */
            plane_index = 0;
          }
          else {
            float normal_unit[3];
            float plane_unit_pair[2][3], plane_ortho_pair[2][3];

            normalize_v3_v3(normal_unit, normal);
            normalize_v3_v3(plane_unit_pair[0], plane_pair[0]);
            normalize_v3_v3(plane_unit_pair[1], plane_pair[1]);

            cross_v3_v3v3(plane_ortho_pair[0], normal_unit, plane_unit_pair[0]);
            cross_v3_v3v3(plane_ortho_pair[1], normal_unit, plane_unit_pair[1]);

            plane_index = (len_squared_v3(plane_ortho_pair[0]) >
                           len_squared_v3(plane_ortho_pair[1])) ?
                              0 :
                              1;
          }

          add_v3_v3(r_normal, normal);
          add_v3_v3(r_plane, plane_pair[plane_index]);

          result = ORIENTATION_FACE;
        }
        else if (em->bm->totvertsel == 3) {
          BMVert *v_tri[3];

          if (bm_mesh_verts_select_get_n(em->bm, v_tri, 3) == 3) {
            BMEdge *e = nullptr;
            float no_test[3];

            normal_tri_v3(r_normal, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co);

            /* Check if the normal is pointing opposite to vert normals. */
            no_test[0] = v_tri[0]->no[0] + v_tri[1]->no[0] + v_tri[2]->no[0];
            no_test[1] = v_tri[0]->no[1] + v_tri[1]->no[1] + v_tri[2]->no[1];
            no_test[2] = v_tri[0]->no[2] + v_tri[1]->no[2] + v_tri[2]->no[2];
            if (dot_v3v3(no_test, r_normal) < 0.0f) {
              negate_v3(r_normal);
            }

            if (em->bm->totedgesel >= 1) {
              /* Find an edge that's a part of v_tri (no need to search all edges). */
              float e_length;
              int j;

              for (j = 0; j < 3; j++) {
                BMEdge *e_test = BM_edge_exists(v_tri[j], v_tri[(j + 1) % 3]);
                if (e_test && BM_elem_flag_test(e_test, BM_ELEM_SELECT)) {
                  const float e_test_length = BM_edge_calc_length_squared(e_test);
                  if ((e == nullptr) || (e_length < e_test_length)) {
                    e = e_test;
                    e_length = e_test_length;
                  }
                }
              }
            }

            if (e) {
              BMVert *v_pair[2];
              if (BM_edge_is_boundary(e)) {
                BM_edge_ordered_verts(e, &v_pair[0], &v_pair[1]);
              }
              else {
                v_pair[0] = e->v1;
                v_pair[1] = e->v2;
              }
              sub_v3_v3v3(r_plane, v_pair[0]->co, v_pair[1]->co);
            }
            else {
              BM_vert_tri_calc_tangent_from_edge(v_tri, r_plane);
            }
          }
          else {
            BLI_assert(0);
          }

          result = ORIENTATION_FACE;
        }
        else if (em->bm->totedgesel == 1 || em->bm->totvertsel == 2) {
          BMVert *v_pair[2] = {nullptr, nullptr};
          BMEdge *eed = nullptr;

          if (em->bm->totedgesel == 1) {
            if (bm_mesh_edges_select_get_n(em->bm, &eed, 1) == 1) {
              v_pair[0] = eed->v1;
              v_pair[1] = eed->v2;
            }
          }
          else {
            BLI_assert(em->bm->totvertsel == 2);
            bm_mesh_verts_select_get_n(em->bm, v_pair, 2);
          }

          /* Should never fail. */
          if (LIKELY(v_pair[0] && v_pair[1])) {
            bool v_pair_swap = false;
            /**
             * Logic explained:
             *
             * - Edges and vert-pairs treated the same way.
             * - Point the Y axis along the edge vector (towards the active vertex).
             * - Point the Z axis outwards (the same direction as the normals).
             *
             * \note Z points outwards - along the normal.
             * take care making changes here, see: #38592, #43708
             */

            /* Be deterministic where possible and ensure `v_pair[0]` is active. */
            if (BM_mesh_active_vert_get(em->bm) == v_pair[1]) {
              v_pair_swap = true;
            }
            else if (eed && BM_edge_is_boundary(eed)) {
              /* Predictable direction for boundary edges. */
              if (eed->l->v != v_pair[0]) {
                v_pair_swap = true;
              }
            }

            if (v_pair_swap) {
              std::swap(v_pair[0], v_pair[1]);
            }

            add_v3_v3v3(r_normal, v_pair[1]->no, v_pair[0]->no);
            sub_v3_v3v3(r_plane, v_pair[1]->co, v_pair[0]->co);

            if (normalize_v3(r_plane) != 0.0f) {
              /* For edges it'd important the resulting matrix can rotate around the edge,
               * project onto the plane so we can use a fallback value. */
              project_plane_normalized_v3_v3v3(r_normal, r_normal, r_plane);
              if (UNLIKELY(normalize_v3(r_normal) == 0.0f)) {
                /* In the case the normal and plane are aligned,
                 * use a fallback normal which is orthogonal to the plane. */
                ortho_v3_v3(r_normal, r_plane);
              }
            }
          }

          result = ORIENTATION_EDGE;
        }
        else if (em->bm->totvertsel == 1) {
          BMVert *v = nullptr;

          if (bm_mesh_verts_select_get_n(em->bm, &v, 1) == 1) {
            copy_v3_v3(r_normal, v->no);
            BMEdge *e_pair[2];

            if (BM_vert_edge_pair(v, &e_pair[0], &e_pair[1])) {
              bool v_pair_swap = false;
              BMVert *v_pair[2] = {
                  BM_edge_other_vert(e_pair[0], v),
                  BM_edge_other_vert(e_pair[1], v),
              };
              float dir_pair[2][3];

              if (BM_edge_is_boundary(e_pair[0])) {
                if (e_pair[0]->l->v != v) {
                  v_pair_swap = true;
                }
              }
              else {
                if (BM_edge_calc_length_squared(e_pair[0]) <
                    BM_edge_calc_length_squared(e_pair[1]))
                {
                  v_pair_swap = true;
                }
              }

              if (v_pair_swap) {
                std::swap(v_pair[0], v_pair[1]);
              }

              sub_v3_v3v3(dir_pair[0], v->co, v_pair[0]->co);
              sub_v3_v3v3(dir_pair[1], v_pair[1]->co, v->co);
              normalize_v3(dir_pair[0]);
              normalize_v3(dir_pair[1]);

              add_v3_v3v3(r_plane, dir_pair[0], dir_pair[1]);
            }
          }

          result = is_zero_v3(r_plane) ? ORIENTATION_VERT : ORIENTATION_EDGE;
        }
        else if (em->bm->totvertsel > 3) {
          BMIter iter;
          BMVert *v;

          zero_v3(r_normal);

          BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
            if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
              add_v3_v3(r_normal, v->no);
            }
          }
          normalize_v3(r_normal);
          result = ORIENTATION_VERT;
        }
      }

      /* Not needed but this matches 2.68 and older behavior. */
      negate_v3(r_plane);

    } /* End edit-mesh. */
    else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
      Curve *cu = static_cast<Curve *>(obedit->data);
      Nurb *nu = nullptr;
      int a;
      ListBase *nurbs = BKE_curve_editNurbs_get(cu);

      void *vert_act = nullptr;
      if (activeOnly && BKE_curve_nurb_vert_active_get(cu, &nu, &vert_act)) {
        if (nu->type == CU_BEZIER) {
          BezTriple *bezt = static_cast<BezTriple *>(vert_act);
          BKE_nurb_bezt_calc_normal(nu, bezt, r_normal);
          BKE_nurb_bezt_calc_plane(nu, bezt, r_plane);
        }
        else {
          BPoint *bp = static_cast<BPoint *>(vert_act);
          BKE_nurb_bpoint_calc_normal(nu, bp, r_normal);
          BKE_nurb_bpoint_calc_plane(nu, bp, r_plane);
        }
      }
      else {
        const bool use_handle = v3d ? (v3d->overlay.handle_display != CURVE_HANDLE_NONE) : true;

        for (nu = static_cast<Nurb *>(nurbs->first); nu; nu = nu->next) {
          /* Only bezier has a normal. */
          if (nu->type == CU_BEZIER) {
            BezTriple *bezt = nu->bezt;
            a = nu->pntsu;
            while (a--) {
              short flag = 0;

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

              if (use_handle) {
                if (bezt->f1 & SELECT) {
                  flag |= SEL_F1;
                }
                if (bezt->f2 & SELECT) {
                  flag |= SEL_F2;
                }
                if (bezt->f3 & SELECT) {
                  flag |= SEL_F3;
                }
              }
              else {
                flag = (bezt->f2 & SELECT) ? (SEL_F1 | SEL_F2 | SEL_F3) : 0;
              }

              /* Exception. */
              if (flag) {
                float tvec[3];
                if ((around == V3D_AROUND_LOCAL_ORIGINS) ||
                    ELEM(flag, SEL_F2, SEL_F1 | SEL_F3, SEL_F1 | SEL_F2 | SEL_F3))
                {
                  BKE_nurb_bezt_calc_normal(nu, bezt, tvec);
                  add_v3_v3(r_normal, tvec);
                }
                else {
                  /* Ignore `bezt->f2` in this case. */
                  if (flag & SEL_F1) {
                    sub_v3_v3v3(tvec, bezt->vec[0], bezt->vec[1]);
                    normalize_v3(tvec);
                    add_v3_v3(r_normal, tvec);
                  }
                  if (flag & SEL_F3) {
                    sub_v3_v3v3(tvec, bezt->vec[1], bezt->vec[2]);
                    normalize_v3(tvec);
                    add_v3_v3(r_normal, tvec);
                  }
                }

                BKE_nurb_bezt_calc_plane(nu, bezt, tvec);
                add_v3_v3(r_plane, tvec);
              }

#undef SEL_F1
#undef SEL_F2
#undef SEL_F3

              bezt++;
            }
          }
          else if (nu->bp && (nu->pntsv == 1)) {
            BPoint *bp = nu->bp;
            a = nu->pntsu;
            while (a--) {
              if (bp->f1 & SELECT) {
                float tvec[3];

                BPoint *bp_prev = BKE_nurb_bpoint_get_prev(nu, bp);
                BPoint *bp_next = BKE_nurb_bpoint_get_next(nu, bp);

                const bool is_prev_sel = bp_prev && (bp_prev->f1 & SELECT);
                const bool is_next_sel = bp_next && (bp_next->f1 & SELECT);
                if (is_prev_sel == false && is_next_sel == false) {
                  /* Isolated, add based on surrounding. */
                  BKE_nurb_bpoint_calc_normal(nu, bp, tvec);
                  add_v3_v3(r_normal, tvec);
                }
                else if (is_next_sel) {
                  /* A segment, add the edge normal. */
                  sub_v3_v3v3(tvec, bp->vec, bp_next->vec);
                  normalize_v3(tvec);
                  add_v3_v3(r_normal, tvec);
                }

                BKE_nurb_bpoint_calc_plane(nu, bp, tvec);
                add_v3_v3(r_plane, tvec);
              }
              bp++;
            }
          }
        }
      }

      if (!is_zero_v3(r_normal)) {
        result = ORIENTATION_FACE;
      }
    }
    else if (obedit->type == OB_MBALL) {
      MetaBall *mb = static_cast<MetaBall *>(obedit->data);
      MetaElem *ml;
      bool ok = false;
      float tmat[3][3];

      if (activeOnly && (ml = mb->lastelem)) {
        quat_to_mat3(tmat, ml->quat);
        add_v3_v3(r_normal, tmat[2]);
        add_v3_v3(r_plane, tmat[1]);
        ok = true;
      }
      else {
        LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
          if (ml->flag & SELECT) {
            quat_to_mat3(tmat, ml->quat);
            add_v3_v3(r_normal, tmat[2]);
            add_v3_v3(r_plane, tmat[1]);
            ok = true;
          }
        }
      }

      if (ok) {
        if (!is_zero_v3(r_plane)) {
          result = ORIENTATION_FACE;
        }
      }
    }
    else if (obedit->type == OB_ARMATURE) {
      bArmature *arm = static_cast<bArmature *>(obedit->data);
      EditBone *ebone;
      bool ok = false;
      float tmat[3][3];

      if (activeOnly && (ebone = arm->act_edbone)) {
        ED_armature_ebone_to_mat3(ebone, tmat);
        add_v3_v3(r_normal, tmat[2]);
        add_v3_v3(r_plane, tmat[1]);
        ok = true;
      }
      else {
        /* When we only have the root/tip are selected. */
        bool fallback_ok = false;
        float fallback_normal[3];
        float fallback_plane[3];

        zero_v3(fallback_normal);
        zero_v3(fallback_plane);

        LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
          if (blender::animrig::bone_is_visible(arm, ebone)) {
            if (ebone->flag & BONE_SELECTED) {
              ED_armature_ebone_to_mat3(ebone, tmat);
              add_v3_v3(r_normal, tmat[2]);
              add_v3_v3(r_plane, tmat[1]);
              ok = true;
            }
            else if ((ok == false) && ((ebone->flag & BONE_TIPSEL) ||
                                       ((ebone->flag & BONE_ROOTSEL) &&
                                        (ebone->parent && ebone->flag & BONE_CONNECTED) == false)))
            {
              ED_armature_ebone_to_mat3(ebone, tmat);
              add_v3_v3(fallback_normal, tmat[2]);
              add_v3_v3(fallback_plane, tmat[1]);
              fallback_ok = true;
            }
          }
        }
        if ((ok == false) && fallback_ok) {
          ok = true;
          copy_v3_v3(r_normal, fallback_normal);
          copy_v3_v3(r_plane, fallback_plane);
        }
      }

      if (ok) {
        if (!is_zero_v3(r_plane)) {
          result = ORIENTATION_EDGE;
        }
      }
    }

    /* Vectors from edges don't need the special transpose inverse multiplication. */
    if (result == ORIENTATION_EDGE) {
      float tvec[3];

      mul_mat3_m4_v3(ob->object_to_world().ptr(), r_normal);
      mul_mat3_m4_v3(ob->object_to_world().ptr(), r_plane);

      /* Align normal to edge direction (so normal is perpendicular to the plane).
       * 'ORIENTATION_EDGE' will do the other way around.
       * This has to be done **after** applying obmat, see #45775! */
      project_v3_v3v3(tvec, r_normal, r_plane);
      sub_v3_v3(r_normal, tvec);
    }
    else {
      mul_m3_v3(mat, r_normal);
      mul_m3_v3(mat, r_plane);
    }
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    bPoseChannel *pchan;
    float imat[3][3], mat[3][3];
    bool ok = false;

    if (activeOnly && (pchan = BKE_pose_channel_active_if_bonecoll_visible(ob))) {
      float pose_mat[3][3];
      BKE_pose_channel_transform_orientation(arm, pchan, pose_mat);

      add_v3_v3(r_normal, pose_mat[2]);
      add_v3_v3(r_plane, pose_mat[1]);
      ok = true;
    }
    else {
      const int transformed_len = armature_bone_transflags_update(*ob, arm, &ob->pose->chanbase);
      if (transformed_len) {
        /* Use channels to get stats. */
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          if (pchan->runtime.flag & POSE_RUNTIME_TRANSFORM) {
            float pose_mat[3][3];
            BKE_pose_channel_transform_orientation(arm, pchan, pose_mat);

            add_v3_v3(r_normal, pose_mat[2]);
            add_v3_v3(r_plane, pose_mat[1]);
          }
        }
        ok = true;
      }
    }

    /* Use for both active & all. */
    if (ok) {
      /* We need the transpose of the inverse for a normal. */
      copy_m3_m4(imat, ob->object_to_world().ptr());

      invert_m3_m3(mat, imat);
      transpose_m3(mat);
      mul_m3_v3(mat, r_normal);
      mul_m3_v3(mat, r_plane);

      result = ORIENTATION_EDGE;
    }
  }
  else {
    /* We need the one selected object, if its not active. */
    if (ob != nullptr) {
      bool ok = false;
      if (activeOnly || (ob->mode & (OB_MODE_ALL_PAINT | OB_MODE_PARTICLE_EDIT))) {
        /* Ignore selection state. */
        ok = true;
      }
      else {
        BKE_view_layer_synced_ensure(scene, view_layer);
        Base *base = BKE_view_layer_base_find(view_layer, ob);
        if (UNLIKELY(base == nullptr)) {
          /* This is very unlikely, if it happens allow the value to be set since the caller
           * may have taken the object from outside this view-layer. */
          ok = true;
        }
        else if (BASE_SELECTED(v3d, base)) {
          ok = true;
        }
      }

      if (ok) {
        copy_v3_v3(r_normal, ob->object_to_world().ptr()[2]);
        copy_v3_v3(r_plane, ob->object_to_world().ptr()[1]);
      }
    }
    result = ORIENTATION_NORMAL;
  }

  normalize_v3(r_normal);
  normalize_v3(r_plane);

  return result;
}

int getTransformOrientation(const bContext *C, float r_normal[3], float r_plane[3])
{
  Object *obact = CTX_data_active_object(C);
  Object *obedit = CTX_data_edit_object(C);

  /* Dummy value, not #V3D_AROUND_ACTIVE and not #V3D_AROUND_LOCAL_ORIGINS. */
  short around = V3D_AROUND_CENTER_BOUNDS;

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  return getTransformOrientation_ex(
      scene, view_layer, v3d, obact, obedit, around, r_normal, r_plane);
}

void ED_getTransformOrientationMatrix(const Scene *scene,
                                      ViewLayer *view_layer,
                                      const View3D *v3d,
                                      Object *ob,
                                      Object *obedit,
                                      const short around,
                                      float r_orientation_mat[3][3])
{
  float normal[3] = {0.0, 0.0, 0.0};
  float plane[3] = {0.0, 0.0, 0.0};

  int type;

  type = getTransformOrientation_ex(scene, view_layer, v3d, ob, obedit, around, normal, plane);

  /* Fallback, when the plane can't be calculated. */
  if (ORIENTATION_USE_PLANE(type) && is_zero_v3(plane)) {
    type = ORIENTATION_VERT;
  }

  switch (type) {
    case ORIENTATION_NORMAL:
      if (createSpaceNormalTangent(r_orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_VERT:
      if (createSpaceNormal(r_orientation_mat, normal) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_EDGE:
      if (createSpaceNormalTangent(r_orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    case ORIENTATION_FACE:
      if (createSpaceNormalTangent(r_orientation_mat, normal, plane) == 0) {
        type = ORIENTATION_NONE;
      }
      break;
    default:
      BLI_assert(type == ORIENTATION_NONE);
      break;
  }

  if (type == ORIENTATION_NONE) {
    unit_m3(r_orientation_mat);
  }
}

}  // namespace blender::ed::transform
