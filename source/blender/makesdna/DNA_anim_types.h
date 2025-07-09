/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_anim_enums.h"
#include "DNA_curve_types.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
#  include "BLI_span.hh"

#  include <type_traits>
#endif

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

  /** User-defined description for the modifier. */
  char name[/*MAX_NAME*/ 64];
  /** Type of f-curve modifier. */
  short type;
  /** Settings for the modifier. */
  short flag;
  /**
   * Expansion state for the modifier panel and its sub-panels, stored as a bit-field
   * in depth-first order. (Maximum of `sizeof(short)` total panels).
   */
  short ui_expand_flag;

  char _pad[6];

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

/* envelope modifier - envelope data */
typedef struct FCM_EnvelopeData {
  /** Min/max values for envelope at this point (absolute values). */
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
  /** Data-points defining envelope to apply (array). */
  FCM_EnvelopeData *data;
  /** Number of envelope points. */
  int totvert;

  /** Value that envelope's influence is centered around / based on. */
  float midval;
  /** Distances from 'middle-value' for 1:1 envelope influence. */
  float min, max;
} FMod_Envelope;

/* cycling/repetition modifier data */
/* TODO: we can only do complete cycles. */
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

/* limits modifier data */
typedef struct FMod_Limits {
  /** Rect defining the min/max values. */
  rctf rect;
  /** Settings for limiting. */
  int flag;
  char _pad[4];
} FMod_Limits;

/* noise modifier data */
typedef struct FMod_Noise {
  float size;
  float strength;
  float phase;
  float offset;
  float roughness;
  float lacunarity;

  short depth;
  short modification;
  char legacy_noise;
  char _pad[3];
} FMod_Noise;

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

/* Drivers -------------------------------------- */

/**
 * Driver Target (`dtar`)
 *
 * Defines how to access a dependency needed for a driver variable.
 */
typedef struct DriverTarget {
  /** ID-block which owns the target, no user count. */
  ID *id;

  /** RNA path defining the setting to use (for DVAR_TYPE_SINGLE_PROP). */
  char *rna_path;

  /**
   * Name of the pose-bone to use
   * (for vars where DTAR_FLAG_STRUCT_REF is used).
   */
  char pchan_name[/*MAX_NAME*/ 64];
  /** Transform channel index (for #DVAR_TYPE_TRANSFORM_CHAN). */
  short transChan;

  /** Rotation channel calculation type. */
  char rotation_mode;
  char _pad[5];

  /**
   * Flags for the validity of the target
   * (NOTE: these get reset every time the types change).
   */
  short flag;
  /** Single-bit user-visible toggles (not reset on type change) from eDriverTarget_Options. */
  short options;
  /** Type of ID-block that this target can use. */
  int idtype;

  /* Context-dependent property of a "Context Property" type target.
   * The `rna_path` of this property is used as a target.
   * This is a value of enumerator #eDriverTarget_ContextProperty. */
  int context_property;

  /* Fall back value to use with DTAR_OPTION_USE_FALLBACK. */
  float fallback_value;
} DriverTarget;

/* --- */

/**
 * Driver Variable (`dvar`)
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
   * (must be valid python identifier).
   */
  char name[/*MAX_NAME*/ 64];

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
  /* XXX to be implemented... this is like the constraint influence setting. */
  /** Influence of driver on result. */
  float influence;

  /* general settings */
  /** Type of driver. */
  int type;
  /** Settings of driver. */
  int flag;
} ChannelDriver;

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

/** 'Function-Curve' - defines values over time for a given setting (fcu). */
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

  /**
   * Index of active keyframe in #bezt for numerical editing in the interface. A value of
   * #FCURVE_ACTIVE_KEYFRAME_NONE indicates that the FCurve has no active keyframe.
   *
   * Do not access directly, use #BKE_fcurve_active_keyframe_index() and
   * #BKE_fcurve_active_keyframe_set() instead.
   */
  int active_keyframe_index;

  /* value cache + settings */
  /** Value stored from last time curve was evaluated (not threadsafe, debug display only!). */
  float curval;
  /** User-editable settings for this curve. */
  short flag;
  /** Value-extending mode for this curve (does not cover). */
  short extend;
  /** Auto-handle smoothing mode. */
  char auto_smoothing;

  char _pad[3];

  /* RNA - data link */
  /**
   * When the RNA property from `rna_path` is an array, use this to access the array index.
   *
   * \note This may be negative (as it wasn't prevented in 2.91 and older).
   * Currently it silently fails to resolve the data-path in this case.
   */
  int array_index;
  /**
   * RNA-path to resolve data-access, see: #RNA_path_resolve_property.
   *
   * \note String look-ups for collection and custom-properties are escaped using #BLI_str_escape.
   */
  char *rna_path;

  /* curve coloring (for editor) */
  /** Coloring method to use (eFCurve_Coloring). */
  int color_mode;
  /** The last-color this curve took. */
  float color[3];

  float prev_norm_factor, prev_offset;
} FCurve;

