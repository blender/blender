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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * Constraint DNA data
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_CONSTRAINT_TYPES_H__
#define __DNA_CONSTRAINT_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

struct Action;
struct Ipo;
struct Text;

/* channels reside in Object or Action (ListBase) constraintChannels */
// XXX deprecated... old AnimSys
typedef struct bConstraintChannel {
  struct bConstraintChannel *next, *prev;
  struct Ipo *ipo;
  short flag;
  char name[30];
} bConstraintChannel;

/* A Constraint */
typedef struct bConstraint {
  struct bConstraint *next, *prev;

  /** Constraint data (a valid constraint type). */
  void *data;
  /** Constraint type.    */
  short type;
  /** Flag - General Settings.    */
  short flag;

  /** Space that owner should be evaluated in. */
  char ownspace;
  /** Space that target should be evaluated in (only used if 1 target). */
  char tarspace;

  /** Constraint name, MAX_NAME. */
  char name[64];

  char _pad[2];

  /** Amount of influence exherted by constraint (0.0-1.0). */
  float enforce;
  /** Point along subtarget bone where the actual target is. 0=head (default for all), 1=tail. */
  float headtail;

  /* old animation system, deprecated for 2.5. */
  /** Local influence ipo or driver */
  struct Ipo *ipo DNA_DEPRECATED;

  /* below are readonly fields that are set at runtime
   * by the solver for use in the GE (only IK atm) */
  /** Residual error on constraint expressed in blender unit. */
  float lin_error;
  /** Residual error on constraint expressed in radiant. */
  float rot_error;
} bConstraint;

/* Multiple-target constraints ---------------------  */

/* This struct defines a constraint target.
 * It is used during constraint solving regardless of how many targets the
 * constraint has.
 */
typedef struct bConstraintTarget {
  struct bConstraintTarget *next, *prev;

  /** Object to use as target. */
  struct Object *tar;
  /** Subtarget - pchan or vgroup name, MAX_ID_NAME-2. */
  char subtarget[64];

  /** Matrix used during constraint solving - should be cleared before each use. */
  float matrix[4][4];

  /** Space that target should be evaluated in (overrides bConstraint->tarspace). */
  short space;
  /** Runtime settings (for editor, etc.). */
  short flag;
  /** Type of target (eConstraintObType). */
  short type;
  /** Rotation order for target (as defined in BLI_math.h). */
  short rotOrder;
  /** Weight for armature deform. */
  float weight;
  char _pad[4];
} bConstraintTarget;

/* bConstraintTarget -> flag */
typedef enum eConstraintTargetFlag {
  /** temporary target-struct that needs to be freed after use */
  CONSTRAINT_TAR_TEMP = (1 << 0),
} eConstraintTargetFlag;

/* bConstraintTarget/bConstraintOb -> type */
typedef enum eConstraintObType {
  /** string is "" */
  CONSTRAINT_OBTYPE_OBJECT = 1,
  /** string is bone-name */
  CONSTRAINT_OBTYPE_BONE = 2,
  /** string is vertex-group name */
  CONSTRAINT_OBTYPE_VERT = 3,
  /** string is vertex-group name - is not available until curves get vgroups */
  CONSTRAINT_OBTYPE_CV = 4,
} eConstraintObType;

/* Python Script Constraint */
typedef struct bPythonConstraint {
  /** Text-buffer (containing script) to execute. */
  struct Text *text;
  /** 'id-properties' used to store custom properties for constraint. */
  IDProperty *prop;

  /** General settings/state indicators accessed by bitmapping. */
  int flag;
  /** Number of targets - usually only 1-3 are needed. */
  int tarnum;

  /** A list of targets that this constraint has (bConstraintTarget-s). */
  ListBase targets;

  /**
   * Target from previous implementation
   * (version-patch sets this to NULL on file-load).
   */
  struct Object *tar;
  /**
   * Subtarger from previous implementation
   * (version-patch sets this to "" on file-load), MAX_ID_NAME-2.
   */
  char subtarget[64];
} bPythonConstraint;

/* Inverse-Kinematics (IK) constraint
 * This constraint supports a variety of mode determine by the type field
 * according to eConstraint_IK_Type.
 * Some fields are used by all types, some are specific to some types
 * This is indicated in the comments for each field
 */
