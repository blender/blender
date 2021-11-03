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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct LatticeDeformData;

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

typedef enum GpencilModifierType {
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
  /* Keep last. */
  NUM_GREASEPENCIL_MODIFIER_TYPES,
} GpencilModifierType;

typedef enum GpencilModifierMode {
  eGpencilModifierMode_Realtime = (1 << 0),
  eGpencilModifierMode_Render = (1 << 1),
  eGpencilModifierMode_Editmode = (1 << 2),
#ifdef DNA_DEPRECATED_ALLOW
  eGpencilModifierMode_Expanded_DEPRECATED = (1 << 3),
#endif
  eGpencilModifierMode_Virtual = (1 << 4),
} GpencilModifierMode;

typedef enum {
  /* This modifier has been inserted in local override, and hence can be fully edited. */
  eGpencilModifierFlag_OverrideLibrary_Local = (1 << 0),
} GpencilModifierFlag;

typedef struct GpencilModifierData {
  struct GpencilModifierData *next, *prev;

  int type, mode;
  char _pad0[4];
  short flag;
  /* An "expand" bit for each of the modifier's (sub)panels (uiPanelDataExpansion). */
  short ui_expand_flag;
  /** MAX_NAME. */
  char name[64];

  char *error;
} GpencilModifierData;

typedef struct NoiseGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Several flags. */
  int flag;
  /** Factor of noise. */
  float factor;
  float factor_strength;
  float factor_thickness;
  float factor_uvs;
  /** Noise Frequency scaling */
  float noise_scale;
  float noise_offset;
  char _pad[4];
  /** How many frames before recalculate randoms. */
  int step;
  /** Custom index for passes. */
  int layer_pass;
  /** Random seed */
  int seed;
  struct CurveMapping *curve_intensity;
} NoiseGpencilModifierData;

typedef enum eNoiseGpencil_Flag {
  GP_NOISE_USE_RANDOM = (1 << 0),
  GP_NOISE_MOD_LOCATION = (1 << 1),  /* Deprecated (only for versioning). */
  GP_NOISE_MOD_STRENGTH = (1 << 2),  /* Deprecated (only for versioning). */
  GP_NOISE_MOD_THICKNESS = (1 << 3), /* Deprecated (only for versioning). */
  GP_NOISE_FULL_STROKE = (1 << 4),
  GP_NOISE_CUSTOM_CURVE = (1 << 5),
  GP_NOISE_INVERT_LAYER = (1 << 6),
  GP_NOISE_INVERT_PASS = (1 << 7),
  GP_NOISE_INVERT_VGROUP = (1 << 8),
  GP_NOISE_MOD_UV = (1 << 9), /* Deprecated (only for versioning). */
  GP_NOISE_INVERT_LAYERPASS = (1 << 10),
  GP_NOISE_INVERT_MATERIAL = (1 << 11),
} eNoiseGpencil_Flag;

typedef struct SubdivGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Factor of subdivision. */
  int level;
  /** Custom index for passes. */
  int layer_pass;
  /** Type of subdivision */
  short type;
  char _pad[6];
} SubdivGpencilModifierData;

typedef enum eSubdivGpencil_Flag {
  GP_SUBDIV_INVERT_LAYER = (1 << 1),
  GP_SUBDIV_INVERT_PASS = (1 << 2),
  GP_SUBDIV_INVERT_LAYERPASS = (1 << 3),
  GP_SUBDIV_INVERT_MATERIAL = (1 << 4),
} eSubdivGpencil_Flag;

typedef enum eSubdivGpencil_Type {
  GP_SUBDIV_CATMULL = 0,
  GP_SUBDIV_SIMPLE = 1,
} eSubdivGpencil_Type;

typedef struct ThickGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Relative thickness factor. */
  float thickness_fac;
  /** Absolute thickness override. */
  int thickness;
  /** Custom index for passes. */
  int layer_pass;
  char _pad[4];
  struct CurveMapping *curve_thickness;
} ThickGpencilModifierData;

