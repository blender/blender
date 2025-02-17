/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "BLI_bounds.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "grease_pencil_intern.hh"
#include "paint_intern.hh"

#include <numeric>

namespace blender::ed::sculpt_paint::greasepencil {

class CloneOperation : public GreasePencilStrokeOperationCommon {
 public:
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext & /*C*/) override {}
};

void CloneOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  Main &bmain = *CTX_data_main(&C);
  Object &object = *CTX_data_active_object(&C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  this->init_stroke(C, start_sample);

  /* NOTE: Only one copy is created at the beginning of each stroke.
   * GPv2 supposedly has 2 modes:
   * - Stamp: Clone on stroke start and then transform (the transform part doesn't work)
   * - Continuous: Create multiple copies during the stroke (disabled)
   *
   * Here we only have the GPv2 behavior that actually works for now. */
  this->foreach_editable_drawing(
      C, [&](const GreasePencilStrokeParams &params, const DeltaProjectionFunc &projection_fn) {
        /* Only insert on the active layer. */
        if (&params.layer != grease_pencil.get_active_layer()) {
          return false;
        }

        /* TODO: Could become a tool setting. */
        const bool keep_world_transform = false;
        const float4x4 object_to_layer = math::invert(params.layer.to_object_space(object));
        const IndexRange pasted_curves = ed::greasepencil::paste_all_strokes_from_clipboard(
            bmain, object, object_to_layer, keep_world_transform, false, params.drawing);
        if (pasted_curves.is_empty()) {
          return false;
        }

        bke::CurvesGeometry &curves = params.drawing.strokes_for_write();
        const OffsetIndices<int> pasted_points_by_curve = curves.points_by_curve().slice(
            pasted_curves);
        const IndexRange pasted_points = IndexRange::from_begin_size(
            pasted_points_by_curve[0].start(), pasted_points_by_curve.total_size());
        if (pasted_points.is_empty()) {
          return false;
        }

        const Bounds<float3> bounds = *bounds::min_max(curves.positions().slice(pasted_points));
        const float4x4 transform = params.layer.to_world_space(params.ob_eval);
        /* FIXME: Projecting the center of the bounds to the view can sometimes fail. This might
         * result in unexpected behavior on the user end. Figure out a way to not rely on view
         * space here and compute the translation offset in layer space instead. */
        float2 view_center(0.0f);
        if (ED_view3d_project_float_global(&params.region,
                                           math::transform_point(transform, bounds.center()),
                                           view_center,
                                           V3D_PROJ_TEST_NOP) != V3D_PROJ_RET_OK)
        {
          return false;
        }

        const float2 &mouse_delta = start_sample.mouse_position - view_center;
        const bke::crazyspace::GeometryDeformation deformation = get_drawing_deformation(params);

        MutableSpan<float3> positions = curves.positions_for_write();
        threading::parallel_for(pasted_points, 4096, [&](const IndexRange range) {
          for (const int point_i : range) {
            positions[point_i] += compute_orig_delta(
                projection_fn, deformation, point_i, mouse_delta);
          }
        });
        params.drawing.tag_positions_changed();

        return true;
      });
}

void CloneOperation::on_stroke_extended(const bContext & /*C*/,
                                        const InputSample &extension_sample)
{
  this->stroke_extended(extension_sample);
}

std::unique_ptr<GreasePencilStrokeOperation> new_clone_operation(const BrushStrokeMode stroke_mode)
{
  return std::make_unique<CloneOperation>(stroke_mode);
}

}  // namespace blender::ed::sculpt_paint::greasepencil
