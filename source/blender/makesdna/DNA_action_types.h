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
 */

/** \file
 * \ingroup DNA
 *
 * Define actions data-block for the animation system.
 * A collection of animation curves and drivers to be assigned to data-blocks
 * or sequenced in the non-linear-editor (NLA).
 */

#ifndef __DNA_ACTION_TYPES_H__
#define __DNA_ACTION_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_view2d_types.h"
#include "DNA_vec_types.h"
#include "DNA_userdef_types.h" /* ThemeWireColor */

struct Collection;
struct GHash;
struct Object;
struct SpaceLink;

/* ************************************************ */
/* Visualization */

/* Motion Paths ------------------------------------ */
/* (used for Pose Channels and Objects) */

/* Data point for motion path (mpv) */
typedef struct bMotionPathVert {
  /** Coordinates of point in 3D-space. */
  float co[3];
  /** Quick settings. */
  int flag;
} bMotionPathVert;

/* bMotionPathVert->flag */
typedef enum eMotionPathVert_Flag {
  /* vert is selected */
  MOTIONPATH_VERT_SEL = (1 << 0),
  MOTIONPATH_VERT_KEY = (1 << 1),
} eMotionPathVert_Flag;

/* ........ */

/* Motion Path data cache (mpath)
 * - for elements providing transforms (i.e. Objects or PoseChannels)
 */
typedef struct bMotionPath {
  /** Path samples. */
  bMotionPathVert *points;
  /** The number of cached verts. */
  int length;

  /** For drawing paths, the start frame number. */
  int start_frame;
  /** For drawing paths, the end frame number. */
  int end_frame;

  /** Optional custom color. */
  float color[3];
  /** Line thickness. */
  int line_thickness;
  /** Baking settings - eMotionPath_Flag. */
  int flag;

  /* Used for drawing. */
  struct GPUVertBuf *points_vbo;
  struct GPUBatch *batch_line;
  struct GPUBatch *batch_points;
  void *_pad;
} bMotionPath;

/* bMotionPath->flag */
typedef enum eMotionPath_Flag {
  /* (for bones) path represents the head of the bone */
  MOTIONPATH_FLAG_BHEAD = (1 << 0),
  /* motion path is being edited */
  MOTIONPATH_FLAG_EDIT = (1 << 1),
  /* Custom colors */
  MOTIONPATH_FLAG_CUSTOM = (1 << 2),
  /* Draw lines or only points */
  MOTIONPATH_FLAG_LINES = (1 << 3),
} eMotionPath_Flag;

/* Visualization General --------------------------- */
/* for Objects or Poses (but NOT PoseChannels) */

/* Animation Visualization Settings (avs) */
typedef struct bAnimVizSettings {
  /* General Settings ------------------------ */
  /** #eAnimViz_RecalcFlags. */
  short recalc;

  /* Motion Path Settings ------------------- */
  /** #eMotionPath_Types. */
  short path_type;
  /** Number of frames between points indicated on the paths. */
  short path_step;

  /** #eMotionPaths_ViewFlag. */
  short path_viewflag;
  /** #eMotionPaths_BakeFlag. */
  short path_bakeflag;
  char _pad[6];

  /** Start and end frames of path-calculation range. */
  int path_sf, path_ef;
  /** Number of frames before/after current frame to show. */
  int path_bc, path_ac;
} bAnimVizSettings;

/* bAnimVizSettings->recalc */
typedef enum eAnimViz_RecalcFlags {
  /* motionpaths need recalculating */
  ANIMVIZ_RECALC_PATHS = (1 << 0),
} eAnimViz_RecalcFlags;

/* bAnimVizSettings->path_type */
typedef enum eMotionPaths_Types {
  /* show the paths along their entire ranges */
  MOTIONPATH_TYPE_RANGE = 0,
  /* only show the parts of the paths around the current frame */
  MOTIONPATH_TYPE_ACFRA = 1,
} eMotionPath_Types;

/* bAnimVizSettings->path_viewflag */
typedef enum eMotionPaths_ViewFlag {
  /* show frames on path */
  MOTIONPATH_VIEW_FNUMS = (1 << 0),
  /* show keyframes on path */
  MOTIONPATH_VIEW_KFRAS = (1 << 1),
  /* show keyframe/frame numbers */
  MOTIONPATH_VIEW_KFNOS = (1 << 2),
  /* find keyframes in whole action (instead of just in matching group name) */
  MOTIONPATH_VIEW_KFACT = (1 << 3),
  /* draw lines on path */
  MOTIONPATH_VIEW_LINES = (1 << 4),
} eMotionPath_ViewFlag;

