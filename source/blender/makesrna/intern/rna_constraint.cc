/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_math_rotation.h"

#include "BLT_translation.hh"

#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

#include "ED_object.hh"

/* Please keep the names in sync with `constraint.cc`. */
const EnumPropertyItem rna_enum_constraint_type_items[] = {
    RNA_ENUM_ITEM_HEADING(N_("Motion Tracking"), nullptr),
    {CONSTRAINT_TYPE_CAMERASOLVER, "CAMERA_SOLVER", ICON_CON_CAMERASOLVER, "Camera Solver", ""},
    {CONSTRAINT_TYPE_FOLLOWTRACK, "FOLLOW_TRACK", ICON_CON_FOLLOWTRACK, "Follow Track", ""},
    {CONSTRAINT_TYPE_OBJECTSOLVER, "OBJECT_SOLVER", ICON_CON_OBJECTSOLVER, "Object Solver", ""},

    RNA_ENUM_ITEM_HEADING(N_("Transform"), nullptr),
    {CONSTRAINT_TYPE_LOCLIKE,
     "COPY_LOCATION",
     ICON_CON_LOCLIKE,
     "Copy Location",
     "Copy the location of a target (with an optional offset), so that they move together"},
    {CONSTRAINT_TYPE_ROTLIKE,
     "COPY_ROTATION",
     ICON_CON_ROTLIKE,
     "Copy Rotation",
     "Copy the rotation of a target (with an optional offset), so that they rotate together"},
    {CONSTRAINT_TYPE_SIZELIKE,
     "COPY_SCALE",
     ICON_CON_SIZELIKE,
     "Copy Scale",
     "Copy the scale factors of a target (with an optional offset), so that they are scaled by "
     "the same amount"},
    {CONSTRAINT_TYPE_TRANSLIKE,
     "COPY_TRANSFORMS",
     ICON_CON_TRANSLIKE,
     "Copy Transforms",
     "Copy all the transformations of a target, so that they move together"},
    {CONSTRAINT_TYPE_DISTLIMIT,
     "LIMIT_DISTANCE",
     ICON_CON_DISTLIMIT,
     "Limit Distance",
     "Restrict movements to within a certain distance of a target (at the time of constraint "
     "evaluation only)"},
    {CONSTRAINT_TYPE_LOCLIMIT,
     "LIMIT_LOCATION",
     ICON_CON_LOCLIMIT,
     "Limit Location",
     "Restrict movement along each axis within given ranges"},
    {CONSTRAINT_TYPE_ROTLIMIT,
     "LIMIT_ROTATION",
     ICON_CON_ROTLIMIT,
     "Limit Rotation",
     "Restrict rotation along each axis within given ranges"},
    {CONSTRAINT_TYPE_SIZELIMIT,
     "LIMIT_SCALE",
     ICON_CON_SIZELIMIT,
     "Limit Scale",
     "Restrict scaling along each axis with given ranges"},
    {CONSTRAINT_TYPE_SAMEVOL,
     "MAINTAIN_VOLUME",
     ICON_CON_SAMEVOL,
     "Maintain Volume",
     "Compensate for scaling one axis by applying suitable scaling to the other two axes"},
    {CONSTRAINT_TYPE_TRANSFORM,
     "TRANSFORM",
     ICON_CON_TRANSFORM,
     "Transformation",
     "Use one transform property from target to control another (or same) property on owner"},
    {CONSTRAINT_TYPE_TRANSFORM_CACHE,
     "TRANSFORM_CACHE",
     ICON_CON_TRANSFORM_CACHE,
     "Transform Cache",
     "Look up the transformation matrix from an external file"},

    RNA_ENUM_ITEM_HEADING(N_("Tracking"), nullptr),
    {CONSTRAINT_TYPE_CLAMPTO,
     "CLAMP_TO",
     ICON_CON_CLAMPTO,
     "Clamp To",
     "Restrict movements to lie along a curve by remapping location along curve's longest axis"},
    {CONSTRAINT_TYPE_DAMPTRACK,
     "DAMPED_TRACK",
     ICON_CON_TRACKTO,
     "Damped Track",
     "Point towards a target by performing the smallest rotation necessary"},
    {CONSTRAINT_TYPE_KINEMATIC,
     "IK",
     ICON_CON_KINEMATIC,
     "Inverse Kinematics",
     "Control a chain of bones by specifying the endpoint target (Bones only)"},
    {CONSTRAINT_TYPE_LOCKTRACK,
     "LOCKED_TRACK",
     ICON_CON_LOCKTRACK,
     "Locked Track",
     "Rotate around the specified ('locked') axis to point towards a target"},
    {CONSTRAINT_TYPE_SPLINEIK,
     "SPLINE_IK",
     ICON_CON_SPLINEIK,
     "Spline IK",
     "Align chain of bones along a curve (Bones only)"},
    {CONSTRAINT_TYPE_STRETCHTO,
     "STRETCH_TO",
     ICON_CON_STRETCHTO,
     "Stretch To",
     "Stretch along Y-Axis to point towards a target"},
    {CONSTRAINT_TYPE_TRACKTO,
     "TRACK_TO",
     ICON_CON_TRACKTO,
     "Track To",
     "Legacy tracking constraint prone to twisting artifacts"},

    RNA_ENUM_ITEM_HEADING(N_("Relationship"), nullptr),
    {CONSTRAINT_TYPE_ACTION,
     "ACTION",
     ICON_ACTION,
     "Action",
     "Use transform property of target to look up pose for owner from an Action"},
    {CONSTRAINT_TYPE_ARMATURE,
     "ARMATURE",
     ICON_CON_ARMATURE,
     "Armature",
     "Apply weight-blended transformation from multiple bones like the Armature modifier"},
    {CONSTRAINT_TYPE_CHILDOF,
     "CHILD_OF",
     ICON_CON_CHILDOF,
     "Child Of",
     "Make target the 'detachable' parent of owner"},
    {CONSTRAINT_TYPE_MINMAX,
     "FLOOR",
     ICON_CON_FLOOR,
     "Floor",
     "Use position (and optionally rotation) of target to define a 'wall' or 'floor' that the "
     "owner cannot cross"},
    {CONSTRAINT_TYPE_FOLLOWPATH,
     "FOLLOW_PATH",
     ICON_CON_FOLLOWPATH,
     "Follow Path",
     "Use to animate an object/bone following a path"},
    {CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE,
     "GEOMETRY_ATTRIBUTE",
     ICON_CON_GEOMETRYATTRIBUTE,
     "Geometry Attribute",
     "Retrieve transform from target geometry attribute data"},
    {CONSTRAINT_TYPE_PIVOT,
     "PIVOT",
     ICON_CON_PIVOT,
     "Pivot",
     "Change pivot point for transforms (buggy)"},
    {CONSTRAINT_TYPE_SHRINKWRAP,
     "SHRINKWRAP",
     ICON_CON_SHRINKWRAP,
     "Shrinkwrap",
     "Restrict movements to surface of target mesh"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem target_space_pchan_items[] = {
    {CONSTRAINT_SPACE_WORLD,
     "WORLD",
     0,
     "World Space",
     "The transformation of the target is evaluated relative to the world "
     "coordinate system"},
    {CONSTRAINT_SPACE_CUSTOM,
     "CUSTOM",
     0,
     "Custom Space",
     "The transformation of the target is evaluated relative to a custom object/bone/vertex "
     "group"},
    RNA_ENUM_ITEM_SEPR,
    {CONSTRAINT_SPACE_POSE,
     "POSE",
     0,
     "Pose Space",
     "The transformation of the target is only evaluated in the Pose Space, "
     "the target armature object transformation is ignored"},
    {CONSTRAINT_SPACE_PARLOCAL,
     "LOCAL_WITH_PARENT",
     0,
     "Local With Parent",
     "The transformation of the target bone is evaluated relative to its rest pose "
     "local coordinate system, thus including the parent-induced transformation"},
    {CONSTRAINT_SPACE_LOCAL,
     "LOCAL",
     0,
     "Local Space",
     "The transformation of the target is evaluated relative to its local "
     "coordinate system"},
    {CONSTRAINT_SPACE_OWNLOCAL,
     "LOCAL_OWNER_ORIENT",
     0,
     "Local Space (Owner Orientation)",
     "The transformation of the target bone is evaluated relative to its local coordinate "
     "system, followed by a correction for the difference in target and owner rest pose "
     "orientations. When applied as local transform to the owner produces the same global "
     "motion as the target if the parents are still in rest pose."},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem owner_space_pchan_items[] = {
    {CONSTRAINT_SPACE_WORLD,
     "WORLD",
     0,
     "World Space",
     "The constraint is applied relative to the world coordinate system"},
    {CONSTRAINT_SPACE_CUSTOM,
     "CUSTOM",
     0,
     "Custom Space",
     "The constraint is applied in local space of a custom object/bone/vertex group"},
    RNA_ENUM_ITEM_SEPR,
    {CONSTRAINT_SPACE_POSE,
     "POSE",
     0,
     "Pose Space",
     "The constraint is applied in Pose Space, the object transformation is ignored"},
    {CONSTRAINT_SPACE_PARLOCAL,
     "LOCAL_WITH_PARENT",
     0,
     "Local With Parent",
     "The constraint is applied relative to the rest pose local coordinate system "
     "of the bone, thus including the parent-induced transformation"},
    {CONSTRAINT_SPACE_LOCAL,
     "LOCAL",
     0,
     "Local Space",
     "The constraint is applied relative to the local coordinate system of the object"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem track_axis_items[] = {
    {TRACK_X, "TRACK_X", 0, "X", ""},
    {TRACK_Y, "TRACK_Y", 0, "Y", ""},
    {TRACK_Z, "TRACK_Z", 0, "Z", ""},
    {TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
    {TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
    {TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem euler_order_items[] = {
    {CONSTRAINT_EULER_AUTO, "AUTO", 0, "Default", "Euler using the default rotation order"},
    {CONSTRAINT_EULER_XYZ, "XYZ", 0, "XYZ Euler", "Euler using the XYZ rotation order"},
    {CONSTRAINT_EULER_XZY, "XZY", 0, "XZY Euler", "Euler using the XZY rotation order"},
    {CONSTRAINT_EULER_YXZ, "YXZ", 0, "YXZ Euler", "Euler using the YXZ rotation order"},
    {CONSTRAINT_EULER_YZX, "YZX", 0, "YZX Euler", "Euler using the YZX rotation order"},
    {CONSTRAINT_EULER_ZXY, "ZXY", 0, "ZXY Euler", "Euler using the ZXY rotation order"},
    {CONSTRAINT_EULER_ZYX, "ZYX", 0, "ZYX Euler", "Euler using the ZYX rotation order"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

static const EnumPropertyItem owner_space_object_items[] = {
    {CONSTRAINT_SPACE_WORLD,
     "WORLD",
     0,
     "World Space",
     "The constraint is applied relative to the world coordinate system"},
    {CONSTRAINT_SPACE_CUSTOM,
     "CUSTOM",
     0,
     "Custom Space",
     "The constraint is applied in local space of a custom object/bone/vertex group"},
    {CONSTRAINT_SPACE_LOCAL,
     "LOCAL",
     0,
     "Local Space",
     "The constraint is applied relative to the local coordinate system of the object"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem target_space_object_items[] = {
    {CONSTRAINT_SPACE_WORLD,
     "WORLD",
     0,
     "World Space",
     "The transformation of the target is evaluated relative to the world coordinate system"},
    {CONSTRAINT_SPACE_CUSTOM,
     "CUSTOM",
     0,
     "Custom Space",
     "The transformation of the target is evaluated relative to a custom object/bone/vertex "
     "group"},
    {CONSTRAINT_SPACE_LOCAL,
     "LOCAL",
     0,
     "Local Space",
     "The transformation of the target is evaluated relative to its local coordinate system"},
    {0, nullptr, 0, nullptr, nullptr},
};

#  include <fmt/format.h>

#  include "DNA_cachefile_types.h"

#  include "BKE_action.hh"
#  include "BKE_animsys.h"
#  include "BKE_constraint.h"
#  include "BKE_context.hh"

#  ifdef WITH_ALEMBIC
#    include "ABC_alembic.h"
#  endif

#  include "ANIM_action.hh"
#  include "rna_action_tools.hh"

static StructRNA *rna_ConstraintType_refine(PointerRNA *ptr)
{
  bConstraint *con = (bConstraint *)ptr->data;

  switch (con->type) {
    case CONSTRAINT_TYPE_CHILDOF:
      return &RNA_ChildOfConstraint;
    case CONSTRAINT_TYPE_TRACKTO:
      return &RNA_TrackToConstraint;
    case CONSTRAINT_TYPE_KINEMATIC:
      return &RNA_KinematicConstraint;
    case CONSTRAINT_TYPE_FOLLOWPATH:
      return &RNA_FollowPathConstraint;
    case CONSTRAINT_TYPE_ROTLIKE:
      return &RNA_CopyRotationConstraint;
    case CONSTRAINT_TYPE_LOCLIKE:
      return &RNA_CopyLocationConstraint;
    case CONSTRAINT_TYPE_SIZELIKE:
      return &RNA_CopyScaleConstraint;
    case CONSTRAINT_TYPE_SAMEVOL:
      return &RNA_MaintainVolumeConstraint;
    case CONSTRAINT_TYPE_ARMATURE:
      return &RNA_ArmatureConstraint;
    case CONSTRAINT_TYPE_ACTION:
      return &RNA_ActionConstraint;
    case CONSTRAINT_TYPE_LOCKTRACK:
      return &RNA_LockedTrackConstraint;
    case CONSTRAINT_TYPE_STRETCHTO:
      return &RNA_StretchToConstraint;
    case CONSTRAINT_TYPE_MINMAX:
      return &RNA_FloorConstraint;
    case CONSTRAINT_TYPE_CLAMPTO:
      return &RNA_ClampToConstraint;
    case CONSTRAINT_TYPE_TRANSFORM:
      return &RNA_TransformConstraint;
    case CONSTRAINT_TYPE_ROTLIMIT:
      return &RNA_LimitRotationConstraint;
    case CONSTRAINT_TYPE_LOCLIMIT:
      return &RNA_LimitLocationConstraint;
    case CONSTRAINT_TYPE_SIZELIMIT:
      return &RNA_LimitScaleConstraint;
    case CONSTRAINT_TYPE_DISTLIMIT:
      return &RNA_LimitDistanceConstraint;
    case CONSTRAINT_TYPE_SHRINKWRAP:
      return &RNA_ShrinkwrapConstraint;
    case CONSTRAINT_TYPE_DAMPTRACK:
      return &RNA_DampedTrackConstraint;
    case CONSTRAINT_TYPE_SPLINEIK:
      return &RNA_SplineIKConstraint;
    case CONSTRAINT_TYPE_TRANSLIKE:
      return &RNA_CopyTransformsConstraint;
    case CONSTRAINT_TYPE_PIVOT:
      return &RNA_PivotConstraint;
    case CONSTRAINT_TYPE_FOLLOWTRACK:
      return &RNA_FollowTrackConstraint;
    case CONSTRAINT_TYPE_CAMERASOLVER:
      return &RNA_CameraSolverConstraint;
    case CONSTRAINT_TYPE_OBJECTSOLVER:
      return &RNA_ObjectSolverConstraint;
    case CONSTRAINT_TYPE_TRANSFORM_CACHE:
      return &RNA_TransformCacheConstraint;
    case CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE:
      return &RNA_GeometryAttributeConstraint;
    default:
      return &RNA_UnknownType;
  }
}

static void rna_ConstraintTargetBone_target_set(PointerRNA *ptr,
                                                PointerRNA value,
                                                ReportList * /*reports*/)
{
  bConstraintTarget *tgt = (bConstraintTarget *)ptr->data;
  Object *ob = static_cast<Object *>(value.data);

  if (!ob || ob->type == OB_ARMATURE) {
    id_lib_extern((ID *)ob);
    tgt->tar = ob;
  }
}

static void rna_Constraint_name_set(PointerRNA *ptr, const char *value)
{
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  char oldname[sizeof(con->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, con->name);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(con->name, value);

  /* make sure name is unique */
  if (ptr->owner_id) {
    Object *ob = (Object *)ptr->owner_id;
    ListBase *list = blender::ed::object::constraint_list_from_constraint(ob, con, nullptr);

    /* if we have the list, check for unique name, otherwise give up */
    if (list) {
      BKE_constraint_unique_name(con, list);
    }
  }

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(nullptr, "constraints", oldname, con->name);
}

static std::optional<std::string> rna_Constraint_do_compute_path(Object *ob, bConstraint *con)
{
  bPoseChannel *pchan;
  ListBase *lb = blender::ed::object::constraint_list_from_constraint(ob, con, &pchan);

  if (lb == nullptr) {
    printf("%s: internal error, constraint '%s' not found in object '%s'\n",
           __func__,
           con->name,
           ob->id.name);
  }

  if (pchan) {
    char name_esc_pchan[sizeof(pchan->name) * 2];
    char name_esc_const[sizeof(con->name) * 2];
    BLI_str_escape(name_esc_pchan, pchan->name, sizeof(name_esc_pchan));
    BLI_str_escape(name_esc_const, con->name, sizeof(name_esc_const));
    return fmt::format("pose.bones[\"{}\"].constraints[\"{}\"]", name_esc_pchan, name_esc_const);
  }
  char name_esc_const[sizeof(con->name) * 2];
  BLI_str_escape(name_esc_const, con->name, sizeof(name_esc_const));
  return fmt::format("constraints[\"{}\"]", name_esc_const);
}

static std::optional<std::string> rna_Constraint_path(const PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  bConstraint *con = static_cast<bConstraint *>(ptr->data);

  return rna_Constraint_do_compute_path(ob, con);
}

static bConstraint *rna_constraint_from_target(const PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  bConstraintTarget *tgt = static_cast<bConstraintTarget *>(ptr->data);

  return BKE_constraint_find_from_target(ob, tgt, nullptr);
}

static std::optional<std::string> rna_ConstraintTarget_path(const PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  bConstraintTarget *tgt = static_cast<bConstraintTarget *>(ptr->data);
  bConstraint *con = rna_constraint_from_target(ptr);
  int index = -1;

  if (con != nullptr) {
    if (con->type == CONSTRAINT_TYPE_ARMATURE) {
      bArmatureConstraint *acon = static_cast<bArmatureConstraint *>(con->data);
      index = BLI_findindex(&acon->targets, tgt);
    }
  }

  if (index >= 0) {
    return fmt::format(
        "{}.targets[{}]", rna_Constraint_do_compute_path(ob, con).value_or(""), index);
  }
  printf("%s: internal error, constraint '%s' of object '%s' does not contain the target\n",
         __func__,
         con->name,
         ob->id.name);

  return std::nullopt;
}

static void rna_Constraint_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  blender::ed::object::constraint_tag_update(
      bmain, (Object *)ptr->owner_id, static_cast<bConstraint *>(ptr->data));
}

static void rna_Constraint_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  blender::ed::object::constraint_dependency_tag_update(
      bmain, (Object *)ptr->owner_id, static_cast<bConstraint *>(ptr->data));
}

static void rna_ConstraintTarget_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  blender::ed::object::constraint_tag_update(
      bmain, (Object *)ptr->owner_id, rna_constraint_from_target(ptr));
}

static void rna_ConstraintTarget_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  blender::ed::object::constraint_dependency_tag_update(
      bmain, (Object *)ptr->owner_id, rna_constraint_from_target(ptr));
}

static void rna_Constraint_influence_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  if (ob->pose) {
    ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);
  }

  rna_Constraint_update(bmain, scene, ptr);
}

/* Update only needed so this isn't overwritten on first evaluation. */
static void rna_Constraint_childof_inverse_matrix_update(Main *bmain,
                                                         Scene *scene,
                                                         PointerRNA *ptr)
{
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  bChildOfConstraint *data = static_cast<bChildOfConstraint *>(con->data);
  data->flag &= ~CHILDOF_SET_INVERSE;
  rna_Constraint_update(bmain, scene, ptr);
}

static void rna_Constraint_ik_type_set(PointerRNA *ptr, int value)
{
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  bKinematicConstraint *ikdata = static_cast<bKinematicConstraint *>(con->data);

  if (ikdata->type != value) {
    /* the type of IK constraint has changed, set suitable default values */
    /* in case constraints reuse same fields incompatible */
    switch (value) {
      case CONSTRAINT_IK_COPYPOSE:
        break;
      case CONSTRAINT_IK_DISTANCE:
        break;
    }
    ikdata->type = value;
  }
}

/* DEPRECATED: use_offset replaced with mix_mode */
static bool rna_Constraint_RotLike_use_offset_get(PointerRNA *ptr)
{
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  bRotateLikeConstraint *rotlike = static_cast<bRotateLikeConstraint *>(con->data);
  return rotlike->mix_mode != ROTLIKE_MIX_REPLACE;
}

static void rna_Constraint_RotLike_use_offset_set(PointerRNA *ptr, bool value)
{
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  bRotateLikeConstraint *rotlike = static_cast<bRotateLikeConstraint *>(con->data);
  bool curval = (rotlike->mix_mode != ROTLIKE_MIX_REPLACE);
  if (curval != value) {
    rotlike->mix_mode = (value ? ROTLIKE_MIX_OFFSET : ROTLIKE_MIX_REPLACE);
  }
}

static const EnumPropertyItem *rna_Constraint_owner_space_itemf(bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/,
                                                                bool * /*r_free*/)
{
  Object *ob = (Object *)ptr->owner_id;
  bConstraint *con = (bConstraint *)ptr->data;

  if (BLI_findindex(&ob->constraints, con) == -1) {
    return owner_space_pchan_items;
  }
  else {
    /* object */
    return owner_space_object_items;
  }
}

static const EnumPropertyItem *rna_Constraint_target_space_itemf(bContext * /*C*/,
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool * /*r_free*/)
{
  bConstraint *con = (bConstraint *)ptr->data;
  ListBase targets = {nullptr, nullptr};
  bConstraintTarget *ct;

  if (BKE_constraint_targets_get(con, &targets)) {
    for (ct = static_cast<bConstraintTarget *>(targets.first); ct; ct = ct->next) {
      if (ct->tar && ct->tar->type == OB_ARMATURE && !(ct->flag & CONSTRAINT_TAR_CUSTOM_SPACE)) {
        break;
      }
    }

    BKE_constraint_targets_flush(con, &targets, 1);

    if (ct) {
      return target_space_pchan_items;
    }
  }

  return target_space_object_items;
}

static bConstraintTarget *rna_ArmatureConstraint_target_new(ID *id, bConstraint *con, Main *bmain)
{
  bArmatureConstraint *acon = static_cast<bArmatureConstraint *>(con->data);
  bConstraintTarget *tgt = MEM_callocN<bConstraintTarget>("Constraint Target");

  tgt->weight = 1.0f;
  BLI_addtail(&acon->targets, tgt);

  blender::ed::object::constraint_dependency_tag_update(bmain, (Object *)id, con);
  return tgt;
}

static void rna_ArmatureConstraint_target_remove(
    ID *id, bConstraint *con, Main *bmain, ReportList *reports, PointerRNA *target_ptr)
{
  bArmatureConstraint *acon = static_cast<bArmatureConstraint *>(con->data);
  bConstraintTarget *tgt = static_cast<bConstraintTarget *>(target_ptr->data);

  if (BLI_findindex(&acon->targets, tgt) == -1) {
    BKE_report(reports, RPT_ERROR, "Target is not in the constraint target list");
    return;
  }

  BLI_freelinkN(&acon->targets, tgt);

  blender::ed::object::constraint_dependency_tag_update(bmain, (Object *)id, con);
}

static void rna_ArmatureConstraint_target_clear(ID *id, bConstraint *con, Main *bmain)
{
  bArmatureConstraint *acon = static_cast<bArmatureConstraint *>(con->data);

  BLI_freelistN(&acon->targets);

  blender::ed::object::constraint_dependency_tag_update(bmain, (Object *)id, con);
}

static void rna_ActionConstraint_mix_mode_set(PointerRNA *ptr, int value)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  acon->mix_mode = value;

  /* The After mode can be computed in world space for efficiency
   * and backward compatibility, while Before or Split requires Local. */
  if (ELEM(value, ACTCON_MIX_AFTER, ACTCON_MIX_AFTER_FULL)) {
    con->ownspace = CONSTRAINT_SPACE_WORLD;
  }
  else {
    con->ownspace = CONSTRAINT_SPACE_LOCAL;
  }
}

static void rna_ActionConstraint_minmax_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  /* 0, 1, 2 = magic numbers for rotX, rotY, rotZ */
  if (ELEM(acon->type, 0, 1, 2)) {
    *min = -180.0f;
    *max = 180.0f;
  }
  else {
    *min = -1000.0f;
    *max = 1000.0f;
  }
}

static void rna_ActionConstraint_action_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  using namespace blender::animrig;
  BLI_assert(ptr->owner_id);
  BLI_assert(ptr->data);

  ID &animated_id = *ptr->owner_id;
  bConstraint *con = static_cast<bConstraint *>(ptr->data);
  bActionConstraint *acon = static_cast<bActionConstraint *>(con->data);

  Action *action = static_cast<Action *>(value.data);

  if (!action) {
    const bool ok = generic_assign_action(
        animated_id, nullptr, acon->act, acon->action_slot_handle, acon->last_slot_identifier);
    BLI_assert_msg(ok, "Un-assigning an Action from an Action Constraint should always work.");
    UNUSED_VARS_NDEBUG(ok);
    return;
  }

  const bool ok = generic_assign_action(
      animated_id, action, acon->act, acon->action_slot_handle, acon->last_slot_identifier);
  if (!ok) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not assign action %s to Action Constraint %s",
                action->id.name + 2,
                con->name);
    return;
  }

  /* For the Action Constraint, the auto slot selection gets one more fallback
   * option (compared to the generic code). This is to support the following
   * scenario, which used to be necessary as a workaround for a bug in Blender (#127976):
   *
   * - Python script creates an Action,
   * - assigns it to the animated object,
   * - unassigns it from that object,
   * - and assigns it to the object's Action Constraint.
   *
   * The generic code doesn't work for this. The first assignment would see the slot
   * `XXSlot`, and because it has never been used, just use it. This would change its name to
   * `OBSlot`. The assignment to the Action Constraint would not see a 'virgin' slot, and thus not
   * auto-select `OBSlot`. This behavior makes sense when assigning Actions in the Action editor
   * (it shouldn't automatically pick the first slot of matching ID type), but for the Action
   * Constraint I (Sybren) feel that it could be a bit more 'enthusiastic' in auto-picking a slot.
   *
   * Note that this is the same behavior as for NLA strips, albeit for a slightly different
   * reason. Because of that it's not sharing code with the NLA.
   */
  if (acon->action_slot_handle == Slot::unassigned && action->slots().size() == 1) {
    Slot *first_slot = action->slot(0);
    if (first_slot->is_suitable_for(animated_id)) {
      const ActionSlotAssignmentResult result = generic_assign_action_slot(
          first_slot,
          animated_id,
          acon->act,
          acon->action_slot_handle,
          acon->last_slot_identifier);
      BLI_assert(result == ActionSlotAssignmentResult::OK);
      UNUSED_VARS_NDEBUG(result);
    }
  }
}

static void rna_ActionConstraint_action_slot_handle_set(
    PointerRNA *ptr, const blender::animrig::slot_handle_t new_slot_handle)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  rna_generic_action_slot_handle_set(new_slot_handle,
                                     *ptr->owner_id,
                                     acon->act,
                                     acon->action_slot_handle,
                                     acon->last_slot_identifier);
}

/**
 * Emit a 'diff' for the .action_slot_handle property whenever the .action property differs.
 *
 * \see rna_generic_action_slot_handle_override_diff()
 */
static void rna_ActionConstraint_action_slot_handle_override_diff(
    Main *bmain, RNAPropertyOverrideDiffContext &rnadiff_ctx)
{
  const bConstraint *con_a = static_cast<bConstraint *>(rnadiff_ctx.prop_a->ptr->data);
  const bConstraint *con_b = static_cast<bConstraint *>(rnadiff_ctx.prop_b->ptr->data);

  const bActionConstraint *act_con_a = static_cast<bActionConstraint *>(con_a->data);
  const bActionConstraint *act_con_b = static_cast<bActionConstraint *>(con_b->data);

  rna_generic_action_slot_handle_override_diff(bmain, rnadiff_ctx, act_con_a->act, act_con_b->act);
}

static PointerRNA rna_ActionConstraint_action_slot_get(PointerRNA *ptr)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  return rna_generic_action_slot_get(acon->act, acon->action_slot_handle);
}

static void rna_ActionConstraint_action_slot_set(PointerRNA *ptr,
                                                 PointerRNA value,
                                                 ReportList *reports)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  rna_generic_action_slot_set(value,
                              *ptr->owner_id,
                              acon->act,
                              acon->action_slot_handle,
                              acon->last_slot_identifier,
                              reports);
}

