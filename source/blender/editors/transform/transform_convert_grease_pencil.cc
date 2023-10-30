/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_math_matrix.h"

#include "BKE_context.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Grease Pencil Transform Creation
 * \{ */

namespace blender::ed::transform::greasepencil {

static void createTransGreasePencilVerts(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_scene(C);
  MutableSpan<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  const bool use_proportional_edit = (t->flag & T_PROP_EDIT_ALL) != 0;
  const bool use_connected_only = (t->flag & T_PROP_CONNECTED) != 0;

  int number_of_layers_total = 0;
  /* Count the number layers in all objects. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    grease_pencil.foreach_editable_drawing(
        scene->r.cfra, [&](int /*layer_index*/, bke::greasepencil::Drawing & /*drawing*/) {
          number_of_layers_total++;
        });
  }

  int layer_offset = 0;
  IndexMaskMemory memory;
  Array<IndexMask> selection_per_layer_per_object(number_of_layers_total);

  /* Count selected elements per layer per object and create TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    grease_pencil.foreach_editable_drawing(
        scene->r.cfra, [&](int /*layer_index*/, bke::greasepencil::Drawing &drawing) {
          const bke::CurvesGeometry &curves = drawing.strokes();

          if (use_proportional_edit) {
            tc.data_len += curves.point_num;
          }
          else {
            selection_per_layer_per_object[layer_offset] = ed::curves::retrieve_selected_points(
                curves, memory);
            tc.data_len += selection_per_layer_per_object[layer_offset].size();
          }

          layer_offset++;
        });

    if (tc.data_len > 0) {
      tc.data = MEM_cnew_array<TransData>(tc.data_len, __func__);
    }
  }

  /* Reuse the variable `layer_offset` */
  layer_offset = 0;

  /* Populate TransData structs. */
  for (const int i : trans_data_contrainers.index_range()) {
    TransDataContainer &tc = trans_data_contrainers[i];
    if (tc.data_len == 0) {
      continue;
    }
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    float mtx[3][3], smtx[3][3];
    copy_m3_m4(mtx, tc.obedit->object_to_world);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    int layer_points_offset = 0;

    grease_pencil.foreach_editable_drawing(
        scene->r.cfra, [&](int /*layer_index*/, bke::greasepencil::Drawing &drawing) {
          bke::CurvesGeometry &curves = drawing.strokes_for_write();
          const IndexMask selected_indices = selection_per_layer_per_object[layer_offset];

          std::optional<MutableSpan<float>> value_attribute;
          if (t->mode == TFM_CURVE_SHRINKFATTEN) {
            MutableSpan<float> radii = drawing.radii_for_write();
            value_attribute = radii;
          }
          else if (t->mode == TFM_GPENCIL_OPACITY) {
            MutableSpan<float> opacities = drawing.opacities_for_write();
            value_attribute = opacities;
          }

          curve_populate_trans_data_structs(tc,
                                            curves,
                                            value_attribute,
                                            selected_indices,
                                            use_proportional_edit,
                                            use_connected_only,
                                            layer_points_offset);

          if (use_proportional_edit) {
            layer_points_offset += curves.points_num();
          }
          else {
            layer_points_offset += selected_indices.size();
          }
          layer_offset++;
        });
  }
}

static void recalcData_grease_pencil(TransInfo *t)
{
  bContext *C = t->context;
  Scene *scene = CTX_data_scene(C);

  const Span<TransDataContainer> trans_data_contrainers(t->data_container, t->data_container_len);
  for (const TransDataContainer &tc : trans_data_contrainers) {
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(tc.obedit->data);

    grease_pencil.foreach_editable_drawing(
        scene->r.cfra, [&](int /*layer_index*/, bke::greasepencil::Drawing &drawing) {
          bke::CurvesGeometry &curves = drawing.strokes_for_write();

          curves.calculate_bezier_auto_handles();
          curves.tag_positions_changed();
          drawing.tag_positions_changed();
        });

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
