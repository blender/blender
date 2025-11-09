/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/* BrushGpencilSettings->preset_type.
 * Use a range for each group and not continuous values. */
typedef enum eGPBrush_Presets {
  GP_BRUSH_PRESET_UNKNOWN = 0,

  /* Draw 1-99. */
  GP_BRUSH_PRESET_AIRBRUSH = 1,
  GP_BRUSH_PRESET_INK_PEN = 2,
  GP_BRUSH_PRESET_INK_PEN_ROUGH = 3,
  GP_BRUSH_PRESET_MARKER_BOLD = 4,
  GP_BRUSH_PRESET_MARKER_CHISEL = 5,
  GP_BRUSH_PRESET_PEN = 6,
  GP_BRUSH_PRESET_PENCIL_SOFT = 7,
  GP_BRUSH_PRESET_PENCIL = 8,
  GP_BRUSH_PRESET_FILL_AREA = 9,
  GP_BRUSH_PRESET_ERASER_SOFT = 10,
  GP_BRUSH_PRESET_ERASER_HARD = 11,
  GP_BRUSH_PRESET_ERASER_POINT = 12,
  GP_BRUSH_PRESET_ERASER_STROKE = 13,
  GP_BRUSH_PRESET_TINT = 14,

  /* Vertex Paint 100-199. */
  GP_BRUSH_PRESET_VERTEX_DRAW = 100,
  GP_BRUSH_PRESET_VERTEX_BLUR = 101,
  GP_BRUSH_PRESET_VERTEX_AVERAGE = 102,
  GP_BRUSH_PRESET_VERTEX_SMEAR = 103,
  GP_BRUSH_PRESET_VERTEX_REPLACE = 104,

  /* Sculpt 200-299. */
  GP_BRUSH_PRESET_SMOOTH_STROKE = 200,
  GP_BRUSH_PRESET_STRENGTH_STROKE = 201,
  GP_BRUSH_PRESET_THICKNESS_STROKE = 202,
  GP_BRUSH_PRESET_GRAB_STROKE = 203,
  GP_BRUSH_PRESET_PUSH_STROKE = 204,
  GP_BRUSH_PRESET_TWIST_STROKE = 205,
  GP_BRUSH_PRESET_PINCH_STROKE = 206,
  GP_BRUSH_PRESET_RANDOMIZE_STROKE = 207,
  GP_BRUSH_PRESET_CLONE_STROKE = 208,

  /* Weight Paint 300-399. */
  GP_BRUSH_PRESET_WEIGHT_DRAW = 300,
  GP_BRUSH_PRESET_WEIGHT_BLUR = 301,
  GP_BRUSH_PRESET_WEIGHT_AVERAGE = 302,
  GP_BRUSH_PRESET_WEIGHT_SMEAR = 303,
} eGPBrush_Presets;

/* BrushGpencilSettings->flag */
typedef enum eGPDbrush_Flag {
  /* brush use pressure */
  GP_BRUSH_USE_PRESSURE = (1 << 0),
  /* brush use pressure for alpha factor */
  GP_BRUSH_USE_STRENGTH_PRESSURE = (1 << 1),
  /* brush use pressure for alpha factor */
  GP_BRUSH_USE_JITTER_PRESSURE = (1 << 2),
  /* Disable automatic zoom for filling. */
  GP_BRUSH_FILL_FIT_DISABLE = (1 << 3),
  /* Show extend fill help lines. */
  GP_BRUSH_FILL_SHOW_EXTENDLINES = (1 << 4),
  /* fill hide transparent */
  GP_BRUSH_FILL_HIDE = (1 << 6),
  /* show fill help lines */
  GP_BRUSH_FILL_SHOW_HELPLINES = (1 << 7),
  /* lazy mouse */
  GP_BRUSH_STABILIZE_MOUSE = (1 << 8),
  /* lazy mouse override (internal only) */
  GP_BRUSH_STABILIZE_MOUSE_TEMP = (1 << 9),
  /* deprecated, was default eraser brush for quick switch */
  GP_BRUSH_DEPRECATED1 = (1 << 10),
  /* settings group */
  GP_BRUSH_GROUP_SETTINGS = (1 << 11),
  /* Random settings group */
  GP_BRUSH_GROUP_RANDOM = (1 << 12),
  /* Keep material assigned to brush */
  GP_BRUSH_MATERIAL_PINNED = (1 << 13),
  /* Do not show fill color while drawing (no lasso mode) */
  GP_BRUSH_DISSABLE_LASSO = (1 << 14),
  /* Do not erase strokes occluded. */
  GP_BRUSH_OCCLUDE_ERASER = (1 << 15),
  /* Post process trim stroke */
  GP_BRUSH_TRIM_STROKE = (1 << 16),
  /* Post process convert to outline stroke */
  GP_BRUSH_OUTLINE_STROKE = (1 << 17),
  /* Collide with stroke. */
  GP_BRUSH_FILL_STROKE_COLLIDE = (1 << 18),
  /* Keep the caps as they are when erasing. Otherwise flatten the caps. */
  GP_BRUSH_ERASER_KEEP_CAPS = (1 << 19),
  /* Affect only the drawing in the active layer.
   * Otherwise affect all editable drawings in the object. */
  GP_BRUSH_ACTIVE_LAYER_ONLY = (1 << 20),
  /* Automatically remove fill guides created with fill tool. */
  GP_BRUSH_FILL_AUTO_REMOVE_FILL_GUIDES = (1 << 21),
} eGPDbrush_Flag;

