/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* ******************* channel Distance Matte ********************************* */

namespace blender::nodes::node_composite_distance_matte_cc {

NODE_STORAGE_FUNCS(NodeChroma)

static void cmp_node_distance_matte_declare(NodeDeclarationBuilder &b)
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

static void node_composit_init_distance_matte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeChroma *c = MEM_cnew<NodeChroma>(__func__);
  node->storage = c;
  c->channel = CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA;
  c->t1 = 0.1f;
  c->t2 = 0.1f;
}

static void node_composit_buts_distance_matte(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, true);

  uiItemL(layout, IFACE_("Color Space:"), ICON_NONE);
  row = uiLayoutRow(layout, false);
  uiItemR(
      row, ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  uiItemR(col,
          ptr,
          "tolerance",
          UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
          std::nullopt,
          ICON_NONE);
  uiItemR(
      col, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static CMPNodeDistanceMatteColorSpace get_color_space(const bNode &node)
{
  return static_cast<CMPNodeDistanceMatteColorSpace>(node_storage(node).channel);
}

static float get_tolerance(const bNode &node)
{
  return node_storage(node).t1;
}

static float get_falloff(const bNode &node)
{
  return node_storage(node).t2;
}

class DistanceMatteShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float tolerance = get_tolerance(bnode());
    const float falloff = get_falloff(bnode());

    if (get_color_space(bnode()) == CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA) {
      GPU_stack_link(material,
                     &bnode(),
                     "node_composite_distance_matte_rgba",
                     inputs,
                     outputs,
                     GPU_uniform(&tolerance),
                     GPU_uniform(&falloff));
      return;
    }

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_distance_matte_ycca",
                   inputs,
                   outputs,
                   GPU_uniform(&tolerance),
                   GPU_uniform(&falloff));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new DistanceMatteShaderNode(node);
}

static void distance_key_rgba(const float4 &color,
                              const float4 &key,
                              const float tolerance,
                              const float falloff,
                              float4 &result,
                              float &matte)
{
  float difference = math::distance(color.xyz(), key.xyz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w : math::max(0.0f, difference - tolerance) / falloff;
  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void distance_key_ycca(const float4 &color,
                              const float4 &key,
                              const float tolerance,
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

  float difference = math::distance(color_ycca.yz(), key_ycca.yz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w : math::max(0.0f, difference - tolerance) / falloff;
  matte = math::min(alpha, color.w);
  result = color * matte;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const float tolerance = get_tolerance(builder.node());
  const float falloff = get_falloff(builder.node());
  const CMPNodeDistanceMatteColorSpace color_space = get_color_space(builder.node());

  switch (color_space) {
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI2_SO2<float4, float4, float4, float>(
            "Distance Key YCCA",
            [=](const float4 &color, const float4 &key_color, float4 &output_color, float &matte)
                -> void {
              distance_key_ycca(color, key_color, tolerance, falloff, output_color, matte);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
      break;
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA:
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI2_SO2<float4, float4, float4, float>(
            "Distance Key RGBA",
            [=](const float4 &color, const float4 &key_color, float4 &output_color, float &matte)
                -> void {
              distance_key_rgba(color, key_color, tolerance, falloff, output_color, matte);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
      break;
  }
}

}  // namespace blender::nodes::node_composite_distance_matte_cc

void register_node_type_cmp_distance_matte()
{
  namespace file_ns = blender::nodes::node_composite_distance_matte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDistanceMatte", CMP_NODE_DIST_MATTE);
  ntype.ui_name = "Distance Key";
  ntype.ui_description = "Create matte based on 3D distance between colors";
  ntype.enum_name_legacy = "DISTANCE_MATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_distance_matte_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_distance_matte;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_distance_matte;
  blender::bke::node_type_storage(
      &ntype, "NodeChroma", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
