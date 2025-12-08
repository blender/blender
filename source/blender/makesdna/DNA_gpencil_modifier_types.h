/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_constants.h"

#include "DNA_armature_types.h"
#include "DNA_brush_enums.h"
#include "DNA_defs.h"
#include "DNA_lineart_types.h"
#include "DNA_modifier_enums.h"
#include "DNA_vec_defaults.h"

struct LatticeDeformData;
struct ShrinkwrapTreeData;

enum GpencilModifierMode {
  eGpencilModifierMode_Realtime = (1 << 0),
  eGpencilModifierMode_Render = (1 << 1),
  eGpencilModifierMode_Editmode = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  eGpencilModifierMode_Expanded_DEPRECATED = (1 << 3),
#endif
  eGpencilModifierMode_Virtual = (1 << 4),
};

enum GpencilModifierFlag {
  /* This modifier has been inserted in local override, and hence can be fully edited. */
  eGpencilModifierFlag_OverrideLibrary_Local = (1 << 0),
};

enum eGpencilModifierSpace {
  GP_SPACE_LOCAL = 0,
  GP_SPACE_WORLD = 1,
};

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

enum GpencilModifierType {
  eGpencilModifierType_None = 0,
  eGpencilModifierType_Noise = 1,
  eGpencilModifierType_Subdiv = 2,
  eGpencilModifierType_Thick = 3,
  eGpencilModifierType_Tint = 4,
  eGpencilModifierType_Array = 5,
  eGpencilModifierType_Build = 6,
  eGpencilModifierType_Opacity = 7,
  eGpencilModifierType_Color = 8,
  eGpencilModifierType_Lattice = 9,
  eGpencilModifierType_Simplify = 10,
  eGpencilModifierType_Smooth = 11,
  eGpencilModifierType_Hook = 12,
  eGpencilModifierType_Offset = 13,
  eGpencilModifierType_Mirror = 14,
  eGpencilModifierType_Armature = 15,
  eGpencilModifierType_Time = 16,
  eGpencilModifierType_Multiply = 17,
  eGpencilModifierType_Texture = 18,
  eGpencilModifierType_Lineart = 19,
  eGpencilModifierType_Length = 20,
  eGpencilModifierType_WeightProximity = 21,
  eGpencilModifierType_Dash = 22,
  eGpencilModifierType_WeightAngle = 23,
  eGpencilModifierType_Shrinkwrap = 24,
  eGpencilModifierType_Envelope = 25,
  eGpencilModifierType_Outline = 26,
  /* Keep last. */
  NUM_GREASEPENCIL_MODIFIER_TYPES,
};

struct GpencilModifierData {
  struct GpencilModifierData *next = nullptr, *prev = nullptr;

  int type = 0, mode = 0;
  char _pad0[4] = {};
  short flag = 0;
  /* An "expand" bit for each of the modifier's (sub)panels (uiPanelDataExpansion). */
  short ui_expand_flag = 0;
  char name[/*MAX_NAME*/ 64] = "";

  char *error = nullptr;
};

struct NoiseGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Several flags. */
  int flag = GP_NOISE_FULL_STROKE | GP_NOISE_USE_RANDOM;
  /** Factor of noise. */
  float factor = 0.5f;
  float factor_strength = 0.0f;
  float factor_thickness = 0.0f;
  float factor_uvs = 0.0f;
  /** Noise Frequency scaling */
  float noise_scale = 0.0f;
  float noise_offset = 0.0f;
  short noise_mode = 0;
  char _pad[2] = {};
  /** How many frames before recalculate randoms. */
  int step = 4;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Random seed */
  int seed = 1;
  struct CurveMapping *curve_intensity = nullptr;
};

enum eSubdivGpencil_Flag {
  GP_SUBDIV_INVERT_LAYER = (1 << 1),
  GP_SUBDIV_INVERT_PASS = (1 << 2),
  GP_SUBDIV_INVERT_LAYERPASS = (1 << 3),
  GP_SUBDIV_INVERT_MATERIAL = (1 << 4),
};

enum eSubdivGpencil_Type {
  GP_SUBDIV_CATMULL = 0,
  GP_SUBDIV_SIMPLE = 1,
};

struct SubdivGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Factor of subdivision. */
  int level = 1;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Type of subdivision */
  short type = 0;
  char _pad[6] = {};
};

