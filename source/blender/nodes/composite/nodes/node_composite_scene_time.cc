/* SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

namespace blender::nodes {

static void cmp_node_scene_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Seconds"));
  b.add_output<decl::Float>(N_("Frame"));
}

}  // namespace blender::nodes

void register_node_type_cmp_scene_time()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCENE_TIME, "Scene Time", NODE_CLASS_INPUT);
  ntype.declare = blender::nodes::cmp_node_scene_time_declare;
  nodeRegisterType(&ntype);
}
