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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_ANIM_TYPES_H__
#define __DNA_ANIM_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"

/* ************************************************ */
/* F-Curve DataTypes */

/* Modifiers -------------------------------------- */

/**
 * F-Curve Modifiers (fcm)
 *
 * These alter the way F-Curves behave, by altering the value that is returned
 * when evaluating the curve's data at some time (t).
 */
typedef struct FModifier {
  struct FModifier *next, *prev;

  /** Containing curve, only used for updates to CYCLES. */
  struct FCurve *curve;
  /** Pointer to modifier data. */
  void *data;

  /** User-defined description for the modifier - MAX_ID_NAME-2. */
  char name[64];
  /** Type of f-curve modifier. */
  short type;
  /** Settings for the modifier. */
  short flag;

  /** The amount that the modifier should influence the value. */
  float influence;

  /** Start frame of restricted frame-range. */
  float sfra;
  /** End frame of restricted frame-range. */
  float efra;
  /** Number of frames from sfra before modifier takes full influence. */
  float blendin;
  /** Number of frames from efra before modifier fades out. */
  float blendout;
} FModifier;

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
  /** Unimplemented - for applying: fft, high/low pass filters, etc. */
  FMODIFIER_TYPE_FILTER = 6,
  FMODIFIER_TYPE_PYTHON = 7,
  FMODIFIER_TYPE_LIMITS = 8,
  FMODIFIER_TYPE_STEPPED = 9,

  /* NOTE: all new modifiers must be added above this line */
  FMODIFIER_NUM_TYPES,
} eFModifier_Types;

