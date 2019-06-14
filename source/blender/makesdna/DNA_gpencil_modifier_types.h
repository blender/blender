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

#ifndef __DNA_GPENCIL_MODIFIER_TYPES_H__
#define __DNA_GPENCIL_MODIFIER_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

/* WARNING ALERT! TYPEDEF VALUES ARE WRITTEN IN FILES! SO DO NOT CHANGE!
 * (ONLY ADD NEW ITEMS AT THE END)
 */

struct RNG;

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
  NUM_GREASEPENCIL_MODIFIER_TYPES,
} GpencilModifierType;

typedef enum GpencilModifierMode {
  eGpencilModifierMode_Realtime = (1 << 0),
  eGpencilModifierMode_Render = (1 << 1),
  eGpencilModifierMode_Editmode = (1 << 2),
  eGpencilModifierMode_Expanded = (1 << 3),
} GpencilModifierMode;

typedef enum {
  /* This modifier has been inserted in local override, and hence can be fully edited. */
  eGpencilModifierFlag_OverrideLibrary_Local = (1 << 0),
} GpencilModifierFlag;

typedef struct GpencilModifierData {
  struct GpencilModifierData *next, *prev;

  int type, mode;
  int stackindex;
  short flag;
  short _pad;
  /** MAX_NAME. */
  char name[64];

  char *error;
} GpencilModifierData;

typedef struct NoiseGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Several flags. */
  int flag;
  /** Factor of noise. */
  float factor;
  /** How many frames before recalculate randoms. */
  int step;
  /** Last gp frame used. */
  int gp_frame;
  /** Last scene frame used. */
  int scene_frame;
  /** Random values. */
  float vrand1, vrand2;
  struct RNG *rng;
  /** Custom index for passes. */
  int layer_pass;
  char _pad[4];
} NoiseGpencilModifierData;

typedef enum eNoiseGpencil_Flag {
  GP_NOISE_USE_RANDOM = (1 << 0),
  GP_NOISE_MOD_LOCATION = (1 << 1),
  GP_NOISE_MOD_STRENGTH = (1 << 2),
  GP_NOISE_MOD_THICKNESS = (1 << 3),
  GP_NOISE_FULL_STROKE = (1 << 4),
  GP_NOISE_MOVE_EXTREME = (1 << 5),
  GP_NOISE_INVERT_LAYER = (1 << 6),
  GP_NOISE_INVERT_PASS = (1 << 7),
  GP_NOISE_INVERT_VGROUP = (1 << 8),
  GP_NOISE_MOD_UV = (1 << 9),
  GP_NOISE_INVERT_LAYERPASS = (1 << 10),
} eNoiseGpencil_Flag;

typedef struct SubdivGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Factor of subdivision. */
  int level;
  /** Custom index for passes. */
  int layer_pass;
} SubdivGpencilModifierData;

typedef enum eSubdivGpencil_Flag {
  GP_SUBDIV_SIMPLE = (1 << 0),
  GP_SUBDIV_INVERT_LAYER = (1 << 1),
  GP_SUBDIV_INVERT_PASS = (1 << 2),
  GP_SUBDIV_INVERT_LAYERPASS = (1 << 3),
} eSubdivGpencil_Flag;

typedef struct ThickGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Thickness change. */
  int thickness;
  /** Custom index for passes. */
  int layer_pass;
  struct CurveMapping *curve_thickness;
} ThickGpencilModifierData;

typedef enum eThickGpencil_Flag {
  GP_THICK_INVERT_LAYER = (1 << 0),
  GP_THICK_INVERT_PASS = (1 << 1),
  GP_THICK_INVERT_VGROUP = (1 << 2),
  GP_THICK_CUSTOM_CURVE = (1 << 3),
  GP_THICK_NORMALIZE = (1 << 4),
  GP_THICK_INVERT_LAYERPASS = (1 << 5),
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
} eModifyColorGpencil_Flag;

typedef struct TintGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Tint color. */
  float rgb[3];
  /** Mix factor. */
  float factor;
  /** Modify stroke, fill or both. */
  char modify_color;
  char _pad[7];
  /** Custom index for passes. */
  int layer_pass;
  char _pad1[4];
} TintGpencilModifierData;

typedef enum eTintGpencil_Flag {
  GP_TINT_CREATE_COLORS = (1 << 0),
  GP_TINT_INVERT_LAYER = (1 << 1),
  GP_TINT_INVERT_PASS = (1 << 2),
  GP_TINT_INVERT_LAYERPASS = (1 << 3),
} eTintGpencil_Flag;

typedef struct ColorGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  /** Hsv factors. */
  float hsv[3];
  /** Modify stroke, fill or both. */
  char modify_color;
  char _pad[3];
  /** Custom index for passes. */
  int layer_pass;
  char _pad1[4];
} ColorGpencilModifierData;

typedef enum eColorGpencil_Flag {
  GP_COLOR_CREATE_COLORS = (1 << 0),
  GP_COLOR_INVERT_LAYER = (1 << 1),
  GP_COLOR_INVERT_PASS = (1 << 2),
  GP_COLOR_INVERT_LAYERPASS = (1 << 3),
} eColorGpencil_Flag;