static void rna_iterator_ActionConstraint_action_suitable_slots_begin(
    CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bActionConstraint *acon = (bActionConstraint *)con->data;

  rna_iterator_generic_action_suitable_slots_begin(iter, ptr, acon->act);
}

static int rna_SplineIKConstraint_joint_bindings_get_length(const PointerRNA *ptr,
                                                            int length[RNA_MAX_ARRAY_DIMENSION])
{
  const bConstraint *con = (bConstraint *)ptr->data;
  const bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;

  if (ikData) {
    length[0] = ikData->numpoints;
  }
  else {
    length[0] = 0;
  }

  return length[0];
}

static void rna_SplineIKConstraint_joint_bindings_get(PointerRNA *ptr, float *values)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;

  memcpy(values, ikData->points, ikData->numpoints * sizeof(float));
}

static void rna_SplineIKConstraint_joint_bindings_set(PointerRNA *ptr, const float *values)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bSplineIKConstraint *ikData = (bSplineIKConstraint *)con->data;

  memcpy(ikData->points, values, ikData->numpoints * sizeof(float));
}

static int rna_ShrinkwrapConstraint_face_cull_get(PointerRNA *ptr)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bShrinkwrapConstraint *swc = (bShrinkwrapConstraint *)con->data;
  return swc->flag & CON_SHRINKWRAP_PROJECT_CULL_MASK;
}

