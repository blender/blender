/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender {

namespace nodes::node_composite_viewer_cc {

static void cmp_node_viewer_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
}

static void node_composit_init_viewer(bNodeTree * /*ntree*/, bNode *node)
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
    const Result &image = this->get_input("Image");
    this->context().write_viewer(image);
  }

  Domain compute_domain() override
  {
    /* Viewers nodes are treated as group outputs that should be the compositing domain. */
    if (this->context().treat_viewer_as_group_output() &&
        this->context().use_compositing_domain_for_input_output())
    {
      return this->context().get_compositing_domain();
    }

    return NodeOperation::compute_domain();
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new ViewerOperation(context, node);
}

}  // namespace nodes::node_composite_viewer_cc

static void register_node_type_cmp_viewer()
{
  namespace file_ns = nodes::node_composite_viewer_cc;

  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeViewer", CMP_NODE_VIEWER);
  ntype.ui_name = "Viewer";
  ntype.ui_description =
      "Visualize data from inside a node graph, in the image editor or as a backdrop";
  ntype.enum_name_legacy = "VIEWER";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = file_ns::cmp_node_viewer_declare;
  ntype.initfunc = file_ns::node_composit_init_viewer;
  bke::node_type_storage(
      ntype, "ImageUser", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  ntype.no_muting = true;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_viewer)

}  // namespace blender
