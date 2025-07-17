/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_grease_pencil_softness_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Grease Pencil")
      .supported_type(GeometryComponent::Type::GreasePencil)
      .align_with_previous()
      .description("Grease Pencil to set the softness of");
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Softness").default_value(0.0f).min(0.0f).max(1.0f).field_on_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Grease Pencil");
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<float> softness = params.extract_input<Field<float>>("Softness");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
    if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
      using namespace bke::greasepencil;
      for (const int layer_index : grease_pencil->layers().index_range()) {
        Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
        if (drawing == nullptr) {
          continue;
        }
        bke::CurvesGeometry &curves = drawing->strokes_for_write();

        const bke::GreasePencilLayerFieldContext layer_field_context(
            *grease_pencil, AttrDomain::Curve, layer_index);

        bke::try_capture_fields_on_geometry(curves.attributes_for_write(),
                                            layer_field_context,
                                            {"softness"},
                                            AttrDomain::Curve,
                                            selection,
                                            {softness});
      }
    }
  });

  params.set_output("Grease Pencil", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGreasePencilSoftness");
  ntype.ui_name = "Set Grease Pencil Softness";
  ntype.ui_description = "Set softness attribute on Grease Pencil geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 180, 120, NODE_DEFAULT_MAX_WIDTH);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_grease_pencil_softness_cc
