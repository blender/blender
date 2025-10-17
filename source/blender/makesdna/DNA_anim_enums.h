/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/* ************************************************ */
/* F-Curve DataTypes */

/* Modifiers -------------------------------------- */

/**
 * Types of F-Curve modifier
 * WARNING: order here is important!
 */
typedef enum eFModifier_Types {
  FMODIFIER_TYPE_NULL = 0,
  FMODIFIER_TYPE_GENERATOR = 1,
  FMODIFIER_TYPE_FN_GENERATOR = 2,
  FMODIFIER_TYPE_ENVELOPE = 3,
  FMODIFIER_TYPE_CYCLES = 4,
  FMODIFIER_TYPE_NOISE = 5,
  FMODIFIER_TYPE_FILTER = 6, /* Was never implemented, removed in #123906. */
  FMODIFIER_TYPE_PYTHON = 7, /* Was never implemented, removed in #123906. */
  FMODIFIER_TYPE_LIMITS = 8,
  FMODIFIER_TYPE_STEPPED = 9,

  /* NOTE: all new modifiers must be added above this line */
  FMODIFIER_NUM_TYPES,
} eFModifier_Types;

/** F-Curve Modifier Settings. */
typedef enum eFModifier_Flags {
  /** Modifier is not able to be evaluated for some reason, and should be skipped (internal). */
  FMODIFIER_FLAG_DISABLED = (1 << 0),
#ifdef DNA_DEPRECATED_ALLOW
  /** Modifier's data is expanded (in UI). Deprecated, use `ui_expand_flag`. */
  FMODIFIER_FLAG_EXPANDED = (1 << 1),
#endif
  /** Modifier is active one (in UI) for editing purposes. */
  FMODIFIER_FLAG_ACTIVE = (1 << 2),
  /** User wants modifier to be skipped. */
  FMODIFIER_FLAG_MUTED = (1 << 3),
  /** Restrict range that F-Modifier can be considered over. */
  FMODIFIER_FLAG_RANGERESTRICT = (1 << 4),
  /** Use influence control. */
  FMODIFIER_FLAG_USEINFLUENCE = (1 << 5),
} eFModifier_Flags;

/* --- */

/* generator modes */
typedef enum eFMod_Generator_Modes {
  FCM_GENERATOR_POLYNOMIAL = 0,
  FCM_GENERATOR_POLYNOMIAL_FACTORISED = 1,
} eFMod_Generator_Modes;

/* generator flags
 * - shared by Generator and Function Generator
 */
typedef enum eFMod_Generator_Flags {
  /* generator works in conjunction with other modifiers (i.e. doesn't replace those before it) */
  FCM_GENERATOR_ADDITIVE = (1 << 0),
} eFMod_Generator_Flags;

/* 'function' generator types */
typedef enum eFMod_Generator_Functions {
  FCM_GENERATOR_FN_SIN = 0,
  FCM_GENERATOR_FN_COS = 1,
  FCM_GENERATOR_FN_TAN = 2,
  FCM_GENERATOR_FN_SQRT = 3,
  FCM_GENERATOR_FN_LN = 4,
  FCM_GENERATOR_FN_SINC = 5,
} eFMod_Generator_Functions;

/* cycling modes */
typedef enum eFMod_Cycling_Modes {
  /** don't do anything */
  FCM_EXTRAPOLATE_NONE = 0,
  /** repeat keyframe range as-is */
  FCM_EXTRAPOLATE_CYCLIC,
  /** repeat keyframe range, but with offset based on gradient between values */
  FCM_EXTRAPOLATE_CYCLIC_OFFSET,
  /** alternate between forward and reverse playback of keyframe range */
  FCM_EXTRAPOLATE_MIRROR,
} eFMod_Cycling_Modes;

/* limiting flags */
typedef enum eFMod_Limit_Flags {
  FCM_LIMIT_XMIN = (1 << 0),
  FCM_LIMIT_XMAX = (1 << 1),
  FCM_LIMIT_YMIN = (1 << 2),
  FCM_LIMIT_YMAX = (1 << 3),
} eFMod_Limit_Flags;

