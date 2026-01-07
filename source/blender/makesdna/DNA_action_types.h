/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Define actions data-block for the animation system.
 * A collection of animation curves and drivers to be assigned to data-blocks
 * or sequenced in the non-linear-editor (NLA).
 */

#pragma once

#include <cmath>

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_listBase.h"
#include "DNA_session_uid_types.h"
#include "DNA_userdef_types.h" /* ThemeWireColor */
#include "DNA_view2d_types.h"

#include "BLI_enum_flags.hh"

#include <type_traits>

namespace blender {

struct AnimData;
struct Collection;
struct FCurve;
struct GHash;
struct Object;
struct SpaceLink;
namespace gpu {
class VertBuf;
class Batch;
}  // namespace gpu

/* Forward declarations so the actual declarations can happen top-down. */
struct ActionLayer;
struct ActionSlot;
struct ActionStrip;
struct ActionChannelbag;

/* Declarations of the C++ wrappers. */
namespace animrig {
class Action;
class Slot;
class SlotRuntime;
class Channelbag;
class ChannelGroup;
class Layer;
class Strip;
class StripKeyframeData;
}  // namespace animrig

/* The last_slot_handle is set to a high value to disambiguate slot handles from
 * array indices.
 *
 * This particular value was obtained by taking the 31 most-significant bits of
 * the TentHash value (Nathan's hash function) for the string "Quercus&Laksa"
 * (Sybren's cats). */
#define DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE 0x37627bf5

/** #bMotionPathVert::flag */
enum eMotionPathVert_Flag {
  /* vert is selected */
  MOTIONPATH_VERT_SEL = (1 << 0),
  MOTIONPATH_VERT_KEY = (1 << 1),
};

/* bMotionPath->flag */
enum eMotionPath_Flag {
  /* (for bones) path represents the head of the bone */
  MOTIONPATH_FLAG_BHEAD = (1 << 0),
  /* motion path is being edited */
  MOTIONPATH_FLAG_EDIT = (1 << 1),
  /* Custom colors */
  MOTIONPATH_FLAG_CUSTOM = (1 << 2),
  /* Draw lines or only points */
  MOTIONPATH_FLAG_LINES = (1 << 3),
  /* Bake to scene camera. */
  MOTIONPATH_FLAG_BAKE_CAMERA = (1 << 4),
};

/* bAnimVizSettings->recalc */
enum eAnimViz_RecalcFlags {
  /* Motion-paths need recalculating. */
  ANIMVIZ_RECALC_PATHS = (1 << 0),
};

/* bAnimVizSettings->path_type */
enum eMotionPaths_Types {
  /* show the paths along their entire ranges */
  MOTIONPATH_TYPE_RANGE = 0,
  /* only show the parts of the paths around the current frame */
  MOTIONPATH_TYPE_ACFRA = 1,
};

/* bAnimVizSettings->path_range */
enum eMotionPath_Ranges {
  /* Default is scene */
  MOTIONPATH_RANGE_SCENE = 0,
  MOTIONPATH_RANGE_KEYS_SELECTED = 1,
  MOTIONPATH_RANGE_KEYS_ALL = 2,
  MOTIONPATH_RANGE_MANUAL = 3,
};

/* bAnimVizSettings->path_viewflag */
enum eMotionPaths_ViewFlag {
  /* show frames on path */
  MOTIONPATH_VIEW_FNUMS = (1 << 0),
  /* show keyframes on path */
  MOTIONPATH_VIEW_KFRAS = (1 << 1),
  /* show keyframe/frame numbers */
  MOTIONPATH_VIEW_KFNOS = (1 << 2),
  /* find keyframes in whole action (instead of just in matching group name) */
  MOTIONPATH_VIEW_KFACT = (1 << 3),
  /* draw lines on path */
  /* MOTIONPATH_VIEW_LINES = (1 << 4), */ /* UNUSED */
};

/* bAnimVizSettings->path_bakeflag */
enum eMotionPath_BakeFlag {
  /** motion paths directly associated with this block of settings needs updating */
  /* MOTIONPATH_BAKE_NEEDS_RECALC = (1 << 0), */ /* UNUSED */
  /** for bones - calculate head-points for curves instead of tips */
  MOTIONPATH_BAKE_HEADS = (1 << 1),
  /** motion paths exist for AnimVizSettings instance - set when calc for first time,
   * and unset when clearing */
  MOTIONPATH_BAKE_HAS_PATHS = (1 << 2),
  /* Bake the path in camera space. */
  MOTIONPATH_BAKE_CAMERA_SPACE = (1 << 3),
};

/**
 * Runtime flags on pose bones. Those are only used internally and are not exposed to the user.
 */
enum bPoseChannelRuntimeFlag {
  /**
   * Used during transform. Not every selected bone is transformed. For example in a chain of
   * bones, only the first selected may be transformed.
   */
  POSE_RUNTIME_TRANSFORM = (1 << 0),
  /** Set to prevent hinge child bones from influencing the transform center. */
  POSE_RUNTIME_HINGE_CHILD_TRANSFORM = (1 << 1),
  /** Indicates that a parent is also being transformed. */
  POSE_RUNTIME_TRANSFORM_CHILD = (1 << 2),
  /** Set on bones during selection to tell following code that this bone should be operated on. */
  POSE_RUNTIME_IN_SELECTION_AREA = (1 << 3),
};

/* PoseChannel (transform) flags */
enum ePchan_Flag {
  /* has transforms */
  POSE_LOC = (1 << 0),
  POSE_ROT = (1 << 1),
  POSE_SCALE = (1 << 2),

