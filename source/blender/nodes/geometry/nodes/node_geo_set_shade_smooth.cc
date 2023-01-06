/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_shade_smooth_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().field_on_all();
  b.add_input<decl::Bool>(N_("Shade Smooth")).field_on_all().default_value(true);
  b.add_output<decl::Geometry>(N_("Geometry")).propagate_all();
}

static void set_smooth(Mesh &mesh,
                       const Field<bool> &selection_field,
                       const Field<bool> &shade_field)
{
  if (mesh.totpoly == 0) {
    return;
  }

  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  AttributeWriter<bool> smooth = attributes.lookup_or_add_for_write<bool>("shade_smooth",
                                                                          ATTR_DOMAIN_FACE);

  bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  fn::FieldEvaluator evaluator{field_context, mesh.totpoly};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(shade_field, smooth.varray);
  evaluator.evaluate();

  smooth.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<bool> shade_field = params.extract_input<Field<bool>>("Shade Smooth");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      set_smooth(*mesh, selection_field, shade_field);
    }
  });
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_shade_smooth_cc

void register_node_type_geo_set_shade_smooth()
{
  namespace file_ns = blender::nodes::node_geo_set_shade_smooth_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_SHADE_SMOOTH, "Set Shade Smooth", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
