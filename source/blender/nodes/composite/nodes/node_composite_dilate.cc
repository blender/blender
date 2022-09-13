/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_math_base.hh"

#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RE_pipeline.h"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Dilate/Erode ******************** */

namespace blender::nodes::node_composite_dilate_cc {

NODE_STORAGE_FUNCS(NodeDilateErode)

static void cmp_node_dilate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Mask")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Mask"));
}

static void node_composit_init_dilateerode(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeDilateErode *data = MEM_cnew<NodeDilateErode>(__func__);
  data->falloff = PROP_SMOOTH;
  node->storage = data;
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  switch (RNA_enum_get(ptr, "mode")) {
    case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
      uiItemR(layout, ptr, "edge", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
    case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
      uiItemR(layout, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
  }
}

using namespace blender::realtime_compositor;

/* Computes a falloff that is equal to 1 at an input of zero and decrease to zero at an input of 1,
 * with the rate of decrease depending on the falloff type. */
static float compute_distance_falloff(float x, int falloff_type)
{
  x = 1.0f - x;

  switch (falloff_type) {
    case PROP_SMOOTH:
      return 3.0f * x * x - 2.0f * x * x * x;
    case PROP_SPHERE:
      return std::sqrt(2.0f * x - x * x);
    case PROP_ROOT:
      return std::sqrt(x);
    case PROP_SHARP:
      return x * x;
    case PROP_INVSQUARE:
      return x * (2.0f - x);
    case PROP_LIN:
      return x;
    default:
      BLI_assert_unreachable();
      return x;
  }
}

/* A helper class that computes and caches 1D GPU textures containing the weights of the separable
 * Gaussian filter of the given radius as well as an inverse distance falloff of the given type and
 * radius. The weights and falloffs are symmetric, because the Gaussian and falloff functions are
 * all even functions. Consequently, only the positive half of the filter is computed and the
 * shader takes that into consideration. */
class SymmetricSeparableMorphologicalDistanceFeatherWeights {
 private:
  int radius_ = 1;
  int falloff_type_ = PROP_SMOOTH;
  GPUTexture *weights_texture_ = nullptr;
  GPUTexture *distance_falloffs_texture_ = nullptr;

 public:
  ~SymmetricSeparableMorphologicalDistanceFeatherWeights()
  {
    if (weights_texture_) {
      GPU_texture_free(weights_texture_);
    }

    if (distance_falloffs_texture_) {
      GPU_texture_free(distance_falloffs_texture_);
    }
  }

  /* Check if textures containing the weights and distance falloffs were already computed for the
   * given distance falloff type and radius. If such textures exists, do nothing, otherwise, free
   * the already computed textures and recompute it with the given distance falloff type and
   * radius. */
  void update(int radius, int falloff_type)
  {
    if (weights_texture_ && distance_falloffs_texture_ && falloff_type == falloff_type_ &&
        radius == radius_) {
      return;
    }

    radius_ = radius;
    falloff_type_ = falloff_type;

    compute_weights();
    compute_distance_falloffs();
  }

  void compute_weights()
  {
    if (weights_texture_) {
      GPU_texture_free(weights_texture_);
    }

    /* The size of filter is double the radius plus 1, but since the filter is symmetric, we only
     * compute half of it and no doubling happens. We add 1 to make sure the filter size is always
     * odd and there is a center weight. */
    const int size = radius_ + 1;
    Array<float> weights(size);

    float sum = 0.0f;

    /* First, compute the center weight. */
    const float center_weight = RE_filter_value(R_FILTER_GAUSS, 0.0f);
    weights[0] = center_weight;
    sum += center_weight;

    /* Second, compute the other weights in the positive direction, making sure to add double the
     * weight to the sum of weights because the filter is symmetric and we only loop over half of
     * it. Skip the center weight already computed by dropping the front index. */
    const float scale = radius_ > 0.0f ? 1.0f / radius_ : 0.0f;
    for (const int i : weights.index_range().drop_front(1)) {
      const float weight = RE_filter_value(R_FILTER_GAUSS, i * scale);
      weights[i] = weight;
      sum += weight * 2.0f;
    }

    /* Finally, normalize the weights. */
    for (const int i : weights.index_range()) {
      weights[i] /= sum;
    }

    weights_texture_ = GPU_texture_create_1d("Weights", size, 1, GPU_R16F, weights.data());
  }

  void compute_distance_falloffs()
  {
    if (distance_falloffs_texture_) {
      GPU_texture_free(distance_falloffs_texture_);
    }

    /* The size of the distance falloffs is double the radius plus 1, but since the falloffs are
     * symmetric, we only compute half of them and no doubling happens. We add 1 to make sure the
     * falloffs size is always odd and there is a center falloff. */
    const int size = radius_ + 1;
    Array<float> falloffs(size);

    /* Compute the distance falloffs in the positive direction only, because the falloffs are
     * symmetric. */
    const float scale = radius_ > 0.0f ? 1.0f / radius_ : 0.0f;
    for (const int i : falloffs.index_range()) {
      falloffs[i] = compute_distance_falloff(i * scale, falloff_type_);
    }

    distance_falloffs_texture_ = GPU_texture_create_1d(
        "Distance Factors", size, 1, GPU_R16F, falloffs.data());
  }

  void bind_weights_as_texture(GPUShader *shader, const char *texture_name)
  {
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, texture_name);
    GPU_texture_bind(weights_texture_, texture_image_unit);
  }

  void unbind_weights_as_texture()
  {
    GPU_texture_unbind(weights_texture_);
  }

  void bind_distance_falloffs_as_texture(GPUShader *shader, const char *texture_name)
  {
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, texture_name);
    GPU_texture_bind(distance_falloffs_texture_, texture_image_unit);
  }

  void unbind_distance_falloffs_as_texture()
  {
    GPU_texture_unbind(distance_falloffs_texture_);
  }
};