  /* old IK/cache stuff
   * - used to be here from (1 << 3) to (1 << 8)
   *   but has been repurposed since 2.77.2
   *   as they haven't been used in over 10 years
   */

  /* has BBone deforms */
  POSE_BBONE_SHAPE = (1 << 3),
  /**
   * When set and bPoseChan.custom_tx is not a nullptr,
   * the gizmo will be drawn at the location and
   * orientation of the custom_tx instead of this bone.
   */
  POSE_TRANSFORM_AT_CUSTOM_TX = (1 << 4),
  /**
   * When set, transformations will modify the bone as if it was a child of the
   * #bPoseChan.custom_tx. The flag only has an effect when #POSE_TRANSFORM_AT_CUSTOM_TX and
   * `custom_tx` are set. This can be useful for rigs where the deformation is coming from
   * shape-keys in addition to the armature.
   */
  POSE_TRANSFORM_AROUND_CUSTOM_TX = (1 << 5),
  /**
   * Marks the pose bone as selected. Do not set directly, use
   * animrig::bone_select and animrig::bone_deselect instead.
   */
  POSE_SELECTED = (1 << 6),
  /**
   * Even though root and tip selection is not used in pose mode, we still have to store that
   * state in order to retain selection when switching back and forth between pose and edit mode.
   */
  POSE_SELECTED_ROOT = (1 << 7),
  POSE_SELECTED_TIP = (1 << 8),
  /**
   * selection state should only be against `POSE_SELECTED`. Do not set directly, use
   * animrig::bone_select and animrig::bone_deselect instead.
   */
  POSE_SELECTED_ALL = (POSE_SELECTED | POSE_SELECTED_ROOT | POSE_SELECTED_TIP),

  /* IK/Pose solving */
  POSE_CHAIN = (1 << 9),
  POSE_DONE = (1 << 10),
  /* POSE_KEY = (1 << 11) */     /* UNUSED */
  /* POSE_STRIDE = (1 << 12), */ /* UNUSED */
  /* standard IK solving */
  POSE_IKTREE = (1 << 13),
#if 0
  /* has Spline IK */
  POSE_HAS_IKS = (1 << 14),
#endif
  /* spline IK solving */
  POSE_IKSPLINE = (1 << 15),
};

/* PoseChannel constflag (constraint detection) */
enum ePchan_ConstFlag {
  PCHAN_HAS_IK = (1 << 0),           /* Has IK constraint. */
  PCHAN_HAS_CONST = (1 << 1),        /* Has any constraint. */
  /* PCHAN_HAS_ACTION = (1 << 2), */ /* UNUSED */
  PCHAN_HAS_NO_TARGET = (1 << 3),    /* Has (spline) IK constraint but no target is set. */
  /* PCHAN_HAS_STRIDE = (1 << 4), */ /* UNUSED */
  PCHAN_HAS_SPLINEIK = (1 << 5),     /* Has Spline IK constraint. */
  PCHAN_INFLUENCED_BY_IK = (1 << 6), /* Is part of a (non-spline) IK chain. */
};
ENUM_OPERATORS(ePchan_ConstFlag);

/* PoseChannel->ikflag */
enum ePchan_IkFlag {
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
};

/* PoseChannel->drawflag */
enum ePchan_DrawFlag {
  PCHAN_DRAW_NO_CUSTOM_BONE_SIZE = (1 << 0),
  PCHAN_DRAW_HIDDEN = (1 << 1),
};

/* NOTE: It doesn't take custom_scale_xyz into account. */
#define PCHAN_CUSTOM_BONE_LENGTH(pchan) \
  (((pchan)->drawflag & PCHAN_DRAW_NO_CUSTOM_BONE_SIZE) ? 1.0f : (pchan)->bone->length)

#ifdef DNA_DEPRECATED_ALLOW
/* PoseChannel->bboneflag */
enum ePchan_BBoneFlag {
  /* Use custom reference bones (for roll and handle alignment), instead of immediate neighbors */
  PCHAN_BBONE_CUSTOM_HANDLES = (1 << 1),
  /* Evaluate start handle as being "relative" */
  PCHAN_BBONE_CUSTOM_START_REL = (1 << 2),
  /* Evaluate end handle as being "relative" */
  PCHAN_BBONE_CUSTOM_END_REL = (1 << 3),
};
#endif

/* PoseChannel->rotmode and Object->rotmode */
enum eRotationModes {
  /* quaternion rotations (default, and for older Blender versions) */
  ROT_MODE_QUAT = 0,
  /* euler rotations - keep in sync with enum in BLI_math_rotation.h */
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
};

/* Pose->flag */
enum ePose_Flags {
  /* results in BKE_pose_rebuild being called */
  POSE_RECALC = (1 << 0),
  /* prevents any channel from getting overridden by anim from IPO */
  POSE_LOCKED = (1 << 1),
  /* clears the POSE_LOCKED flag for the next time the pose is evaluated */
  POSE_DO_UNLOCK = (1 << 2),
  /* pose has constraints which depend on time (used when depsgraph updates for a new frame) */
  POSE_CONSTRAINTS_TIMEDEPEND = (1 << 3),
  /* recalculate bone paths */
  /* POSE_RECALCPATHS = (1 << 4), */ /* UNUSED */
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
};

/* bPose->iksolver and bPose->ikparam->iksolver */
enum ePose_IKSolverType {
  IKSOLVER_STANDARD = 0,
  IKSOLVER_ITASC = 1,
};

/* bItasc->flag */
enum eItasc_Flags {
  ITASC_AUTO_STEP = (1 << 0),
  ITASC_INITIAL_REITERATION = (1 << 1),
  ITASC_REITERATION = (1 << 2),
  ITASC_SIMULATION = (1 << 3),
  /**
   * Set this flag to always translate root bones (i.e. bones without a parent) to (0, 0, 0).
   * This was the pre-3.6 behavior, and this flag was introduced for backward compatibility.
   */
  ITASC_TRANSLATE_ROOT_BONES = (1 << 4),
};

/* bItasc->solver */
enum eItasc_Solver {
  ITASC_SOLVER_SDLS = 0, /* selective damped least square, suitable for CopyPose constraint */
  ITASC_SOLVER_DLS = 1,  /* damped least square with numerical filtering of damping */
};

/* Action Group flags */
enum eActionGroup_Flag {
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
};

/** Flags for the action. */
enum eAction_Flags {
  /* flags for displaying in UI */
  ACT_COLLAPSED = (1 << 0),
  ACT_SELECTED = (1 << 1),