/* ************************************************ */
/* 'Action' Data-types */

/* NOTE: Although these are part of the Animation System,
 * they are not stored here, see `DNA_action_types.h` instead. */

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
  /**
   * Action that is referenced by this strip (strip is 'user' of the action).
   *
   * \note Most code should not write to this field directly, but use functions from
   * `blender::animrig::nla` instead, see ANIM_nla.hh.
   */
  bAction *act;

  /**
   * Slot Handle to determine which animation data to look at in `act`.
   *
   * An NLA strip is limited to using a single slot in the Action.
   *
   * \note Most code should not write to this field directly, but use functions from
   * `blender::animrig::nla` instead, see ANIM_nla.hh.
   */
  int32_t action_slot_handle;
  /**
   * Slot name, primarily used for mapping to the right slot when assigning
   * another Action. Should be the same type as #ActionSlot::name.
   *
   * \see #ActionSlot::name
   *
   * \note Most code should not write to this field directly, but use functions from
   * `blender::animrig::nla` instead, see ANIM_nla.hh.
   */
  char last_slot_identifier[/*MAX_ID_NAME*/ 258];
  char _pad0[2];

  /** F-Curves for controlling this strip's influence and timing */ /* TODO: move out? */
  ListBase fcurves;
  /** F-Curve modifiers to be applied to the entire strip's referenced F-Curves. */
  ListBase modifiers;

  /** User-Visible Identifier for Strip. */
  char name[/*MAX_NAME*/ 64];

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

#ifdef __cplusplus
/* Some static assertions that things that should have the same type actually do. */
static_assert(
    std::is_same_v<decltype(ActionSlot::handle), decltype(NlaStrip::action_slot_handle)>);
#endif

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

  /** Short user-description of this track. */
  char name[/*MAX_NAME*/ 64];
} NlaTrack;

/* ************************************ */
/* KeyingSet Data-types */

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
  /** Name of the group to add to. */
  char group[/*MAX_NAME*/ 64];

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

  /** (#eInsertKeyFlags) settings to supply insert-key() with. */
  short keyingflag;
  /** (#eInsertKeyFlags) for each flag set, the relevant keying-flag bit overrides the default. */
  short keyingoverride;
} KS_Path;

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

  /** Unique name (for search, etc.). */
  char idname[/*MAX_NAME*/ 64];
  /** User-viewable name for KeyingSet (for menus, etc.). */
  char name[/*MAX_NAME*/ 64];
  /** (#RNA_DYN_DESCR_MAX) help text. */
  char description[1024];
  /** Name of the typeinfo data used for the relative paths. */
  char typeinfo[/*MAX_NAME*/ 64];

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
 * See `blenkernel/intern/anim_sys.cc` for details.
 */
typedef struct AnimData {
  /**
   * Active action - acts as the 'tweaking track' for the NLA.
   *
   * Legacy Actions: Either use BKE_animdata_set_action() to set this, or call
   * #BKE_animdata_action_ensure_idroot() after setting.
   *
   * Layered Actions: never set this directly, use one of the assignment
   * functions in ANIM_action.hh instead.
   */
  bAction *action;

  /**
   * Identifier for which ActionSlot of the above Action is actually animating this
   * data-block.
   *
   * Do not set this directly, use one of the assignment functions in ANIM_action.hh instead.
   *
   * This can be set to `blender::animrig::Slot::unassigned` when no slot is assigned. Note that
   * this field being set to any other value does NOT guarantee that there is a slot with that
   * handle, as it might have been deleted from the Action.
   */
  int32_t slot_handle;
  /**
   * Slot name, primarily used for mapping to the right slot when assigning
   * another Action. Should be the same type as #ActionSlot::name.
   *
   * \see #ActionSlot::name
   */
  char last_slot_identifier[/*MAX_ID_NAME*/ 258];
  uint8_t _pad0[2];

  /**
   * Temp-storage for the 'real' active action + slot (i.e. the ones used before
   * NLA Tweak mode took over the Action to be edited in the Animation Editors).
   */
  bAction *tmpact;
  int32_t tmp_slot_handle;
  char tmp_last_slot_identifier[/*MAX_ID_NAME*/ 258];
  uint8_t _pad1[2];

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

  /* settings for active action evaluation (based on NLA strip settings) */
  /** Accumulation mode for active action. */
  short act_blendmode;
  /** Extrapolation mode for active action. */
  short act_extendmode;
  /** Influence for active action. */
  float act_influence;

  uint8_t _pad2[4];
} AnimData;

#ifdef __cplusplus
/* Some static assertions that things that should have the same type actually do. */
static_assert(std::is_same_v<decltype(ActionSlot::handle), decltype(AnimData::slot_handle)>);
static_assert(
    std::is_same_v<decltype(ActionSlot::identifier), decltype(AnimData::last_slot_identifier)>);
#endif

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
