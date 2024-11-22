/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_algorithm_recursive_gaussian_blur.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_blur_cc {

NODE_STORAGE_FUNCS(NodeBlurData)

static void cmp_node_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Size")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_blur(bNodeTree * /*ntree*/, bNode *node)
{
  NodeBlurData *data = MEM_cnew<NodeBlurData>(__func__);
  data->filtertype = R_FILTER_GAUSS;
  node->storage = data;
}

static void node_composit_buts_blur(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col, *row;

  col = uiLayoutColumn(layout, false);
  const int filter = RNA_enum_get(ptr, "filter_type");
  const int reference = RNA_boolean_get(ptr, "use_variable_size");

  uiItemR(col, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (filter != R_FILTER_FAST_GAUSS) {
    uiItemR(col, ptr, "use_variable_size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    if (!reference) {
      uiItemR(col, ptr, "use_bokeh", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
    uiItemR(col, ptr, "use_gamma_correction", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }

  uiItemR(col, ptr, "use_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_boolean_get(ptr, "use_relative")) {
    uiItemL(col, IFACE_("Aspect Correction"), ICON_NONE);
    row = uiLayoutRow(layout, true);
    uiItemR(row,
            ptr,
            "aspect_correction",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND,
            nullptr,
            ICON_NONE);

    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "factor_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "factor_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  else {
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "size_x", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("X"), ICON_NONE);
    uiItemR(col, ptr, "size_y", UI_ITEM_R_SPLIT_EMPTY_NAME, IFACE_("Y"), ICON_NONE);
  }
  uiItemR(col, ptr, "use_extended_bounds", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class BlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (node_storage(bnode()).filtertype == R_FILTER_FAST_GAUSS) {
      recursive_gaussian_blur(
          context(), get_input("Image"), get_result("Image"), compute_blur_radius());
    }
    else if (use_variable_size()) {
      execute_variable_size();
    }
    else if (use_separable_filter()) {
      symmetric_separable_blur(context(),
                               get_input("Image"),
                               get_result("Image"),
                               compute_blur_radius(),
                               node_storage(bnode()).filtertype,
                               get_extend_bounds(),
                               node_storage(bnode()).gamma);
    }
    else {
      execute_constant_size();
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
    GPUShader *shader = context().get_shader("compositor_symmetric_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const float2 blur_radius = compute_blur_radius();

    const Result &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), node_storage(bnode()).filtertype, blur_radius);
    weights.bind_as_texture(shader, "weights_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    weights.unbind_as_texture();
  }

  void execute_constant_size_cpu()
  {
    const float2 blur_radius = this->compute_blur_radius();
    const Result &weights = this->context().cache_manager().symmetric_blur_weights.get(
        this->context(), node_storage(this->bnode()).filtertype, blur_radius);

    Domain domain = this->compute_domain();
    const bool extend_bounds = this->get_extend_bounds();
    if (extend_bounds) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const Result &input = this->get_input("Image");
    const bool gamma_correct = node_storage(this->bnode()).gamma;
    auto load_input = [&](const int2 texel) {
      return this->load_input(input, weights, texel, extend_bounds, gamma_correct);
    };

    parallel_for(domain.size, [&](const int2 texel) {
      float4 accumulated_color = float4(0.0f);

      /* First, compute the contribution of the center pixel. */
      float4 center_color = load_input(texel);
      accumulated_color += center_color * weights.load_pixel(int2(0)).x;

      int2 weights_size = weights.domain().size;

      /* Then, compute the contributions of the pixels along the x axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int x = 1; x < weights_size.x; x++) {
        float weight = weights.load_pixel(int2(x, 0)).x;
        accumulated_color += load_input(texel + int2(x, 0)) * weight;
        accumulated_color += load_input(texel + int2(-x, 0)) * weight;
      }

      /* Then, compute the contributions of the pixels along the y axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int y = 1; y < weights_size.y; y++) {
        float weight = weights.load_pixel(int2(0, y)).x;
        accumulated_color += load_input(texel + int2(0, y)) * weight;
        accumulated_color += load_input(texel + int2(0, -y)) * weight;
      }

      /* Finally, compute the contributions of the pixels in the four quadrants of the filter,
       * noting that the weights texture only stores the weights for the upper right quadrant, but
       * since the filter is symmetric, the same weight is used for the rest of the quadrants and
       * we add all four of their contributions. */
      for (int y = 1; y < weights_size.y; y++) {
        for (int x = 1; x < weights_size.x; x++) {
          float weight = weights.load_pixel(int2(x, y)).x;
          accumulated_color += load_input(texel + int2(x, y)) * weight;
          accumulated_color += load_input(texel + int2(-x, y)) * weight;
          accumulated_color += load_input(texel + int2(x, -y)) * weight;
          accumulated_color += load_input(texel + int2(-x, -y)) * weight;
        }
      }

      if (gamma_correct) {
        accumulated_color = this->gamma_uncorrect_blur_output(accumulated_color);
      }

      output.store_pixel(texel, accumulated_color);
    });
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
    GPUShader *shader = context().get_shader("compositor_symmetric_blur_variable_size");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const float2 blur_radius = compute_blur_radius();

    const Result &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), node_storage(bnode()).filtertype, blur_radius);
    weights.bind_as_texture(shader, "weights_tx");

    const Result &input_size = get_input("Size");
    input_size.bind_as_texture(shader, "size_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    weights.unbind_as_texture();
    input_size.unbind_as_texture();
  }

  void execute_variable_size_cpu()
  {
    const float2 blur_radius = this->compute_blur_radius();
    const Result &weights = this->context().cache_manager().symmetric_blur_weights.get(
        this->context(), node_storage(this->bnode()).filtertype, blur_radius);

    Domain domain = this->compute_domain();
    const bool extend_bounds = this->get_extend_bounds();
    if (extend_bounds) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(blur_radius)) * 2;
    }

    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const Result &input = this->get_input("Image");
    const bool gamma_correct = node_storage(this->bnode()).gamma;
    auto load_input = [&](const int2 texel) {
      return this->load_input(input, weights, texel, extend_bounds, gamma_correct);
    };

    const Result &size = get_input("Size");
    /* Similar to load_input but loads the size instead, has no gamma correction, and clamps to
     * borders instead of returning zero for out of bound access. See load_input for more
     * information. */
    auto load_size = [&](const int2 texel) {
      int2 blur_radius = weights.domain().size - 1;
      int2 offset = extend_bounds ? blur_radius : int2(0);
      return math::clamp(size.load_pixel_extended(texel - offset).x, 0.0f, 1.0f);
    };

    parallel_for(domain.size, [&](const int2 texel) {
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);

      /* The weights texture only stores the weights for the first quadrant, but since the weights
       * are symmetric, other quadrants can be found using mirroring. It follows that the base blur
       * radius is the weights texture size minus one, where the one corresponds to the zero
       * weight. */
      int2 weights_size = weights.domain().size;
      int2 base_radius = weights_size - int2(1);
      int2 radius = int2(math::ceil(float2(base_radius) * load_size(texel)));
      float2 coordinates_scale = float2(1.0f) / float2(radius + int2(1));

      /* First, compute the contribution of the center pixel. */
      float4 center_color = load_input(texel);
      float center_weight = weights.load_pixel(int2(0)).x;
      accumulated_color += center_color * center_weight;
      accumulated_weight += center_weight;

      /* Then, compute the contributions of the pixels along the x axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int x = 1; x <= radius.x; x++) {
        float weight_coordinates = (x + 0.5f) * coordinates_scale.x;
        float weight = weights.sample_bilinear_extended(float2(weight_coordinates, 0.0f)).x;
        accumulated_color += load_input(texel + int2(x, 0)) * weight;
        accumulated_color += load_input(texel + int2(-x, 0)) * weight;
        accumulated_weight += weight * 2.0f;
      }

      /* Then, compute the contributions of the pixels along the y axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int y = 1; y <= radius.y; y++) {
        float weight_coordinates = (y + 0.5f) * coordinates_scale.y;
        float weight = weights.sample_bilinear_extended(float2(0.0f, weight_coordinates)).x;
        accumulated_color += load_input(texel + int2(0, y)) * weight;
        accumulated_color += load_input(texel + int2(0, -y)) * weight;
        accumulated_weight += weight * 2.0f;
      }

      /* Finally, compute the contributions of the pixels in the four quadrants of the filter,
       * noting that the weights texture only stores the weights for the upper right quadrant, but
       * since the filter is symmetric, the same weight is used for the rest of the quadrants and
       * we add all four of their contributions. */
      for (int y = 1; y <= radius.y; y++) {
        for (int x = 1; x <= radius.x; x++) {
          float2 weight_coordinates = (float2(x, y) + float2(0.5f)) * coordinates_scale;
          float weight = weights.sample_bilinear_extended(weight_coordinates).x;
          accumulated_color += load_input(texel + int2(x, y)) * weight;
          accumulated_color += load_input(texel + int2(-x, y)) * weight;
          accumulated_color += load_input(texel + int2(x, -y)) * weight;
          accumulated_color += load_input(texel + int2(-x, -y)) * weight;
          accumulated_weight += weight * 4.0f;
        }
      }

      accumulated_color = math::safe_divide(accumulated_color, accumulated_weight);

      if (gamma_correct) {
        accumulated_color = this->gamma_uncorrect_blur_output(accumulated_color);
      }

      output.store_pixel(texel, accumulated_color);
    });
  }

  /* Loads the input color of the pixel at the given texel. If gamma correction is enabled, the
   * color is gamma corrected. If bounds are extended, then the input is treated as padded by a
   * blur size amount of pixels of zero color, and the given texel is assumed to be in the space of
   * the image after padding. So we offset the texel by the blur radius amount and fallback to a
   * zero color if it is out of bounds. For instance, if the input is padded by 5 pixels to the
   * left of the image, the first 5 pixels should be out of bounds and thus zero, hence the
   * introduced offset. */
  float4 load_input(const Result &input,
                    const Result &weights,
                    const int2 texel,
                    const bool extend_bounds,
                    const bool gamma_correct)
  {
    float4 color;
    if (extend_bounds) {
      /* Notice that we subtract 1 because the weights result have an extra center weight, see the
       * SymmetricBlurWeights class for more information. */
      int2 blur_radius = weights.domain().size - 1;
      color = input.load_pixel_fallback(texel - blur_radius, float4(0.0f));
    }
    else {
      color = input.load_pixel_extended(texel);
    }

    if (gamma_correct) {
      color = this->gamma_correct_blur_input(color);
    }

    return color;
  }

  /* Preprocess the input of the blur filter by squaring it in its alpha straight form, assuming
   * the given color is alpha pre-multiplied. */
  float4 gamma_correct_blur_input(const float4 &color)
  {
    float alpha = color.w > 0.0f ? color.w : 1.0f;
    float3 corrected_color = math::square(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
    return float4(corrected_color, color.w);
  }

  /* Postprocess the output of the blur filter by taking its square root it in its alpha straight
   * form, assuming the given color is alpha pre-multiplied. This essential undoes the processing
   * done by the gamma_correct_blur_input function. */
  float4 gamma_uncorrect_blur_output(const float4 &color)
  {
    float alpha = color.w > 0.0f ? color.w : 1.0f;
    float3 uncorrected_color = math::sqrt(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
    return float4(uncorrected_color, color.w);
  }

  float2 compute_blur_radius()
  {
    const float size = math::clamp(get_input("Size").get_float_value_default(1.0f), 0.0f, 1.0f);

    if (!node_storage(bnode()).relative) {
      return float2(node_storage(bnode()).sizex, node_storage(bnode()).sizey) * size;
    }

    int2 image_size = get_input("Image").domain().size;
    switch (node_storage(bnode()).aspect) {
      case CMP_NODE_BLUR_ASPECT_Y:
        image_size.y = image_size.x;
        break;
      case CMP_NODE_BLUR_ASPECT_X:
        image_size.x = image_size.y;
        break;
      default:
        BLI_assert(node_storage(bnode()).aspect == CMP_NODE_BLUR_ASPECT_NONE);
        break;
    }

    return float2(image_size) * get_size_factor() * size;
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    const Result &input = get_input("Image");
    /* Single value inputs can't be blurred and are returned as is. */
    if (input.is_single_value()) {
      return true;
    }

    /* Zero blur radius. The operation does nothing and the input can be passed through. */
    if (compute_blur_radius() == float2(0.0)) {
      return true;
    }

    return false;
  }

  /* The blur node can operate with different filter types, evaluated on the normalized distance to
   * the center of the filter. Some of those filters are separable and can be computed as such. If
   * the bokeh member is disabled in the node, then the filter is always computed as separable even
   * if it is not in fact separable, in which case, the used filter is a cheaper approximation to
   * the actual filter. If the bokeh member is enabled, then the filter is computed as separable if
   * it is in fact separable and as a normal 2D filter otherwise. */
  bool use_separable_filter()
  {
    if (!node_storage(bnode()).bokeh) {
      return true;
    }

    /* Only Gaussian filters are separable. The rest is not. */
    switch (node_storage(bnode()).filtertype) {
      case R_FILTER_GAUSS:
      case R_FILTER_FAST_GAUSS:
        return true;
      default:
        return false;
    }
  }

  bool use_variable_size()
  {
    return get_variable_size() && !get_input("Size").is_single_value() &&
           node_storage(bnode()).filtertype != R_FILTER_FAST_GAUSS;
  }

  float2 get_size_factor()
  {
    return float2(node_storage(bnode()).percentx, node_storage(bnode()).percenty) / 100.0f;
  }

  bool get_extend_bounds()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS;
  }

  bool get_variable_size()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_blur_cc

void register_node_type_cmp_blur()
{
  namespace file_ns = blender::nodes::node_composite_blur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_blur;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_blur;
  blender::bke::node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