typedef enum eGPDbrush_Flag2 {
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_HUE_AT_STROKE */
  /* Brush use random Hue at stroke level */
  GP_BRUSH_USE_HUE_AT_STROKE = (1 << 0),
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_SAT_AT_STROKE */
  /* Brush use random Saturation at stroke level */
  GP_BRUSH_USE_SAT_AT_STROKE = (1 << 1),
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_VAL_AT_STROKE */
  /* Brush use random Value at stroke level */
  GP_BRUSH_USE_VAL_AT_STROKE = (1 << 2),
  /* Brush use random Pressure at stroke level */
  GP_BRUSH_USE_PRESS_AT_STROKE = (1 << 3),
  /* Brush use random Strength at stroke level */
  GP_BRUSH_USE_STRENGTH_AT_STROKE = (1 << 4),
  /* Brush use random UV at stroke level */
  GP_BRUSH_USE_UV_AT_STROKE = (1 << 5),
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_HUE_RAND_PRESS */
  /* Brush use Hue random pressure */
  GP_BRUSH_USE_HUE_RAND_PRESS = (1 << 6),
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_SAT_RAND_PRESS */
  /* Brush use Saturation random pressure */
  GP_BRUSH_USE_SAT_RAND_PRESS = (1 << 7),
  /* DEPRECATED: replaced with BRUSH_COLOR_JITTER_USE_VAL_RAND_PRESS */
  /* Brush use Value random pressure */
  GP_BRUSH_USE_VAL_RAND_PRESS = (1 << 8),
  /* Brush use Pressure random pressure */
  GP_BRUSH_USE_PRESSURE_RAND_PRESS = (1 << 9),
  /* Brush use Strength random pressure */
  GP_BRUSH_USE_STRENGTH_RAND_PRESS = (1 << 10),
  /* Brush use UV random pressure */
  GP_BRUSH_USE_UV_RAND_PRESS = (1 << 11),
} eGPDbrush_Flag2;

/* BrushGpencilSettings->fill_draw_mode */
typedef enum eGP_FillDrawModes {
  GP_FILL_DMODE_BOTH = 0,
  GP_FILL_DMODE_STROKE = 1,
  GP_FILL_DMODE_CONTROL = 2,
} eGP_FillDrawModes;

/* BrushGpencilSettings->fill_extend_mode */
typedef enum eGP_FillExtendModes {
  GP_FILL_EMODE_EXTEND = 0,
  GP_FILL_EMODE_RADIUS = 1,
} eGP_FillExtendModes;

/* BrushGpencilSettings->fill_layer_mode */
typedef enum eGP_FillLayerModes {
  GP_FILL_GPLMODE_VISIBLE = 0,
  GP_FILL_GPLMODE_ACTIVE = 1,
  GP_FILL_GPLMODE_ALL_ABOVE = 2,
  GP_FILL_GPLMODE_ALL_BELOW = 3,
  GP_FILL_GPLMODE_ABOVE = 4,
  GP_FILL_GPLMODE_BELOW = 5,
} eGP_FillLayerModes;