/** F-Curve Modifier Settings. */
typedef enum eFModifier_Flags {
  /** Modifier is not able to be evaluated for some reason, and should be skipped (internal). */
  FMODIFIER_FLAG_DISABLED = (1 << 0),
  /** Modifier's data is expanded (in UI). */
  FMODIFIER_FLAG_EXPANDED = (1 << 1),
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

/* Generator modifier data */
typedef struct FMod_Generator {
  /* general generator information */
  /** Coefficients array. */
  float *coefficients;
  /** Size of the coefficients array. */
  unsigned int arraysize;

  /** Order of polynomial generated (i.e. 1 for linear, 2 for quadratic). */
  int poly_order;
  /** Which 'generator' to use eFMod_Generator_Modes. */
  int mode;

  /** Settings. */
  int flag;
} FMod_Generator;

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

/**
 * 'Built-In Function' Generator modifier data
 *
 * This uses the general equation for equations:
 * y = amplitude * fn(phase_multiplier*x + phase_offset) + y_offset
 *
 * where amplitude, phase_multiplier/offset, y_offset are user-defined coefficients,
 * x is the evaluation 'time', and 'y' is the resultant value
 */
typedef struct FMod_FunctionGenerator {
  /** Coefficients for general equation (as above). */
  float amplitude;
  float phase_multiplier;
  float phase_offset;
  float value_offset;

  /* flags */
  /** #eFMod_Generator_Functions. */
  int type;
  /** #eFMod_Generator_flags. */
  int flag;
} FMod_FunctionGenerator;

/* 'function' generator types */
typedef enum eFMod_Generator_Functions {
  FCM_GENERATOR_FN_SIN = 0,
  FCM_GENERATOR_FN_COS = 1,
  FCM_GENERATOR_FN_TAN = 2,
  FCM_GENERATOR_FN_SQRT = 3,
  FCM_GENERATOR_FN_LN = 4,
  FCM_GENERATOR_FN_SINC = 5,
} eFMod_Generator_Functions;

/* envelope modifier - envelope data */
typedef struct FCM_EnvelopeData {
  /** Min/max values for envelope at this point (absolute values) . */
  float min, max;
  /** Time for that this sample-point occurs. */
  float time;

  /** Settings for 'min' control point. */
  short f1;
  /** Settings for 'max' control point. */
  short f2;
} FCM_EnvelopeData;

/* envelope-like adjustment to values (for fade in/out) */
typedef struct FMod_Envelope {
  /** Data-points defining envelope to apply (array) . */
  FCM_EnvelopeData *data;
  /** Number of envelope points. */
  int totvert;

  /** Value that envelope's influence is centered around / based on. */
  float midval;
  /** Distances from 'middle-value' for 1:1 envelope influence. */
  float min, max;
} FMod_Envelope;

/* cycling/repetition modifier data */
// TODO: we can only do complete cycles...
typedef struct FMod_Cycles {
  /** Extrapolation mode to use before first keyframe. */
  short before_mode;
  /** Extrapolation mode to use after last keyframe. */
  short after_mode;
  /** Number of 'cycles' before first keyframe to do. */
  short before_cycles;
  /** Number of 'cycles' after last keyframe to do. */
  short after_cycles;
} FMod_Cycles;

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

/* Python-script modifier data */
typedef struct FMod_Python {
  /** Text buffer containing script to execute. */
  struct Text *script;
  /** ID-properties to provide 'custom' settings. */
  IDProperty *prop;
} FMod_Python;

/* limits modifier data */
typedef struct FMod_Limits {
  /** Rect defining the min/max values. */
  rctf rect;
  /** Settings for limiting. */
  int flag;
  char _pad[4];
} FMod_Limits;

/* limiting flags */
typedef enum eFMod_Limit_Flags {
  FCM_LIMIT_XMIN = (1 << 0),
  FCM_LIMIT_XMAX = (1 << 1),
  FCM_LIMIT_YMIN = (1 << 2),
  FCM_LIMIT_YMAX = (1 << 3),
} eFMod_Limit_Flags;

/* noise modifier data */
typedef struct FMod_Noise {
  float size;
  float strength;
  float phase;
  float offset;

  short depth;
  short modification;
} FMod_Noise;

/* modification modes */
typedef enum eFMod_Noise_Modifications {
  /** Modify existing curve, matching it's shape. */
  FCM_NOISE_MODIF_REPLACE = 0,
  /** Add noise to the curve. */
  FCM_NOISE_MODIF_ADD,
  /** Subtract noise from the curve. */
  FCM_NOISE_MODIF_SUBTRACT,
  /** Multiply the curve by noise. */
  FCM_NOISE_MODIF_MULTIPLY,
} eFMod_Noise_Modifications;

/* stepped modifier data */
typedef struct FMod_Stepped {
  /** Number of frames each interpolated value should be held. */
  float step_size;
  /** Reference frame number that stepping starts from. */
  float offset;

  /** Start frame of the frame range that modifier works in. */
  float start_frame;
  /** End frame of the frame range that modifier works in. */
  float end_frame;

  /** Various settings. */
  int flag;
} FMod_Stepped;

/* stepped modifier range flags */
typedef enum eFMod_Stepped_Flags {
  /** Don't affect frames before the start frame. */
  FCM_STEPPED_NO_BEFORE = (1 << 0),
  /** Don't affect frames after the end frame. */
  FCM_STEPPED_NO_AFTER = (1 << 1),
} eFMod_Stepped_Flags;

/* Drivers -------------------------------------- */

/* Driver Target (dtar)
 *
 * Defines how to access a dependency needed for a driver variable.
 */
typedef struct DriverTarget {
  /** ID-block which owns the target, no user count. */
  ID *id;

  /** RNA path defining the setting to use (for DVAR_TYPE_SINGLE_PROP). */
  char *rna_path;

  /**
   * Name of the posebone to use
   * (for vars where DTAR_FLAG_STRUCT_REF is used) - MAX_ID_NAME-2.
   */
  char pchan_name[64];
  /** Transform channel index (for DVAR_TYPE_TRANSFORM_CHAN.)*/
  short transChan;

  /**
   * Flags for the validity of the target
   * (NOTE: these get reset every time the types change).
   */
  short flag;
  /** Type of ID-block that this target can use. */
  int idtype;
} DriverTarget;

/** Driver Target flags. */
typedef enum eDriverTarget_Flag {
  /** used for targets that use the pchan_name instead of RNA path
   * (i.e. rotation difference) */
  DTAR_FLAG_STRUCT_REF = (1 << 0),
  /** idtype can only be 'Object' */
  DTAR_FLAG_ID_OB_ONLY = (1 << 1),

  /* "localspace" flags */
  /** base flag - basically "pre parent+constraints" */
  DTAR_FLAG_LOCALSPACE = (1 << 2),
  /** include constraints transformed to space including parents */
  DTAR_FLAG_LOCAL_CONSTS = (1 << 3),

  /** error flags */
  DTAR_FLAG_INVALID = (1 << 4),
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

  MAX_DTAR_TRANSCHAN_TYPES,
} eDriverTarget_TransformChannels;

/* --- */

/* maximum number of driver targets per variable */
#define MAX_DRIVER_TARGETS 8

/**
 * Driver Variable (dvar)
 *
 * A 'variable' for use as an input for the driver evaluation.
 * Defines a way of accessing some channel to use, that can be
 * referred to in the expression as a variable, thus simplifying
 * expressions and also Depsgraph building.
 */
typedef struct DriverVar {
  struct DriverVar *next, *prev;

  /**
   * Name of the variable to use in py-expression
   * (must be valid python identifier) - MAX_ID_NAME-2.
   */
  char name[64];

  /** MAX_DRIVER_TARGETS, target slots. */
  DriverTarget targets[8];

  /** Number of targets actually used by this variable. */
  char num_targets;
  /** Type of driver variable (eDriverVar_Types). */
  char type;

  /** Validation tags, etc. (eDriverVar_Flags). */
  short flag;
  /** Result of previous evaluation. */
  float curval;
} DriverVar;

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

  /** Maximum number of variable types.
   *
   * \note This must always be th last item in this list,
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

/* All invalid dvar name flags */
#define DVAR_ALL_INVALID_FLAGS \
  (DVAR_FLAG_INVALID_NAME | DVAR_FLAG_INVALID_START_NUM | DVAR_FLAG_INVALID_START_CHAR | \
   DVAR_FLAG_INVALID_HAS_SPACE | DVAR_FLAG_INVALID_HAS_DOT | DVAR_FLAG_INVALID_HAS_SPECIAL | \
   DVAR_FLAG_INVALID_PY_KEYWORD | DVAR_FLAG_INVALID_EMPTY)

/* --- */

/**
 * Channel Driver (i.e. Drivers / Expressions) (driver)
 *
 * Channel Drivers are part of the dependency system, and are executed in addition to
 * normal user-defined animation. They take the animation result of some channel(s), and
 * use that (optionally combined with its own F-Curve for modification of results) to define
 * the value of some setting semi-procedurally.
 *
 * Drivers are stored as part of F-Curve data, so that the F-Curve's RNA-path settings (for storing
 * what setting the driver will affect). The order in which they are stored defines the order that
 * they're evaluated in. This order is set by the Depsgraph's sorting stuff.
 */
typedef struct ChannelDriver {
  /** Targets for this driver (i.e. list of DriverVar). */
  ListBase variables;

  /* python expression to execute (may call functions defined in an accessory file)
   * which relates the target 'variables' in some way to yield a single usable value
   */
  /** Expression to compile for evaluation. */
  char expression[256];
  /** PyObject - compiled expression, don't save this. */
  void *expr_comp;

  /** Compiled simple arithmetic expression. */
  struct ExprPyLike_Parsed *expr_simple;

  /** Result of previous evaluation. */
  float curval;
  // XXX to be implemented... this is like the constraint influence setting
  /** Influence of driver on result. */
  float influence;

  /* general settings */
  /** Type of driver. */
  int type;
  /** Settings of driver. */
  int flag;
} ChannelDriver;

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
  /** Driver has invalid settings (internal flag)  */
  DRIVER_FLAG_INVALID = (1 << 0),
  DRIVER_FLAG_DEPRECATED = (1 << 1),
  /** Driver does replace value, but overrides (for layering of animation over driver) */
  // TODO: this needs to be implemented at some stage or left out...
  // DRIVER_FLAG_LAYERING  = (1 << 2),
  /** Use when the expression needs to be recompiled. */
  DRIVER_FLAG_RECOMPILE = (1 << 3),
  /** The names are cached so they don't need have python unicode versions created each time */
  DRIVER_FLAG_RENAMEVAR = (1 << 4),
  // DRIVER_FLAG_UNUSED_5 = (1 << 5),
  /** Include 'self' in the drivers namespace. */
  DRIVER_FLAG_USE_SELF = (1 << 6),
} eDriver_Flags;

