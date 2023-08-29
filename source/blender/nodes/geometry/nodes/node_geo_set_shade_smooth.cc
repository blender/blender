/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_shade_smooth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Bool>("Shade Smooth").field_on_all().default_value(true);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

/**
 * When the `sharp_face` attribute doesn't exist, all faces are considered smooth. If all faces
 * are selected and the sharp value is a constant false value, we can remove the attribute instead
 * as an optimization to avoid storing it and propagating it in the future.
 */
static bool try_removing_sharp_attribute(Mesh &mesh,
                                         const Field<bool> &selection_field,
                                         const Field<bool> &sharp_field)
{
  if (selection_field.node().depends_on_input() || sharp_field.node().depends_on_input()) {
    return false;
  }
  const bool selection = fn::evaluate_constant_field(selection_field);
  if (!selection) {
    return true;
  }
  const bool sharp = fn::evaluate_constant_field(sharp_field);
  if (sharp) {
    return false;
  }
  mesh.attributes_for_write().remove("sharp_face");
  return true;
}

static void set_sharp_faces(Mesh &mesh,
                            const Field<bool> &selection_field,
                            const Field<bool> &sharp_field)
{
  if (mesh.faces_num == 0) {
    return;
  }
  if (try_removing_sharp_attribute(mesh, selection_field, sharp_field)) {
    return;
  }

  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  AttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write<bool>("sharp_face",
                                                                               ATTR_DOMAIN_FACE);

  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  fn::FieldEvaluator evaluator{field_context, mesh.faces_num};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(sharp_field, sharp_faces.varray);
  evaluator.evaluate();

  sharp_faces.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<bool> smooth_field = params.extract_input<Field<bool>>("Shade Smooth");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      set_sharp_faces(*mesh, selection_field, fn::invert_boolean_field(smooth_field));
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_SHADE_SMOOTH, "Set Shade Smooth", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_shade_smooth_cc