typedef enum eThickGpencil_Flag {
  GP_THICK_INVERT_LAYER = (1 << 0),
  GP_THICK_INVERT_PASS = (1 << 1),
  GP_THICK_INVERT_VGROUP = (1 << 2),
  GP_THICK_CUSTOM_CURVE = (1 << 3),
  GP_THICK_NORMALIZE = (1 << 4),
  GP_THICK_INVERT_LAYERPASS = (1 << 5),
  GP_THICK_INVERT_MATERIAL = (1 << 6),
  GP_THICK_WEIGHT_FACTOR = (1 << 7),
} eThickGpencil_Flag;

typedef struct TimeGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int layer_pass;
  /** Flags. */
  int flag;
  int offset;
  /** Animation scale. */
  float frame_scale;
  int mode;
  /** Start and end frame for custom range. */
  int sfra, efra;
  char _pad[4];
} TimeGpencilModifierData;

typedef enum eTimeGpencil_Flag {
  GP_TIME_INVERT_LAYER = (1 << 0),
  GP_TIME_KEEP_LOOP = (1 << 1),
  GP_TIME_INVERT_LAYERPASS = (1 << 2),
  GP_TIME_CUSTOM_RANGE = (1 << 3),
} eTimeGpencil_Flag;

typedef enum eTimeGpencil_Mode {
  GP_TIME_MODE_NORMAL = 0,
  GP_TIME_MODE_REVERSE = 1,
  GP_TIME_MODE_FIX = 2,
} eTimeGpencil_Mode;

typedef enum eModifyColorGpencil_Flag {
  GP_MODIFY_COLOR_BOTH = 0,
  GP_MODIFY_COLOR_STROKE = 1,
  GP_MODIFY_COLOR_FILL = 2,
  GP_MODIFY_COLOR_HARDNESS = 3,
} eModifyColorGpencil_Flag;

typedef enum eOpacityModesGpencil_Flag {
  GP_OPACITY_MODE_MATERIAL = 0,
  GP_OPACITY_MODE_STRENGTH = 1,
} eOpacityModesGpencil_Flag;

typedef struct ColorGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** HSV factors. */
  float hsv[3];
  /** Modify stroke, fill or both. */
  char modify_color;
  char _pad[3];
  /** Custom index for passes. */
  int layer_pass;

  char _pad1[4];
  struct CurveMapping *curve_intensity;
} ColorGpencilModifierData;

typedef enum eColorGpencil_Flag {
  GP_COLOR_INVERT_LAYER = (1 << 1),
  GP_COLOR_INVERT_PASS = (1 << 2),
  GP_COLOR_INVERT_LAYERPASS = (1 << 3),
  GP_COLOR_INVERT_MATERIAL = (1 << 4),
  GP_COLOR_CUSTOM_CURVE = (1 << 5),
} eColorGpencil_Flag;

typedef struct OpacityGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Main Opacity factor. */
  float factor;
  /** Modify stroke, fill or both. */
  char modify_color;
  char _pad[3];
  /** Custom index for passes. */
  int layer_pass;

  float hardeness;
  struct CurveMapping *curve_intensity;
} OpacityGpencilModifierData;

typedef enum eOpacityGpencil_Flag {
  GP_OPACITY_INVERT_LAYER = (1 << 0),
  GP_OPACITY_INVERT_PASS = (1 << 1),
  GP_OPACITY_INVERT_VGROUP = (1 << 2),
  GP_OPACITY_INVERT_LAYERPASS = (1 << 4),
  GP_OPACITY_INVERT_MATERIAL = (1 << 5),
  GP_OPACITY_CUSTOM_CURVE = (1 << 6),
  GP_OPACITY_NORMALIZE = (1 << 7),
  GP_OPACITY_WEIGHT_FACTOR = (1 << 8),
} eOpacityGpencil_Flag;

typedef struct ArrayGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Material for filtering. */
  struct Material *material;
  /** Number of elements in array. */
  int count;
  /** Several flags. */
  int flag;
  /** Location increments. */
  float offset[3];
  /** Shift increment. */
  float shift[3];
  /** Random Offset. */
  float rnd_offset[3];
  /** Random Rotation. */
  float rnd_rot[3];
  /** Random Scales. */
  float rnd_scale[3];
  char _pad[4];
  /** (first element is the index) random values. */
  int seed;

  /** Custom index for passes. */
  int pass_index;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Material replace (0 keep default). */
  int mat_rpl;
  /** Custom index for passes. */
  int layer_pass;
} ArrayGpencilModifierData;