/* F-Curves -------------------------------------- */

/**
 * FPoint (fpt)
 *
 * This is the bare-minimum data required storing motion samples. Should be more efficient
 * than using BPoints, which contain a lot of other unnecessary data...
 */
typedef struct FPoint {
  /** Time + value. */
  float vec[2];
  /** Selection info. */
  int flag;
  char _pad[4];
} FPoint;

/* 'Function-Curve' - defines values over time for a given setting (fcu) */
typedef struct FCurve {
  struct FCurve *next, *prev;

  /* group */
  /** Group that F-Curve belongs to. */
  bActionGroup *grp;

  /* driver settings */
  /** Only valid for drivers (i.e. stored in AnimData not Actions). */
  ChannelDriver *driver;
  /* evaluation settings */
  /** FCurve Modifiers. */
  ListBase modifiers;

  /* motion data */
  /** User-editable keyframes (array). */
  BezTriple *bezt;
  /** 'baked/imported' motion samples (array). */
  FPoint *fpt;
  /** Total number of points which define the curve (i.e. size of arrays in FPoints). */
  unsigned int totvert;

  /* value cache + settings */
  /** Value stored from last time curve was evaluated (not threadsafe, debug display only!). */
  float curval;
  char _pad2[4];
  /** User-editable settings for this curve. */
  short flag;
  /** Value-extending mode for this curve (does not cover). */
  short extend;
  /** Auto-handle smoothing mode. */
  char auto_smoothing;

  char _pad[3];

  /* RNA - data link */
  /** If applicable, the index of the RNA-array item to get. */
  int array_index;
  /** RNA-path to resolve data-access. */
  char *rna_path;

  /* curve coloring (for editor) */
  /** Coloring method to use (eFCurve_Coloring). */
  int color_mode;
  /** The last-color this curve took. */
  float color[3];

  float prev_norm_factor, prev_offset;
} FCurve;