/* BrushGpencilSettings->gp_eraser_mode */
typedef enum eGP_BrushEraserMode {
  GP_BRUSH_ERASER_SOFT = 0,
  GP_BRUSH_ERASER_HARD = 1,
  GP_BRUSH_ERASER_STROKE = 2,
} eGP_BrushEraserMode;

/* BrushGpencilSettings->brush_draw_mode */
typedef enum eGP_BrushMode {
  GP_BRUSH_MODE_ACTIVE = 0,
  GP_BRUSH_MODE_MATERIAL = 1,
  GP_BRUSH_MODE_VERTEXCOLOR = 2,
} eGP_BrushMode;

/* Brush.curve_preset */
typedef enum eBrushCurvePreset {
  BRUSH_CURVE_CUSTOM = 0,
  /** Corresponds to CURVE_PRESET_SMOOTH */
  BRUSH_CURVE_SMOOTH = 1,
  /** Corresponds to CURVE_PRESET_ROUND */
  BRUSH_CURVE_SPHERE = 2,
  /** Corresponds to CURVE_PRESET_ROOT */
  BRUSH_CURVE_ROOT = 3,
  /** Corresponds to CURVE_PRESET_SHARP */
  BRUSH_CURVE_SHARP = 4,
  /** Corresponds to CURVE_PRESET_LINE */
  BRUSH_CURVE_LIN = 5,
  /** No corresponding CurveMapping.preset */
  BRUSH_CURVE_POW4 = 6,
  /** No corresponding CurveMapping.preset */
  BRUSH_CURVE_INVSQUARE = 7,
  /** Corresponds to CURVE_PRESET_MAX */
  BRUSH_CURVE_CONSTANT = 8,
  /** No corresponding CurveMapping.preset */
  BRUSH_CURVE_SMOOTHER = 9,
} eBrushCurvePreset;

typedef enum eBrushDeformTarget {
  BRUSH_DEFORM_TARGET_GEOMETRY = 0,
  BRUSH_DEFORM_TARGET_CLOTH_SIM = 1,
} eBrushDeformTarget;

typedef enum eBrushElasticDeformType {
  BRUSH_ELASTIC_DEFORM_GRAB = 0,
  BRUSH_ELASTIC_DEFORM_GRAB_BISCALE = 1,
  BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE = 2,
  BRUSH_ELASTIC_DEFORM_SCALE = 3,
  BRUSH_ELASTIC_DEFORM_TWIST = 4,
} eBrushElasticDeformType;

typedef enum eBrushClothDeformType {
  BRUSH_CLOTH_DEFORM_DRAG = 0,
  BRUSH_CLOTH_DEFORM_PUSH = 1,
  BRUSH_CLOTH_DEFORM_GRAB = 2,
  BRUSH_CLOTH_DEFORM_PINCH_POINT = 3,
  BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR = 4,
  BRUSH_CLOTH_DEFORM_INFLATE = 5,
  BRUSH_CLOTH_DEFORM_EXPAND = 6,
  BRUSH_CLOTH_DEFORM_SNAKE_HOOK = 7,
} eBrushClothDeformType;

typedef enum eBrushSmoothDeformType {
  BRUSH_SMOOTH_DEFORM_LAPLACIAN = 0,
  BRUSH_SMOOTH_DEFORM_SURFACE = 1,
} eBrushSmoothDeformType;

typedef enum eBrushClothForceFalloffType {
  BRUSH_CLOTH_FORCE_FALLOFF_RADIAL = 0,
  BRUSH_CLOTH_FORCE_FALLOFF_PLANE = 1,
} eBrushClothForceFalloffType;

typedef enum eBrushClothSimulationAreaType {
  BRUSH_CLOTH_SIMULATION_AREA_LOCAL = 0,
  BRUSH_CLOTH_SIMULATION_AREA_GLOBAL = 1,
  BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC = 2,
} eBrushClothSimulationAreaType;

typedef enum eBrushPoseDeformType {
  BRUSH_POSE_DEFORM_ROTATE_TWIST = 0,
  BRUSH_POSE_DEFORM_SCALE_TRASLATE = 1,
  BRUSH_POSE_DEFORM_SQUASH_STRETCH = 2,
} eBrushPoseDeformType;

