/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_scene_types.h"
#include "ED_grease_pencil.hh"

#include "paint_intern.hh"

namespace blender::bke::greasepencil {
class Drawing;
class Layer;
}  // namespace blender::bke::greasepencil
namespace blender::bke::crazyspace {
struct GeometryDeformation;
}

namespace blender::ed::sculpt_paint {

struct InputSample {
  float2 mouse_position;
  float pressure;
};

class GreasePencilStrokeOperation {
 public:
  virtual ~GreasePencilStrokeOperation() = default;
  virtual void on_stroke_begin(const bContext &C, const InputSample &start_sample) = 0;
  virtual void on_stroke_extended(const bContext &C, const InputSample &extension_sample) = 0;
  virtual void on_stroke_done(const bContext &C) = 0;
};

namespace greasepencil {

/* Get list of drawings the tool should be operating on. */
Vector<ed::greasepencil::MutableDrawingInfo> get_drawings_for_sculpt(const bContext &C);

/* Make sure the brush has all necessary grease pencil settings. */
void init_brush(Brush &brush);

/* Index mask of all points within the brush radius. */
IndexMask brush_influence_mask(const Scene &scene,
                               const Brush &brush,
                               const float2 &mouse_position,
                               float pressure,
                               float multi_frame_falloff,
                               const IndexMask &selection,
                               Span<float2> view_positions,
                               Vector<float> &influences,
                               IndexMaskMemory &memory);

/* Influence value at point co for the brush. */
float brush_influence(const Scene &scene,
                      const Brush &brush,
                      const float2 &co,
                      const InputSample &sample,
                      float multi_frame_falloff);

/* True if influence of the brush should be inverted. */
bool is_brush_inverted(const Brush &brush, BrushStrokeMode stroke_mode);

/* Common parameters for stroke callbacks that can be passed to utility functions. */
struct GreasePencilStrokeParams {
  const ToolSettings &toolsettings;
  const ARegion &region;
  Object &ob_orig;
  Object &ob_eval;
  const bke::greasepencil::Layer &layer;
  int layer_index;
  int frame_number;
  float multi_frame_falloff;
  const ed::greasepencil::DrawingPlacement &placement;
  bke::greasepencil::Drawing &drawing;

  /* NOTE: accessing region in worker threads will return null,
   * this has to be done on the main thread and passed explicitly. */
  static GreasePencilStrokeParams from_context(const Scene &scene,
                                               Depsgraph &depsgraph,
                                               ARegion &region,
                                               Object &object,
                                               int layer_index,
                                               int frame_number,
                                               float multi_frame_falloff,
                                               const ed::greasepencil::DrawingPlacement &placement,
                                               bke::greasepencil::Drawing &drawing);
};

/* Point index mask for a drawing based on selection tool settings. */
IndexMask point_selection_mask(const GreasePencilStrokeParams &params, IndexMaskMemory &memory);

bke::crazyspace::GeometryDeformation get_drawing_deformation(
    const GreasePencilStrokeParams &params);

/* Project points from layer space into 2D view space. */
Array<float2> calculate_view_positions(const GreasePencilStrokeParams &params,
                                       const IndexMask &selection);

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

  GreasePencilStrokeOperationCommon(const BrushStrokeMode stroke_mode) : stroke_mode(stroke_mode)
  {
  }

  bool is_inverted(const Brush &brush) const;
  float2 mouse_delta(const InputSample &input_sample) const;

  void init_stroke(const bContext &C, const InputSample &start_sample);
  void stroke_extended(const InputSample &extension_sample);

  void foreach_editable_drawing(
      const bContext &C, FunctionRef<bool(const GreasePencilStrokeParams &params)> fn) const;
};

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation();
std::unique_ptr<GreasePencilStrokeOperation> new_erase_operation();
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

}  // namespace greasepencil

}  // namespace blender::ed::sculpt_paint