static void rna_ShrinkwrapConstraint_face_cull_set(PointerRNA *ptr, int value)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bShrinkwrapConstraint *swc = (bShrinkwrapConstraint *)con->data;
  swc->flag = (swc->flag & ~CON_SHRINKWRAP_PROJECT_CULL_MASK) | value;
}

static bool rna_Constraint_cameraObject_poll(PointerRNA *ptr, PointerRNA value)
{
  Object *ob = (Object *)value.data;

  if (ob) {
    if (ob->type == OB_CAMERA && ob != (Object *)ptr->owner_id) {
      return 1;
    }
  }

  return 0;
}

static void rna_Constraint_followTrack_camera_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
  Object *ob = (Object *)value.data;

  if (ob) {
    if (ob->type == OB_CAMERA && ob != (Object *)ptr->owner_id) {
      data->camera = ob;
      id_lib_extern((ID *)ob);
    }
  }
  else {
    data->camera = nullptr;
  }
}

static void rna_Constraint_followTrack_depthObject_set(PointerRNA *ptr,
                                                       PointerRNA value,
                                                       ReportList * /*reports*/)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
  Object *ob = (Object *)value.data;

  if (ob) {
    if (ob->type == OB_MESH && ob != (Object *)ptr->owner_id) {
      data->depth_ob = ob;
      id_lib_extern((ID *)ob);
    }
  }
  else {
    data->depth_ob = nullptr;
  }
}

static bool rna_Constraint_followTrack_depthObject_poll(PointerRNA *ptr, PointerRNA value)
{
  Object *ob = (Object *)value.data;

  if (ob) {
    if (ob->type == OB_MESH && ob != (Object *)ptr->owner_id) {
      return 1;
    }
  }

  return 0;
}

static void rna_Constraint_objectSolver_camera_set(PointerRNA *ptr,
                                                   PointerRNA value,
                                                   ReportList * /*reports*/)
{
  bConstraint *con = (bConstraint *)ptr->data;
  bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;
  Object *ob = (Object *)value.data;

  if (ob) {
    if (ob->type == OB_CAMERA && ob != (Object *)ptr->owner_id) {
      data->camera = ob;
      id_lib_extern((ID *)ob);
    }
  }
  else {
    data->camera = nullptr;
  }
}

#else

