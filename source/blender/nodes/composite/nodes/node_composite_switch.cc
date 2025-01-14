/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Switch ******************** */

namespace blender::nodes::node_composite_switch_cc {

static void cmp_node_switch_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Off").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>("On").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_switch(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "check", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class SwitchOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input(get_condition() ? "On" : "Off");
    Result &result = get_result("Image");
    input.pass_through(result);
  }

  bool get_condition()
  {
    return bnode().custom1;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new SwitchOperation(context, node);
}

}  // namespace blender::nodes::node_composite_switch_cc

void register_node_type_cmp_switch()
{
  namespace file_ns = blender::nodes::node_composite_switch_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSwitch", CMP_NODE_SWITCH);
  ntype.ui_name = "Switch";
  ntype.ui_description = "Switch between two images using a checkbox";
  ntype.enum_name_legacy = "SWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_switch_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_switch;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Default);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
