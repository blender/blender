/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_grease_pencil_fills.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "BLI_bounds.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"
#include "DNA_windowmanager_types.h"

#include "WM_api.hh"

#include "GEO_resample_curves.hh"
#include "GEO_smooth_curves.hh"

namespace blender {

namespace ed::greasepencil {

/**
 * Apply the stroke carver to a drawing.
 */
static bool execute_carver_on_drawing(const Object &ob_eval,
                                      Object &obact,
                                      const ARegion &region,
                                      const float4x4 &projection,
                                      const float4x4 &layer_to_world,
                                      const DrawingPlacement &placement,
                                      const Span<float2> mcoords,
                                      const bool keep_caps,
                                      bke::greasepencil::Drawing &drawing,
                                      bool *r_converted)
{
  const bke::CurvesGeometry &src = drawing.strokes();
  const OffsetIndices<int> src_points_by_curve = src.points_by_curve();

  /* Get evaluated geometry. */
  bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(&ob_eval, obact, drawing);

  const Span<float3> normals = drawing.curve_plane_normals();

  Array<float4> curve_planes(src.curves_num());
  threading::parallel_for(src.curves_range(), 4096, [&](const IndexRange src_curves) {
    for (const int src_curve : src_curves) {
      const float3 &normal = normals[src_curve];
      const IndexRange points = src_points_by_curve[src_curve];
      const float3 &point = deformation.positions[points.first()];
      curve_planes[src_curve] = float4(normal, -math::dot(point, normal));
    }
  });

  bke::CurvesGeometry input_curves = bke::CurvesGeometry(src);
  input_curves.resize(src.points_num() + mcoords.size(), src.curves_num() + 1);
  input_curves.offsets_for_write().last() = src.points_num() + mcoords.size();

  bke::MutableAttributeAccessor attributes = input_curves.attributes_for_write();

  placement.project(mcoords, input_curves.positions_for_write().take_back(mcoords.size()));

  bke::SpanAttributeWriter<int> fill_ids = attributes.lookup_or_add_for_write_span<int>(
      "fill_id", bke::AttrDomain::Curve);
  fill_ids.span.last() = bke::greasepencil::get_next_available_fill_id(fill_ids.span.varray());

  const bke::greasepencil::ShapeData shapes_data = bke::greasepencil::shapes_from_fill_ids(
      fill_ids.span.varray(), input_curves.curves_num());

  fill_ids.finish();

  const IndexRange clipping_points = IndexRange::from_begin_size(src.points_num(), mcoords.size());
  const IndexRange clipping_curves = IndexRange::from_single(src.curves_num());

  input_curves.fill_curve_types(clipping_curves, CURVE_TYPE_POLY);
  input_curves.cyclic_for_write().last() = true;

  /* Initialize the rest of the attributes with default values. */
  bke::fill_attribute_range_default(attributes,
                                    bke::AttrDomain::Point,
                                    bke::attribute_filter_from_skip_ref({"position"}),
                                    clipping_points);
  bke::fill_attribute_range_default(
      attributes,
      bke::AttrDomain::Curve,
      bke::attribute_filter_from_skip_ref({"fill_id", "cyclic", "curve_type"}),
      clipping_curves);

  auto project_fn = [&](const float3 &position) {
    return ED_view3d_project_float_v2_m4(&region, position, projection);
  };

  /* WORKAROUND. Currently only poly curves are supported. Convert any curve close to the clipping
   * shape to poly. */
  if (!input_curves.is_single_type(CURVE_TYPE_POLY)) {
    const OffsetIndices<int> points_by_curve = input_curves.points_by_curve();
    Array<float2> src_positions_2d(input_curves.points_num());
    const Span<float3> positions = input_curves.positions();
    for (const int i : input_curves.points_range()) {
      src_positions_2d[i] = project_fn(positions[i]);
    }

    Array<Bounds<float2>> screen_space_bbox(input_curves.curves_num());
    threading::parallel_for(points_by_curve.index_range(), 512, [&](const IndexRange curves_i) {
      for (const int curve_i : curves_i) {
        const IndexRange points_i = points_by_curve[curve_i];
        screen_space_bbox[curve_i] = *bounds::min_max(src_positions_2d.as_span().slice(points_i));
      }
    });

    const VArray<int8_t> curve_types = input_curves.curve_types();

    const Bounds<float2> bbox_j = screen_space_bbox.last();

    IndexMaskMemory memory;
    IndexMask curves_to_convert = IndexMask::from_predicate(
        input_curves.curves_range(), memory, [&](const int64_t curve_i) {
          const Bounds<float2> bbox_i = screen_space_bbox[curve_i];

          /* Poly curve do not need to be converted. */
          if (curve_types[curve_i] == CURVE_TYPE_POLY) {
            return false;
          }

          if (bounds::intersect(bbox_i, bbox_j).has_value()) {
            return true;
          }
          return false;
        });
    curves_to_convert = bke::greasepencil::selected_mask_to_fills(
        curves_to_convert, input_curves, bke::AttrDomain::Curve, memory);

    if (!curves_to_convert.is_empty()) {
      input_curves = geometry::resample_to_evaluated(input_curves, curves_to_convert);
      *r_converted = true;
    }
  }

  boolean::CurveBooleanOpParameters op_params;
  op_params.boolean_mode = boolean::Operation::Difference;
  op_params.keep_caps = keep_caps;
  op_params.skip_clipping_attributes = true;
  op_params.separate_islands = true;

  const GroupedSpan<int> shapes = shapes_data.shapes();
  const IndexRange clipping_shapes = IndexRange::from_single(shapes.size() - 1);

  bke::CurvesGeometry carved_strokes = boolean::curve_boolean_with_planes(op_params,
                                                                          input_curves,
                                                                          project_fn,
                                                                          shapes,
                                                                          curve_planes,
                                                                          clipping_shapes,
                                                                          layer_to_world,
                                                                          region);

  /* Set the new geometry. */
  drawing.strokes_for_write() = std::move(carved_strokes);
  drawing.tag_topology_changed();

  return true;
}

/**
 * Apply the stroke carver to all layers.
 */
static wmOperatorStatus grease_pencil_stroke_carver_exec(bContext *C, wmOperator *op)
{
  const Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  const Scene *scene = CTX_data_scene(C);
  const ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  View3D *view3d = CTX_wm_view3d(C);
  Object *obact = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated(depsgraph, obact);

  GreasePencil &grease_pencil = *id_cast<GreasePencil *>(obact->data);

  Array<float2> coords(mcoords.size());
  threading::parallel_for(mcoords.index_range(), 4096, [&](const IndexRange i_range) {
    for (const int i : i_range) {
      coords[i] = float2(mcoords[i]);
    }
  });

  Array<float2> lasso_pos(coords.size());
  const int smooth_iterations = 4;
  const float smooth_factor = 0.8f;
  geometry::gaussian_blur_1D(coords.as_span(),
                             smooth_iterations,
                             VArray<float>::from_single(smooth_factor, coords.size()),
                             true,
                             true,
                             false,
                             lasso_pos.as_mutable_span());

  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  if (brush->gpencil_settings == nullptr) {
    BKE_brush_init_gpencil_settings(brush);
  }
  const bool keep_caps = (brush->gpencil_settings->flag & GP_BRUSH_ERASER_KEEP_CAPS) != 0;
  const bool active_layer_only = (brush->gpencil_settings->flag & GP_BRUSH_ACTIVE_LAYER_ONLY) != 0;
  std::atomic<bool> changed = false;
  std::atomic<bool> converted = false;

  bool inserted_keyframe = false;
  if (active_layer_only) {
    /* Apply carver on drawings of active layer. */
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    bke::greasepencil::Layer &layer = *grease_pencil.get_active_layer();
    if (!layer.is_editable()) {
      return OPERATOR_CANCELLED;
    }

    ensure_active_keyframe(*scene, grease_pencil, layer, true, inserted_keyframe);
    const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
    const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings_from_layer(*scene, grease_pencil, layer);

    /* Initialize helper class for projecting screen space coordinates. */
    DrawingPlacement placement = ed::greasepencil::DrawingPlacement(
        *scene, *region, *view3d, *ob_eval, &layer);

    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      bool r_converted = false;
      if (execute_carver_on_drawing(*ob_eval,
                                    *obact,
                                    *region,
                                    projection,
                                    layer_to_world,
                                    placement,
                                    lasso_pos.as_span(),
                                    keep_caps,
                                    info.drawing,
                                    &r_converted))
      {
        changed = true;
      }

      if (r_converted) {
        converted = true;
      }
    });
  }
  else {
    for (bke::greasepencil::Layer *layer : grease_pencil.layers_for_write()) {
      if (layer->is_editable()) {
        ed::greasepencil::ensure_active_keyframe(
            *scene, grease_pencil, *layer, true, inserted_keyframe);
      }
    }

    /* Apply carver on every editable drawing. */
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    threading::parallel_for_each(drawings, [&](const ed::greasepencil::MutableDrawingInfo &info) {
      const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
      const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
      const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(rv3d, layer_to_world);

      /* Initialize helper class for projecting screen space coordinates. */
      DrawingPlacement placement = ed::greasepencil::DrawingPlacement(
          *scene, *region, *view3d, *ob_eval, &layer);

      bool r_converted = false;
      if (execute_carver_on_drawing(*ob_eval,
                                    *obact,
                                    *region,
                                    projection,
                                    layer_to_world,
                                    placement,
                                    lasso_pos,
                                    keep_caps,
                                    info.drawing,
                                    &r_converted))
      {
        changed = true;
      }

      if (r_converted) {
        converted = true;
      }
    });
  }

  if (converted) {
    BKE_report(op->reports, RPT_WARNING, "Some curves were converted to Poly");
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    if (inserted_keyframe) {
      WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
    }
  }

  return OPERATOR_FINISHED;
}

}  // namespace ed::greasepencil

void GREASE_PENCIL_OT_stroke_carver(wmOperatorType *ot)
{
  using namespace ed::greasepencil;

  ot->name = "Grease Pencil Carver";
  ot->idname = "GREASE_PENCIL_OT_stroke_carver";
  ot->description = "Cuts stroke point in the intersect lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = grease_pencil_stroke_carver_exec;
  ot->poll = grease_pencil_painting_poll;
  ot->cancel = WM_gesture_lasso_cancel;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  WM_operator_properties_gesture_lasso(ot);
}

}  // namespace blender
