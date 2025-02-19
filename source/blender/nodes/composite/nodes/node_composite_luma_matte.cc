/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "IMB_colormanagement.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "node_composite_util.hh"

/* ******************* Luma Matte Node ********************************* */

namespace blender::nodes::node_composite_luma_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_luma_matte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
}

static void node_composit_init_luma_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->t1 = 1.0f;
  c->t2 = 0.0f;
}

static void node_composit_buts_luma_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col,
          ptr,
          "limit_max",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          std::nullopt,
          ICON_NONE);
  uiItemR(col,
          ptr,
          "limit_min",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          std::nullopt,
          ICON_NONE);
}

using namespace blender::compositor;

static float get_high(const bNode &node)
{
  return node_storage(node).t1;
}

static float get_low(const bNode &node)
{
  return node_storage(node).t2;
}

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  const float high = get_high(*node);
  const float low = get_low(*node);
  float luminance_coefficients[3];
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  return GPU_stack_link(material,
                        node,
                        "node_composite_luminance_matte",
                        inputs,
                        outputs,
                        GPU_uniform(&high),
                        GPU_uniform(&low),
                        GPU_constant(luminance_coefficients));
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const float high = get_high(builder.node());
  const float low = get_low(builder.node());
  float3 luminance_coefficients;
  IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

  builder.construct_and_set_matching_fn_cb([=]() {
    return mf::build::SI1_SO2<float4, float4, float>(
        "Luminance Key",
        [=](const float4 &color, float4 &result, float &matte) -> void {
          float luminance = math::dot(color.xyz(), luminance_coefficients);
          float alpha = math::clamp((luminance - low) / (high - low), 0.0f, 1.0f);
          matte = math::min(alpha, color.w);
          result = color * matte;
        },
        mf::build::exec_presets::AllSpanOrSingle());
  });
}

}  // namespace blender::nodes::node_composite_luma_matte_cc

void register_node_type_cmp_luma_matte()
{
  namespace file_ns = blender::nodes::node_composite_luma_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeLumaMatte", CMP_NODE_LUMA_MATTE);
  ntype.ui_name = "Luminance Key";
  ntype.ui_description = "Create a matte based on luminance (brightness) difference";
  ntype.enum_name_legacy = "LUMA_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_luma_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_luma_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_luma_matte;
  blender::bke::node_type_storage(
      ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