enum eThickGpencil_Flag {
  GP_THICK_INVERT_LAYER = (1 << 0),
  GP_THICK_INVERT_PASS = (1 << 1),
  GP_THICK_INVERT_VGROUP = (1 << 2),
  GP_THICK_CUSTOM_CURVE = (1 << 3),
  GP_THICK_NORMALIZE = (1 << 4),
  GP_THICK_INVERT_LAYERPASS = (1 << 5),
  GP_THICK_INVERT_MATERIAL = (1 << 6),
  GP_THICK_WEIGHT_FACTOR = (1 << 7),
};

struct ThickGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Relative thickness factor. */
  float thickness_fac = 1.0f;
  /** Absolute thickness override. */
  int thickness = 30;
  /** Custom index for passes. */
  int layer_pass = 0;
  char _pad[4] = {};
  struct CurveMapping *curve_thickness = nullptr;
};

enum eTimeGpencil_Flag {
  GP_TIME_INVERT_LAYER = (1 << 0),
  GP_TIME_KEEP_LOOP = (1 << 1),
  GP_TIME_INVERT_LAYERPASS = (1 << 2),
  GP_TIME_CUSTOM_RANGE = (1 << 3),
};

enum eTimeGpencil_Mode {
  GP_TIME_MODE_NORMAL = 0,
  GP_TIME_MODE_REVERSE = 1,
  GP_TIME_MODE_FIX = 2,
  GP_TIME_MODE_PINGPONG = 3,
  GP_TIME_MODE_CHAIN = 4,
};

enum eTimeGpencil_Seg_Mode {
  GP_TIME_SEG_MODE_NORMAL = 0,
  GP_TIME_SEG_MODE_REVERSE = 1,
  GP_TIME_SEG_MODE_PINGPONG = 2,
};

struct TimeGpencilModifierSegment {
  char name[64] = "";
  /* For path reference. */
  struct TimeGpencilModifierData *gpmd = nullptr;
  int seg_start = 1;
  int seg_end = 2;
  int seg_mode = 0;
  int seg_repeat = 1;
};

struct TimeGpencilModifierData {
  GpencilModifierData modifier;
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Flags. */
  int flag = GP_TIME_KEEP_LOOP;
  int offset = 1;
  /** Animation scale. */
  float frame_scale = 1.0f;
  int mode = 0;
  /** Start and end frame for custom range. */
  int sfra = 1, efra = 250;

  char _pad[4] = {};

  TimeGpencilModifierSegment *segments = nullptr;
  int segments_len = 1;
  int segment_active_index = 0;
};

enum eModifyColorGpencil_Flag {
  GP_MODIFY_COLOR_BOTH = 0,
  GP_MODIFY_COLOR_STROKE = 1,
  GP_MODIFY_COLOR_FILL = 2,
  GP_MODIFY_COLOR_HARDNESS = 3,
};

enum eOpacityModesGpencil_Flag {
  GP_OPACITY_MODE_MATERIAL = 0,
  GP_OPACITY_MODE_STRENGTH = 1,
};

enum eColorGpencil_Flag {
  GP_COLOR_INVERT_LAYER = (1 << 1),
  GP_COLOR_INVERT_PASS = (1 << 2),
  GP_COLOR_INVERT_LAYERPASS = (1 << 3),
  GP_COLOR_INVERT_MATERIAL = (1 << 4),
  GP_COLOR_CUSTOM_CURVE = (1 << 5),
};

enum eOpacityGpencil_Flag {
  GP_OPACITY_INVERT_LAYER = (1 << 0),
  GP_OPACITY_INVERT_PASS = (1 << 1),
  GP_OPACITY_INVERT_VGROUP = (1 << 2),
  GP_OPACITY_INVERT_LAYERPASS = (1 << 4),
  GP_OPACITY_INVERT_MATERIAL = (1 << 5),
  GP_OPACITY_CUSTOM_CURVE = (1 << 6),
  GP_OPACITY_NORMALIZE = (1 << 7),
  GP_OPACITY_WEIGHT_FACTOR = (1 << 8),
};

struct ColorGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** HSV factors. */
  float hsv[3] = {0.5f, 1.0f, 1.0f};
  /** Modify stroke, fill or both. */
  char modify_color = GP_MODIFY_COLOR_BOTH;
  char _pad[3] = {};
  /** Custom index for passes. */
  int layer_pass = 0;

  char _pad1[4] = {};
  struct CurveMapping *curve_intensity = nullptr;
};