/* modification modes */
typedef enum eFMod_Noise_Modifications {
  /** Modify existing curve, matching its shape. */
  FCM_NOISE_MODIF_REPLACE = 0,
  /** Add noise to the curve. */
  FCM_NOISE_MODIF_ADD,
  /** Subtract noise from the curve. */
  FCM_NOISE_MODIF_SUBTRACT,
  /** Multiply the curve by noise. */
  FCM_NOISE_MODIF_MULTIPLY,
} eFMod_Noise_Modifications;

/* stepped modifier range flags */
typedef enum eFMod_Stepped_Flags {
  /** Don't affect frames before the start frame. */
  FCM_STEPPED_NO_BEFORE = (1 << 0),
  /** Don't affect frames after the end frame. */
  FCM_STEPPED_NO_AFTER = (1 << 1),
} eFMod_Stepped_Flags;

/* Drivers -------------------------------------- */

/** Driver Target options. */
typedef enum eDriverTarget_Options {
  /** Use the fallback value when the target is invalid (rna_path cannot be resolved). */
  DTAR_OPTION_USE_FALLBACK = (1 << 0),
} eDriverTarget_Options;

/** Driver Target flags. */
typedef enum eDriverTarget_Flag {
  /** used for targets that use the pchan_name instead of RNA path
   * (i.e. rotation difference) */
  DTAR_FLAG_STRUCT_REF = (1 << 0),
  /** The `idtype` can only be "Object". */
  DTAR_FLAG_ID_OB_ONLY = (1 << 1),

  /* "local-space" flags. */
  /** base flag - basically "pre parent+constraints" */
  DTAR_FLAG_LOCALSPACE = (1 << 2),
  /** include constraints transformed to space including parents */
  DTAR_FLAG_LOCAL_CONSTS = (1 << 3),

  /** error flags */
  DTAR_FLAG_INVALID = (1 << 4),

  /** the fallback value was actually used */
  DTAR_FLAG_FALLBACK_USED = (1 << 5),
} eDriverTarget_Flag;

/* Transform Channels for Driver Targets */
typedef enum eDriverTarget_TransformChannels {
  DTAR_TRANSCHAN_LOCX = 0,
  DTAR_TRANSCHAN_LOCY,
  DTAR_TRANSCHAN_LOCZ,
  DTAR_TRANSCHAN_ROTX,
  DTAR_TRANSCHAN_ROTY,
  DTAR_TRANSCHAN_ROTZ,
  DTAR_TRANSCHAN_SCALEX,
  DTAR_TRANSCHAN_SCALEY,
  DTAR_TRANSCHAN_SCALEZ,
  DTAR_TRANSCHAN_SCALE_AVG,
  DTAR_TRANSCHAN_ROTW,

  MAX_DTAR_TRANSCHAN_TYPES,
} eDriverTarget_TransformChannels;

/* Rotation channel mode for Driver Targets */
typedef enum eDriverTarget_RotationMode {
  /** Automatic euler mode. */
  DTAR_ROTMODE_AUTO = 0,

  /** Explicit euler rotation modes - must sync with BLI_math_rotation.h defines. */
  DTAR_ROTMODE_EULER_XYZ = 1,
  DTAR_ROTMODE_EULER_XZY,
  DTAR_ROTMODE_EULER_YXZ,
  DTAR_ROTMODE_EULER_YZX,
  DTAR_ROTMODE_EULER_ZXY,
  DTAR_ROTMODE_EULER_ZYX,

  DTAR_ROTMODE_QUATERNION,

  /**
   * Implements the very common Damped Track + child trick to decompose
   * rotation into bending followed by twist around the remaining axis.
   */
  DTAR_ROTMODE_SWING_TWIST_X,
  DTAR_ROTMODE_SWING_TWIST_Y,
  DTAR_ROTMODE_SWING_TWIST_Z,

  DTAR_ROTMODE_EULER_MIN = DTAR_ROTMODE_EULER_XYZ,
  DTAR_ROTMODE_EULER_MAX = DTAR_ROTMODE_EULER_ZYX,
} eDriverTarget_RotationMode;

typedef enum eDriverTarget_ContextProperty {
  DTAR_CONTEXT_PROPERTY_ACTIVE_SCENE = 0,
  DTAR_CONTEXT_PROPERTY_ACTIVE_VIEW_LAYER = 1,
} eDriverTarget_ContextProperty;

/* --- */

/* maximum number of driver targets per variable */
#define MAX_DRIVER_TARGETS 8