typedef enum eArrayGpencil_Flag {
  GP_ARRAY_INVERT_LAYER = (1 << 2),
  GP_ARRAY_INVERT_PASS = (1 << 3),
  GP_ARRAY_INVERT_LAYERPASS = (1 << 5),
  GP_ARRAY_INVERT_MATERIAL = (1 << 6),
  GP_ARRAY_USE_OFFSET = (1 << 7),
  GP_ARRAY_USE_RELATIVE = (1 << 8),
  GP_ARRAY_USE_OB_OFFSET = (1 << 9),
  GP_ARRAY_UNIFORM_RANDOM_SCALE = (1 << 10),
} eArrayGpencil_Flag;

typedef struct BuildGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;

  /** If set, restrict modifier to operating on this layer. */
  char layername[64];
  int pass_index;

  /** Material name. */
  char materialname[64] DNA_DEPRECATED;

  /** Custom index for passes. */
  int layer_pass;

  /**
   * If GP_BUILD_RESTRICT_TIME is set,
   * the defines the frame range where GP frames are considered.
   */
  float start_frame;
  float end_frame;

  /** For each pair of gp keys, number of frames before strokes start appearing. */
  float start_delay;
  /** For each pair of gp keys, number of frames that build effect must be completed within. */
  float length;

  /** (eGpencilBuild_Flag) Options for controlling modifier behavior. */
  short flag;

  /** (eGpencilBuild_Mode) How are strokes ordered. */
  short mode;
  /** (eGpencilBuild_Transition) In what order do stroke points appear/disappear. */
  short transition;

  /**
   * (eGpencilBuild_TimeAlignment)
   * For the "Concurrent" mode, when should "shorter" strips start/end.
   */
  short time_alignment;
  /** Factor of the stroke (used instead of frame evaluation. */
  float percentage_fac;
  char _pad[4];
} BuildGpencilModifierData;

typedef enum eBuildGpencil_Mode {
  /* Strokes are shown one by one until all have appeared */
  GP_BUILD_MODE_SEQUENTIAL = 0,
  /* All strokes start at the same time */
  GP_BUILD_MODE_CONCURRENT = 1,
} eBuildGpencil_Mode;

typedef enum eBuildGpencil_Transition {
  /* Show in forward order */
  GP_BUILD_TRANSITION_GROW = 0,
  /* Hide in reverse order */
  GP_BUILD_TRANSITION_SHRINK = 1,
  /* Hide in forward order */
  GP_BUILD_TRANSITION_FADE = 2,
} eBuildGpencil_Transition;

typedef enum eBuildGpencil_TimeAlignment {
  /* All strokes start at same time */
  GP_BUILD_TIMEALIGN_START = 0,
  /* All strokes end at same time */
  GP_BUILD_TIMEALIGN_END = 1,

  /* TODO: Random Offsets, Stretch-to-Fill */
} eBuildGpencil_TimeAlignment;

typedef enum eBuildGpencil_Flag {
  /* Restrict modifier to particular layer/passes? */
  GP_BUILD_INVERT_LAYER = (1 << 0),
  GP_BUILD_INVERT_PASS = (1 << 1),

  /* Restrict modifier to only operating between the nominated frames */
  GP_BUILD_RESTRICT_TIME = (1 << 2),
  GP_BUILD_INVERT_LAYERPASS = (1 << 3),

  /* Use a percentage instead of frame number to evaluate strokes. */
  GP_BUILD_PERCENTAGE = (1 << 4),
} eBuildGpencil_Flag;

typedef struct LatticeGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  float strength;
  /** Custom index for passes. */
  int layer_pass;
  /** Runtime only. */
  struct LatticeDeformData *cache_data;
} LatticeGpencilModifierData;