  /* flags for evaluation/editing */
  ACT_MUTED = (1 << 9),
  /* ACT_PROTECTED = (1 << 10), */ /* UNUSED */
  /* ACT_DISABLED = (1 << 11), */  /* UNUSED */
  /** The action has a manually set intended playback frame range. */
  ACT_FRAME_RANGE = (1 << 12),
  /** The action is intended to be a cycle (requires ACT_FRAME_RANGE). */
  ACT_CYCLIC = (1 << 13),
};

/** DopeSheet filter-flag. */
enum eDopeSheet_FilterFlag {
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

  /**
   * Show all Action slots; if not set, only show the Slot of the
   * data-block that's being animated by the Action.
   */
  ADS_FILTER_ONLY_SLOTS_OF_ACTIVE = (1 << 5),

  /* datatype-based filtering */
  ADS_FILTER_NOSHAPEKEYS = (1 << 6),
  ADS_FILTER_NOMESH = (1 << 7),
  /** For animation-data on object level, if we only want to concentrate on materials/etc. */
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

#if 0
  /** combination filters (some only used at runtime) */
  ADS_FILTER_NOOBDATA = (ADS_FILTER_NOCAM | ADS_FILTER_NOMAT | ADS_FILTER_NOLAM |
                         ADS_FILTER_NOCUR | ADS_FILTER_NOPART | ADS_FILTER_NOARM |
                         ADS_FILTER_NOSPK | ADS_FILTER_NOMODIFIERS),
#endif
};
ENUM_OPERATORS(eDopeSheet_FilterFlag);

/* DopeSheet filter-flags - Overflow (filterflag2) */
enum eDopeSheet_FilterFlag2 {
  ADS_FILTER_NOCACHEFILES = (1 << 1),
  ADS_FILTER_NOMOVIECLIPS = (1 << 2),
  ADS_FILTER_NOHAIR = (1 << 3),
  ADS_FILTER_NOPOINTCLOUD = (1 << 4),
  ADS_FILTER_NOVOLUME = (1 << 5),

  /** Include working drivers with variables using their fallback values into Only Show Errors. */
  ADS_FILTER_DRIVER_FALLBACK_AS_ERROR = (1 << 6),

  ADS_FILTER_NOLIGHTPROBE = (1 << 7),
};
ENUM_OPERATORS(eDopeSheet_FilterFlag2);

/* DopeSheet general flags */
enum eDopeSheet_Flag {
  /** when summary is shown, it is collapsed, so all other channels get hidden */
  ADS_FLAG_SUMMARY_COLLAPSED = (1 << 0),
  /** show filters for datablocks */
  ADS_FLAG_SHOW_DBFILTERS = (1 << 1),