/* bAnimVizSettings->path_bakeflag */
typedef enum eMotionPaths_BakeFlag {
  /** motion paths directly associated with this block of settings needs updating */
  MOTIONPATH_BAKE_NEEDS_RECALC = (1 << 0),
  /** for bones - calculate head-points for curves instead of tips */
  MOTIONPATH_BAKE_HEADS = (1 << 1),
  /** motion paths exist for AnimVizSettings instance - set when calc for first time,
   * and unset when clearing */
  MOTIONPATH_BAKE_HAS_PATHS = (1 << 2),
} eMotionPath_BakeFlag;

/* runtime */
#
#
typedef struct bPoseChannelDrawData {
  float solid_color[4];
  float wire_color[4];

  int bbone_matrix_len;
  /* keep last */
  float bbone_matrix[0][4][4];
} bPoseChannelDrawData;

struct DualQuat;
struct Mat4;

typedef struct bPoseChannel_Runtime {
  /* Cached dual quaternion for deformation. */
  struct DualQuat deform_dual_quat;

  /* B-Bone shape data: copy of the segment count for validation. */
  int bbone_segments;

  /* Rest and posed matrices for segments. */
  struct Mat4 *bbone_rest_mats;
  struct Mat4 *bbone_pose_mats;

  /* Delta from rest to pose in matrix and DualQuat form. */
  struct Mat4 *bbone_deform_mats;
  struct DualQuat *bbone_dual_quats;
} bPoseChannel_Runtime;

/* ************************************************ */
/* Poses */

/* PoseChannel ------------------------------------ */

/* PoseChannel
 *
 * A PoseChannel stores the results of Actions and transform information
 * with respect to the restposition of Armature bones
 */
typedef struct bPoseChannel {
  struct bPoseChannel *next, *prev;

  /** User-Defined Properties on this PoseChannel. */
  IDProperty *prop;

  /** Constraints that act on this PoseChannel. */
  ListBase constraints;
  /** Need to match bone name length: MAXBONENAME. */
  char name[64];

  /** Dynamic, for detecting transform changes. */
  short flag;
  /** Settings for IK bones. */
  short ikflag;
  /** Protect channels from being transformed. */
  short protectflag;
  /** Index of action-group this bone belongs to (0 = default/no group). */
  short agrp_index;
  /** For quick detecting which constraints affect this channel. */
  char constflag;
  /** Copy of bone flag, so you can work with library armatures, not for runtime use. */
  char selectflag;
  char drawflag;
  char bboneflag DNA_DEPRECATED;
  char _pad0[4];

  /** Set on read file or rebuild pose. */
  struct Bone *bone;
  /** Set on read file or rebuild pose. */
  struct bPoseChannel *parent;
  /** Set on read file or rebuild pose, the 'ik' child, for b-bones. */
  struct bPoseChannel *child;

  /** "IK trees" - only while evaluating pose. */
  struct ListBase iktree;
  /** Spline-IK "trees" - only while evaluating pose. */
  struct ListBase siktree;

  /** Motion path cache for this bone. */
  bMotionPath *mpath;
  /** Draws custom object instead of default bone shape. */
  struct Object *custom;
  /**
   * Odd feature, display with another bones transform.
   * needed in rare cases for advanced rigs,
   * since the alternative is highly complicated - campbell
   */
  struct bPoseChannel *custom_tx;
  float custom_scale;

  char _pad1[4];

  /** Transforms - written in by actions or transform. */
  float loc[3];
  float size[3];

  /**
   * Rotations - written in by actions or transform
   * (but only one representation gets used at any time)
   */
  /** Euler rotation. */
  float eul[3];
  /** Quaternion rotation. */
  float quat[4];
  /** Axis-angle rotation. */
  float rotAxis[3], rotAngle;
  /** #eRotationModes - rotation representation to use. */
  short rotmode;
  char _pad[2];

  /** Matrix result of loc/quat/size, and where we put deform in, see next line */
  float chan_mat[4][4];
  /**
   * Constraints accumulate here. in the end, pose_mat = bone->arm_mat * chan_mat
   * this matrix is object space.
   */
  float pose_mat[4][4];
  /** For display, pose_mat with bone length applied. */
  float disp_mat[4][4];
  /** For display, pose_mat with bone length applied and translated to tai.l*/
  float disp_tail_mat[4][4];
  /**
   * Inverse result of constraints.
   * doesn't include effect of restposition, parent, and local transform.
   */
  float constinv[4][4];

  /** Actually pose_mat[3]. */
  float pose_head[3];
  /** Also used for drawing help lines. */
  float pose_tail[3];

  /** DOF constraint, note! - these are stored in degrees, not radians. */
  float limitmin[3], limitmax[3];
  /** DOF stiffness. */
  float stiffness[3];
  float ikstretch;
  /** Weight of joint rotation constraint. */
  float ikrotweight;
  /** Weight of joint stretch constraint. */
  float iklinweight;

  /**
   * Curved bones settings - these are for animating,
   * and are applied on top of the copies in pchan->bone
   */
  float roll1, roll2;
  float curve_in_x, curve_in_y;
  float curve_out_x, curve_out_y;
  float ease1, ease2;
  float scale_in_x, scale_in_y;
  float scale_out_x, scale_out_y;

  /** B-Bone custom handles; set on read file or rebuild pose based on pchan->bone data. */
  struct bPoseChannel *bbone_prev;
  struct bPoseChannel *bbone_next;

  /** Use for outliner. */
  void *temp;
  /** Runtime data for color and bbone segment matrix. */
  bPoseChannelDrawData *draw_data;

  /** Points to an original pose channel. */
  struct bPoseChannel *orig_pchan;

  /** Runtime data (keep last). */
  struct bPoseChannel_Runtime runtime;
} bPoseChannel;