/* user-editable flags/settings */
typedef enum eFCurve_Flags {
  /** curve/keyframes are visible in editor */
  FCURVE_VISIBLE = (1 << 0),
  /** curve is selected for editing  */
  FCURVE_SELECTED = (1 << 1),
  /** curve is active one */
  FCURVE_ACTIVE = (1 << 2),
  /** keyframes (beztriples) cannot be edited */
  FCURVE_PROTECTED = (1 << 3),
  /** fcurve will not be evaluated for the next round */
  FCURVE_MUTED = (1 << 4),

  /** fcurve uses 'auto-handles', which stay horizontal... */
  // DEPRECATED
  FCURVE_AUTO_HANDLES = (1 << 5),
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

/* extrapolation modes (only simple value 'extending') */
typedef enum eFCurve_Extend {
  /** just extend min/max keyframe value  */
  FCURVE_EXTRAPOLATE_CONSTANT = 0,
  /** just extend gradient of segment between first segment keyframes */
  FCURVE_EXTRAPOLATE_LINEAR,
} eFCurve_Extend;

/* curve coloring modes */
typedef enum eFCurve_Coloring {
  /** automatically determine color using rainbow (calculated at drawtime) */
  FCURVE_COLOR_AUTO_RAINBOW = 0,
  /** automatically determine color using XYZ (array index) <-> RGB */
  FCURVE_COLOR_AUTO_RGB = 1,
  /** automatically determine color where XYZ <-> RGB, but index(X) != 0 */
  FCURVE_COLOR_AUTO_YRGB = 3,
  /** custom color */
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
/* 'Action' Datatypes */

/* NOTE: Although these are part of the Animation System,
 * they are not stored here... see DNA_action_types.h instead
 */

/* ************************************************ */
/* NLA - Non-Linear Animation */

/* NLA Strips ------------------------------------- */

/**
 * NLA Strip (strip)
 *
 * A NLA Strip is a container for the reuse of Action data, defining parameters
 * to control the remapping of the Action data to some destination.
 */
typedef struct NlaStrip {
  struct NlaStrip *next, *prev;

  /** 'Child' strips (used for 'meta' strips). */
  ListBase strips;
  /** Action that is referenced by this strip (strip is 'user' of the action). */
  bAction *act;

  /** F-Curves for controlling this strip's influence and timing */  // TODO: move o.ut?
  ListBase fcurves;
  /** F-Curve modifiers to be applied to the entire strip's referenced F-Curves. */
  ListBase modifiers;

  /** User-Visible Identifier for Strip - MAX_ID_NAME-2. */
  char name[64];

  /** Influence of strip. */
  float influence;
  /** Current 'time' within action being used (automatically evaluated, but can be overridden). */
  float strip_time;

  /** Extents of the strip. */
  float start, end;
  /** Range of the action to use. */
  float actstart, actend;

  /** The number of times to repeat the action range (only when no F-Curves). */
  float repeat;
  /** The amount the action range is scaled by (only when no F-Curves). */
  float scale;

  /** Strip blending length (only used when there are no F-Curves). */
  float blendin, blendout;
  /** Strip blending mode (layer-based mixing). */
  short blendmode;

  /** Strip extrapolation mode (time-based mixing). */
  short extendmode;
  char _pad1[2];

  /** Type of NLA strip. */
  short type;

  /** Handle for speaker objects. */
  void *speaker_handle;

  /** Settings. */
  int flag;
  char _pad2[4];

  /* Pointer to an original NLA strip. */
  struct NlaStrip *orig_strip;

  void *_pad3;
} NlaStrip;

/* NLA Strip Blending Mode */
typedef enum eNlaStrip_Blend_Mode {
  NLASTRIP_MODE_REPLACE = 0,
  NLASTRIP_MODE_ADD,
  NLASTRIP_MODE_SUBTRACT,
  NLASTRIP_MODE_MULTIPLY,
  NLASTRIP_MODE_COMBINE,
} eNlaStrip_Blend_Mode;

/** NLA Strip Extrpolation Mode. */
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
  //  NLASTRIP_FLAG_SELECT_L      = (1 << 2),   // left handle selected
  //  NLASTRIP_FLAG_SELECT_R      = (1 << 3),   // right handle selected

  /** NLA strip uses the same action that the action being tweaked uses
   * (not set for the tweaking one though). */
  NLASTRIP_FLAG_TWEAKUSER = (1 << 4),

  /* controls driven by local F-Curves */
  /** strip influence is controlled by local F-Curve */
  NLASTRIP_FLAG_USR_INFLUENCE = (1 << 5),
  NLASTRIP_FLAG_USR_TIME = (1 << 6),
  NLASTRIP_FLAG_USR_TIME_CYCLIC = (1 << 7),

  /** NLA strip length is synced to the length of the referenced action */
  NLASTRIP_FLAG_SYNC_LENGTH = (1 << 9),

  /* playback flags (may be overridden by F-Curves) */
  /** NLA strip blendin/out values are set automatically based on overlaps */
  NLASTRIP_FLAG_AUTO_BLENDS = (1 << 10),
  /** NLA strip is played back in reverse order */
  NLASTRIP_FLAG_REVERSE = (1 << 11),
  /** NLA strip is muted (i.e. doesn't contribute in any way) */
  NLASTRIP_FLAG_MUTED = (1 << 12),
  /** NLA Strip is played back in 'ping-pong' style */
  NLASTRIP_FLAG_MIRROR = (1 << 13),

  /* temporary editing flags */
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

/**
 * NLA Track (nlt)
 *
 * A track groups a bunch of 'strips', which should form a continuous set of
 * motion, on top of which other such groups can be layered. This should allow
 * for animators to work in a non-destructive manner, layering tweaks, etc. over
 * 'rough' blocks of their work.
 */
typedef struct NlaTrack {
  struct NlaTrack *next, *prev;

  /** BActionStrips in this track. */
  ListBase strips;

  /** Settings for this track. */
  int flag;
  /** Index of the track in the stack
   * \note not really useful, but we need a '_pad' var anyways! */
  int index;

  /** Short user-description of this track - MAX_ID_NAME-2. */
  char name[64];
} NlaTrack;

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
} eNlaTrack_Flag;

/* ************************************ */
/* KeyingSet Datatypes */

/**
 * Path for use in KeyingSet definitions (ksp)
 *
 * Paths may be either specific (specifying the exact sub-ID
 * dynamic data-block - such as PoseChannels - to act upon, ala
 * Maya's 'Character Sets' and XSI's 'Marking Sets'), or they may
 * be generic (using various placeholder template tags that will be
 * replaced with appropriate information from the context).
 */
typedef struct KS_Path {
  struct KS_Path *next, *prev;

  /** ID block that keyframes are for. */
  ID *id;
  /** Name of the group to add to - MAX_ID_NAME-2. */
  char group[64];

  /** ID-type that path can be used on. */
  int idtype;

  /** Group naming (eKSP_Grouping). */
  short groupmode;
  /** Various settings, etc. */
  short flag;

  /** Dynamically (or statically in the case of predefined sets) path. */
  char *rna_path;
  /** Index that path affects. */
  int array_index;

  /** (eInsertKeyFlags) settings to supply insertkey() with. */
  short keyingflag;
  /** (eInsertKeyFlags) for each flag set, the relevant keyingflag bit overrides the default. */
  short keyingoverride;
} KS_Path;

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
  KSP_GROUP_TEMPLATE_ITEM,
} eKSP_Grouping;

