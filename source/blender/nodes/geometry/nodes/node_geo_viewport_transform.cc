/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_viewport_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Matrix>("Projection")
      .description("The 3D viewport's perspective or orthographic projection matrix");
  b.add_output<decl::Matrix>("View").description(
      "The view direction and location of the 3D viewport");
  b.add_output<decl::Bool>("Is Orthographic")
      .description("Whether the viewport is using orthographic projection");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const Object &self_object = *params.self_object();
  const GeoNodesOperatorData &data = *params.user_data()->call_data->operator_data;

  params.set_output("Projection", data.viewport_winmat);
  params.set_output("View", data.viewport_viewmat * self_object.object_to_world());
  params.set_output("Is Orthographic", !data.viewport_is_perspective);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_TOOL_VIEWPORT_TRANSFORM, "Viewport Transform", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_viewport_transform_cc
