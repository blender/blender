/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender {

/* **************** Switch ******************** */

namespace nodes::node_composite_switch_cc {

static void cmp_node_switch_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Switch").default_value(false);
  b.add_input<decl::Color>("Off")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Color>("On")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image");
}

using namespace blender::compositor;

class SwitchOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input(this->get_condition() ? "On" : "Off");
    Result &output = this->get_result("Image");
    output.share_data(input);
  }

  bool get_condition()
  {
    return this->get_input("Switch").get_single_value_default<bool>();
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new SwitchOperation(context, node);
}

}  // namespace nodes::node_composite_switch_cc

static void register_node_type_cmp_switch()
{
  namespace file_ns = nodes::node_composite_switch_cc;

  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSwitch", CMP_NODE_SWITCH);
  ntype.ui_name = "Switch";
  ntype.ui_description = "Switch between two images using a checkbox";
  ntype.enum_name_legacy = "SWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::cmp_node_switch_declare;
  bke::node_type_size_preset(ntype, bke::eNodeSizePreset::Default);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_switch)

}  // namespace blender
