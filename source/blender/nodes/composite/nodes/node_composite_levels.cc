/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

static void node_declare(NodeDeclarationBuilder &b)
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
    if (this->get_input("Image").is_single_value()) {
      this->execute_single_value();
      return;
    }

    const float4 mean = this->compute_mean();

    Result &mean_result = this->get_result("Mean");
    if (mean_result.should_compute()) {
      mean_result.allocate_single_value();
      this->set_output(mean, mean_result);
    }

    Result &standard_deviation_result = this->get_result("Standard Deviation");
    if (standard_deviation_result.should_compute()) {
      const float4 standard_deviation = this->compute_standard_deviation(mean);
      standard_deviation_result.allocate_single_value();
      this->set_output(standard_deviation, standard_deviation_result);
    }
  }

  void execute_single_value()
  {
    Result &standard_deviation_result = this->get_result("Standard Deviation");
    if (standard_deviation_result.should_compute()) {
      standard_deviation_result.allocate_single_value();
      standard_deviation_result.set_single_value(0.0f);
    }

    Result &mean_result = this->get_result("Mean");
    if (!mean_result.should_compute()) {
      return;
    }

    mean_result.allocate_single_value();
    this->set_output(float4(this->get_input("Image").get_single_value<Color>()), mean_result);
  }

  float4 compute_mean()
  {
    const Result &input = this->get_input("Image");
    return sum_color(this->context(), input) / math::reduce_mul(input.domain().data_size);
  }

  float4 compute_standard_deviation(float4 mean)
  {
    const Result &input = this->get_input("Image");
    const float4 sum_of_squared_difference_to_mean = sum_squared_difference_color(
        this->context(), input, mean);
    const float4 mean_squared_difference_to_mean = sum_of_squared_difference_to_mean /
                                                   math::reduce_mul(input.domain().data_size);
    return math::sqrt(mean_squared_difference_to_mean);
  }

  void set_output(const float4 value, Result &output)
  {
    switch (this->get_channel()) {
      case CMP_NODE_LEVLES_RED:
        output.set_single_value(value.x);
        return;
      case CMP_NODE_LEVLES_GREEN:
        output.set_single_value(value.y);
        return;
      case CMP_NODE_LEVLES_BLUE:
        output.set_single_value(value.z);
        return;
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        output.set_single_value(math::dot(value.xyz(), float3(luminance_coefficients_bt709_)));
        return;
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        output.set_single_value(math::dot(value.xyz(), float3(luminance_coefficients)));
        return;
      }
    }
  }

  CMPNodeLevelsChannel get_channel()
  {
    return CMPNodeLevelsChannel(
        this->get_input("Channel").get_single_value_default<MenuValue>().value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new LevelsOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeLevels", CMP_NODE_VIEW_LEVELS);
  ntype.ui_name = "Levels";
  ntype.ui_description = "Compute average and standard deviation of pixel values";
  ntype.enum_name_legacy = "LEVELS";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_levels_cc
