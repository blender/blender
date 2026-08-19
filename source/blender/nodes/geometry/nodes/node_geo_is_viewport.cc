/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_is_viewport_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Is Viewport"_ustr);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Depsgraph *depsgraph = params.depsgraph();
  const eEvaluationMode mode = DEG_get_mode(depsgraph);
  const bool is_viewport = mode == DAG_EVAL_VIEWPORT;

  params.set_output("Is Viewport"_ustr, is_viewport);
}

using namespace blender::compositor;

class IsViewportOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const bool is_viewport = this->context().is_viewport();

    Result &result = this->get_result("Is Viewport");
    result.allocate_single_value();
    result.set_single_value(is_viewport);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new IsViewportOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeIsViewport"_ustr, GEO_NODE_IS_VIEWPORT);
  ntype.ui_name = "Is Viewport";
  ntype.ui_description =
      "Retrieve whether the nodes are being evaluated for the viewport rather than the final "
      "render";
  ntype.enum_name_legacy = "IS_VIEWPORT";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_is_viewport_cc