  /** use fuzzy/partial string matches when ADS_FILTER_BY_FCU_NAME is enabled
   * (WARNING: expensive operation) */
  ADS_FLAG_FUZZY_NAMES = (1 << 2),
  /** do not sort datablocks (mostly objects) by name (NOTE: potentially expensive operation) */
  ADS_FLAG_NO_DB_SORT = (1 << 3),
  /** Invert the search filter */
  ADS_FLAG_INVERT_FILTER = (1 << 4),
};

enum SpaceActionOverlays_Flag {
  ADS_OVERLAY_SHOW_OVERLAYS = (1 << 0),
  ADS_SHOW_SCENE_STRIP_FRAME_RANGE = (1 << 1)
};

/* SpaceAction flag */
enum eSAction_Flag {
  /* during transform (only set for TimeSlide) */
  SACTION_MOVING = (1 << 0),
  /* show sliders */
  SACTION_SLIDERS = (1 << 1),
  /* draw time in seconds instead of time in frames */
  SACTION_DRAWTIME = (1 << 2),
  /* don't filter action channels according to visibility */
  // SACTION_NOHIDE = (1 << 3), /* Deprecated, old animation systems. */
  /* don't kill overlapping keyframes after transform */
  SACTION_NOTRANSKEYCULL = (1 << 4),
  /* don't include keyframes that are out of view */
  // SACTION_HORIZOPTIMISEON = (1 << 5), /* Deprecated, old irrelevant trick. */
  /* show pose-markers (local to action) in Action Editor mode. */
  SACTION_POSEMARKERS_SHOW = (1 << 6),
  /* don't draw action channels using group colors (where applicable) */
  /* SACTION_NODRAWGCOLORS = (1 << 7), DEPRECATED */
  /* SACTION_NODRAWCFRANUM = (1 << 8), DEPRECATED */
  /* don't perform realtime updates */
  SACTION_NOREALTIMEUPDATES = (1 << 10),
  /* move markers as well as keyframes */
  SACTION_MARKERS_MOVE = (1 << 11),
  /* show interpolation type */
  SACTION_SHOW_INTERPOLATION = (1 << 12),
  /* show extremes */
  SACTION_SHOW_EXTREMES = (1 << 13),
  /* show markers region */
  SACTION_SHOW_MARKERS = (1 << 14),
};

/** #SpaceAction_Runtime.flag */
enum eSAction_Runtime_Flag {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
};

/** #SpaceAction.mode */
enum eAnimEdit_Context {
  /** Action on the active object. */
  SACTCONT_ACTION = 0,
  /** List of all shape-keys on the active object, linked with their F-Curves. */
  SACTCONT_SHAPEKEY = 1,
  /** Editing of grease-pencil data. */
  SACTCONT_GPENCIL = 2,
  /** Dope-sheet (default). */
  SACTCONT_DOPESHEET = 3,
  /** Mask. */
  SACTCONT_MASK = 4,
  /** Cache file */
  SACTCONT_CACHEFILE = 5,
  /** Timeline. */
  SACTCONT_TIMELINE = 6,
};

/* Old snapping enum that is only needed because of the versioning code. */
enum DNA_DEPRECATED eAnimEdit_AutoSnap {
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
};

/* SAction->cache_display */
enum eTimeline_Cache_Flag {
  TIME_CACHE_DISPLAY = (1 << 0),
  TIME_CACHE_SOFTBODY = (1 << 1),
  TIME_CACHE_PARTICLES = (1 << 2),
  TIME_CACHE_CLOTH = (1 << 3),
  TIME_CACHE_SMOKE = (1 << 4),
  TIME_CACHE_DYNAMICPAINT = (1 << 5),
  TIME_CACHE_RIGIDBODY = (1 << 6),
  TIME_CACHE_SIMULATION_NODES = (1 << 7),
};

/* ************************************************ */
/* Visualization */

/* Motion Paths ------------------------------------ */
/* (used for Pose Channels and Objects) */

/** Data point for motion path (`mpv`). */
struct bMotionPathVert {
  /** Coordinates of point in 3D-space. */
  float co[3] = {};
  /** Quick settings. */
  int flag = 0;
};

/* ........ */

/* Motion Path data cache (mpath)
 * - for elements providing transforms (i.e. Objects or PoseChannels)
 */
struct bMotionPath {
  /** Path samples. */
  bMotionPathVert *points = nullptr;
  /** The number of cached verts. */
  int length = 0;

  /** For drawing paths, the start frame number. Inclusive. */
  int start_frame = 0;
  /** For drawing paths, the end frame number. Exclusive. */
  int end_frame = 0;

  /** Optional custom color. */
  float color[3] = {};
  float color_post[3] = {};
  /** Line thickness. */
  int line_thickness = 0;
  /** Baking settings - eMotionPath_Flag. */
  int flag = 0;

  char _pad2[4] = {};
  /* Used for drawing. */
  gpu::VertBuf *points_vbo = nullptr;
  gpu::Batch *batch_line = nullptr;
  gpu::Batch *batch_points = nullptr;
  void *_pad = nullptr;
};

/* Visualization General --------------------------- */
/* for Objects or Poses (but NOT PoseChannels) */

/* Animation Visualization Settings (avs) */
struct bAnimVizSettings {
  /* General Settings ------------------------ */
  /** #eAnimViz_RecalcFlags. */
  short recalc = 0;

  /* Motion Path Settings ------------------- */
  /** #eMotionPath_Types. */
  short path_type = 0;
  /** Number of frames between points indicated on the paths. */
  short path_step = 0;
  /** #eMotionPath_Ranges. */
  short path_range = 0;

  /** #eMotionPaths_ViewFlag. */
  short path_viewflag = 0;
  /** #eMotionPath_BakeFlag. */
  short path_bakeflag = 0;
  char _pad[4] = {};

  /** Start and end frames of path-calculation range. Both are inclusive. */
  int path_sf = 0, path_ef = 0;
  /** Number of frames before/after current frame to show. */
  int path_bc = 0, path_ac = 0;
};

/* runtime */
#
#
struct bPoseChannelDrawData {
  float solid_color[4] = {};
  float wire_color[4] = {};

  int bbone_matrix_len = 0;
  /* keep last */
  float bbone_matrix[0][4][4] = {};
};

struct DualQuat;
struct Mat4;

/* Describes a plane in pose space that delimits B-Bone segments. */
struct bPoseChannel_BBoneSegmentBoundary {
  /* Boundary data in pose space. */
  float point[3] = {};
  float plane_normal[3] = {};
  /* Dot product of point and plane_normal to speed up distance computation. */
  float plane_offset = 0;

  /**
   * Inverse width of the smoothing at this level in head-tail space.
   * Optimization: this value is actually indexed by BSP depth (0 to `bsp_depth - 1`), not joint
   * index. It's put here to avoid allocating a separate array by utilizing the padding space.
   */
  float depth_scale = 0;
};

struct bPoseChannel_Runtime {
  SessionUID session_uid;

  /* Cached dual quaternion for deformation. */
  struct DualQuat deform_dual_quat;

  /* B-Bone shape data: copy of the segment count for validation. */
  int bbone_segments = 0;

  /* Inverse of the total length of the segment polyline. */
  float bbone_arc_length_reciprocal = 0;
  /* bPoseChannelRuntimeFlag */
  uint8_t flag = 0;
  char _pad1[3] = {};

