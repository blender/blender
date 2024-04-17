/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_view3d_types.h"

#include "BLI_math_matrix.hh"

#include "BKE_scene.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_3d_cursor_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Location")
      .subtype(PROP_TRANSLATION)
      .description(
          "The location of the scene's 3D cursor, in the local space of the modified object");
  b.add_output<decl::Rotation>("Rotation")
      .description(
          "The rotation of the scene's 3D cursor, in the local space of the modified object");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const View3DCursor &cursor = params.user_data()->call_data->operator_data->scene->cursor;
  const float4x4 &world_to_object = params.self_object()->world_to_object();

  const float3 location_global(cursor.location);
  params.set_output("Location", math::transform_point(world_to_object, location_global));

  math::Quaternion rotation_global;
  BKE_scene_cursor_rot_to_quat(&cursor, &rotation_global.w);
  params.set_output("Rotation", math::to_quaternion(world_to_object) * rotation_global);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_3D_CURSOR, "3D Cursor", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_3d_cursor_cc