typedef struct OpacityGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
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
  char _pad1[4];
} OpacityGpencilModifierData;

typedef enum eOpacityGpencil_Flag {
  GP_OPACITY_INVERT_LAYER = (1 << 0),
  GP_OPACITY_INVERT_PASS = (1 << 1),
  GP_OPACITY_INVERT_VGROUP = (1 << 2),
  GP_OPACITY_CREATE_COLORS = (1 << 3),
  GP_OPACITY_INVERT_LAYERPASS = (1 << 4),
} eOpacityGpencil_Flag;

typedef struct ArrayGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Number of elements in array. */
  int count;
  /** Several flags. */
  int flag;
  /** Location increments. */
  float offset[3];
  /** Shift increment. */
  float shift[3];
  /** Random size factor. */
  float rnd_size;
  /** Random size factor. */
  float rnd_rot;
  /** Rotation changes. */
  float rot[3];
  /** Scale changes. */
  float scale[3];
  /** (first element is the index) random values. */
  float rnd[20];
  char _pad[4];

  /** Custom index for passes. */
  int pass_index;
  /** Layer name. */
  char layername[64];
  /** Material replace (0 keep default). */
  int mat_rpl;
  /** Custom index for passes. */
  int layer_pass;
} ArrayGpencilModifierData;

typedef enum eArrayGpencil_Flag {
  GP_ARRAY_RANDOM_SIZE = (1 << 0),
  GP_ARRAY_RANDOM_ROT = (1 << 1),
  GP_ARRAY_INVERT_LAYER = (1 << 2),
  GP_ARRAY_INVERT_PASS = (1 << 3),
  GP_ARRAY_KEEP_ONTOP = (1 << 4),
  GP_ARRAY_INVERT_LAYERPASS = (1 << 5),
} eArrayGpencil_Flag;

typedef struct BuildGpencilModifierData {
  GpencilModifierData modifier;

  /** If set, restrict modifier to operating on this layer. */
  char layername[64];
  int pass_index;

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
} eBuildGpencil_Flag;

typedef struct LatticeGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  float strength;
  /** Custom index for passes. */
  int layer_pass;
  /** Runtime only (LatticeDeformData). */
  void *cache_data;
} LatticeGpencilModifierData;

typedef enum eLatticeGpencil_Flag {
  GP_LATTICE_INVERT_LAYER = (1 << 0),
  GP_LATTICE_INVERT_PASS = (1 << 1),
  GP_LATTICE_INVERT_VGROUP = (1 << 2),
  GP_LATTICE_INVERT_LAYERPASS = (1 << 3),
} eLatticeGpencil_Flag;

typedef struct MirrorGpencilModifierData {
  GpencilModifierData modifier;
  struct Object *object;
  /** Layer name. */
  char layername[64];
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
} eMirrorGpencil_Flag;

typedef struct HookGpencilModifierData {
  GpencilModifierData modifier;

  struct Object *object;
  /** Optional name of bone target, MAX_ID_NAME-2. */
  char subtarget[64];
  /** Layer name. */
  char layername[64];
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
  /** Layer name. */
  char layername[64];
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
  char _pad[4];
} SimplifyGpencilModifierData;

typedef enum eSimplifyGpencil_Flag {
  GP_SIMPLIFY_INVERT_LAYER = (1 << 0),
  GP_SIMPLIFY_INVERT_PASS = (1 << 1),
  GP_SIMPLIFY_INVERT_LAYERPASS = (1 << 2),
} eSimplifyGpencil_Flag;

typedef enum eSimplifyGpencil_Mode {
  /* Keep only one vertex every n vertices */
  GP_SIMPLIFY_FIXED = 0,
  /* Use RDP algorithm */
  GP_SIMPLIFY_ADAPTIVE = 1,
} eSimplifyGpencil_Mode;

typedef struct OffsetGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
  /** Optional vertexgroup name, MAX_VGROUP_NAME. */
  char vgname[64];
  /** Custom index for passes. */
  int pass_index;
  /** Flags. */
  int flag;
  float loc[3];
  float rot[3];
  float scale[3];
  /** Custom index for passes. */
  int layer_pass;
} OffsetGpencilModifierData;

typedef enum eOffsetGpencil_Flag {
  GP_OFFSET_INVERT_LAYER = (1 << 0),
  GP_OFFSET_INVERT_PASS = (1 << 1),
  GP_OFFSET_INVERT_VGROUP = (1 << 2),
  GP_OFFSET_INVERT_LAYERPASS = (1 << 3),
} eOffsetGpencil_Flag;

typedef struct SmoothGpencilModifierData {
  GpencilModifierData modifier;
  /** Layer name. */
  char layername[64];
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
  char _pad[4];
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
} eSmoothGpencil_Flag;

typedef struct ArmatureGpencilModifierData {
  GpencilModifierData modifier;
  /** Deformflag replaces armature->deformflag. */
  short deformflag, multi;
  int _pad;
  struct Object *object;
  /** Stored input of previous modifier, for vertexgroup blending. */
  float *prevCos;
  /** MAX_VGROUP_NAME. */
  char vgname[64];

} ArmatureGpencilModifierData;

#endif /* __DNA_GPENCIL_MODIFIER_TYPES_H__ */
