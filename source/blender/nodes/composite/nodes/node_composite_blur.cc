/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include <cstdint>

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_math_vector.hh"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_pipeline.h"

#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** BLUR ******************** */

namespace blender::nodes::node_composite_blur_cc {

NODE_STORAGE_FUNCS(NodeBlurData)

static void cmp_node_blur_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Size")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_blur(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeBlurData *data = MEM_cnew<NodeBlurData>(__func__);
  data->filtertype = R_FILTER_GAUSS;
  node->storage = data;
}

static void node_composit_buts_blur(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
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

/* A helper class that computes and caches a 1D GPU texture containing the weights of the separable
 * filter of the given type and radius. The filter is assumed to be symmetric, because the filter
 * functions are all even functions. Consequently, only the positive half of the filter is computed
 * and the shader takes that into consideration. */
class SymmetricSeparableBlurWeights {
 private:
  float radius_ = 1.0f;
  int type_ = R_FILTER_GAUSS;
  GPUTexture *texture_ = nullptr;

 public:
  ~SymmetricSeparableBlurWeights()
  {
    if (texture_) {
      GPU_texture_free(texture_);
    }
  }

  /* Check if a texture containing the weights was already computed for the given filter type and
   * radius. If such texture exists, do nothing, otherwise, free the already computed texture and
   * recompute it with the given filter type and radius. */
  void update(float radius, int type)
  {
    if (texture_ && type == type_ && radius == radius_) {
      return;
    }

    if (texture_) {
      GPU_texture_free(texture_);
    }

    /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
     * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
     * odd and there is a center weight. */
    const int size = math::ceil(radius) + 1;
    Array<float> weights(size);

    float sum = 0.0f;

    /* First, compute the center weight. */
    const float center_weight = RE_filter_value(type, 0.0f);
    weights[0] = center_weight;
    sum += center_weight;

    /* Second, compute the other weights in the positive direction, making sure to add double the
     * weight to the sum of weights because the filter is symmetric and we only loop over half of
     * it. Skip the center weight already computed by dropping the front index. */
    const float scale = radius > 0.0f ? 1.0f / radius : 0.0f;
    for (const int i : weights.index_range().drop_front(1)) {
      const float weight = RE_filter_value(type, i * scale);
      weights[i] = weight;
      sum += weight * 2.0f;
    }

    /* Finally, normalize the weights. */
    for (const int i : weights.index_range()) {
      weights[i] /= sum;
    }

    texture_ = GPU_texture_create_1d("Weights", size, 1, GPU_R16F, weights.data());

    type_ = type;
    radius_ = radius;
  }

  void bind_as_texture(GPUShader *shader, const char *texture_name)
  {
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, texture_name);
    GPU_texture_bind(texture_, texture_image_unit);
  }

  void unbind_as_texture()
  {
    GPU_texture_unbind(texture_);
  }
};

/* A helper class that computes and caches a 2D GPU texture containing the weights of the filter of
 * the given type and radius. The filter is assumed to be symmetric, because the filter functions
 * are evaluated on the normalized distance to the center. Consequently, only the upper right
 * quadrant are computed and the shader takes that into consideration. */
class SymmetricBlurWeights {
 private:
  int type_ = R_FILTER_GAUSS;
  float2 radius_ = float2(1.0f);
  GPUTexture *texture_ = nullptr;

 public:
  ~SymmetricBlurWeights()
  {
    if (texture_) {
      GPU_texture_free(texture_);
    }
  }

  /* Check if a texture containing the weights was already computed for the given filter type and
   * radius. If such texture exists, do nothing, otherwise, free the already computed texture and
   * recompute it with the given filter type and radius. */
  void update(float2 radius, int type)
  {
    if (texture_ && type == type_ && radius == radius_) {
      return;
    }

    if (texture_) {
      GPU_texture_free(texture_);
    }

    /* The full size of filter is double the radius plus 1, but since the filter is symmetric, we
     * only compute a single quadrant of it and so no doubling happens. We add 1 to make sure the
     * filter size is always odd and there is a center weight. */
    const float2 scale = math::safe_divide(float2(1.0f), radius);
    const int2 size = int2(math::ceil(radius)) + int2(1);
    Array<float> weights(size.x * size.y);

    float sum = 0.0f;

    /* First, compute the center weight. */
    const float center_weight = RE_filter_value(type, 0.0f);
    weights[0] = center_weight;
    sum += center_weight;

    /* Then, compute the weights along the positive x axis, making sure to add double the weight to
     * the sum of weights because the filter is symmetric and we only loop over the positive half
     * of the x axis. Skip the center weight already computed by dropping the front index. */
    for (const int x : IndexRange(size.x).drop_front(1)) {
      const float weight = RE_filter_value(type, x * scale.x);
      weights[x] = weight;
      sum += weight * 2.0f;
    }

    /* Then, compute the weights along the positive y axis, making sure to add double the weight to
     * the sum of weights because the filter is symmetric and we only loop over the positive half
     * of the y axis. Skip the center weight already computed by dropping the front index. */
    for (const int y : IndexRange(size.y).drop_front(1)) {
      const float weight = RE_filter_value(type, y * scale.y);
      weights[size.x * y] = weight;
      sum += weight * 2.0f;
    }

    /* Then, compute the other weights in the upper right quadrant, making sure to add quadruple
     * the weight to the sum of weights because the filter is symmetric and we only loop over one
     * quadrant of it. Skip the weights along the y and x axis already computed by dropping the
     * front index. */
    for (const int y : IndexRange(size.y).drop_front(1)) {
      for (const int x : IndexRange(size.x).drop_front(1)) {
        const float weight = RE_filter_value(type, math::length(float2(x, y) * scale));
        weights[size.x * y + x] = weight;
        sum += weight * 4.0f;
      }
    }

    /* Finally, normalize the weights. */
    for (const int y : IndexRange(size.y)) {
      for (const int x : IndexRange(size.x)) {
        weights[size.x * y + x] /= sum;
      }
    }

    texture_ = GPU_texture_create_2d("Weights", size.x, size.y, 1, GPU_R16F, weights.data());

    type_ = type;
    radius_ = radius;
  }