/* PoseChannel (transform) flags */
typedef enum ePchan_Flag {
  /* has transforms */
  POSE_LOC = (1 << 0),
  POSE_ROT = (1 << 1),
  POSE_SIZE = (1 << 2),

  /* old IK/cache stuff
   * - used to be here from (1 << 3) to (1 << 8)
   *   but has been repurposed since 2.77.2
   *   as they haven't been used in over 10 years
   */

  /* has BBone deforms */
  POSE_BBONE_SHAPE = (1 << 3),

  /* IK/Pose solving */
  POSE_CHAIN = (1 << 9),
  POSE_DONE = (1 << 10),
  /* visualization */
  POSE_KEY = (1 << 11),
  POSE_STRIDE = (1 << 12),
  /* standard IK solving */
  POSE_IKTREE = (1 << 13),
#if 0
  /* has Spline IK */
  POSE_HAS_IKS = (1 << 14),
#endif
  /* spline IK solving */
  POSE_IKSPLINE = (1 << 15),
} ePchan_Flag;

/* PoseChannel constflag (constraint detection) */
typedef enum ePchan_ConstFlag {
  PCHAN_HAS_IK = (1 << 0),
  PCHAN_HAS_CONST = (1 << 1),
  /* only used for drawing Posemode, not stored in channel */
  PCHAN_HAS_ACTION = (1 << 2),
  PCHAN_HAS_TARGET = (1 << 3),
  /* only for drawing Posemode too */
  PCHAN_HAS_STRIDE = (1 << 4),
  /* spline IK */
  PCHAN_HAS_SPLINEIK = (1 << 5),
} ePchan_ConstFlag;

/* PoseChannel->ikflag */
typedef enum ePchan_IkFlag {
  BONE_IK_NO_XDOF = (1 << 0),
  BONE_IK_NO_YDOF = (1 << 1),
  BONE_IK_NO_ZDOF = (1 << 2),

  BONE_IK_XLIMIT = (1 << 3),
  BONE_IK_YLIMIT = (1 << 4),
  BONE_IK_ZLIMIT = (1 << 5),

  BONE_IK_ROTCTL = (1 << 6),
  BONE_IK_LINCTL = (1 << 7),

  BONE_IK_NO_XDOF_TEMP = (1 << 10),
  BONE_IK_NO_YDOF_TEMP = (1 << 11),
  BONE_IK_NO_ZDOF_TEMP = (1 << 12),
} ePchan_IkFlag;

/* PoseChannel->drawflag */
typedef enum ePchan_DrawFlag {
  PCHAN_DRAW_NO_CUSTOM_BONE_SIZE = (1 << 0),
} ePchan_DrawFlag;

