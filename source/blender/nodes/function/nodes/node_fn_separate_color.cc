/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void fn_node_separate_color_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>(N_("Red"));
  b.add_output<decl::Float>(N_("Green"));
  b.add_output<decl::Float>(N_("Blue"));
  b.add_output<decl::Float>(N_("Alpha"));
};

static void fn_node_separate_color_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void fn_node_separate_color_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeCombSepColor &storage = node_storage(*node);
  node_combsep_color_label(&node->outputs, (NodeCombSepColorMode)storage.mode);
}

static void fn_node_separate_color_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeCombSepColor *data = MEM_cnew<NodeCombSepColor>(__func__);
  data->mode = NODE_COMBSEP_COLOR_RGB;
  node->storage = data;
}

class SeparateRGBAFunction : public fn::MultiFunction {
 public:
  SeparateRGBAFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Separate Color"};
    signature.single_input<ColorGeometry4f>("Color");
    signature.single_output<float>("Red");
    signature.single_output<float>("Green");
    signature.single_output<float>("Blue");
    signature.single_output<float>("Alpha");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");

    MutableSpan<float> red = params.uninitialized_single_output_if_required<float>(1, "Red");
    MutableSpan<float> green = params.uninitialized_single_output_if_required<float>(2, "Green");
    MutableSpan<float> blue = params.uninitialized_single_output_if_required<float>(3, "Blue");
    MutableSpan<float> alpha = params.uninitialized_single_output_if_required<float>(4, "Alpha");

    std::array<MutableSpan<float>, 4> outputs = {red, green, blue, alpha};
    Vector<int> used_outputs;
    if (!red.is_empty()) {
      used_outputs.append(0);
    }
    if (!green.is_empty()) {
      used_outputs.append(1);
    }
    if (!blue.is_empty()) {
      used_outputs.append(2);
    }
    if (!alpha.is_empty()) {
      used_outputs.append(3);
    }

    devirtualize_varray(colors, [&](auto colors) {
      mask.to_best_mask_type([&](auto mask) {
        const int used_outputs_num = used_outputs.size();
        const int *used_outputs_data = used_outputs.data();

        for (const int64_t i : mask) {
          const ColorGeometry4f &color = colors[i];
          for (const int out_i : IndexRange(used_outputs_num)) {
            const int channel = used_outputs_data[out_i];
            outputs[channel][i] = color[channel];
          }
        }
      });
    });
  }
};

class SeparateHSVAFunction : public fn::MultiFunction {
 public:
  SeparateHSVAFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Separate Color"};
    signature.single_input<ColorGeometry4f>("Color");
    signature.single_output<float>("Hue");
    signature.single_output<float>("Saturation");
    signature.single_output<float>("Value");
    signature.single_output<float>("Alpha");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> hue = params.uninitialized_single_output<float>(1, "Hue");
    MutableSpan<float> saturation = params.uninitialized_single_output<float>(2, "Saturation");
    MutableSpan<float> value = params.uninitialized_single_output<float>(3, "Value");
    MutableSpan<float> alpha = params.uninitialized_single_output_if_required<float>(4, "Alpha");

    for (int64_t i : mask) {
      rgb_to_hsv(colors[i].r, colors[i].g, colors[i].b, &hue[i], &saturation[i], &value[i]);
    }

    if (!alpha.is_empty()) {
      for (int64_t i : mask) {
        alpha[i] = colors[i].a;
      }
    }
  }
};

class SeparateHSLAFunction : public fn::MultiFunction {
 public:
  SeparateHSLAFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Separate Color"};
    signature.single_input<ColorGeometry4f>("Color");
    signature.single_output<float>("Hue");
    signature.single_output<float>("Saturation");
    signature.single_output<float>("Lightness");
    signature.single_output<float>("Alpha");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> hue = params.uninitialized_single_output<float>(1, "Hue");
    MutableSpan<float> saturation = params.uninitialized_single_output<float>(2, "Saturation");
    MutableSpan<float> lightness = params.uninitialized_single_output<float>(3, "Lightness");
    MutableSpan<float> alpha = params.uninitialized_single_output_if_required<float>(4, "Alpha");

    for (int64_t i : mask) {
      rgb_to_hsl(colors[i].r, colors[i].g, colors[i].b, &hue[i], &saturation[i], &lightness[i]);
    }

    if (!alpha.is_empty()) {
      for (int64_t i : mask) {
        alpha[i] = colors[i].a;
      }
    }
  }
};

static void fn_node_separate_color_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeCombSepColor &storage = node_storage(builder.node());

  switch (storage.mode) {
    case NODE_COMBSEP_COLOR_RGB: {
      static SeparateRGBAFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_COMBSEP_COLOR_HSV: {
      static SeparateHSVAFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_COMBSEP_COLOR_HSL: {
      static SeparateHSLAFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

}  // namespace blender::nodes

void register_node_type_fn_separate_color(void)
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_CONVERTER);
  ntype.declare = blender::nodes::fn_node_separate_color_declare;
  node_type_update(&ntype, blender::nodes::fn_node_separate_color_update);
  node_type_init(&ntype, blender::nodes::fn_node_separate_color_init);
  node_type_storage(
      &ntype, "NodeCombSepColor", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = blender::nodes::fn_node_separate_color_build_multi_function;
  ntype.draw_buttons = blender::nodes::fn_node_separate_color_layout;

  nodeRegisterType(&ntype);
}
