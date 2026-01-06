/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * Constraint DNA data.
 */

#pragma once

#include "BLI_sys_types.h"

#include "DNA_listBase.h"

namespace blender {

struct Text;

#define CON_SHRINKWRAP_PROJECT_CULL_MASK \
  (CON_SHRINKWRAP_PROJECT_CULL_FRONTFACE | CON_SHRINKWRAP_PROJECT_CULL_BACKFACE)

/* bConstraintTarget -> flag */
enum eConstraintTargetFlag {
  /** Temporary target-struct that needs to be freed after use. */
  CONSTRAINT_TAR_TEMP = (1 << 0),
  /** Temporary target for the custom space reference. */
  CONSTRAINT_TAR_CUSTOM_SPACE = (1 << 1),
};

/* bConstraintTarget/bConstraintOb -> type */
enum eConstraintObType {
  /** string is "" */
  CONSTRAINT_OBTYPE_OBJECT = 1,
  /** string is bone-name */
  CONSTRAINT_OBTYPE_BONE = 2,
  /** string is vertex-group name */
  CONSTRAINT_OBTYPE_VERT = 3,
  /** string is vertex-group name - is not available until curves get vgroups */
  /* CONSTRAINT_OBTYPE_CV = 4, */ /* UNUSED */
};

enum eConstraint_IK_Type {
  /** 'standard' IK constraint: match position and/or orientation of target */
  CONSTRAINT_IK_COPYPOSE = 0,
  /** maintain distance with target */
  CONSTRAINT_IK_DISTANCE = 1,
};

/* bGeometryAttributeConstraint->flag */
enum eGeometryAttributeConstraint_Flags {
  APPLY_TARGET_TRANSFORM = (1 << 0),
  MIX_LOC = (1 << 1),
  MIX_ROT = (1 << 2),
  MIX_SCALE = (1 << 3),
};

/** Attribute Domain */
enum Attribute_Domain {
  CON_ATTRIBUTE_DOMAIN_POINT = 0,
  CON_ATTRIBUTE_DOMAIN_EDGE = 1,
  CON_ATTRIBUTE_DOMAIN_FACE = 2,
  CON_ATTRIBUTE_DOMAIN_FACE_CORNER = 3,
  CON_ATTRIBUTE_DOMAIN_CURVE = 4,
  CON_ATTRIBUTE_DOMAIN_INSTANCE = 5,
};

/** Attribute Data Type*/
enum Attribute_Data_Type {
  CON_ATTRIBUTE_VECTOR = 0,
  CON_ATTRIBUTE_QUATERNION = 1,
  CON_ATTRIBUTE_4X4MATRIX = 2,
};

/** Attribute Component Mix Mode */
enum Attribute_MixMode {
  /* Replace rotation channel values. */
  CON_ATTRIBUTE_MIX_REPLACE = 0,
  /* Multiply the copied transformation on the left, handling loc/rot/scale separately. */
  CON_ATTRIBUTE_MIX_BEFORE_SPLIT = 1,
  /* Multiply the copied transformation on the right, handling loc/rot/scale separately. */
  CON_ATTRIBUTE_MIX_AFTER_SPLIT = 2,
  /* Multiply the copied transformation on the left, using simple matrix multiplication. */
  CON_ATTRIBUTE_MIX_BEFORE_FULL = 3,
  /* Multiply the copied transformation on the right, using simple matrix multiplication. */
  CON_ATTRIBUTE_MIX_AFTER_FULL = 4,
};

/* bConstraint->type
 * - Do not ever change the order of these, or else files could get
 *   broken as their correct value cannot be resolved
 */
enum eBConstraint_Types {
  /** Invalid/legacy constraint */
  CONSTRAINT_TYPE_NULL = 0,
  CONSTRAINT_TYPE_CHILDOF = 1,
  CONSTRAINT_TYPE_TRACKTO = 2,
  CONSTRAINT_TYPE_KINEMATIC = 3,
  CONSTRAINT_TYPE_FOLLOWPATH = 4,
  CONSTRAINT_TYPE_ROTLIMIT = 5,
  CONSTRAINT_TYPE_LOCLIMIT = 6,
  CONSTRAINT_TYPE_SIZELIMIT = 7,
  CONSTRAINT_TYPE_ROTLIKE = 8,
  CONSTRAINT_TYPE_LOCLIKE = 9,
  CONSTRAINT_TYPE_SIZELIKE = 10,
  /* CONSTRAINT_TYPE_DEPRECATED = 11, */
  CONSTRAINT_TYPE_ACTION = 12,
  CONSTRAINT_TYPE_LOCKTRACK = 13,
  CONSTRAINT_TYPE_DISTLIMIT = 14,
  CONSTRAINT_TYPE_STRETCHTO = 15,
  /** floor constraint */
  CONSTRAINT_TYPE_MINMAX = 16,
  /* CONSTRAINT_TYPE_DEPRECATED = 17 */
  CONSTRAINT_TYPE_CLAMPTO = 18,
  /** transformation (loc/rot/size -> loc/rot/size) constraint */
  CONSTRAINT_TYPE_TRANSFORM = 19,
  /** shrinkwrap (loc/rot) constraint */
  CONSTRAINT_TYPE_SHRINKWRAP = 20,
  /** Tracking constraint that minimizes twisting */
  CONSTRAINT_TYPE_DAMPTRACK = 21,
  CONSTRAINT_TYPE_SPLINEIK = 22,
  /** Copy transform matrix */
  CONSTRAINT_TYPE_TRANSLIKE = 23,
  /** Maintain volume during scaling */
  CONSTRAINT_TYPE_SAMEVOL = 24,
  CONSTRAINT_TYPE_PIVOT = 25,
  CONSTRAINT_TYPE_FOLLOWTRACK = 26,
  CONSTRAINT_TYPE_CAMERASOLVER = 27,
  CONSTRAINT_TYPE_OBJECTSOLVER = 28,
  CONSTRAINT_TYPE_TRANSFORM_CACHE = 29,
  CONSTRAINT_TYPE_ARMATURE = 30,
  CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE = 31,

