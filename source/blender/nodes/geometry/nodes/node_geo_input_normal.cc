/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Normal").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> normal_field{std::make_shared<bke::NormalFieldInput>()};
  params.set_output("Normal", std::move(normal_field));
}

}  // namespace blender::nodes::node_geo_input_normal_cc

void register_node_type_geo_input_normal()
{
  namespace file_ns = blender::nodes::node_geo_input_normal_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_NORMAL, "Normal", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