/* ---------------- */

/**
 * KeyingSet definition (ks)
 *
 * A KeyingSet defines a group of properties that should
 * be keyframed together, providing a convenient way for animators
 * to insert keyframes without resorting to Auto-Keyframing.
 *
 * A few 'generic' (non-absolute and dependent on templates) KeyingSets
 * are defined 'built-in' to facilitate easy animating for the casual
 * animator without the need to add extra steps to the rigging process.
 */
typedef struct KeyingSet {
  struct KeyingSet *next, *prev;

  /** (KS_Path) paths to keyframe to. */
  ListBase paths;

  /** Unique name (for search, etc.) - MAX_ID_NAME-2 . */
  char idname[64];
  /** User-viewable name for KeyingSet (for menus, etc.) - MAX_ID_NAME-2. */
  char name[64];
  /** (RNA_DYN_DESCR_MAX) short help text. */
  char description[240];
  /** Name of the typeinfo data used for the relative paths - MAX_ID_NAME-2. */
  char typeinfo[64];

  /** Index of the active path. */
  int active_path;

  /** Settings for KeyingSet. */
  short flag;

  /** (eInsertKeyFlags) settings to supply insertkey() with. */
  short keyingflag;
  /** (eInsertKeyFlags) for each flag set, the relevant keyingflag bit overrides the default. */
  short keyingoverride;

  char _pad[6];
} KeyingSet;

