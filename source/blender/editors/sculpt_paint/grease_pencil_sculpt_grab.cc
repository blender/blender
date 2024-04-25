/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "BLI_index_mask.hh"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_view3d_types.h"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class GrabOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;
  using MutableDrawingInfo = blender::ed::greasepencil::MutableDrawingInfo;
  using DrawingPlacement = ed::greasepencil::DrawingPlacement;

  /* Cached point mask and influence for a particular drawing. */
  struct PointWeights {
    int layer_index;
    int frame_number;
    float multi_frame_falloff;

    /* Layer space to view space projection at the start of the stroke. */
    float4x4 layer_to_win;
    /* Points that are grabbed at the beginning of the stroke. */
    IndexMask point_mask;
    /* Influence weights for grabbed points. */
    Vector<float> weights;

    IndexMaskMemory memory;
  };
  /* Cached point data for each affected drawing. */
  Array<PointWeights> drawing_data;

  void foreach_grabbed_drawing(const bContext &C,
                               FunctionRef<bool(const GreasePencilStrokeParams &params,
                                                const IndexMask &mask,
                                                Span<float> weights)> fn) const;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void GrabOperation::foreach_grabbed_drawing(
    const bContext &C,
    FunctionRef<bool(
        const GreasePencilStrokeParams &params, const IndexMask &mask, Span<float> weights)> fn)
    const
{
  const Scene &scene = *CTX_data_scene(&C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  const ARegion &region = *CTX_wm_region(&C);
  const View3D &view3d = *CTX_wm_view3d(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  bool changed = false;
  threading::parallel_for_each(this->drawing_data.index_range(), [&](const int i) {
    const PointWeights &data = this->drawing_data[i];
    if (data.point_mask.is_empty()) {
      return;
    }
    const bke::greasepencil::Layer &layer = *grease_pencil.layers()[data.layer_index];
    /* If a new frame is created, could be impossible find the stroke. */
    const int drawing_index = layer.drawing_index_at(data.frame_number);
    if (drawing_index < 0) {
      return;
    }
    GreasePencilDrawingBase &drawing_base = *grease_pencil.drawing(drawing_index);
    if (drawing_base.type != GP_DRAWING) {
      return;
    }
    bke::greasepencil::Drawing &drawing =
        reinterpret_cast<GreasePencilDrawing &>(drawing_base).wrap();

    GreasePencilStrokeParams params = GreasePencilStrokeParams::from_context(
        scene,
        depsgraph,
        region,
        view3d,
        object,
        data.layer_index,
        data.frame_number,
        data.multi_frame_falloff,
        drawing);
    if (fn(params, data.point_mask, data.weights)) {
      changed = true;
    }
  });

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &grease_pencil);
  }
}

void GrabOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  const ARegion &region = *CTX_wm_region(&C);
  const View3D &view3d = *CTX_wm_view3d(&C);
  const RegionView3D &rv3d = *CTX_wm_region_view3d(&C);
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  Brush &brush = *BKE_paint_brush(&paint);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(&C);
  Object &ob_orig = *CTX_data_active_object(&C);
  Object &ob_eval = *DEG_get_evaluated_object(&depsgraph, &ob_orig);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_orig.data);

  init_brush(brush);

  this->prev_mouse_position = start_sample.mouse_position;

  const Vector<MutableDrawingInfo> drawings = get_drawings_for_sculpt(C);
  this->drawing_data.reinitialize(drawings.size());
  threading::parallel_for_each(drawings.index_range(), [&](const int i) {
    const MutableDrawingInfo &info = drawings[i];
    BLI_assert(info.layer_index >= 0);
    PointWeights &data = this->drawing_data[i];

    const bke::greasepencil::Layer &layer = *grease_pencil.layers()[info.layer_index];
    BLI_assert(layer.drawing_index_at(info.frame_number) >= 0);
    BLI_assert(grease_pencil.get_drawing_at(layer, info.frame_number) == &info.drawing);

    ed::greasepencil::DrawingPlacement placement(scene, region, view3d, ob_eval, layer);
    GreasePencilStrokeParams params = {*scene.toolsettings,
                                       region,
                                       ob_orig,
                                       ob_eval,
                                       layer,
                                       info.layer_index,
                                       info.frame_number,
                                       info.multi_frame_falloff,
                                       std::move(placement),
                                       info.drawing};

    IndexMaskMemory selection_memory;
    IndexMask selection = point_selection_mask(params, selection_memory);

    Array<float2> view_positions = calculate_view_positions(params, selection);

    /* Cache points under brush influence. */
    Vector<float> weights;
    IndexMask point_mask = brush_influence_mask(scene,
                                                brush,
                                                start_sample.mouse_position,
                                                start_sample.pressure,
                                                info.multi_frame_falloff,
                                                selection,
                                                view_positions,
                                                weights,
                                                data.memory);

    if (point_mask.is_empty()) {
      /* Set empty point mask to skip. */
      data.point_mask = {};
      return;
    }
    data.layer_index = info.layer_index;
    data.frame_number = info.frame_number;
    data.multi_frame_falloff = info.multi_frame_falloff;
    data.layer_to_win = ED_view3d_ob_project_mat_get(&rv3d, &ob_eval) *
                        layer.to_object_space(ob_eval);
    data.point_mask = std::move(point_mask);
    data.weights = std::move(weights);
  });
}

void GrabOperation::on_stroke_extended(const bContext &C, const InputSample &extension_sample)
{
  const ARegion &region = *CTX_wm_region(&C);
  const RegionView3D &rv3d = *CTX_wm_region_view3d(&C);

  this->foreach_grabbed_drawing(
      C,
      [&](const GreasePencilStrokeParams &params,
          const IndexMask &mask,
          const Span<float> weights) {
        /* Crazyspace deformation. */
        bke::crazyspace::GeometryDeformation deformation = get_drawing_deformation(params);

        /* Transform mouse delta into layer space. */
        const float2 mouse_delta_win = this->mouse_delta(extension_sample);
        const float3 layer_origin = params.layer.to_world_space(params.ob_eval).location();
        const float zfac = ED_view3d_calc_zfac(&rv3d, layer_origin);
        float3 mouse_delta;
        ED_view3d_win_to_delta(&region, mouse_delta_win, zfac, mouse_delta);

        bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
        MutableSpan<float3> positions = curves.positions_for_write();
        mask.foreach_index(GrainSize(1024), [&](const int point_i, const int index) {
          /* Translate the point with the influence factor. */
          const float3 new_pos_layer = deformation.positions[point_i] +
                                       mouse_delta * weights[index];
          const float3 new_pos_world = math::transform_point(
              params.layer.to_world_space(params.ob_eval), new_pos_layer);
          float2 new_pos_view;
          ED_view3d_project_float_global(&region, new_pos_world, new_pos_view, V3D_PROJ_TEST_NOP);
          positions[point_i] = params.placement.project(new_pos_view);
        });

        params.drawing.tag_positions_changed();
        return true;
      });
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_grab_operation(const BrushStrokeMode stroke_mode)
{
  return std::make_unique<GrabOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
