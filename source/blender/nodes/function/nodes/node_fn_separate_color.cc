/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "BLI_math_color.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

namespace blender::nodes::node_fn_separate_color_cc {

NODE_STORAGE_FUNCS(NodeCombSepColor)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>("Red").label_fn([](bNode node) {
    switch (node_storage(node).mode) {
      case NODE_COMBSEP_COLOR_RGB:
      default:
        return IFACE_("Red");
      case NODE_COMBSEP_COLOR_HSV:
      case NODE_COMBSEP_COLOR_HSL:
        return IFACE_("Hue");
    }
  });
  b.add_output<decl::Float>("Green").label_fn([](bNode node) {
    switch (node_storage(node).mode) {
      case NODE_COMBSEP_COLOR_RGB:
      default:
        return IFACE_("Green");
      case NODE_COMBSEP_COLOR_HSV:
      case NODE_COMBSEP_COLOR_HSL:
        return IFACE_("Saturation");
    }
  });
  b.add_output<decl::Float>("Blue").label_fn([](bNode node) {
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
  b.add_output<decl::Float>("Alpha");
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

class SeparateRGBAFunction : public mf::MultiFunction {
 public:
  SeparateRGBAFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate Color", signature};
      builder.single_input<ColorGeometry4f>("Color");
      builder.single_output<float>("Red", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Green", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Blue", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Alpha", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
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
      mask.foreach_segment_optimized([&](const auto segment) {
        const int used_outputs_num = used_outputs.size();
        const int *used_outputs_data = used_outputs.data();

        for (const int64_t i : segment) {
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

class SeparateHSVAFunction : public mf::MultiFunction {
 public:
  SeparateHSVAFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate Color", signature};
      builder.single_input<ColorGeometry4f>("Color");
      builder.single_output<float>("Hue");
      builder.single_output<float>("Saturation");
      builder.single_output<float>("Value");
      builder.single_output<float>("Alpha", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> hue = params.uninitialized_single_output<float>(1, "Hue");
    MutableSpan<float> saturation = params.uninitialized_single_output<float>(2, "Saturation");
    MutableSpan<float> value = params.uninitialized_single_output<float>(3, "Value");
    MutableSpan<float> alpha = params.uninitialized_single_output_if_required<float>(4, "Alpha");

    mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
      rgb_to_hsv(colors[i].r, colors[i].g, colors[i].b, &hue[i], &saturation[i], &value[i]);
    });

    if (!alpha.is_empty()) {
      mask.foreach_index_optimized<int64_t>([&](const int64_t i) { alpha[i] = colors[i].a; });
    }
  }
};

class SeparateHSLAFunction : public mf::MultiFunction {
 public:
  SeparateHSLAFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate Color", signature};
      builder.single_input<ColorGeometry4f>("Color");
      builder.single_output<float>("Hue");
      builder.single_output<float>("Saturation");
      builder.single_output<float>("Lightness");
      builder.single_output<float>("Alpha", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<ColorGeometry4f> &colors = params.readonly_single_input<ColorGeometry4f>(0,
                                                                                          "Color");
    MutableSpan<float> hue = params.uninitialized_single_output<float>(1, "Hue");
    MutableSpan<float> saturation = params.uninitialized_single_output<float>(2, "Saturation");
    MutableSpan<float> lightness = params.uninitialized_single_output<float>(3, "Lightness");
    MutableSpan<float> alpha = params.uninitialized_single_output_if_required<float>(4, "Alpha");

    mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
      rgb_to_hsl(colors[i].r, colors[i].g, colors[i].b, &hue[i], &saturation[i], &lightness[i]);
    });

    if (!alpha.is_empty()) {
      mask.foreach_index_optimized<int64_t>([&](const int64_t i) { alpha[i] = colors[i].a; });
    }
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
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

  fn_node_type_base(&ntype, "FunctionNodeSeparateColor", FN_NODE_SEPARATE_COLOR);
  ntype.ui_name = "Separate Color";
  ntype.ui_description = "Split a color into separate channels, based on a particular color model";
  ntype.enum_name_legacy = "SEPARATE_COLOR";
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

}  // namespace blender::nodes::node_fn_separate_color_cc
