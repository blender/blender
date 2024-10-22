/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "BKE_texture.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_material.hh"

#include "COM_shader_node.hh"

#include "node_composite_util.hh"

/* **************** MAP VALUE ******************** */

namespace blender::nodes::node_composite_map_value_cc {

NODE_STORAGE_FUNCS(TexMapping)

static void cmp_node_map_value_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Value");
}

static void node_composit_init_map_value(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_texture_mapping_add(TEXMAP_TYPE_POINT);
}

static void node_composit_buts_map_value(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *sub, *col;

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(col, ptr, "size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_min", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_min"));
  uiItemR(sub, ptr, "min", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "use_max", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_max"));
  uiItemR(sub, ptr, "max", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class MapValueShaderNode : public ShaderNode {
 public:
  using ShaderNode::ShaderNode;

  void compile(GPUMaterial *material) override
  {
    GPUNodeStack *inputs = get_inputs_array();
    GPUNodeStack *outputs = get_outputs_array();

    const TexMapping &texture_mapping = node_storage(bnode());

    const float use_min = get_use_min();
    const float use_max = get_use_max();

    GPU_stack_link(material,
                   &bnode(),
                   "node_composite_map_value",
                   inputs,
                   outputs,
                   GPU_uniform(texture_mapping.loc),
                   GPU_uniform(texture_mapping.size),
                   GPU_constant(&use_min),
                   GPU_uniform(texture_mapping.min),
                   GPU_constant(&use_max),
                   GPU_uniform(texture_mapping.max));
  }

  bool get_use_min()
  {
    return node_storage(bnode()).flag & TEXMAP_CLIP_MIN;
  }

  bool get_use_max()
  {
    return node_storage(bnode()).flag & TEXMAP_CLIP_MAX;
  }
};

static ShaderNode *get_compositor_shader_node(DNode node)
{
  return new MapValueShaderNode(node);
}

template<bool UseMin, bool UseMax>
static float map_value(
    const float value, const float offset, const float size, const float min, const float max)
{
  float result = (value + offset) * size;

  if constexpr (UseMin) {
    if (result < min) {
      result = min;
    }
  }

  if constexpr (UseMax) {
    if (result > max) {
      result = max;
    }
  }

  return result;
}

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const TexMapping &texture_mapping = node_storage(builder.node());
  const float offset = texture_mapping.loc[0];
  const float size = texture_mapping.size[0];
  const float min = texture_mapping.min[0];
  const float max = texture_mapping.max[0];
  const bool use_min = texture_mapping.flag & TEXMAP_CLIP_MIN;
  const bool use_max = texture_mapping.flag & TEXMAP_CLIP_MAX;

  if (use_min) {
    if (use_max) {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI1_SO<float, float>(
            "Map Value With Min With Max",
            [=](const float value) -> float {
              return map_value<true, true>(value, offset, size, min, max);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
    }
    else {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI1_SO<float, float>(
            "Map Value With Min No Max",
            [=](const float value) -> float {
              return map_value<true, false>(value, offset, size, min, max);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
    }
  }
  else {
    if (use_max) {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI1_SO<float, float>(
            "Map Value No Min With Max",
            [=](const float value) -> float {
              return map_value<false, true>(value, offset, size, min, max);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
    }
    else {
      builder.construct_and_set_matching_fn_cb([=]() {
        return mf::build::SI1_SO<float, float>(
            "Map Value No Min No Max",
            [=](const float value) -> float {
              return map_value<false, false>(value, offset, size, min, max);
            },
            mf::build::exec_presets::AllSpanOrSingle());
      });
    }
  }
}

}  // namespace blender::nodes::node_composite_map_value_cc

void register_node_type_cmp_map_value()
{
  namespace file_ns = blender::nodes::node_composite_map_value_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_VALUE, "Map Value", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_map_value_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_value;
  ntype.initfunc = file_ns::node_composit_init_map_value;
  blender::bke::node_type_storage(
      &ntype, "TexMapping", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_shader_node = file_ns::get_compositor_shader_node;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(&ntype);
}