typedef enum eLatticeGpencil_Flag {
  GP_LATTICE_INVERT_LAYER = (1 << 0),
  GP_LATTICE_INVERT_PASS = (1 << 1),
  GP_LATTICE_INVERT_VGROUP = (1 << 2),
  GP_LATTICE_INVERT_LAYERPASS = (1 << 3),
  GP_LATTICE_INVERT_MATERIAL = (1 << 4),
} eLatticeGpencil_Flag;

typedef struct LengthGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Custom index for passes. */
  int layer_pass;
  /** Length. */
  float start_fac, end_fac;
  /** Overshoot trajectory factor. */
  float overshoot_fac;
  /** Modifier mode. */
  int mode;
  /* Curvature parameters. */
  float point_density;
  float segment_influence;
  float max_angle;
} LengthGpencilModifierData;

typedef enum eLengthGpencil_Flag {
  GP_LENGTH_INVERT_LAYER = (1 << 0),
  GP_LENGTH_INVERT_PASS = (1 << 1),
  GP_LENGTH_INVERT_LAYERPASS = (1 << 2),
  GP_LENGTH_INVERT_MATERIAL = (1 << 3),
  GP_LENGTH_USE_CURVATURE = (1 << 4),
  GP_LENGTH_INVERT_CURVATURE = (1 << 5),
} eLengthGpencil_Flag;

typedef enum eLengthGpencil_Type {
  GP_LENGTH_RELATIVE = 0,
  GP_LENGTH_ABSOLUTE = 1,
} eLengthGpencil_Type;

typedef struct DashGpencilModifierSegment {
  char name[64];
  /* For path reference. */
  struct DashGpencilModifierData *dmd;
  int dash;
  int gap;
  float radius;
  float opacity;
  int mat_nr;
  int _pad;
} DashGpencilModifierSegment;

typedef struct DashGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Custom index for passes. */
  int layer_pass;

  int dash_offset;

  DashGpencilModifierSegment *segments;
  int segments_len;
  int segment_active_index;

} DashGpencilModifierData;

typedef struct MirrorGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Custom index for passes. */
  int layer_pass;
  char _pad[4];
} MirrorGpencilModifierData;

typedef enum eMirrorGpencil_Flag {
  GP_MIRROR_INVERT_LAYER = (1 << 0),
  GP_MIRROR_INVERT_PASS = (1 << 1),
  GP_MIRROR_CLIPPING = (1 << 2),
  GP_MIRROR_AXIS_X = (1 << 3),
  GP_MIRROR_AXIS_Y = (1 << 4),
  GP_MIRROR_AXIS_Z = (1 << 5),
  GP_MIRROR_INVERT_LAYERPASS = (1 << 6),
  GP_MIRROR_INVERT_MATERIAL = (1 << 7),
} eMirrorGpencil_Flag;

typedef struct HookGpencilModifierData {
  GpencilModifierData modifier;

  struct Object *object;
  /** Material for filtering. */
  struct Material *material;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char subtarget[64];
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Custom index for passes. */
  int layer_pass;
  char _pad[4];

  int flag;
  /** Use enums from WarpGpencilModifier (exact same functionality). */
  char falloff_type;
  char _pad1[3];
  /** Matrix making current transform unmodified. */
  float parentinv[4][4];
  /** Visualization of hook. */
  float cent[3];
  /** If not zero, falloff is distance where influence zero. */
  float falloff;
  float force;
  struct CurveMapping *curfalloff;
} HookGpencilModifierData;

typedef enum eHookGpencil_Flag {
  GP_HOOK_INVERT_LAYER = (1 << 0),
  GP_HOOK_INVERT_PASS = (1 << 1),
  GP_HOOK_INVERT_VGROUP = (1 << 2),
  GP_HOOK_UNIFORM_SPACE = (1 << 3),
  GP_HOOK_INVERT_LAYERPASS = (1 << 4),
  GP_HOOK_INVERT_MATERIAL = (1 << 5),
} eHookGpencil_Flag;