#define PCHAN_CUSTOM_DRAW_SIZE(pchan) \
  (pchan)->custom_scale *( \
      ((pchan)->drawflag & PCHAN_DRAW_NO_CUSTOM_BONE_SIZE) ? 1.0f : (pchan)->bone->length)

#ifdef DNA_DEPRECATED_ALLOW
/* PoseChannel->bboneflag */
typedef enum ePchan_BBoneFlag {
  /* Use custom reference bones (for roll and handle alignment), instead of immediate neighbors */
  PCHAN_BBONE_CUSTOM_HANDLES = (1 << 1),
  /* Evaluate start handle as being "relative" */
  PCHAN_BBONE_CUSTOM_START_REL = (1 << 2),
  /* Evaluate end handle as being "relative" */
  PCHAN_BBONE_CUSTOM_END_REL = (1 << 3),
} ePchan_BBoneFlag;
#endif

/* PoseChannel->rotmode and Object->rotmode */
typedef enum eRotationModes {
  /* quaternion rotations (default, and for older Blender versions) */
  ROT_MODE_QUAT = 0,
  /* euler rotations - keep in sync with enum in BLI_math.h */
  /** Blender 'default' (classic) - must be as 1 to sync with BLI_math_rotation.h defines */
  ROT_MODE_EUL = 1,
  ROT_MODE_XYZ = 1,
  ROT_MODE_XZY = 2,
  ROT_MODE_YXZ = 3,
  ROT_MODE_YZX = 4,
  ROT_MODE_ZXY = 5,
  ROT_MODE_ZYX = 6,
  /* NOTE: space is reserved here for 18 other possible
   * euler rotation orders not implemented
   */
  /* axis angle rotations */
  ROT_MODE_AXISANGLE = -1,

  ROT_MODE_MIN = ROT_MODE_AXISANGLE, /* sentinel for Py API */
  ROT_MODE_MAX = ROT_MODE_ZYX,
} eRotationModes;

/* Pose ------------------------------------ */

/* Pose-Object.
 *
 * It is only found under ob->pose. It is not library data, even
 * though there is a define for it (hack for the outliner).
 */
typedef struct bPose {
  /** List of pose channels, PoseBones in RNA. */
  ListBase chanbase;
  /** Ghash for quicker string lookups. */
  struct GHash *chanhash;

  /* Flat array of pose channels. It references pointers from
   * chanbase. Used for quick pose channel lookup from an index.
   */
  bPoseChannel **chan_array;

  short flag;
  char _pad[2];
  /** Proxy layer: copy from armature, gets synced. */
  unsigned int proxy_layer;
  char _pad1[4];

  /** Local action time of this pose. */
  float ctime;
  /** Applied to object. */
  float stride_offset[3];
  /** Result of match and cycles, applied in BKE_pose_where_is(). */
  float cyclic_offset[3];

  /** List of bActionGroups. */
  ListBase agroups;

  /** Index of active group (starts from 1). */
  int active_group;
  /** Ik solver to use, see ePose_IKSolverType. */
  int iksolver;
  /** Temporary IK data, depends on the IK solver. Not saved in file. */
  void *ikdata;
  /** IK solver parameters, structure depends on iksolver. */
  void *ikparam;

  /** Settings for visualization of bone animation. */
  bAnimVizSettings avs;
  /** Proxy active bone name, MAXBONENAME. */
  char proxy_act_bone[64];
} bPose;

/* Pose->flag */
typedef enum ePose_Flags {
  /* results in BKE_pose_rebuild being called */
  POSE_RECALC = (1 << 0),
  /* prevents any channel from getting overridden by anim from IPO */
  POSE_LOCKED = (1 << 1),
  /* clears the POSE_LOCKED flag for the next time the pose is evaluated */
  POSE_DO_UNLOCK = (1 << 2),
  /* pose has constraints which depend on time (used when depsgraph updates for a new frame) */
  POSE_CONSTRAINTS_TIMEDEPEND = (1 << 3),
  /* recalculate bone paths */
  POSE_RECALCPATHS = (1 << 4),
  /* set by BKE_pose_rebuild to give a chance to the IK solver to rebuild IK tree */
  POSE_WAS_REBUILT = (1 << 5),
  POSE_FLAG_DEPRECATED = (1 << 6), /* deprecated. */
  /* pose constraint flags needs to be updated */
  POSE_CONSTRAINTS_NEED_UPDATE_FLAGS = (1 << 7),
  /* Use auto IK in pose mode */
  POSE_AUTO_IK = (1 << 8),
  /* Use x-axis mirror in pose mode */
  POSE_MIRROR_EDIT = (1 << 9),
  /* Use relative mirroring in mirror mode */
  POSE_MIRROR_RELATIVE = (1 << 10),
} ePose_Flags;

