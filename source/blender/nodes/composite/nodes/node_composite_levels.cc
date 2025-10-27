/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_types.hh"

#include "IMB_colormanagement.hh"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_levels_cc {

static const EnumPropertyItem channel_items[] = {
    {CMP_NODE_LEVLES_LUMINANCE, "COMBINED_RGB", 0, N_("Combined"), N_("Combined RGB")},
    {CMP_NODE_LEVLES_RED, "RED", 0, N_("Red"), N_("Red Channel")},
    {CMP_NODE_LEVLES_GREEN, "GREEN", 0, N_("Green"), N_("Green Channel")},
    {CMP_NODE_LEVLES_BLUE, "BLUE", 0, N_("Blue"), N_("Blue Channel")},
    {CMP_NODE_LEVLES_LUMINANCE_BT709, "LUMINANCE", 0, N_("Luminance"), N_("Luminance Channel")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_levels_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Menu>("Channel")
      .default_value(CMP_NODE_LEVLES_LUMINANCE)
      .static_items(channel_items)
      .optional_label();

  b.add_output<decl::Float>("Mean");
  b.add_output<decl::Float>("Standard Deviation");
}

using namespace blender::compositor;

class LevelsOperation : public NodeOperation {
 private:
  constexpr static float luminance_coefficients_bt709_[3] = {0.2126f, 0.7152f, 0.0722f};

 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (get_input("Image").is_single_value()) {
      execute_single_value();
      return;
    }

    const float mean = compute_mean();

    Result &mean_result = get_result("Mean");
    if (mean_result.should_compute()) {
      mean_result.allocate_single_value();
      mean_result.set_single_value(mean);
    }

    Result &standard_deviation_result = get_result("Standard Deviation");
    if (standard_deviation_result.should_compute()) {
      const float standard_deviation = compute_standard_deviation(mean);
      standard_deviation_result.allocate_single_value();
      standard_deviation_result.set_single_value(standard_deviation);
    }
  }

  void execute_single_value()
  {
    Result &standard_deviation_result = get_result("Standard Deviation");
    if (standard_deviation_result.should_compute()) {
      standard_deviation_result.allocate_single_value();
      standard_deviation_result.set_single_value(0.0f);
    }

    Result &mean_result = get_result("Mean");
    if (!mean_result.should_compute()) {
      return;
    }

    mean_result.allocate_single_value();
    const float3 input = float3(get_input("Image").get_single_value<Color>());

    switch (get_channel()) {
      case CMP_NODE_LEVLES_RED:
        mean_result.set_single_value(input.x);
        break;
      case CMP_NODE_LEVLES_GREEN:
        mean_result.set_single_value(input.y);
        break;
      case CMP_NODE_LEVLES_BLUE:
        mean_result.set_single_value(input.z);
        break;
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        mean_result.set_single_value(math::dot(input, float3(luminance_coefficients_bt709_)));
        break;
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        mean_result.set_single_value(math::dot(input, float3(luminance_coefficients)));
        break;
      }
    }
  }

  float compute_mean()
  {
    const Result &input = get_input("Image");
    return compute_sum() / (input.domain().size.x * input.domain().size.y);
  }

  float compute_sum()
  {
    const Result &input = get_input("Image");
    switch (get_channel()) {
      case CMP_NODE_LEVLES_RED:
        return sum_red(context(), input);
      case CMP_NODE_LEVLES_GREEN:
        return sum_green(context(), input);
      case CMP_NODE_LEVLES_BLUE:
        return sum_blue(context(), input);
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        return sum_luminance(context(), input, float3(luminance_coefficients_bt709_));
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        return sum_luminance(context(), input, float3(luminance_coefficients));
      }
    }

    return 0.0f;
  }

  float compute_standard_deviation(float mean)
  {
    const Result &input = get_input("Image");
    const float sum = compute_sum_squared_difference(mean);
    return std::sqrt(sum / (input.domain().size.x * input.domain().size.y));
  }

  float compute_sum_squared_difference(float subtrahend)
  {
    const Result &input = get_input("Image");
    switch (get_channel()) {
      case CMP_NODE_LEVLES_RED:
        return sum_red_squared_difference(context(), input, subtrahend);
      case CMP_NODE_LEVLES_GREEN:
        return sum_green_squared_difference(context(), input, subtrahend);
      case CMP_NODE_LEVLES_BLUE:
        return sum_blue_squared_difference(context(), input, subtrahend);
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        return sum_luminance_squared_difference(
            context(), input, float3(luminance_coefficients_bt709_), subtrahend);
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        return sum_luminance_squared_difference(
            context(), input, float3(luminance_coefficients), subtrahend);
      }
    }

    return 0.0f;
  }

  CMPNodeLevelsChannel get_channel()
  {
    const Result &input = this->get_input("Channel");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_LEVLES_LUMINANCE);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeLevelsChannel>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LevelsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_levels_cc

static void register_node_type_cmp_view_levels()
{
  namespace file_ns = blender::nodes::node_composite_levels_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeLevels", CMP_NODE_VIEW_LEVELS);
  ntype.ui_name = "Levels";
  ntype.ui_description = "Compute average and standard deviation of pixel values";
  ntype.enum_name_legacy = "LEVELS";
  ntype.nclass = NODE_CLASS_OUTPUT;
  ntype.declare = file_ns::cmp_node_levels_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_view_levels)