typedef enum eBrushPoseOriginType {
  BRUSH_POSE_ORIGIN_TOPOLOGY = 0,
  BRUSH_POSE_ORIGIN_FACE_SETS = 1,
  BRUSH_POSE_ORIGIN_FACE_SETS_FK = 2,
} eBrushPoseOriginType;

typedef enum eBrushSmearDeformType {
  BRUSH_SMEAR_DEFORM_DRAG = 0,
  BRUSH_SMEAR_DEFORM_PINCH = 1,
  BRUSH_SMEAR_DEFORM_EXPAND = 2,
} eBrushSmearDeformType;

typedef enum eBrushSlideDeformType {
  BRUSH_SLIDE_DEFORM_DRAG = 0,
  BRUSH_SLIDE_DEFORM_PINCH = 1,
  BRUSH_SLIDE_DEFORM_EXPAND = 2,
} eBrushSlideDeformType;

typedef enum eBrushBoundaryDeformType {
  BRUSH_BOUNDARY_DEFORM_BEND = 0,
  BRUSH_BOUNDARY_DEFORM_EXPAND = 1,
  BRUSH_BOUNDARY_DEFORM_INFLATE = 2,
  BRUSH_BOUNDARY_DEFORM_GRAB = 3,
  BRUSH_BOUNDARY_DEFORM_TWIST = 4,
  BRUSH_BOUNDARY_DEFORM_SMOOTH = 5,
} eBrushBushBoundaryDeformType;

typedef enum eBrushBoundaryFalloffType {
  BRUSH_BOUNDARY_FALLOFF_CONSTANT = 0,
  BRUSH_BOUNDARY_FALLOFF_RADIUS = 1,
  BRUSH_BOUNDARY_FALLOFF_LOOP = 2,
  BRUSH_BOUNDARY_FALLOFF_LOOP_INVERT = 3,
} eBrushBoundaryFalloffType;

typedef enum eBrushSnakeHookDeformType {
  BRUSH_SNAKE_HOOK_DEFORM_FALLOFF = 0,
  BRUSH_SNAKE_HOOK_DEFORM_ELASTIC = 1,
} eBrushSnakeHookDeformType;

typedef enum eBrushPlaneInversionMode {
  BRUSH_PLANE_INVERT_DISPLACEMENT = 0,
  BRUSH_PLANE_SWAP_HEIGHT_AND_DEPTH = 1,
} eBrushPlaneInversionMode;

/** #Gpencilsettings.Vertex_mode */
typedef enum eGp_Vertex_Mode {
  /* Affect to Stroke only. */
  GPPAINT_MODE_STROKE = 0,
  /* Affect to Fill only. */
  GPPAINT_MODE_FILL = 1,
  /* Affect to both. */
  GPPAINT_MODE_BOTH = 2,
} eGp_Vertex_Mode;

/* sculpt_flag */
typedef enum eGP_Sculpt_Flag {
  /* invert the effect of the brush */
  GP_SCULPT_FLAG_INVERT = (1 << 0),
  /* temporary invert action */
  GP_SCULPT_FLAG_TMP_INVERT = (1 << 3),
} eGP_Sculpt_Flag;
ENUM_OPERATORS(eGP_Sculpt_Flag)

/* sculpt_mode_flag */
typedef enum eGP_Sculpt_Mode_Flag {
  /* apply brush to position */
  GP_SCULPT_FLAGMODE_APPLY_POSITION = (1 << 0),
  /* apply brush to strength */
  GP_SCULPT_FLAGMODE_APPLY_STRENGTH = (1 << 1),
  /* apply brush to thickness */
  GP_SCULPT_FLAGMODE_APPLY_THICKNESS = (1 << 2),
  /* apply brush to uv data */
  GP_SCULPT_FLAGMODE_APPLY_UV = (1 << 3),
} eGP_Sculpt_Mode_Flag;
ENUM_OPERATORS(eGP_Sculpt_Mode_Flag)

