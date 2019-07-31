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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_BRUSH_TYPES_H__
#define __DNA_BRUSH_TYPES_H__

#include "DNA_ID.h"
#include "DNA_texture_types.h" /* for MTex */
#include "DNA_curve_types.h"

//#ifndef MAX_MTEX // XXX Not used?
//#define MAX_MTEX  18
//#endif

struct CurveMapping;
struct Image;
struct MTex;
struct Material;

typedef struct BrushClone {
  /** Image for clone tool. */
  struct Image *image;
  /** Offset of clone image from canvas. */
  float offset[2];
  /** Transparency for drawing of clone image. */
  float alpha;
  char _pad[4];
} BrushClone;

typedef struct BrushGpencilSettings {
  /** Amount of smoothing to apply to newly created strokes. */
  float draw_smoothfac;
  /** Amount of sensitivity to apply to newly created strokes. */
  float draw_sensitivity;
  /** Amount of alpha strength to apply to newly created strokes. */
  float draw_strength;
  /** Amount of jitter to apply to newly created strokes. */
  float draw_jitter;
  /** Angle when the brush has full thickness. */
  float draw_angle;
  /** Factor to apply when angle change (only 90 degrees). */
  float draw_angle_factor;
  /** Factor of randomness for pressure. */
  float draw_random_press;
  /** Factor of strength for strength. */
  float draw_random_strength;
  /** Factor of randomness for subdivision. */
  float draw_random_sub;
  /** Number of times to apply smooth factor to new strokes. */
  short draw_smoothlvl;
  /** Number of times to subdivide new strokes. */
  short draw_subdivide;
  short _pad;

  /** Number of times to apply thickness smooth factor to new strokes. */
  short thick_smoothlvl;
  /** Amount of thickness smoothing to apply to newly created strokes. */
  float thick_smoothfac;

  /** Factor for transparency. */
  float fill_threshold;
  /** Number of pixel to consider the leak is too small (x 2). */
  short fill_leak;
  /** Fill zoom factor */
  short fill_factor;
  char _pad_1[4];

  /** Number of simplify steps. */
  int fill_simplylvl;
  /** Type of control lines drawing mode. */
  int fill_draw_mode;
  /** Icon identifier. */
  int icon_id;

  /** Maximum distance before generate new point for very fast mouse movements. */
  int input_samples;
  /** Random factor for UV rotation. */
  float uv_random;
  /** Moved to 'Brush.gpencil_tool'. */
  int brush_type DNA_DEPRECATED;
  /** Soft, hard or stroke. */
  int eraser_mode;
  /** Smooth while drawing factor. */
  float active_smooth;
  /** Factor to apply to strength for soft eraser. */
  float era_strength_f;
  /** Factor to apply to thickness for soft eraser. */
  float era_thickness_f;
  /** Internal grease pencil drawing flags. */
  int flag;

  /** gradient control along y for color */
  float gradient_f;
  /** factor xy of shape for dots gradients */
  float gradient_s[2];
  char _pad_2[4];

  struct CurveMapping *curve_sensitivity;
  struct CurveMapping *curve_strength;
  struct CurveMapping *curve_jitter;

  /* optional link of material to replace default in context */
  /** Material. */
  struct Material *material;
} BrushGpencilSettings;

/* BrushGpencilSettings->gp_flag */
typedef enum eGPDbrush_Flag {
  /* brush use pressure */
  GP_BRUSH_USE_PRESSURE = (1 << 0),
  /* brush use pressure for alpha factor */
  GP_BRUSH_USE_STENGTH_PRESSURE = (1 << 1),
  /* brush use pressure for alpha factor */
  GP_BRUSH_USE_JITTER_PRESSURE = (1 << 2),
  /* enable screen cursor */
  GP_BRUSH_ENABLE_CURSOR = (1 << 5),
  /* fill hide transparent */
  GP_BRUSH_FILL_HIDE = (1 << 6),
  /* show fill help lines */
  GP_BRUSH_FILL_SHOW_HELPLINES = (1 << 7),
  /* lazy mouse */
  GP_BRUSH_STABILIZE_MOUSE = (1 << 8),
  /* lazy mouse override (internal only) */
  GP_BRUSH_STABILIZE_MOUSE_TEMP = (1 << 9),
  /* default eraser brush for quick switch */
  GP_BRUSH_DEFAULT_ERASER = (1 << 10),
  /* settings group */
  GP_BRUSH_GROUP_SETTINGS = (1 << 11),
  /* Random settings group */
  GP_BRUSH_GROUP_RANDOM = (1 << 12),
  /* Keep material assigned to brush */
  GP_BRUSH_MATERIAL_PINNED = (1 << 13),
  /* Do not show fill color while drawing (no lasso mode) */
  GP_BRUSH_DISSABLE_LASSO = (1 << 14),
  /* Do not erase strokes oLcluded */
  GP_BRUSH_OCCLUDE_ERASER = (1 << 15),
  /* Post process trim stroke */
  GP_BRUSH_TRIM_STROKE = (1 << 16),
} eGPDbrush_Flag;

