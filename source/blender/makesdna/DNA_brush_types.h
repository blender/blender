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
  char _pad[1];

  int flag2;

  /** Number of simplify steps. */
  int fill_simplylvl;
  /** Type of control lines drawing mode. */
  int fill_draw_mode;
  /** Type of gap filling extension to use. */
  int fill_extend_mode;

  /** Maximum distance before generate new point for very fast mouse movements. */
  int input_samples;
  /** Random factor for UV rotation. */
  float uv_random;
  /** Moved to 'Brush.gpencil_brush_type'. */
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

  int color_jitter_flag;
  char _pad1[4];

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
  /** Screen space simplify threshold. Points within this margin are treated as a straight line. */
  float simplify_px;

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
  /** The initial radius of curve. */
  float curve_radius;
  /** How often the Density brush tries to add a new curve. */
  int density_add_attempts;
  /** #eBrushCurvesSculptDensityMode. */
  uint8_t density_mode;
  char _pad[7];
  struct CurveMapping *curve_parameter_falloff;
} BrushCurvesSculptSettings;

/** Max number of propagation steps for automasking settings. */
#define AUTOMASKING_BOUNDARY_EDGES_MAX_PROPAGATION_STEPS 20
/**
 * \note Any change to members that is user visible and that may make the brush differ from the one
 * saved in the asset library should be followed by a #BKE_brush_tag_unsaved_changes() call.
 */
typedef struct Brush {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(Brush)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_BR;
#endif

  ID id;

  struct CurveMapping *curve_distance_falloff;
  struct MTex mtex;
  struct MTex mask_mtex;

  PreviewImage *preview;
  /** Color gradient. */
  struct ColorBand *gradient;
  struct PaintCurve *paint_curve;

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
  float color[3];
  int color_jitter_flag;
  float hsv_jitter[3];

  /** Color jitter pressure curves. */
  struct CurveMapping *curve_rand_hue;
  struct CurveMapping *curve_rand_saturation;
  struct CurveMapping *curve_rand_value;

  struct CurveMapping *curve_size;
  struct CurveMapping *curve_strength;
  struct CurveMapping *curve_jitter;

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
  float secondary_color[3];

  /* Deprecated sRGB color for forward compatibility. */
  float rgb[3] DNA_DEPRECATED;
  float secondary_rgb[3] DNA_DEPRECATED;

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
  /** Source for fill brush color gradient application. */
  char gradient_fill_mode;

  /**
   * Tag to indicate to the user that the brush has been changed since being imported. Only set for
   * brushes that are actually imported (must have #ID.lib set). Runtime only.
   */
  char has_unsaved_changes;

  /** Projection shape (sphere, circle). */
  char falloff_shape;
  float falloff_angle;

  /** Active sculpt brush type. */
  char sculpt_brush_type;
  /** Active vertex paint. */
  char vertex_brush_type;
  /** Active weight paint. */
  char weight_brush_type;
  /** Active image paint brush type. */
  char image_brush_type;
  /** Enum eBrushMaskTool, only used if sculpt_brush_type is SCULPT_BRUSH_TYPE_MASK. */
  char mask_tool;
  /** Active grease pencil brush type. */
  char gpencil_brush_type;
  /** Active grease pencil vertex brush type. */
  char gpencil_vertex_brush_type;
  /** Active grease pencil sculpt brush type. */
  char gpencil_sculpt_brush_type;
  /** Active grease pencil weight brush type. */
  char gpencil_weight_brush_type;
  /** Active curves sculpt brush type (#eBrushCurvesSculptType). */
  char curves_sculpt_brush_type;
  char _pad1[10];

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

  /* Plane Brush */
  float plane_height;
  float plane_depth;
  float stabilize_normal;
  float stabilize_plane;
  int plane_inversion_mode;

  float texture_sample_bias;

  /**
   * This preset is used to specify an exact function used for the distance falloff instead
   * of doing a Bezier spline evaluation via CurveMapping for performance reasons.
   * \see #eBrushCurvePreset and #eCurveMappingPreset
   */
  int curve_distance_falloff_preset;

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

  float unprojected_size;

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
#
#
typedef struct tPaletteColorHSV {
  float rgb[3];
  float value;
  float h;
  float s;
  float v;
} tPaletteColorHSV;

typedef struct PaletteColor {
  struct PaletteColor *next, *prev;
  /* Two values, one to store color, other to store values for sculpt/weight. */
  float color[3];
  float value;

  /* For forward compatibility. */
  float rgb[3] DNA_DEPRECATED;
  float _pad;
} PaletteColor;

typedef struct Palette {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_PAL;
#endif

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
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_PC;
#endif

  ID id;
  /** Points of curve. */
  PaintCurvePoint *points;
  int tot_points;
  /** Index where next point will be added. */
  int add_index;
} PaintCurve;