struct OpacityGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Main Opacity factor. */
  float factor = 1.0f;
  /** Modify stroke, fill or both. */
  char modify_color = GP_MODIFY_COLOR_BOTH;
  char _pad[3] = {};
  /** Custom index for passes. */
  int layer_pass = 0;

  float hardness = 1.0f;
  struct CurveMapping *curve_intensity = nullptr;
};

enum eOutlineGpencil_Flag {
  GP_OUTLINE_INVERT_LAYER = (1 << 0),
  GP_OUTLINE_INVERT_PASS = (1 << 1),
  GP_OUTLINE_INVERT_LAYERPASS = (1 << 2),
  GP_OUTLINE_INVERT_MATERIAL = (1 << 3),
  GP_OUTLINE_KEEP_SHAPE = (1 << 4),
};

struct OutlineGpencilModifierData {
  GpencilModifierData modifier;
  /** Target stroke origin. */
  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = GP_OUTLINE_KEEP_SHAPE;
  /** Thickness. */
  int thickness = 1;
  /** Sample Length. */
  float sample_length = 0.0f;
  /** Subdivisions. */
  int subdiv = 3;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Material for outline. */
  struct Material *outline_material = nullptr;
};

struct ArrayGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Number of elements in array. */
  int count = 2;
  /** Several flags. */
  int flag = GP_ARRAY_USE_RELATIVE;
  /** Location increments. */
  float offset[3] = {0.0f, 0.0f, 0.0f};
  /** Shift increment. */
  float shift[3] = {1.0f, 0.0f, 0.0f};
  /** Random Offset. */
  float rnd_offset[3] = {0.0f, 0.0f, 0.0f};
  /** Random Rotation. */
  float rnd_rot[3] = {0.0f, 0.0f, 0.0f};
  /** Random Scales. */
  float rnd_scale[3] = {0.0f, 0.0f, 0.0f};

  char _pad[4] = {};
  /** (first element is the index) random values. */
  int seed = 1;

  /** Custom index for passes. */
  int pass_index = 0;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Material replace (0 keep default). */
  int mat_rpl = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
};

enum eBuildGpencil_Mode {
  /* Strokes are shown one by one until all have appeared */
  GP_BUILD_MODE_SEQUENTIAL = 0,
  /* All strokes start at the same time */
  GP_BUILD_MODE_CONCURRENT = 1,
  /* Only the new strokes are built */
  GP_BUILD_MODE_ADDITIVE = 2,
};

enum eBuildGpencil_Transition {
  /* Show in forward order */
  GP_BUILD_TRANSITION_GROW = 0,
  /* Hide in reverse order */
  GP_BUILD_TRANSITION_SHRINK = 1,
  /* Hide in forward order */
  GP_BUILD_TRANSITION_VANISH = 2,
};

enum eBuildGpencil_TimeAlignment {
  /* All strokes start at same time */
  GP_BUILD_TIMEALIGN_START = 0,
  /* All strokes end at same time */
  GP_BUILD_TIMEALIGN_END = 1,

  /* TODO: Random Offsets, Stretch-to-Fill */
};

enum eBuildGpencil_TimeMode {
  /** Use a number of frames build. */
  GP_BUILD_TIMEMODE_FRAMES = 0,
  /** Use manual percentage to build. */
  GP_BUILD_TIMEMODE_PERCENTAGE = 1,
  /** Use factor of recorded speed to build. */
  GP_BUILD_TIMEMODE_DRAWSPEED = 2,
};

enum eBuildGpencil_Flag {
  /* Restrict modifier to particular layer/passes? */
  GP_BUILD_INVERT_LAYER = (1 << 0),
  GP_BUILD_INVERT_PASS = (1 << 1),

  /* Restrict modifier to only operating between the nominated frames */
  GP_BUILD_RESTRICT_TIME = (1 << 2),
  GP_BUILD_INVERT_LAYERPASS = (1 << 3),
  GP_BUILD_USE_FADING = (1 << 4),
};