typedef enum eAutomasking_flag {
  BRUSH_AUTOMASKING_TOPOLOGY = (1 << 0),
  BRUSH_AUTOMASKING_FACE_SETS = (1 << 1),
  BRUSH_AUTOMASKING_BOUNDARY_EDGES = (1 << 2),
  BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS = (1 << 3),
  BRUSH_AUTOMASKING_CAVITY_NORMAL = (1 << 4),

  /* NOTE: normal and inverted are mutually exclusive,
   * inverted has priority if both bits are set. */
  BRUSH_AUTOMASKING_CAVITY_INVERTED = (1 << 5),
  BRUSH_AUTOMASKING_CAVITY_ALL = (1 << 4) | (1 << 5),
  BRUSH_AUTOMASKING_CAVITY_USE_CURVE = (1 << 6),
  /* (1 << 7) - unused. */
  BRUSH_AUTOMASKING_BRUSH_NORMAL = (1 << 8),
  BRUSH_AUTOMASKING_VIEW_NORMAL = (1 << 9),
  BRUSH_AUTOMASKING_VIEW_OCCLUSION = (1 << 10),
} eAutomasking_flag;

typedef enum ePaintBrush_flag {
  BRUSH_PAINT_HARDNESS_PRESSURE = (1 << 0),
  BRUSH_PAINT_HARDNESS_PRESSURE_INVERT = (1 << 1),
  BRUSH_PAINT_FLOW_PRESSURE = (1 << 2),
  BRUSH_PAINT_FLOW_PRESSURE_INVERT = (1 << 3),
  BRUSH_PAINT_WET_MIX_PRESSURE = (1 << 4),
  BRUSH_PAINT_WET_MIX_PRESSURE_INVERT = (1 << 5),
  BRUSH_PAINT_WET_PERSISTENCE_PRESSURE = (1 << 6),
  BRUSH_PAINT_WET_PERSISTENCE_PRESSURE_INVERT = (1 << 7),
  BRUSH_PAINT_DENSITY_PRESSURE = (1 << 8),
  BRUSH_PAINT_DENSITY_PRESSURE_INVERT = (1 << 9),
} ePaintBrush_flag;

/** #Brush.gradient_source */
typedef enum eBrushGradientSourceStroke {
  BRUSH_GRADIENT_PRESSURE = 0,       /* gradient from pressure */
  BRUSH_GRADIENT_SPACING_REPEAT = 1, /* gradient from spacing */
  BRUSH_GRADIENT_SPACING_CLAMP = 2,  /* gradient from spacing */
} eBrushGradientSourceStroke;

typedef enum eBrushGradientSourceFill {
  BRUSH_GRADIENT_LINEAR = 0, /* gradient from pressure */
  BRUSH_GRADIENT_RADIAL = 1, /* gradient from spacing */
} eBrushGradientSourceFill;

/** #Brush.flag */
typedef enum eBrushFlags {
  BRUSH_AIRBRUSH = (1 << 0),
  BRUSH_INVERT_TO_SCRAPE_FILL = (1 << 1),
  BRUSH_ALPHA_PRESSURE = (1 << 2),
  BRUSH_SIZE_PRESSURE = (1 << 3),
  BRUSH_JITTER_PRESSURE = (1 << 4),
  BRUSH_SPACING_PRESSURE = (1 << 5),
  BRUSH_ORIGINAL_PLANE = (1 << 6),
  BRUSH_GRAB_ACTIVE_VERTEX = (1 << 7),
  BRUSH_ANCHORED = (1 << 8),
  BRUSH_DIR_IN = (1 << 9),
  BRUSH_SPACE = (1 << 10),
  BRUSH_SMOOTH_STROKE = (1 << 11),
  BRUSH_PERSISTENT = (1 << 12),
  BRUSH_ACCUMULATE = (1 << 13),
  BRUSH_LOCK_ALPHA = (1 << 14),
  BRUSH_ORIGINAL_NORMAL = (1 << 15),
  BRUSH_OFFSET_PRESSURE = (1 << 16),
  BRUSH_SCENE_SPACING = (1 << 17),
  BRUSH_SPACE_ATTEN = (1 << 18),
  BRUSH_ADAPTIVE_SPACE = (1 << 19),
  BRUSH_LOCK_SIZE = (1 << 20),
  BRUSH_USE_GRADIENT = (1 << 21),
  BRUSH_EDGE_TO_EDGE = (1 << 22),
  BRUSH_DRAG_DOT = (1 << 23),
  BRUSH_INVERSE_SMOOTH_PRESSURE = (1 << 24),
  BRUSH_FRONTFACE_FALLOFF = (1 << 25),
  BRUSH_PLANE_TRIM = (1 << 26),
  BRUSH_FRONTFACE = (1 << 27),
  /* BRUSH_CUSTOM_ICON = (1 << 28), */
  BRUSH_LINE = (1 << 29),
  BRUSH_ABSOLUTE_JITTER = (1 << 30),
  BRUSH_CURVE = (1u << 31),
} eBrushFlags;

