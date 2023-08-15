/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Keying  ******************** */

namespace blender::nodes::node_composite_keying_cc {

NODE_STORAGE_FUNCS(NodeKeyingData)

static void cmp_node_keying_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("Key Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Garbage Matte").hide_value().compositor_domain_priority(2);
  b.add_input<decl::Float>("Core Matte").hide_value().compositor_domain_priority(3);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
  b.add_output<decl::Float>("Edges");
}

static void node_composit_init_keying(bNodeTree * /*ntree*/, bNode *node)
{
  NodeKeyingData *data = MEM_cnew<NodeKeyingData>(__func__);

  data->screen_balance = 0.5f;
  data->despill_balance = 0.5f;
  data->despill_factor = 1.0f;
  data->edge_kernel_radius = 3;
  data->edge_kernel_tolerance = 0.1f;
  data->clip_black = 0.0f;
  data->clip_white = 1.0f;
  node->storage = data;
}

static void node_composit_buts_keying(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  // bNode *node = (bNode*)ptr->data; /* UNUSED */

  uiItemR(layout, ptr, "blur_pre", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "screen_balance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "despill_factor", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "despill_balance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "edge_kernel_radius", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "edge_kernel_tolerance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "clip_black", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "clip_white", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "dilate_distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "feather_falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "feather_distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "blur_post", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class KeyingOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result blurred_input = compute_blurred_input();

    Result matte = compute_matte(blurred_input);
    blurred_input.release();

    /* This also computes the edges output if needed. */
    Result tweaked_matte = compute_tweaked_matte(matte);
    matte.release();

    Result &output_image = get_result("Image");
    Result &output_matte = get_result("Matte");
    if (output_image.should_compute() || output_matte.should_compute()) {
      Result blurred_matte = compute_blurred_matte(tweaked_matte);
      tweaked_matte.release();

      Result morphed_matte = compute_morphed_matte(blurred_matte);
      blurred_matte.release();

      Result feathered_matte = compute_feathered_matte(morphed_matte);
      morphed_matte.release();

      if (output_image.should_compute()) {
        compute_image(feathered_matte);
      }

      if (output_matte.should_compute()) {
        output_matte.steal_data(feathered_matte);
      }
      else {
        feathered_matte.release();
      }
    }
  }

  Result compute_blurred_input()
  {
    /* No blur needed, return the original matte. We also increment the reference count of the
     * input because the caller will release it after the call, and we want to extend its life
     * since it is now returned as the output. */
    const float blur_size = node_storage(bnode()).blur_pre;
    if (blur_size == 0.0f) {
      Result output = get_input("Image");
      output.increment_reference_count();
      return output;
    }

    Result chroma = extract_input_chroma();

    Result blurred_chroma = Result::Temporary(ResultType::Color, context().texture_pool());
    symmetric_separable_blur(context(), chroma, blurred_chroma, float2(blur_size), R_FILTER_BOX);
    chroma.release();

    Result blurred_input = replace_input_chroma(blurred_chroma);
    blurred_chroma.release();

    return blurred_input;
  }

  Result extract_input_chroma()
  {
    GPUShader *shader = context().shader_manager().get("compositor_keying_extract_chroma");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result output = Result::Temporary(ResultType::Color, context().texture_pool());
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result replace_input_chroma(Result &new_chroma)
  {
    GPUShader *shader = context().shader_manager().get("compositor_keying_replace_chroma");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    new_chroma.bind_as_texture(shader, "new_chroma_tx");

    Result output = Result::Temporary(ResultType::Color, context().texture_pool());
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    new_chroma.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result compute_matte(Result &input)
  {
    GPUShader *shader = context().shader_manager().get("compositor_keying_compute_matte");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "key_balance", node_storage(bnode()).screen_balance);

    input.bind_as_texture(shader, "input_tx");

    Result &key_color = get_input("Key Color");
    key_color.bind_as_texture(shader, "key_tx");

    Result output = Result::Temporary(ResultType::Float, context().texture_pool());
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    key_color.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result compute_tweaked_matte(Result &input_matte)
  {
    Result &output_edges = get_result("Edges");

    const float black_level = node_storage(bnode()).clip_black;
    const float white_level = node_storage(bnode()).clip_white;

    const bool core_matte_exists = node().input_by_identifier("Core Matte")->is_logically_linked();
    const bool garbage_matte_exists =
        node().input_by_identifier("Garbage Matte")->is_logically_linked();

    /* The edges output is not needed and the matte is not tweaked, so return the original matte.
     * We also increment the reference count of the input because the caller will release it after
     * the call, and we want to extend its life since it is now returned as the output. */
    if (!output_edges.should_compute() && (black_level == 0.0f && white_level == 1.0f) &&
        !core_matte_exists && !garbage_matte_exists)
    {
      Result output_matte = input_matte;
      input_matte.increment_reference_count();
      return output_matte;
    }

    GPUShader *shader = context().shader_manager().get("compositor_keying_tweak_matte");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "compute_edges", output_edges.should_compute());
    GPU_shader_uniform_1b(shader, "apply_core_matte", core_matte_exists);
    GPU_shader_uniform_1b(shader, "apply_garbage_matte", garbage_matte_exists);
    GPU_shader_uniform_1i(shader, "edge_search_radius", node_storage(bnode()).edge_kernel_radius);
    GPU_shader_uniform_1f(shader, "edge_tolerance", node_storage(bnode()).edge_kernel_tolerance);
    GPU_shader_uniform_1f(shader, "black_level", black_level);
    GPU_shader_uniform_1f(shader, "white_level", white_level);

    input_matte.bind_as_texture(shader, "input_matte_tx");

    Result &garbage_matte = get_input("Garbage Matte");
    garbage_matte.bind_as_texture(shader, "garbage_matte_tx");

    Result &core_matte = get_input("Core Matte");
    core_matte.bind_as_texture(shader, "core_matte_tx");

    Result output_matte = Result::Temporary(ResultType::Float, context().texture_pool());
    output_matte.allocate_texture(input_matte.domain());
    output_matte.bind_as_image(shader, "output_matte_img");

    output_edges.allocate_texture(input_matte.domain());
    output_edges.bind_as_image(shader, "output_edges_img");

    compute_dispatch_threads_at_least(shader, input_matte.domain().size);

    GPU_shader_unbind();
    input_matte.unbind_as_texture();
    garbage_matte.unbind_as_texture();
    core_matte.unbind_as_texture();
    output_matte.unbind_as_image();
    output_edges.unbind_as_image();

    return output_matte;
  }

  Result compute_blurred_matte(Result &input_matte)
  {
    const float blur_size = node_storage(bnode()).blur_post;
    /* No blur needed, return the original matte. We also increment the reference count of the
     * input because the caller will release it after the call, and we want to extend its life
     * since it is now returned as the output. */
    if (blur_size == 0.0f) {
      Result output_matte = input_matte;
      input_matte.increment_reference_count();
      return output_matte;
    }

    Result blurred_matte = Result::Temporary(ResultType::Float, context().texture_pool());
    symmetric_separable_blur(context(), input_matte, blurred_matte, float2(blur_size));

    return blurred_matte;
  }

  Result compute_morphed_matte(Result &input_matte)
  {
    const int distance = node_storage(bnode()).dilate_distance;
    /* No morphology needed, return the original matte. We also increment the reference count of
     * the input because the caller will release it after the call, and we want to extend its life
     * since it is now returned as the output. */
    if (distance == 0) {
      Result output_matte = input_matte;
      input_matte.increment_reference_count();
      return output_matte;
    }

    Result morphed_matte = Result::Temporary(ResultType::Float, context().texture_pool());
    morphological_distance(context(), input_matte, morphed_matte, distance);

    return morphed_matte;
  }

  Result compute_feathered_matte(Result &input_matte)
  {
    const int distance = node_storage(bnode()).feather_distance;
    /* No feathering needed, return the original matte. We also increment the reference count of
     * the input because the caller will release it after the call, and we want to extend its life
     * since it is now returned as the output. */
    if (distance == 0) {
      Result output_matte = input_matte;
      input_matte.increment_reference_count();
      return output_matte;
    }

    Result feathered_matte = Result::Temporary(ResultType::Float, context().texture_pool());
    morphological_distance_feather(
        context(), input_matte, feathered_matte, distance, node_storage(bnode()).feather_falloff);

    return feathered_matte;
  }

  void compute_image(Result &matte)
  {
    GPUShader *shader = context().shader_manager().get("compositor_keying_compute_image");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "despill_factor", node_storage(bnode()).despill_factor);
    GPU_shader_uniform_1f(shader, "despill_balance", node_storage(bnode()).despill_balance);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &key = get_input("Key Color");
    key.bind_as_texture(shader, "key_tx");

    matte.bind_as_texture(shader, "matte_tx");

    Result &output = get_result("Image");
    output.allocate_texture(matte.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    key.unbind_as_texture();
    matte.unbind_as_texture();
    output.unbind_as_image();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new KeyingOperation(context, node);
}

}  // namespace blender::nodes::node_composite_keying_cc

void register_node_type_cmp_keying()
{
  namespace file_ns = blender::nodes::node_composite_keying_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KEYING, "Keying", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_keying_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_keying;
  ntype.initfunc = file_ns::node_composit_init_keying;
  node_type_storage(
      &ntype, "NodeKeyingData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
