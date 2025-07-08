/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "ANIM_keyframing.hh"

#include "BKE_context.hh"
#include "BKE_curves_utils.hh"

#include "BLI_index_mask_expression.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Transform Creation
 * \{ */

namespace blender::ed::transform::greasepencil {

static void createTransGreasePencilVerts(bContext *C, TransInfo *t)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;
  const bool use_connected_only = (t->flag & T_PROP_CONNECTED) != 0;
  const bool use_individual_origins = (t->around == V3D_AROUND_LOCAL_ORIGINS);
  ToolSettings *ts = scene->toolsettings;
  const bool is_scale_thickness = ((t->mode == TFM_CURVE_SHRINKFATTEN) ||
                                   (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_SCALE_THICKNESS));

  int total_number_of_drawings = 0;
  Vector<Vector<ed::greasepencil::MutableDrawingInfo>> all_drawings;
  /* Count the number layers in all objects. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings_with_falloff(*scene, grease_pencil);

    if (animrig::is_autokey_on(scene)) {
      for (const int info_i : drawings.index_range()) {
        bke::greasepencil::Layer &target_layer = grease_pencil.layer(drawings[info_i].layer_index);
        const int current_frame = scene->r.cfra;
        std::optional<int> start_frame = target_layer.start_frame_at(current_frame);
        if (start_frame.has_value() && (start_frame.value() != current_frame)) {
          grease_pencil.insert_duplicate_frame(
              target_layer, *target_layer.start_frame_at(current_frame), current_frame, false);
        }
      }
      drawings = ed::greasepencil::retrieve_editable_drawings_with_falloff(*scene, grease_pencil);
    }

    all_drawings.append(drawings);
    total_number_of_drawings += drawings.size();
  }

  Array<Vector<IndexMask>> points_to_transform_per_attribute(total_number_of_drawings);
  Array<IndexMask> bezier_curves(total_number_of_drawings);
  int layer_offset = 0;

  /* Count selected elements per layer per object and create TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    CurvesTransformData *curves_transform_data = curves::create_curves_transform_custom_data(
        tc.custom.type);
    tc.data_len = 0;

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings = all_drawings[i];
    curves_transform_data->grease_pencil_falloffs.reinitialize(drawings.size());
    for (ed::greasepencil::MutableDrawingInfo info : drawings) {
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      Span<StringRef> selection_attribute_names = ed::curves::get_curves_selection_attribute_names(
          curves);
      std::array<IndexMask, 3> selection_per_attribute;

      const IndexMask editable_points = ed::greasepencil::retrieve_editable_points(
          *object, info.drawing, info.layer_index, curves_transform_data->memory);
      const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
          *object, info.drawing, info.layer_index, curves_transform_data->memory);

      for (const int attribute_i : selection_attribute_names.index_range()) {
        const StringRef &selection_name = selection_attribute_names[attribute_i];
        selection_per_attribute[attribute_i] = ed::curves::retrieve_selected_points(
            curves, selection_name, curves_transform_data->memory);

        /* Make sure only editable points are used. */
        selection_per_attribute[attribute_i] = IndexMask::from_intersection(
            selection_per_attribute[attribute_i], editable_points, curves_transform_data->memory);
      }

      bezier_curves[layer_offset] = bke::curves::indices_for_type(curves.curve_types(),
                                                                  curves.curve_type_counts(),
                                                                  CURVE_TYPE_BEZIER,
                                                                  editable_strokes,
                                                                  curves_transform_data->memory);
      const OffsetIndices<int> points_by_curve = curves.points_by_curve();
      const IndexMask bezier_points = IndexMask::from_ranges(
          points_by_curve, bezier_curves[layer_offset], curves_transform_data->memory);

      /* Alter selection as in legacy curves bezt_select_to_transform_triple_flag(). */
      if (!bezier_points.is_empty()) {
        if (curves::update_handle_types_for_transform(
                curves, selection_per_attribute, bezier_points))
        {
          info.drawing.tag_topology_changed();
        }

        index_mask::ExprBuilder builder;
        const index_mask::Expr &selected_bezier_points = builder.intersect(
            {&bezier_points, &selection_per_attribute[0]});

        /* Select bezier handles that must be transformed because the control point is
         * selected. */
        selection_per_attribute[1] = evaluate_expression(
            builder.merge({&selection_per_attribute[1], &selected_bezier_points}),
            curves_transform_data->memory);
        selection_per_attribute[2] = evaluate_expression(
            builder.merge({&selection_per_attribute[2], &selected_bezier_points}),
            curves_transform_data->memory);
      }

      if (use_proportional_edit) {
        tc.data_len += editable_points.size() + 2 * bezier_points.size();
        points_to_transform_per_attribute[layer_offset].append(editable_points);

        if (selection_attribute_names.size() > 1) {
          points_to_transform_per_attribute[layer_offset].append(bezier_points);
          points_to_transform_per_attribute[layer_offset].append(bezier_points);
        }
      }
      else {
        for (const int selection_i : selection_attribute_names.index_range()) {
          points_to_transform_per_attribute[layer_offset].append(
              selection_per_attribute[selection_i]);
          tc.data_len += points_to_transform_per_attribute[layer_offset][selection_i].size();
        }
      }

      layer_offset++;
    }

    if (tc.data_len > 0) {
      tc.data = MEM_calloc_arrayN<TransData>(tc.data_len, __func__);
      curves_transform_data->positions.reinitialize(tc.data_len);
    }
    else {
      tc.custom.type.free_cb(t, &tc, &tc.custom.type);
    }
  }

  /* Reuse the variable `layer_offset`. */
  layer_offset = 0;
  IndexMaskMemory memory;

  /* Populate TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    if (tc.data_len == 0) {
      continue;
    }
    Object *object_eval = DEG_get_evaluated(depsgraph, tc.obedit);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);
    Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings = all_drawings[i];

    CurvesTransformData &transform_data = *static_cast<CurvesTransformData *>(tc.custom.type.data);
    transform_data.aligned_with_left.reinitialize(drawings.size());
    transform_data.aligned_with_right.reinitialize(drawings.size());

    for (const int drawing : drawings.index_range()) {
      ed::greasepencil::MutableDrawingInfo info = drawings[drawing];
      const bke::greasepencil::Layer &layer = *layers[info.layer_index];
      const float4x4 layer_space_to_world_space = layer.to_world_space(*object_eval);
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      const bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
              *CTX_data_depsgraph_pointer(C), *object, info.drawing);

      std::optional<MutableSpan<float>> value_attribute;
      if (t->mode == TFM_GPENCIL_OPACITY) {
        MutableSpan<float> opacities = info.drawing.opacities_for_write();
        value_attribute = opacities;
      }
      else if (is_scale_thickness) {
        MutableSpan<float> radii = info.drawing.radii_for_write();
        value_attribute = radii;
      }

      const IndexMask affected_strokes = use_proportional_edit || use_individual_origins ?
                                             ed::greasepencil::retrieve_editable_strokes(
                                                 *object, info.drawing, info.layer_index, memory) :
                                             IndexMask();

      CurvesTransformData &curves_transform_data = *static_cast<CurvesTransformData *>(
          tc.custom.type.data);
      curves_transform_data.grease_pencil_falloffs[drawing] = info.multi_frame_falloff;
      float &drawing_falloff = curves_transform_data.grease_pencil_falloffs[drawing];
      curves::curve_populate_trans_data_structs(*t,
                                                tc,
                                                curves,
                                                layer_space_to_world_space,
                                                deformation,
                                                value_attribute,
                                                points_to_transform_per_attribute[layer_offset],
                                                affected_strokes,
                                                use_connected_only,
                                                bezier_curves[layer_offset],
                                                &drawing_falloff);
      curves::create_aligned_handles_masks(
          curves, points_to_transform_per_attribute[layer_offset], drawing, tc.custom.type);

      layer_offset++;
    }
  }
}

static void recalcData_grease_pencil(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    transform_snap_project_individual_apply(t);
  }

  bContext *C = t->context;
  Scene *scene = CTX_data_scene(C);

  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);

    int layer_i = 0;
    for (const int64_t i : drawings.index_range()) {
      ed::greasepencil::MutableDrawingInfo info = drawings[i];
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

      if (t->mode == TFM_CURVE_SHRINKFATTEN) {
        curves.tag_radii_changed();
      }
      else if (t->mode == TFM_TILT) {
        curves.tag_normals_changed();
      }
      else {
        const Vector<MutableSpan<float3>> positions_per_selection_attr =
            ed::curves::get_curves_positions_for_write(curves);
        for (MutableSpan<float3> positions : positions_per_selection_attr) {
          curves::copy_positions_from_curves_transform_custom_data(
              tc.custom.type, layer_i++, positions);
        }
        curves.tag_positions_changed();
        curves.calculate_bezier_auto_handles();
        info.drawing.tag_positions_changed();
        curves::calculate_aligned_handles(tc.custom.type, curves, i);
      }
    }

    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_GreasePencil = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransGreasePencilVerts,
    /*recalc_data*/ recalcData_grease_pencil,
    /*special_aftertrans_update*/ nullptr,
};

}  // namespace blender::ed::transform::greasepencil