typedef enum eHookGpencil_Falloff {
  eGPHook_Falloff_None = 0,
  eGPHook_Falloff_Curve = 1,
  eGPHook_Falloff_Sharp = 2,
  eGPHook_Falloff_Smooth = 3,
  eGPHook_Falloff_Root = 4,
  eGPHook_Falloff_Linear = 5,
  eGPHook_Falloff_Const = 6,
  eGPHook_Falloff_Sphere = 7,
  eGPHook_Falloff_InvSquare = 8,
} eHookGpencil_Falloff;

typedef struct SimplifyGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Factor of simplify. */
  float factor;
  /** Type of simplify. */
  short mode;
  /** Every n vertex to keep. */
  short step;
  /** Custom index for passes. */
  int layer_pass;
  /** Sample length */
  float length;
  /** Merge distance */
  float distance;
  char _pad[4];
} SimplifyGpencilModifierData;

typedef enum eSimplifyGpencil_Flag {
  GP_SIMPLIFY_INVERT_LAYER = (1 << 0),
  GP_SIMPLIFY_INVERT_PASS = (1 << 1),
  GP_SIMPLIFY_INVERT_LAYERPASS = (1 << 2),
  GP_SIMPLIFY_INVERT_MATERIAL = (1 << 3),
} eSimplifyGpencil_Flag;

typedef enum eSimplifyGpencil_Mode {
  /* Keep only one vertex every n vertices */
  GP_SIMPLIFY_FIXED = 0,
  /* Use RDP algorithm */
  GP_SIMPLIFY_ADAPTIVE = 1,
  /* Sample the stroke using a fixed length */
  GP_SIMPLIFY_SAMPLE = 2,
  /* Sample the stroke doing vertex merge */
  GP_SIMPLIFY_MERGE = 3,
} eSimplifyGpencil_Mode;

typedef struct OffsetGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  float loc[3];
  float rot[3];
  float scale[3];
  /** Random Offset. */
  float rnd_offset[3];
  /** Random Rotation. */
  float rnd_rot[3];
  /** Random Scales. */
  float rnd_scale[3];
  /** (first element is the index) random values. */
  int seed;
  /** Custom index for passes. */
  int layer_pass;
} OffsetGpencilModifierData;

typedef enum eOffsetGpencil_Flag {
  GP_OFFSET_INVERT_LAYER = (1 << 0),
  GP_OFFSET_INVERT_PASS = (1 << 1),
  GP_OFFSET_INVERT_VGROUP = (1 << 2),
  GP_OFFSET_INVERT_LAYERPASS = (1 << 3),
  GP_OFFSET_INVERT_MATERIAL = (1 << 4),
  GP_OFFSET_UNIFORM_RANDOM_SCALE = (1 << 5),
} eOffsetGpencil_Flag;

typedef struct SmoothGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Several flags. */
  int flag;
  /** Factor of noise. */
  float factor;
  /** How many times apply smooth. */
  int step;
  /** Custom index for passes. */
  int layer_pass;

  char _pad1[4];
  struct CurveMapping *curve_intensity;
} SmoothGpencilModifierData;

typedef enum eSmoothGpencil_Flag {
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
} eSmoothGpencil_Flag;

typedef struct ArmatureGpencilModifierData {
  GpencilModifierData modifier;
  /** #eArmature_DeformFlag use instead of #bArmature.deformflag. */
  short deformflag, multi;
  int _pad;
  struct Object *object;
  /** Stored input of previous modifier, for vertex-group blending. */
  float (*vert_coords_prev)[3];
  /** MAX_VGROUP_NAME. */
  char vgname[64];

} ArmatureGpencilModifierData;

typedef struct MultiplyGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Custom index for passes. */
  int layer_pass;

  int flags;

  int duplications;
  float distance;
  /* -1:inner 0:middle 1:outer */
  float offset;

  float fading_center;
  float fading_thickness;
  float fading_opacity;

} MultiplyGpencilModifierData;

typedef enum eMultiplyGpencil_Flag {
  /* GP_MULTIPLY_ENABLE_ANGLE_SPLITTING = (1 << 1),  Deprecated. */
  GP_MULTIPLY_ENABLE_FADING = (1 << 2),
} eMultiplyGpencil_Flag;