/* IK Solvers ------------------------------------ */

/* bPose->iksolver and bPose->ikparam->iksolver */
typedef enum ePose_IKSolverType {
  IKSOLVER_STANDARD = 0,
  IKSOLVER_ITASC = 1,
} ePose_IKSolverType;

/* header for all bPose->ikparam structures */
typedef struct bIKParam {
  int iksolver;
} bIKParam;

/* bPose->ikparam when bPose->iksolver=1 */
typedef struct bItasc {
  int iksolver;
  float precision;
  short numiter;
  short numstep;
  float minstep;
  float maxstep;
  short solver;
  short flag;
  float feedback;
  /** Max velocity to SDLS solver. */
  float maxvel;
  /** Maximum damping for DLS solver. */
  float dampmax;
  /** Threshold of singular value from which the damping start progressively. */
  float dampeps;
} bItasc;

/* bItasc->flag */
typedef enum eItasc_Flags {
  ITASC_AUTO_STEP = (1 << 0),
  ITASC_INITIAL_REITERATION = (1 << 1),
  ITASC_REITERATION = (1 << 2),
  ITASC_SIMULATION = (1 << 3),
} eItasc_Flags;

/* bItasc->solver */
typedef enum eItasc_Solver {
  ITASC_SOLVER_SDLS = 0, /* selective damped least square, suitable for CopyPose constraint */
  ITASC_SOLVER_DLS = 1,  /* damped least square with numerical filtering of damping */
} eItasc_Solver;

/* ************************************************ */
/* Action */

/* Groups -------------------------------------- */

/* Action-Channel Group (agrp)
 *
 * These are stored as a list per-Action, and are only used to
 * group that Action's channels in an Animation Editor.
 *
 * Even though all FCurves live in a big list per Action, each group they are in also
 * holds references to the achans within that list which belong to it. Care must be taken to
 * ensure that action-groups never end up being the sole 'owner' of a channel.
 *
 * This is also exploited for bone-groups. Bone-Groups are stored per bPose, and are used
 * primarily to color bones in the 3d-view. There are other benefits too, but those are mostly
 * related to Action-Groups.
 *
 * Note that these two uses each have their own RNA 'ActionGroup' and 'BoneGroup'.
 */
typedef struct bActionGroup {
  struct bActionGroup *next, *prev;

  /**
   * Note: this must not be touched by standard listbase functions
   * which would clear links to other channels.
   */
  ListBase channels;

  /** Settings for this action-group. */
  int flag;
  /**
   * Index of custom color set to use when used for bones
   * (0=default - used for all old files, -1=custom set).
   */
  int customCol;
  /** Name of the group. */
  char name[64];

  /** Color set to use when customCol == -1. */
  ThemeWireColor cs;
} bActionGroup;

/* Action Group flags */
typedef enum eActionGroup_Flag {
  /* group is selected */
  AGRP_SELECTED = (1 << 0),
  /* group is 'active' / last selected one */
  AGRP_ACTIVE = (1 << 1),
  /* keyframes/channels belonging to it cannot be edited */
  AGRP_PROTECTED = (1 << 2),
  /* for UI (DopeSheet), sub-channels are shown */
  AGRP_EXPANDED = (1 << 3),
  /* sub-channels are not evaluated */
  AGRP_MUTED = (1 << 4),
  /* sub-channels are not visible in Graph Editor */
  AGRP_NOTVISIBLE = (1 << 5),
  /* for UI (Graph Editor), sub-channels are shown */
  AGRP_EXPANDED_G = (1 << 6),

  /* sub channel modifiers off */
  AGRP_MODIFIERS_OFF = (1 << 7),

  AGRP_TEMP = (1 << 30),
  AGRP_MOVED = (1u << 31),
} eActionGroup_Flag;

/* Actions -------------------------------------- */

/* Action - reusable F-Curve 'bag'  (act)
 *
 * This contains F-Curves that may affect settings from more than one ID blocktype and/or datablock
 * (i.e. sub-data linked/used directly to the ID block that the animation data is linked to),
 * but with the restriction that the other unrelated data (i.e. data that is not directly used or
 * linked to by the source ID block).
 *
 * It serves as a 'unit' of reusable animation information (i.e. keyframes/motion data),
 * that affects a group of related settings (as defined by the user).
 */
