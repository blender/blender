/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_brush_enums.h"
#include "DNA_curve_types.h"
#include "DNA_defs.h"
#include "DNA_texture_types.h" /* for MTex */

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
  /** Fill zoom factor */
  float fill_factor;
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
  /** Number of times to apply smooth factor to new strokes. */
  short draw_smoothlvl;
  /** Number of times to subdivide new strokes. */
  short draw_subdivide;
  /** Layers used for fill. */
  short fill_layer_mode;
  short fill_direction;

  /** Factor for transparency. */
  float fill_threshold;
  char _pad2[2];
  /* Type of caps: eGPDstroke_Caps. */
  int8_t caps_type;
  char _pad[5];

  int flag2;

  /** Number of simplify steps. */
  int fill_simplylvl;
  /** Type of control lines drawing mode. */
  int fill_draw_mode;
  /** Type of gap filling extension to use. */
  int fill_extend_mode;
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
  float hardness;
  /** factor xy of shape for dots gradients */
  float aspect_ratio[2];
  /** Simplify adaptive factor */
  float simplify_f;

  /** Mix color-factor. */
  float vertex_factor;
  int vertex_mode;

  /** eGP_Sculpt_Flag. */
  int sculpt_flag;
  /** eGP_Sculpt_Mode_Flag. */
  int sculpt_mode_flag;
  /** Preset type (used to reset brushes - internal). */
  short preset_type;
  /** Brush preselected mode (Active/Material/Vertex-color). */
  short brush_draw_mode;

  /** Randomness for Hue. */
  float random_hue;
  /** Randomness for Saturation. */
  float random_saturation;
  /** Randomness for Value. */
  float random_value;

  /** Factor to extend stroke extremes using fill tool. */
  float fill_extend_fac;
  /** Number of pixels to dilate fill area. */
  int dilate_pixels;

  struct CurveMapping *curve_sensitivity;
  struct CurveMapping *curve_strength;
  struct CurveMapping *curve_jitter;
  struct CurveMapping *curve_rand_pressure;
  struct CurveMapping *curve_rand_strength;
  struct CurveMapping *curve_rand_uv;
  struct CurveMapping *curve_rand_hue;
  struct CurveMapping *curve_rand_saturation;
  struct CurveMapping *curve_rand_value;

  /** Factor for external line thickness conversion to outline. */
  float outline_fac;
  char _pad1[4];

  /* optional link of material to replace default in context */
  /** Material. */
  struct Material *material;
  /** Material Alternative for secondary operations. */
  struct Material *material_alt;
} BrushGpencilSettings;

typedef struct BrushCurvesSculptSettings {
  /** Number of curves added by the add brush. */
  int add_amount;
  /** Number of control points in new curves added by the add brush. */
  int points_per_curve;
  /* eBrushCurvesSculptFlag. */
  uint32_t flag;
  /** When shrinking curves, they shouldn't become shorter than this length. */
  float minimum_length;
  /** Length of newly added curves when it is not interpolated from other curves. */
  float curve_length;
  /** Minimum distance between curve root points used by the Density brush. */
  float minimum_distance;
  /** The initial radius of curve. **/
  float curve_radius;
  /** How often the Density brush tries to add a new curve. */
  int density_add_attempts;
  /** #eBrushCurvesSculptDensityMode. */
  uint8_t density_mode;
  char _pad[7];
  struct CurveMapping *curve_parameter_falloff;
} BrushCurvesSculptSettings;

/** Max number of propagation steps for automasking settings.*/
#define AUTOMASKING_BOUNDARY_EDGES_MAX_PROPAGATION_STEPS 20
typedef struct Brush {
  DNA_DEFINE_CXX_METHODS(Brush)

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
  /** General purpose flags. */
  int flag;
  int flag2;
  int sampling_flag;

  /** Number of samples used to smooth the stroke. */
  int input_samples;

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
  /** Hardness */
  float hardness;
  /** Flow */
  float flow;
  /** Wet Mix */
  float wet_mix;
  float wet_persistence;
  /** Density */
  float density;
  int paint_flags;

  /** Tip Shape */
  /* Factor that controls the shape of the brush tip by rounding the corners of a square. */
  /* 0.0 value produces a square, 1.0 produces a circle. */
  float tip_roundness;
  float tip_scale_x;

  /** Background color. */
  float secondary_rgb[3];

  /** Rate */
  float dash_ratio;
  int dash_samples;

  /** The direction of movement for sculpt vertices. */
  int sculpt_plane;

  /** Offset for plane brushes (clay, flatten, fill, scrape). */
  float plane_offset;

  int gradient_spacing;
  /** Source for stroke color gradient application. */
  char gradient_stroke_mode;
  /** Source for fill tool color gradient application. */
  char gradient_fill_mode;

  char _pad0;

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
  /** Active grease pencil vertex tool. */
  char gpencil_vertex_tool;
  /** Active grease pencil sculpt tool. */
  char gpencil_sculpt_tool;
  /** Active grease pencil weight tool. */
  char gpencil_weight_tool;
  /** Active curves sculpt tool (#eBrushCurvesSculptTool). */
  char curves_sculpt_tool;
  char _pad1[5];

  float autosmooth_factor;

  float tilt_strength_factor;

  float topology_rake_factor;

  float crease_pinch_factor;

  float normal_radius_factor;
  float area_radius_factor;
  float wet_paint_radius_factor;

  float plane_trim;
  /** Affectable height of brush (layer height for layer tool, i.e.). */
  float height;

  float texture_sample_bias;

  int curve_preset;

  /* Maximum distance to search fake neighbors from a vertex. */
  float disconnected_distance_max;

  int deform_target;

  /* automasking */
  int automasking_flags;
  int automasking_boundary_edges_propagation_steps;

  float automasking_start_normal_limit;
  float automasking_start_normal_falloff;
  float automasking_view_normal_limit;
  float automasking_view_normal_falloff;

  int elastic_deform_type;
  float elastic_deform_volume_preservation;

  /* snake hook */
  int snake_hook_deform_type;

  /* pose */
  int pose_deform_type;
  float pose_offset;
  int pose_smooth_iterations;
  int pose_ik_segments;
  int pose_origin_type;

  /* boundary */
  int boundary_deform_type;
  int boundary_falloff_type;
  float boundary_offset;

  /* cloth */
  int cloth_deform_type;
  int cloth_force_falloff_type;
  int cloth_simulation_area_type;

  float cloth_mass;
  float cloth_damping;

  float cloth_sim_limit;
  float cloth_sim_falloff;

  float cloth_constraint_softbody_strength;

  /* smooth */
  int smooth_deform_type;
  float surface_smooth_shape_preservation;
  float surface_smooth_current_vertex;
  int surface_smooth_iterations;

  /* multiplane scrape */
  float multiplane_scrape_angle;

  /* smear */
  int smear_deform_type;

  /* slide/relax */
  int slide_deform_type;

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

  float add_col[4];
  float sub_col[4];

  float stencil_pos[2];
  float stencil_dimension[2];

  float mask_stencil_pos[2];
  float mask_stencil_dimension[2];

  struct BrushGpencilSettings *gpencil_settings;
  struct BrushCurvesSculptSettings *curves_sculpt_settings;

  int automasking_cavity_blur_steps;
  float automasking_cavity_factor;

  struct CurveMapping *automasking_cavity_curve;
} Brush;

/* Struct to hold palette colors for sorting. */
typedef struct tPaletteColorHSV {
  float rgb[3];
  float value;
  float h;
  float s;
  float v;
} tPaletteColorHSV;

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