typedef struct TintGpencilModifierData {
  GpencilModifierData modifier;

  struct Object *object;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Custom index for passes. */
  int layer_pass;
  /** Flags. */
  int flag;
  /** Mode (Stroke/Fill/Both). */
  int mode;

  float factor;
  float radius;
  /** Simple Tint color. */
  float rgb[3];
  /** Type of Tint. */
  int type;

  struct CurveMapping *curve_intensity;

  struct ColorBand *colorband;
} TintGpencilModifierData;

typedef enum eTintGpencil_Type {
  GP_TINT_UNIFORM = 0,
  GP_TINT_GRADIENT = 1,
} eTintGpencil_Type;

typedef enum eTintGpencil_Flag {
  GP_TINT_INVERT_LAYER = (1 << 0),
  GP_TINT_INVERT_PASS = (1 << 1),
  GP_TINT_INVERT_VGROUP = (1 << 2),
  GP_TINT_INVERT_LAYERPASS = (1 << 4),
  GP_TINT_INVERT_MATERIAL = (1 << 5),
  GP_TINT_CUSTOM_CURVE = (1 << 6),
  GP_TINT_WEIGHT_FACTOR = (1 << 7),
} eTintGpencil_Flag;

typedef struct TextureGpencilModifierData {
  GpencilModifierData modifier;
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Material name. */
  char materialname[64] DNA_DEPRECATED;
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Offset value to add to uv_fac. */
  float uv_offset;
  float uv_scale;
  float fill_rotation;
  float fill_offset[2];
  float fill_scale;
  /** Custom index for passes. */
  int layer_pass;
  /** Texture fit options. */
  short fit_method;
  short mode;
  /** Dot texture rotation */
  float alignment_rotation;
  char _pad[4];
} TextureGpencilModifierData;

typedef enum eTextureGpencil_Flag {
  GP_TEX_INVERT_LAYER = (1 << 0),
  GP_TEX_INVERT_PASS = (1 << 1),
  GP_TEX_INVERT_VGROUP = (1 << 2),
  GP_TEX_INVERT_LAYERPASS = (1 << 3),
  GP_TEX_INVERT_MATERIAL = (1 << 4),
} eTextureGpencil_Flag;

/* Texture->fit_method */
typedef enum eTextureGpencil_Fit {
  GP_TEX_FIT_STROKE = 0,
  GP_TEX_CONSTANT_LENGTH = 1,
} eTextureGpencil_Fit;

/* Texture->mode */
typedef enum eTextureGpencil_Mode {
  STROKE = 0,
  FILL = 1,
  STROKE_AND_FILL = 2,
} eTextureGpencil_Mode;

typedef struct WeightProxGpencilModifierData {
  GpencilModifierData modifier;
  /** Target vertexgroup name, MAX_VGROUP_NAME. */
  char target_vgname[64];
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup filter name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Minimum valid weight (clamp value). */
  float min_weight;
  /** Custom index for passes. */
  int layer_pass;
  /** Start/end distances. */
  float dist_start;
  float dist_end;

  /** Reference object */
  struct Object *object;
} WeightProxGpencilModifierData;

typedef struct WeightAngleGpencilModifierData {
  GpencilModifierData modifier;
  /** Target vertexgroup name, MAX_VGROUP_NAME. */
  char target_vgname[64];
  /** Material for filtering. */
  struct Material *material;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup filter name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Minimum valid weight (clamp value). */
  float min_weight;
  /** Custom index for passes. */
  int layer_pass;
  /** Axis. */
  short axis;
  /** Space (Local/World). */
  short space;
  /** Angle */
  float angle;
} WeightAngleGpencilModifierData;

typedef enum eWeightGpencil_Flag {
  GP_WEIGHT_INVERT_LAYER = (1 << 0),
  GP_WEIGHT_INVERT_PASS = (1 << 1),
  GP_WEIGHT_INVERT_VGROUP = (1 << 2),
  GP_WEIGHT_INVERT_LAYERPASS = (1 << 3),
  GP_WEIGHT_INVERT_MATERIAL = (1 << 4),
  GP_WEIGHT_MULTIPLY_DATA = (1 << 5),
  GP_WEIGHT_INVERT_OUTPUT = (1 << 6),
} eWeightGpencil_Flag;

