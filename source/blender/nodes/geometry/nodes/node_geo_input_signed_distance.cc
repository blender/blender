/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_signed_distance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Signed Distance").field_source();
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_node_add_ops_for_basic_node(params);
  }
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> signed_distance_field{AttributeFieldInput::Create<float>("distance")};
  params.set_output("Signed Distance", std::move(signed_distance_field));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_SIGNED_DISTANCE, "Signed Distance", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.gather_add_node_search_ops = search_node_add_ops;
  ntype.gather_link_search_ops = search_link_ops;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_signed_distance_cc