typedef struct bAction {
  /** ID-serialisation for relinking. */
  ID id;

  /** Function-curves (FCurve). */
  ListBase curves;
  /** Legacy data - Action Channels (bActionChannel) in pre-2.5 animation system. */
  ListBase chanbase DNA_DEPRECATED;
  /** Groups of function-curves (bActionGroup). */
  ListBase groups;
  /** Markers local to the Action (used to provide Pose-Libraries). */
  ListBase markers;

  /** Settings for this action. */
  int flag;
  /** Index of the active marker. */
  int active_marker;

  /**
   * Type of ID-blocks that action can be assigned to
   * (if 0, will be set to whatever ID first evaluates it).
   */
  int idroot;
  char _pad[4];
} bAction;

/* Flags for the action */
typedef enum eAction_Flags {
  /* flags for displaying in UI */
  ACT_COLLAPSED = (1 << 0),
  ACT_SELECTED = (1 << 1),

  /* flags for evaluation/editing */
  ACT_MUTED = (1 << 9),
  ACT_PROTECTED = (1 << 10),
  ACT_DISABLED = (1 << 11),
} eAction_Flags;

/* ************************************************ */
/* Action/Dopesheet Editor */

/* Storage for Dopesheet/Grease-Pencil Editor data */
typedef struct bDopeSheet {
  /** Currently ID_SCE (for Dopesheet), and ID_SC (for Grease Pencil). */
  ID *source;
  /** Cache for channels (only initialized when pinned). */  // XXX not used!
  ListBase chanbase;

  /** Object group for option to only include objects that belong to this Collection. */
  struct Collection *filter_grp;
  /** String to search for in displayed names of F-Curves, or NlaTracks/GP Layers/etc. */
  char searchstr[64];

  /** Flags to use for filtering data. */
  int filterflag;
  int filterflag2;
  /** Standard flags. */
  int flag;

  /** `index + 1` of channel to rename - only gets set by renaming operator. */
  int renameIndex;
} bDopeSheet;

/* DopeSheet filter-flag */
typedef enum eDopeSheet_FilterFlag {
  /* general filtering */
  /** only include channels relating to selected data */
  ADS_FILTER_ONLYSEL = (1 << 0),

  /* temporary filters */
  /** for 'Drivers' editor - only include Driver data from AnimData */
  ADS_FILTER_ONLYDRIVERS = (1 << 1),
  /** for 'NLA' editor - only include NLA data from AnimData */
  ADS_FILTER_ONLYNLA = (1 << 2),
  /** for Graph Editor - used to indicate whether to include a filtering flag or not */
  ADS_FILTER_SELEDIT = (1 << 3),

  /* general filtering */
  /** for 'DopeSheet' Editors - include 'summary' line */
  ADS_FILTER_SUMMARY = (1 << 4),

  /* datatype-based filtering */
  ADS_FILTER_NOSHAPEKEYS = (1 << 6),
  ADS_FILTER_NOMESH = (1 << 7),
  /** for animdata on object level, if we only want to concentrate on materials/etc. */
  ADS_FILTER_NOOBJ = (1 << 8),
  ADS_FILTER_NOLAT = (1 << 9),
  ADS_FILTER_NOCAM = (1 << 10),
  ADS_FILTER_NOMAT = (1 << 11),
  ADS_FILTER_NOLAM = (1 << 12),
  ADS_FILTER_NOCUR = (1 << 13),
  ADS_FILTER_NOWOR = (1 << 14),
  ADS_FILTER_NOSCE = (1 << 15),
  ADS_FILTER_NOPART = (1 << 16),
  ADS_FILTER_NOMBA = (1 << 17),
  ADS_FILTER_NOARM = (1 << 18),
  ADS_FILTER_NONTREE = (1 << 19),
  ADS_FILTER_NOTEX = (1 << 20),
  ADS_FILTER_NOSPK = (1 << 21),
  ADS_FILTER_NOLINESTYLE = (1 << 22),
  ADS_FILTER_NOMODIFIERS = (1 << 23),
  ADS_FILTER_NOGPENCIL = (1 << 24),
  /* NOTE: all new datablock filters will have to go in filterflag2 (see below) */

  /* NLA-specific filters */
  /** if the AnimData block has no NLA data, don't include to just show Action-line */
  ADS_FILTER_NLA_NOACT = (1 << 25),

  /* general filtering 3 */
  /** include 'hidden' channels too (i.e. those from hidden Objects/Bones) */
  ADS_FILTER_INCL_HIDDEN = (1 << 26),
  /** show only F-Curves which are disabled/have errors - for debugging drivers */
  ADS_FILTER_ONLY_ERRORS = (1 << 28),

  /* GPencil Mode */
  /** GP Mode - Only show datablocks used in the scene */
  ADS_FILTER_GP_3DONLY = (1 << 29),

  /** combination filters (some only used at runtime) */
  ADS_FILTER_NOOBDATA = (ADS_FILTER_NOCAM | ADS_FILTER_NOMAT | ADS_FILTER_NOLAM |
                         ADS_FILTER_NOCUR | ADS_FILTER_NOPART | ADS_FILTER_NOARM |
                         ADS_FILTER_NOSPK | ADS_FILTER_NOMODIFIERS),
} eDopeSheet_FilterFlag;

