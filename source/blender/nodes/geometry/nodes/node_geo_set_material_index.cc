/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

namespace blender::nodes::node_geo_set_material_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("Material Index").min(0).field_on_all();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void set_material_index_in_grease_pencil(GreasePencil &grease_pencil,
                                                const Field<bool> &selection,
                                                const Field<int> &material_index)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = grease_pencil.get_eval_drawing(*grease_pencil.layer(layer_index));
    if (drawing == nullptr) {
      continue;
    }
    bke::try_capture_field_on_geometry(
        drawing->strokes_for_write().attributes_for_write(),
        bke::GreasePencilLayerFieldContext(grease_pencil, AttrDomain::Curve, layer_index),
        "material_index",
        AttrDomain::Curve,
        selection,
        material_index);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<int> material_index = params.extract_input<Field<int>>("Material Index");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      bke::try_capture_field_on_geometry(mesh->attributes_for_write(),
                                         bke::MeshFieldContext(*mesh, AttrDomain::Face),
                                         "material_index",
                                         AttrDomain::Face,
                                         selection,
                                         material_index);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      set_material_index_in_grease_pencil(*grease_pencil, selection, material_index);
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_MATERIAL_INDEX, "Set Material Index", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_material_index_cc