  /* This should be the last entry in this list. */
  NUM_CONSTRAINT_TYPES,
};

/* bConstraint->flag */
/* flags 0x2 (1 << 1) and 0x8 (1 << 3) were used in past */
/* flag 0x20 (1 << 5) was used to indicate that a constraint was evaluated
 *                    using a 'local' hack for pose-bones only. */
enum eBConstraint_Flags {
#ifdef DNA_DEPRECATED_ALLOW
  /* Expansion for old box constraint layouts. Just for versioning. */
  CONSTRAINT_EXPAND_DEPRECATED = (1 << 0),
#endif
  /* Constraint is disabled because it is considered invalid. `is_valid` in RNA. */
  CONSTRAINT_DISABLE = (1 << 2),
  /* to indicate which Ipo should be shown, maybe for 3d access later too */
  CONSTRAINT_ACTIVE = (1 << 4),
  /* to indicate that the owner's space should only be changed into ownspace, but not out of it */
  CONSTRAINT_SPACEONCE = (1 << 6),
  /* influence ipo is on constraint itself, not in action channel */
  CONSTRAINT_OWN_IPO = (1 << 7),
  /* Constraint is disabled by the user or the animation system (eye icon in the interface). */
  CONSTRAINT_OFF = (1 << 9),
  /* use bbone curve shape when calculating headtail values (also used by dependency graph!) */
  CONSTRAINT_BBONE_SHAPE = (1 << 10),
  /* That constraint has been inserted in local override (i.e. it can be fully edited!). */
  CONSTRAINT_OVERRIDE_LIBRARY_LOCAL = (1 << 11),
  /* use full transformation (not just segment locations) - only set at runtime. */
  CONSTRAINT_BBONE_SHAPE_FULL = (1 << 12),
};

/* bConstraint->ownspace/tarspace */
enum eBConstraint_SpaceTypes {
  /** Default for all - world-space. */
  CONSTRAINT_SPACE_WORLD = 0,
  /** For all - custom space. */
  CONSTRAINT_SPACE_CUSTOM = 5,
  /**
   * For objects (relative to parent/without parent influence),
   * for bones (along normals of bone, without parent/rest-positions).
   */
  CONSTRAINT_SPACE_LOCAL = 1,
  /** For posechannels - pose space. */
  CONSTRAINT_SPACE_POSE = 2,
  /** For posechannels - local with parent. */
  CONSTRAINT_SPACE_PARLOCAL = 3,
  /** For posechannels - local converted to the owner bone orientation. */
  CONSTRAINT_SPACE_OWNLOCAL = 6,
  /** For files from between 2.43-2.46 (should have been parlocal). */
  CONSTRAINT_SPACE_INVALID = 4, /* do not exchange for anything! */
};

/* Common enum for constraints that support override. */
enum eConstraint_EulerOrder {
  /** Automatic euler mode. */
  CONSTRAINT_EULER_AUTO = 0,

