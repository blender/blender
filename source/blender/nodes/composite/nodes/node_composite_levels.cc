/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_colormanagement.h"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** LEVELS ******************** */

namespace blender::nodes::node_composite_levels_cc {

static void cmp_node_levels_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Mean");
  b.add_output<decl::Float>("Std Dev");
}

static void node_composit_init_view_levels(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1; /* All channels. */
}

static void node_composit_buts_view_levels(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "channel", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

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
      mean_result.set_float_value(mean);
    }

    Result &standard_deviation_result = get_result("Std Dev");
    if (standard_deviation_result.should_compute()) {
      const float standard_deviation = compute_standard_deviation(mean);
      standard_deviation_result.allocate_single_value();
      standard_deviation_result.set_float_value(standard_deviation);
    }
  }

  void execute_single_value()
  {
    Result &standard_deviation_result = get_result("Std Dev");
    if (standard_deviation_result.should_compute()) {
      standard_deviation_result.allocate_single_value();
      standard_deviation_result.set_float_value(0.0f);
    }

    Result &mean_result = get_result("Mean");
    if (!mean_result.should_compute()) {
      return;
    }

    mean_result.allocate_single_value();
    const float3 input = float3(get_input("Image").get_color_value());

    switch (get_channel()) {
      case CMP_NODE_LEVLES_RED:
        mean_result.set_float_value(input.x);
        break;
      case CMP_NODE_LEVLES_GREEN:
        mean_result.set_float_value(input.y);
        break;
      case CMP_NODE_LEVLES_BLUE:
        mean_result.set_float_value(input.z);
        break;
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        mean_result.set_float_value(math::dot(input, float3(luminance_coefficients_bt709_)));
        break;
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        mean_result.set_float_value(math::dot(input, float3(luminance_coefficients)));
        break;
      }
      default:
        BLI_assert_unreachable();
        break;
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
        return sum_red(context(), input.texture());
      case CMP_NODE_LEVLES_GREEN:
        return sum_green(context(), input.texture());
      case CMP_NODE_LEVLES_BLUE:
        return sum_blue(context(), input.texture());
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        return sum_luminance(context(), input.texture(), float3(luminance_coefficients_bt709_));
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        return sum_luminance(context(), input.texture(), float3(luminance_coefficients));
      }
      default:
        BLI_assert_unreachable();
        return 0.0f;
    }
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
        return sum_red_squared_difference(context(), input.texture(), subtrahend);
      case CMP_NODE_LEVLES_GREEN:
        return sum_green_squared_difference(context(), input.texture(), subtrahend);
      case CMP_NODE_LEVLES_BLUE:
        return sum_blue_squared_difference(context(), input.texture(), subtrahend);
      case CMP_NODE_LEVLES_LUMINANCE_BT709:
        return sum_luminance_squared_difference(
            context(), input.texture(), float3(luminance_coefficients_bt709_), subtrahend);
      case CMP_NODE_LEVLES_LUMINANCE: {
        float luminance_coefficients[3];
        IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
        return sum_luminance_squared_difference(
            context(), input.texture(), float3(luminance_coefficients), subtrahend);
      }
      default:
        BLI_assert_unreachable();
        return 0.0f;
    }
  }

  CMPNodeLevelsChannel get_channel()
  {
    return static_cast<CMPNodeLevelsChannel>(bnode().custom1);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LevelsOperation(context, node);
}

}  // namespace blender::nodes::node_composite_levels_cc

void register_node_type_cmp_view_levels()
{
  namespace file_ns = blender::nodes::node_composite_levels_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VIEW_LEVELS, "Levels", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::cmp_node_levels_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_view_levels;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_view_levels;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