/** Driver Variable Types.* */
typedef enum eDriverVar_Types {
  /** single RNA property */
  DVAR_TYPE_SINGLE_PROP = 0,
  /** rotation difference (between 2 bones) */
  DVAR_TYPE_ROT_DIFF,
  /** distance between objects/bones */
  DVAR_TYPE_LOC_DIFF,
  /** 'final' transform for object/bones */
  DVAR_TYPE_TRANSFORM_CHAN,
  /** Property within a current evaluation context */
  DVAR_TYPE_CONTEXT_PROP,

  /**
   * Maximum number of variable types.
   *
   * \note This must always be the last item in this list,
   * so add new types above this line.
   */
  MAX_DVAR_TYPES,
} eDriverVar_Types;

/* Driver Variable Flags */
typedef enum eDriverVar_Flags {
  /* variable is not set up correctly */
  DVAR_FLAG_ERROR = (1 << 0),

  /* variable name doesn't pass the validation tests */
  DVAR_FLAG_INVALID_NAME = (1 << 1),
  /* name starts with a number */
  DVAR_FLAG_INVALID_START_NUM = (1 << 2),
  /* name starts with a special character (!, $, @, #, _, etc.) */
  DVAR_FLAG_INVALID_START_CHAR = (1 << 3),
  /* name contains a space */
  DVAR_FLAG_INVALID_HAS_SPACE = (1 << 4),
  /* name contains a dot */
  DVAR_FLAG_INVALID_HAS_DOT = (1 << 5),
  /* name contains invalid chars */
  DVAR_FLAG_INVALID_HAS_SPECIAL = (1 << 6),
  /* name is a reserved keyword */
  DVAR_FLAG_INVALID_PY_KEYWORD = (1 << 7),
  /* name is zero-length */
  DVAR_FLAG_INVALID_EMPTY = (1 << 8),
} eDriverVar_Flags;

/** All invalid `dvar` name flags. */
#define DVAR_ALL_INVALID_FLAGS \
  (DVAR_FLAG_INVALID_NAME | DVAR_FLAG_INVALID_START_NUM | DVAR_FLAG_INVALID_START_CHAR | \
   DVAR_FLAG_INVALID_HAS_SPACE | DVAR_FLAG_INVALID_HAS_DOT | DVAR_FLAG_INVALID_HAS_SPECIAL | \
   DVAR_FLAG_INVALID_PY_KEYWORD | DVAR_FLAG_INVALID_EMPTY)

/* --- */

/** Driver type. */
typedef enum eDriver_Types {
  /** target values are averaged together. */
  DRIVER_TYPE_AVERAGE = 0,
  /** python expression/function relates targets. */
  DRIVER_TYPE_PYTHON,
  /** sum of all values. */
  DRIVER_TYPE_SUM,
  /** smallest value. */
  DRIVER_TYPE_MIN,
  /** largest value. */
  DRIVER_TYPE_MAX,
} eDriver_Types;

/** Driver flags. */
typedef enum eDriver_Flags {
  /** Driver has invalid settings (internal flag). */
  DRIVER_FLAG_INVALID = (1 << 0),
  DRIVER_FLAG_DEPRECATED = (1 << 1),
  /** Driver does replace value, but overrides (for layering of animation over driver) */
  /* TODO: this needs to be implemented at some stage or left out... */
  // DRIVER_FLAG_LAYERING  = (1 << 2),
  /** Use when the expression needs to be recompiled. */
  DRIVER_FLAG_RECOMPILE = (1 << 3),
  /** The names are cached so they don't need have python unicode versions created each time */
  DRIVER_FLAG_RENAMEVAR = (1 << 4),
  /* Set if the driver cannot run because it uses Python which isn't allowed to execute. */
  DRIVER_FLAG_PYTHON_BLOCKED = (1 << 5),
  /** Include 'self' in the drivers namespace. */
  DRIVER_FLAG_USE_SELF = (1 << 6),
} eDriver_Flags;

/* F-Curves -------------------------------------- */

/** When #active_keyframe_index is set to this, the FCurve does not have an active keyframe. */
#define FCURVE_ACTIVE_KEYFRAME_NONE -1