  /** Explicit euler rotation modes - must sync with BLI_math_rotation.h defines. */
  CONSTRAINT_EULER_XYZ = 1,
  CONSTRAINT_EULER_XZY = 2,
  CONSTRAINT_EULER_YXZ = 3,
  CONSTRAINT_EULER_YZX = 4,
  CONSTRAINT_EULER_ZXY = 5,
  CONSTRAINT_EULER_ZYX = 6,
};

/** #bRotateLikeConstraint.flag */
enum eCopyRotation_Flags {
  ROTLIKE_X = (1 << 0),
  ROTLIKE_Y = (1 << 1),
  ROTLIKE_Z = (1 << 2),
  ROTLIKE_X_INVERT = (1 << 4),
  ROTLIKE_Y_INVERT = (1 << 5),
  ROTLIKE_Z_INVERT = (1 << 6),
#ifdef DNA_DEPRECATED_ALLOW
  ROTLIKE_OFFSET = (1 << 7),
#endif
};

/** #bRotateLikeConstraint.mix_mode */
enum eCopyRotation_MixMode {
  /* Replace rotation channel values. */
  ROTLIKE_MIX_REPLACE = 0,
  /* Legacy Offset mode - don't use. */
  ROTLIKE_MIX_OFFSET = 1,
  /* Add Euler components together. */
  ROTLIKE_MIX_ADD = 2,
  /* Multiply the copied rotation on the left. */
  ROTLIKE_MIX_BEFORE = 3,
  /* Multiply the copied rotation on the right. */
  ROTLIKE_MIX_AFTER = 4,
};

/** #bLocateLikeConstraint.flag */
enum eCopyLocation_Flags {
  LOCLIKE_X = (1 << 0),
  LOCLIKE_Y = (1 << 1),
  LOCLIKE_Z = (1 << 2),
  /** LOCLIKE_TIP is a deprecated option... use headtail=1.0f instead */
  LOCLIKE_TIP = (1 << 3),
  LOCLIKE_X_INVERT = (1 << 4),
  LOCLIKE_Y_INVERT = (1 << 5),
  LOCLIKE_Z_INVERT = (1 << 6),
  LOCLIKE_OFFSET = (1 << 7),
};

/** #bSizeLikeConstraint.flag */
enum eCopyScale_Flags {
  SIZELIKE_X = (1 << 0),
  SIZELIKE_Y = (1 << 1),
  SIZELIKE_Z = (1 << 2),
  SIZELIKE_OFFSET = (1 << 3),
  SIZELIKE_MULTIPLY = (1 << 4),
  SIZELIKE_UNIFORM = (1 << 5),
};

/** #bTransLikeConstraint.flag */
enum eCopyTransforms_Flags {
  /* Remove shear from the target matrix. */
  TRANSLIKE_REMOVE_TARGET_SHEAR = (1 << 0),
};

/** #bTransLikeConstraint.mix_mode */
enum eCopyTransforms_MixMode {
  /* Replace rotation channel values. */
  TRANSLIKE_MIX_REPLACE = 0,
  /* Multiply the copied transformation on the left, with anti-shear scale handling. */
  TRANSLIKE_MIX_BEFORE = 1,
  /* Multiply the copied transformation on the right, with anti-shear scale handling. */
  TRANSLIKE_MIX_AFTER = 2,
  /* Multiply the copied transformation on the left, handling loc/rot/scale separately. */
  TRANSLIKE_MIX_BEFORE_SPLIT = 3,
  /* Multiply the copied transformation on the right, handling loc/rot/scale separately. */
  TRANSLIKE_MIX_AFTER_SPLIT = 4,
  /* Multiply the copied transformation on the left, using simple matrix multiplication. */
  TRANSLIKE_MIX_BEFORE_FULL = 5,
  /* Multiply the copied transformation on the right, using simple matrix multiplication. */
  TRANSLIKE_MIX_AFTER_FULL = 6,
};

/* bTransformConstraint.to/from */
enum eTransform_ToFrom {
  TRANS_LOCATION = 0,
  TRANS_ROTATION = 1,
  TRANS_SCALE = 2,
};

/** #bTransformConstraint.mix_mode_loc */
enum eTransform_MixModeLoc {
  /* Add component values together (default). */
  TRANS_MIXLOC_ADD = 0,
  /* Replace component values. */
  TRANS_MIXLOC_REPLACE = 1,
};

/** #bTransformConstraint.mix_mode_rot */
enum eTransform_MixModeRot {
  /* Add component values together (default). */
  TRANS_MIXROT_ADD = 0,
  /* Replace component values. */
  TRANS_MIXROT_REPLACE = 1,
  /* Multiply the generated rotation on the left. */
  TRANS_MIXROT_BEFORE = 2,
  /* Multiply the generated rotation on the right. */
  TRANS_MIXROT_AFTER = 3,
};

/** #bTransformConstraint.mix_mode_scale */
enum eTransform_MixModeScale {
  /* Replace component values (default). */
  TRANS_MIXSCALE_REPLACE = 0,
  /* Multiply component values together. */
  TRANS_MIXSCALE_MULTIPLY = 1,
};

/** #bSameVolumeConstraint.free_axis */
enum eSameVolume_Axis {
  SAMEVOL_X = 0,
  SAMEVOL_Y = 1,
  SAMEVOL_Z = 2,
};

/** #bSameVolumeConstraint.mode */
enum eSameVolume_Mode {
  /* Strictly maintain the volume, overriding non-free axis scale. */
  SAMEVOL_STRICT = 0,
  /* Maintain the volume when scale is uniform, pass non-uniform other axis scale through. */
  SAMEVOL_UNIFORM = 1,
  /* Maintain the volume when scaled only on the free axis, pass other axis scale through. */
  SAMEVOL_SINGLE_AXIS = 2,
};

/** #bActionConstraint.flag */
enum eActionConstraint_Flags {
  /* Bones use "object" part of target action, instead of "same bone name" part */
  ACTCON_BONE_USE_OBJECT_ACTION = (1 << 0),
  /* Ignore the transform of 'tar' and use 'eval_time' instead: */
  ACTCON_USE_EVAL_TIME = (1 << 1),
};

/** #bActionConstraint.mix_mode */
enum eActionConstraint_MixMode {
  /* Replace the input transformation. */
  ACTCON_MIX_REPLACE = 6,
  /* Multiply the action transformation on the right. */
  ACTCON_MIX_AFTER_FULL = 0,
  /* Multiply the action transformation on the left. */
  ACTCON_MIX_BEFORE_FULL = 3,
  /* Multiply the action transformation on the right, with anti-shear scale handling. */
  ACTCON_MIX_AFTER = 1,
  /* Multiply the action transformation on the left, with anti-shear scale handling. */
  ACTCON_MIX_BEFORE = 2,
  /* Separately combine Translation, Rotation and Scale, with rotation on the right. */
  ACTCON_MIX_AFTER_SPLIT = 4,
  /* Separately combine Translation, Rotation and Scale, with rotation on the left. */
  ACTCON_MIX_BEFORE_SPLIT = 5,
};

/* Locked-Axis Values (Locked Track) */
enum eLockAxis_Modes {
  LOCK_X = 0,
  LOCK_Y = 1,
  LOCK_Z = 2,
};

/* Up-Axis Values (TrackTo and Locked Track) */
enum eUpAxis_Modes {
  UP_X = 0,
  UP_Y = 1,
  UP_Z = 2,
};

/* Tracking axis (TrackTo, Locked Track, Damped Track) and minmax (floor) constraint */
enum eTrackToAxis_Modes {
  TRACK_X = 0,
  TRACK_Y = 1,
  TRACK_Z = 2,
  TRACK_nX = 3,
  TRACK_nY = 4,
  TRACK_nZ = 5,
};

/* Shrinkwrap flags */
enum eShrinkwrap_Flags {
  /* Also ray-cast in the opposite direction. */
  CON_SHRINKWRAP_PROJECT_OPPOSITE = (1 << 0),
  /* Invert the cull mode when projecting opposite. */
  CON_SHRINKWRAP_PROJECT_INVERT_CULL = (1 << 1),
  /* Align the specified axis to the target normal. */
  CON_SHRINKWRAP_TRACK_NORMAL = (1 << 2),