struct BuildGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;

  /** If set, restrict modifier to operating on this layer. */
  char layername[64] = "";
  int pass_index = 0;

  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";

  /** Custom index for passes. */
  int layer_pass = 0;

  /**
   * If GP_BUILD_RESTRICT_TIME is set,
   * the defines the frame range where GP frames are considered.
   */
  float start_frame = 1;
  float end_frame = 125;

  /** For each pair of gp keys, number of frames before strokes start appearing. */
  float start_delay = 0.0f;
  /** For each pair of gp keys, number of frames that build effect must be completed within. */
  float length = 100.0f;

  /** (eGpencilBuild_Flag) Options for controlling modifier behavior. */
  short flag = 0;

  /** (eGpencilBuild_Mode) How are strokes ordered. */
  short mode = 0;
  /** (eGpencilBuild_Transition) In what order do stroke points appear/disappear. */
  short transition = 0;

  /**
   * (eBuildGpencil_TimeAlignment)
   * For the "Concurrent" mode, when should "shorter" strips start/end.
   */
  short time_alignment = 0;

  /** Speed factor for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_fac = 1.2f;
  /** Maximum time gap between strokes for #GP_BUILD_TIMEMODE_DRAWSPEED. */
  float speed_maxgap = 0.5f;
  /** Which time mode should be used. */
  short time_mode = 0;
  char _pad[6] = {};

  /** Build origin control object. */
  struct Object *object = nullptr;

  /** Factor of the stroke (used instead of frame evaluation. */
  float percentage_fac = 0.0f;

  /** Weight fading at the end of the stroke. */
  float fade_fac = 0;
  /** Target vertex-group name. */
  char target_vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Fading strength of opacity and thickness */
  float fade_opacity_strength = 0;
  float fade_thickness_strength = 0;
};

struct LatticeGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name, . */
  char vgname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  float strength = 1.0f;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Runtime only. */
  struct LatticeDeformData *cache_data = nullptr;
};

enum eLatticeGpencil_Flag {
  GP_LATTICE_INVERT_LAYER = (1 << 0),
  GP_LATTICE_INVERT_PASS = (1 << 1),
  GP_LATTICE_INVERT_VGROUP = (1 << 2),
  GP_LATTICE_INVERT_LAYERPASS = (1 << 3),
  GP_LATTICE_INVERT_MATERIAL = (1 << 4),
};

struct LengthGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = GP_LENGTH_USE_CURVATURE;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Length. */
  float start_fac = 0.1f, end_fac = 0.1f;
  /** Random length factors. */
  float rand_start_fac = 0.0f, rand_end_fac = 0.0f, rand_offset = 0.0f;
  /** Overshoot trajectory factor. */
  float overshoot_fac = 0.1f;
  /** (first element is the index) random values. */
  int seed = 0;
  /** How many frames before recalculate randoms. */
  int step = 4;
  /** Modifier mode. */
  int mode = 0;
  char _pad[4] = {};
  /* Curvature parameters. */
  float point_density = 30.0f;
  float segment_influence = 0.0f;
  float max_angle = DEG2RAD(170.0f);
};

enum eDashGpencil_Flag {
  GP_DASH_INVERT_LAYER = (1 << 0),
  GP_DASH_INVERT_PASS = (1 << 1),
  GP_DASH_INVERT_LAYERPASS = (1 << 2),
  GP_DASH_INVERT_MATERIAL = (1 << 3),
  GP_DASH_USE_CYCLIC = (1 << 7),
};

struct DashGpencilModifierSegment {
  char name[64] = "";
  /* For path reference. */
  struct DashGpencilModifierData *dmd = nullptr;
  int dash = 2;
  int gap = 1;
  float radius = 1.0f;
  float opacity = 1.0f;
  int mat_nr = -1;
  int flag = 0;
};

struct DashGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Custom index for passes. */
  int layer_pass = 0;

  int dash_offset = 0;

  DashGpencilModifierSegment *segments = nullptr;
  int segments_len = 1;
  int segment_active_index = 0;
};

enum eMirrorGpencil_Flag {
  GP_MIRROR_INVERT_LAYER = (1 << 0),
  GP_MIRROR_INVERT_PASS = (1 << 1),
  GP_MIRROR_CLIPPING = (1 << 2),
  GP_MIRROR_AXIS_X = (1 << 3),
  GP_MIRROR_AXIS_Y = (1 << 4),
  GP_MIRROR_AXIS_Z = (1 << 5),
  GP_MIRROR_INVERT_LAYERPASS = (1 << 6),
  GP_MIRROR_INVERT_MATERIAL = (1 << 7),
};