typedef struct bKinematicConstraint {
  /** All: target object in case constraint needs a target. */
  struct Object *tar;
  /** All: Maximum number of iterations to try. */
  short iterations;
  /** All & CopyPose: some options Like CONSTRAINT_IK_TIP. */
  short flag;
  /** All: index to rootbone, if zero go all the way to mother bone. */
  short rootbone;
  /** CopyPose: for auto-ik, maximum length of chain. */
  short max_rootbone;
  /** All: String to specify sub-object target, MAX_ID_NAME-2. */
  char subtarget[64];
  /** All: Pole vector target. */
  struct Object *poletar;
  /** All: Pole vector sub-object target, MAX_ID_NAME-2. */
  char polesubtarget[64];
  /** All: Pole vector rest angle. */
  float poleangle;
  /** All: Weight of constraint in IK tree. */
  float weight;
  /** CopyPose: Amount of rotation a target applies on chain. */
  float orientweight;
  /** CopyPose: for target-less IK. */
  float grabtarget[3];
  /** Subtype of IK constraint: eConstraint_IK_Type. */
  short type;
  /** Distance: how to limit in relation to clamping sphere: LIMITDIST_... */
  short mode;
  /** Distance: distance (radius of clamping sphere) from target. */
  float dist;
} bKinematicConstraint;

typedef enum eConstraint_IK_Type {
  /** 'standard' IK constraint: match position and/or orientation of target */
  CONSTRAINT_IK_COPYPOSE = 0,
  /** maintain distance with target */
  CONSTRAINT_IK_DISTANCE = 1,
} eConstraint_IK_Type;

/* Spline IK Constraint
 * Aligns 'n' bones to the curvature defined by the curve,
 * with the chain ending on the bone that owns this constraint,
 * and starting on the nth parent.
 */
typedef struct bSplineIKConstraint {
  /* target(s) */
  /** Curve object (with follow path enabled) which drives the bone chain. */
  struct Object *tar;

  /* binding details */
  /**
   * Array of numpoints items,
   * denoting parametric positions along curve that joints should follow.
   */
  float *points;
  /** Number of points to bound in points array. */
  short numpoints;
  /** Number of bones ('n') that are in the chain. */
  short chainlen;

  /* settings */
  /** General settings for constraint. */
  short flag;
  /** Method used for determining the x & z scaling of the bones. */
  short xzScaleMode;
  /** Method used for determining the y scaling of the bones. */
  short yScaleMode;
  short _pad[3];

  /* volume preservation settings */
  float bulge;
  float bulge_min;
  float bulge_max;
  float bulge_smooth;
} bSplineIKConstraint;

/* Armature Constraint */
typedef struct bArmatureConstraint {
  /** General settings/state indicators accessed by bitmapping. */
  int flag;
  char _pad[4];

  /** A list of targets that this constraint has (bConstraintTarget-s). */
  ListBase targets;
} bArmatureConstraint;

/* Single-target subobject constraints ---------------------  */