/* BrushGpencilSettings->gp_fill_draw_mode */
typedef enum eGP_FillDrawModes {
  GP_FILL_DMODE_BOTH = 0,
  GP_FILL_DMODE_STROKE = 1,
  GP_FILL_DMODE_CONTROL = 2,
} eGP_FillDrawModes;

/* BrushGpencilSettings->gp_eraser_mode */
typedef enum eGP_BrushEraserMode {
  GP_BRUSH_ERASER_SOFT = 0,
  GP_BRUSH_ERASER_HARD = 1,
  GP_BRUSH_ERASER_STROKE = 2,
} eGP_BrushEraserMode;

/* BrushGpencilSettings default brush icons */
typedef enum eGP_BrushIcons {
  GP_BRUSH_ICON_PENCIL = 1,
  GP_BRUSH_ICON_PEN = 2,
  GP_BRUSH_ICON_INK = 3,
  GP_BRUSH_ICON_INKNOISE = 4,
  GP_BRUSH_ICON_BLOCK = 5,
  GP_BRUSH_ICON_MARKER = 6,
  GP_BRUSH_ICON_FILL = 7,
  GP_BRUSH_ICON_ERASE_SOFT = 8,
  GP_BRUSH_ICON_ERASE_HARD = 9,
  GP_BRUSH_ICON_ERASE_STROKE = 10,
} eGP_BrushIcons;

typedef enum eBrushCurvePreset {
  BRUSH_CURVE_CUSTOM = 0,
  BRUSH_CURVE_SMOOTH = 1,
  BRUSH_CURVE_SPHERE = 2,
  BRUSH_CURVE_ROOT = 3,
  BRUSH_CURVE_SHARP = 4,
  BRUSH_CURVE_LIN = 5,
  BRUSH_CURVE_POW4 = 6,
  BRUSH_CURVE_INVSQUARE = 7,
  BRUSH_CURVE_CONSTANT = 8,
} eBrushCurvePreset;

typedef struct Brush {
  ID id;

  struct BrushClone clone;
  /** Falloff curve. */
  struct CurveMapping *curve;
  struct MTex mtex;
  struct MTex mask_mtex;

  struct Brush *toggle_brush;

  struct ImBuf *icon_imbuf;
  PreviewImage *preview;
  /** Color gradient. */
  struct ColorBand *gradient;
  struct PaintCurve *paint_curve;

  /** 1024 = FILE_MAX. */
  char icon_filepath[1024];

  float normal_weight;
  /** Rake actual data (not texture), used for sculpt. */
  float rake_factor;

  /** Blend mode. */
  short blend;
  /** #eObjectMode: to see if the brush is compatible, use for display only. */
  short ob_mode;
  /** Brush weight. */
  float weight;
  /** Brush diameter. */
  int size;
  /** General purpose flag. */
  int flag;
  /** Pressure influence for mask. */
  int mask_pressure;
  /** Jitter the position of the brush. */
  float jitter;
  /** Absolute jitter in pixels. */
  int jitter_absolute;
  int overlay_flags;
  /** Spacing of paint operations. */
  int spacing;
  /** Turning radius (in pixels) for smooth stroke. */
  int smooth_stroke_radius;
  /** Higher values limit fast changes in the stroke direction. */
  float smooth_stroke_factor;
  /** Paint operations / second (airbrush). */
  float rate;

  /** Color. */
  float rgb[3];
  /** Opacity. */
  float alpha;

  /** Background color. */
  float secondary_rgb[3];

  /** The direction of movement for sculpt vertices. */
  int sculpt_plane;

  /** Offset for plane brushes (clay, flatten, fill, scrape). */
  float plane_offset;

  int gradient_spacing;
  /** Source for stroke color gradient application. */
  char gradient_stroke_mode;
  /** Source for fill tool color gradient application. */
  char gradient_fill_mode;

  char _pad;
  /** Projection shape (sphere, circle). */
  char falloff_shape;
  float falloff_angle;

  /** Active sculpt tool. */
  char sculpt_tool;
  /** Active sculpt tool. */
  char uv_sculpt_tool;
  /** Active vertex paint. */
  char vertexpaint_tool;
  /** Active weight paint. */
  char weightpaint_tool;
  /** Active image paint tool. */
  char imagepaint_tool;
  /** Enum eBrushMaskTool, only used if sculpt_tool is SCULPT_TOOL_MASK. */
  char mask_tool;
  /** Active grease pencil tool. */
  char gpencil_tool;
  char _pad0[1];

  float autosmooth_factor;

  float topology_rake_factor;

  float crease_pinch_factor;

  float plane_trim;
  /** Affectable height of brush (layer height for layer tool, i.e.). */
  float height;

  float texture_sample_bias;

  int curve_preset;
  char _pad1[4];

  /* overlay */
  int texture_overlay_alpha;
  int mask_overlay_alpha;
  int cursor_overlay_alpha;

  float unprojected_radius;

  /* soften/sharpen */
  float sharp_threshold;
  int blur_kernel_radius;
  int blur_mode;

  /* fill tool */
  float fill_threshold;

  float add_col[3];
  float sub_col[3];

  float stencil_pos[2];
  float stencil_dimension[2];

  float mask_stencil_pos[2];
  float mask_stencil_dimension[2];

  struct BrushGpencilSettings *gpencil_settings;

} Brush;
typedef struct PaletteColor {
  struct PaletteColor *next, *prev;
  /* two values, one to store rgb, other to store values for sculpt/weight */
  float rgb[3];
  float value;
} PaletteColor;