/** #Brush.sampling_flag */
typedef enum eBrushSamplingFlags {
  BRUSH_PAINT_ANTIALIASING = (1 << 0),
} eBrushSamplingFlags;

/** #Brush.flag2 */
typedef enum eBrushFlags2 {
  BRUSH_MULTIPLANE_SCRAPE_DYNAMIC = (1 << 0),
  BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW = (1 << 1),
  BRUSH_POSE_IK_ANCHORED = (1 << 2),
  BRUSH_USE_CONNECTED_ONLY = (1 << 3),
  BRUSH_CLOTH_PIN_SIMULATION_BOUNDARY = (1 << 4),
  BRUSH_POSE_USE_LOCK_ROTATION = (1 << 5),
  BRUSH_CLOTH_USE_COLLISION = (1 << 6),
  BRUSH_AREA_RADIUS_PRESSURE = (1 << 7),
  BRUSH_GRAB_SILHOUETTE = (1 << 8),
  BRUSH_USE_COLOR_AS_DISPLACEMENT = (1 << 9),
  BRUSH_JITTER_COLOR = (1 << 10),
} eBrushFlags2;

typedef enum {
  BRUSH_MASK_PRESSURE_RAMP = (1 << 1),
  BRUSH_MASK_PRESSURE_CUTOFF = (1 << 2),
} BrushMaskPressureFlags;

/** #Brush.overlay_flags */
typedef enum eOverlayFlags {
  BRUSH_OVERLAY_CURSOR = (1),
  BRUSH_OVERLAY_PRIMARY = (1 << 1),
  BRUSH_OVERLAY_SECONDARY = (1 << 2),
  BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE = (1 << 3),
  BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE = (1 << 4),
  BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE = (1 << 5),
} eOverlayFlags;

#define BRUSH_OVERLAY_OVERRIDE_MASK \
  (BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE | BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE | \
   BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE)

/** #Brush.sculpt_brush_type */
typedef enum eBrushSculptType {
  SCULPT_BRUSH_TYPE_DRAW = 1,
  SCULPT_BRUSH_TYPE_SMOOTH = 2,
  SCULPT_BRUSH_TYPE_PINCH = 3,
  SCULPT_BRUSH_TYPE_INFLATE = 4,
  SCULPT_BRUSH_TYPE_GRAB = 5,
  SCULPT_BRUSH_TYPE_LAYER = 6,
#ifdef DNA_DEPRECATED_ALLOW
  SCULPT_BRUSH_TYPE_FLATTEN = 7,
#endif
  SCULPT_BRUSH_TYPE_CLAY = 8,
#ifdef DNA_DEPRECATED_ALLOW
  SCULPT_BRUSH_TYPE_FILL = 9,
  SCULPT_BRUSH_TYPE_SCRAPE = 10,
#endif
  SCULPT_BRUSH_TYPE_NUDGE = 11,
  SCULPT_BRUSH_TYPE_THUMB = 12,
  SCULPT_BRUSH_TYPE_SNAKE_HOOK = 13,
  SCULPT_BRUSH_TYPE_ROTATE = 14,
  SCULPT_BRUSH_TYPE_SIMPLIFY = 15,
  SCULPT_BRUSH_TYPE_CREASE = 16,
  SCULPT_BRUSH_TYPE_BLOB = 17,
  SCULPT_BRUSH_TYPE_CLAY_STRIPS = 18,
  SCULPT_BRUSH_TYPE_MASK = 19,
  SCULPT_BRUSH_TYPE_DRAW_SHARP = 20,
  SCULPT_BRUSH_TYPE_ELASTIC_DEFORM = 21,
  SCULPT_BRUSH_TYPE_POSE = 22,
  SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE = 23,
  SCULPT_BRUSH_TYPE_SLIDE_RELAX = 24,
  SCULPT_BRUSH_TYPE_CLAY_THUMB = 25,
  SCULPT_BRUSH_TYPE_CLOTH = 26,
  SCULPT_BRUSH_TYPE_DRAW_FACE_SETS = 27,
  SCULPT_BRUSH_TYPE_PAINT = 28,
  SCULPT_BRUSH_TYPE_SMEAR = 29,
  SCULPT_BRUSH_TYPE_BOUNDARY = 30,
  SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER = 31,
  SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR = 32,
  SCULPT_BRUSH_TYPE_PLANE = 33,
} eBrushSculptType;

