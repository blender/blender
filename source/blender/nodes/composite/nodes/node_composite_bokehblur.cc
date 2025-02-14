/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_bokehblur_cc {

static void cmp_node_bokehblur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Bokeh")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms);
  b.add_input<decl::Float>("Size")
      .default_value(1.0f)
      .min(0.0f)
      .max(10.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Bounding box")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(2);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_bokehblur(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom3 = 4.0f;
  node->custom4 = 16.0f;
}

static void node_composit_buts_bokehblur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  // uiItemR(layout, ptr, "f_stop", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE); /* UNUSED
  // */
  uiItemR(layout, ptr, "blur_max", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class BokehBlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (get_input("Size").is_single_value() || !get_variable_size()) {
      execute_constant_size();
    }
    else {
      execute_variable_size();
    }
  }

  void execute_constant_size()
  {
    if (this->context().use_gpu()) {
      this->execute_constant_size_gpu();
    }
    else {
      this->execute_constant_size_cpu();
    }
  }

  void execute_constant_size_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_bokeh_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", int(compute_blur_radius()));
    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_weights = get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    const Result &input_mask = get_input("Bounding box");
    input_mask.bind_as_texture(shader, "mask_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(int(compute_blur_radius()) * 2);
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    input_weights.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  void execute_constant_size_cpu()
  {
    const int radius = int(this->compute_blur_radius());
    const bool extend_bounds = this->get_extend_bounds();

    const Result &input = this->get_input("Image");
    const Result &mask_image = this->get_input("Bounding box");

    Domain domain = this->compute_domain();
    if (extend_bounds) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(int(this->compute_blur_radius()) * 2);
    }

    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    Result blur_kernel = this->compute_blur_kernel(radius);

    auto load_input = [&](const int2 texel) {
      /* If bounds are extended, then we treat the input as padded by a radius amount of pixels.
       * So we load the input with an offset by the radius amount and fallback to a transparent
       * color if it is out of bounds. */
      if (extend_bounds) {
        return input.load_pixel_zero<float4>(texel - radius);
      }
      return input.load_pixel_extended<float4>(texel);
    };

    parallel_for(domain.size, [&](const int2 texel) {
      /* The mask input is treated as a boolean. If it is zero, then no blurring happens for this
       * pixel. Otherwise, the pixel is blurred normally and the mask value is irrelevant. */
      float mask = mask_image.load_pixel<float, true>(texel);
      if (mask == 0.0f) {
        output.store_pixel(texel, input.load_pixel<float4>(texel));
        return;
      }

      /* Go over the window of the given radius and accumulate the colors multiplied by their
       * respective weights as well as the weights themselves. */
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          float4 weight = blur_kernel.load_pixel<float4>(int2(x, y) + radius);
          accumulated_color += load_input(texel + int2(x, y)) * weight;
          accumulated_weight += weight;
        }
      }

      output.store_pixel(texel, math::safe_divide(accumulated_color, accumulated_weight));
    });

    blur_kernel.release();
  }

  void execute_variable_size()
  {
    if (this->context().use_gpu()) {
      this->execute_variable_size_gpu();
    }
    else {
      this->execute_variable_size_cpu();
    }
  }

  void execute_variable_size_gpu()
  {
    const int search_radius = compute_variable_size_search_radius();

    GPUShader *shader = context().get_shader("compositor_bokeh_blur_variable_size");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "base_size", compute_blur_radius());
    GPU_shader_uniform_1i(shader, "search_radius", search_radius);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_weights = get_input("Bokeh");
    input_weights.bind_as_texture(shader, "weights_tx");

    const Result &input_size = get_input("Size");
    input_size.bind_as_texture(shader, "size_tx");

    const Result &input_mask = get_input("Bounding box");
    input_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    input_weights.unbind_as_texture();
    input_size.unbind_as_texture();
    input_mask.unbind_as_texture();
  }

  void execute_variable_size_cpu()
  {
    const float base_size = this->compute_blur_radius();
    const int search_radius = this->compute_variable_size_search_radius();

    const Result &input = get_input("Image");
    const Result &weights = get_input("Bokeh");
    const Result &size_image = get_input("Size");
    const Result &mask_image = get_input("Bounding box");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
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
        output.store_pixel(texel, input.load_pixel<float4>(texel));
        return;
      }

      float center_size = math::max(0.0f, size_image.load_pixel<float>(texel) * base_size);

      /* Go over the window of the given search radius and accumulate the colors multiplied by
       * their respective weights as well as the weights themselves, but only if both the size of
       * the center pixel and the size of the candidate pixel are less than both the x and y
       * distances of the candidate pixel. */
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);
      for (int y = -search_radius; y <= search_radius; y++) {
        for (int x = -search_radius; x <= search_radius; x++) {
          float candidate_size = math::max(
              0.0f, size_image.load_pixel_extended<float>(texel + int2(x, y)) * base_size);

          /* Skip accumulation if either the x or y distances of the candidate pixel are larger
           * than either the center or candidate pixel size. Note that the max and min functions
           * here denote "either" in the aforementioned description. */
          float size = math::min(center_size, candidate_size);
          if (math::max(math::abs(x), math::abs(y)) > size) {
            continue;
          }

          float4 weight = load_weight(int2(x, y), size);
          accumulated_color += input.load_pixel_extended<float4>(texel + int2(x, y)) * weight;
          accumulated_weight += weight;
        }
      }

      output.store_pixel(texel, math::safe_divide(accumulated_color, accumulated_weight));
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

    Result kernel = context().create_result(ResultType::Color);
    const int2 kernel_size = int2(radius * 2 + 1);
    kernel.allocate_texture(kernel_size);
    parallel_for(kernel_size, [&](const int2 texel) {
      /* Add 0.5 to sample at the center of the pixels, then divide by the kernel size to transform
       * the texel into the normalized range [0, 1] needed to sample the bokeh result. Finally,
       * invert the textures coordinates by subtracting from 1 to maintain the shape of the weights
       * as mentioned above. */
      const float2 weight_coordinates = 1.0f - ((float2(texel) + 0.5f) / float2(kernel_size));
      float4 weight = bokeh.sample_bilinear_extended(weight_coordinates);
      kernel.store_pixel(texel, weight);
    });

    return kernel;
  }

  int compute_variable_size_search_radius()
  {
    const Result &input_size = get_input("Size");
    const float maximum_size = maximum_float(context(), input_size);

    const float base_size = compute_blur_radius();
    return math::clamp(int(maximum_size * base_size), 0, get_max_size());
  }

  float compute_blur_radius()
  {
    const int2 image_size = get_input("Image").domain().size;
    const int max_size = math::max(image_size.x, image_size.y);

    /* The [0, 10] range of the size is arbitrary and is merely in place to avoid very long
     * computations of the bokeh blur. */
    const float size = math::clamp(get_input("Size").get_single_value_default(1.0f), 0.0f, 10.0f);

    /* The 100 divisor is arbitrary and was chosen using visual judgment. */
    return size * (max_size / 100.0f);
  }

  bool is_identity()
  {
    const Result &input = get_input("Image");
    if (input.is_single_value()) {
      return true;
    }

    if (compute_blur_radius() == 0.0f) {
      return true;
    }

    /* This input is, in fact, a boolean mask. If it is zero, no blurring will take place.
     * Otherwise, the blurring will take place ignoring the value of the input entirely. */
    const Result &bounding_box = get_input("Bounding box");
    if (bounding_box.is_single_value() && bounding_box.get_single_value<float>() == 0.0) {
      return true;
    }

    return false;
  }

  bool get_extend_bounds()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS;
  }

  bool get_variable_size()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE;
  }

  int get_max_size()
  {
    return int(bnode().custom4);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehBlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehblur_cc

void register_node_type_cmp_bokehblur()
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
  ntype.draw_buttons = file_ns::node_composit_buts_bokehblur;
  ntype.initfunc = file_ns::node_composit_init_bokehblur;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
