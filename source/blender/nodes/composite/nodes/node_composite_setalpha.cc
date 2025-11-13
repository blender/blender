/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"

#include "GPU_material.hh"

#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_setalpha_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_SETALPHA_MODE_APPLY,
     "APPLY",
     0,
     N_("Apply Mask"),
     N_("Multiply the input image's RGBA channels by the alpha input value")},
    {CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA,
     "REPLACE_ALPHA",
     0,
     N_("Replace Alpha"),
     N_("Replace the input image's alpha channel by the alpha input value")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_setalpha_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.is_function_node();
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f}).hide_value();
  b.add_output<decl::Color>("Image").align_with_previous();

  b.add_input<decl::Float>("Alpha").default_value(1.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_SETALPHA_MODE_APPLY)
      .static_items(type_items)
      .optional_label();
}

static void node_composit_init_setalpha(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, but allocated for forward compatibility. */
  NodeSetAlpha *settings = MEM_callocN<NodeSetAlpha>(__func__);
  node->storage = settings;
}

using namespace blender::compositor;

static int node_gpu_material(GPUMaterial *material,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *inputs,
                             GPUNodeStack *outputs)
{
  return GPU_stack_link(material, node, "node_composite_set_alpha", inputs, outputs);
}

static float4 set_alpha(const float4 &color, const float alpha, const MenuValue &type)
{
  switch (CMPNodeSetAlphaMode(type.value)) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      return color * alpha;
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      return float4(color.xyz(), alpha);
  }
  return color;
}

using blender::compositor::Color;

static void node_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static auto function = mf::build::SI3_SO<Color, float, MenuValue, Color>(
      "Set Alpha",
      [](const Color &color, const float alpha, const MenuValue &type) -> Color {
        return Color(set_alpha(float4(color), alpha, type));
      },
      mf::build::exec_presets::AllSpanOrSingle());

  builder.set_matching_fn(function);
}

}  // namespace blender::nodes::node_composite_setalpha_cc

static void register_node_type_cmp_setalpha()
{
  namespace file_ns = blender::nodes::node_composite_setalpha_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSetAlpha", CMP_NODE_SETALPHA);
  ntype.ui_name = "Set Alpha";
  ntype.ui_description = "Add an alpha channel to an image";
  ntype.enum_name_legacy = "SETALPHA";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_setalpha_declare;
  ntype.initfunc = file_ns::node_composit_init_setalpha;
  blender::bke::node_type_storage(
      ntype, "NodeSetAlpha", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_gpu_material;
  ntype.build_multi_function = file_ns::node_build_multi_function;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_setalpha)
