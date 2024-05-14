/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_matrix.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_algorithm_transform.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Translate ******************** */

namespace blender::nodes::node_composite_translate_cc {

NODE_STORAGE_FUNCS(NodeTranslateData)

static void cmp_node_translate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("X")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Y")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_translate(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTranslateData *data = MEM_cnew<NodeTranslateData>(__func__);
  node->storage = data;
}

static void node_composit_buts_translate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "wrap_axis", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class TranslateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &result = get_result("Image");
    input.pass_through(result);

    float x = get_input("X").get_float_value_default(0.0f);
    float y = get_input("Y").get_float_value_default(0.0f);
    if (get_use_relative()) {
      x *= input.domain().size.x;
      y *= input.domain().size.y;
    }

    const float2 translation = float2(x, y);
    const float3x3 transformation = math::from_location<float3x3>(translation);

    RealizationOptions realization_options = input.get_realization_options();
    realization_options.wrap_x = get_wrap_x();
    realization_options.wrap_y = get_wrap_y();
    realization_options.interpolation = get_interpolation();

    transform(context(), input, result, transformation, realization_options);
  }

  Interpolation get_interpolation()
  {
    switch (node_storage(bnode()).interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    BLI_assert_unreachable();
    return Interpolation::Nearest;
  }

  bool get_use_relative()
  {
    return node_storage(bnode()).relative;
  }

  bool get_wrap_x()
  {
    return ELEM(node_storage(bnode()).wrap_axis, CMP_NODE_WRAP_X, CMP_NODE_WRAP_XY);
  }

  bool get_wrap_y()
  {
    return ELEM(node_storage(bnode()).wrap_axis, CMP_NODE_WRAP_Y, CMP_NODE_WRAP_XY);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TranslateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_translate_cc

void register_node_type_cmp_translate()
{
  namespace file_ns = blender::nodes::node_composite_translate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TRANSLATE, "Translate", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_translate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_translate;
  ntype.initfunc = file_ns::node_composit_init_translate;
  blender::bke::node_type_storage(
      &ntype, "NodeTranslateData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
