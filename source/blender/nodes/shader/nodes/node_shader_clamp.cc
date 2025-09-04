/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "BLI_math_base.h"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_clamp_cc {

static void sh_node_clamp_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Value").default_value(1.0f);
  b.add_input<decl::Float>("Min").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Max").default_value(1.0f).min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>("Result");
}

static void node_shader_buts_clamp(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "clamp_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_clamp(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = NODE_CLAMP_MINMAX; /* clamp type */
}

static int gpu_shader_clamp(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  return (node->custom1 == NODE_CLAMP_MINMAX) ?
             GPU_stack_link(mat, node, "clamp_minmax", in, out) :
             GPU_stack_link(mat, node, "clamp_range", in, out);
}

static void sh_node_clamp_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto minmax_fn = mf::build::SI3_SO<float, float, float, float>(
      "Clamp (Min Max)",
      [](float value, float min, float max) { return std::min(std::max(value, min), max); });
  static auto range_fn = mf::build::SI3_SO<float, float, float, float>(
      "Clamp (Range)", [](float value, float a, float b) {
        if (a < b) {
          return clamp_f(value, a, b);
        }

        return clamp_f(value, b, a);
      });

  int clamp_type = builder.node().custom1;
  if (clamp_type == NODE_CLAMP_MINMAX) {
    builder.set_matching_fn(minmax_fn);
  }
  else {
    builder.set_matching_fn(range_fn);
  }
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  auto type = node_->custom1;
  NodeItem value = get_input_value("Value", NodeItem::Type::Float);
  NodeItem min = get_input_value("Min", NodeItem::Type::Float);
  NodeItem max = get_input_value("Max", NodeItem::Type::Float);

  NodeItem res = empty();
  if (type == NODE_CLAMP_RANGE) {
    res = min.if_else(
        NodeItem::CompareOp::Less, max, value.clamp(min, max), value.clamp(max, min));
  }
  else {
    res = value.clamp(min, max);
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_clamp_cc

void register_node_type_sh_clamp()
{
  namespace file_ns = blender::nodes::node_shader_clamp_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeClamp", SH_NODE_CLAMP);
  ntype.ui_name = "Clamp";
  ntype.ui_description = "Clamp a value between a minimum and a maximum";
  ntype.enum_name_legacy = "CLAMP";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_clamp_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_clamp;
  ntype.initfunc = file_ns::node_shader_init_clamp;
  ntype.gpu_fn = file_ns::gpu_shader_clamp;
  ntype.build_multi_function = file_ns::sh_node_clamp_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