/* KeyingSet settings */
typedef enum eKS_Settings {
  /** Keyingset cannot be removed (and doesn't need to be freed). */
  KEYINGSET_BUILTIN = (1 << 0),
  /** Keyingset does not depend on context info (i.e. paths are absolute). */
  KEYINGSET_ABSOLUTE = (1 << 1),
} eKS_Settings;

/* Flags for use by keyframe creation/deletion calls */
typedef enum eInsertKeyFlags {
  INSERTKEY_NOFLAGS = 0,
  /** only insert keyframes where they're needed */
  INSERTKEY_NEEDED = (1 << 0),
  /** insert 'visual' keyframes where possible/needed */
  INSERTKEY_MATRIX = (1 << 1),
  /** don't recalculate handles,etc. after adding key */
  INSERTKEY_FAST = (1 << 2),
  /** don't realloc mem (or increase count, as array has already been set out) */
  INSERTKEY_FASTR = (1 << 3),
  /** only replace an existing keyframe (this overrides INSERTKEY_NEEDED) */
  INSERTKEY_REPLACE = (1 << 4),
  /** transform F-Curves should have XYZ->RGB color mode */
  INSERTKEY_XYZ2RGB = (1 << 5),
  /** ignore user-prefs (needed for predictable API use) */
  INSERTKEY_NO_USERPREF = (1 << 6),
  /** Allow to make a full copy of new key into existing one, if any,
   * instead of 'reusing' existing handles.
   * Used by copy/paste code. */
  INSERTKEY_OVERWRITE_FULL = (1 << 7),
  /** for driver FCurves, use driver's "input" value - for easier corrective driver setup */
  INSERTKEY_DRIVER = (1 << 8),
  /** for cyclic FCurves, adjust key timing to preserve the cycle period and flow */
  INSERTKEY_CYCLE_AWARE = (1 << 9),
  /** don't create new F-Curves (implied by INSERTKEY_REPLACE) */
  INSERTKEY_AVAILABLE = (1 << 10),
} eInsertKeyFlags;