  /* Rest and posed matrices for segments. */
  struct Mat4 *bbone_rest_mats = nullptr;
  struct Mat4 *bbone_pose_mats = nullptr;

  /* Delta from rest to pose in matrix and DualQuat form. */
  struct Mat4 *bbone_deform_mats = nullptr;
  struct DualQuat *bbone_dual_quats = nullptr;

  /* Segment boundaries for curved mode. */
  struct bPoseChannel_BBoneSegmentBoundary *bbone_segment_boundaries = nullptr;
  void *_pad = nullptr;
};

/* ************************************************ */
/* Poses */

/* PoseChannel ------------------------------------ */

/**
 * PoseChannel
 *
 * A #bPoseChannel stores the results of Actions and transform information
 * with respect to the rest-position of #bArmature bones.
 */
struct bPoseChannel {
  DNA_DEFINE_CXX_METHODS(bPoseChannel)

  struct bPoseChannel *next = nullptr, *prev = nullptr;

  /**
   * User-defined custom properties storage on this PoseChannel. Typically Accessed through the
   * 'dict' syntax from Python.
   */
  IDProperty *prop = nullptr;

  /**
   * System-defined custom properties storage. Used to store data dynamically defined either by
   * Blender itself (e.g. the GeoNode modifier), or some python script, extension etc.
   *
   * Typically accessed through RNA paths (`C.object.my_dynamic_float_property = 33.3`), when
   * wrapped/defined by RNA.
   */
  IDProperty *system_properties = nullptr;

  /** Constraints that act on this PoseChannel. */
  ListBaseT<struct bConstraint> constraints = {nullptr, nullptr};
  char name[/*MAXBONENAME*/ 64] = "";

  /** Dynamic, for detecting transform changes (ePchan_Flag). */
  short flag = 0;
  /** Settings for IK bones. */
  short ikflag = 0;
  /** Protect channels from being transformed. */
  short protectflag = 0;
  /** Index of action-group this bone belongs to (0 = default/no group). */
  short agrp_index = 0;
  /** For quick detecting which constraints affect this channel. */
  char constflag = 0;
  /**
   * This used to store the selection-flag for serialization but is not longer required
   * since that is now natively stored on the `flag` property.
   */
  DNA_DEPRECATED char selectflag = 0;
  char drawflag = 0;
  DNA_DEPRECATED char bboneflag = 0;
  char _pad0[4] = {};

  /** Set on read file or rebuild pose. */
  struct Bone *bone = nullptr;
  /** Set on read file or rebuild pose. */
  struct bPoseChannel *parent = nullptr;
  /** Set on read file or rebuild pose, the 'ik' child, for b-bones. */
  struct bPoseChannel *child = nullptr;

  /** "IK trees" - only while evaluating pose. */
  ListBaseT<struct PoseTree> iktree = {nullptr, nullptr};
  /** Spline-IK "trees" - only while evaluating pose. */
  ListBaseT<struct tSplineIK_Tree> siktree = {nullptr, nullptr};

  /** Motion path cache for this bone. */
  bMotionPath *mpath = nullptr;
  /**
   * Draws custom object instead of default bone shape.
   *
   * \note For the purpose of user interaction (selection, display etc),
   * it's important this value is treated as NULL when #ARM_NO_CUSTOM is set.
   */
  struct Object *custom = nullptr;
  /**
   * This is a specific feature to display with another bones transform.
   * Needed in rare cases for advanced rigs, since alternative solutions are highly complicated.
   *
   * \note This depends #bPoseChannel.custom being set and the #ARM_NO_CUSTOM flag being unset.
   */
  struct bPoseChannel *custom_tx = nullptr;
  float custom_scale = 0; /* Deprecated */
  float custom_scale_xyz[3] = {1.0f, 1.0f, 1.0f};
  float custom_translation[3] = {0.0f, 0.0f, 0.0f};
  float custom_rotation_euler[3] = {};
  float custom_shape_wire_width = 0;

  /** Transforms - written in by actions or transform. */
  float loc[3] = {};
  float scale[3] = {1.0f, 1.0f, 1.0f};

  /**
   * Rotations - written in by actions or transform
   * (but only one representation gets used at any time)
   */
  /** Euler rotation. */
  float eul[3] = {};
  /** Quaternion rotation. */
  float quat[4] = {1.0f, 0.0f, 0.0f};
  /** Axis-angle rotation. */
  float rotAxis[3] = {}, rotAngle = 0;
  /** #eRotationModes - rotation representation to use. */
  short rotmode = 0;
  char _pad[6] = {};

  /**
   * Matrix result of location/rotation/scale components, and evaluation of
   * animation data and constraints.
   *
   * This is the dynamic component of `pose_mat` (without #Bone.arm_mat).
   */
  float chan_mat[4][4] = {};
  /**
   * Channel matrix in the armature object space, i.e. `pose_mat = bone->arm_mat * chan_mat`.
   */
  float pose_mat[4][4] = {};
  /** For display, pose_mat with bone length applied. */
  float disp_mat[4][4] = {};
  /** For display, pose_mat with bone length applied and translated to tail. */
  float disp_tail_mat[4][4] = {};
  /**
   * Inverse result of constraints.
   * doesn't include effect of rest-position, parent, and local transform.
   */
  float constinv[4][4] = {};

  /** Actually pose_mat[3]. */
  float pose_head[3] = {};
  /** Also used for drawing help lines. */
  float pose_tail[3] = {};

