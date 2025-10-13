/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>

#include "COM_algorithm_convolve.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_convolve_cc {

enum class KernelDataType : uint8_t {
  Float = 0,
  Color = 1,
};

static const EnumPropertyItem kernel_data_type_items[] = {
    {int(KernelDataType::Float),
     "FLOAT",
     0,
     N_("Float"),
     N_("The kernel is a float and will be convolved with all input channels")},
    {int(KernelDataType::Color),
     "COLOR",
     0,
     N_("Color"),
     N_("The kernel is a color and each channel of the kernel will be convolved with each "
        "respective channel in the input")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image").hide_value().structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Menu>("Kernel Data Type")
      .default_value(KernelDataType::Float)
      .static_items(kernel_data_type_items)
      .optional_label();
  b.add_input<decl::Float>("Kernel", "Float Kernel")
      .hide_value()
      .structure_type(StructureType::Dynamic)
      .usage_by_single_menu(int(KernelDataType::Float))
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms);
  b.add_input<decl::Color>("Kernel", "Color Kernel")
      .hide_value()
      .structure_type(StructureType::Dynamic)
      .usage_by_single_menu(int(KernelDataType::Color))
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms);
  b.add_input<decl::Bool>("Normalize Kernel")
      .default_value(true)
      .description("Normalizes the kernel such that it integrates to one");
}

using namespace blender::compositor;

class ConvolveOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    const Result &kernel = this->get_kernel_input();
    Result &output = this->get_result("Image");

    if (input.is_single_value() || kernel.is_single_value()) {
      output.share_data(input);
      return;
    }

    convolve(this->context(), input, kernel, output, this->get_normalize_kernel());
  }

  const Result &get_kernel_input()
  {
    switch (this->get_kernel_data_type()) {
      case KernelDataType::Float:
        return this->get_input("Float Kernel");
      case KernelDataType::Color:
        return this->get_input("Color Kernel");
    }

    BLI_assert_unreachable();
    return this->get_input("Float Kernel");
  }

  KernelDataType get_kernel_data_type()
  {
    const Result &input = this->get_input("Kernel Data Type");
    const MenuValue default_menu_value = MenuValue(KernelDataType::Float);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<KernelDataType>(menu_value.value);
  }

  bool get_normalize_kernel()
  {
    return this->get_input("Normalize Kernel").get_single_value_default(true);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvolveOperation(context, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeConvolve");
  ntype.ui_name = "Convolve";
  ntype.ui_description = "Convolves an image with a kernel";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_convolve_cc