typedef struct Palette {
  ID id;

  /** Pointer to individual colors. */
  ListBase colors;

  int active_color;
  char _pad[4];
} Palette;

typedef struct PaintCurvePoint {
  /** Bezier handle. */
  BezTriple bez;
  /** Pressure on that point. */
  float pressure;
} PaintCurvePoint;

typedef struct PaintCurve {
  ID id;
  /** Points of curve. */
  PaintCurvePoint *points;
  int tot_points;
  /** Index where next point will be added. */
  int add_index;
} PaintCurve;

/* Brush.gradient_source */
typedef enum eBrushGradientSourceStroke {
  BRUSH_GRADIENT_PRESSURE = 0,       /* gradient from pressure */
  BRUSH_GRADIENT_SPACING_REPEAT = 1, /* gradient from spacing */
  BRUSH_GRADIENT_SPACING_CLAMP = 2,  /* gradient from spacing */
} eBrushGradientSourceStroke;

typedef enum eBrushGradientSourceFill {
  BRUSH_GRADIENT_LINEAR = 0, /* gradient from pressure */
  BRUSH_GRADIENT_RADIAL = 1, /* gradient from spacing */
} eBrushGradientSourceFill;

/* Brush.flag */
typedef enum eBrushFlags {
  BRUSH_AIRBRUSH = (1 << 0),
  BRUSH_FLAG_UNUSED_1 = (1 << 1), /* cleared */
  BRUSH_ALPHA_PRESSURE = (1 << 2),
  BRUSH_SIZE_PRESSURE = (1 << 3),
  BRUSH_JITTER_PRESSURE = (1 << 4),
  BRUSH_SPACING_PRESSURE = (1 << 5),
  BRUSH_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  BRUSH_FLAG_UNUSED_7 = (1 << 7), /* cleared */
  BRUSH_ANCHORED = (1 << 8),
  BRUSH_DIR_IN = (1 << 9),
  BRUSH_SPACE = (1 << 10),
  BRUSH_SMOOTH_STROKE = (1 << 11),
  BRUSH_PERSISTENT = (1 << 12),
  BRUSH_ACCUMULATE = (1 << 13),
  BRUSH_LOCK_ALPHA = (1 << 14),
  BRUSH_ORIGINAL_NORMAL = (1 << 15),
  BRUSH_OFFSET_PRESSURE = (1 << 16),
  BRUSH_FLAG_UNUSED_17 = (1 << 17), /* cleared */
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
  BRUSH_CUSTOM_ICON = (1 << 28),
  BRUSH_LINE = (1 << 29),
  BRUSH_ABSOLUTE_JITTER = (1 << 30),
  BRUSH_CURVE = (1u << 31),
} eBrushFlags;

typedef enum {
  BRUSH_MASK_PRESSURE_RAMP = (1 << 1),
  BRUSH_MASK_PRESSURE_CUTOFF = (1 << 2),
} BrushMaskPressureFlags;

/* Brush.overlay_flags */
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

