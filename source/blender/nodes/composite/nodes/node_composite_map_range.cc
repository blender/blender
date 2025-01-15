/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** Map Range ******************** */

namespace blender::nodes::node_composite_map_range_cc {

static void cmp_node_map_range_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("From Min")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("From Max")
      .default_value(1.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("To Min")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(3);
  b.add_input<decl::Float>("To Max")
      .default_value(1.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_domain_priority(4);
  b.add_output<decl::Float>("Value");
}

static void node_composit_buts_map_range(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_clamp", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

static bool get_should_clamp(const bNode &node)
{
  return node.custom1;
}

class MapRangeShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const float should_clamp = get_should_clamp(bnode());

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_map_range",
                   inputs,
                   outputs,
                   GPU_constant(&should_clamp));
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new MapRangeShaderNode(node);
}

/* An arbitrary value determined by Blender. */
#define BLENDER_ZMAX 10000.0f

template<bool ShouldClamp>
static float map_range(const float value,
                       const float from_min,
                       const float from_max,
                       const float to_min,
                       const float to_max)
{
  if (math::abs(from_max - from_min) < 1e-6f) {
    return 0.0f;
  }

  float result = 0.0f;
  if (value >= -BLENDER_ZMAX && value <= BLENDER_ZMAX) {
    result = (value - from_min) / (from_max - from_min);
    result = to_min + result * (to_max - to_min);
  }
  else if (value > BLENDER_ZMAX) {
    result = to_max;
  }
  else {
    result = to_min;
  }

  if constexpr (ShouldClamp) {
    if (to_max > to_min) {
      result = math::clamp(result, to_min, to_max);
    }
    else {
      result = math::clamp(result, to_max, to_min);
    }
  }

  return result;
}

#undef BLENDER_ZMAX

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto no_clamp_function = mf::build::SI5_SO<float, float, float, float, float, float>(
      "Map Range No CLamp",
      [](const float value,
         const float from_min,
         const float from_max,
         const float to_min,
         const float to_max) -> float {
        return map_range<false>(value, from_min, from_max, to_min, to_max);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
  static auto clamp_function = mf::build::SI5_SO<float, float, float, float, float, float>(
      "Map Range Clamp",
      [](const float value,
         const float from_min,
         const float from_max,
         const float to_min,
         const float to_max) -> float {
        return map_range<true>(value, from_min, from_max, to_min, to_max);
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());

  const bool should_clamp = get_should_clamp(builder.node());
  if (should_clamp) {
    builder.set_matching_fn(clamp_function);
  }
  else {
    builder.set_matching_fn(no_clamp_function);
  }
}

}  // namespace blender::nodes::node_composite_map_range_cc

void register_node_type_cmp_map_range()
{
  namespace file_ns = blender::nodes::node_composite_map_range_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMapRange", CMP_NODE_MAP_RANGE);
  ntype.ui_name = "Map Range";
  ntype.ui_description = "Map an input value range into a destination range";
  ntype.enum_name_legacy = "MAP_RANGE";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::cmp_node_map_range_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_range;
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
