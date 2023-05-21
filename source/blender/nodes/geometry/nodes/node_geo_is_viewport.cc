/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_is_viewport_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Is Viewport");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Depsgraph *depsgraph = params.depsgraph();
  const eEvaluationMode mode = DEG_get_mode(depsgraph);
  const bool is_viewport = mode == DAG_EVAL_VIEWPORT;

  params.set_output("Is Viewport", is_viewport);
}

}  // namespace blender::nodes::node_geo_is_viewport_cc

void register_node_type_geo_is_viewport()
{
  namespace file_ns = blender::nodes::node_geo_is_viewport_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_IS_VIEWPORT, "Is Viewport", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