/* Brush.curves_sculpt_brush_type. */
typedef enum eBrushCurvesSculptType {
  CURVES_SCULPT_BRUSH_TYPE_COMB = 0,
  CURVES_SCULPT_BRUSH_TYPE_DELETE = 1,
  CURVES_SCULPT_BRUSH_TYPE_SNAKE_HOOK = 2,
  CURVES_SCULPT_BRUSH_TYPE_ADD = 3,
  CURVES_SCULPT_BRUSH_TYPE_GROW_SHRINK = 4,
  CURVES_SCULPT_BRUSH_TYPE_SELECTION_PAINT = 5,
  CURVES_SCULPT_BRUSH_TYPE_PINCH = 6,
  CURVES_SCULPT_BRUSH_TYPE_SMOOTH = 7,
  CURVES_SCULPT_BRUSH_TYPE_PUFF = 8,
  CURVES_SCULPT_BRUSH_TYPE_DENSITY = 9,
  CURVES_SCULPT_BRUSH_TYPE_SLIDE = 10,
} eBrushCurvesSculptType;

/** #Brush.image_brush_type */
typedef enum eBrushImagePaintType {
  IMAGE_PAINT_BRUSH_TYPE_DRAW = 0,
  IMAGE_PAINT_BRUSH_TYPE_SOFTEN = 1,
  IMAGE_PAINT_BRUSH_TYPE_SMEAR = 2,
  IMAGE_PAINT_BRUSH_TYPE_CLONE = 3,
  IMAGE_PAINT_BRUSH_TYPE_FILL = 4,
  IMAGE_PAINT_BRUSH_TYPE_MASK = 5,
} eBrushImagePaintType;

/* The enums here should be kept in sync with the weight paint brush type.
 * This is because #smooth_brush_toggle_on and #smooth_brush_toggle_off
 * assumes that the blur brush has the same enum value. */
/** #Brush.vertex_brush_type */
typedef enum eBrushVertexPaintType {
  VPAINT_BRUSH_TYPE_DRAW = 0,
  VPAINT_BRUSH_TYPE_BLUR = 1,
  VPAINT_BRUSH_TYPE_AVERAGE = 2,
  VPAINT_BRUSH_TYPE_SMEAR = 3,
} eBrushVertexPaintType;

/* See #eBrushVertexPaintType when changing this definition. */
/** #Brush.weight_brush_type */
typedef enum eBrushWeightPaintType {
  WPAINT_BRUSH_TYPE_DRAW = 0,
  WPAINT_BRUSH_TYPE_BLUR = 1,
  WPAINT_BRUSH_TYPE_AVERAGE = 2,
  WPAINT_BRUSH_TYPE_SMEAR = 3,
} eBrushWeightPaintType;

/** #Brush.gpencil_brush_type */
typedef enum eBrushGPaintType {
  GPAINT_BRUSH_TYPE_DRAW = 0,
  GPAINT_BRUSH_TYPE_FILL = 1,
  GPAINT_BRUSH_TYPE_ERASE = 2,
  GPAINT_BRUSH_TYPE_TINT = 3,
} eBrushGPaintType;

/** #Brush.gpencil_vertex_brush_type */
typedef enum eBrushGPVertexType {
  GPVERTEX_BRUSH_TYPE_DRAW = 0,
  GPVERTEX_BRUSH_TYPE_BLUR = 1,
  GPVERTEX_BRUSH_TYPE_AVERAGE = 2,
  GPVERTEX_BRUSH_TYPE_TINT = 3,
  GPVERTEX_BRUSH_TYPE_SMEAR = 4,
  GPVERTEX_BRUSH_TYPE_REPLACE = 5,
} eBrushGPVertexType;