struct MirrorGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = GP_MIRROR_AXIS_X;
  /** Custom index for passes. */
  int layer_pass = 0;
  char _pad[4] = {};
};

enum eHookGpencil_Flag {
  GP_HOOK_INVERT_LAYER = (1 << 0),
  GP_HOOK_INVERT_PASS = (1 << 1),
  GP_HOOK_INVERT_VGROUP = (1 << 2),
  GP_HOOK_UNIFORM_SPACE = (1 << 3),
  GP_HOOK_INVERT_LAYERPASS = (1 << 4),
  GP_HOOK_INVERT_MATERIAL = (1 << 5),
};

enum eHookGpencil_Falloff {
  eGPHook_Falloff_None = 0,
  eGPHook_Falloff_Curve = 1,
  eGPHook_Falloff_Sharp = 2,
  eGPHook_Falloff_Smooth = 3,
  eGPHook_Falloff_Root = 4,
  eGPHook_Falloff_Linear = 5,
  eGPHook_Falloff_Const = 6,
  eGPHook_Falloff_Sphere = 7,
  eGPHook_Falloff_InvSquare = 8,
};

struct HookGpencilModifierData {
  GpencilModifierData modifier;

  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Optional name of bone target. */
  char subtarget[/*MAX_NAME*/ 64] = "";
  /** Layer name. */
  char layername[/*MAX_NAME*/ 64] = "";
  /**
   * Material name.
   * \note as this is legacy there is no need to use the current size of an ID name.
   */
  DNA_DEPRECATED char materialname[/*MAX_ID_NAME - 194*/ 64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  char _pad[4] = {};

  int flag = 0;
  /** #eHookGpencil_Falloff. */
  char falloff_type = eGPHook_Falloff_Smooth;
  char _pad1[3] = {};
  /** Matrix making current transform unmodified. */
  float parentinv[4][4] = _DNA_DEFAULT_UNIT_M4;
  /** Visualization of hook. */
  float cent[3] = {0.0f, 0.0f, 0.0f};
  /** If not zero, falloff is distance where influence zero. */
  float falloff = 0.0f;
  float force = 0.5f;
  struct CurveMapping *curfalloff = nullptr;
};

enum eSimplifyGpencil_Flag {
  GP_SIMPLIFY_INVERT_LAYER = (1 << 0),
  GP_SIMPLIFY_INVERT_PASS = (1 << 1),
  GP_SIMPLIFY_INVERT_LAYERPASS = (1 << 2),
  GP_SIMPLIFY_INVERT_MATERIAL = (1 << 3),
};

enum eSimplifyGpencil_Mode {
  /* Keep only one vertex every n vertices */
  GP_SIMPLIFY_FIXED = 0,
  /* Use RDP algorithm */
  GP_SIMPLIFY_ADAPTIVE = 1,
  /* Sample the stroke using a fixed length */
  GP_SIMPLIFY_SAMPLE = 2,
  /* Sample the stroke doing vertex merge */
  GP_SIMPLIFY_MERGE = 3,
};

struct SimplifyGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Factor of simplify. */
  float factor = 0.0f;
  /** Type of simplify. */
  short mode = 0;
  /** Every n vertex to keep. */
  short step = 1;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Sample length */
  float length = 0.1f;
  /** Sample sharp threshold */
  float sharp_threshold = 0;
  /** Merge distance */
  float distance = 0.1f;
};

enum eOffsetGpencil_Mode {
  GP_OFFSET_RANDOM = 0,
  GP_OFFSET_LAYER = 1,
  GP_OFFSET_MATERIAL = 2,
  GP_OFFSET_STROKE = 3

};

enum eOffsetGpencil_Flag {
  GP_OFFSET_INVERT_LAYER = (1 << 0),
  GP_OFFSET_INVERT_PASS = (1 << 1),
  GP_OFFSET_INVERT_VGROUP = (1 << 2),
  GP_OFFSET_INVERT_LAYERPASS = (1 << 3),
  GP_OFFSET_INVERT_MATERIAL = (1 << 4),
  GP_OFFSET_UNIFORM_RANDOM_SCALE = (1 << 5),
};

