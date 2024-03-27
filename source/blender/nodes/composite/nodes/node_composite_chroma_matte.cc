/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_rotation.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* Chroma Key ********************************************************** */

namespace blender::nodes::node_composite_chroma_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_chroma_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Key Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_chroma_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = DEG2RADF(30.0f);
  c->t2 = DEG2RADF(10.0f);
  c->t3 = 0.0f;
  c->fsize = 0.0f;
  c->fstrength = 1.0f;
}

static void node_composit_buts_chroma_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "tolerance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  /* Removed for now. */
  // uiItemR(col, ptr, "lift", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  uiItemR(col, ptr, "gain", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  /* Removed for now. */
  // uiItemR(col, ptr, "shadow_adjust", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class ChromaMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float acceptance = get_acceptance();
    const float cutoff = get_cutoff();
    const float falloff = get_falloff();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_chroma_matte",
                   inputs,
                   outputs,
                   GPU_uniform(&acceptance),
                   GPU_uniform(&cutoff),
                   GPU_uniform(&falloff));
  }

  float get_acceptance()
  {
    return std::tan(node_storage(bnode()).t1 / 2.0f);
  }

  float get_cutoff()
  {
    return node_storage(bnode()).t2;
  }

  float get_falloff()
  {
    return node_storage(bnode()).fstrength;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ChromaMatteShaderNode(node);
}

}  // namespace blender::nodes::node_composite_chroma_matte_cc

void register_node_type_cmp_chroma_matte()
{
  namespace file_ns = blender::nodes::node_composite_chroma_matte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CHROMA_MATTE, "Chroma Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_chroma_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_chroma_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_chroma_matte;
  node_type_storage(&ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;

  nodeRegisterType(&ntype);
}