  void bind_as_texture(GPUShader *shader, const char *texture_name)
  {
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, texture_name);
    GPU_texture_bind(texture_, texture_image_unit);
  }

  void unbind_as_texture()
  {
    GPU_texture_unbind(texture_);
  }
};

class BlurOperation : public NodeOperation {
 private:
  /* Cached symmetric blur weights. */
  SymmetricBlurWeights blur_weights_;
  /* Cached symmetric blur weights for the separable horizontal pass. */
  SymmetricSeparableBlurWeights blur_horizontal_weights_;
  /* Cached symmetric blur weights for the separable vertical pass. */
  SymmetricSeparableBlurWeights blur_vertical_weights_;

 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (use_separable_filter()) {
      GPUTexture *horizontal_pass_result = execute_separable_blur_horizontal_pass();
      execute_separable_blur_vertical_pass(horizontal_pass_result);
    }
    else {
      execute_blur();
    }
  }

  void execute_blur()
  {
    GPUShader *shader = shader_manager().get("compositor_symmetric_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct", node_storage(bnode()).gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    blur_weights_.update(compute_blur_radius(), node_storage(bnode()).filtertype);
    blur_weights_.bind_as_texture(shader, "weights_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(compute_blur_radius())) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    blur_weights_.unbind_as_texture();
  }

  GPUTexture *execute_separable_blur_horizontal_pass()
  {
    GPUShader *shader = shader_manager().get("compositor_symmetric_separable_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct_input", node_storage(bnode()).gamma);
    GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", false);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    blur_horizontal_weights_.update(compute_blur_radius().x, node_storage(bnode()).filtertype);
    blur_horizontal_weights_.bind_as_texture(shader, "weights_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      domain.size.x += static_cast<int>(math::ceil(compute_blur_radius().x)) * 2;
    }

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will blur the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal blur shader, but since its input is
     * transposed, it will effectively do a vertical blur and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each blur
     * pass. */
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    GPUTexture *horizontal_pass_result = texture_pool().acquire_color(transposed_domain);
    const int image_unit = GPU_shader_get_texture_binding(shader, "output_img");
    GPU_texture_image_bind(horizontal_pass_result, image_unit);

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    blur_horizontal_weights_.unbind_as_texture();
    GPU_texture_image_unbind(horizontal_pass_result);

    return horizontal_pass_result;
  }

  void execute_separable_blur_vertical_pass(GPUTexture *horizontal_pass_result)
  {
    GPUShader *shader = shader_manager().get("compositor_symmetric_separable_blur");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "extend_bounds", get_extend_bounds());
    GPU_shader_uniform_1b(shader, "gamma_correct_input", false);
    GPU_shader_uniform_1b(shader, "gamma_uncorrect_output", node_storage(bnode()).gamma);

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, "input_tx");
    GPU_texture_bind(horizontal_pass_result, texture_image_unit);

    blur_vertical_weights_.update(compute_blur_radius().y, node_storage(bnode()).filtertype);
    blur_vertical_weights_.bind_as_texture(shader, "weights_tx");

    Domain domain = compute_domain();
    if (get_extend_bounds()) {
      /* Add a radius amount of pixels in both sides of the image, hence the multiply by 2. */
      domain.size += int2(math::ceil(compute_blur_radius())) * 2;
    }

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

    GPU_shader_unbind();
    output_image.unbind_as_image();
    blur_vertical_weights_.unbind_as_texture();
    GPU_texture_unbind(horizontal_pass_result);
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

    /* Both Box and Gaussian filters are separable. The rest is not. */
    switch (node_storage(bnode()).filtertype) {
      case R_FILTER_BOX:
      case R_FILTER_GAUSS:
      case R_FILTER_FAST_GAUSS:
        return true;
      default:
        return false;
    }
  }

  float2 get_size_factor()
  {
    return float2(node_storage(bnode()).percentx, node_storage(bnode()).percenty) / 100.0f;
  }

  bool get_extend_bounds()
  {
    return bnode().custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS;
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

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_BLUR, "Blur", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_blur_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_blur;
  ntype.flag |= NODE_PREVIEW;
  node_type_init(&ntype, file_ns::node_composit_init_blur);
  node_type_storage(
      &ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
