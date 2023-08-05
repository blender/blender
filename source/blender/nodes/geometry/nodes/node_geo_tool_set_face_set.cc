/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_set_face_set_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Int>("Face Set").hide_value().field_on_all();
  b.add_output<decl::Geometry>("Mesh");
}

static bool is_constant_zero(const Field<int> &face_set)
{
  if (face_set.node().depends_on_input()) {
    return false;
  }
  return fn::evaluate_constant_field<int>(face_set) == 0;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<int> face_set = params.extract_input<Field<int>>("Face Set");
  const bool is_zero = is_constant_zero(face_set);

  GeometrySet geometry = params.extract_input<GeometrySet>("Mesh");
  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      if (is_zero) {
        mesh->attributes_for_write().remove(".sculpt_face_set");
      }
      else {
        bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                           ".sculpt_face_set",
                                           ATTR_DOMAIN_FACE,
                                           selection,
                                           face_set);
      }
    }
  });
  params.set_output("Mesh", std::move(geometry));
}

}  // namespace blender::nodes::node_geo_tool_set_face_set_cc

void register_node_type_geo_tool_set_face_set()
{
  namespace file_ns = blender::nodes::node_geo_tool_set_face_set_cc;
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_SET_FACE_SET, "Set Face Set", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.gather_add_node_search_ops = blender::nodes::search_link_ops_for_for_tool_node;
  ntype.gather_link_search_ops = blender::nodes::search_link_ops_for_tool_node;
  nodeRegisterType(&ntype);
}