  /* Ignore front faces in project; same value as MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE */
  CON_SHRINKWRAP_PROJECT_CULL_FRONTFACE = (1 << 3),
  /* Ignore back faces in project; same value as MOD_SHRINKWRAP_CULL_TARGET_BACKFACE */
  CON_SHRINKWRAP_PROJECT_CULL_BACKFACE = (1 << 4),
};

/* FollowPath flags */
enum eFollowPath_Flags {
  FOLLOWPATH_FOLLOW = (1 << 0),
  FOLLOWPATH_STATIC = (1 << 1),
  FOLLOWPATH_RADIUS = (1 << 2),
};

/* bTrackToConstraint->flags */
enum eTrackTo_Flags {
  TARGET_Z_UP = (1 << 0),
};

/* Stretch To Constraint -> volmode */
enum eStretchTo_VolMode {
  VOLUME_XZ = 0,
  VOLUME_X = 1,
  VOLUME_Z = 2,
  NO_VOLUME = 3,
};

/* Stretch To Constraint -> plane mode */
enum eStretchTo_PlaneMode {
  PLANE_X = 0,
  SWING_Y = 1,
  PLANE_Z = 2,
};

/* Clamp-To Constraint ->flag */
enum eClampTo_Modes {
  CLAMPTO_AUTO = 0,
  CLAMPTO_X = 1,
  CLAMPTO_Y = 2,
  CLAMPTO_Z = 3,
};

/* ClampTo Constraint ->flag2 */
enum eClampTo_Flags {
  CLAMPTO_CYCLIC = (1 << 0),
};

/* bKinematicConstraint->flag */
enum eKinematic_Flags {
  CONSTRAINT_IK_TIP = (1 << 0),
  CONSTRAINT_IK_ROT = (1 << 1),
  /* targetless */
  CONSTRAINT_IK_AUTO = (1 << 2),
  /* autoik */
  CONSTRAINT_IK_TEMP = (1 << 3),
  CONSTRAINT_IK_STRETCH = (1 << 4),
  CONSTRAINT_IK_POS = (1 << 5),
  CONSTRAINT_IK_SETANGLE = (1 << 6),
  CONSTRAINT_IK_GETANGLE = (1 << 7),
  /* limit axis */
  CONSTRAINT_IK_NO_POS_X = (1 << 8),
  CONSTRAINT_IK_NO_POS_Y = (1 << 9),
  CONSTRAINT_IK_NO_POS_Z = (1 << 10),
  CONSTRAINT_IK_NO_ROT_X = (1 << 11),
  CONSTRAINT_IK_NO_ROT_Y = (1 << 12),
  CONSTRAINT_IK_NO_ROT_Z = (1 << 13),
  /* axis relative to target */
  CONSTRAINT_IK_TARGETAXIS = (1 << 14),
};

/** #bSplineIKConstraint::flag */
enum eSplineIK_Flags {
  /** Chain has been attached to spline. */
  CONSTRAINT_SPLINEIK_BOUND = (1 << 0),
  /** Root of chain is not influenced by the constraint. */
  CONSTRAINT_SPLINEIK_NO_ROOT = (1 << 1),
#ifdef DNA_DEPRECATED_ALLOW
  /** Bones in the chain should not scale to fit the curve. */
  CONSTRAINT_SPLINEIK_SCALE_LIMITED = (1 << 2),
#endif
  /** Evenly distribute the bones along the path regardless of length. */
  CONSTRAINT_SPLINEIK_EVENSPLITS = (1 << 3),
  /** Don't adjust the X and Z scaling of the bones by the curve radius. */
  CONSTRAINT_SPLINEIK_NO_CURVERAD = (1 << 4),

  /** For "volumetric" XZ scale mode, limit the minimum or maximum scale values. */
  CONSTRAINT_SPLINEIK_USE_BULGE_MIN = (1 << 5),
  CONSTRAINT_SPLINEIK_USE_BULGE_MAX = (1 << 6),