/* DopeSheet filter-flags - Overflow (filterflag2) */
typedef enum eDopeSheet_FilterFlag2 {
  ADS_FILTER_NOCACHEFILES = (1 << 1),
  ADS_FILTER_NOMOVIECLIPS = (1 << 2),
} eDopeSheet_FilterFlag2;

/* DopeSheet general flags */
typedef enum eDopeSheet_Flag {
  /** when summary is shown, it is collapsed, so all other channels get hidden */
  ADS_FLAG_SUMMARY_COLLAPSED = (1 << 0),
  /** show filters for datablocks */
  ADS_FLAG_SHOW_DBFILTERS = (1 << 1),

  /** use fuzzy/partial string matches when ADS_FILTER_BY_FCU_NAME is enabled
   * (WARNING: expensive operation) */
  ADS_FLAG_FUZZY_NAMES = (1 << 2),
  /** do not sort datablocks (mostly objects) by name (NOTE: potentially expensive operation) */
  ADS_FLAG_NO_DB_SORT = (1 << 3),
} eDopeSheet_Flag;

typedef struct SpaceAction_Runtime {
  char flag;
  char _pad0[7];
} SpaceAction_Runtime;

/* Action Editor Space. This is defined here instead of in DNA_space_types.h */
typedef struct SpaceAction {
  struct SpaceLink *next, *prev;
  /** Storage of regions for inactive spaces. */
  ListBase regionbase;
  char spacetype;
  char link_flag;
  char _pad0[6];
  /* End 'SpaceLink' header. */

  /** Copied to region. */
  View2D v2d DNA_DEPRECATED;

  /** The currently active action. */
  bAction *action;
  /** The currently active context (when not showing action). */
  bDopeSheet ads;

  /** For Time-Slide transform mode drawing - current frame?. */
  float timeslide;

  short flag;
  /* Editing context */
  char mode;
  /* Storage for sub-space types. */
  char mode_prev;
  /** Automatic keyframe snapping mode  . */
  char autosnap;
  /** (eTimeline_Cache_Flag). */
  char cache_display;
  char _pad1[6];

  SpaceAction_Runtime runtime;
} SpaceAction;

/* SpaceAction flag */
typedef enum eSAction_Flag {
  /* during transform (only set for TimeSlide) */
  SACTION_MOVING = (1 << 0),
  /* show sliders */
  SACTION_SLIDERS = (1 << 1),
  /* draw time in seconds instead of time in frames */
  SACTION_DRAWTIME = (1 << 2),
  /* don't filter action channels according to visibility */
  // SACTION_NOHIDE = (1 << 3), // XXX deprecated... old animation system
  /* don't kill overlapping keyframes after transform */
  SACTION_NOTRANSKEYCULL = (1 << 4),
  /* don't include keyframes that are out of view */
  // SACTION_HORIZOPTIMISEON = (1 << 5), // XXX deprecated... old irrelevant trick
  /* show pose-markers (local to action) in Action Editor mode  */
  SACTION_POSEMARKERS_SHOW = (1 << 6),
  /* don't draw action channels using group colors (where applicable) */
  SACTION_NODRAWGCOLORS = (1 << 7),
  /* don't draw current frame number beside frame indicator */
  SACTION_NODRAWCFRANUM = (1 << 8),
  /* don't perform realtime updates */
  SACTION_NOREALTIMEUPDATES = (1 << 10),
  /* move markers as well as keyframes */
  SACTION_MARKERS_MOVE = (1 << 11),
  /* show interpolation type */
  SACTION_SHOW_INTERPOLATION = (1 << 12),
  /* show extremes */
  SACTION_SHOW_EXTREMES = (1 << 13),
  /* show vertical line markers */
  SACTION_SHOW_MARKER_LINES = (1 << 14),
} eSAction_Flag;