struct OffsetGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  float loc[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};
  float scale[3] = {0.0f, 0.0f, 0.0f};
  /** Random Offset. */
  float rnd_offset[3] = {};
  /** Random Rotation. */
  float rnd_rot[3] = {};
  /** Random Scales. */
  float rnd_scale[3] = {};
  /** (first element is the index) random values. */
  int seed = 0;
  int mode = GP_OFFSET_RANDOM;
  int stroke_step = 1;
  int stroke_start_offset = 0;
  int layer_pass = 0;
  char _pad[4] = {};
};

enum eSmoothGpencil_Flag {
  GP_SMOOTH_MOD_LOCATION = (1 << 0),
  GP_SMOOTH_MOD_STRENGTH = (1 << 1),
  GP_SMOOTH_MOD_THICKNESS = (1 << 2),
  GP_SMOOTH_INVERT_LAYER = (1 << 3),
  GP_SMOOTH_INVERT_PASS = (1 << 4),
  GP_SMOOTH_INVERT_VGROUP = (1 << 5),
  GP_SMOOTH_MOD_UV = (1 << 6),
  GP_SMOOTH_INVERT_LAYERPASS = (1 << 7),
  GP_SMOOTH_INVERT_MATERIAL = (1 << 4),
  GP_SMOOTH_CUSTOM_CURVE = (1 << 8),
  GP_SMOOTH_KEEP_SHAPE = (1 << 9),
};

struct SmoothGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Several flags. */
  int flag = GP_SMOOTH_MOD_LOCATION;
  /** Factor of smooth. */
  float factor = 1.0f;
  /** How many times apply smooth. */
  int step = 1;
  /** Custom index for passes. */
  int layer_pass = 0;

  char _pad1[4] = {};
  struct CurveMapping *curve_intensity = nullptr;
};

struct ArmatureGpencilModifierData {
  GpencilModifierData modifier;
  /** #eArmature_DeformFlag use instead of #bArmature.deformflag. */
  short deformflag = ARM_DEF_VGROUP, multi = 0;
  int _pad = {};
  struct Object *object = nullptr;
  /** Stored input of previous modifier, for vertex-group blending. */
  float (*vert_coords_prev)[3] = nullptr;
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
};

enum eMultiplyGpencil_Flag {
  /* GP_MULTIPLY_ENABLE_ANGLE_SPLITTING = (1 << 1),  Deprecated. */
  GP_MULTIPLY_ENABLE_FADING = (1 << 2),
};

struct MultiplyGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Custom index for passes. */
  int layer_pass = 0;

  int flags = 0;

  int duplications = 3;
  float distance = 0.1f;
  /* -1:inner 0:middle 1:outer */
  float offset = 0.0f;

  float fading_center = 0.5f;
  float fading_thickness = 0.5f;
  float fading_opacity = 0.5f;
};

enum eTintGpencil_Type {
  GP_TINT_UNIFORM = 0,
  GP_TINT_GRADIENT = 1,
};

enum eTintGpencil_Flag {
  GP_TINT_INVERT_LAYER = (1 << 0),
  GP_TINT_INVERT_PASS = (1 << 1),
  GP_TINT_INVERT_VGROUP = (1 << 2),
  GP_TINT_INVERT_LAYERPASS = (1 << 4),
  GP_TINT_INVERT_MATERIAL = (1 << 5),
  GP_TINT_CUSTOM_CURVE = (1 << 6),
  GP_TINT_WEIGHT_FACTOR = (1 << 7),
};

struct TintGpencilModifierData {
  GpencilModifierData modifier;

  struct Object *object = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Flags. */
  int flag = 0;
  /** Mode (Stroke/Fill/Both). */
  int mode = GPPAINT_MODE_BOTH;

  float factor = 0.5f;
  float radius = 1.0f;
  /** Simple Tint color. */
  float rgb[3] = {1.0f, 1.0f, 1.0f};
  /** Type of Tint. */
  int type = 0;

  struct CurveMapping *curve_intensity = nullptr;

  struct ColorBand *colorband = nullptr;
};

enum eTextureGpencil_Flag {
  GP_TEX_INVERT_LAYER = (1 << 0),
  GP_TEX_INVERT_PASS = (1 << 1),
  GP_TEX_INVERT_VGROUP = (1 << 2),
  GP_TEX_INVERT_LAYERPASS = (1 << 3),
  GP_TEX_INVERT_MATERIAL = (1 << 4),
};

/* Texture->mode */
enum eTextureGpencil_Mode {
  STROKE = 0,
  FILL = 1,
  STROKE_AND_FILL = 2,
};

