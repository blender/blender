/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_math_color.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

namespace blender::nodes::node_fn_combine_color_cc {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Red")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Red");
          case NODE_COMBSEP_COLOR_HSV:
          case NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Hue");
        }
      });
  b.add_input<decl::Float>("Green")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Green");
          case NODE_COMBSEP_COLOR_HSV:
          case NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Saturation");
        }
      });
  b.add_input<decl::Float>("Blue")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .label_fn([](bNode node) {
        switch (node_storage(node).mode) {
          case NODE_COMBSEP_COLOR_RGB:
          default:
            return IFACE_("Blue");
          case NODE_COMBSEP_COLOR_HSV:
            return CTX_IFACE_(BLT_I18NCONTEXT_COLOR, "Value");
          case NODE_COMBSEP_COLOR_HSL:
            return IFACE_("Lightness");
        }
      });
  b.add_input<decl::Float>("Alpha").default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>("Color");
};

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeCombSepColor *data = MEM_callocN<NodeCombSepColor>(__func__);
  data->mode = NODE_COMBSEP_COLOR_RGB;
  node->storage = data;
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  const NodeCombSepColor &storage = node_storage(bnode);

  static auto rgba_fn = mf::build::SI4_SO<float, float, float, float, ColorGeometry4f>(
      "RGB", [](float r, float g, float b, float a) { return ColorGeometry4f(r, g, b, a); });
  static auto hsva_fn = mf::build::SI4_SO<float, float, float, float, ColorGeometry4f>(
      "HSV", [](float h, float s, float v, float a) {
        ColorGeometry4f color;
        hsv_to_rgb(h, s, v, &color.r, &color.g, &color.b);
        color.a = a;
        return color;
      });
  static auto hsla_fn = mf::build::SI4_SO<float, float, float, float, ColorGeometry4f>(
      "HSL", [](float h, float s, float l, float a) {
        ColorGeometry4f color;
        hsl_to_rgb(h, s, l, &color.r, &color.g, &color.b);
        color.a = a;
        return color;
      });

  switch (storage.mode) {
    case NODE_COMBSEP_COLOR_RGB:
      return &rgba_fn;
    case NODE_COMBSEP_COLOR_HSV:
      return &hsva_fn;
    case NODE_COMBSEP_COLOR_HSL:
      return &hsla_fn;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Mode of color processing",
                    rna_enum_node_combsep_color_items,
                    NOD_storage_enum_accessors(mode));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeCombineColor", FN_NODE_COMBINE_COLOR);
  ntype.ui_name = "Combine Color";
  ntype.ui_description =
      "Combine four channels into a single color, based on a particular color model";
  ntype.enum_name_legacy = "COMBINE_COLOR";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = node_build_multi_function;
  ntype.draw_buttons = node_layout;

  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_combine_color_cc