/* Brush.sculpt_tool */
typedef enum eBrushSculptTool {
  SCULPT_TOOL_DRAW = 1,
  SCULPT_TOOL_SMOOTH = 2,
  SCULPT_TOOL_PINCH = 3,
  SCULPT_TOOL_INFLATE = 4,
  SCULPT_TOOL_GRAB = 5,
  SCULPT_TOOL_LAYER = 6,
  SCULPT_TOOL_FLATTEN = 7,
  SCULPT_TOOL_CLAY = 8,
  SCULPT_TOOL_FILL = 9,
  SCULPT_TOOL_SCRAPE = 10,
  SCULPT_TOOL_NUDGE = 11,
  SCULPT_TOOL_THUMB = 12,
  SCULPT_TOOL_SNAKE_HOOK = 13,
  SCULPT_TOOL_ROTATE = 14,
  SCULPT_TOOL_SIMPLIFY = 15,
  SCULPT_TOOL_CREASE = 16,
  SCULPT_TOOL_BLOB = 17,
  SCULPT_TOOL_CLAY_STRIPS = 18,
  SCULPT_TOOL_MASK = 19,
} eBrushSculptTool;

/* Brush.uv_sculpt_tool */
typedef enum eBrushUVSculptTool {
  UV_SCULPT_TOOL_GRAB = 0,
  UV_SCULPT_TOOL_RELAX = 1,
  UV_SCULPT_TOOL_PINCH = 2,
} eBrushUVSculptTool;

/** When #BRUSH_ACCUMULATE is used */
#define SCULPT_TOOL_HAS_ACCUMULATE(t) \
  ELEM(t, \
       SCULPT_TOOL_DRAW, \
       SCULPT_TOOL_CREASE, \
       SCULPT_TOOL_BLOB, \
       SCULPT_TOOL_LAYER, \
       SCULPT_TOOL_INFLATE, \
       SCULPT_TOOL_CLAY, \
       SCULPT_TOOL_CLAY_STRIPS, \
       SCULPT_TOOL_ROTATE, \
       SCULPT_TOOL_FLATTEN)

#define SCULPT_TOOL_HAS_NORMAL_WEIGHT(t) ELEM(t, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK)

#define SCULPT_TOOL_HAS_RAKE(t) ELEM(t, SCULPT_TOOL_SNAKE_HOOK)

#define SCULPT_TOOL_HAS_DYNTOPO(t) \
  (ELEM(t, /* These brushes, as currently coded, cannot support dynamic topology */ \
        SCULPT_TOOL_GRAB, \
        SCULPT_TOOL_ROTATE, \
        SCULPT_TOOL_THUMB, \
        SCULPT_TOOL_LAYER, \
\
        /* These brushes could handle dynamic topology, \
         * but user feedback indicates it's better not to */ \
        SCULPT_TOOL_SMOOTH, \
        SCULPT_TOOL_MASK) == 0)

#define SCULPT_TOOL_HAS_TOPOLOGY_RAKE(t) \
  (ELEM(t, /* These brushes, as currently coded, cannot support topology rake. */ \
        SCULPT_TOOL_GRAB, \
        SCULPT_TOOL_ROTATE, \
        SCULPT_TOOL_THUMB, \
        SCULPT_TOOL_MASK) == 0)

/* ImagePaintSettings.tool */
typedef enum eBrushImagePaintTool {
  PAINT_TOOL_DRAW = 0,
  PAINT_TOOL_SOFTEN = 1,
  PAINT_TOOL_SMEAR = 2,
  PAINT_TOOL_CLONE = 3,
  PAINT_TOOL_FILL = 4,
  PAINT_TOOL_MASK = 5,
} eBrushImagePaintTool;

typedef enum eBrushVertexPaintTool {
  VPAINT_TOOL_DRAW = 0,
  VPAINT_TOOL_BLUR = 1,
  VPAINT_TOOL_AVERAGE = 2,
  VPAINT_TOOL_SMEAR = 3,
} eBrushVertexPaintTool;

typedef enum eBrushWeightPaintTool {
  WPAINT_TOOL_DRAW = 0,
  WPAINT_TOOL_BLUR = 1,
  WPAINT_TOOL_AVERAGE = 2,
  WPAINT_TOOL_SMEAR = 3,
} eBrushWeightPaintTool;

/* BrushGpencilSettings->brush type */
typedef enum eBrushGPaintTool {
  GPAINT_TOOL_DRAW = 0,
  GPAINT_TOOL_FILL = 1,
  GPAINT_TOOL_ERASE = 2,
} eBrushGPaintTool;

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
  KERNEL_GAUSSIAN,
  KERNEL_BOX,
} eBlurKernelType;

/* Brush.falloff_shape */
enum {
  PAINT_FALLOFF_SHAPE_SPHERE = 0,
  PAINT_FALLOFF_SHAPE_TUBE = 1,
};

#define MAX_BRUSH_PIXEL_RADIUS 500
#define GP_MAX_BRUSH_PIXEL_RADIUS 1000

#endif /* __DNA_BRUSH_TYPES_H__ */