typedef enum eGpencilModifierSpace {
  GP_SPACE_LOCAL = 0,
  GP_SPACE_WORLD = 1,
} eGpencilModifierSpace;

typedef enum eLineartGpencilModifierSource {
  LRT_SOURCE_COLLECTION = 0,
  LRT_SOURCE_OBJECT = 1,
  LRT_SOURCE_SCENE = 2,
} eLineartGpencilModifierSource;

/* This enum is for modifier internal state only. */
typedef enum eLineArtGPencilModifierFlags {
  /* These two moved to #eLineartMainFlags to keep consistent with flag variable purpose. */
  /* LRT_GPENCIL_INVERT_SOURCE_VGROUP = (1 << 0), */
  /* LRT_GPENCIL_MATCH_OUTPUT_VGROUP = (1 << 1), */
  LRT_GPENCIL_BINARY_WEIGHTS = (1 << 2) /* Deprecated, this is removed for lack of use case. */,
  LRT_GPENCIL_IS_BAKED = (1 << 3),
  LRT_GPENCIL_USE_CACHE = (1 << 4),
  LRT_GPENCIL_OFFSET_TOWARDS_CUSTOM_CAMERA = (1 << 5),
} eLineArtGPencilModifierFlags;

typedef enum eLineartGpencilMaskSwitches {
  LRT_GPENCIL_MATERIAL_MASK_ENABLE = (1 << 0),
  /** When set, material mask bit comparisons are done with bit wise "AND" instead of "OR". */
  LRT_GPENCIL_MATERIAL_MASK_MATCH = (1 << 1),
  LRT_GPENCIL_INTERSECTION_MATCH = (1 << 2),
} eLineartGpencilMaskSwitches;

struct LineartCache;

struct LineartCache;

typedef struct LineartGpencilModifierData {
  GpencilModifierData modifier;

  /** Line type enable flags, bits in #eLineartEdgeFlag. */
  short edge_types;

  /** Object or Collection, from #eLineartGpencilModifierSource. */
  char source_type;

  char use_multiple_levels;
  short level_start;
  short level_end;

  struct Object *source_camera;

  struct Object *source_object;
  struct Collection *source_collection;

  struct Material *target_material;
  char target_layer[64];

  /**
   * These two variables are to pass on vertex group information from mesh to strokes.
   * `vgname` specifies which vertex groups our strokes from source_vertex_group will go to.
   */
  char source_vertex_group[64];
  char vgname[64];

  /**
   * Camera focal length is divided by `1 + overscan`, before calculation, which give a wider FOV,
   * this doesn't change coordinates range internally (-1, 1), but makes the calculated frame
   * bigger than actual output. This is for the easier shifting calculation. A value of 0.5 means
   * the "internal" focal length become 2/3 of the actual camera.
   */
  float overscan;

  float opacity;
  short thickness;

  unsigned char mask_switches; /* #eLineartGpencilMaskSwitches */
  unsigned char material_mask_bits;
  unsigned char intersection_mask;

  char _pad[3];

  /** `0..1` range for cosine angle */
  float crease_threshold;

  /** `0..PI` angle, for splitting strokes at sharp points. */
  float angle_splitting_threshold;

  /** Strength for smoothing jagged chains. */
  float chain_smooth_tolerance;

  /* CPU mode */
  float chaining_image_threshold;

  /* Ported from SceneLineArt flags. */
  int calculation_flags;

  /* #eLineArtGPencilModifierFlags, modifier internal state. */
  int flags;

  /* Move strokes towards camera to avoid clipping while preserve depth for the viewport. */
  float stroke_depth_offset;

  /* Runtime data. */

  /* Because we can potentially only compute features lines once per modifier stack (Use Cache), we
   * need to have these override values to ensure that we have the data we need is computed and
   * stored in the cache. */
  char level_start_override;
  char level_end_override;
  short edge_types_override;

  struct LineartCache *cache;
  /* Keep a pointer to the render buffer so we can call destroy from ModifierData. */
  struct LineartRenderBuffer *render_buffer_ptr;

} LineartGpencilModifierData;

#ifdef __cplusplus
}
#endif
