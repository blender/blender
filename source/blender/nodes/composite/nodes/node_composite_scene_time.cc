/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_scene_time_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Seconds");
  b.add_output<decl::Float>("Frame");
}

using namespace blender::compositor;

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
    if (!result.should_compute()) {
      return;
    }

    result.allocate_single_value();
    result.set_single_value(context().get_time());
  }

  void execute_frame()
  {
    Result &result = get_result("Frame");
    if (!result.should_compute()) {
      return;
    }

    result.allocate_single_value();
    result.set_single_value(float(context().get_frame_number()));
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new SceneTimeOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSceneTime", CMP_NODE_SCENE_TIME);
  ntype.ui_name = "Scene Time";
  ntype.ui_description = "Input the current scene time in seconds or frames";
  ntype.enum_name_legacy = "SCENE_TIME";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_scene_time_cc