struct TextureGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Material name. */
  DNA_DEPRECATED char materialname[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Offset value to add to uv_fac. */
  float uv_offset = 0.0f;
  float uv_scale = 1.0f;
  float fill_rotation = 0.0f;
  float fill_offset[2] = {0.0f, 0.0f};
  float fill_scale = 1.0f;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Texture fit options. */
  short fit_method = GP_TEX_CONSTANT_LENGTH;
  short mode = 0;
  /** Dot texture rotation */
  float alignment_rotation = 0;
  char _pad[4] = {};
};

enum eWeightGpencil_Flag {
  GP_WEIGHT_INVERT_LAYER = (1 << 0),
  GP_WEIGHT_INVERT_PASS = (1 << 1),
  GP_WEIGHT_INVERT_VGROUP = (1 << 2),
  GP_WEIGHT_INVERT_LAYERPASS = (1 << 3),
  GP_WEIGHT_INVERT_MATERIAL = (1 << 4),
  GP_WEIGHT_MULTIPLY_DATA = (1 << 5),
  GP_WEIGHT_INVERT_OUTPUT = (1 << 6),
};

struct WeightProxGpencilModifierData {
  GpencilModifierData modifier;
  /** Target vertex-group name. */
  char target_vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Optional vertex-group filter name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Minimum valid weight (clamp value). */
  float min_weight = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Start/end distances. */
  float dist_start = 0.0f;
  float dist_end = 20.0f;

  /** Reference object */
  struct Object *object = nullptr;
};

struct WeightAngleGpencilModifierData {
  GpencilModifierData modifier;
  /** Target vertex-group name. */
  char target_vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Optional vertex-group filter name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Minimum valid weight (clamp value). */
  float min_weight = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Axis. */
  short axis = 1;
  /** Space (Local/World). */
  short space = 0;
  /** Angle */
  float angle = 0;
};

struct LineartCache;

struct LineartGpencilModifierData {
  GpencilModifierData modifier;

  uint16_t edge_types =
      MOD_LINEART_EDGE_FLAG_INIT_TYPE; /* line type enable flags, bits in eLineartEdgeFlag */

  /** Object or Collection, from #GreasePencilLineartModifierSource. */
  char source_type = 0;

  char use_multiple_levels = 0;
  short level_start = 0;
  short level_end = 0;

  struct Object *source_camera = nullptr;
  struct Object *light_contour_object = nullptr;

  struct Object *source_object = nullptr;
  struct Collection *source_collection = nullptr;

  struct Material *target_material = nullptr;
  char target_layer[64] = "";

  /**
   * These two variables are to pass on vertex group information from mesh to strokes.
   * `vgname` specifies which vertex groups our strokes from source_vertex_group will go to.
   */
  char source_vertex_group[64] = "";
  char vgname[64] = "";

  /* Camera focal length is divided by (1 + over-scan), before calculation, which give a wider FOV,
   * this doesn't change coordinates range internally (-1, 1), but makes the calculated frame
   * bigger than actual output. This is for the easier shifting calculation. A value of 0.5 means
   * the "internal" focal length become 2/3 of the actual camera. */
  float overscan = 0.1f;

  /* Values for point light and directional (sun) light. */
  /* For point light, fov always gonna be 120 deg horizontal, with 3 "cameras" covering 360 deg. */
  float shadow_camera_fov = 0;
  float shadow_camera_size = 200.0f;
  float shadow_camera_near = 0.1f;
  float shadow_camera_far = 200.0f;

  float opacity = 1.0f;
  short thickness = 25;

  unsigned char mask_switches = 0; /* #GreasePencilLineartMaskSwitches */
  unsigned char material_mask_bits = 0;
  unsigned char intersection_mask = 0;

  unsigned char shadow_selection = 0;
  unsigned char silhouette_selection = 0;
  char _pad[1] = {};

  /** `0..1` range for cosine angle */
  float crease_threshold = DEG2RAD(140.0f);

  /** `0..PI` angle, for splitting strokes at sharp points. */
  float angle_splitting_threshold = 0.0f;

  /** Strength for smoothing jagged chains. */
  float chain_smooth_tolerance = 0.0f;

  /* CPU mode */
  float chaining_image_threshold = 0.001f;

