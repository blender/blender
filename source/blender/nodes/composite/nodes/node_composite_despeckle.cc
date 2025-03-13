/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** FILTER  ******************** */

namespace blender::nodes::node_composite_despeckle_cc {

static void cmp_node_despeckle_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Fac")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_despeckle(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom3 = 0.5f;
  node->custom4 = 0.5f;
}

static void node_composit_buts_despeckle(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(col, ptr, "threshold_neighbor", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class DespeckleOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_despeckle");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "color_threshold", get_color_threshold());
    GPU_shader_uniform_1f(shader, "neighbor_threshold", get_neighbor_threshold());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &factor_image = get_input("Fac");
    factor_image.bind_as_texture(shader, "factor_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    factor_image.unbind_as_texture();
  }

  void execute_cpu()
  {
    const float color_threshold = this->get_color_threshold();
    const float neighbor_threshold = this->get_neighbor_threshold();

    const Result &input = get_input("Image");
    const Result &factor_image = get_input("Fac");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    /* A 3x3 weights kernel whose weights are the inverse of the distance to the center of the
     * kernel. So the center weight is zero, the corners weights are (1 / sqrt(2)), and the rest
     * of the weights are 1. The total sum of weights is 4 plus quadruple the corner weight. */
    float corner_weight = 1.0f / math::sqrt(2.0f);
    float sum_of_weights = 4.0f + corner_weight * 4.0f;
    float3x3 weights = float3x3(float3(corner_weight, 1.0f, corner_weight),
                                float3(1.0f, 0.0f, 1.0f),
                                float3(corner_weight, 1.0f, corner_weight));

    parallel_for(domain.size, [&](const int2 texel) {
      float4 center_color = input.load_pixel<float4>(texel);

      /* Go over the pixels in the 3x3 window around the center pixel and compute the total sum of
       * their colors multiplied by their weights. Additionally, for pixels whose colors are not
       * close enough to the color of the center pixel, accumulate their color as well as their
       * weights. */
      float4 sum_of_colors = float4(0.0f);
      float accumulated_weight = 0.0f;
      float4 accumulated_color = float4(0.0f);
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
          float weight = weights[j][i];
          float4 color = input.load_pixel_extended<float4>(texel + int2(i - 1, j - 1)) * weight;
          sum_of_colors += color;
          if (!math::is_equal(center_color.xyz(), color.xyz(), color_threshold)) {
            accumulated_color += color;
            accumulated_weight += weight;
          }
        }
      }

      /* If the accumulated weight is zero, that means all pixels in the 3x3 window are similar and
       * no need to despeckle anything, so write the original center color and return. */
      if (accumulated_weight == 0.0f) {
        output.store_pixel(texel, center_color);
        return;
      }

      /* If the ratio between the accumulated weights and the total sum of weights is not larger
       * than the user specified neighbor threshold, then the number of pixels in the neighborhood
       * that are not close enough to the center pixel is low, and no need to despeckle anything,
       * so write the original center color and return. */
      if (accumulated_weight / sum_of_weights < neighbor_threshold) {
        output.store_pixel(texel, center_color);
        return;
      }

      /* If the weighted average color of the neighborhood is close enough to the center pixel,
       * then no need to despeckle anything, so write the original center color and return. */
      if (math::is_equal(
              center_color.xyz(), (sum_of_colors / sum_of_weights).xyz(), color_threshold))
      {
        output.store_pixel(texel, center_color);
        return;
      }

      /* We need to despeckle, so write the mean accumulated color. */
      float factor = factor_image.load_pixel<float, true>(texel);
      float4 mean_color = accumulated_color / accumulated_weight;
      output.store_pixel(texel, math::interpolate(center_color, mean_color, factor));
    });
  }

  float get_color_threshold()
  {
    return bnode().custom3;
  }

  float get_neighbor_threshold()
  {
    return bnode().custom4;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DespeckleOperation(context, node);
}

}  // namespace blender::nodes::node_composite_despeckle_cc

void register_node_type_cmp_despeckle()
{
  namespace file_ns = blender::nodes::node_composite_despeckle_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDespeckle", CMP_NODE_DESPECKLE);
  ntype.ui_name = "Despeckle";
  ntype.ui_description =
      "Smooth areas of an image in which noise is noticeable, while leaving complex areas "
      "untouched";
  ntype.enum_name_legacy = "DESPECKLE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_despeckle_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_despeckle;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_despeckle;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
