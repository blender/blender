/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Map UV  ******************** */

namespace blender::nodes::node_composite_map_uv_cc {

static void cmp_node_map_uv_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_options(CompositorInputRealizationOptions::None);
  b.add_input<decl::Vector>("UV")
      .default_value({1.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_map_uv(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "alpha", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static void node_composit_init_map_uv(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom2 = CMP_NODE_MAP_UV_FILTERING_ANISOTROPIC;
}

using namespace blender::realtime_compositor;

class MapUVOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (get_input("Image").is_single_value()) {
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
    GPUShader *shader = context().get_shader(get_shader_name());
    GPU_shader_bind(shader);

    const bool nearest_neighbour = get_nearest_neighbour();
    if (!nearest_neighbour) {
      GPU_shader_uniform_1f(
          shader, "gradient_attenuation_factor", get_gradient_attenuation_factor());
    }

    const Result &input_image = get_input("Image");
    if (nearest_neighbour) {
      GPU_texture_mipmap_mode(input_image, false, false);
      GPU_texture_anisotropic_filter(input_image, false);
    }
    else {
      GPU_texture_mipmap_mode(input_image, true, true);
      GPU_texture_anisotropic_filter(input_image, true);
    }

    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_uv = get_input("UV");
    input_uv.bind_as_texture(shader, "uv_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_uv.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  char const *get_shader_name()
  {
    return get_nearest_neighbour() ? "compositor_map_uv_nearest_neighbour" :
                                     "compositor_map_uv_anisotropic";
  }

  void execute_cpu()
  {
    if (this->get_nearest_neighbour()) {
      this->execute_cpu_nearest();
    }
    else {
      this->execute_cpu_anisotropic();
    }
  }

  void execute_cpu_nearest()
  {
    const Result &input_image = get_input("Image");
    const Result &input_uv = get_input("UV");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float2 uv_coordinates = input_uv.load_pixel(texel).xy();

      float4 sampled_color = input_image.sample_nearest_zero(uv_coordinates);

      /* The UV texture is assumed to contain an alpha channel as its third channel, since the
       * UV coordinates might be defined in only a subset area of the UV texture as mentioned.
       * In that case, the alpha is typically opaque at the subset area and transparent
       * everywhere else, and alpha pre-multiplication is then performed. This format of having
       * an alpha channel in the UV coordinates is the format used by UV passes in render
       * engines, hence the mentioned logic. */
      float alpha = input_uv.load_pixel(texel).z;

      float4 result = sampled_color * alpha;

      output_image.store_pixel(texel, result);
    });
  }

  void execute_cpu_anisotropic()
  {
    const Result &input_image = get_input("Image");
    const Result &input_uv = get_input("UV");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    const float gradient_attenuation_factor = this->get_gradient_attenuation_factor();

    /* In order to perform EWA sampling, we need to compute the partial derivative of the UV
     * coordinates along the x and y directions using a finite difference approximation. But in
     * order to avoid loading multiple neighboring UV coordinates for each pixel, we operate on
     * the image in 2x2 blocks of pixels, where the derivatives are computed horizontally and
     * vertically across the 2x2 block such that odd texels use a forward finite difference
     * equation while even invocations use a backward finite difference equation. */
    const int2 size = domain.size;
    const int2 uv_size = input_uv.domain().size;
    parallel_for(math::divide_ceil(size, int2(2)), [&](const int2 base_texel) {
      const int x = base_texel.x * 2;
      const int y = base_texel.y * 2;

      const int2 lower_left_texel = int2(x, y);
      const int2 lower_right_texel = int2(x + 1, y);
      const int2 upper_left_texel = int2(x, y + 1);
      const int2 upper_right_texel = int2(x + 1, y + 1);

      const float2 lower_left_uv = input_uv.load_pixel(lower_left_texel).xy();
      const float2 lower_right_uv = input_uv.load_pixel_extended(lower_right_texel).xy();
      const float2 upper_left_uv = input_uv.load_pixel_extended(upper_left_texel).xy();
      const float2 upper_right_uv = input_uv.load_pixel_extended(upper_right_texel).xy();

      /* Compute the partial derivatives using finite difference. Divide by the input size since
       * sample_ewa_zero assumes derivatives with respect to texel coordinates. */
      const float2 lower_x_gradient = (lower_right_uv - lower_left_uv) / uv_size.x;
      const float2 left_y_gradient = (upper_left_uv - lower_left_uv) / uv_size.y;
      const float2 right_y_gradient = (upper_right_uv - lower_right_uv) / uv_size.y;
      const float2 upper_x_gradient = (upper_right_uv - upper_left_uv) / uv_size.x;

      /* Computes one of the 2x2 pixels given its texel location, coordinates, and gradients. */
      auto compute_pixel = [&](const int2 &texel,
                               const float2 &coordinates,
                               const float2 &x_gradient,
                               const float2 &y_gradient) {
        /* Sample the input using the UV coordinates passing in the computed gradients in order
         * to utilize the anisotropic filtering capabilities of the sampler. */
        float4 sampled_color = input_image.sample_ewa_zero(coordinates, x_gradient, y_gradient);

        /* The UV coordinates might be defined in only a subset area of the UV textures, in which
         * case, the gradients would be infinite at the boundary of that area, which would
         * produce erroneous results due to anisotropic filtering. To workaround this, we
         * attenuate the result if its computed gradients are too high such that the result tends
         * to zero when the magnitude of the gradients tends to one, that is when their sum tends
         * to 2. One is chosen as the threshold because that's the maximum gradient magnitude
         * when the boundary is the maximum sampler value of one and the out of bound values are
         * zero. Additionally, the user supplied gradient attenuation factor can be used to
         * control this attenuation or even disable it when it is zero, ranging between zero and
         * one. */
        float gradient_magnitude = (math::length(x_gradient) + math::length(y_gradient)) / 2.0f;
        float gradient_attenuation = math::max(
            0.0f, 1.0f - gradient_attenuation_factor * gradient_magnitude);

        /* The UV texture is assumed to contain an alpha channel as its third channel, since the
         * UV coordinates might be defined in only a subset area of the UV texture as mentioned.
         * In that case, the alpha is typically opaque at the subset area and transparent
         * everywhere else, and alpha pre-multiplication is then performed. This format of having
         * an alpha channel in the UV coordinates is the format used by UV passes in render
         * engines, hence the mentioned logic. */
        float alpha = input_uv.load_pixel(texel).z;

        float4 result = sampled_color * gradient_attenuation * alpha;

        output_image.store_pixel(texel, result);
      };

      /* Compute each of the pixels in the 2x2 block, making sure to exempt out of bounds right
       * and upper pixels. */
      compute_pixel(lower_left_texel, lower_left_uv, lower_x_gradient, left_y_gradient);
      if (lower_right_texel.x != size.x) {
        compute_pixel(lower_right_texel, lower_right_uv, lower_x_gradient, right_y_gradient);
      }
      if (upper_left_texel.y != size.y) {
        compute_pixel(upper_left_texel, upper_left_uv, upper_x_gradient, left_y_gradient);
      }
      if (upper_right_texel.x != size.x && upper_right_texel.y != size.y) {
        compute_pixel(upper_right_texel, upper_right_uv, upper_x_gradient, right_y_gradient);
      }
    });
  }

  /* A factor that controls the attenuation of the result at the pixels where the gradients of
   * the UV texture are too high, see the shader for more information. The factor ranges between
   * zero and one, where it has no effect when it is zero and performs full attenuation when it
   * is 1. */
  float get_gradient_attenuation_factor()
  {
    return bnode().custom1 / 100.0f;
  }

  bool get_nearest_neighbour()
  {
    return bnode().custom2 == CMP_NODE_MAP_UV_FILTERING_NEAREST;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MapUVOperation(context, node);
}

}  // namespace blender::nodes::node_composite_map_uv_cc

void register_node_type_cmp_mapuv()
{
  namespace file_ns = blender::nodes::node_composite_map_uv_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MAP_UV, "Map UV", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_map_uv_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_map_uv;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc = file_ns::node_composit_init_map_uv;

  blender::bke::node_register_type(&ntype);
}