static const EnumPropertyItem constraint_distance_items[] = {
    {LIMITDIST_INSIDE,
     "LIMITDIST_INSIDE",
     0,
     "Inside",
     "The object is constrained inside a virtual sphere around the target object, "
     "with a radius defined by the limit distance"},
    {LIMITDIST_OUTSIDE,
     "LIMITDIST_OUTSIDE",
     0,
     "Outside",
     "The object is constrained outside a virtual sphere around the target object, "
     "with a radius defined by the limit distance"},
    {LIMITDIST_ONSURFACE,
     "LIMITDIST_ONSURFACE",
     0,
     "On Surface",
     "The object is constrained on the surface of a virtual sphere around the target object, "
     "with a radius defined by the limit distance"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_constraint_headtail_common(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "head_tail", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "bConstraint", "headtail");
  RNA_def_property_ui_text(prop, "Head/Tail", "Target along length of bone: Head is 0, Tail is 1");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bbone_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, "bConstraint", "flag", CONSTRAINT_BBONE_SHAPE);
  RNA_def_property_ui_text(prop,
                           "Follow B-Bone",
                           "Follow shape of B-Bone segments when calculating Head/Tail position");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_target_common(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_ui_text(prop, "Target", "Target object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "subtarget");
  RNA_def_property_ui_text(prop, "Sub-Target", "Armature bone, mesh or lattice vertex group, ...");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constrainttarget(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ConstraintTarget", nullptr);
  RNA_def_struct_ui_text(srna, "Constraint Target", "Target object for multi-target constraints");
  RNA_def_struct_path_func(srna, "rna_ConstraintTarget_path");
  RNA_def_struct_sdna(srna, "bConstraintTarget");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_ui_text(prop, "Target", "Target object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_OBJECT | ND_CONSTRAINT, "rna_ConstraintTarget_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "subtarget");
  RNA_def_property_ui_text(prop, "Sub-Target", "Armature bone, mesh or lattice vertex group, ...");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_CONSTRAINT, "rna_ConstraintTarget_dependency_update");

  /* space, flag and type still to do. */

  RNA_define_lib_overridable(false);
}

static void rna_def_constrainttarget_bone(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ConstraintTargetBone", nullptr);
  RNA_def_struct_ui_text(
      srna, "Constraint Target Bone", "Target bone for multi-target constraints");
  RNA_def_struct_path_func(srna, "rna_ConstraintTarget_path");
  RNA_def_struct_sdna(srna, "bConstraintTarget");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_ui_text(prop, "Target", "Target armature");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_ConstraintTargetBone_target_set", nullptr, "rna_Armature_object_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_OBJECT | ND_CONSTRAINT, "rna_ConstraintTarget_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "subtarget");
  RNA_def_property_ui_text(prop, "Sub-Target", "Target armature bone");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_CONSTRAINT, "rna_ConstraintTarget_dependency_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "weight");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Blend Weight", "Blending weight of this bone");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_ConstraintTarget_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_childof(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ChildOfConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Child Of Constraint", "Create constraint-based parent-child relationship");
  RNA_def_struct_sdna_from(srna, "bChildOfConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_CHILDOF);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_location_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_LOCX);
  RNA_def_property_ui_text(prop, "Location X", "Use X Location of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_location_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_LOCY);
  RNA_def_property_ui_text(prop, "Location Y", "Use Y Location of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_location_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_LOCZ);
  RNA_def_property_ui_text(prop, "Location Z", "Use Z Location of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_rotation_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_ROTX);
  RNA_def_property_ui_text(prop, "Rotation X", "Use X Rotation of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_rotation_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_ROTY);
  RNA_def_property_ui_text(prop, "Rotation Y", "Use Y Rotation of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_rotation_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_ROTZ);
  RNA_def_property_ui_text(prop, "Rotation Z", "Use Z Rotation of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_scale_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_SIZEX);
  RNA_def_property_ui_text(prop, "Scale X", "Use X Scale of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_scale_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_SIZEY);
  RNA_def_property_ui_text(prop, "Scale Y", "Use Y Scale of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_scale_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_SIZEZ);
  RNA_def_property_ui_text(prop, "Scale Z", "Use Z Scale of Parent");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "set_inverse_pending", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CHILDOF_SET_INVERSE);
  RNA_def_property_ui_text(
      prop, "Set Inverse Pending", "Set to true to request recalculation of the inverse matrix");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "inverse_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "invmat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Inverse Matrix", "Transformation matrix to apply before");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_childof_inverse_matrix_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_armature_deform_targets(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ArmatureConstraintTargets");
  srna = RNA_def_struct(brna, "ArmatureConstraintTargets", nullptr);
  RNA_def_struct_sdna(srna, "bConstraint");
  RNA_def_struct_ui_text(
      srna, "Armature Deform Constraint Targets", "Collection of target bones and weights");
  RNA_def_struct_ui_icon(srna, ICON_CON_ARMATURE);

  func = RNA_def_function(srna, "new", "rna_ArmatureConstraint_target_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new target to the constraint");
  parm = RNA_def_pointer(func, "target", "ConstraintTargetBone", "", "New target bone");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_ArmatureConstraint_target_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete target from the constraint");
  parm = RNA_def_pointer(func, "target", "ConstraintTargetBone", "", "Target to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_ArmatureConstraint_target_clear");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Delete all targets from object");
}

static void rna_def_constraint_armature_deform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ArmatureConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Armature Constraint", "Applies transformations done by the Armature modifier");
  RNA_def_struct_sdna_from(srna, "bArmatureConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_ARMATURE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "targets", nullptr);
  RNA_def_property_struct_type(prop, "ConstraintTargetBone");
  RNA_def_property_ui_text(prop, "Targets", "Target Bones");
  rna_def_constraint_armature_deform_targets(brna, prop);

  prop = RNA_def_property(srna, "use_deform_preserve_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_ARMATURE_QUATERNION);
  RNA_def_property_ui_text(
      prop, "Preserve Volume", "Deform rotation interpolation with quaternions");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bone_envelopes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_ARMATURE_ENVELOPE);
  RNA_def_property_ui_text(
      prop,
      "Use Envelopes",
      "Multiply weights by envelope for all bones, instead of acting like Vertex Group based "
      "blending. "
      "The specified weights are still used, and only the listed bones are considered.");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_current_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_ARMATURE_CUR_LOCATION);
  RNA_def_property_ui_text(prop,
                           "Use Current Location",
                           "Use the current bone location for envelopes and choosing B-Bone "
                           "segments instead of rest position");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_kinematic(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem constraint_ik_axisref_items[] = {
      {0, "BONE", 0, "Bone", ""},
      {CONSTRAINT_IK_TARGETAXIS, "TARGET", 0, "Target", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem constraint_ik_type_items[] = {
      {CONSTRAINT_IK_COPYPOSE, "COPY_POSE", 0, "Copy Pose", ""},
      {CONSTRAINT_IK_DISTANCE, "DISTANCE", 0, "Distance", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "KinematicConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Kinematic Constraint", "Inverse Kinematics");
  RNA_def_struct_sdna_from(srna, "bKinematicConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_KINEMATIC);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "iterations", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_text(prop, "Iterations", "Maximum number of solving iterations");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "pole_target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "poletar");
  RNA_def_property_ui_text(prop, "Pole Target", "Object for pole rotation");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "pole_subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "polesubtarget");
  RNA_def_property_ui_text(prop, "Pole Sub-Target", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "pole_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "poleangle");
  RNA_def_property_range(prop, -M_PI, M_PI);
  RNA_def_property_ui_range(prop, -M_PI, M_PI, 10, 4);
  RNA_def_property_ui_text(prop, "Pole Angle", "Pole rotation offset");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01, 1.0f);
  RNA_def_property_ui_text(
      prop, "Weight", "For Tree-IK: Weight of position control for this target");

  prop = RNA_def_property(srna, "orient_weight", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "orientweight");
  RNA_def_property_range(prop, 0.01, 1.0f);
  RNA_def_property_ui_text(
      prop, "Orientation Weight", "For Tree-IK: Weight of orientation control for this target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "chain_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "rootbone");
  /* Changing the IK chain length requires a rebuild of depsgraph relations. This makes it
   * unsuitable for animation. */
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 255);
  RNA_def_property_ui_text(
      prop, "Chain Length", "How many bones are included in the IK effect - 0 uses all bones");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "use_tail", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_IK_TIP);
  RNA_def_property_ui_text(prop, "Use Tail", "Include bone's tail as last element in chain");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "reference_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, constraint_ik_axisref_items);
  RNA_def_property_ui_text(
      prop, "Axis Reference", "Constraint axis Lock options relative to Bone or Target reference");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "use_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_IK_POS);
  RNA_def_property_ui_text(prop, "Position", "Chain follows position of target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_location_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_POS_X);
  RNA_def_property_ui_text(prop, "Lock X Pos", "Constraint position along X axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_location_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_POS_Y);
  RNA_def_property_ui_text(prop, "Lock Y Pos", "Constraint position along Y axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_location_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_POS_Z);
  RNA_def_property_ui_text(prop, "Lock Z Pos", "Constraint position along Z axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "use_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_IK_ROT);
  RNA_def_property_ui_text(prop, "Rotation", "Chain follows rotation of target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_rotation_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_ROT_X);
  RNA_def_property_ui_text(prop, "Lock X Rotation", "Constraint rotation along X axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_rotation_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_ROT_Y);
  RNA_def_property_ui_text(prop, "Lock Y Rotation", "Constraint rotation along Y axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "lock_rotation_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_IK_NO_ROT_Z);
  RNA_def_property_ui_text(prop, "Lock Z Rotation", "Constraint rotation along Z axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_POSE, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "use_stretch", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_IK_STRETCH);
  RNA_def_property_ui_text(prop, "Stretch", "Enable IK Stretching");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "ik_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Constraint_ik_type_set", nullptr);
  RNA_def_property_enum_items(prop, constraint_ik_type_items);
  RNA_def_property_ui_text(prop, "IK Type", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "limit_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, constraint_distance_items);
  RNA_def_property_ui_text(
      prop, "Limit Mode", "Distances in relation to sphere of influence to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_range(prop, 0.0, 100.0f);
  RNA_def_property_ui_text(prop, "Distance", "Radius of limiting sphere");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_track_to(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem up_items[] = {
      {TRACK_X, "UP_X", 0, "X", ""},
      {TRACK_Y, "UP_Y", 0, "Y", ""},
      {TRACK_Z, "UP_Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "TrackToConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Track To Constraint", "Aim the constrained object toward the target");

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bTrackToConstraint", "data");

  rna_def_constraint_target_common(srna);

  RNA_def_struct_ui_icon(srna, ICON_CON_TRACKTO);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "reserved1");
  RNA_def_property_enum_items(prop, track_axis_items);
  RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "reserved2");
  RNA_def_property_enum_items(prop, up_items);
  RNA_def_property_ui_text(prop, "Up Axis", "Axis that points upward");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_target_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", TARGET_Z_UP);
  RNA_def_property_ui_text(
      prop, "Target Z", "Target's Z axis, not World Z axis, will constrain the Up direction");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_locate_like(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CopyLocationConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Copy Location Constraint", "Copy the location of the target");
  RNA_def_struct_ui_icon(srna, ICON_CON_LOCLIKE);

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bLocateLikeConstraint", "data");

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_X);
  RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_Y);
  RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_Z);
  RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_X_INVERT);
  RNA_def_property_ui_text(prop, "Invert X", "Invert the X location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_Y_INVERT);
  RNA_def_property_ui_text(prop, "Invert Y", "Invert the Y location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_Z_INVERT);
  RNA_def_property_ui_text(prop, "Invert Z", "Invert the Z location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LOCLIKE_OFFSET);
  RNA_def_property_ui_text(prop, "Offset", "Add original location into copied location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_rotate_like(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mix_mode_items[] = {
      {ROTLIKE_MIX_REPLACE, "REPLACE", 0, "Replace", "Replace the original rotation with copied"},
      {ROTLIKE_MIX_ADD, "ADD", 0, "Add", "Add euler component values together"},
      {ROTLIKE_MIX_BEFORE,
       "BEFORE",
       0,
       "Before Original",
       "Apply copied rotation before original, as if the constraint target is a parent"},
      {ROTLIKE_MIX_AFTER,
       "AFTER",
       0,
       "After Original",
       "Apply copied rotation after original, as if the constraint target is a child"},
      {ROTLIKE_MIX_OFFSET,
       "OFFSET",
       0,
       "Offset (Legacy)",
       "Combine rotations like the original Offset checkbox. Does not work well for "
       "multiple axis rotations."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "CopyRotationConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Copy Rotation Constraint", "Copy the rotation of the target");
  RNA_def_struct_sdna_from(srna, "bRotateLikeConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_ROTLIKE);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_X);
  RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_Y);
  RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_Z);
  RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_X_INVERT);
  RNA_def_property_ui_text(prop, "Invert X", "Invert the X rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_Y_INVERT);
  RNA_def_property_ui_text(prop, "Invert Y", "Invert the Y rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "invert_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ROTLIKE_Z_INVERT);
  RNA_def_property_ui_text(prop, "Invert Z", "Invert the Z rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "euler_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "euler_order");
  RNA_def_property_enum_items(prop, euler_order_items);
  RNA_def_property_ui_text(prop, "Euler Order", "Explicitly specify the euler rotation order");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode");
  RNA_def_property_enum_items(prop, mix_mode_items);
  RNA_def_property_ui_text(
      prop, "Mix Mode", "Specify how the copied and existing rotations are combined");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* DEPRECATED: replaced with mix_mode */
  prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Constraint_RotLike_use_offset_get", "rna_Constraint_RotLike_use_offset_set");
  RNA_def_property_ui_text(
      prop, "Offset", "DEPRECATED: Add original rotation into copied rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_size_like(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CopyScaleConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Copy Scale Constraint", "Copy the scale of the target");
  RNA_def_struct_sdna_from(srna, "bSizeLikeConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_SIZELIKE);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIZELIKE_X);
  RNA_def_property_ui_text(prop, "Copy X", "Copy the target's X scale");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIZELIKE_Y);
  RNA_def_property_ui_text(prop, "Copy Y", "Copy the target's Y scale");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIZELIKE_Z);
  RNA_def_property_ui_text(prop, "Copy Z", "Copy the target's Z scale");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "power");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Power", "Raise the target's scale to the specified power");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_CONSTRAINT);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_make_uniform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIZELIKE_UNIFORM);
  RNA_def_property_ui_text(prop,
                           "Make Uniform",
                           "Redistribute the copied change in volume equally "
                           "between the three axes of the owner");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIZELIKE_OFFSET);
  RNA_def_property_ui_text(prop, "Offset", "Combine original scale with copied scale");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_add", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIZELIKE_MULTIPLY);
  RNA_def_property_ui_text(
      prop,
      "Additive",
      "Use addition instead of multiplication to combine scale (2.7 compatibility)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_CONSTRAINT);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_same_volume(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem axis_items[] = {
      {SAMEVOL_X, "SAMEVOL_X", 0, "X", ""},
      {SAMEVOL_Y, "SAMEVOL_Y", 0, "Y", ""},
      {SAMEVOL_Z, "SAMEVOL_Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mode_items[] = {
      {SAMEVOL_STRICT,
       "STRICT",
       0,
       "Strict",
       "Volume is strictly preserved, overriding the scaling of non-free axes"},
      {SAMEVOL_UNIFORM,
       "UNIFORM",
       0,
       "Uniform",
       "Volume is preserved when the object is scaled uniformly. "
       "Deviations from uniform scale on non-free axes are passed through."},
      {SAMEVOL_SINGLE_AXIS,
       "SINGLE_AXIS",
       0,
       "Single Axis",
       "Volume is preserved when the object is scaled only on the free axis. "
       "Non-free axis scaling is passed through."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MaintainVolumeConstraint", "Constraint");
  RNA_def_struct_ui_text(srna,
                         "Maintain Volume Constraint",
                         "Maintain a constant volume along a single scaling axis");
  RNA_def_struct_sdna_from(srna, "bSameVolumeConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_SAMEVOL);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "free_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "free_axis");
  RNA_def_property_enum_items(prop, axis_items);
  RNA_def_property_ui_text(prop, "Free Axis", "The free scaling axis of the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(
      prop, "Mode", "The way the constraint treats original non-free axis scaling");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, 100.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Volume", "Volume of the bone at rest");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_transform_like(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mix_mode_items[] = {
      {TRANSLIKE_MIX_REPLACE,
       "REPLACE",
       0,
       "Replace",
       "Replace the original transformation with copied"},
      RNA_ENUM_ITEM_SEPR,
      {TRANSLIKE_MIX_BEFORE_FULL,
       "BEFORE_FULL",
       0,
       "Before Original (Full)",
       "Apply copied transformation before original, using simple matrix multiplication as if "
       "the constraint target is a parent in Full Inherit Scale mode. "
       "Will create shear when combining rotation and non-uniform scale."},
      {TRANSLIKE_MIX_BEFORE,
       "BEFORE",
       0,
       "Before Original (Aligned)",
       "Apply copied transformation before original, as if the constraint target is a parent in "
       "Aligned Inherit Scale mode. This effectively uses Full for location and Split Channels "
       "for rotation and scale."},
      {TRANSLIKE_MIX_BEFORE_SPLIT,
       "BEFORE_SPLIT",
       0,
       "Before Original (Split Channels)",
       "Apply copied transformation before original, handling location, rotation and scale "
       "separately, similar to a sequence of three Copy constraints"},
      RNA_ENUM_ITEM_SEPR,
      {TRANSLIKE_MIX_AFTER_FULL,
       "AFTER_FULL",
       0,
       "After Original (Full)",
       "Apply copied transformation after original, using simple matrix multiplication as if "
       "the constraint target is a child in Full Inherit Scale mode. "
       "Will create shear when combining rotation and non-uniform scale."},
      {TRANSLIKE_MIX_AFTER,
       "AFTER",
       0,
       "After Original (Aligned)",
       "Apply copied transformation after original, as if the constraint target is a child in "
       "Aligned Inherit Scale mode. This effectively uses Full for location and Split Channels "
       "for rotation and scale."},
      {TRANSLIKE_MIX_AFTER_SPLIT,
       "AFTER_SPLIT",
       0,
       "After Original (Split Channels)",
       "Apply copied transformation after original, handling location, rotation and scale "
       "separately, similar to a sequence of three Copy constraints"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "CopyTransformsConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Copy Transforms Constraint", "Copy all the transforms of the target");

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bTransLikeConstraint", "data");

  RNA_def_struct_ui_icon(srna, ICON_CON_TRANSLIKE);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "remove_target_shear", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", TRANSLIKE_REMOVE_TARGET_SHEAR);
  RNA_def_property_ui_text(
      prop, "Remove Target Shear", "Remove shear from the target transformation before combining");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode");
  RNA_def_property_enum_items(prop, mix_mode_items);
  RNA_def_property_ui_text(
      prop, "Mix Mode", "Specify how the copied and existing transformations are combined");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_minmax(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem minmax_items[] = {
      {TRACK_X, "FLOOR_X", 0, "X", ""},
      {TRACK_Y, "FLOOR_Y", 0, "Y", ""},
      {TRACK_Z, "FLOOR_Z", 0, "Z", ""},
      {TRACK_nX, "FLOOR_NEGATIVE_X", 0, "-X", ""},
      {TRACK_nY, "FLOOR_NEGATIVE_Y", 0, "-Y", ""},
      {TRACK_nZ, "FLOOR_NEGATIVE_Z", 0, "-Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FloorConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Floor Constraint", "Use the target object for location limitation");
  RNA_def_struct_sdna_from(srna, "bMinMaxConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_FLOOR);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "floor_location", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "minmaxflag");
  RNA_def_property_enum_items(prop, minmax_items);
  RNA_def_property_ui_text(
      prop, "Floor Location", "Location of target that object will not pass through");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MINMAX_USEROT);
  RNA_def_property_ui_text(prop, "Use Rotation", "Use the target's rotation to determine floor");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_ui_range(prop, -100.0f, 100.0f, 1, -1);
  RNA_def_property_ui_text(prop, "Offset", "Offset of floor from object origin");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_action(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem transform_channel_items[] = {
      {20, "LOCATION_X", 0, "X Location", ""},
      {21, "LOCATION_Y", 0, "Y Location", ""},
      {22, "LOCATION_Z", 0, "Z Location", ""},
      {00, "ROTATION_X", 0, "X Rotation", ""},
      {01, "ROTATION_Y", 0, "Y Rotation", ""},
      {02, "ROTATION_Z", 0, "Z Rotation", ""},
      {10, "SCALE_X", 0, "X Scale", ""},
      {11, "SCALE_Y", 0, "Y Scale", ""},
      {12, "SCALE_Z", 0, "Z Scale", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mix_mode_items[] = {
      {ACTCON_MIX_REPLACE,
       "REPLACE",
       0,
       "Replace",
       "Replace the original transformation with the action channels"},
      RNA_ENUM_ITEM_SEPR,
      {ACTCON_MIX_BEFORE_FULL,
       "BEFORE_FULL",
       0,
       "Before Original (Full)",
       "Apply the action channels before the original transformation, as if applied to an "
       "imaginary parent in Full Inherit Scale mode. Will create shear when combining rotation "
       "and non-uniform scale."},
      {ACTCON_MIX_BEFORE,
       "BEFORE",
       0,
       "Before Original (Aligned)",
       "Apply the action channels before the original transformation, as if applied to an "
       "imaginary parent in Aligned Inherit Scale mode. This effectively uses Full for location "
       "and Split Channels for rotation and scale."},
      {ACTCON_MIX_BEFORE_SPLIT,
       "BEFORE_SPLIT",
       0,
       "Before Original (Split Channels)",
       "Apply the action channels before the original transformation, handling location, rotation "
       "and scale separately"},
      RNA_ENUM_ITEM_SEPR,
      {ACTCON_MIX_AFTER_FULL,
       "AFTER_FULL",
       0,
       "After Original (Full)",
       "Apply the action channels after the original transformation, as if applied to an "
       "imaginary child in Full Inherit Scale mode. Will create shear when combining rotation "
       "and non-uniform scale."},
      {ACTCON_MIX_AFTER,
       "AFTER",
       0,
       "After Original (Aligned)",
       "Apply the action channels after the original transformation, as if applied to an "
       "imaginary child in Aligned Inherit Scale mode. This effectively uses Full for location "
       "and Split Channels for rotation and scale."},
      {ACTCON_MIX_AFTER_SPLIT,
       "AFTER_SPLIT",
       0,
       "After Original (Split Channels)",
       "Apply the action channels after the original transformation, handling location, rotation "
       "and scale separately"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ActionConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Action Constraint", "Map an action to the transform axes of a bone");
  RNA_def_struct_sdna_from(srna, "bActionConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_ACTION);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode");
  RNA_def_property_enum_items(prop, mix_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ActionConstraint_mix_mode_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Mix Mode",
      "Specify how existing transformations and the action channels are combined");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "transform_channel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, transform_channel_items);
  RNA_def_property_ui_text(
      prop,
      "Transform Channel",
      "Transformation channel from the target that is used to key the Action");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "act");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_ActionConstraint_action_set", nullptr, "rna_Action_id_poll");
  RNA_def_property_ui_text(prop, "Action", "The constraining action");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* This property is not necessary for the Python API (that is better off using
   * slot references/pointers directly), but it is needed for library overrides
   * to work. */
  prop = RNA_def_property(srna, "action_slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "action_slot_handle");
  RNA_def_property_int_funcs(
      prop, nullptr, "rna_ActionConstraint_action_slot_handle_set", nullptr);
  RNA_def_property_ui_text(prop,
                           "Action Slot Handle",
                           "A number that identifies which sub-set of the Action is considered "
                           "to be for this Action Constraint");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(
      prop, "rna_ActionConstraint_action_slot_handle_override_diff", nullptr, nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_Constraint_update");

  prop = RNA_def_property(srna, "last_slot_identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "last_slot_identifier");
  RNA_def_property_ui_text(
      prop,
      "Last Action Slot Identifier",
      "The identifier of the most recently assigned action slot. The slot identifies which "
      "sub-set of the Action is considered to be for this constraint, and its identifier is used "
      "to find the right slot when assigning an Action.");

  prop = RNA_def_property(srna, "action_slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Action Slot",
      "The slot identifies which sub-set of the Action is considered to be for this "
      "strip, and its name is used to find the right slot when assigning another Action");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ActionConstraint_action_slot_get",
                                 "rna_ActionConstraint_action_slot_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_Constraint_update");
  /* `strip.action_slot` is exposed to RNA as a pointer for things like the action slot selector in
   * the GUI. The ground truth of the assigned slot, however, is `action_slot_handle` declared
   * above. That property is used for library override operations, and this pointer property should
   * just be ignored.
   *
   * This needs PROPOVERRIDE_IGNORE; PROPOVERRIDE_NO_COMPARISON is not suitable here. This property
   * should act as if it is an overridable property (as from the user's perspective, it is), but an
   * override operation should not be created for it. It will be created for `action_slot_handle`,
   * and that's enough. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);

  prop = RNA_def_property(srna, "action_suitable_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_ActionConstraint_action_suitable_slots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Action Slots", "The list of action slots suitable for this NLA strip");

  prop = RNA_def_property(srna, "use_bone_object_action", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ACTCON_BONE_USE_OBJECT_ACTION);
  RNA_def_property_ui_text(prop,
                           "Object Action",
                           "Bones only: apply the object's transformation channels of the action "
                           "to the constrained bone, instead of bone's channels");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "start");
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Start Frame", "First frame of the Action to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "end");
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "End Frame", "Last frame of the Action to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "max");
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Maximum", "Maximum value for target channel range");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_ActionConstraint_minmax_range");

  prop = RNA_def_property(srna, "min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "min");
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "Minimum", "Minimum value for target channel range");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_ActionConstraint_minmax_range");

  prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "eval_time");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Evaluation Time", "Interpolates between Action Start and End frames");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_eval_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ACTCON_USE_EVAL_TIME);
  RNA_def_property_ui_text(prop,
                           "Use Evaluation Time",
                           "Interpolate between Action Start and End frames, with the Evaluation "
                           "Time slider instead of the Target object/bone");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_locked_track(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem lock_items[] = {
      {TRACK_X, "LOCK_X", 0, "X", ""},
      {TRACK_Y, "LOCK_Y", 0, "Y", ""},
      {TRACK_Z, "LOCK_Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "LockedTrackConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna,
      "Locked Track Constraint",
      "Point toward the target along the track axis, while locking the other axis");
  RNA_def_struct_ui_icon(srna, ICON_CON_LOCKTRACK);

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bLockTrackConstraint", "data");

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "trackflag");
  RNA_def_property_enum_items(prop, track_axis_items);
  RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "lock_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "lockflag");
  RNA_def_property_enum_items(prop, lock_items);
  RNA_def_property_ui_text(prop, "Locked Axis", "Axis that points upward");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_follow_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem forwardpath_items[] = {
      {TRACK_X, "FORWARD_X", 0, "X", ""},
      {TRACK_Y, "FORWARD_Y", 0, "Y", ""},
      {TRACK_Z, "FORWARD_Z", 0, "Z", ""},
      {TRACK_nX, "TRACK_NEGATIVE_X", 0, "-X", ""},
      {TRACK_nY, "TRACK_NEGATIVE_Y", 0, "-Y", ""},
      {TRACK_nZ, "TRACK_NEGATIVE_Z", 0, "-Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pathup_items[] = {
      {TRACK_X, "UP_X", 0, "X", ""},
      {TRACK_Y, "UP_Y", 0, "Y", ""},
      {TRACK_Z, "UP_Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FollowPathConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Follow Path Constraint", "Lock motion to the target path");
  RNA_def_struct_sdna_from(srna, "bFollowPathConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_FOLLOWPATH);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Curve_object_poll");
  RNA_def_property_ui_text(prop, "Target", "Target Curve object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TIME);
  RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
  RNA_def_property_ui_text(
      prop, "Offset", "Offset from the position corresponding to the time frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "offset_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "offset_fac");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01, 3);
  RNA_def_property_ui_text(
      prop, "Offset Factor", "Percentage value defining target position along length of curve");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "forward_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "trackflag");
  RNA_def_property_enum_items(prop, forwardpath_items);
  RNA_def_property_ui_text(prop, "Forward Axis", "Axis that points forward along the path");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "upflag");
  RNA_def_property_enum_items(prop, pathup_items);
  RNA_def_property_ui_text(prop, "Up Axis", "Axis that points upward");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_curve_follow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "followflag", FOLLOWPATH_FOLLOW);
  RNA_def_property_ui_text(
      prop, "Follow Curve", "Object will follow the heading and banking of the curve");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_fixed_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "followflag", FOLLOWPATH_STATIC);
  RNA_def_property_ui_text(
      prop,
      "Fixed Position",
      "Object will stay locked to a single point somewhere along the length of the curve "
      "regardless of time");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_curve_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "followflag", FOLLOWPATH_RADIUS);
  RNA_def_property_ui_text(prop, "Curve Radius", "Object is scaled by the curve radius");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_stretch_to(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem volume_items[] = {
      {VOLUME_XZ, "VOLUME_XZX", 0, "XZ", ""},
      {VOLUME_X, "VOLUME_X", 0, "X", ""},
      {VOLUME_Z, "VOLUME_Z", 0, "Z", ""},
      {NO_VOLUME, "NO_VOLUME", 0, "None", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem plane_items[] = {
      {PLANE_X, "PLANE_X", 0, "XZ", "Rotate around local X, then Z"},
      {PLANE_Z, "PLANE_Z", 0, "ZX", "Rotate around local Z, then X"},
      {SWING_Y,
       "SWING_Y",
       0,
       "Swing",
       "Use the smallest single axis rotation, similar to Damped Track"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "StretchToConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Stretch To Constraint", "Stretch to meet the target object");
  RNA_def_struct_ui_icon(srna, ICON_CON_STRETCHTO);

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bStretchToConstraint", "data");

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "volume", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "volmode");
  RNA_def_property_enum_items(prop, volume_items);
  RNA_def_property_ui_text(
      prop, "Maintain Volume", "Maintain the object's volume as it stretches");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "keep_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "plane");
  RNA_def_property_enum_items(prop, plane_items);
  RNA_def_property_ui_text(prop, "Keep Axis", "The rotation type and axis order to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "rest_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "orglength");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_range(prop, 0, 100.0f, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Original Length", "Length at rest position");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0f);
  RNA_def_property_ui_text(
      prop, "Volume Variation", "Factor between volume variation and stretching");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bulge_min", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", STRETCHTOCON_USE_BULGE_MIN);
  RNA_def_property_ui_text(
      prop, "Use Volume Variation Minimum", "Use lower limit for volume variation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bulge_max", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", STRETCHTOCON_USE_BULGE_MAX);
  RNA_def_property_ui_text(
      prop, "Use Volume Variation Maximum", "Use upper limit for volume variation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Volume Variation Minimum", "Minimum volume stretching factor");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0, 100.0f);
  RNA_def_property_ui_text(prop, "Volume Variation Maximum", "Maximum volume stretching factor");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_smooth", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(
      prop, "Volume Variation Smoothness", "Strength of volume stretching clamping");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_clamp_to(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem clamp_items[] = {
      {CLAMPTO_AUTO, "CLAMPTO_AUTO", 0, "Auto", ""},
      {CLAMPTO_X, "CLAMPTO_X", 0, "X", ""},
      {CLAMPTO_Y, "CLAMPTO_Y", 0, "Y", ""},
      {CLAMPTO_Z, "CLAMPTO_Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ClampToConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna,
      "Clamp To Constraint",
      "Constrain an object's location to the nearest point along the target path");
  RNA_def_struct_sdna_from(srna, "bClampToConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_CLAMPTO);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Curve_object_poll");
  RNA_def_property_ui_text(prop, "Target", "Target Object (Curves only)");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "main_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, clamp_items);
  RNA_def_property_ui_text(prop, "Main Axis", "Main axis of movement");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", CLAMPTO_CYCLIC);
  RNA_def_property_ui_text(
      prop, "Cyclic", "Treat curve as cyclic curve (no clamping to curve bounding box)");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_transform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem transform_items[] = {
      {TRANS_LOCATION, "LOCATION", 0, "Location", ""},
      {TRANS_ROTATION, "ROTATION", 0, "Rotation", ""},
      {TRANS_SCALE, "SCALE", 0, "Scale", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mix_mode_loc_items[] = {
      {TRANS_MIXLOC_REPLACE, "REPLACE", 0, "Replace", "Replace component values"},
      {TRANS_MIXLOC_ADD, "ADD", 0, "Add", "Add component values together"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mix_mode_rot_items[] = {
      {TRANS_MIXROT_REPLACE, "REPLACE", 0, "Replace", "Replace component values"},
      {TRANS_MIXROT_ADD, "ADD", 0, "Add", "Add component values together"},
      {TRANS_MIXROT_BEFORE,
       "BEFORE",
       0,
       "Before Original",
       "Apply new rotation before original, as if it was on a parent"},
      {TRANS_MIXROT_AFTER,
       "AFTER",
       0,
       "After Original",
       "Apply new rotation after original, as if it was on a child"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mix_mode_scale_items[] = {
      {TRANS_MIXSCALE_REPLACE, "REPLACE", 0, "Replace", "Replace component values"},
      {TRANS_MIXSCALE_MULTIPLY, "MULTIPLY", 0, "Multiply", "Multiply component values together"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "TransformConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Transformation Constraint", "Map transformations of the target to the object");
  RNA_def_struct_sdna_from(srna, "bTransformConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_TRANSFORM);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "map_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "from");
  RNA_def_property_enum_items(prop, transform_items);
  RNA_def_property_ui_text(prop, "Map From", "The transformation type to use from the target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "map_to", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "to");
  RNA_def_property_enum_items(prop, transform_items);
  RNA_def_property_ui_text(
      prop, "Map To", "The transformation type to affect on the constrained object");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "map_to_x_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "map[0]");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(
      prop, "Map To X From", "The source axis constrained object's X axis uses");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "map_to_y_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "map[1]");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(
      prop, "Map To Y From", "The source axis constrained object's Y axis uses");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "map_to_z_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "map[2]");
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(
      prop, "Map To Z From", "The source axis constrained object's Z axis uses");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_motion_extrapolate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "expo", CLAMPTO_CYCLIC);
  RNA_def_property_ui_text(prop, "Extrapolate Motion", "Extrapolate ranges");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "from_rotation_mode");
  RNA_def_property_enum_items(prop, rna_enum_driver_target_rotation_mode_items);
  RNA_def_property_ui_text(prop, "From Mode", "Specify the type of rotation channels to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_euler_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "to_euler_order");
  RNA_def_property_enum_items(prop, euler_order_items);
  RNA_def_property_ui_text(prop, "To Order", "Explicitly specify the output euler rotation order");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* Loc */
  prop = RNA_def_property(srna, "from_min_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum X", "Bottom range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Y", "Bottom range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Z", "Bottom range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum X", "Top range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Y", "Top range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Z", "Top range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum X", "Bottom range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Y", "Bottom range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Z", "Bottom range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum X", "Top range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Y", "Top range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Z", "Top range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode_loc");
  RNA_def_property_enum_items(prop, mix_mode_loc_items);
  RNA_def_property_ui_text(
      prop, "Location Mix Mode", "Specify how to combine the new location with original");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* Rot */
  prop = RNA_def_property(srna, "from_min_x_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_rot[0]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum X", "Bottom range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_y_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_rot[1]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Y", "Bottom range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_z_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_rot[2]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Z", "Bottom range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_x_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_rot[0]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum X", "Top range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_y_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_rot[1]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Y", "Top range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_z_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_rot[2]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Z", "Top range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_x_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_rot[0]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum X", "Bottom range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_y_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_rot[1]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Y", "Bottom range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_z_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_rot[2]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Z", "Bottom range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_x_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_rot[0]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum X", "Top range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_y_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_rot[1]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Y", "Top range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_z_rot", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_rot[2]");
  RNA_def_property_ui_range(prop, DEG2RADF(-180.0f), DEG2RADF(180.0f), 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Z", "Top range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode_rot", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode_rot");
  RNA_def_property_enum_items(prop, mix_mode_rot_items);
  RNA_def_property_ui_text(
      prop, "Rotation Mix Mode", "Specify how to combine the new rotation with original");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* Scale */
  prop = RNA_def_property(srna, "from_min_x_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_scale[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum X", "Bottom range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_y_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_scale[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Y", "Bottom range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_min_z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_min_scale[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Minimum Z", "Bottom range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_x_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_scale[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum X", "Top range of X axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_y_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_scale[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Y", "Top range of Y axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "from_max_z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "from_max_scale[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "From Maximum Z", "Top range of Z axis source motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_x_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_scale[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum X", "Bottom range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_y_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_scale[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Y", "Bottom range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_min_z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_min_scale[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Minimum Z", "Bottom range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_x_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_scale[0]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum X", "Top range of X axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_y_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_scale[1]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Y", "Top range of Y axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "to_max_z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "to_max_scale[2]");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "To Maximum Z", "Top range of Z axis destination motion");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode_scale", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode_scale");
  RNA_def_property_enum_items(prop, mix_mode_scale_items);
  RNA_def_property_ui_text(
      prop, "Scale Mix Mode", "Specify how to combine the new scale with original");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_location_limit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LimitLocationConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Limit Location Constraint", "Limit the location of the constrained object");
  RNA_def_struct_sdna_from(srna, "bLocLimitConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_LOCLIMIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_min_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_XMIN);
  RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_min_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_YMIN);
  RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_min_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ZMIN);
  RNA_def_property_ui_text(prop, "Minimum Z", "Use the minimum Z value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_XMAX);
  RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_YMAX);
  RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ZMAX);
  RNA_def_property_ui_text(prop, "Maximum Z", "Use the maximum Z value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "xmin");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "ymin");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "zmin");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Minimum Z", "Lowest Z value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "xmax");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "ymax");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "zmax");
  RNA_def_property_ui_range(prop, -1000.0f, 1000.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Maximum Z", "Highest Z value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", LIMIT_TRANSFORM);
  RNA_def_property_ui_text(
      prop, "Affect Transform", "Transform tools are affected by this constraint as well");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_rotation_limit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LimitRotationConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Limit Rotation Constraint", "Limit the rotation of the constrained object");
  RNA_def_struct_sdna_from(srna, "bRotLimitConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_ROTLIMIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_limit_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_XROT);
  RNA_def_property_ui_text(prop, "Limit X", "Use the minimum X value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_limit_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_YROT);
  RNA_def_property_ui_text(prop, "Limit Y", "Use the minimum Y value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_limit_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ZROT);
  RNA_def_property_ui_text(prop, "Limit Z", "Use the minimum Z value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "xmin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Minimum X", "Lower X angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "ymin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Minimum Y", "Lower Y angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "zmin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Minimum Z", "Lower Z angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "xmax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Maximum X", "Upper X angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "ymax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Maximum Y", "Upper Y angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "zmax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_range(prop, -2 * M_PI, 2 * M_PI, 10.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Maximum Z", "Upper Z angle bound");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "euler_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "euler_order");
  RNA_def_property_enum_items(prop, euler_order_items);
  RNA_def_property_ui_text(prop, "Euler Order", "Explicitly specify the euler rotation order");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", LIMIT_TRANSFORM);
  RNA_def_property_ui_text(
      prop, "Affect Transform", "Transform tools are affected by this constraint as well");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_legacy_behavior", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ROT_LEGACY_BEHAVIOR);
  RNA_def_property_ui_text(
      prop,
      "Legacy Behavior",
      "Use the old semi-broken behavior that does not understand that rotations loop around");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_size_limit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LimitScaleConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Limit Size Constraint", "Limit the scaling of the constrained object");
  RNA_def_struct_sdna_from(srna, "bSizeLimitConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_SIZELIMIT);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "use_min_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_XMIN);
  RNA_def_property_ui_text(prop, "Minimum X", "Use the minimum X value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_min_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_YMIN);
  RNA_def_property_ui_text(prop, "Minimum Y", "Use the minimum Y value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_min_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ZMIN);
  RNA_def_property_ui_text(prop, "Minimum Z", "Use the minimum Z value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_XMAX);
  RNA_def_property_ui_text(prop, "Maximum X", "Use the maximum X value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_YMAX);
  RNA_def_property_ui_text(prop, "Maximum Y", "Use the maximum Y value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_max_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMIT_ZMAX);
  RNA_def_property_ui_text(prop, "Maximum Z", "Use the maximum Z value");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "xmin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Minimum X", "Lowest X value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ymin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Minimum Y", "Lowest Y value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "min_z", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "zmin");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Minimum Z", "Lowest Z value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "xmax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Maximum X", "Highest X value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ymax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Maximum Y", "Highest Y value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "max_z", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "zmax");
  RNA_def_property_range(prop, -1000.0, 1000.0f);
  RNA_def_property_ui_text(prop, "Maximum Z", "Highest Z value to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", LIMIT_TRANSFORM);
  RNA_def_property_ui_text(
      prop, "Affect Transform", "Transform tools are affected by this constraint as well");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_distance_limit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LimitDistanceConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Limit Distance Constraint", "Limit the distance from target object");

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bDistLimitConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_DISTLIMIT);

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Distance", "Radius of limiting sphere");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "limit_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, constraint_distance_items);
  RNA_def_property_ui_text(
      prop, "Limit Mode", "Distances in relation to sphere of influence to allow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_transform_limit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LIMITDIST_TRANSFORM);
  RNA_def_property_ui_text(
      prop, "Affect Transform", "Transforms are affected by this constraint as well");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_shrinkwrap(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem type_items[] = {
      {MOD_SHRINKWRAP_NEAREST_SURFACE,
       "NEAREST_SURFACE",
       0,
       "Nearest Surface Point",
       "Shrink the location to the nearest target surface"},
      {MOD_SHRINKWRAP_PROJECT,
       "PROJECT",
       0,
       "Project",
       "Shrink the location to the nearest target surface along a given axis"},
      {MOD_SHRINKWRAP_NEAREST_VERTEX,
       "NEAREST_VERTEX",
       0,
       "Nearest Vertex",
       "Shrink the location to the nearest target vertex"},
      {MOD_SHRINKWRAP_TARGET_PROJECT,
       "TARGET_PROJECT",
       0,
       "Target Normal Project",
       "Shrink the location to the nearest target surface "
       "along the interpolated vertex normals of the target"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem shrink_face_cull_items[] = {
      {0, "OFF", 0, "Off", "No culling"},
      {CON_SHRINKWRAP_PROJECT_CULL_FRONTFACE,
       "FRONT",
       0,
       "Front",
       "No projection when in front of the face"},
      {CON_SHRINKWRAP_PROJECT_CULL_BACKFACE,
       "BACK",
       0,
       "Back",
       "No projection when behind the face"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ShrinkwrapConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Shrinkwrap Constraint", "Create constraint-based shrinkwrap relationship");
  RNA_def_struct_sdna_from(srna, "bShrinkwrapConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_SHRINKWRAP);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "target"); /* TODO: mesh type. */
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Mesh_object_poll");
  RNA_def_property_ui_text(prop, "Target", "Target Mesh object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "shrinkwrap_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shrinkType");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(
      prop, "Shrinkwrap Type", "Select type of shrinkwrap algorithm for target position");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "wrap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shrinkMode");
  RNA_def_property_enum_items(prop, rna_enum_modifier_shrinkwrap_mode_items);
  RNA_def_property_ui_text(
      prop, "Snap Mode", "Select how to constrain the object to the target surface");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Distance", "Distance to Target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "project_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "projAxis");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(prop, "Project Axis", "Axis constrain to");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "project_axis_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "projAxisSpace");
  RNA_def_property_enum_items(prop, owner_space_pchan_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Constraint_owner_space_itemf");
  RNA_def_property_ui_text(prop, "Axis Space", "Space for the projection axis");

  prop = RNA_def_property(srna, "project_limit", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "projLimit");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 10, 3);
  RNA_def_property_ui_text(
      prop, "Project Distance", "Limit the distance used for projection (zero disables)");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_project_opposite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CON_SHRINKWRAP_PROJECT_OPPOSITE);
  RNA_def_property_ui_text(
      prop, "Project Opposite", "Project in both specified and opposite directions");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "cull_face", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, shrink_face_cull_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_ShrinkwrapConstraint_face_cull_get",
                              "rna_ShrinkwrapConstraint_face_cull_set",
                              nullptr);
  RNA_def_property_ui_text(
      prop,
      "Face Cull",
      "Stop vertices from projecting to a face on the target when facing towards/away");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_invert_cull", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CON_SHRINKWRAP_PROJECT_INVERT_CULL);
  RNA_def_property_ui_text(
      prop, "Invert Cull", "When projecting in the opposite direction invert the face cull mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_track_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CON_SHRINKWRAP_TRACK_NORMAL);
  RNA_def_property_ui_text(
      prop, "Align Axis To Normal", "Align the specified axis to the surface normal");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "trackAxis");
  RNA_def_property_enum_items(prop, track_axis_items);
  RNA_def_property_ui_text(prop, "Track Axis", "Axis that is aligned to the normal");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_damped_track(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "DampedTrackConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Damped Track Constraint", "Point toward target by taking the shortest rotation path");
  RNA_def_struct_ui_icon(srna, ICON_CON_TRACKTO);

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bDampTrackConstraint", "data");

  rna_def_constraint_target_common(srna);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "trackflag");
  RNA_def_property_enum_items(prop, track_axis_items);
  RNA_def_property_ui_text(prop, "Track Axis", "Axis that points to the target object");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_spline_ik(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem splineik_xz_scale_mode[] = {
      {CONSTRAINT_SPLINEIK_XZS_NONE, "NONE", 0, "None", "Don't scale the X and Z axes"},
      {CONSTRAINT_SPLINEIK_XZS_ORIGINAL,
       "BONE_ORIGINAL",
       0,
       "Bone Original",
       "Use the original scaling of the bones"},
      {CONSTRAINT_SPLINEIK_XZS_INVERSE,
       "INVERSE_PRESERVE",
       0,
       "Inverse Scale",
       "Scale of the X and Z axes is the inverse of the Y-Scale"},
      {CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC,
       "VOLUME_PRESERVE",
       0,
       "Volume Preservation",
       "Scale of the X and Z axes are adjusted to preserve the volume of the bones"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem splineik_y_scale_mode[] = {
      {CONSTRAINT_SPLINEIK_YS_NONE, "NONE", 0, "None", "Don't scale in the Y axis"},
      {CONSTRAINT_SPLINEIK_YS_FIT_CURVE,
       "FIT_CURVE",
       0,
       "Fit Curve",
       "Scale the bones to fit the entire length of the curve"},
      {CONSTRAINT_SPLINEIK_YS_ORIGINAL,
       "BONE_ORIGINAL",
       0,
       "Bone Original",
       "Use the original Y scale of the bone"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SplineIKConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Spline IK Constraint", "Align 'n' bones along a curve");
  RNA_def_struct_sdna_from(srna, "bSplineIKConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_SPLINEIK);

  RNA_define_lib_overridable(true);

  /* target chain */
  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Curve_object_poll");
  RNA_def_property_ui_text(prop, "Target", "Curve that controls this relationship");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "chain_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "chainlen");
  /* Changing the IK chain length requires a rebuild of depsgraph relations. This makes it
   * unsuitable for animation. */
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  /* TODO: this should really check the max length of the chain the constraint is attached to */
  RNA_def_property_range(prop, 1, 255);
  RNA_def_property_ui_text(prop, "Chain Length", "How many bones are included in the chain");
  /* XXX: this update goes wrong... needs extra flush? */
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* direct access to bindings */
  /* NOTE: only to be used by experienced users */
  prop = RNA_def_property(srna, "joint_bindings", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_array(prop, 32); /* XXX this is the maximum value allowed - why? */
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_dynamic_array_funcs(prop, "rna_SplineIKConstraint_joint_bindings_get_length");
  RNA_def_property_float_funcs(prop,
                               "rna_SplineIKConstraint_joint_bindings_get",
                               "rna_SplineIKConstraint_joint_bindings_set",
                               nullptr);
  RNA_def_property_ui_text(
      prop,
      "Joint Bindings",
      "(EXPERIENCED USERS ONLY) The relative positions of the joints along the chain, "
      "as percentages");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* settings */
  prop = RNA_def_property(srna, "use_chain_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_NO_ROOT);
  RNA_def_property_ui_text(
      prop, "Chain Offset", "Offset the entire chain relative to the root joint");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_even_divisions", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_EVENSPLITS);
  RNA_def_property_ui_text(prop,
                           "Even Divisions",
                           "Ignore the relative lengths of the bones when fitting to the curve");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_curve_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_NO_CURVERAD);
  RNA_def_property_ui_text(
      prop,
      "Use Curve Radius",
      "Average radius of the endpoints is used to tweak the X and Z Scaling of the bones, "
      "on top of XZ Scale mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* xz scaling mode */
  prop = RNA_def_property(srna, "xz_scale_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "xzScaleMode");
  RNA_def_property_enum_items(prop, splineik_xz_scale_mode);
  RNA_def_property_ui_text(
      prop,
      "XZ Scale Mode",
      "Method used for determining the scaling of the X and Z axes of the bones");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* y scaling mode */
  prop = RNA_def_property(srna, "y_scale_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "yScaleMode");
  RNA_def_property_enum_items(prop, splineik_y_scale_mode);
  RNA_def_property_ui_text(prop,
                           "Y Scale Mode",
                           "Method used for determining the scaling of the Y axis of the bones, "
                           "on top of the shape and scaling of the curve itself");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* take original scaling of the bone into account in volume preservation */
  prop = RNA_def_property(srna, "use_original_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_USE_ORIGINAL_SCALE);
  RNA_def_property_ui_text(
      prop, "Use Original Scale", "Apply volume preservation over the original scaling");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* Volume preservation for "volumetric" scale mode. */
  prop = RNA_def_property(srna, "bulge", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0f);
  RNA_def_property_ui_text(
      prop, "Volume Variation", "Factor between volume variation and stretching");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bulge_min", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_USE_BULGE_MIN);
  RNA_def_property_ui_text(
      prop, "Use Volume Variation Minimum", "Use lower limit for volume variation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "use_bulge_max", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_SPLINEIK_USE_BULGE_MAX);
  RNA_def_property_ui_text(
      prop, "Use Volume Variation Maximum", "Use upper limit for volume variation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(prop, "Volume Variation Minimum", "Minimum volume stretching factor");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0, 100.0f);
  RNA_def_property_ui_text(prop, "Volume Variation Maximum", "Maximum volume stretching factor");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "bulge_smooth", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_ui_text(
      prop, "Volume Variation Smoothness", "Strength of volume stretching clamping");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_pivot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem pivot_rotAxis_items[] = {
      {PIVOTCON_AXIS_NONE, "ALWAYS_ACTIVE", 0, "Always", "Use the pivot point in every rotation"},
      {PIVOTCON_AXIS_X_NEG,
       "NX",
       0,
       "-X Rotation",
       "Use the pivot point in the negative rotation range around the X-axis"},
      {PIVOTCON_AXIS_Y_NEG,
       "NY",
       0,
       "-Y Rotation",
       "Use the pivot point in the negative rotation range around the Y-axis"},
      {PIVOTCON_AXIS_Z_NEG,
       "NZ",
       0,
       "-Z Rotation",
       "Use the pivot point in the negative rotation range around the Z-axis"},
      {PIVOTCON_AXIS_X,
       "X",
       0,
       "X Rotation",
       "Use the pivot point in the positive rotation range around the X-axis"},
      {PIVOTCON_AXIS_Y,
       "Y",
       0,
       "Y Rotation",
       "Use the pivot point in the positive rotation range around the Y-axis"},
      {PIVOTCON_AXIS_Z,
       "Z",
       0,
       "Z Rotation",
       "Use the pivot point in the positive rotation range around the Z-axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "PivotConstraint", "Constraint");
  RNA_def_struct_ui_text(srna, "Pivot Constraint", "Rotate around a different point");

  rna_def_constraint_headtail_common(srna);

  RNA_def_struct_sdna_from(srna, "bPivotConstraint", "data");

  RNA_def_struct_ui_icon(srna, ICON_CON_PIVOT);

  RNA_define_lib_overridable(true);

  /* target-defined pivot */
  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tar");
  RNA_def_property_ui_text(
      prop, "Target", "Target Object, defining the position of the pivot when defined");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "subtarget");
  RNA_def_property_ui_text(prop, "Sub-Target", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* pivot offset */
  prop = RNA_def_property(srna, "use_relative_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", PIVOTCON_FLAG_OFFSET_ABS);
  RNA_def_property_ui_text(
      prop,
      "Use Relative Offset",
      "Offset will be an absolute point in space instead of relative to the target");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "offset");
  RNA_def_property_ui_text(prop,
                           "Offset",
                           "Offset of pivot from target (when set), or from owner's location "
                           "(when Fixed Position is off), or the absolute pivot point");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* rotation-based activation */
  prop = RNA_def_property(srna, "rotation_range", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "rotAxis");
  RNA_def_property_enum_items(prop, pivot_rotAxis_items);
  RNA_def_property_ui_text(
      prop, "Enabled Rotation Range", "Rotation range on which pivoting should occur");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_follow_track(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem frame_method_items[] = {
      {FOLLOWTRACK_FRAME_STRETCH, "STRETCH", 0, "Stretch", ""},
      {FOLLOWTRACK_FRAME_FIT, "FIT", 0, "Fit", ""},
      {FOLLOWTRACK_FRAME_CROP, "CROP", 0, "Crop", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FollowTrackConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Follow Track Constraint", "Lock motion to the target motion track");
  RNA_def_struct_sdna_from(srna, "bFollowTrackConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_FOLLOWTRACK);

  RNA_define_lib_overridable(true);

  /* movie clip */
  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clip");
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* track */
  prop = RNA_def_property(srna, "track", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "track");
  RNA_def_property_ui_text(prop, "Track", "Movie tracking track to follow");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* use default clip */
  prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FOLLOWTRACK_ACTIVECLIP);
  RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* use 3d position */
  prop = RNA_def_property(srna, "use_3d_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FOLLOWTRACK_USE_3D_POSITION);
  RNA_def_property_ui_text(prop, "3D Position", "Use 3D position of track to parent to");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* object */
  prop = RNA_def_property(srna, "object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "object");
  RNA_def_property_ui_text(
      prop, "Object", "Movie tracking object to follow (if empty, camera object is used)");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* camera */
  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "camera");
  RNA_def_property_ui_text(
      prop, "Camera", "Camera to which motion is parented (if empty active scene camera is used)");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_Constraint_followTrack_camera_set",
                                 nullptr,
                                 "rna_Constraint_cameraObject_poll");

  /* depth object */
  prop = RNA_def_property(srna, "depth_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "depth_ob");
  RNA_def_property_ui_text(
      prop,
      "Depth Object",
      "Object used to define depth in camera space by projecting onto surface of this object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_Constraint_followTrack_depthObject_set",
                                 nullptr,
                                 "rna_Constraint_followTrack_depthObject_poll");

  /* frame method */
  prop = RNA_def_property(srna, "frame_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "frame_method");
  RNA_def_property_enum_items(prop, frame_method_items);
  RNA_def_property_ui_text(prop, "Frame Method", "How the footage fits in the camera frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* use undistortion */
  prop = RNA_def_property(srna, "use_undistorted_position", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FOLLOWTRACK_USE_UNDISTORTION);
  RNA_def_property_ui_text(prop, "Undistort", "Parent to undistorted position of 2D track");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_camera_solver(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CameraSolverConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Camera Solver Constraint", "Lock motion to the reconstructed camera movement");
  RNA_def_struct_sdna_from(srna, "bCameraSolverConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_CAMERASOLVER);

  RNA_define_lib_overridable(true);

  /* movie clip */
  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clip");
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* use default clip */
  prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CAMERASOLVER_ACTIVECLIP);
  RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_object_solver(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectSolverConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Object Solver Constraint", "Lock motion to the reconstructed object movement");
  RNA_def_struct_sdna_from(srna, "bObjectSolverConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_OBJECTSOLVER);

  RNA_define_lib_overridable(true);

  /* movie clip */
  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clip");
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie Clip to get tracking data from");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* use default clip */
  prop = RNA_def_property(srna, "use_active_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CAMERASOLVER_ACTIVECLIP);
  RNA_def_property_ui_text(prop, "Active Clip", "Use active clip defined in scene");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "set_inverse_pending", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", OBJECTSOLVER_SET_INVERSE);
  RNA_def_property_ui_text(
      prop, "Set Inverse Pending", "Set to true to request recalculation of the inverse matrix");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  /* object */
  prop = RNA_def_property(srna, "object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "object");
  RNA_def_property_ui_text(prop, "Object", "Movie tracking object to follow");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* camera */
  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "camera");
  RNA_def_property_ui_text(
      prop, "Camera", "Camera to which motion is parented (if empty active scene camera is used)");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_Constraint_objectSolver_camera_set",
                                 nullptr,
                                 "rna_Constraint_cameraObject_poll");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_transform_cache(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TransformCacheConstraint", "Constraint");
  RNA_def_struct_ui_text(
      srna, "Transform Cache Constraint", "Look up transformation from an external file");
  RNA_def_struct_sdna_from(srna, "bTransformCacheConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_TRANSFORM_CACHE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "cache_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "cache_file");
  RNA_def_property_struct_type(prop, "CacheFile");
  RNA_def_property_ui_text(prop, "Cache File", "");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, 0, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "object_path", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Object Path",
      "Path to the object in the Alembic archive used to lookup the transform matrix");
  RNA_def_property_update(prop, 0, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

static void rna_def_constraint_geometry_attribute(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem domain_items[] = {
      {CON_ATTRIBUTE_DOMAIN_POINT, "POINT", 0, "Point"},
      {CON_ATTRIBUTE_DOMAIN_EDGE, "EDGE", 0, "Edge"},
      {CON_ATTRIBUTE_DOMAIN_FACE, "FACE", 0, "Face"},
      {CON_ATTRIBUTE_DOMAIN_FACE_CORNER, "FACE_CORNER", 0, "Face Corner"},
      {CON_ATTRIBUTE_DOMAIN_CURVE, "CURVE", 0, "Spline"},
      {CON_ATTRIBUTE_DOMAIN_INSTANCE, "INSTANCE", 0, "Instance"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem type_items[] = {
      {CON_ATTRIBUTE_VECTOR, "VECTOR", 0, "Vector", "Vector data type, affects position"},
      {CON_ATTRIBUTE_QUATERNION,
       "QUATERNION",
       0,
       "Quaternion",
       "Quaternion data type, affects rotation"},
      {CON_ATTRIBUTE_4X4MATRIX,
       "FLOAT4X4",
       0,
       "4x4 Matrix",
       "4x4 Matrix data type, affects transform"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem attribute_mix_mode_items[] = {
      {CON_ATTRIBUTE_MIX_REPLACE,
       "REPLACE",
       0,
       "Replace",
       "Replace the original transformation with the transform from the attribute"},
      RNA_ENUM_ITEM_SEPR,
      {CON_ATTRIBUTE_MIX_BEFORE_FULL,
       "BEFORE_FULL",
       0,
       "Before Original (Full)",
       "Apply copied transformation before original, using simple matrix multiplication as if "
       "the constraint target is a parent in Full Inherit Scale mode. "
       "Will create shear when combining rotation and non-uniform scale."},
      {CON_ATTRIBUTE_MIX_BEFORE_SPLIT,
       "BEFORE_SPLIT",
       0,
       "Before Original (Split Channels)",
       "Apply copied transformation before original, handling location, rotation and scale "
       "separately, similar to a sequence of three Copy constraints"},
      RNA_ENUM_ITEM_SEPR,
      {CON_ATTRIBUTE_MIX_AFTER_FULL,
       "AFTER_FULL",
       0,
       "After Original (Full)",
       "Apply copied transformation after original, using simple matrix multiplication as if "
       "the constraint target is a child in Full Inherit Scale mode. "
       "Will create shear when combining rotation and non-uniform scale."},
      {CON_ATTRIBUTE_MIX_AFTER_SPLIT,
       "AFTER_SPLIT",
       0,
       "After Original (Split Channels)",
       "Apply copied transformation after original, handling location, rotation and scale "
       "separately, similar to a sequence of three Copy constraints"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "GeometryAttributeConstraint", "Constraint");
  RNA_def_struct_ui_text(srna,
                         "Geometry Attribute Constraint",
                         "Create a constraint-based relationship with an attribute from geometry");
  RNA_def_struct_sdna_from(srna, "bGeometryAttributeConstraint", "data");
  RNA_def_struct_ui_icon(srna, ICON_CON_GEOMETRYATTRIBUTE);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "target");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Target", "Target geometry object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "attribute_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "attribute_name");
  RNA_def_property_ui_text(
      prop, "Attribute Name", "Name of the attribute to retrieve the transform from");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "domain");
  RNA_def_property_enum_items(prop, domain_items);
  RNA_def_property_ui_text(prop, "Domain Type", "Attribute domain");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "apply_target_transform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "apply_target_transform", 1);
  RNA_def_property_ui_text(
      prop,
      "Target Transform",
      "Apply the target object's world transform on top of the attribute's transform");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "data_type");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Data Type", "Select data type of attribute");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "sample_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sample_index");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Sample Index", "Sample Index");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_loc", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", MIX_LOC);
  RNA_def_property_ui_text(prop, "Mix Location", "Mix Location");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_rot", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", MIX_ROT);
  RNA_def_property_ui_text(prop, "Mix Rotation", "Mix Rotation");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_scl", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", MIX_SCALE);
  RNA_def_property_ui_text(prop, "Mix Scale", "Mix Scale");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mix_mode");
  RNA_def_property_enum_items(prop, attribute_mix_mode_items);
  RNA_def_property_ui_text(
      prop, "Mix Mode", "Specify how the copied and existing transformations are combined");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  RNA_define_lib_overridable(false);
}

/* Define the base struct for constraints. */

void RNA_def_constraint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* data */
  srna = RNA_def_struct(brna, "Constraint", nullptr);
  RNA_def_struct_ui_text(
      srna, "Constraint", "Constraint modifying the transformation of objects and bones");
  RNA_def_struct_refine_func(srna, "rna_ConstraintType_refine");
  RNA_def_struct_path_func(srna, "rna_Constraint_path");
  RNA_def_struct_sdna(srna, "bConstraint");
  RNA_def_struct_ui_icon(srna, ICON_CONSTRAINT);

  /* strings */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Constraint_name_set");
  RNA_def_property_ui_text(prop, "Name", "Constraint name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT | NA_RENAME, nullptr);

  /* enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_constraint_type_items);
  RNA_def_property_ui_text(prop, "Type", "");

  prop = RNA_def_boolean(srna,
                         "is_override_data",
                         false,
                         "Override Constraint",
                         "In a local override object, whether this constraint comes from the "
                         "linked reference object, or is local to the override");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_OVERRIDE_LIBRARY_LOCAL);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "owner_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "ownspace");
  RNA_def_property_enum_items(prop, owner_space_pchan_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Constraint_owner_space_itemf");
  RNA_def_property_ui_text(prop, "Owner Space", "Space that owner is evaluated in");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "target_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "tarspace");
  RNA_def_property_enum_items(prop, target_space_pchan_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Constraint_target_space_itemf");
  RNA_def_property_ui_text(prop, "Target Space", "Space that target is evaluated in");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");

  prop = RNA_def_property(srna, "space_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "space_object");
  RNA_def_property_ui_text(prop, "Object", "Object for Custom Space");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  prop = RNA_def_property(srna, "space_subtarget", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "space_subtarget");
  RNA_def_property_ui_text(prop, "Sub-Target", "Armature bone, mesh or lattice vertex group, ...");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_dependency_update");

  /* flags */
  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_OFF);
  RNA_def_property_ui_text(prop, "Disable", "Enable/Disable Constraint");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_OFF);
  RNA_def_property_ui_text(prop, "Enabled", "Use the results of this constraint");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_update");
  RNA_def_property_ui_icon(prop, ICON_HIDE_ON, 1);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ui_expand_flag", 0);
  RNA_def_property_ui_text(prop, "Expanded", "Constraint's panel is expanded in UI");
  RNA_def_property_ui_icon(prop, ICON_RIGHTARROW, 1);

  /* XXX this is really an internal flag,
   * but it may be useful for some tools to be able to access this... */
  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", CONSTRAINT_DISABLE);
  RNA_def_property_ui_text(prop, "Valid", "Constraint has valid settings and can be evaluated");

  /* TODO: setting this to true must ensure that all others in stack are turned off too... */
  prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", CONSTRAINT_ACTIVE);
  RNA_def_property_ui_text(prop, "Active", "Constraint is the one being edited");

  /* values */
  prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "enforce");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Influence", "Amount of influence constraint will have on the final solution");
  RNA_def_property_update(prop, NC_OBJECT | ND_CONSTRAINT, "rna_Constraint_influence_update");

  /* readonly values */
  prop = RNA_def_property(srna, "error_location", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "lin_error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Lin error",
      "Amount of residual error in Blender space unit for constraints that work on position");

  prop = RNA_def_property(srna, "error_rotation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rot_error");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Rotation error",
      "Amount of residual error in radians for constraints that work on orientation");

  RNA_define_lib_overridable(false);

  /* pointers */
  rna_def_constrainttarget(brna);
  rna_def_constrainttarget_bone(brna);

  rna_def_constraint_childof(brna);
  rna_def_constraint_armature_deform(brna);
  rna_def_constraint_stretch_to(brna);
  rna_def_constraint_follow_path(brna);
  rna_def_constraint_locked_track(brna);
  rna_def_constraint_action(brna);
  rna_def_constraint_size_like(brna);
  rna_def_constraint_same_volume(brna);
  rna_def_constraint_locate_like(brna);
  rna_def_constraint_rotate_like(brna);
  rna_def_constraint_transform_like(brna);
  rna_def_constraint_minmax(brna);
  rna_def_constraint_track_to(brna);
  rna_def_constraint_kinematic(brna);
  rna_def_constraint_clamp_to(brna);
  rna_def_constraint_distance_limit(brna);
  rna_def_constraint_size_limit(brna);
  rna_def_constraint_rotation_limit(brna);
  rna_def_constraint_location_limit(brna);
  rna_def_constraint_transform(brna);
  rna_def_constraint_shrinkwrap(brna);
  rna_def_constraint_damped_track(brna);
  rna_def_constraint_spline_ik(brna);
  rna_def_constraint_pivot(brna);
  rna_def_constraint_follow_track(brna);
  rna_def_constraint_camera_solver(brna);
  rna_def_constraint_object_solver(brna);
  rna_def_constraint_transform_cache(brna);
  rna_def_constraint_geometry_attribute(brna);
}

#endif