/* Track To Constraint */
typedef struct bTrackToConstraint {
  struct Object *tar;
  /**
   * I'll be using reserved1 and reserved2 as Track and Up flags,
   * not sure if that's what they were intended for anyway.
   * Not sure either if it would create backward incompatibility if I were to rename them.
   * - theeth
   */
  int reserved1;
  int reserved2;
  int flags;
  char _pad[4];
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bTrackToConstraint;

/* Copy Rotation Constraint */
typedef struct bRotateLikeConstraint {
  struct Object *tar;
  int flag;
  int reserved1;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bRotateLikeConstraint;

/* Copy Location Constraint */
typedef struct bLocateLikeConstraint {
  struct Object *tar;
  int flag;
  int reserved1;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bLocateLikeConstraint;

/* Copy Scale Constraint */
typedef struct bSizeLikeConstraint {
  struct Object *tar;
  int flag;
  int reserved1;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bSizeLikeConstraint;

/* Maintain Volume Constraint */
typedef struct bSameVolumeConstraint {
  int flag;
  float volume;
} bSameVolumeConstraint;

/* Copy Transform Constraint */
typedef struct bTransLikeConstraint {
  struct Object *tar;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bTransLikeConstraint;

/* Floor Constraint */
typedef struct bMinMaxConstraint {
  struct Object *tar;
  int minmaxflag;
  float offset;
  int flag;
  /** For backward compatibility. */
  short sticky, stuck;
  char _pad[4];
  float cache[3];
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bMinMaxConstraint;

/* Action Constraint */
typedef struct bActionConstraint {
  struct Object *tar;
  /** What transform 'channel' drives the result. */
  short type;
  /** Was used in versions prior to the Constraints recode. */
  short local;
  int start;
  int end;
  float min;
  float max;
  int flag;
  struct bAction *act;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bActionConstraint;

/* Locked Axis Tracking constraint */
typedef struct bLockTrackConstraint {
  struct Object *tar;
  int trackflag;
  int lockflag;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bLockTrackConstraint;

/* Damped Tracking constraint */
typedef struct bDampTrackConstraint {
  struct Object *tar;
  int trackflag;
  char _pad[4];
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bDampTrackConstraint;

/* Follow Path constraints */
typedef struct bFollowPathConstraint {
  /** Must be path object. */
  struct Object *tar;

  /** Offset in time on the path (in frames), when NOT using 'fixed position'. */
  float offset;
  /** Parametric offset factor defining position along path, when using 'fixed position'. */
  float offset_fac;

  int followflag;

  short trackflag;
  short upflag;
} bFollowPathConstraint;

/* Stretch to constraint */
typedef struct bStretchToConstraint {
  struct Object *tar;
  int flag;
  int volmode;
  int plane;
  float orglength;
  float bulge;
  float bulge_min;
  float bulge_max;
  float bulge_smooth;
  /** MAX_ID_NAME-2. */
  char subtarget[64];
} bStretchToConstraint;

/* Rigid Body constraint */
typedef struct bRigidBodyJointConstraint {
  struct Object *tar;
  struct Object *child;
  int type;
  float pivX;
  float pivY;
  float pivZ;
  float axX;
  float axY;
  float axZ;
  float minLimit[6];
  float maxLimit[6];
  float extraFz;
  short flag;
  char _pad[6];
} bRigidBodyJointConstraint;

/* Clamp-To Constraint */
typedef struct bClampToConstraint {
  /** 'target' must be a curve. */
  struct Object *tar;
  /** Which axis/plane to compare owner's location on . */
  int flag;
  /** For legacy reasons, this is flag2. used for any extra settings. */
  int flag2;
} bClampToConstraint;

/* Child Of Constraint */
typedef struct bChildOfConstraint {
  /** Object which will act as parent (or target comes from). */
  struct Object *tar;
  /** Settings. */
  int flag;
  char _pad[4];
  /** Parent-inverse matrix to use. */
  float invmat[4][4];
  /** String to specify a subobject target, MAX_ID_NAME-2. */
  char subtarget[64];
} bChildOfConstraint;

/* Generic Transform->Transform Constraint */
typedef struct bTransformConstraint {
  /** Target (i.e. 'driver' object/bone). */
  struct Object *tar;
  /** MAX_ID_NAME-2. */
  char subtarget[64];

  /** Can be loc(0), rot(1) or size(2). */
  short from, to;
  /** Defines which target-axis deform is copied by each owner-axis. */
  char map[3];
  /** Extrapolate motion? if 0, confine to ranges. */
  char expo;

  /** From_min/max defines range of target transform. */
  float from_min[3];
  /** To map on to to_min/max range. */
  float from_max[3];
  /** Range of motion on owner caused by target . */
  float to_min[3];
  float to_max[3];

  /** From_min/max defines range of target transform. */
  float from_min_rot[3];
  /** To map on to to_min/max range. */
  float from_max_rot[3];
  /** Range of motion on owner caused by target . */
  float to_min_rot[3];
  float to_max_rot[3];

  /** From_min/max defines range of target transform. */
  float from_min_scale[3];
  /** To map on to to_min/max range. */
  float from_max_scale[3];
  /** Range of motion on owner caused by target . */
  float to_min_scale[3];
  float to_max_scale[3];
} bTransformConstraint;

/* Pivot Constraint */
typedef struct bPivotConstraint {
  /* Pivot Point:
   * Either target object + offset, or just offset is used
   */
  /** Target object (optional). */
  struct Object *tar;
  /** Subtarget name (optional), MAX_ID_NAME-2. */
  char subtarget[64];
  /** Offset from the target to use, regardless of whether it exists. */
  float offset[3];

  /* Rotation-driven activation:
   * This option provides easier one-stop setups for footrolls
   */
  /** Rotation axes to consider for this (#ePivotConstraint_Axis). */
  short rotAxis;

  /* General flags */
  /** #ePivotConstraint_Flag. */
  short flag;
} bPivotConstraint;

/* transform limiting constraints - zero target ----------------------------  */
/* Limit Location Constraint */
typedef struct bLocLimitConstraint {
  float xmin, xmax;
  float ymin, ymax;
  float zmin, zmax;
  short flag;
  short flag2;
} bLocLimitConstraint;

/* Limit Rotation Constraint */
typedef struct bRotLimitConstraint {
  float xmin, xmax;
  float ymin, ymax;
  float zmin, zmax;
  short flag;
  short flag2;
} bRotLimitConstraint;

/* Limit Scale Constraint */
typedef struct bSizeLimitConstraint {
  float xmin, xmax;
  float ymin, ymax;
  float zmin, zmax;
  short flag;
  short flag2;
} bSizeLimitConstraint;

/* Limit Distance Constraint */
typedef struct bDistLimitConstraint {
  struct Object *tar;
  /** MAX_ID_NAME-2. */
  char subtarget[64];

  /** Distance (radius of clamping sphere) from target. */
  float dist;
  /** Distance from clamping-sphere to start applying 'fade'. */
  float soft;

  /** Settings. */
  short flag;
  /** How to limit in relation to clamping sphere. */
  short mode;
  char _pad[4];
} bDistLimitConstraint;

/* ShrinkWrap Constraint */
typedef struct bShrinkwrapConstraint {
  struct Object *target;
  /** Distance to kept from target. */
  float dist;
  /** Shrink type (look on MOD shrinkwrap for values). */
  short shrinkType;
  /** Axis to project/constrain. */
  char projAxis;
  /** Space to project axis in. */
  char projAxisSpace;
  /** Distance to search. */
  float projLimit;
  /** Inside/outside/on surface (see MOD shrinkwrap). */
  char shrinkMode;
  /** Options. */
  char flag;
  /** Axis to align to normal. */
  char trackAxis;
  char _pad;
} bShrinkwrapConstraint;

/* Follow Track constraints */
typedef struct bFollowTrackConstraint {
  struct MovieClip *clip;
  /** MAX_NAME. */
  char track[64];
  int flag;
  int frame_method;
  /** MAX_NAME. */
  char object[64];
  struct Object *camera;
  struct Object *depth_ob;
} bFollowTrackConstraint;

/* Camera Solver constraints */
typedef struct bCameraSolverConstraint {
  struct MovieClip *clip;
  int flag;
  char _pad[4];
} bCameraSolverConstraint;

/* Camera Solver constraints */
typedef struct bObjectSolverConstraint {
  struct MovieClip *clip;
  int flag;
  char _pad[4];
  /** MAX_NAME. */
  char object[64];
  /** Parent-inverse matrix to use. */
  float invmat[4][4];
  struct Object *camera;
} bObjectSolverConstraint;

/* Transform matrix cache constraint */
typedef struct bTransformCacheConstraint {
  struct CacheFile *cache_file;
  /** FILE_MAX. */
  char object_path[1024];

  /* Runtime. */
  struct CacheReader *reader;
  char reader_object_path[1024];
} bTransformCacheConstraint;

/* ------------------------------------------ */

/* bConstraint->type
 * - Do not ever change the order of these, or else files could get
 *   broken as their correct value cannot be resolved
 */
typedef enum eBConstraint_Types {
  /** Invalid/legacy constraint */
  CONSTRAINT_TYPE_NULL = 0,
  /** Unimplemented non longer :) - during constraints recode, Aligorith */
  CONSTRAINT_TYPE_CHILDOF = 1,
  CONSTRAINT_TYPE_TRACKTO = 2,
  CONSTRAINT_TYPE_KINEMATIC = 3,
  CONSTRAINT_TYPE_FOLLOWPATH = 4,
  /** Unimplemented no longer :) - Aligorith */
  CONSTRAINT_TYPE_ROTLIMIT = 5,
  /** Unimplemented no longer :) - Aligorith */
  CONSTRAINT_TYPE_LOCLIMIT = 6,
  /** Unimplemented no longer :) - Aligorith */
  CONSTRAINT_TYPE_SIZELIMIT = 7,
  CONSTRAINT_TYPE_ROTLIKE = 8,
  CONSTRAINT_TYPE_LOCLIKE = 9,
  CONSTRAINT_TYPE_SIZELIKE = 10,
  /** Unimplemented no longer :) - Aligorith. Scripts */
  CONSTRAINT_TYPE_PYTHON = 11,
  CONSTRAINT_TYPE_ACTION = 12,
  /** New Tracking constraint that locks an axis in place - theeth */
  CONSTRAINT_TYPE_LOCKTRACK = 13,
  /** limit distance */
  CONSTRAINT_TYPE_DISTLIMIT = 14,
  /** claiming this to be mine :) is in tuhopuu bjornmose */
  CONSTRAINT_TYPE_STRETCHTO = 15,
  /** floor constraint */
  CONSTRAINT_TYPE_MINMAX = 16,
  /* CONSTRAINT_TYPE_DEPRECATED = 17 */
  /** clampto constraint */
  CONSTRAINT_TYPE_CLAMPTO = 18,
  /** transformation (loc/rot/size -> loc/rot/size) constraint */
  CONSTRAINT_TYPE_TRANSFORM = 19,
  /** shrinkwrap (loc/rot) constraint */
  CONSTRAINT_TYPE_SHRINKWRAP = 20,
  /** New Tracking constraint that minimizes twisting */
  CONSTRAINT_TYPE_DAMPTRACK = 21,
  /** Spline-IK - Align 'n' bones to a curve */
  CONSTRAINT_TYPE_SPLINEIK = 22,
  /** Copy transform matrix */
  CONSTRAINT_TYPE_TRANSLIKE = 23,
  /** Maintain volume during scaling */
  CONSTRAINT_TYPE_SAMEVOL = 24,
  /** Pivot Constraint */
  CONSTRAINT_TYPE_PIVOT = 25,
  /** Follow Track Constraint */
  CONSTRAINT_TYPE_FOLLOWTRACK = 26,
  /** Camera Solver Constraint */
  CONSTRAINT_TYPE_CAMERASOLVER = 27,
  /** Object Solver Constraint */
  CONSTRAINT_TYPE_OBJECTSOLVER = 28,
  /** Transform Cache Constraint */
  CONSTRAINT_TYPE_TRANSFORM_CACHE = 29,
  /** Armature Deform Constraint */
  CONSTRAINT_TYPE_ARMATURE = 30,

  /* NOTE: no constraints are allowed to be added after this */
  NUM_CONSTRAINT_TYPES,
} eBConstraint_Types;

/* bConstraint->flag */
/* flags 0x2 (1 << 1) and 0x8 (1 << 3) were used in past */
/* flag 0x20 (1 << 5) was used to indicate that a constraint was evaluated
 *                  using a 'local' hack for posebones only. */
typedef enum eBConstraint_Flags {
  /* expand for UI */
  CONSTRAINT_EXPAND = (1 << 0),
  /* pre-check for illegal object name or bone name */
  CONSTRAINT_DISABLE = (1 << 2),
  /* to indicate which Ipo should be shown, maybe for 3d access later too */
  CONSTRAINT_ACTIVE = (1 << 4),
  /* to indicate that the owner's space should only be changed into ownspace, but not out of it */
  CONSTRAINT_SPACEONCE = (1 << 6),
  /* influence ipo is on constraint itself, not in action channel */
  CONSTRAINT_OWN_IPO = (1 << 7),
  /* indicates that constraint was added locally (i.e.  didn't come from the proxy-lib) */
  CONSTRAINT_PROXY_LOCAL = (1 << 8),
  /* indicates that constraint is temporarily disabled (only used in GE) */
  CONSTRAINT_OFF = (1 << 9),
  /* use bbone curve shape when calculating headtail values (also used by dependency graph!) */
  CONSTRAINT_BBONE_SHAPE = (1 << 10),
  /* That constraint has been inserted in local override (i.e. it can be fully edited!). */
  CONSTRAINT_STATICOVERRIDE_LOCAL = (1 << 11),
  /* use full transformation (not just segment locations) - only set at runtime  */
  CONSTRAINT_BBONE_SHAPE_FULL = (1 << 12),
} eBConstraint_Flags;

/* bConstraint->ownspace/tarspace */
typedef enum eBConstraint_SpaceTypes {
  /** Default for all - worldspace. */
  CONSTRAINT_SPACE_WORLD = 0,
  /** For objects (relative to parent/without parent influence),
   * for bones (along normals of bone, without parent/restpositions). */
  CONSTRAINT_SPACE_LOCAL = 1,
  /** For posechannels - pose space. */
  CONSTRAINT_SPACE_POSE = 2,
  /** For posechannels - local with parent. */
  CONSTRAINT_SPACE_PARLOCAL = 3,
  /** For files from between 2.43-2.46 (should have been parlocal). */
  CONSTRAINT_SPACE_INVALID = 4, /* do not exchange for anything! */
} eBConstraint_SpaceTypes;

/* bConstraintChannel.flag */
// XXX deprecated... old AnimSys
typedef enum eConstraintChannel_Flags {
  CONSTRAINT_CHANNEL_SELECT = (1 << 0),
  CONSTRAINT_CHANNEL_PROTECTED = (1 << 1),
} eConstraintChannel_Flags;

/* -------------------------------------- */

/* bRotateLikeConstraint.flag */
typedef enum eCopyRotation_Flags {
  ROTLIKE_X = (1 << 0),
  ROTLIKE_Y = (1 << 1),
  ROTLIKE_Z = (1 << 2),
  ROTLIKE_X_INVERT = (1 << 4),
  ROTLIKE_Y_INVERT = (1 << 5),
  ROTLIKE_Z_INVERT = (1 << 6),
  ROTLIKE_OFFSET = (1 << 7),
} eCopyRotation_Flags;

/* bLocateLikeConstraint.flag */
typedef enum eCopyLocation_Flags {
  LOCLIKE_X = (1 << 0),
  LOCLIKE_Y = (1 << 1),
  LOCLIKE_Z = (1 << 2),
  /** LOCLIKE_TIP is a deprecated option... use headtail=1.0f instead */
  LOCLIKE_TIP = (1 << 3),
  LOCLIKE_X_INVERT = (1 << 4),
  LOCLIKE_Y_INVERT = (1 << 5),
  LOCLIKE_Z_INVERT = (1 << 6),
  LOCLIKE_OFFSET = (1 << 7),
} eCopyLocation_Flags;

/* bSizeLikeConstraint.flag */
typedef enum eCopyScale_Flags {
  SIZELIKE_X = (1 << 0),
  SIZELIKE_Y = (1 << 1),
  SIZELIKE_Z = (1 << 2),
  SIZELIKE_OFFSET = (1 << 3),
  SIZELIKE_MULTIPLY = (1 << 4),
} eCopyScale_Flags;

/* bTransformConstraint.to/from */
typedef enum eTransform_ToFrom {
  TRANS_LOCATION = 0,
  TRANS_ROTATION = 1,
  TRANS_SCALE = 2,
} eTransform_ToFrom;

/* bSameVolumeConstraint.flag */
typedef enum eSameVolume_Modes {
  SAMEVOL_X = 0,
  SAMEVOL_Y = 1,
  SAMEVOL_Z = 2,
} eSameVolume_Modes;

/* bActionConstraint.flag */
typedef enum eActionConstraint_Flags {
  /* Bones use "object" part of target action, instead of "same bone name" part */
  ACTCON_BONE_USE_OBJECT_ACTION = (1 << 0),
} eActionConstraint_Flags;

/* Locked-Axis Values (Locked Track) */
typedef enum eLockAxis_Modes {
  LOCK_X = 0,
  LOCK_Y = 1,
  LOCK_Z = 2,
} eLockAxis_Modes;

/* Up-Axis Values (TrackTo and Locked Track) */
typedef enum eUpAxis_Modes {
  UP_X = 0,
  UP_Y = 1,
  UP_Z = 2,
} eUpAxis_Modes;

/* Tracking axis (TrackTo, Locked Track, Damped Track) and minmax (floor) constraint */
typedef enum eTrackToAxis_Modes {
  TRACK_X = 0,
  TRACK_Y = 1,
  TRACK_Z = 2,
  TRACK_nX = 3,
  TRACK_nY = 4,
  TRACK_nZ = 5,
} eTrackToAxis_Modes;

/* Shrinkwrap flags */
typedef enum eShrinkwrap_Flags {
  /* Also raycast in the opposite direction. */
  CON_SHRINKWRAP_PROJECT_OPPOSITE = (1 << 0),
  /* Invert the cull mode when projecting opposite. */
  CON_SHRINKWRAP_PROJECT_INVERT_CULL = (1 << 1),
  /* Align the specified axis to the target normal. */
  CON_SHRINKWRAP_TRACK_NORMAL = (1 << 2),

  /* Ignore front faces in project; same value as MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE */
  CON_SHRINKWRAP_PROJECT_CULL_FRONTFACE = (1 << 3),
  /* Ignore back faces in project; same value as MOD_SHRINKWRAP_CULL_TARGET_BACKFACE */
  CON_SHRINKWRAP_PROJECT_CULL_BACKFACE = (1 << 4),
} eShrinkwrap_Flags;

#define CON_SHRINKWRAP_PROJECT_CULL_MASK \
  (CON_SHRINKWRAP_PROJECT_CULL_FRONTFACE | CON_SHRINKWRAP_PROJECT_CULL_BACKFACE)

/* FollowPath flags */
typedef enum eFollowPath_Flags {
  FOLLOWPATH_FOLLOW = (1 << 0),
  FOLLOWPATH_STATIC = (1 << 1),
  FOLLOWPATH_RADIUS = (1 << 2),
} eFollowPath_Flags;

/* bTrackToConstraint->flags */
typedef enum eTrackTo_Flags {
  TARGET_Z_UP = (1 << 0),
} eTrackTo_Flags;

/* Stretch To Constraint -> volmode */
typedef enum eStretchTo_VolMode {
  VOLUME_XZ = 0,
  VOLUME_X = 1,
  VOLUME_Z = 2,
  NO_VOLUME = 3,
} eStretchTo_VolMode;

/* Stretch To Constraint -> plane mode */
typedef enum eStretchTo_PlaneMode {
  PLANE_X = 0,
  PLANE_Y = 1,
  PLANE_Z = 2,
} eStretchTo_PlaneMode;

/* Clamp-To Constraint ->flag */
typedef enum eClampTo_Modes {
  CLAMPTO_AUTO = 0,
  CLAMPTO_X = 1,
  CLAMPTO_Y = 2,
  CLAMPTO_Z = 3,
} eClampTo_Modes;

/* ClampTo Constraint ->flag2 */
typedef enum eClampTo_Flags {
  CLAMPTO_CYCLIC = (1 << 0),
} eClampTo_Flags;

/* bKinematicConstraint->flag */
typedef enum eKinematic_Flags {
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
} eKinematic_Flags;

/* bSplineIKConstraint->flag */
typedef enum eSplineIK_Flags {
  /* chain has been attached to spline */
  CONSTRAINT_SPLINEIK_BOUND = (1 << 0),
  /* root of chain is not influenced by the constraint */
  CONSTRAINT_SPLINEIK_NO_ROOT = (1 << 1),
#ifdef DNA_DEPRECATED
  /* bones in the chain should not scale to fit the curve */
  CONSTRAINT_SPLINEIK_SCALE_LIMITED = (1 << 2),
#endif
  /* evenly distribute the bones along the path regardless of length */
  CONSTRAINT_SPLINEIK_EVENSPLITS = (1 << 3),
  /* don't adjust the x and z scaling of the bones by the curve radius */
  CONSTRAINT_SPLINEIK_NO_CURVERAD = (1 << 4),

  /* for "volumetric" xz scale mode, limit the minimum or maximum scale values */
  CONSTRAINT_SPLINEIK_USE_BULGE_MIN = (1 << 5),
  CONSTRAINT_SPLINEIK_USE_BULGE_MAX = (1 << 6),
} eSplineIK_Flags;

/* bSplineIKConstraint->xzScaleMode */
typedef enum eSplineIK_XZScaleModes {
  /* no x/z scaling */
  CONSTRAINT_SPLINEIK_XZS_NONE = 0,
  /* bones in the chain should take their x/z scales from the original scaling */
  CONSTRAINT_SPLINEIK_XZS_ORIGINAL = 1,
  /* x/z scales are the inverse of the y-scale */
  CONSTRAINT_SPLINEIK_XZS_INVERSE = 2,
  /* x/z scales are computed using a volume preserving technique (from Stretch To constraint) */
  CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC = 3,
} eSplineIK_XZScaleModes;

/* bSplineIKConstraint->yScaleMode */
typedef enum eSplineIK_YScaleModes {
  /* no y scaling */
  CONSTRAINT_SPLINEIK_YS_NONE = 0,
  /* bones in the chain should be scaled to fit the length of the curve */
  CONSTRAINT_SPLINEIK_YS_FIT_CURVE = 1,
  /* bones in the chain should take their y scales from the original scaling */
  CONSTRAINT_SPLINEIK_YS_ORIGINAL = 2,
} eSplineIK_YScaleModes;

/* bArmatureConstraint -> flag */
typedef enum eArmature_Flags {
  /** use dual quaternion blending */
  CONSTRAINT_ARMATURE_QUATERNION = (1 << 0),
  /** use envelopes */
  CONSTRAINT_ARMATURE_ENVELOPE = (1 << 1),
  /** use current bone location */
  CONSTRAINT_ARMATURE_CUR_LOCATION = (1 << 2),
} eArmature_Flags;

/* MinMax (floor) flags */
typedef enum eFloor_Flags {
  MINMAX_STICKY = (1 << 0),
  MINMAX_STUCK = (1 << 1),
  MINMAX_USEROT = (1 << 2),
} eFloor_Flags;

/* transform limiting constraints -> flag2 */
typedef enum eTransformLimits_Flags2 {
  /* not used anymore - for older Limit Location constraints only */
  LIMIT_NOPARENT = (1 << 0),
  /* for all Limit constraints - allow to be used during transform? */
  LIMIT_TRANSFORM = (1 << 1),
} eTransformLimits_Flags2;

/* transform limiting constraints -> flag (own flags)  */
typedef enum eTransformLimits_Flags {
  LIMIT_XMIN = (1 << 0),
  LIMIT_XMAX = (1 << 1),
  LIMIT_YMIN = (1 << 2),
  LIMIT_YMAX = (1 << 3),
  LIMIT_ZMIN = (1 << 4),
  LIMIT_ZMAX = (1 << 5),
} eTransformLimits_Flags;

/* limit rotation constraint -> flag (own flags) */
typedef enum eRotLimit_Flags {
  LIMIT_XROT = (1 << 0),
  LIMIT_YROT = (1 << 1),
  LIMIT_ZROT = (1 << 2),
} eRotLimit_Flags;

/* distance limit constraint */
/* bDistLimitConstraint->flag */
typedef enum eDistLimit_Flag {
  /* "soft" cushion effect when reaching the limit sphere */  // NOT IMPLEMENTED!
  LIMITDIST_USESOFT = (1 << 0),
  /* as for all Limit constraints - allow to be used during transform? */
  LIMITDIST_TRANSFORM = (1 << 1),
} eDistLimit_Flag;

/* bDistLimitConstraint->mode */
typedef enum eDistLimit_Modes {
  LIMITDIST_INSIDE = 0,
  LIMITDIST_OUTSIDE = 1,
  LIMITDIST_ONSURFACE = 2,
} eDistLimit_Modes;

/* python constraint -> flag */
typedef enum ePyConstraint_Flags {
  PYCON_USETARGETS = (1 << 0),
  PYCON_SCRIPTERROR = (1 << 1),
} ePyConstraint_Flags;

/* ChildOf Constraint -> flag */
typedef enum eChildOf_Flags {
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
} eChildOf_Flags;

/* Pivot Constraint */
/* Restrictions for Pivot Constraint axis to consider for enabling constraint */
typedef enum ePivotConstraint_Axis {
  /* do not consider this activity-clamping */
  PIVOTCON_AXIS_NONE = -1,

  /* consider -ve x-axis rotations */
  PIVOTCON_AXIS_X_NEG = 0,
  /* consider -ve y-axis rotations */
  PIVOTCON_AXIS_Y_NEG = 1,
  /* consider -ve z-axis rotations */
  PIVOTCON_AXIS_Z_NEG = 2,

  /* consider +ve x-axis rotations */
  PIVOTCON_AXIS_X = 3,
  /* consider +ve y-axis rotations */
  PIVOTCON_AXIS_Y = 4,
  /* consider +ve z-axis rotations */
  PIVOTCON_AXIS_Z = 5,
} ePivotConstraint_Axis;

/* settings for Pivot Constraint in general */
typedef enum ePivotConstraint_Flag {
  /* offset is to be interpreted as being a fixed-point in space */
  PIVOTCON_FLAG_OFFSET_ABS = (1 << 0),
  /* rotation-based activation uses negative rotation to drive result */
  PIVOTCON_FLAG_ROTACT_NEG = (1 << 1),
} ePivotConstraint_Flag;

typedef enum eFollowTrack_Flags {
  FOLLOWTRACK_ACTIVECLIP = (1 << 0),
  FOLLOWTRACK_USE_3D_POSITION = (1 << 1),
  FOLLOWTRACK_USE_UNDISTORTION = (1 << 2),
} eFollowTrack_Flags;

typedef enum eFollowTrack_FrameMethod {
  FOLLOWTRACK_FRAME_STRETCH = 0,
  FOLLOWTRACK_FRAME_FIT = 1,
  FOLLOWTRACK_FRAME_CROP = 2,
} eFollowTrack_FrameMethod;

/* CameraSolver Constraint -> flag */
typedef enum eCameraSolver_Flags {
  CAMERASOLVER_ACTIVECLIP = (1 << 0),
} eCameraSolver_Flags;

/* ObjectSolver Constraint -> flag */
typedef enum eObjectSolver_Flags {
  OBJECTSOLVER_ACTIVECLIP = (1 << 0),
} eObjectSolver_Flags;

/* ObjectSolver Constraint -> flag */
typedef enum eStretchTo_Flags {
  STRETCHTOCON_USE_BULGE_MIN = (1 << 0),
  STRETCHTOCON_USE_BULGE_MAX = (1 << 1),
} eStretchTo_Flags;

/* important: these defines need to match up with PHY_DynamicTypes headerfile */
#define CONSTRAINT_RB_BALL 1
#define CONSTRAINT_RB_HINGE 2
#define CONSTRAINT_RB_CONETWIST 4
#define CONSTRAINT_RB_VEHICLE 11
#define CONSTRAINT_RB_GENERIC6DOF 12

#endif