  /** Apply volume preservation over original scaling of the bone. */
  CONSTRAINT_SPLINEIK_USE_ORIGINAL_SCALE = (1 << 7),
};

/** #bSplineIKConstraint::xzScaleMode */
enum eSplineIK_XZScaleModes {
  /** No X/Z scaling. */
  CONSTRAINT_SPLINEIK_XZS_NONE = 0,
  /** Bones in the chain should take their X/Z scales from the original scaling. */
  CONSTRAINT_SPLINEIK_XZS_ORIGINAL = 1,
  /** X/Z scales are the inverse of the Y-scale. */
  CONSTRAINT_SPLINEIK_XZS_INVERSE = 2,
  /** X/Z scales are computed using a volume preserving technique (from Stretch To constraint). */
  CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC = 3,
};

/** #bSplineIKConstraint::yScaleMode */
enum eSplineIK_YScaleModes {
  /** No Y scaling. */
  CONSTRAINT_SPLINEIK_YS_NONE = 0,
  /** Bones in the chain should be scaled to fit the length of the curve. */
  CONSTRAINT_SPLINEIK_YS_FIT_CURVE = 1,
  /** Bones in the chain should take their y scales from the original scaling. */
  CONSTRAINT_SPLINEIK_YS_ORIGINAL = 2,
};

/** #bArmatureConstraint::flag */
enum eArmature_Flags {
  /** use dual quaternion blending */
  CONSTRAINT_ARMATURE_QUATERNION = (1 << 0),
  /** use envelopes */
  CONSTRAINT_ARMATURE_ENVELOPE = (1 << 1),
  /** use current bone location */
  CONSTRAINT_ARMATURE_CUR_LOCATION = (1 << 2),
};

/* MinMax (floor) flags */
enum eFloor_Flags {
  /* MINMAX_STICKY = (1 << 0), */ /* Deprecated. */
  /* MINMAX_STUCK = (1 << 1), */  /* Deprecated. */
  MINMAX_USEROT = (1 << 2),
};

/* transform limiting constraints -> flag2 */
enum eTransformLimits_Flags2 {
  /* not used anymore - for older Limit Location constraints only */
  /* LIMIT_NOPARENT = (1 << 0), */ /* UNUSED */
  /* for all Limit constraints - allow to be used during transform? */
  LIMIT_TRANSFORM = (1 << 1),
};

/* transform limiting constraints -> flag. */
enum eTransformLimits_Flags {
  LIMIT_XMIN = (1 << 0),
  LIMIT_XMAX = (1 << 1),
  LIMIT_YMIN = (1 << 2),
  LIMIT_YMAX = (1 << 3),
  LIMIT_ZMIN = (1 << 4),
  LIMIT_ZMAX = (1 << 5),
};

/* limit rotation constraint -> flag. */
enum eRotLimit_Flags {
  LIMIT_XROT = (1 << 0),
  LIMIT_YROT = (1 << 1),
  LIMIT_ZROT = (1 << 2),

  /* Use the legacy behavior of the Limit Rotation constraint. See the
   * implementation of `rotlimit_evaluate()` in constraint.cc for more
   * details. */
  LIMIT_ROT_LEGACY_BEHAVIOR = (1 << 3),
};

/* distance limit constraint */
/* bDistLimitConstraint->flag */
enum eDistLimit_Flag {
  /* "soft" cushion effect when reaching the limit sphere */ /* NOT IMPLEMENTED! */
  LIMITDIST_USESOFT = (1 << 0),
  /* as for all Limit constraints - allow to be used during transform? */
  LIMITDIST_TRANSFORM = (1 << 1),
};

/* bDistLimitConstraint->mode */
enum eDistLimit_Modes {
  LIMITDIST_INSIDE = 0,
  LIMITDIST_OUTSIDE = 1,
  LIMITDIST_ONSURFACE = 2,
};

/* ChildOf Constraint -> flag */
enum eChildOf_Flags {
  CHILDOF_LOCX = (1 << 0),
  CHILDOF_LOCY = (1 << 1),
  CHILDOF_LOCZ = (1 << 2),
  CHILDOF_ROTX = (1 << 3),
  CHILDOF_ROTY = (1 << 4),
  CHILDOF_ROTZ = (1 << 5),
  CHILDOF_SIZEX = (1 << 6),
  CHILDOF_SIZEY = (1 << 7),
  CHILDOF_SIZEZ = (1 << 8),
  CHILDOF_ALL = 511,
  /* Temporary flag used by the Set Inverse operator. */
  CHILDOF_SET_INVERSE = (1 << 9),
};

/**
 * Pivot Constraint
 *
 * Restrictions for Pivot Constraint axis to consider for enabling constraint.
 */
enum ePivotConstraint_Axis {
  /** Do not consider this activity-clamping. */
  PIVOTCON_AXIS_NONE = -1,

  /** Consider -VE X-axis rotations. */
  PIVOTCON_AXIS_X_NEG = 0,
  /** Consider -VE Y-axis rotations. */
  PIVOTCON_AXIS_Y_NEG = 1,
  /** Consider -VE Z-axis rotations. */
  PIVOTCON_AXIS_Z_NEG = 2,

  /** Consider +VE X-axis rotations. */
  PIVOTCON_AXIS_X = 3,
  /** Consider +VE Y-axis rotations. */
  PIVOTCON_AXIS_Y = 4,
  /** Consider +VE Z-axis rotations. */
  PIVOTCON_AXIS_Z = 5,
};

/* settings for Pivot Constraint in general */
enum ePivotConstraint_Flag {
  /* offset is to be interpreted as being a fixed-point in space */
  PIVOTCON_FLAG_OFFSET_ABS = (1 << 0),
  /* rotation-based activation uses negative rotation to drive result */
  PIVOTCON_FLAG_ROTACT_NEG = (1 << 1),
};

enum eFollowTrack_Flags {
  FOLLOWTRACK_ACTIVECLIP = (1 << 0),
  FOLLOWTRACK_USE_3D_POSITION = (1 << 1),
  FOLLOWTRACK_USE_UNDISTORTION = (1 << 2),
};

enum eFollowTrack_FrameMethod {
  FOLLOWTRACK_FRAME_STRETCH = 0,
  FOLLOWTRACK_FRAME_FIT = 1,
  FOLLOWTRACK_FRAME_CROP = 2,
};

/* CameraSolver Constraint -> flag */
enum eCameraSolver_Flags {
  CAMERASOLVER_ACTIVECLIP = (1 << 0),
};

/* ObjectSolver Constraint -> flag */
enum eObjectSolver_Flags {
  OBJECTSOLVER_ACTIVECLIP = (1 << 0),
  /* Temporary flag used by the Set Inverse operator. */
  OBJECTSOLVER_SET_INVERSE = (1 << 1),
};

/* ObjectSolver Constraint -> flag */
enum eStretchTo_Flags {
  STRETCHTOCON_USE_BULGE_MIN = (1 << 0),
  STRETCHTOCON_USE_BULGE_MAX = (1 << 1),
};

/** A Constraint. */
struct bConstraint {
  struct bConstraint *next = nullptr, *prev = nullptr;