/** #Brush.gpencil_sculpt_brush_type */
typedef enum eBrushGPSculptType {
  GPSCULPT_BRUSH_TYPE_SMOOTH = 0,
  GPSCULPT_BRUSH_TYPE_THICKNESS = 1,
  GPSCULPT_BRUSH_TYPE_STRENGTH = 2,
  GPSCULPT_BRUSH_TYPE_GRAB = 3,
  GPSCULPT_BRUSH_TYPE_PUSH = 4,
  GPSCULPT_BRUSH_TYPE_TWIST = 5,
  GPSCULPT_BRUSH_TYPE_PINCH = 6,
  GPSCULPT_BRUSH_TYPE_RANDOMIZE = 7,
  GPSCULPT_BRUSH_TYPE_CLONE = 8,
} eBrushGPSculptType;

/** #Brush.gpencil_weight_brush_type */
typedef enum eBrushGPWeightType {
  GPWEIGHT_BRUSH_TYPE_DRAW = 0,
  GPWEIGHT_BRUSH_TYPE_BLUR = 1,
  GPWEIGHT_BRUSH_TYPE_AVERAGE = 2,
  GPWEIGHT_BRUSH_TYPE_SMEAR = 3,
} eBrushGPWeightType;

/* direction that the brush displaces along */
enum {
  SCULPT_DISP_DIR_AREA = 0,
  SCULPT_DISP_DIR_VIEW = 1,
  SCULPT_DISP_DIR_X = 2,
  SCULPT_DISP_DIR_Y = 3,
  SCULPT_DISP_DIR_Z = 4,
};

typedef enum {
  BRUSH_MASK_DRAW = 0,
  BRUSH_MASK_SMOOTH = 1,
} BrushMaskTool;

/* blur kernel types, Brush.blur_mode */
typedef enum eBlurKernelType {
  KERNEL_GAUSSIAN = 0,
  KERNEL_BOX = 1,
} eBlurKernelType;

/** #Brush.falloff_shape */
typedef enum eBrushFalloffShape {
  PAINT_FALLOFF_SHAPE_SPHERE = 0,
  PAINT_FALLOFF_SHAPE_TUBE = 1,
} eBrushFalloffShape;

typedef enum eBrushCurvesSculptFlag {
  BRUSH_CURVES_SCULPT_FLAG_SCALE_UNIFORM = (1 << 0),
  BRUSH_CURVES_SCULPT_FLAG_GROW_SHRINK_INVERT = (1 << 1),
  BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_LENGTH = (1 << 2),
  BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_SHAPE = (1 << 3),
  BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_POINT_COUNT = (1 << 4),
  BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_RADIUS = (1 << 5),
} eBrushCurvesSculptFlag;

typedef enum eBrushCurvesSculptDensityMode {
  BRUSH_CURVES_SCULPT_DENSITY_MODE_AUTO = 0,
  BRUSH_CURVES_SCULPT_DENSITY_MODE_ADD = 1,
  BRUSH_CURVES_SCULPT_DENSITY_MODE_REMOVE = 2,
} eBrushCurvesSculptDensityMode;

typedef enum eBrushColorJitterSettings_Flag {
  BRUSH_COLOR_JITTER_USE_HUE_AT_STROKE = (1 << 0),
  BRUSH_COLOR_JITTER_USE_SAT_AT_STROKE = (1 << 1),
  BRUSH_COLOR_JITTER_USE_VAL_AT_STROKE = (1 << 2),
  BRUSH_COLOR_JITTER_USE_HUE_RAND_PRESS = (1 << 3),
  BRUSH_COLOR_JITTER_USE_SAT_RAND_PRESS = (1 << 4),
  BRUSH_COLOR_JITTER_USE_VAL_RAND_PRESS = (1 << 5),
} eBrushColorJitterSettings_Flag;

#define MAX_BRUSH_PIXEL_RADIUS 500
#define MAX_BRUSH_PIXEL_DIAMETER 1000