  /** DOF constraint, note! - these are stored in degrees, not radians. */
  float limitmin[3] = {}, limitmax[3] = {};
  /** DOF stiffness. */
  float stiffness[3] = {};
  float ikstretch = 0;
  /** Weight of joint rotation constraint. */
  float ikrotweight = 0;
  /** Weight of joint stretch constraint. */
  float iklinweight = 0;

  /**
   * Curved bones settings - these are for animating,
   * and are applied on top of the copies in pchan->bone
   */
  float roll1 = 0, roll2 = 0;
  float curve_in_x = 0, curve_in_z = 0;
  float curve_out_x = 0, curve_out_z = 0;
  float ease1 = 0, ease2 = 0;
  DNA_DEPRECATED float scale_in_x = 0;
  DNA_DEPRECATED float scale_in_z = 0;
  DNA_DEPRECATED float scale_out_x = 0;
  DNA_DEPRECATED float scale_out_z = 0;
  float scale_in[3] = {1.0f, 1.0f, 1.0f};
  float scale_out[3] = {1.0f, 1.0f, 1.0f};

  /** B-Bone custom handles; set on read file or rebuild pose based on pchan->bone data. */
  struct bPoseChannel *bbone_prev = nullptr;
  struct bPoseChannel *bbone_next = nullptr;

  /** Use for outliner. */
  void *temp = nullptr;
  /** Runtime data for color and bbone segment matrix. */
  bPoseChannelDrawData *draw_data = nullptr;

  /** Points to an original pose channel. */
  struct bPoseChannel *orig_pchan = nullptr;

  BoneColor color; /* MUST be named the same as in Bone and EditBone structs. */

  void *_pad2 = nullptr;

  /** Runtime data (keep last). */
  struct bPoseChannel_Runtime runtime;
};

/* Pose ------------------------------------ */

/* Pose-Object.
 *
 * It is only found under ob->pose. It is not library data, even
 * though there is a define for it (hack for the outliner).
 */
struct bPose {
  /** List of pose channels, PoseBones in RNA. */
  ListBaseT<bPoseChannel> chanbase = {nullptr, nullptr};
  /** Use a hash-table for quicker string lookups. */
  struct GHash *chanhash = nullptr;

  /* Flat array of pose channels. It references pointers from
   * chanbase. Used for quick pose channel lookup from an index.
   */
  bPoseChannel **chan_array = nullptr;

  short flag = 0;
  char _pad[2] = {};

  /** Local action time of this pose. */
  float ctime = 0;
  /** Applied to object. */
  float stride_offset[3] = {};
  /** Result of match and cycles, applied in BKE_pose_where_is(). */
  float cyclic_offset[3] = {};

  /** List of bActionGroups. */
  ListBaseT<struct bActionGroup> agroups = {nullptr, nullptr};

  /** Index of active group (starts from 1). */
  int active_group = 0;
  /** Ik solver to use, see ePose_IKSolverType. */
  int iksolver = 0;
  /** Temporary IK data, depends on the IK solver. Not saved in file. */
  void *ikdata = nullptr;
  /** IK solver parameters, structure depends on iksolver. */
  void *ikparam = nullptr;

  /** Settings for visualization of bone animation. */
  bAnimVizSettings avs;
};

/* IK Solvers ------------------------------------ */

/* header for all bPose->ikparam structures */
struct bIKParam {
  int iksolver = 0;
};

/* bPose->ikparam when bPose->iksolver=1 */
struct bItasc {
  int iksolver = 0;
  float precision = 0;
  short numiter = 0;
  short numstep = 0;
  float minstep = 0;
  float maxstep = 0;
  short solver = 0;
  short flag = 0;
  float feedback = 0;
  /** Max velocity to SDLS solver. */
  float maxvel = 0;
  /** Maximum damping for DLS solver. */
  float dampmax = 0;
  /** Threshold of singular value from which the damping start progressively. */
  float dampeps = 0;
};

/* ************************************************ */
/* Action */

/* Groups -------------------------------------- */

/**
 * Action-Channel Group (agrp)
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
struct bActionGroup {
  struct bActionGroup *next = nullptr, *prev = nullptr;

  /**
   * List of channels in this group for legacy actions.
   *
   * NOTE: this must not be touched by standard listbase functions
   * which would clear links to other channels.
   */
  ListBaseT<FCurve> channels = {nullptr, nullptr};

  /**
   * Span of channels in this group for layered actions.
   *
   * This specifies that span as a range of items in a Channelbag's fcurve
   * array.
   *
   * Note that empty groups (`fcurve_range_length == 0`) are allowed, and they
   * still have a position in the fcurves array, as specified by
   * `fcurve_range_start`. You can imagine these cases as a zero-width range
   * that sits at the border between the element at `fcurve_range_start` and the
   * element just before it.
   */
  int fcurve_range_start = 0;
  int fcurve_range_length = 0;

  /**
   * For layered actions: the Channelbag this group belongs to.
   *
   * This is needed in the keyframe drawing code, etc., to give direct access to
   * the fcurves in this group.
   */
  struct ActionChannelbag *channelbag = nullptr;

  /** Settings for this action-group. */
  int flag = 0;
  /**
   * Index of custom color set to use when used for bones
   * (0=default - used for all old files, -1=custom set).
   */
  int customCol = 0;
  /** Name of the group. */
  char name[64] = "";

  /** Color set to use when customCol == -1. */
  ThemeWireColor cs = {};

#ifdef __cplusplus
  animrig::ChannelGroup &wrap();
  const animrig::ChannelGroup &wrap() const;
#endif
};

/* Actions -------------------------------------- */

