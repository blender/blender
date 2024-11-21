/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

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

static float get_acceptance(const bNode &node)
{
  return std::tan(node_storage(node).t1 / 2.0f);
}

static float get_cutoff(const bNode &node)
{
  return node_storage(node).t2;
}

static float get_falloff(const bNode &node)
{
  return node_storage(node).fstrength;
}

class ChromaMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float acceptance = get_acceptance(bnode());
    const float cutoff = get_cutoff(bnode());
    const float falloff = get_falloff(bnode());

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_chroma_matte",
                   inputs,
                   outputs,
                   GPU_uniform(&acceptance),
                   GPU_uniform(&cutoff),
                   GPU_uniform(&falloff));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new ChromaMatteShaderNode(node);
}

/* Algorithm from the book Video Demystified. Chapter 7. Chroma Keying. */
static void chroma_matte(const float4 &color,
                         const float4 &key,
                         const float acceptance,
                         const float cutoff,
                         const float falloff,
                         float4 &result,
                         float &matte)
{
  float3 color_ycca;
  rgb_to_ycc(
      color.x, color.y, color.z, &color_ycca.x, &color_ycca.y, &color_ycca.z, BLI_YCC_ITU_BT709);
  color_ycca /= 255.0f;
  float3 key_ycca;
  rgb_to_ycc(key.x, key.y, key.z, &key_ycca.x, &key_ycca.y, &key_ycca.z, BLI_YCC_ITU_BT709);
  key_ycca /= 255.0f;

  /* Normalize the CrCb components into the [-1, 1] range. */
  float2 color_cc = color_ycca.yz() * 2.0f - 1.0f;
  float2 key_cc = math::normalize(key_ycca.yz() * 2.0f - 1.0f);

  /* Rotate the color onto the space of the key such that x axis of the color space passes
   * through the key color. */
  color_cc = math::from_direction(key_cc * float2(1.0f, -1.0f)) * color_cc;

  /* Compute foreground key. If positive, the value is in the [0, 1] range. */
  float foreground_key = color_cc.x - (math::abs(color_cc.y) / acceptance);

  /* Negative foreground key values retain the original alpha. Positive values are scaled by the
   * falloff, while colors that make an angle less than the cutoff angle get a zero alpha. */
  float alpha = color.w;
  if (foreground_key > 0.0f) {
    alpha = 1.0f - (foreground_key / falloff);

    if (math::abs(math::atan2(color_cc.y, color_cc.x)) < (cutoff / 2.0f)) {
      alpha = 0.0f;
    }
  }

  /* Compute output. */
  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const float acceptance = get_acceptance(builder.node());
  const float cutoff = get_cutoff(builder.node());
  const float falloff = get_falloff(builder.node());

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI2_SO2<float4, float4, float4, float>(
        "Chroma Key",
        [=](const float4 &color, const float4 &key_color, float4 &output_color, float &matte)
            -> void {
          chroma_matte(color, key_color, acceptance, cutoff, falloff, output_color, matte);
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_chroma_matte_cc

void register_node_type_cmp_chroma_matte()
{
  namespace file_ns = blender::nodes::node_composite_chroma_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CHROMA_MATTE, "Chroma Key", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_chroma_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_chroma_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_chroma_matte;
  blender::bke::node_type_storage(
      &ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