/* SpaceAction_Runtime.flag */
typedef enum eSAction_Runtime_Flag {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
} eSAction_Runtime_Flag;

/* SpaceAction Mode Settings */
typedef enum eAnimEdit_Context {
  /* action on the active object */
  SACTCONT_ACTION = 0,
  /* list of all shapekeys on the active object, linked with their F-Curves */
  SACTCONT_SHAPEKEY = 1,
  /* editing of gpencil data */
  SACTCONT_GPENCIL = 2,
  /* dopesheet (default) */
  SACTCONT_DOPESHEET = 3,
  /* mask */
  SACTCONT_MASK = 4,
  /* cache file */
  SACTCONT_CACHEFILE = 5,
  /* timeline - replacement for the standalone "timeline editor" */
  SACTCONT_TIMELINE = 6,
} eAnimEdit_Context;

/* SpaceAction AutoSnap Settings (also used by other Animation Editors) */
typedef enum eAnimEdit_AutoSnap {
  /* no auto-snap */
  SACTSNAP_OFF = 0,
  /* snap to 1.0 frame/second intervals */
  SACTSNAP_STEP = 1,
  /* snap to actual frames/seconds (nla-action time) */
  SACTSNAP_FRAME = 2,
  /* snap to nearest marker */
  SACTSNAP_MARKER = 3,
  /* snap to actual seconds (nla-action time) */
  SACTSNAP_SECOND = 4,
  /* snap to 1.0 second increments */
  SACTSNAP_TSTEP = 5,
} eAnimEdit_AutoSnap;

/* SAction->cache_display */
typedef enum eTimeline_Cache_Flag {
  TIME_CACHE_DISPLAY = (1 << 0),
  TIME_CACHE_SOFTBODY = (1 << 1),
  TIME_CACHE_PARTICLES = (1 << 2),
  TIME_CACHE_CLOTH = (1 << 3),
  TIME_CACHE_SMOKE = (1 << 4),
  TIME_CACHE_DYNAMICPAINT = (1 << 5),
  TIME_CACHE_RIGIDBODY = (1 << 6),
} eTimeline_Cache_Flag;

/* ************************************************ */
/* Legacy Data */

/* WARNING: Action Channels are now deprecated... they were part of the old animation system!
 *        (ONLY USED FOR DO_VERSIONS...)
 *
 * Action Channels belong to Actions. They are linked with an IPO block, and can also own
 * Constraint Channels in certain situations.
 *
 * Action-Channels can only belong to one group at a time, but they still live the Action's
 * list of achans (to preserve backwards compatibility, and also minimize the code
 * that would need to be recoded). Grouped achans are stored at the start of the list, according
 * to the position of the group in the list, and their position within the group.
 */
typedef struct bActionChannel {
  struct bActionChannel *next, *prev;
  /** Action Group this Action Channel belongs to. */
  bActionGroup *grp;

  /** IPO block this action channel references. */
  struct Ipo *ipo;
  /** Constraint Channels (when Action Channel represents an Object or Bone). */
  ListBase constraintChannels;

  /** Settings accessed via bitmapping. */
  int flag;
  /** Channel name, MAX_NAME. */
  char name[64];
  /** Temporary setting - may be used to indicate group that channel belongs to during syncing. */
  int temp;
} bActionChannel;

/* Action Channel flags (ONLY USED FOR DO_VERSIONS...) */
typedef enum eActionChannelFlag {
  ACHAN_SELECTED = (1 << 0),
  ACHAN_HIGHLIGHTED = (1 << 1),
  ACHAN_HIDDEN = (1 << 2),
  ACHAN_PROTECTED = (1 << 3),
  ACHAN_EXPANDED = (1 << 4),
  ACHAN_SHOWIPO = (1 << 5),
  ACHAN_SHOWCONS = (1 << 6),
  ACHAN_MOVED = (1u << 31),
} eActionChannelFlag;

#endif /* __DNA_ACTION_TYPES_H__ */
