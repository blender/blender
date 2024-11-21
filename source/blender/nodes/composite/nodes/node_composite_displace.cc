/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Displace  ******************** */

namespace blender::nodes::node_composite_displace_cc {

static void cmp_node_displace_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Vector>("Vector")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_TRANSLATION)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("X Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .compositor_domain_priority(2);
  b.add_input<decl::Float>("Y Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
}

using namespace blender::realtime_compositor;

class DisplaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
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
    GPUShader *shader = context().get_shader("compositor_displace");
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    GPU_texture_mipmap_mode(input_image, true, true);
    GPU_texture_anisotropic_filter(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_displacement = get_input("Vector");
    input_displacement.bind_as_texture(shader, "displacement_tx");
    const Result &input_x_scale = get_input("X Scale");
    input_x_scale.bind_as_texture(shader, "x_scale_tx");
    const Result &input_y_scale = get_input("Y Scale");
    input_y_scale.bind_as_texture(shader, "y_scale_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_displacement.unbind_as_texture();
    input_x_scale.unbind_as_texture();
    input_y_scale.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const Result &image = get_input("Image");
    const Result &input_displacement = get_input("Vector");
    const Result &x_scale = get_input("X Scale");
    const Result &y_scale = get_input("Y Scale");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    /* In order to perform EWA sampling, we need to compute the partial derivative of the displaced
     * coordinates along the x and y directions using a finite difference approximation. But in
     * order to avoid loading multiple neighbouring displacement values for each pixel, we operate
     * on the image in 2x2 blocks of pixels, where the derivatives are computed horizontally and
     * vertically across the 2x2 block such that odd texels use a forward finite difference
     * equation while even invocations use a backward finite difference equation. */
    const int2 size = domain.size;
    parallel_for(math::divide_ceil(size, int2(2)), [&](const int2 base_texel) {
      const int x = base_texel.x * 2;
      const int y = base_texel.y * 2;

      const int2 lower_left_texel = int2(x, y);
      const int2 lower_right_texel = int2(x + 1, y);
      const int2 upper_left_texel = int2(x, y + 1);
      const int2 upper_right_texel = int2(x + 1, y + 1);

      auto compute_coordinates = [&](const int2 &texel) -> float2 {
        /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size
         * to get the coordinates into the sampler's expected [0, 1] range. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

        /* Note that the input displacement is in pixel space, so divide by the input size to
         * transform it into the normalized sampler space. */
        float2 scale = float2(x_scale.load_pixel_extended(texel).x,
                              y_scale.load_pixel_extended(texel).x);
        float2 displacement = input_displacement.load_pixel_extended(texel).xy() * scale /
                              float2(size);
        return coordinates - displacement;
      };

      const float2 lower_left_coordinates = compute_coordinates(lower_left_texel);
      const float2 lower_right_coordinates = compute_coordinates(lower_right_texel);
      const float2 upper_left_coordinates = compute_coordinates(upper_left_texel);
      const float2 upper_right_coordinates = compute_coordinates(upper_right_texel);

      /* Compute the partial derivatives using finite difference. Divide by the input size since
       * sample_ewa_zero assumes derivatives with respect to texel coordinates. */
      const float2 lower_x_gradient = (lower_right_coordinates - lower_left_coordinates) / size.x;
      const float2 left_y_gradient = (upper_left_coordinates - lower_left_coordinates) / size.y;
      const float2 right_y_gradient = (upper_right_coordinates - lower_right_coordinates) / size.y;
      const float2 upper_x_gradient = (upper_right_coordinates - upper_left_coordinates) / size.x;

      /* Computes one of the 2x2 pixels given its texel location, coordinates, and gradients. */
      auto compute_pixel = [&](const int2 &texel,
                               const float2 &coordinates,
                               const float2 &x_gradient,
                               const float2 &y_gradient) {
        /* Sample the input using the displaced coordinates passing in the computed gradients in
         * order to utilize the anisotropic filtering capabilities of the sampler. */
        float4 displaced_color = image.sample_ewa_zero(coordinates, x_gradient, y_gradient);
        output.store_pixel(texel, displaced_color);
      };

      /* Compute each of the pixels in the 2x2 block, making sure to exempt out of bounds right
       * and upper pixels. */
      compute_pixel(lower_left_texel, lower_left_coordinates, lower_x_gradient, left_y_gradient);
      if (lower_right_texel.x != size.x) {
        compute_pixel(
            lower_right_texel, lower_right_coordinates, lower_x_gradient, right_y_gradient);
      }
      if (upper_left_texel.y != size.y) {
        compute_pixel(upper_left_texel, upper_left_coordinates, upper_x_gradient, left_y_gradient);
      }
      if (upper_right_texel.x != size.x && upper_right_texel.y != size.y) {
        compute_pixel(
            upper_right_texel, upper_right_coordinates, upper_x_gradient, right_y_gradient);
      }
    });
  }

  bool is_identity()
  {
    const Result &input_image = get_input("Image");
    if (input_image.is_single_value()) {
      return true;
    }

    const Result &input_displacement = get_input("Vector");
    if (input_displacement.is_single_value() &&
        math::is_zero(input_displacement.get_vector_value()))
    {
      return true;
    }

    const Result &input_x_scale = get_input("X Scale");
    const Result &input_y_scale = get_input("Y Scale");
    if (input_x_scale.is_single_value() && input_x_scale.get_float_value() == 0.0f &&
        input_y_scale.is_single_value() && input_y_scale.get_float_value() == 0.0f)
    {
      return true;
    }

    return false;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DisplaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_displace_cc

void register_node_type_cmp_displace()
{
  namespace file_ns = blender::nodes::node_composite_displace_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DISPLACE, "Displace", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_displace_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
