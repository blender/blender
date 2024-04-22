/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BKE_context.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"

#include "transform.hh"
#include "transform_convert.hh"

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

  int total_number_of_drawings = 0;
  Vector<Vector<ed::greasepencil::MutableDrawingInfo>> all_drawings;
  /* Count the number layers in all objects. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    all_drawings.append(drawings);
    total_number_of_drawings += drawings.size();
  }

  int layer_offset = 0;
  Array<IndexMask> points_per_layer_per_object(total_number_of_drawings);

  /* Count selected elements per layer per object and create TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    CurvesTransformData *curves_transform_data = create_curves_transform_custom_data(
        tc.custom.type);

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings = all_drawings[i];
    for (ed::greasepencil::MutableDrawingInfo info : drawings) {
      if (use_proportional_edit) {
        points_per_layer_per_object[layer_offset] = ed::greasepencil::retrieve_editable_points(
            *object, info.drawing, curves_transform_data->memory);
        tc.data_len += points_per_layer_per_object[layer_offset].size();
      }
      else {
        points_per_layer_per_object[layer_offset] =
            ed::greasepencil::retrieve_editable_and_selected_points(
                *object, info.drawing, curves_transform_data->memory);
        tc.data_len += points_per_layer_per_object[layer_offset].size();
      }

      layer_offset++;
    }

    if (tc.data_len > 0) {
      tc.data = MEM_cnew_array<TransData>(tc.data_len, __func__);
      curves_transform_data->positions.reinitialize(tc.data_len);
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
    Object *object_eval = DEG_get_evaluated_object(depsgraph, tc.obedit);
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);
    Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();

    int layer_points_offset = 0;
    const Vector<ed::greasepencil::MutableDrawingInfo> drawings = all_drawings[i];
    for (ed::greasepencil::MutableDrawingInfo info : drawings) {
      const bke::greasepencil::Layer &layer = *layers[info.layer_index];
      const float4x4 layer_space_to_world_space = layer.to_world_space(*object_eval);
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      const IndexMask points = points_per_layer_per_object[layer_offset];

      std::optional<MutableSpan<float>> value_attribute;
      if (t->mode == TFM_CURVE_SHRINKFATTEN) {
        MutableSpan<float> radii = info.drawing.radii_for_write();
        value_attribute = radii;
      }
      else if (t->mode == TFM_GPENCIL_OPACITY) {
        MutableSpan<float> opacities = info.drawing.opacities_for_write();
        value_attribute = opacities;
      }

      const IndexMask affected_strokes = use_proportional_edit ?
                                             ed::greasepencil::retrieve_editable_strokes(
                                                 *object, info.drawing, memory) :
                                             IndexMask();
      curve_populate_trans_data_structs(tc,
                                        curves,
                                        layer_space_to_world_space,
                                        value_attribute,
                                        points,
                                        use_proportional_edit,
                                        affected_strokes,
                                        use_connected_only,
                                        layer_points_offset);

      layer_points_offset += points.size();
      layer_offset++;
    }
  }
}

static void recalcData_grease_pencil(TransInfo *t)
{
  bContext *C = t->context;
  Scene *scene = CTX_data_scene(C);

  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
        ed::greasepencil::retrieve_editable_drawings(*scene, grease_pencil);
    for (const int64_t i : drawings.index_range()) {
      ed::greasepencil::MutableDrawingInfo info = drawings[i];
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      copy_positions_from_curves_transform_custom_data(
          tc.custom.type, i, curves.positions_for_write());

      curves.calculate_bezier_auto_handles();
      curves.tag_positions_changed();
      info.drawing.tag_positions_changed();
    }

    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  }
}

}  // namespace blender::ed::transform::greasepencil

/** \} */

TransConvertTypeInfo TransConvertType_GreasePencil = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ blender::ed::transform::greasepencil::createTransGreasePencilVerts,
    /*recalc_data*/ blender::ed::transform::greasepencil::recalcData_grease_pencil,
    /*special_aftertrans_update*/ nullptr,
};