/* ************************************************ */
/* Animation Data */

/* AnimOverride ------------------------------------- */

/**
 * Animation Override (aor)
 *
 * This is used to as temporary storage of values which have been changed by the user, but not
 * yet keyframed (thus, would get overwritten by the animation system before the user had a chance
 * to see the changes that were made).
 *
 * It is probably not needed for overriding keyframed values in most cases, as those will only get
 * evaluated on frame-change now. That situation may change in future.
 */
typedef struct AnimOverride {
  struct AnimOverride *next, *prev;

  /** RNA-path to use to resolve data-access. */
  char *rna_path;
  /** If applicable, the index of the RNA-array item to get. */
  int array_index;

  /** Value to override setting with. */
  float value;
} AnimOverride;

/* AnimData ------------------------------------- */

/**
 * Animation data for some ID block (adt)
 *
 * This block of data is used to provide all of the necessary animation data for a data-block.
 * Currently, this data will not be reusable, as there shouldn't be any need to do so.
 *
 * This information should be made available for most if not all ID-blocks, which should
 * enable all of its settings to be animatable locally. Animation from 'higher-up' ID-AnimData
 * blocks may override local settings.
 *
 * This data-block should be placed immediately after the ID block where it is used, so that
 * the code which retrieves this data can do so in an easier manner.
 * See blenkernel/intern/anim_sys.c for details.
 */
typedef struct AnimData {
  /** active action - acts as the 'tweaking track' for the NLA */
  bAction *action;
  /** temp-storage for the 'real' active action (i.e. the one used before the tweaking-action
   * took over to be edited in the Animation Editors)
   */
  bAction *tmpact;

  /* nla-tracks */
  ListBase nla_tracks;
  /**
   * Active NLA-track
   * (only set/used during tweaking, so no need to worry about dangling pointers).
   */
  NlaTrack *act_track;
  /**
   * Active NLA-strip
   * (only set/used during tweaking, so no need to worry about dangling pointers).
   */
  NlaStrip *actstrip;

  /* 'drivers' for this ID-block's settings - FCurves, but are completely
   * separate from those for animation data
   */
  /** Standard user-created Drivers/Expressions (used as part of a rig). */
  ListBase drivers;
  /** Temp storage (AnimOverride) of values for settings that are animated
   * (but the value hasn't been keyframed). */
  ListBase overrides;

  /** Runtime data, for depsgraph evaluation. */
  FCurve **driver_array;

  /* settings for animation evaluation */
  /** User-defined settings. */
  int flag;
  char _pad[4];

  /* settings for active action evaluation (based on NLA strip settings) */
  /** Accumulation mode for active action. */
  short act_blendmode;
  /** Extrapolation mode for active action. */
  short act_extendmode;
  /** Influence for active action. */
  float act_influence;
} AnimData;

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

  /** Drivers expanded in UI. */
  ADT_DRIVERS_COLLAPSED = (1 << 10),
  /** Don't execute drivers. */
  ADT_DRIVERS_DISABLED = (1 << 11),

  /** AnimData block is selected in UI. */
  ADT_UI_SELECTED = (1 << 14),
  /** AnimData block is active in UI. */
  ADT_UI_ACTIVE = (1 << 15),

  /** F-Curves from this AnimData block are not visible in the Graph Editor. */
  ADT_CURVES_NOT_VISIBLE = (1 << 16),

  /** F-Curves from this AnimData block are always visible. */
  ADT_CURVES_ALWAYS_VISIBLE = (1 << 17),
} eAnimData_Flag;

/* Base Struct for Anim ------------------------------------- */

/**
 * Used for #BKE_animdata_from_id()
 * All ID-data-blocks which have their own 'local' AnimData
 * should have the same arrangement in their structs.
 */
typedef struct IdAdtTemplate {
  ID id;
  AnimData *adt;
} IdAdtTemplate;

/* ************************************************ */

#ifdef __cplusplus
};
#endif

#endif /* __DNA_ANIM_TYPES_H__ */
