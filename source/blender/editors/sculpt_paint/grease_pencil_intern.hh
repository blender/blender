/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>

#include "BLI_color.hh"
#include "BLI_task.hh"

#include "DNA_scene_types.h"

#include "ED_grease_pencil.hh"

#include "IMB_imbuf_types.hh"

#include "paint_intern.hh"

#ifdef WITH_POTRACE
#  include "potracelib.h"
#endif

namespace blender::bke::greasepencil {
class Drawing;
class Layer;
}  // namespace blender::bke::greasepencil
namespace blender::bke::crazyspace {
struct GeometryDeformation;
}

namespace blender::ed::sculpt_paint {

/**
 * Projects a screen-space displacement vector into layer space.
 * Current position (in layer space) is used to compute the perspective distance (`zfac`).
 * Returns the new layer space position with the projected delta applied.
 */
using DeltaProjectionFunc =
    std::function<float3(const float3 position, const float2 &screen_delta)>;

struct InputSample {
  float2 mouse_position;
  float pressure;
};

class GreasePencilStrokeOperation : public PaintModeData {
 public:
  virtual void on_stroke_begin(const bContext &C, const InputSample &start_sample) = 0;
  virtual void on_stroke_extended(const bContext &C, const InputSample &extension_sample) = 0;
  virtual void on_stroke_done(const bContext &C) = 0;
};

namespace greasepencil {

/* Get list of drawings the tool should be operating on. */
Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_for_painting(const bContext &C);
/* Get the brush radius accounting for pen pressure. */
float brush_radius(const Scene &scene, const Brush &brush, float pressure);

/* Make sure the brush has all necessary grease pencil settings. */
void init_brush(Brush &brush);

/* Index mask of all points within the brush radius. */
IndexMask brush_point_influence_mask(const Scene &scene,
                                     const Brush &brush,
                                     const float2 &mouse_position,
                                     float pressure,
                                     float multi_frame_falloff,
                                     const IndexMask &selection,
                                     Span<float2> view_positions,
                                     Vector<float> &influences,
                                     IndexMaskMemory &memory);

/* Influence value at point co for the brush. */
float brush_point_influence(const Scene &scene,
                            const Brush &brush,
                            const float2 &co,
                            const InputSample &sample,
                            float multi_frame_falloff);
/**
 * Compute the closest distance to the "surface".
 * When the point is outside the polygon, compute the closest distance to the polygon points.
 * When the point is inside the polygon return 0.
 */
float closest_distance_to_surface_2d(const float2 pt, const Span<float2> verts);
/* Influence value for an entire fill. */
float brush_fill_influence(const Scene &scene,
                           const Brush &brush,
                           Span<float2> fill_positions,
                           const InputSample &sample,
                           float multi_frame_falloff);

/* True if influence of the brush should be inverted. */
bool is_brush_inverted(const Brush &brush, BrushStrokeMode stroke_mode);

/* Common parameters for stroke callbacks that can be passed to utility functions. */
struct GreasePencilStrokeParams {
  const ToolSettings &toolsettings;
  const ARegion &region;
  const RegionView3D &rv3d;
  const Scene &scene;
  Object &ob_orig;
  Object &ob_eval;
  const bke::greasepencil::Layer &layer;
  int layer_index;
  int frame_number;
  float multi_frame_falloff;
  bke::greasepencil::Drawing &drawing;

  /* NOTE: accessing region in worker threads will return null,
   * this has to be done on the main thread and passed explicitly. */
  static GreasePencilStrokeParams from_context(const Scene &scene,
                                               Depsgraph &depsgraph,
                                               ARegion &region,
                                               RegionView3D &rv3d,
                                               Object &object,
                                               int layer_index,
                                               int frame_number,
                                               float multi_frame_falloff,
                                               bke::greasepencil::Drawing &drawing);
};

/* Point index mask for a drawing based on selection tool settings. */
IndexMask point_selection_mask(const GreasePencilStrokeParams &params,
                               const bool use_masking,
                               IndexMaskMemory &memory);
IndexMask stroke_selection_mask(const GreasePencilStrokeParams &params,
                                const bool use_masking,
                                IndexMaskMemory &memory);
IndexMask fill_selection_mask(const GreasePencilStrokeParams &params,
                              const bool use_masking,
                              IndexMaskMemory &memory);

bke::crazyspace::GeometryDeformation get_drawing_deformation(
    const GreasePencilStrokeParams &params);

/* Project points from layer space into 2D view space. */
Array<float2> calculate_view_positions(const GreasePencilStrokeParams &params,
                                       const IndexMask &selection);
Array<float> calculate_view_radii(const GreasePencilStrokeParams &params,
                                  const IndexMask &selection);

/* Get an appropriate projection function from screen space to layer space.
 * This is an alternative to using the DrawingPlacement. */
DeltaProjectionFunc get_screen_projection_fn(const GreasePencilStrokeParams &params,
                                             const Object &object,
                                             const bke::greasepencil::Layer &layer);

bool do_vertex_color_points(const Brush &brush);
bool do_vertex_color_fill(const Brush &brush);

/* Stroke operation base class that performs various common initializations. */
class GreasePencilStrokeOperationCommon : public GreasePencilStrokeOperation {
 public:
  using MutableDrawingInfo = blender::ed::greasepencil::MutableDrawingInfo;
  using DrawingPlacement = ed::greasepencil::DrawingPlacement;

  BrushStrokeMode stroke_mode;

  /** Initial mouse sample position, used for placement origin. */
  float2 start_mouse_position;
  /** Previous mouse position for computing the direction. */
  float2 prev_mouse_position;

  GreasePencilStrokeOperationCommon() {}
  GreasePencilStrokeOperationCommon(const BrushStrokeMode stroke_mode) : stroke_mode(stroke_mode)
  {
  }

  bool is_inverted(const Brush &brush) const;
  float2 mouse_delta(const InputSample &input_sample) const;

  void init_stroke(const bContext &C, const InputSample &start_sample);
  void stroke_extended(const InputSample &extension_sample);

  void foreach_editable_drawing(
      const bContext &C, FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const;
  void foreach_editable_drawing(
      const bContext &C,
      GrainSize grain_size,
      FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const;
  void foreach_editable_drawing(
      const bContext &C,
      FunctionRef<bool(const GreasePencilStrokeParams &params,
                       const DeltaProjectionFunc &projection_fn)> fn) const;
};

/* Operations */

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_erase_operation(bool temp_eraser);
std::unique_ptr<GreasePencilStrokeOperation> new_tint_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_draw_operation(
    const BrushStrokeMode &brush_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_blur_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_average_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_weight_paint_smear_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_smooth_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_thickness_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_strength_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_randomize_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_grab_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_push_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_pinch_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_twist_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_clone_operation(BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_vertex_average_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_vertex_blur_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_vertex_paint_operation(
    BrushStrokeMode stroke_mode);
std::unique_ptr<GreasePencilStrokeOperation> new_vertex_replace_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_vertex_smear_operation();

}  // namespace greasepencil

}  // namespace blender::ed::sculpt_paint
