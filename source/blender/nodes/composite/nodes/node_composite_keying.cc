/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

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

    Result blurred_chroma = context().create_result(ResultType::Color);
    symmetric_separable_blur(
        context(), chroma, blurred_chroma, float2(blur_size) / 2, R_FILTER_BOX);
    chroma.release();

    Result blurred_input = replace_input_chroma(blurred_chroma);
    blurred_chroma.release();

    return blurred_input;
  }

  Result extract_input_chroma()
  {
    if (this->context().use_gpu()) {
      return this->extract_input_chroma_gpu();
    }
    return this->extract_input_chroma_cpu();
  }

  Result extract_input_chroma_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_keying_extract_chroma");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result extract_input_chroma_cpu()
  {
    Result &input = get_input("Image");

    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(input.domain());

    parallel_for(input.domain().size, [&](const int2 texel) {
      const float4 color = input.load_pixel(texel);
      float4 color_ycca;
      rgb_to_ycc(color.x,
                 color.y,
                 color.z,
                 &color_ycca.x,
                 &color_ycca.y,
                 &color_ycca.z,
                 BLI_YCC_ITU_BT709);
      color_ycca /= 255.0f;
      color_ycca.w = color.w;

      output.store_pixel(texel, color_ycca);
    });

    return output;
  }

  Result replace_input_chroma(Result &new_chroma)
  {
    if (this->context().use_gpu()) {
      return this->replace_input_chroma_gpu(new_chroma);
    }
    return this->replace_input_chroma_cpu(new_chroma);
  }

  Result replace_input_chroma_gpu(Result &new_chroma)
  {
    GPUShader *shader = context().get_shader("compositor_keying_replace_chroma");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    new_chroma.bind_as_texture(shader, "new_chroma_tx");

    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    new_chroma.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result replace_input_chroma_cpu(Result &new_chroma)
  {
    Result &input = get_input("Image");

    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(input.domain());

    parallel_for(input.domain().size, [&](const int2 texel) {
      const float4 color = input.load_pixel(texel);
      float4 color_ycca;
      rgb_to_ycc(color.x,
                 color.y,
                 color.z,
                 &color_ycca.x,
                 &color_ycca.y,
                 &color_ycca.z,
                 BLI_YCC_ITU_BT709);

      const float2 new_chroma_cb_cr = new_chroma.load_pixel(texel).yz();
      color_ycca.y = new_chroma_cb_cr.x * 255.0f;
      color_ycca.z = new_chroma_cb_cr.y * 255.0f;

      float4 color_rgba;
      ycc_to_rgb(color_ycca.x,
                 color_ycca.y,
                 color_ycca.z,
                 &color_rgba.x,
                 &color_rgba.y,
                 &color_rgba.z,
                 BLI_YCC_ITU_BT709);
      color_rgba.w = color.w;

      output.store_pixel(texel, color_rgba);
    });

    return output;
  }

  Result compute_matte(Result &input)
  {
    if (this->context().use_gpu()) {
      return this->compute_matte_gpu(input);
    }
    return this->compute_matte_cpu(input);
  }

  Result compute_matte_gpu(Result &input)
  {
    GPUShader *shader = context().get_shader("compositor_keying_compute_matte");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "key_balance", node_storage(bnode()).screen_balance);

    input.bind_as_texture(shader, "input_tx");

    Result &key_color = get_input("Key Color");
    key_color.bind_as_texture(shader, "key_tx");

    Result output = context().create_result(ResultType::Float);
    output.allocate_texture(input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, input.domain().size);

    GPU_shader_unbind();
    input.unbind_as_texture();
    key_color.unbind_as_texture();
    output.unbind_as_image();

    return output;
  }

  Result compute_matte_cpu(Result &input)
  {
    const float key_balance = node_storage(bnode()).screen_balance;

    Result &key = get_input("Key Color");

    Result output = context().create_result(ResultType::Float);
    output.allocate_texture(input.domain());

    auto compute_saturation_indices = [](const float3 &v) {
      int index_of_max = ((v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2));
      int2 other_indices = (int2(index_of_max) + int2(1, 2)) % 3;
      int min_index = math::min(other_indices.x, other_indices.y);
      int max_index = math::max(other_indices.x, other_indices.y);
      return int3(index_of_max, max_index, min_index);
    };

    auto compute_saturation = [&](const float4 &color, const int3 &indices) {
      float weighted_average = math::interpolate(color[indices.y], color[indices.z], key_balance);
      return (color[indices.x] - weighted_average) * math::abs(1.0f - weighted_average);
    };

    parallel_for(input.domain().size, [&](const int2 texel) {
      float4 input_color = input.load_pixel(texel);

      /* We assume that the keying screen will not be overexposed in the image, so if the input
       * brightness is high, we assume the pixel is opaque. */
      if (math::reduce_min(input_color) > 1.0f) {
        output.store_pixel(texel, float4(1.0f));
        return;
      }

      float4 key_color = key.load_pixel(texel);
      int3 key_saturation_indices = compute_saturation_indices(key_color.xyz());
      float input_saturation = compute_saturation(input_color, key_saturation_indices);
      float key_saturation = compute_saturation(key_color, key_saturation_indices);

      float matte;
      if (input_saturation < 0.0f) {
        /* Means main channel of pixel is different from screen, assume this is completely a
         * foreground. */
        matte = 1.0f;
      }
      else if (input_saturation >= key_saturation) {
        /* Matched main channels and higher saturation on pixel is treated as completely
         * background. */
        matte = 0.0f;
      }
      else {
        matte = 1.0f - math::clamp(input_saturation / key_saturation, 0.0f, 1.0f);
      }

      output.store_pixel(texel, float4(matte));
    });

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

    if (this->context().use_gpu()) {
      return this->compute_tweaked_matte_gpu(input_matte);
    }
    return this->compute_tweaked_matte_cpu(input_matte);
  }

  Result compute_tweaked_matte_gpu(Result &input_matte)
  {
    GPUShader *shader = context().get_shader("compositor_keying_tweak_matte");
    GPU_shader_bind(shader);

    Result &output_edges = get_result("Edges");
    const bool core_matte_exists = node().input_by_identifier("Core Matte")->is_logically_linked();
    const bool garbage_matte_exists =
        node().input_by_identifier("Garbage Matte")->is_logically_linked();

    GPU_shader_uniform_1b(shader, "compute_edges", output_edges.should_compute());
    GPU_shader_uniform_1b(shader, "apply_core_matte", core_matte_exists);
    GPU_shader_uniform_1b(shader, "apply_garbage_matte", garbage_matte_exists);
    GPU_shader_uniform_1i(shader, "edge_search_radius", node_storage(bnode()).edge_kernel_radius);
    GPU_shader_uniform_1f(shader, "edge_tolerance", node_storage(bnode()).edge_kernel_tolerance);
    GPU_shader_uniform_1f(shader, "black_level", node_storage(bnode()).clip_black);
    GPU_shader_uniform_1f(shader, "white_level", node_storage(bnode()).clip_white);

    input_matte.bind_as_texture(shader, "input_matte_tx");

    Result &garbage_matte = get_input("Garbage Matte");
    garbage_matte.bind_as_texture(shader, "garbage_matte_tx");

    Result &core_matte = get_input("Core Matte");
    core_matte.bind_as_texture(shader, "core_matte_tx");

    Result output_matte = context().create_result(ResultType::Float);
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

  Result compute_tweaked_matte_cpu(Result &input_matte)
  {
    const bool apply_core_matte =
        this->node().input_by_identifier("Core Matte")->is_logically_linked();
    const bool apply_garbage_matte =
        this->node().input_by_identifier("Garbage Matte")->is_logically_linked();

    Result &output_edges = this->get_result("Edges");
    const bool compute_edges = output_edges.should_compute();
    const int edge_search_radius = node_storage(bnode()).edge_kernel_radius;
    const float edge_tolerance = node_storage(bnode()).edge_kernel_tolerance;
    const float black_level = node_storage(bnode()).clip_black;
    const float white_level = node_storage(bnode()).clip_white;

    Result &garbage_matte_image = get_input("Garbage Matte");
    Result &core_matte_image = get_input("Core Matte");

    Result output_matte = context().create_result(ResultType::Float);
    output_matte.allocate_texture(input_matte.domain());

    output_edges.allocate_texture(input_matte.domain());

    parallel_for(input_matte.domain().size, [&](const int2 texel) {
      float matte = input_matte.load_pixel(texel).x;

      /* Search the neighborhood around the current matte value and identify if it lies along the
       * edges of the matte. This is needs to be computed only when we need to compute the edges
       * output or tweak the levels of the matte. */
      bool is_edge = false;
      if (compute_edges || black_level != 0.0f || white_level != 1.0f) {
        /* Count the number of neighbors whose matte is sufficiently similar to the current matte,
         * as controlled by the edge_tolerance factor. */
        int count = 0;
        for (int j = -edge_search_radius; j <= edge_search_radius; j++) {
          for (int i = -edge_search_radius; i <= edge_search_radius; i++) {
            float neighbor_matte = input_matte.load_pixel_extended(texel + int2(i, j)).x;
            count += int(math::distance(matte, neighbor_matte) < edge_tolerance);
          }
        }

        /* If the number of neighbors that are sufficiently similar to the center matte is less
         * that 90% of the total number of neighbors, then that means the variance is high in that
         * areas and it is considered an edge. */
        is_edge = count < ((edge_search_radius * 2 + 1) * (edge_search_radius * 2 + 1)) * 0.9f;
      }

      float tweaked_matte = matte;

      /* Remap the matte using the black and white levels, but only for areas that are not on the
       * edge of the matte to preserve details. Also check for equality between levels to avoid
       * zero division. */
      if (!is_edge && white_level != black_level) {
        tweaked_matte = math::clamp(
            (matte - black_level) / (white_level - black_level), 0.0f, 1.0f);
      }

      /* Exclude unwanted areas using the provided garbage matte, 1 means unwanted, so invert the
       * garbage matte and take the minimum. */
      if (apply_garbage_matte) {
        float garbage_matte = garbage_matte_image.load_pixel(texel).x;
        tweaked_matte = math::min(tweaked_matte, 1.0f - garbage_matte);
      }

      /* Include wanted areas that were incorrectly keyed using the provided core matte. */
      if (apply_core_matte) {
        float core_matte = core_matte_image.load_pixel(texel).x;
        tweaked_matte = math::max(tweaked_matte, core_matte);
      }

      output_matte.store_pixel(texel, float4(tweaked_matte));
      output_edges.store_pixel(texel, float4(is_edge ? 1.0f : 0.0f));
    });

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

    Result blurred_matte = context().create_result(ResultType::Float);
    symmetric_separable_blur(
        context(), input_matte, blurred_matte, float2(blur_size) / 2, R_FILTER_BOX);

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

    Result morphed_matte = context().create_result(ResultType::Float);
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

    Result feathered_matte = context().create_result(ResultType::Float);
    morphological_distance_feather(
        context(), input_matte, feathered_matte, distance, node_storage(bnode()).feather_falloff);

    return feathered_matte;
  }

  void compute_image(Result &matte)
  {
    if (this->context().use_gpu()) {
      this->compute_image_gpu(matte);
    }
    else {
      this->compute_image_cpu(matte);
    }
  }

  void compute_image_gpu(Result &matte)
  {
    GPUShader *shader = context().get_shader("compositor_keying_compute_image");
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

  void compute_image_cpu(Result &matte_image)
  {
    const float despill_factor = node_storage(bnode()).despill_factor;
    const float despill_balance = node_storage(bnode()).despill_balance;

    Result &input = get_input("Image");
    Result &key = get_input("Key Color");

    Result &output = get_result("Image");
    output.allocate_texture(matte_image.domain());

    auto compute_saturation_indices = [](const float3 &v) {
      int index_of_max = ((v.x > v.y) ? ((v.x > v.z) ? 0 : 2) : ((v.y > v.z) ? 1 : 2));
      int2 other_indices = (int2(index_of_max) + int2(1, 2)) % 3;
      int min_index = math::min(other_indices.x, other_indices.y);
      int max_index = math::max(other_indices.x, other_indices.y);
      return int3(index_of_max, max_index, min_index);
    };

    parallel_for(input.domain().size, [&](const int2 texel) {
      float4 key_color = key.load_pixel(texel);
      float4 color = input.load_pixel(texel);
      float matte = matte_image.load_pixel(texel).x;

      /* Alpha multiply the matte to the image. */
      color *= matte;

      /* Color despill. */
      int3 indices = compute_saturation_indices(key_color.xyz());
      float weighted_average = math::interpolate(
          color[indices.y], color[indices.z], despill_balance);
      color[indices.x] -= math::max(0.0f, (color[indices.x] - weighted_average) * despill_factor);

      output.store_pixel(texel, color);
    });
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

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KEYING, "Keying", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_keying_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_keying;
  ntype.initfunc = file_ns::node_composit_init_keying;
  blender::bke::node_type_storage(
      &ntype, "NodeKeyingData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