  /** Constraint data (a valid constraint type). */
  void *data = nullptr;
  /** Constraint type. */
  short type = 0;
  /** Flag - General Settings. */
  short flag = 0;

  /** Space that owner should be evaluated in. */
  char ownspace = 0;
  /** Space that target should be evaluated in (only used if 1 target). */
  char tarspace = 0;

  /* An "expand" bit for each of the constraint's (sub)panels (uiPanelDataExpansion). */
  short ui_expand_flag = 0;

  /** Object to use as target for Custom Space of owner. */
  struct Object *space_object = nullptr;
  /** Sub-target for Custom Space of owner - pose-channel or vertex-group name. */
  char space_subtarget[/*MAX_NAME*/ 64] = "";

  /** Constraint name. */
  char name[/*MAX_NAME*/ 64] = "";

  /** Amount of influence exerted by constraint (0.0-1.0). */
  float enforce = 0;
  /** Point along `subtarget` bone where the actual target is. 0=head (default for all), 1=tail. */
  float headtail = 0;

  /* Below are read-only fields that are set at runtime
   * by the solver for use in the GE (only IK at the moment). */
  /** Residual error on constraint expressed in blender unit. */
  float lin_error = 0;
  /** Residual error on constraint expressed in radiant. */
  float rot_error = 0;
};

/* Multiple-target constraints --------------------- */

/* This struct defines a constraint target.
 * It is used during constraint solving regardless of how many targets the
 * constraint has.
 */
struct bConstraintTarget {
  struct bConstraintTarget *next = nullptr, *prev = nullptr;

  /** Object to use as target. */
  struct Object *tar = nullptr;
  /** Sub-target - pose-channel or vertex-group name. */
  char subtarget[/*MAX_NAME*/ 64] = "";

  /** Matrix used during constraint solving - should be cleared before each use. */
  float matrix[4][4] = {};

  /** Space that target should be evaluated in (overrides bConstraint->tarspace). */
  short space = 0;
  /** Runtime settings (for editor, etc.). */
  short flag = 0;
  /** Type of target (eConstraintObType). */
  short type = 0;
  /** Rotation order for target (as defined in BLI_math_rotation.h). */
  short rotOrder = 0;
  /** Weight for armature deform. */
  float weight = 0;
  char _pad[4] = {};
};

/* Inverse-Kinematics (IK) constraint
 * This constraint supports a variety of mode determine by the type field
 * according to eConstraint_IK_Type.
 * Some fields are used by all types, some are specific to some types
 * This is indicated in the comments for each field
 */
struct bKinematicConstraint {
  /** All: target object in case constraint needs a target. */
  struct Object *tar = nullptr;
  /** All: Maximum number of iterations to try. */
  short iterations = 0;
  /** All & CopyPose: some options Like CONSTRAINT_IK_TIP. */
  short flag = 0;
  /** All: index to rootbone, if zero go all the way to mother bone. */
  short rootbone = 0;
  /** CopyPose: for auto-ik, maximum length of chain. */
  short max_rootbone = 0;
  /** All: String to specify sub-object target. */
  char subtarget[/*MAX_NAME*/ 64] = "";
  /** All: Pole vector target. */
  struct Object *poletar = nullptr;
  /** All: Pole vector sub-object target. */
  char polesubtarget[/*MAX_NAME*/ 64] = "";
  /** All: Pole vector rest angle. */
  float poleangle = 0;
  /** All: Weight of constraint in IK tree. */
  float weight = 0;
  /** CopyPose: Amount of rotation a target applies on chain. */
  float orientweight = 0;
  /** CopyPose: for target-less IK. */
  float grabtarget[3] = {};
  /** Sub-type of IK constraint: #eConstraint_IK_Type. */
  short type = 0;
  /** Distance: how to limit in relation to clamping sphere: LIMITDIST_... */
  short mode = 0;
  /** Distance: distance (radius of clamping sphere) from target. */
  float dist = 0;
};

/* Spline IK Constraint
 * Aligns 'n' bones to the curvature defined by the curve,
 * with the chain ending on the bone that owns this constraint,
 * and starting on the nth parent.
 */
struct bSplineIKConstraint {
  /* target(s) */
  /** Curve object (with follow path enabled) which drives the bone chain. */
  struct Object *tar = nullptr;

  /* binding details */
  /**
   * Array of numpoints items,
   * denoting parametric positions along curve that joints should follow.
   */
  float *points = nullptr;
  /** Number of points to bound in points array. */
  short numpoints = 0;
  /** Number of bones ('n') that are in the chain. */
  short chainlen = 0;

  /* settings */
  /** General settings for constraint. */
  short flag = 0;
  /** Method used for determining the x & z scaling of the bones. */
  short xzScaleMode = 0;
  /** Method used for determining the y scaling of the bones. */
  short yScaleMode = 0;
  short _pad[3] = {};

  /* volume preservation settings */
  float bulge = 0;
  float bulge_min = 0;
  float bulge_max = 0;
  float bulge_smooth = 0;
};

/* Armature Constraint */
struct bArmatureConstraint {
  /** General settings/state indicators accessed by bitmapping. */
  int flag = 0;
  char _pad[4] = {};

