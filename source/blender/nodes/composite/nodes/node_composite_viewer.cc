/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_viewer_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .structure_type(StructureType::Dynamic)
      .compositor_realization_mode(CompositorInputRealizationMode::None);
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  ImageUser *iuser = MEM_new_for_free<ImageUser>(__func__);
  node->storage = iuser;
  iuser->sfra = 1;
  node->custom1 = NODE_VIEWER_SHORTCUT_NONE;
}

using namespace blender::compositor;

class ViewerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    this->context().write_viewer(this->get_input("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new ViewerOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeViewer", CMP_NODE_VIEWER);
  ntype.ui_name = "Viewer";
  ntype.ui_description =
      "Visualize data from inside a node graph, in the image editor or as a backdrop";
  ntype.enum_name_legacy = "VIEWER";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(
      ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = get_compositor_operation;

  ntype.no_muting = true;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_viewer_cc
