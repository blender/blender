/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"

#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BILATERALBLUR ******************** */

namespace blender::nodes::node_composite_bilateralblur_cc {

static void cmp_node_bilateralblur_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Color>("Determinator")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Int>("Size").default_value(0).min(0).description(
      "The size of the blur in pixels");
  b.add_input<decl::Float>("Threshold")
      .default_value(0.1f)
      .min(0.0f)
      .description(
          "Pixels are considered in the blur area if the average difference between their "
          "determinator and the determinator of the center pixel is less than this threshold");
}

using namespace blender::compositor;

class BilateralBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    Result &output_image = this->get_result("Image");
    if (input_image.is_single_value() || this->get_blur_radius() == 0 ||
        this->get_threshold() == 0.0f)
    {
      output_image.share_data(input_image);
      return;
    }

    /* If the determinator is a single value, then the node essentially becomes a box blur. */
    const Result &determinator_image = get_input("Determinator");
    if (determinator_image.is_single_value()) {
      symmetric_separable_blur(this->context(),
                               input_image,
                               output_image,
                               float2(this->get_blur_radius()),
                               R_FILTER_BOX);
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
    gpu::Shader *shader = context().get_shader("compositor_bilateral_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", get_blur_radius());
    GPU_shader_uniform_1f(shader, "threshold", get_threshold());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &determinator_image = get_input("Determinator");
    determinator_image.bind_as_texture(shader, "determinator_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    determinator_image.unbind_as_texture();
  }

  void execute_cpu()
  {
    const int radius = this->get_blur_radius();
    const float threshold = this->get_threshold();

    const Result &input = get_input("Image");
    const Result &determinator_image = get_input("Determinator");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 center_determinator = float4(determinator_image.load_pixel<Color>(texel));

      /* Go over the pixels in the blur window of the specified radius around the center pixel, and
       * for pixels whose determinator is close enough to the determinator of the center pixel,
       * accumulate their color as well as their weights. */
      float accumulated_weight = 0.0f;
      float4 accumulated_color = float4(0.0f);
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          float4 determinator = float4(
              determinator_image.load_pixel_extended<Color>(texel + int2(x, y)));
          float difference = math::dot(math::abs(center_determinator - determinator).xyz(),
                                       float3(1.0f)) /
                             3.0f;

          if (difference < threshold) {
            accumulated_weight += 1.0f;
            accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, y)));
          }
        }
      }

      /* Write the accumulated color divided by the accumulated weight if any pixel in the window
       * was accumulated, otherwise, write a fallback black color. */
      float4 fallback = float4(float3(0.0f), 1.0f);
      float4 color = (accumulated_weight != 0.0f) ? (accumulated_color / accumulated_weight) :
                                                    fallback;
      output.store_pixel(texel, Color(color));
    });
  }

  int get_blur_radius()
  {
    return math::max(0, this->get_input("Size").get_single_value_default(0));
  }

  float get_threshold()
  {
    return math::max(0.0f, this->get_input("Threshold").get_single_value_default(0.1f));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BilateralBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bilateralblur_cc

static void register_node_type_cmp_bilateralblur()
{
  namespace file_ns = blender::nodes::node_composite_bilateralblur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeBilateralblur", CMP_NODE_BILATERALBLUR);
  ntype.ui_name = "Bilateral Blur";
  ntype.ui_description = "Adaptively blur image, while retaining sharp edges";
  ntype.enum_name_legacy = "BILATERALBLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_bilateralblur_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_bilateralblur)