  /** A list of targets that this constraint has (bConstraintTarget-s). */
  ListBaseT<bConstraintTarget> targets = {nullptr, nullptr};
};

/* Single-target sub-object constraints --------------------- */

/* Track To Constraint */
struct bTrackToConstraint {
  struct Object *tar = nullptr;
  /**
   * NOTE(@theeth): I'll be using reserved1 and reserved2 as Track and Up flags,
   * not sure if that's what they were intended for anyway.
   * Not sure either if it would create backward incompatibility if I were to rename them.
   */
  int reserved1 = 0;
  int reserved2 = 0;
  int flags = 0;
  char _pad[4] = {};
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Copy Rotation Constraint */
struct bRotateLikeConstraint {
  struct Object *tar = nullptr;
  int flag = 0;
  char euler_order = 0;
  char mix_mode = 0;
  char _pad[2] = {};
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Copy Location Constraint */
struct bLocateLikeConstraint {
  struct Object *tar = nullptr;
  int flag = 0;
  int reserved1 = 0;
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Copy Scale Constraint */
struct bSizeLikeConstraint {
  struct Object *tar = nullptr;
  int flag = 0;
  float power = 0;
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Maintain Volume Constraint */
struct bSameVolumeConstraint {
  char free_axis = 0;
  char mode = 0;
  char _pad[2] = {};
  float volume = 0;
};

/* Copy Transform Constraint */
struct bTransLikeConstraint {
  struct Object *tar = nullptr;
  int flag = 0;
  char mix_mode = 0;
  char _pad[3] = {};
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Floor Constraint */
struct bMinMaxConstraint {
  struct Object *tar = nullptr;
  int minmaxflag = 0;
  float offset = 0;
  int flag = 0;
  char subtarget[/*MAX_NAME*/ 64] = "";
  int _pad = {};
};

/* Action Constraint */
struct bActionConstraint {
  struct Object *tar = nullptr;
  /** What transform 'channel' drives the result. */
  short type = 0;
  /** Was used in versions prior to the Constraints recode. */
  short local = 0;
  /** 'Start' frame in the Action. */
  int start = 0;
  /** 'End' frame in the Action. */
  int end = 0;
  /** 'Start' value of the target property. Note that this may be larger than `max`. */
  float min = 0;
  /** 'End' value of the target property. Note that this may be smaller than `min`. */
  float max = 0;
  int flag = 0;
  char mix_mode = 0;
  char _pad[3] = {};
  float eval_time = 0; /* Only used when flag ACTCON_USE_EVAL_TIME is set. */
  struct bAction *act = nullptr;
  int32_t action_slot_handle = 0;
  char last_slot_identifier[/*MAX_ID_NAME*/ 258] = "";
  char _pad1[2] = {};
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Locked Axis Tracking constraint */
struct bLockTrackConstraint {
  struct Object *tar = nullptr;
  int trackflag = 0;
  int lockflag = 0;
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Damped Tracking constraint */
struct bDampTrackConstraint {
  struct Object *tar = nullptr;
  int trackflag = 0;
  char _pad[4] = {};
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Follow Path constraints */
struct bFollowPathConstraint {
  /** Must be path object. */
  struct Object *tar = nullptr;

  /** Offset in time on the path (in frames), when NOT using 'fixed position'. */
  float offset = 0;
  /** Parametric offset factor defining position along path, when using 'fixed position'. */
  float offset_fac = 0;

  int followflag = 0;

  short trackflag = 0;
  short upflag = 0;
};

/* Stretch to constraint */
struct bStretchToConstraint {
  struct Object *tar = nullptr;
  int flag = 0;
  int volmode = 0;
  int plane = 0;
  float orglength = 0;
  float bulge = 0;
  float bulge_min = 0;
  float bulge_max = 0;
  float bulge_smooth = 0;
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* DEPRECATED: Rigid Body constraint */
struct bRigidBodyJointConstraint {
  struct Object *tar = nullptr;
  struct Object *child = nullptr;
  int type = 0;
  float pivX = 0;
  float pivY = 0;
  float pivZ = 0;
  float axX = 0;
  float axY = 0;
  float axZ = 0;
  float minLimit[6] = {};
  float maxLimit[6] = {};
  float extraFz = 0;
  short flag = 0;
  char _pad[6] = {};
};

/* Clamp-To Constraint */
struct bClampToConstraint {
  /** 'target' must be a curve. */
  struct Object *tar = nullptr;
  /** Which axis/plane to compare owner's location on. */
  int flag = 0;
  /** For legacy reasons, this is flag2. used for any extra settings. */
  int flag2 = 0;
};

/* Child Of Constraint */
struct bChildOfConstraint {
  /** Object which will act as parent (or target comes from). */
  struct Object *tar = nullptr;
  /** Settings. */
  int flag = 0;
  char _pad[4] = {};
  /** Parent-inverse matrix to use. */
  float invmat[4][4] = {};
  /** String to specify a sub-object target. */
  char subtarget[/*MAX_NAME*/ 64] = "";
};

/* Generic Transform->Transform Constraint */
struct bTransformConstraint {
  /** Target (i.e. 'driver' object/bone). */
  struct Object *tar = nullptr;
  char subtarget[/*MAX_NAME*/ 64] = "";

  /** Can be loc(0), rot(1) or size(2). */
  short from = 0, to = 0;
  /** Defines which target-axis deform is copied by each owner-axis. */
  char map[3] = "";
  /** Extrapolate motion? if 0, confine to ranges. */
  char expo = 0;

  /** Input rotation type - uses the same values as driver targets. */
  char from_rotation_mode = 0;
  /** Output euler order override. */
  char to_euler_order = 0;

