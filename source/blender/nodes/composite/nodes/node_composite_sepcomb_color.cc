/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"

#include "GPU_material.h"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

static void node_cmp_combsep_color_init(bNodeTree * /*ntree*/, bNode *node)
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

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_separate_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Red");
  b.add_output<decl::Float>("Green");
  b.add_output<decl::Float>("Blue");
  b.add_output<decl::Float>("Alpha");
}

static void cmp_node_separate_color_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->outputs, (CMPNodeCombSepColorMode)storage->mode);
}

using namespace blender::realtime_compositor;

class SeparateColorShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  const char *get_shader_function_name()
  {
    switch (node_storage(bnode()).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
        return "node_composite_separate_rgba";
      case CMP_NODE_COMBSEP_COLOR_HSV:
        return "node_composite_separate_hsva";
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return "node_composite_separate_hsla";
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return "node_composite_separate_yuva_itu_709";
      case CMP_NODE_COMBSEP_COLOR_YCC:
        switch (node_storage(bnode()).ycc_mode) {
          case BLI_YCC_ITU_BT601:
            return "node_composite_separate_ycca_itu_601";
          case BLI_YCC_ITU_BT709:
            return "node_composite_separate_ycca_itu_709";
          case BLI_YCC_JFIF_0_255:
            return "node_composite_separate_ycca_jpeg";
        }
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new SeparateColorShaderNode(node);
}

}  // namespace blender::nodes::node_composite_separate_color_cc

void register_node_type_cmp_separate_color()
{
  namespace file_ns = blender::nodes::node_composite_separate_color_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_separate_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.updatefunc = file_ns::cmp_node_separate_color_update;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}

/* **************** COMBINE COLOR ******************** */

namespace blender::nodes::node_composite_combine_color_cc {

NODE_STORAGE_FUNCS(NodeCMPCombSepColor)

static void cmp_node_combine_color_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Red")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Green")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Blue")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("Alpha")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
}

static void cmp_node_combine_color_update(bNodeTree * /*ntree*/, bNode *node)
{
  const NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)node->storage;
  node_cmp_combsep_color_label(&node->inputs, (CMPNodeCombSepColorMode)storage->mode);
}

using namespace blender::realtime_compositor;

class CombineColorShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    GPU_stack_link(material, &bnode(), get_shader_function_name(), inputs, outputs);
  }

  const char *get_shader_function_name()
  {
    switch (node_storage(bnode()).mode) {
      case CMP_NODE_COMBSEP_COLOR_RGB:
        return "node_composite_combine_rgba";
      case CMP_NODE_COMBSEP_COLOR_HSV:
        return "node_composite_combine_hsva";
      case CMP_NODE_COMBSEP_COLOR_HSL:
        return "node_composite_combine_hsla";
      case CMP_NODE_COMBSEP_COLOR_YUV:
        return "node_composite_combine_yuva_itu_709";
      case CMP_NODE_COMBSEP_COLOR_YCC:
        switch (node_storage(bnode()).ycc_mode) {
          case BLI_YCC_ITU_BT601:
            return "node_composite_combine_ycca_itu_601";
          case BLI_YCC_ITU_BT709:
            return "node_composite_combine_ycca_itu_709";
          case BLI_YCC_JFIF_0_255:
            return "node_composite_combine_ycca_jpeg";
        }
    }

    BLI_assert_unreachable();
    return nullptr;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new CombineColorShaderNode(node);
}

}  // namespace blender::nodes::node_composite_combine_color_cc

void register_node_type_cmp_combine_color()
{
  namespace file_ns = blender::nodes::node_composite_combine_color_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combine_color_declare;
  ntype.initfunc = node_cmp_combsep_color_init;
  node_type_storage(
      &ntype, "NodeCMPCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.updatefunc = file_ns::cmp_node_combine_color_update;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
