/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "NOD_socket_search_link.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_transform_point_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").subtype(PROP_XYZ);
  b.add_input<decl::Matrix>("Transform");
  b.add_output<decl::Vector>("Vector").subtype(PROP_XYZ);
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_matrix_socket) {
    nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI2_SO<float3, float4x4, float3>(
      "Transform Point",
      [](float3 point, float4x4 matrix) { return math::transform_point(matrix, point); });
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_TRANSFORM_POINT, "Transform Point", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = search_link_ops;
  ntype.build_multi_function = node_build_multi_function;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_transform_point_cc