/* user-editable flags/settings */
typedef enum eFCurve_Flags {
  /** Curve/keyframes are visible in editor */
  FCURVE_VISIBLE = (1 << 0),
  /** Curve is selected for editing. */
  FCURVE_SELECTED = (1 << 1),
  /** Curve is active one. */
  FCURVE_ACTIVE = (1 << 2),
  /** Keyframes (beztriples) cannot be edited. */
  FCURVE_PROTECTED = (1 << 3),
  /** FCurve will not be evaluated for the next round. */
  FCURVE_MUTED = (1 << 4),

#ifdef DNA_DEPRECATED_ALLOW
  /** fcurve uses 'auto-handles', which stay horizontal... */
  FCURVE_AUTO_HANDLES = (1 << 5), /* Dirty. */
#endif
  FCURVE_MOD_OFF = (1 << 6),
  /** skip evaluation, as RNA-path cannot be resolved
   * (similar to muting, but cannot be set by user) */
  FCURVE_DISABLED = (1 << 10),
  /** curve can only have whole-number values (integer types) */
  FCURVE_INT_VALUES = (1 << 11),
  /** curve can only have certain discrete-number values
   * (no interpolation at all, for enums/booleans) */
  FCURVE_DISCRETE_VALUES = (1 << 12),

  /** temporary tag for editing */
  FCURVE_TAGGED = (1 << 15),
} eFCurve_Flags;
ENUM_OPERATORS(eFCurve_Flags);

/* extrapolation modes (only simple value 'extending') */
typedef enum eFCurve_Extend {
  /** Just extend min/max keyframe value. */
  FCURVE_EXTRAPOLATE_CONSTANT = 0,
  /** Just extend gradient of segment between first segment keyframes. */
  FCURVE_EXTRAPOLATE_LINEAR,
} eFCurve_Extend;

/* curve coloring modes */
typedef enum eFCurve_Coloring {
  /** Automatically determine color using rainbow (calculated at draw-time). */
  FCURVE_COLOR_AUTO_RAINBOW = 0,
  /** Automatically determine color using XYZ (array index) <-> RGB. */
  FCURVE_COLOR_AUTO_RGB = 1,
  /** Automatically determine color where XYZ <-> RGB, but index(X) != 0. */
  FCURVE_COLOR_AUTO_YRGB = 3,
  /** Custom color. */
  FCURVE_COLOR_CUSTOM = 2,
} eFCurve_Coloring;

/* curve smoothing modes */
typedef enum eFCurve_Smoothing {
  /** legacy mode: auto handles only consider adjacent points */
  FCURVE_SMOOTH_NONE = 0,
  /** maintain continuity of the acceleration */
  FCURVE_SMOOTH_CONT_ACCEL = 1,
} eFCurve_Smoothing;

/* ************************************************ */
/* 'Action' Data-types */

/* NOTE: Although these are part of the Animation System,
 * they are not stored here, see `DNA_action_types.h` instead. */

/* ************************************************ */
/* NLA - Non-Linear Animation */

/* NLA Strips ------------------------------------- */

/* NLA Strip Blending Mode */
typedef enum eNlaStrip_Blend_Mode {
  NLASTRIP_MODE_REPLACE = 0,
  NLASTRIP_MODE_ADD,
  NLASTRIP_MODE_SUBTRACT,
  NLASTRIP_MODE_MULTIPLY,
  NLASTRIP_MODE_COMBINE,
} eNlaStrip_Blend_Mode;

/** NLA Strip Extrapolation Mode. */
typedef enum eNlaStrip_Extrapolate_Mode {
  /* extend before first frame if no previous strips in track,
   * and always hold+extend last frame */
  NLASTRIP_EXTEND_HOLD = 0,
  /* only hold+extend last frame */
  NLASTRIP_EXTEND_HOLD_FORWARD = 1,
  /* don't contribute at all */
  NLASTRIP_EXTEND_NOTHING = 2,
} eNlaStrip_Extrapolate_Mode;

