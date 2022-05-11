/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_composite_util.hh"

static void node_cmp_combsep_color_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeCMPCombSepColor *data = MEM_cnew<NodeCMPCombSepColor>(__func__);
  data->mode = CMP_NODE_COMBSEP_COLOR_RGB;
  data->ycc_mode = BLI_YCC_ITU_BT709;
  node->storage = data;
}

static void node_cmp_combsep_color_label(const ListBase *sockets, CMPNodeCombSepColorMode mode)
{
  bNodeSocket *sock1 = (bNodeSocket *)sockets->first;
  bNodeSocket *sock2 = sock1->next;
  bNodeSocket *sock3 = sock2->next;

  node_sock_label_clear(sock1);
  node_sock_label_clear(sock2);
  node_sock_label_clear(sock3);

  switch (mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB:
      node_sock_label(sock1, "Red");
      node_sock_label(sock2, "Green");
      node_sock_label(sock3, "Blue");
      break;
    case CMP_NODE_COMBSEP_COLOR_HSV:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Value");
      break;
    case CMP_NODE_COMBSEP_COLOR_HSL:
      node_sock_label(sock1, "Hue");
      node_sock_label(sock2, "Saturation");
      node_sock_label(sock3, "Lightness");
      break;
    case CMP_NODE_COMBSEP_COLOR_YCC:
      node_sock_label(sock1, "Y");
      node_sock_label(sock2, "Cb");
      node_sock_label(sock3, "Cr");
      break;
    case CMP_NODE_COMBSEP_COLOR_YUV:
      node_sock_label(sock1, "Y");
      node_sock_label(sock2, "U");
      node_sock_label(sock3, "V");
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/* **************** SEPARATE COLOR ******************** */

namespace blender::nodes::node_composite_separate_color_cc {

static void cmp_node_separate_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>(N_("Red"));
  b.add_output<decl::Float>(N_("Green"));
  b.add_output<decl::Float>(N_("Blue"));
  b.add_output<decl::Float>(N_("Alpha"));
}

static void cmp_node_separate_color_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->outputs, (CMPNodeCombSepColorMode)storage->mode);
}

}  // namespace blender::nodes::node_composite_separate_color_cc

void register_node_type_cmp_separate_color()
{
  namespace file_ns = blender::nodes::node_composite_separate_color_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_separate_color_declare;
  node_type_init(&ntype, node_cmp_combsep_color_init);
  node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  node_type_update(&ntype, file_ns::cmp_node_separate_color_update);

  nodeRegisterType(&ntype);
}

/* **************** COMBINE COLOR ******************** */

namespace blender::nodes::node_composite_combine_color_cc {

static void cmp_node_combine_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Red")).default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Green"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Blue"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Alpha"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Color>(N_("Image"));
}

static void cmp_node_combine_color_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->inputs, (CMPNodeCombSepColorMode)storage->mode);
}

}  // namespace blender::nodes::node_composite_combine_color_cc

void register_node_type_cmp_combine_color()
{
  namespace file_ns = blender::nodes::node_composite_combine_color_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combine_color_declare;
  node_type_init(&ntype, node_cmp_combsep_color_init);
  node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  node_type_update(&ntype, file_ns::cmp_node_combine_color_update);

  nodeRegisterType(&ntype);
}