class DilateErodeOperation : public NodeOperation {
 private:
  /* Cached symmetric blur weights and distance falloffs for the distance feature method. */
  SymmetricSeparableMorphologicalDistanceFeatherWeights distance_feather_weights_;

 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Mask").pass_through(get_result("Mask"));
      return;
    }

    switch (get_method()) {
      case CMP_NODE_DILATE_ERODE_STEP:
        execute_step();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE:
        execute_distance();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
        execute_distance_threshold();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
        execute_distance_feather();
        return;
      default:
        BLI_assert_unreachable();
        return;
    }
  }

  /* ----------------------------
   * Step Morphological Operator.
   * ---------------------------- */

  void execute_step()
  {
    GPUTexture *horizontal_pass_result = execute_step_horizontal_pass();
    execute_step_vertical_pass(horizontal_pass_result);
  }

  GPUTexture *execute_step_horizontal_pass()
  {
    GPUShader *shader = shader_manager().get(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will process the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal pass shader, but since its input is
     * transposed, it will effectively do a vertical pass and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each of
     * the passes. */
    const Domain domain = compute_domain();
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    GPUTexture *horizontal_pass_result = texture_pool().acquire_color(transposed_domain);
    const int image_unit = GPU_shader_get_texture_binding(shader, "output_img");
    GPU_texture_image_bind(horizontal_pass_result, image_unit);

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_mask.unbind_as_texture();
    GPU_texture_image_unbind(horizontal_pass_result);

    return horizontal_pass_result;
  }

  void execute_step_vertical_pass(GPUTexture *horizontal_pass_result)
  {
    GPUShader *shader = shader_manager().get(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, "input_tx");
    GPU_texture_bind(horizontal_pass_result, texture_image_unit);

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

    GPU_shader_unbind();
    output_mask.unbind_as_image();
    GPU_texture_unbind(horizontal_pass_result);
  }

  const char *get_morphological_step_shader_name()
  {
    if (get_distance() > 0) {
      return "compositor_morphological_step_dilate";
    }
    return "compositor_morphological_step_erode";
  }

  /* --------------------------------
   * Distance Morphological Operator.
   * -------------------------------- */

  void execute_distance()
  {
    GPUShader *shader = shader_manager().get(get_morphological_distance_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_mask.unbind_as_image();
    input_mask.unbind_as_texture();
  }

  const char *get_morphological_distance_shader_name()
  {
    if (get_distance() > 0) {
      return "compositor_morphological_distance_dilate";
    }
    return "compositor_morphological_distance_erode";
  }

  /* ------------------------------------------
   * Distance Threshold Morphological Operator.
   * ------------------------------------------ */

  void execute_distance_threshold()
  {
    GPUShader *shader = shader_manager().get("compositor_morphological_distance_threshold");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "inset", get_inset());
    GPU_shader_uniform_1i(shader, "radius", get_morphological_distance_threshold_radius());
    GPU_shader_uniform_1i(shader, "distance", get_distance());

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_mask.unbind_as_image();
    input_mask.unbind_as_texture();
  }

  /* See the discussion in the implementation for more information. */
  int get_morphological_distance_threshold_radius()
  {
    return static_cast<int>(math::ceil(get_inset())) + math::abs(get_distance());
  }

  /* ----------------------------------------
   * Distance Feather Morphological Operator.
   * ---------------------------------------- */

  void execute_distance_feather()
  {
    GPUTexture *horizontal_pass_result = execute_distance_feather_horizontal_pass();
    execute_distance_feather_vertical_pass(horizontal_pass_result);
  }

  GPUTexture *execute_distance_feather_horizontal_pass()
  {
    GPUShader *shader = shader_manager().get(get_morphological_distance_feather_shader_name());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Mask");
    input_image.bind_as_texture(shader, "input_tx");

    distance_feather_weights_.update(math::abs(get_distance()), node_storage(bnode()).falloff);
    distance_feather_weights_.bind_weights_as_texture(shader, "weights_tx");
    distance_feather_weights_.bind_distance_falloffs_as_texture(shader, "falloffs_tx");

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will process the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal pass shader, but since its input is
     * transposed, it will effectively do a vertical pass and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each of
     * the passes. */
    const Domain domain = compute_domain();
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    GPUTexture *horizontal_pass_result = texture_pool().acquire_color(transposed_domain);
    const int image_unit = GPU_shader_get_texture_binding(shader, "output_img");
    GPU_texture_image_bind(horizontal_pass_result, image_unit);

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    distance_feather_weights_.unbind_weights_as_texture();
    distance_feather_weights_.unbind_distance_falloffs_as_texture();
    GPU_texture_image_unbind(horizontal_pass_result);

    return horizontal_pass_result;
  }

  void execute_distance_feather_vertical_pass(GPUTexture *horizontal_pass_result)
  {
    GPUShader *shader = shader_manager().get(get_morphological_distance_feather_shader_name());
    GPU_shader_bind(shader);

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
    const int texture_image_unit = GPU_shader_get_texture_binding(shader, "input_tx");
    GPU_texture_bind(horizontal_pass_result, texture_image_unit);

    distance_feather_weights_.update(math::abs(get_distance()), node_storage(bnode()).falloff);
    distance_feather_weights_.bind_weights_as_texture(shader, "weights_tx");
    distance_feather_weights_.bind_distance_falloffs_as_texture(shader, "falloffs_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Mask");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

    GPU_shader_unbind();
    output_image.unbind_as_image();
    distance_feather_weights_.unbind_weights_as_texture();
    distance_feather_weights_.unbind_distance_falloffs_as_texture();
    GPU_texture_unbind(horizontal_pass_result);
  }

  const char *get_morphological_distance_feather_shader_name()
  {
    if (get_distance() > 0) {
      return "compositor_morphological_distance_feather_dilate";
    }
    return "compositor_morphological_distance_feather_erode";
  }

  /* ---------------
   * Common Methods.
   * --------------- */

  bool is_identity()
  {
    const Result &input = get_input("Mask");
    if (input.is_single_value()) {
      return true;
    }

    if (get_method() == CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD && get_inset() != 0.0f) {
      return false;
    }

    if (get_distance() == 0) {
      return true;
    }

    return false;
  }

  int get_distance()
  {
    return bnode().custom2;
  }

  float get_inset()
  {
    return bnode().custom3;
  }

  CMPNodeDilateErodeMethod get_method()
  {
    return (CMPNodeDilateErodeMethod)bnode().custom1;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DilateErodeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_dilate_cc

void register_node_type_cmp_dilateerode()
{
  namespace file_ns = blender::nodes::node_composite_dilate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DILATEERODE, "Dilate/Erode", NODE_CLASS_OP_FILTER);
  ntype.draw_buttons = file_ns::node_composit_buts_dilateerode;
  ntype.declare = file_ns::cmp_node_dilate_declare;
  node_type_init(&ntype, file_ns::node_composit_init_dilateerode);
  node_type_storage(
      &ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