/** NLA Strip Settings. */
typedef enum eNlaStrip_Flag {
  /* UI selection flags */
  /** NLA strip is the active one in the track (also indicates if strip is being tweaked) */
  NLASTRIP_FLAG_ACTIVE = (1 << 0),
  /* NLA strip is selected for editing */
  NLASTRIP_FLAG_SELECT = (1 << 1),
  // NLASTRIP_FLAG_SELECT_L      = (1 << 2),   /* left handle selected. */
  // NLASTRIP_FLAG_SELECT_R      = (1 << 3),   /* right handle selected. */

  /**
   * NLA strip uses the same action that the action being tweaked uses
   * (not set for the tweaking one though).
   */
  NLASTRIP_FLAG_TWEAKUSER = (1 << 4),

  /* controls driven by local F-Curves */
  /** strip influence is controlled by local F-Curve */
  NLASTRIP_FLAG_USR_INFLUENCE = (1 << 5),
  NLASTRIP_FLAG_USR_TIME = (1 << 6),
  NLASTRIP_FLAG_USR_TIME_CYCLIC = (1 << 7),

  /** NLA strip length is synced to the length of the referenced action */
  NLASTRIP_FLAG_SYNC_LENGTH = (1 << 9),

  /* playback flags (may be overridden by F-Curves) */
  /** NLA strip blend-in/out values are set automatically based on overlaps */
  NLASTRIP_FLAG_AUTO_BLENDS = (1 << 10),
  /** NLA strip is played back in reverse order */
  NLASTRIP_FLAG_REVERSE = (1 << 11),
  /** NLA strip is muted (i.e. doesn't contribute in any way) */
  NLASTRIP_FLAG_MUTED = (1 << 12),
  /** NLA Strip is played back in 'ping-pong' style */
  /* NLASTRIP_FLAG_MIRROR = (1 << 13), */ /* UNUSED */

  /* temporary editing flags */

  /**
   * When transforming strips, this flag is set when the strip is placed in an invalid location
   * such as overlapping another strip or moved to a locked track. In such cases, the strip's
   * location must be corrected after the transform operator is done.
   */
  NLASTRIP_FLAG_INVALID_LOCATION = (1 << 28),
  /** NLA strip should ignore frame range and hold settings, and evaluate at global time. */
  NLASTRIP_FLAG_NO_TIME_MAP = (1 << 29),
  /** NLA-Strip is really just a temporary meta used to facilitate easier transform code */
  NLASTRIP_FLAG_TEMP_META = (1 << 30),
  NLASTRIP_FLAG_EDIT_TOUCHED = (1u << 31),
} eNlaStrip_Flag;

/* NLA Strip Type */
typedef enum eNlaStrip_Type {
  /* 'clip' - references an Action */
  NLASTRIP_TYPE_CLIP = 0,
  /* 'transition' - blends between the adjacent strips */
  NLASTRIP_TYPE_TRANSITION,
  /* 'meta' - a strip which acts as a container for a few others */
  NLASTRIP_TYPE_META,

  /* 'emit sound' - a strip which is used for timing when speaker emits sounds */
  NLASTRIP_TYPE_SOUND,
} eNlaStrip_Type;

/* NLA Tracks ------------------------------------- */

/* settings for track */
typedef enum eNlaTrack_Flag {
  /** track is the one that settings can be modified on,
   * also indicates if track is being 'tweaked' */
  NLATRACK_ACTIVE = (1 << 0),
  /** track is selected in UI for relevant editing operations */
  NLATRACK_SELECTED = (1 << 1),
  /** track is not evaluated */
  NLATRACK_MUTED = (1 << 2),
  /** track is the only one evaluated (must be used in conjunction with adt->flag) */
  NLATRACK_SOLO = (1 << 3),
  /** track's settings (and strips) cannot be edited (to guard against unwanted changes) */
  NLATRACK_PROTECTED = (1 << 4),

  /** track is not allowed to execute,
   * usually as result of tweaking being enabled (internal flag) */
  NLATRACK_DISABLED = (1 << 10),

  /** Marks tracks automatically added for space while dragging strips vertically.
   * Internal flag that's only set during transform operator. */
  NLATRACK_TEMPORARILY_ADDED = (1 << 11),

  /** This NLA track is added to an override ID, which means it is fully editable.
   * Irrelevant in case the owner ID is not an override. */
  NLATRACK_OVERRIDELIBRARY_LOCAL = 1 << 16,
} eNlaTrack_Flag;

/* ************************************ */
/* KeyingSet Data-types */

/* KeyingSet settings */
typedef enum eKS_Settings {
  /** Keyingset cannot be removed (and doesn't need to be freed). */
  /* KEYINGSET_BUILTIN = (1 << 0), */ /* UNUSED */
  /** Keyingset does not depend on context info (i.e. paths are absolute). */
  KEYINGSET_ABSOLUTE = (1 << 1),
} eKS_Settings;
ENUM_OPERATORS(eKS_Settings)