  /** Mixing modes for location, rotation, and scale. */
  char mix_mode_loc = 0;
  char mix_mode_rot = 0;
  char mix_mode_scale = 0;

  char _pad[3] = {};

  /** From_min/max defines range of target transform. */
  float from_min[3] = {};
  /** To map on to to_min/max range. */
  float from_max[3] = {};
  /** Range of motion on owner caused by target. */
  float to_min[3] = {};
  float to_max[3] = {};

  /** From_min/max defines range of target transform. */
  float from_min_rot[3] = {};
  /** To map on to to_min/max range. */
  float from_max_rot[3] = {};
  /** Range of motion on owner caused by target. */
  float to_min_rot[3] = {};
  float to_max_rot[3] = {};

  /** From_min/max defines range of target transform. */
  float from_min_scale[3] = {};
  /** To map on to to_min/max range. */
  float from_max_scale[3] = {};
  /** Range of motion on owner caused by target. */
  float to_min_scale[3] = {};
  float to_max_scale[3] = {};
};

/* Pivot Constraint */
struct bPivotConstraint {
  /* Pivot Point:
   * Either target object + offset, or just offset is used
   */
  /** Target object (optional). */
  struct Object *tar = nullptr;
  /** Subtarget name (optional). */
  char subtarget[/*MAX_NAME*/ 64] = "";
  /** Offset from the target to use, regardless of whether it exists. */
  float offset[3] = {};

  /* Rotation-driven activation:
   * This option provides easier one-stop setups for foot-rolls.
   */
  /** Rotation axes to consider for this (#ePivotConstraint_Axis). */
  short rotAxis = 0;

  /* General flags */
  /** #ePivotConstraint_Flag. */
  short flag = 0;
};

/* transform limiting constraints - zero target ---------------------------- */
/* Limit Location Constraint */
struct bLocLimitConstraint {
  float xmin = 0, xmax = 0;
  float ymin = 0, ymax = 0;
  float zmin = 0, zmax = 0;
  short flag = 0;
  short flag2 = 0;
};

/* Limit Rotation Constraint */
struct bRotLimitConstraint {
  float xmin = 0, xmax = 0;
  float ymin = 0, ymax = 0;
  float zmin = 0, zmax = 0;
  short flag = 0;
  short flag2 = 0;
  char euler_order = 0;
  char _pad[3] = {};
};

/* Limit Scale Constraint */
struct bSizeLimitConstraint {
  float xmin = 0, xmax = 0;
  float ymin = 0, ymax = 0;
  float zmin = 0, zmax = 0;
  short flag = 0;
  short flag2 = 0;
};

/* Limit Distance Constraint */
struct bDistLimitConstraint {
  struct Object *tar = nullptr;
  char subtarget[/*MAX_NAME*/ 64] = "";

  /** Distance (radius of clamping sphere) from target. */
  float dist = 0;
  /** Distance from clamping-sphere to start applying 'fade'. */
  float soft = 0;

  /** Settings. */
  short flag = 0;
  /** How to limit in relation to clamping sphere. */
  short mode = 0;
  char _pad[4] = {};
};

/* ShrinkWrap Constraint */
struct bShrinkwrapConstraint {
  struct Object *target = nullptr;
  /** Distance to kept from target. */
  float dist = 0;
  /** Shrink type (look on MOD shrinkwrap for values). */
  short shrinkType = 0;
  /** Axis to project/constrain. */
  char projAxis = 0;
  /** Space to project axis in. */
  char projAxisSpace = 0;
  /** Distance to search. */
  float projLimit = 0;
  /** Inside/outside/on surface (see MOD shrinkwrap). */
  char shrinkMode = 0;
  /** Options. */
  char flag = 0;
  /** Axis to align to normal. */
  char trackAxis = 0;
  char _pad = {};
};

/* Follow Track constraints */
struct bFollowTrackConstraint {
  struct MovieClip *clip = nullptr;
  char track[/*MAX_NAME*/ 64] = "";
  int flag = 0;
  int frame_method = 0;
  char object[/*MAX_NAME*/ 64] = "";
  struct Object *camera = nullptr;
  struct Object *depth_ob = nullptr;
};

/* Camera Solver constraints */
struct bCameraSolverConstraint {
  struct MovieClip *clip = nullptr;
  int flag = 0;
  char _pad[4] = {};
};

/* Camera Solver constraints */
struct bObjectSolverConstraint {
  struct MovieClip *clip = nullptr;
  int flag = 0;
  char _pad[4] = {};
  char object[/*MAX_NAME*/ 64] = "";
  /** Parent-inverse matrix to use. */
  float invmat[4][4] = {};
  struct Object *camera = nullptr;
};

/* Transform matrix cache constraint */
struct bTransformCacheConstraint {
  struct CacheFile *cache_file = nullptr;
  char object_path[/*FILE_MAX*/ 1024] = "";

  /* Runtime. */
  struct CacheReader *reader = nullptr;
  char reader_object_path[/*FILE_MAX*/ 1024] = "";
};

/* Geometry Attribute Constraint */
struct bGeometryAttributeConstraint {
  struct Object *target = nullptr;
  char *attribute_name = nullptr;
  int32_t sample_index = 0;
  uint8_t apply_target_transform = 0;
  uint8_t mix_mode = 0;
  /* #Attribute_Domain */
  uint8_t domain = 0;
  /* #Attribute_Data_Type */
  uint8_t data_type = 0;
  /* #eGeometryAttributeConstraint_Flags */
  uint8_t flags = 0;
  char _pad0[7] = {};
};

/* ------------------------------------------ */

/* -------------------------------------- */

}  // namespace blender
