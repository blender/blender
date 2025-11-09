/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "COM_algorithm_pad.hh"
#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_bokehblur_cc {

static void cmp_node_bokehblur_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Color>("Bokeh")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("Size").default_value(0.0f).min(0.0f).structure_type(
      StructureType::Dynamic);
  b.add_input<decl::Float>("Mask").default_value(1.0f).min(0.0f).max(1.0f).structure_type(
      StructureType::Dynamic);
  b.add_input<decl::Bool>("Extend Bounds").default_value(false);
}

using namespace blender::compositor;

class BokehBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    if (this->is_identity()) {
      output.share_data(input);
      return;
    }

    const Result &size = this->get_input("Size");
    if (this->get_extend_bounds()) {
      Result padded_input = this->context().create_result(ResultType::Color);
      Result padded_size = this->context().create_result(ResultType::Float);

      const int2 padding_size = int2(this->compute_extended_boundary_size(size));

      pad(this->context(), input, padded_input, padding_size, PaddingMethod::Zero);
      pad(this->context(), size, padded_size, padding_size, PaddingMethod::Extend);

      this->execute_blur(padded_input, padded_size);
      padded_input.release();
      padded_size.release();
    }
    else {
      this->execute_blur(input, size);
    }
  }

  /* Computes the number of pixels that the image should be extended by if Extend Bounds is
   * enabled. */
  int compute_extended_boundary_size(const Result &size)
  {
    BLI_assert(this->get_extend_bounds());

    /* For constant sized blur, the extension should just be the blur radius. */
    if (size.is_single_value()) {
      return this->get_blur_radius();
    }

    /* For variable sized blur, the extension should be the bokeh search radius. */
    return this->compute_variable_size_search_radius();
  }

  void execute_blur(const Result &input, const Result &size)
  {
    if (size.is_single_value()) {
      this->execute_constant_size(input);
    }
    else {
      this->execute_variable_size(input, size);
    }
  }

  void execute_constant_size(const Result &input)
  {
    if (this->context().use_gpu()) {
      this->execute_constant_size_gpu(input);
    }
    else {
      this->execute_constant_size_cpu(input);
    }
  }

  void execute_constant_size_gpu(const Result &input)
  {
    gpu::Shader *shader = context().get_shader("compositor_bokeh_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", this->get_blur_radius());

    input.bind_as_texture(shader, "input_tx");

    const Result &input_weights = this->get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    const Result &input_mask = this->get_input("Mask");
    input_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = input.domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input.unbind_as_texture();
    input_weights.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  void execute_constant_size_cpu(const Result &input)
  {
    const int radius = this->get_blur_radius();

    const Result &mask_image = this->get_input("Mask");

    const Domain domain = input.domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    Result blur_kernel = this->compute_blur_kernel(radius);

    parallel_for(domain.size, [&](const int2 texel) {
      /* The mask input is treated as a boolean. If it is zero, then no blurring happens for this
       * pixel. Otherwise, the pixel is blurred normally and the mask value is irrelevant. */
      float mask = mask_image.load_pixel<float, true>(texel);
      if (mask == 0.0f) {
        output.store_pixel(texel, input.load_pixel<Color>(texel));
        return;
      }

      /* Go over the window of the given radius and accumulate the colors multiplied by their
       * respective weights as well as the weights themselves. */
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          float4 weight = float4(blur_kernel.load_pixel<Color>(int2(x, y) + radius));
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, y))) *
                               weight;
          accumulated_weight += weight;
        }
      }

      output.store_pixel(texel, Color(math::safe_divide(accumulated_color, accumulated_weight)));
    });

    blur_kernel.release();
  }

  void execute_variable_size(const Result &input, const Result &size)
  {
    if (this->context().use_gpu()) {
      this->execute_variable_size_gpu(input, size);
    }
    else {
      this->execute_variable_size_cpu(input, size);
    }
  }

  void execute_variable_size_gpu(const Result &input, const Result &size)
  {
    const int search_radius = this->compute_variable_size_search_radius();

    gpu::Shader *shader = this->context().get_shader("compositor_bokeh_blur_variable_size");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "search_radius", search_radius);

    input.bind_as_texture(shader, "input_tx");

    const Result &input_weights = this->get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    size.bind_as_texture(shader, "size_tx");

    const Result &input_mask = this->get_input("Mask");
    input_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = input.domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input.unbind_as_texture();
    input_weights.unbind_as_texture();
    size.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  void execute_variable_size_cpu(const Result &input, const Result &size_input)
  {
    const int search_radius = this->compute_variable_size_search_radius();

    const Result &weights = this->get_input("Bokeh");
    const Result &mask_image = this->get_input("Mask");

    const Domain domain = input.domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    /* Given the texel in the range [-radius, radius] in both axis, load the appropriate weight
     * from the weights image, where the given texel (0, 0) corresponds the center of weights
     * image. Note that we load the weights image inverted along both directions to maintain
     * the shape of the weights if it was not symmetrical. To understand why inversion makes sense,
     * consider a 1D weights image whose right half is all ones and whose left half is all zeros.
     * Further, consider that we are blurring a single white pixel on a black background. When
     * computing the value of a pixel that is to the right of the white pixel, the white pixel will
     * be in the left region of the search window, and consequently, without inversion, a zero will
     * be sampled from the left side of the weights image and result will be zero. However, what
     * we expect is that pixels to the right of the white pixel will be white, that is, they should
     * sample a weight of 1 from the right side of the weights image, hence the need for
     * inversion. */
    auto load_weight = [&](const int2 &texel, const float radius) {
      /* The center zero texel is always assigned a unit weight regardless of the corresponding
       * weight in the weights image. That's to guarantee that at last the center pixel will be
       * accumulated even if the weights image is zero at its center. */
      if (texel.x == 0 && texel.y == 0) {
        return float4(1.0f);
      }

      /* Add the radius to transform the texel into the range [0, radius * 2], with an additional
       * 0.5 to sample at the center of the pixels, then divide by the upper bound plus one to
       * transform the texel into the normalized range [0, 1] needed to sample the weights sampler.
       * Finally, invert the textures coordinates by subtracting from 1 to maintain the shape of
       * the weights as mentioned in the function description. */
      return weights.sample_bilinear_extended(
          1.0f - ((float2(texel) + float2(radius + 0.5f)) / (radius * 2.0f + 1.0f)));
    };

    parallel_for(domain.size, [&](const int2 texel) {
      /* The mask input is treated as a boolean. If it is zero, then no blurring happens for this
       * pixel. Otherwise, the pixel is blurred normally and the mask value is irrelevant. */
      float mask = mask_image.load_pixel<float, true>(texel);
      if (mask == 0.0f) {
        output.store_pixel(texel, input.load_pixel<Color>(texel));
        return;
      }

      float center_size = math::max(0.0f, size_input.load_pixel<float>(texel));

      /* Go over the window of the given search radius and accumulate the colors multiplied by
       * their respective weights as well as the weights themselves, but only if both the size of
       * the center pixel and the size of the candidate pixel are less than both the x and y
       * distances of the candidate pixel. */
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);
      for (int y = -search_radius; y <= search_radius; y++) {
        for (int x = -search_radius; x <= search_radius; x++) {
          float candidate_size = math::max(
              0.0f, size_input.load_pixel_extended<float>(texel + int2(x, y)));

          /* Skip accumulation if either the x or y distances of the candidate pixel are larger
           * than either the center or candidate pixel size. Note that the max and min functions
           * here denote "either" in the aforementioned description. */
          float size = math::min(center_size, candidate_size);
          if (math::max(math::abs(x), math::abs(y)) > size) {
            continue;
          }

          float4 weight = load_weight(int2(x, y), size);
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, y))) *
                               weight;
          accumulated_weight += weight;
        }
      }

      output.store_pixel(texel, Color(math::safe_divide(accumulated_color, accumulated_weight)));
    });
  }

  /* Compute a blur kernel from the bokeh result by interpolating it to the size of the kernel.
   * Note that we load the bokeh result inverted along both directions to maintain the shape of the
   * weights if it was not symmetrical. To understand why inversion makes sense, consider a 1D
   * weights image whose right half is all ones and whose left half is all zeros. Further, consider
   * that we are blurring a single white pixel on a black background. When computing the value of a
   * pixel that is to the right of the white pixel, the white pixel will be in the left region of
   * the search window, and consequently, without inversion, a zero will be sampled from the left
   * side of the weights image and result will be zero. However, what we expect is that pixels to
   * the right of the white pixel will be white, that is, they should sample a weight of 1 from the
   * right side of the weights image, hence the need for inversion. */
  Result compute_blur_kernel(const int radius)
  {
    const Result &bokeh = this->get_input("Bokeh");

    Result kernel = this->context().create_result(ResultType::Color);
    const int2 kernel_size = int2(radius * 2 + 1);
    kernel.allocate_texture(kernel_size);
    parallel_for(kernel_size, [&](const int2 texel) {
      /* Add 0.5 to sample at the center of the pixels, then divide by the kernel size to transform
       * the texel into the normalized range [0, 1] needed to sample the bokeh result. Finally,
       * invert the textures coordinates by subtracting from 1 to maintain the shape of the weights
       * as mentioned above. */
      const float2 weight_coordinates = 1.0f - ((float2(texel) + 0.5f) / float2(kernel_size));
      float4 weight = bokeh.sample_bilinear_extended(weight_coordinates);
      kernel.store_pixel(texel, Color(weight));
    });

    return kernel;
  }

  int compute_variable_size_search_radius()
  {
    return math::max(0, int(maximum_float(context(), this->get_input("Size"))));
  }

  int get_blur_radius()
  {
    return math::max(0, int(this->get_input("Size").get_single_value<float>()));
  }

  bool is_identity()
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      return true;
    }

    const Result &size = this->get_input("Size");
    if (size.is_single_value() && this->get_blur_radius() == 0) {
      return true;
    }

    const Result &mask = this->get_input("Mask");
    if (mask.is_single_value() && mask.get_single_value<float>() == 0.0) {
      return true;
    }

    return false;
  }

  bool get_extend_bounds()
  {
    return this->get_input("Extend Bounds").get_single_value_default(false);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehblur_cc

static void register_node_type_cmp_bokehblur()
{
  namespace file_ns = blender::nodes::node_composite_bokehblur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeBokehBlur", CMP_NODE_BOKEHBLUR);
  ntype.ui_name = "Bokeh Blur";
  ntype.ui_description =
      "Generate a bokeh type blur similar to Defocus. Unlike defocus an in-focus region is "
      "defined in the compositor";
  ntype.enum_name_legacy = "BOKEHBLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_bokehblur_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 160, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_bokehblur)
