/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup cmpnodes
 */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes {

static void cmp_node_scene_time_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Seconds");
  b.add_output<decl::Float>("Frame");
}

using namespace blender::realtime_compositor;

class SceneTimeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    execute_seconds();
    execute_frame();
  }

  void execute_seconds()
  {
    Result &result = get_result("Seconds");
    result.allocate_single_value();
    result.set_float_value(context().get_time());
  }

  void execute_frame()
  {
    Result &result = get_result("Frame");
    result.allocate_single_value();
    result.set_float_value(float(context().get_frame_number()));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SceneTimeOperation(context, node);
}

}  // namespace blender::nodes

void register_node_type_cmp_scene_time()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SCENE_TIME, "Scene Time", NODE_CLASS_INPUT);
  ntype.declare = blender::nodes::cmp_node_scene_time_declare;
  ntype.get_compositor_operation = blender::nodes::get_compositor_operation;

  nodeRegisterType(&ntype);
}
