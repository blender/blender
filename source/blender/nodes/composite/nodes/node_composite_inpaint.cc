/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "UI_resources.hh"

#include "COM_algorithm_jump_flooding.hh"
#include "COM_algorithm_symmetric_separable_blur_variable_size.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Inpaint/ ******************** */

namespace blender::nodes::node_composite_inpaint_cc {

static void cmp_node_inpaint_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Int>("Size").default_value(0).min(0).description(
      "The size of the inpaint in pixels");
}

using namespace blender::compositor;

class InpaintOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value() || this->get_max_distance() == 0) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    Result inpainting_boundary = compute_inpainting_boundary();

    /* Compute a jump flooding table to get the closest boundary pixel to each pixel. */
    Result flooded_boundary = context().create_result(ResultType::Int2, ResultPrecision::Half);
    jump_flooding(context(), inpainting_boundary, flooded_boundary);
    inpainting_boundary.release();

    Result filled_region = context().create_result(ResultType::Color);
    Result distance_to_boundary = context().create_result(ResultType::Float);
    Result smoothing_radius = context().create_result(ResultType::Float);
    fill_inpainting_region(
        flooded_boundary, filled_region, distance_to_boundary, smoothing_radius);
    flooded_boundary.release();

    Result smoothed_region = context().create_result(ResultType::Color);
    symmetric_separable_blur_variable_size(
        context(), filled_region, smoothing_radius, smoothed_region, get_max_distance());
    filled_region.release();
    smoothing_radius.release();

    compute_inpainting_region(smoothed_region, distance_to_boundary);
    distance_to_boundary.release();
    smoothed_region.release();
  }

  /* Compute an image that marks the boundary pixels of the inpainting region as seed pixels for
   * the jump flooding algorithm. The inpainting region is the region composed of pixels that are
   * not opaque. */
  Result compute_inpainting_boundary()
  {
    if (this->context().use_gpu()) {
      return this->compute_inpainting_boundary_gpu();
    }

    return this->compute_inpainting_boundary_cpu();
  }

  Result compute_inpainting_boundary_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_inpaint_compute_boundary",
                                               ResultPrecision::Half);
    GPU_shader_bind(shader);

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result inpainting_boundary = context().create_result(ResultType::Int2, ResultPrecision::Half);
    const Domain domain = compute_domain();
    inpainting_boundary.allocate_texture(domain);
    inpainting_boundary.bind_as_image(shader, "boundary_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    inpainting_boundary.unbind_as_image();
    GPU_shader_unbind();

    return inpainting_boundary;
  }

  Result compute_inpainting_boundary_cpu()
  {
    const Result &input = this->get_input("Image");

    Result boundary = this->context().create_result(ResultType::Int2, ResultPrecision::Half);
    const Domain domain = this->compute_domain();
    boundary.allocate_texture(domain);

    /* The in-paint operation uses a jump flood algorithm to flood the region to be in-painted with
     * the pixels at its boundary. The algorithms expects an input image whose values are those
     * returned by the initialize_jump_flooding_value function, given the texel location and a
     * boolean specifying if the pixel is a boundary one.
     *
     * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm
     * can still operate if the interior of the region was also included. However, the algorithm
     * operates more accurately when the number of pixels to be flooded is minimum. */
    parallel_for(domain.size, [&](const int2 texel) {
      /* Identify if any of the 8 neighbors around the center pixel are transparent. */
      bool has_transparent_neighbors = false;
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          int2 offset = int2(i, j);

          /* Exempt the center pixel. */
          if (offset != int2(0)) {
            if (input.load_pixel_extended<Color>(texel + offset).a < 1.0f) {
              has_transparent_neighbors = true;
              break;
            }
          }
        }
      }

      /* The pixels at the boundary are those that are opaque and have transparent neighbors. */
      bool is_opaque = input.load_pixel<Color>(texel).a == 1.0f;
      bool is_boundary_pixel = is_opaque && has_transparent_neighbors;

      /* Encode the boundary information in the format expected by the jump flooding algorithm. */
      int2 jump_flooding_value = initialize_jump_flooding_value(texel, is_boundary_pixel);

      boundary.store_pixel(texel, jump_flooding_value);
    });

    return boundary;
  }

  /* Fill the inpainting region based on the jump flooding table and write the distance to the
   * closest boundary pixel to an intermediate buffer. */
  void fill_inpainting_region(const Result &flooded_boundary,
                              Result &filled_region,
                              Result &distance_to_boundary,
                              Result &smoothing_radius)
  {
    if (this->context().use_gpu()) {
      this->fill_inpainting_region_gpu(
          flooded_boundary, filled_region, distance_to_boundary, smoothing_radius);
    }
    else {
      this->fill_inpainting_region_cpu(
          flooded_boundary, filled_region, distance_to_boundary, smoothing_radius);
    }
  }

  void fill_inpainting_region_gpu(const Result &flooded_boundary,
                                  Result &filled_region,
                                  Result &distance_to_boundary,
                                  Result &smoothing_radius)
  {
    gpu::Shader *shader = context().get_shader("compositor_inpaint_fill_region");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "max_distance", get_max_distance());

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    flooded_boundary.bind_as_texture(shader, "flooded_boundary_tx");

    const Domain domain = compute_domain();
    filled_region.allocate_texture(domain);
    filled_region.bind_as_image(shader, "filled_region_img");

    distance_to_boundary.allocate_texture(domain);
    distance_to_boundary.bind_as_image(shader, "distance_to_boundary_img");

    smoothing_radius.allocate_texture(domain);
    smoothing_radius.bind_as_image(shader, "smoothing_radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    flooded_boundary.unbind_as_texture();
    filled_region.unbind_as_image();
    distance_to_boundary.unbind_as_image();
    smoothing_radius.unbind_as_image();
    GPU_shader_unbind();
  }

  void fill_inpainting_region_cpu(const Result &flooded_boundary,
                                  Result &filled_region,
                                  Result &distance_to_boundary_image,
                                  Result &smoothing_radius_image)
  {
    const int max_distance = this->get_max_distance();

    const Result &input = this->get_input("Image");

    const Domain domain = this->compute_domain();
    filled_region.allocate_texture(domain);
    distance_to_boundary_image.allocate_texture(domain);
    smoothing_radius_image.allocate_texture(domain);

    /* Fill the inpainting region by sampling the color of the nearest boundary pixel.
     * Additionally, compute some information about the inpainting region, like the distance to the
     * boundary, as well as the blur radius to use to smooth out that region. */
    parallel_for(domain.size, [&](const int2 texel) {
      float4 color = float4(input.load_pixel<Color>(texel));

      /* An opaque pixel, not part of the inpainting region. */
      if (color.w == 1.0f) {
        filled_region.store_pixel(texel, Color(color));
        smoothing_radius_image.store_pixel(texel, 0.0f);
        distance_to_boundary_image.store_pixel(texel, 0.0f);
        return;
      }

      int2 closest_boundary_texel = flooded_boundary.load_pixel<int2>(texel);
      float distance_to_boundary = math::distance(float2(texel), float2(closest_boundary_texel));
      distance_to_boundary_image.store_pixel(texel, distance_to_boundary);

      /* We follow this shader by a blur shader that smooths out the inpainting region, where the
       * blur radius is the radius of the circle that touches the boundary. We can imagine the blur
       * window to be inscribed in that circle and thus the blur radius is the distance to the
       * boundary divided by square root two. As a performance optimization, we limit the blurring
       * to areas that will affect the inpainting region, that is, whose distance to boundary is
       * less than double the inpainting distance. Additionally, we clamp to the distance to the
       * inpainting distance since areas outside of the clamp range only indirectly affect the
       * inpainting region due to blurring and thus needn't use higher blur radii. */
      float blur_window_size = math::min(float(max_distance), distance_to_boundary) /
                               math::numbers::sqrt2;
      bool skip_smoothing = distance_to_boundary > (max_distance * 2.0f);
      float smoothing_radius = skip_smoothing ? 0.0f : blur_window_size;
      smoothing_radius_image.store_pixel(texel, smoothing_radius);

      /* Mix the boundary color with the original color using its alpha because semi-transparent
       * areas are considered to be partially inpainted. */
      float4 boundary_color = float4(input.load_pixel<Color>(closest_boundary_texel));
      filled_region.store_pixel(texel, Color(math::interpolate(boundary_color, color, color.w)));
    });
  }

  /* Compute the inpainting region by mixing the smoothed inpainted region with the original input
   * up to the inpainting distance. */
  void compute_inpainting_region(const Result &inpainted_region,
                                 const Result &distance_to_boundary)
  {
    if (this->context().use_gpu()) {
      this->compute_inpainting_region_gpu(inpainted_region, distance_to_boundary);
    }
    else {
      this->compute_inpainting_region_cpu(inpainted_region, distance_to_boundary);
    }
  }

  void compute_inpainting_region_gpu(const Result &inpainted_region,
                                     const Result &distance_to_boundary)
  {
    gpu::Shader *shader = context().get_shader("compositor_inpaint_compute_region");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "max_distance", get_max_distance());

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    inpainted_region.bind_as_texture(shader, "inpainted_region_tx");
    distance_to_boundary.bind_as_texture(shader, "distance_to_boundary_tx");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    inpainted_region.unbind_as_texture();
    distance_to_boundary.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_inpainting_region_cpu(const Result &inpainted_region,
                                     const Result &distance_to_boundary_image)
  {
    const int max_distance = this->get_max_distance();

    const Result &input = this->get_input("Image");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 color = float4(input.load_pixel<Color>(texel));

      /* An opaque pixel, not part of the inpainting region, write the original color. */
      if (color.w == 1.0f) {
        output.store_pixel(texel, Color(color));
        return;
      }

      float distance_to_boundary = distance_to_boundary_image.load_pixel<float>(texel);

      /* Further than the inpainting distance, not part of the inpainting region, write the
       * original color. */
      if (distance_to_boundary > max_distance) {
        output.store_pixel(texel, Color(color));
        return;
      }

      /* Mix the inpainted color with the original color using its alpha because semi-transparent
       * areas are considered to be partially inpainted. */
      float4 inpainted_color = float4(inpainted_region.load_pixel<Color>(texel));
      output.store_pixel(
          texel,
          Color(float4(math::interpolate(inpainted_color.xyz(), color.xyz(), color.w), 1.0f)));
    });
  }

  int get_max_distance()
  {
    return math::max(0, this->get_input("Size").get_single_value_default(0));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new InpaintOperation(context, node);
}

}  // namespace blender::nodes::node_composite_inpaint_cc

static void register_node_type_cmp_inpaint()
{
  namespace file_ns = blender::nodes::node_composite_inpaint_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeInpaint", CMP_NODE_INPAINT);
  ntype.ui_name = "Inpaint";
  ntype.ui_description = "Extend borders of an image into transparent or masked regions";
  ntype.enum_name_legacy = "INPAINT";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_inpaint_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_inpaint)