/**
 * Container of animation data.
 *
 * \see animrig::Action for more detailed documentation.
 */
struct bAction {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_AC;
#endif

  /** ID-serialization for relinking. */
  ID id;

  /** Array of `layer_array_num` layers. */
  struct ActionLayer **layer_array = nullptr;
  int layer_array_num = 0;
  int layer_active_index = 0; /* Index into layer_array, -1 means 'no active'. */

  /** Array of `slot_array_num` slots. */
  struct ActionSlot **slot_array = nullptr;
  int slot_array_num = 0;
  int32_t last_slot_handle = DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE;

  /* Storage for the underlying data of strips. Each strip type has its own
   * array, and strips reference this data with an enum indicating the strip
   * type and an int containing the index in the array to use.
   *
   * NOTE: when adding new strip data arrays, also update `duplicate_slot()`. */
  struct ActionStripKeyframeData **strip_keyframe_data_array = nullptr;
  int strip_keyframe_data_array_num = 0;

  char _pad0[4] = {};

  /* Note about legacy animation data:
   *
   * Blender 2.5 introduced a new animation system 'Animato'. This replaced the
   * IPO ('interpolation') curves with F-Curves. Both are considered 'legacy' at
   * different levels:
   *
   * - Actions with F-Curves in `curves`, as introduced in Blender 2.5, are
   *   considered 'legacy' but still functional in current Blender.
   * - Pre-2.5 data are deprecated and old files are automatically converted to
   *   the post-2.5 data model.
   */

  /** Legacy F-Curves (FCurve), introduced in Blender 2.5. */
  ListBaseT<FCurve> curves = {nullptr, nullptr};
  /** Legacy Groups of function-curves (bActionGroup), introduced in Blender 2.5. */
  ListBaseT<bActionGroup> groups = {nullptr, nullptr};

  /** Markers local to the Action (used to provide Pose-Libraries). */
  ListBaseT<struct TimeMarker> markers = {nullptr, nullptr};

  /** Settings for this action. \see eAction_Flags */
  int flag = 0;
  /** Index of the active marker. */
  int active_marker = 0;

  /**
   * Type of ID-blocks that action can be assigned to
   * (if 0, will be set to whatever ID first evaluates it).
   */
  int idroot = 0;
  char _pad1[4] = {};

  /**
   * Start and end of the manually set intended playback frame range. Used by UI and
   * some editing tools, but doesn't directly affect animation evaluation in any way.
   */
  float frame_start = 0, frame_end = 0;

  PreviewImage *preview = nullptr;

#ifdef __cplusplus
  animrig::Action &wrap();
  const animrig::Action &wrap() const;
#endif
};

/* ************************************************ */
/* Action/Dope-sheet Editor */

/** Storage for Dope-sheet/Grease-Pencil Editor data. */
struct bDopeSheet {
  /** Currently ID_SCE (for Dope-sheet), and ID_SC (for Grease Pencil). */
  ID *source = nullptr;
  /** Cache for channels (only initialized when pinned). */ /* XXX not used! */
  ListBase chanbase = {nullptr, nullptr};

  /** Object group for option to only include objects that belong to this Collection. */
  struct Collection *filter_grp = nullptr;
  /** String to search for in displayed names of F-Curves, or NlaTracks/GP Layers/etc. */
  char searchstr[64] = "";

  /** Flags to use for filtering data #eAnimFilter_Flags. */
  int filterflag = 0;
  /** #eDopeSheet_FilterFlag2 */
  int filterflag2 = 0;
  /** Standard flags. */
  int flag = 0;

  /** `index + 1` of channel to rename - only gets set by renaming operator. */
  int renameIndex = 0;
};

struct SpaceAction_Runtime {
  char flag = 0;
  char _pad0[7] = {};
};

struct SpaceActionOverlays {
  /** #SpaceActionOverlays_Flag */
  int flag = 0;
  char _pad0[4] = {};
};

/* Action Editor Space. This is defined here instead of in DNA_space_types.h */
struct SpaceAction {
  DNA_DEFINE_CXX_METHODS(SpaceAction)

  struct SpaceLink *next = nullptr, *prev = nullptr;
  /** Storage of regions for inactive spaces. */
  ListBaseT<struct ARegion> regionbase = {nullptr, nullptr};
  char spacetype = 0;
  char link_flag = 0;
  char _pad0[6] = {};
  /* End 'SpaceLink' header. */

  /** Copied to region. */
  DNA_DEPRECATED View2D v2d;

  /** The currently active action (deprecated). */
  DNA_DEPRECATED bAction *action = nullptr;

  /** The currently active context (when not showing action). */
  bDopeSheet ads;

  /** For Time-Slide transform mode drawing - current frame? */
  float timeslide = 0;

  short flag = 0;
  /* Editing context */
  char mode = 0;
  /* Storage for sub-space types. */
  char mode_prev = 0;
  /* Snapping now lives on the Scene. */
  DNA_DEPRECATED char autosnap = 0;
  /** (eTimeline_Cache_Flag). */
  char cache_display = 0;
  char _pad1[6] = {};

  SpaceActionOverlays overlays;

  SpaceAction_Runtime runtime;
};

/* ************************************************ */
/* Layered Animation data-types. */

/**
 * \see #animrig::Layer
 */
struct ActionLayer {
  /** User-Visible identifier, unique within the Animation. */
  char name[/*MAX_NAME*/ 64] = "";

  float influence = 1.0f; /* [0-1] */

  /** \see #animrig::Layer::flags() */
  uint8_t layer_flags = 0;

