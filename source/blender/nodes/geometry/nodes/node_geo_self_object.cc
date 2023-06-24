/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_self_object_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Object>("Self Object");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Self Object", const_cast<Object *>(params.self_object()));
}

}  // namespace blender::nodes::node_geo_self_object_cc

void register_node_type_geo_self_object()
{
  namespace file_ns = blender::nodes::node_geo_self_object_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SELF_OBJECT, "Self Object", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