/* Flags for use by keyframe creation/deletion calls */
typedef enum eInsertKeyFlags {
  INSERTKEY_NOFLAGS = 0,
  /** Only insert keyframes where they're needed. */
  INSERTKEY_NEEDED = (1 << 0),
  /** Insert "visual" keyframes where possible/needed. */
  INSERTKEY_MATRIX = (1 << 1),
  /** Don't recalculate handles,etc. after adding key. */
  INSERTKEY_FAST = (1 << 2),
  /** Don't re-allocate memory (or increase count, as array has already been set out). */
  /* INSERTKEY_FASTR = (1 << 3), */ /* UNUSED */
  /** Only replace an existing keyframe (this overrides #INSERTKEY_NEEDED). */
  INSERTKEY_REPLACE = (1 << 4),
  /** Ignore user-preferences (needed for predictable API use). */
  INSERTKEY_NO_USERPREF = (1 << 6),
  /**
   * Allow to make a full copy of new key into existing one, if any,
   * instead of 'reusing' existing handles.
   * Used by copy/paste code.
   */
  INSERTKEY_OVERWRITE_FULL = (1 << 7),
  /** For cyclic FCurves, adjust key timing to preserve the cycle period and flow. */
  INSERTKEY_CYCLE_AWARE = (1 << 9),
  /** Don't create new F-Curves (implied by #INSERTKEY_REPLACE). */
  INSERTKEY_AVAILABLE = (1 << 10),
} eInsertKeyFlags;
ENUM_OPERATORS(eInsertKeyFlags);

/* KS_Path->flag */
typedef enum eKSP_Settings {
  /* entire array (not just the specified index) gets keyframed */
  KSP_FLAG_WHOLE_ARRAY = (1 << 0),
} eKSP_Settings;

/* KS_Path->groupmode */
typedef enum eKSP_Grouping {
  /** Path should be grouped using group name stored in path. */
  KSP_GROUP_NAMED = 0,
  /** Path should not be grouped at all. */
  KSP_GROUP_NONE,
  /** Path should be grouped using KeyingSet's name. */
  KSP_GROUP_KSNAME,
  /** Path should be grouped using name of inner-most context item from templates
   * - this is most useful for relative KeyingSets only. */
  /* KSP_GROUP_TEMPLATE_ITEM, */ /* UNUSED */
} eKSP_Grouping;

/* ************************************************ */
/* Animation Data */

/* AnimData ------------------------------------- */

/* Animation Data settings (mostly for NLA) */
typedef enum eAnimData_Flag {
  /** Only evaluate a single track in the NLA. */
  ADT_NLA_SOLO_TRACK = (1 << 0),
  /** Don't use NLA */
  ADT_NLA_EVAL_OFF = (1 << 1),
  /** NLA is being 'tweaked' (i.e. in EditMode). */
  ADT_NLA_EDIT_ON = (1 << 2),
  /** Active Action for 'tweaking' does not have mapping applied for editing. */
  ADT_NLA_EDIT_NOMAP = (1 << 3),
  /** NLA-Strip F-Curves are expanded in UI. */
  ADT_NLA_SKEYS_COLLAPSED = (1 << 4),
  /* Evaluate tracks above tweaked strip. Only relevant in tweak mode. */
  ADT_NLA_EVAL_UPPER_TRACKS = (1 << 5),

  /** Drivers expanded in UI. */
  ADT_DRIVERS_COLLAPSED = (1 << 10),
  /** Don't execute drivers. */
  /* ADT_DRIVERS_DISABLED = (1 << 11), */ /* UNUSED */

  /** AnimData block is selected in UI. */
  ADT_UI_SELECTED = (1 << 14),
  /** AnimData block is active in UI. */
  ADT_UI_ACTIVE = (1 << 15),

  /** F-Curves from this AnimData block are not visible in the Graph Editor. */
  ADT_CURVES_NOT_VISIBLE = (1 << 16),

  /** F-Curves from this AnimData block are always visible. */
  ADT_CURVES_ALWAYS_VISIBLE = (1 << 17),

  /** Animation pointer to by this AnimData block is expanded in UI. This is stored on the AnimData
   * so that each user of the Animation can have its own expansion/contraction state. */
  ADT_UI_EXPANDED = (1 << 18),
} eAnimData_Flag;

/* From: `DNA_object_types.h`, see its doc-string there. */
#define SELECT 1