  /** \see #animrig::Layer::mixmode() */
  int8_t layer_mix_mode = 0;

  uint8_t _pad0[2] = {};

  /**
   * The layer's array of strips. See the documentation of
   * #animrig::Layer for the invariants of this array.
   */
  struct ActionStrip **strip_array = nullptr; /* Array of 'strip_array_num' strips. */
  int strip_array_num = 0;

  uint8_t _pad1[4] = {};

#ifdef __cplusplus
  animrig::Layer &wrap();
  const animrig::Layer &wrap() const;
#endif
};

/**
 * \see #animrig::Slot
 */
struct ActionSlot {
  /**
   * The string identifier of this Slot within the Action.
   *
   * The first two characters are the two-letter code corresponding to `idtype`
   * below (e.g. 'OB', 'ME', 'LA'), and the remaining characters store slot's
   * display name. Since the combination of the `idtype` and display name are
   * always unique within an action, this string identifier is as well.
   *
   * Typically this matches the ID name this slot was created for, including the
   * two letters indicating the ID type.
   *
   * \see #AnimData::slot_name
   */
  char identifier[/*MAX_ID_NAME*/ 258] = "";

  /**
   * Type of ID-block that this slot is intended for.
   *
   * If 0, will be set to whatever ID is first assigned.
   */
  int16_t idtype = 0;

  /**
   * Numeric identifier of this Slot within the Action.
   *
   * This number allows reorganization of the #bAction::slot_array without
   * invalidating references. Also these remain valid when copy-on-evaluate
   * copies are made.
   *
   * Unlike `identifier` above, this cannot be set by the user and never changes
   * after initial assignment, and thus serves as a "forever" identifier of the
   * slot.
   *
   * Only valid within the Action that owns this Slot.
   *
   * NOTE: keep this type in sync with `slot_handle_t` in BKE_action.hh.
   *
   * \see #animrig::Action::slot_for_handle()
   */
  int32_t handle = 0;

  /** \see #animrig::Slot::flags() */
  int8_t slot_flags = 0;
  uint8_t _pad1[7] = {};

  /** Runtime data. Set to nullptr when writing to disk. */
  animrig::SlotRuntime *runtime = nullptr;

#ifdef __cplusplus
  animrig::Slot &wrap();
  const animrig::Slot &wrap() const;
#endif
};

/**
 * \see #animrig::Strip
 */
struct ActionStrip {
  /**
   * \see #animrig::Strip::type()
   */
  int8_t strip_type = 0;
  uint8_t _pad0[3] = {};

  /**
   * The index of the "strip data" item that this strip uses, in the array of
   * strip data that corresponds to `strip_type`.
   *
   * Note that -1 indicates "no data".  This is an invalid state outside of
   * specific internal APIs, but it's the default value and therefore helps us
   * catch when strips aren't fully initialized before making their way outside
   * of those APIs.
   */
  int data_index = -1;

  float frame_start = -INFINITY; /** Start frame of the strip, in Animation time. */
  float frame_end = INFINITY;    /** End frame of the strip, in Animation time. */

  /**
   * Offset applied to the contents of the strip, in frames.
   *
   * This offset determines the difference between "Animation time" (which would
   * typically be the same as the scene time, until the animation system
   * supports strips referencing other Actions).
   */
  float frame_offset = 0;

  uint8_t _pad1[4] = {};

#ifdef __cplusplus
  animrig::Strip &wrap();
  const animrig::Strip &wrap() const;
#endif
};

/**
 * #ActionStrip::type = #Strip::Type::Keyframe.
 *
 * \see #animrig::StripKeyframeData
 */
struct ActionStripKeyframeData {
  struct ActionChannelbag **channelbag_array = nullptr;
  int channelbag_array_num = 0;

  uint8_t _pad[4] = {};

#ifdef __cplusplus
  animrig::StripKeyframeData &wrap();
  const animrig::StripKeyframeData &wrap() const;
#endif
};

/**
 * \see #animrig::Channelbag
 */
struct ActionChannelbag {
  int32_t slot_handle = 0;

  /* Channel groups. These index into the `fcurve_array` below to specify group
   * membership of the fcurves.
   *
   * Note that although the fcurves also have pointers back to the groups they
   * belong to, those pointers are not the source of truth. The source of truth
   * for membership is the information in the channel groups here.
   *
   * Invariants:
   * 1. The groups are sorted by their `fcurve_range_start` field. In other
   *    words, they are in the same order as their starting positions in the
   *    fcurve array.
   * 2. The grouped fcurves are tightly packed, starting at the first fcurve and
   *    having no gaps of ungrouped fcurves between them. Ungrouped fcurves come
   *    at the end, after all of the grouped fcurves. */
  int group_array_num = 0;
  struct bActionGroup **group_array = nullptr;

  uint8_t _pad[4] = {};

  int fcurve_array_num = 0;
  struct FCurve **fcurve_array = nullptr; /* Array of 'fcurve_array_num' FCurves. */

  /* TODO: Design & implement a way to integrate other channel types as well,
   * and still have them map to a certain slot */
#ifdef __cplusplus
  animrig::Channelbag &wrap();
  const animrig::Channelbag &wrap() const;
#endif
};

#ifdef __cplusplus
/* Some static assertions that things that should have the same type actually do. */
static_assert(std::is_same_v<decltype(ActionSlot::handle), decltype(bAction::last_slot_handle)>);
static_assert(
    std::is_same_v<decltype(ActionSlot::handle), decltype(ActionChannelbag::slot_handle)>);
#endif

}  // namespace blender
