/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void fn_node_combine_color_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Red")).default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Green"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Blue"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Alpha"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Color>(N_("Color"));
};

static void fn_node_combine_color_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void fn_node_combine_color_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeCombSepColor &storage = node_storage(*node);
  node_combsep_color_label(&node->inputs, (NodeCombSepColorMode)storage.mode);
}

static void fn_node_combine_color_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeCombSepColor *data = MEM_cnew<NodeCombSepColor>(__func__);
  data->mode = NODE_COMBSEP_COLOR_RGB;
  node->storage = data;
}

static const fn::MultiFunction *get_multi_function(const bNode &bnode)
{
  const NodeCombSepColor &storage = node_storage(bnode);

  static fn::CustomMF_SI_SI_SI_SI_SO<float, float, float, float, ColorGeometry4f> rgba_fn{
      "RGB", [](float r, float g, float b, float a) { return ColorGeometry4f(r, g, b, a); }};
  static fn::CustomMF_SI_SI_SI_SI_SO<float, float, float, float, ColorGeometry4f> hsva_fn{
      "HSV", [](float h, float s, float v, float a) {
        ColorGeometry4f r_color;
        hsv_to_rgb(h, s, v, &r_color.r, &r_color.g, &r_color.b);
        r_color.a = a;
        return r_color;
      }};
  static fn::CustomMF_SI_SI_SI_SI_SO<float, float, float, float, ColorGeometry4f> hsla_fn{
      "HSL", [](float h, float s, float l, float a) {
        ColorGeometry4f color;
        hsl_to_rgb(h, s, l, &color.r, &color.g, &color.b);
        color.a = a;
        return color;
      }};

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

static void fn_node_combine_color_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const fn::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes

void register_node_type_fn_combine_color(void)
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_CONVERTER);
  ntype.declare = blender::nodes::fn_node_combine_color_declare;
  node_type_update(&ntype, blender::nodes::fn_node_combine_color_update);
  node_type_init(&ntype, blender::nodes::fn_node_combine_color_init);
  node_type_storage(
      &ntype, "NodeCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = blender::nodes::fn_node_combine_color_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_combine_color_layout;

  nodeRegisterType(&ntype);
}
