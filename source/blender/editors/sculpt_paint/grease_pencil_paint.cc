/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

class PaintOperation : public GreasePencilStrokeOperation {

 public:
  ~PaintOperation() override {}

  void on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension) override;
  void on_stroke_done(const bContext &C) override;
};

/**
 * Utility class that actually executes the update when the stroke is updated. That's useful
 * because it avoids passing a very large number of parameters between functions.
 */
struct PaintOperationExecutor {

  PaintOperationExecutor(const bContext & /*C*/) {}

  void execute(PaintOperation & /*self*/,
               const bContext &C,
               const StrokeExtension &stroke_extension)
  {
    using namespace blender::bke;
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
    Scene *scene = CTX_data_scene(&C);
    ARegion *region = CTX_wm_region(&C);
    Object *obact = CTX_data_active_object(&C);
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

    /**
     * Note: We write to the evaluated object here, so that the additional copy from orig -> eval
     * is not needed for every update. After the stroke is done, the result is written to the
     * original object.
     */
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob_eval->data);
    if (!grease_pencil.has_active_layer()) {
      /* TODO: create a new layer. */
      BLI_assert_unreachable();
      // grease_pencil.runtime->set_active_layer_index(0);
    }
    const bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();
    int index = active_layer.drawing_index_at(scene->r.cfra);
    BLI_assert(index != -1);

    GreasePencilDrawing &drawing = *reinterpret_cast<GreasePencilDrawing *>(
        grease_pencil.drawings()[index]);

    float4 plane{0.0f, -1.0f, 0.0f, 0.0f};
    float3 proj_pos;
    ED_view3d_win_to_3d_on_plane(region, plane, stroke_extension.mouse_position, false, proj_pos);

    bke::greasepencil::StrokePoint new_point{
        proj_pos, stroke_extension.pressure * 100.0f, 1.0f, float4(1.0f)};

    drawing.runtime->stroke_cache.points.append(std::move(new_point));

    BKE_grease_pencil_batch_cache_dirty_tag(&grease_pencil, BKE_GREASEPENCIL_BATCH_DIRTY_ALL);
  }
};

void PaintOperation::on_stroke_extended(const bContext &C, const StrokeExtension &stroke_extension)
{
  PaintOperationExecutor executor{C};
  executor.execute(*this, C, stroke_extension);
}

void PaintOperation::on_stroke_done(const bContext &C)
{
  using namespace blender::bke;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);
  Scene *scene = CTX_data_scene(&C);
  Object *obact = CTX_data_active_object(&C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, obact);

  GreasePencil &grease_pencil_orig = *static_cast<GreasePencil *>(obact->data);
  GreasePencil &grease_pencil_eval = *static_cast<GreasePencil *>(ob_eval->data);
  BLI_assert(grease_pencil_orig.has_active_layer() && grease_pencil_eval.has_active_layer());
  const bke::greasepencil::Layer &active_layer_orig = *grease_pencil_orig.get_active_layer();
  const bke::greasepencil::Layer &active_layer_eval = *grease_pencil_eval.get_active_layer();
  int index_orig = active_layer_orig.drawing_index_at(scene->r.cfra);
  int index_eval = active_layer_eval.drawing_index_at(scene->r.cfra);
  BLI_assert(index_orig != -1 && index_eval != -1);

  GreasePencilDrawing &drawing_orig = *reinterpret_cast<GreasePencilDrawing *>(
      grease_pencil_orig.drawings()[index_orig]);
  GreasePencilDrawing &drawing_eval = *reinterpret_cast<GreasePencilDrawing *>(
      grease_pencil_eval.drawings()[index_eval]);

  const Span<bke::greasepencil::StrokePoint> stroke_points = drawing_eval.stroke_buffer();
  CurvesGeometry &curves = drawing_orig.geometry.wrap();

  int num_old_curves = curves.curves_num();
  int num_old_points = curves.points_num();
  curves.resize(num_old_points + stroke_points.size(), num_old_curves + 1);

  curves.offsets_for_write()[num_old_curves] = num_old_points;
  curves.offsets_for_write()[num_old_curves + 1] = num_old_points + stroke_points.size();

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const IndexRange new_points_range = points_by_curve[curves.curves_num() - 1];
  const IndexRange new_curves_range = IndexRange(num_old_curves, 1);

  /* Set position, radius and opacity attribute. */
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  MutableSpan<float3> positions = curves.positions_for_write();
  SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", ATTR_DOMAIN_POINT);
  SpanAttributeWriter<float> opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", ATTR_DOMAIN_POINT);
  for (const int i : IndexRange(stroke_points.size())) {
    const bke::greasepencil::StrokePoint &point = stroke_points[i];
    const int point_i = new_points_range[i];
    positions[point_i] = point.position;
    radii.span[point_i] = point.radius;
    opacities.span[point_i] = point.opacity;
  }

  /* Set material index attribute. */
  int material_index = 0;
  SpanAttributeWriter<int> materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", ATTR_DOMAIN_CURVE);

  materials.span.slice(new_curves_range).fill(material_index);

  /* Set curve_type attribute. */
  curves.fill_curve_types(new_curves_range, CURVE_TYPE_POLY);

  /* Explicitly set all other attributes besides those processed above to default values. */
  Set<std::string> attributes_to_skip{
      {"position", "radius", "opacity", "material_index", "curve_type"}};
  attributes.for_all(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData /*meta_data*/) {
        if (attributes_to_skip.contains(id.name())) {
          return true;
        }
        bke::GSpanAttributeWriter attribute = attributes.lookup_for_write_span(id);
        const CPPType &type = attribute.span.type();
        GMutableSpan new_data = attribute.span.slice(
            attribute.domain == ATTR_DOMAIN_POINT ? new_points_range : new_curves_range);
        type.fill_assign_n(type.default_value(), new_data.data(), new_data.size());
        attribute.finish();
        return true;
      });

  drawing_eval.runtime->stroke_cache.clear();
  drawing_orig.tag_positions_changed();

  radii.finish();
  opacities.finish();
  materials.finish();

  DEG_id_tag_update(&grease_pencil_orig.id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &grease_pencil_orig.id);
}

std::unique_ptr<GreasePencilStrokeOperation> new_paint_operation()
{
  return std::make_unique<PaintOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