  /* eLineartMainFlags, for one time calculation. */
  int calculation_flags = MOD_LINEART_ALLOW_DUPLI_OBJECTS | MOD_LINEART_ALLOW_CLIPPING_BOUNDARIES |
                          MOD_LINEART_USE_CREASE_ON_SHARP_EDGES |
                          MOD_LINEART_FILTER_FACE_MARK_KEEP_CONTOUR |
                          MOD_LINEART_MATCH_OUTPUT_VGROUP;

  /* #eLineArtGPencilModifierFlags, modifier internal state. */
  int flags = 0;

  /* Move strokes towards camera to avoid clipping while preserve depth for the viewport. */
  float stroke_depth_offset = 0.05;

  /* Runtime data. */

  /* Because we can potentially only compute features lines once per modifier stack (Use Cache), we
   * need to have these override values to ensure that we have the data we need is computed and
   * stored in the cache. */
  char level_start_override = 0;
  char level_end_override = 0;
  short edge_types_override = 0;
  char shadow_selection_override = 0;
  char shadow_use_silhouette_override = 0;

  char _pad2[6] = {};

  struct LineartCache *cache = nullptr;
  /** Keep a pointer to the render buffer so we can call destroy from #ModifierData. */
  struct LineartData *la_data_ptr = nullptr;
};

enum eShrinkwrapGpencil_Flag {
  GP_SHRINKWRAP_INVERT_LAYER = (1 << 0),
  GP_SHRINKWRAP_INVERT_PASS = (1 << 1),
  GP_SHRINKWRAP_INVERT_LAYERPASS = (1 << 3),
  GP_SHRINKWRAP_INVERT_MATERIAL = (1 << 4),
  /* Keep next bit as is to be equals to mesh modifier flag to reuse functions. */
  GP_SHRINKWRAP_INVERT_VGROUP = (1 << 6),
};

struct ShrinkwrapGpencilModifierData {
  GpencilModifierData modifier;
  /** Shrink target. */
  struct Object *target = nullptr;
  /** Additional shrink target. */
  struct Object *aux_target = nullptr;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Optional vertex-group filter name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Flags. */
  int flag = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  /** Distance offset to keep from mesh/projection point. */
  float keep_dist = 0.05f;
  /** Shrink type projection. */
  short shrink_type = MOD_SHRINKWRAP_NEAREST_SURFACE;
  /** Shrink options. */
  char shrink_opts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
  /** Shrink to surface mode. */
  char shrink_mode = 0;
  /** Limit the projection ray cast. */
  float proj_limit = 0.0f;
  /** Axis to project over. */
  char proj_axis = 0;

  /**
   * If using projection over vertex normal this controls the level of subsurface that must be
   * done before getting the vertex coordinates and normal.
   */
  char subsurf_levels = 0;
  char _pad[6] = {};
  /** Factor of smooth. */
  float smooth_factor = 0.05f;
  /** How many times apply smooth. */
  int smooth_step = 1;

  /** Runtime only. */
  struct ShrinkwrapTreeData *cache_data = nullptr;
};

enum eEnvelopeGpencil_Flag {
  GP_ENVELOPE_INVERT_LAYER = (1 << 0),
  GP_ENVELOPE_INVERT_PASS = (1 << 1),
  GP_ENVELOPE_INVERT_VGROUP = (1 << 2),
  GP_ENVELOPE_INVERT_LAYERPASS = (1 << 3),
  GP_ENVELOPE_INVERT_MATERIAL = (1 << 4),
};

/* Texture->mode */
enum eEnvelopeGpencil_Mode {
  GP_ENVELOPE_DEFORM = 0,
  GP_ENVELOPE_SEGMENTS = 1,
  GP_ENVELOPE_FILLS = 2,
};

struct EnvelopeGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material = nullptr;
  /** Layer name. */
  char layername[64] = "";
  /** Optional vertex-group name. */
  char vgname[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Custom index for passes. */
  int pass_index = 0;
  /** Several flags. */
  int flag = 0;
  int mode = GP_ENVELOPE_SEGMENTS;
  /** Material for the new strokes. */
  int mat_nr = -1;
  /** Thickness multiplier for the new strokes. */
  float thickness = 1.0f;
  /** Strength multiplier for the new strokes. */
  float strength = 1.0f;
  /** Number of points to skip over. */
  int skip = 0;
  /** Custom index for passes. */
  int layer_pass = 0;
  /* Length of the envelope effect. */
  int spread = 10;

  char _pad[4] = {};
};
