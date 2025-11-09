/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_color.h"
#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

#include "RNA_enum_types.hh"

#include "GPU_shader.hh"

#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_keying_cc {

static void cmp_node_keying_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_output<decl::Float>("Matte").structure_type(StructureType::Dynamic);
  b.add_output<decl::Float>("Edges")
      .structure_type(StructureType::Dynamic)
      .translation_context(BLT_I18NCONTEXT_ID_IMAGE);

  b.add_input<decl::Color>("Key Color")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);

  PanelDeclarationBuilder &preprocess_panel = b.add_panel("Preprocess").default_closed(true);
  preprocess_panel.add_input<decl::Int>("Blur Size", "Preprocess Blur Size")
      .default_value(0)
      .min(0)
      .description(
          "Blur the color of the input image in YCC color space before keying while leaving the "
          "luminance intact using a Gaussian blur of the given size");

  PanelDeclarationBuilder &key_panel = b.add_panel("Key").default_closed(true).translation_context(
      BLT_I18NCONTEXT_ID_NODETREE);
  key_panel.add_input<decl::Float>("Balance", "Key Balance")
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Balances between the two non primary color channels that the primary channel compares "
          "against. 0 means the latter channel of the two is used, while 1 means the former of "
          "the two is used");

  PanelDeclarationBuilder &tweak_panel = b.add_panel("Tweak").default_closed(true);
  tweak_panel.add_input<decl::Float>("Black Level")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "The matte gets remapped such matte values lower than the black level become black. "
          "Pixels at the identified edges are excluded from the remapping to preserve details");
  tweak_panel.add_input<decl::Float>("White Level")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "The matte gets remapped such matte values higher than the white level become white. "
          "Pixels at the identified edges are excluded from the remapping to preserve details");

  PanelDeclarationBuilder &edges_panel =
      tweak_panel.add_panel("Edges").default_closed(true).translation_context(
          BLT_I18NCONTEXT_ID_IMAGE);
  edges_panel.add_input<decl::Int>("Size", "Edge Search Size")
      .default_value(3)
      .min(0)
      .description(
          "Size of the search window used to identify edges. Higher search size corresponds to "
          "less noisy and higher quality edges, not necessarily bigger edges. Edge tolerance can "
          "be used to expend the size of the edges");
  edges_panel.add_input<decl::Float>("Tolerance", "Edge Tolerance")
      .default_value(0.1f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Pixels are considered part of the edges if more than 10% of the neighbouring pixels "
          "have matte values that differ from the pixel's matte value by this tolerance");

  PanelDeclarationBuilder &mask_panel = b.add_panel("Mask").default_closed(true);
  mask_panel.add_input<decl::Float>("Garbage Matte")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic)
      .description("Areas in the garbage matte mask are excluded from the matte");
  mask_panel.add_input<decl::Float>("Core Matte")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic)
      .description("Areas in the core matte mask are included in the matte");

  PanelDeclarationBuilder &postprocess_panel = b.add_panel("Postprocess").default_closed(true);
  postprocess_panel.add_input<decl::Int>("Blur Size", "Postprocess Blur Size")
      .default_value(0)
      .min(0)
      .description("Blur the computed matte using a Gaussian blur of the given size");
  postprocess_panel.add_input<decl::Int>("Dilate Size", "Postprocess Dilate Size")
      .default_value(0)
      .description(
          "Dilate or erode the computed matte using a circular structuring element of the "
          "specified size. Negative sizes means erosion while positive means dilation");
  postprocess_panel.add_input<decl::Int>("Feather Size", "Postprocess Feather Size")
      .default_value(0)
      .description(
          "Dilate or erode the computed matte using an inverse distance operation evaluated at "
          "the given falloff of the specified size. Negative sizes means erosion while positive "
          "means dilation");
  postprocess_panel.add_input<decl::Menu>("Feather Falloff")
      .default_value(PROP_SMOOTH)
      .static_items(rna_enum_proportional_falloff_curve_only_items)
      .optional_label()
      .translation_context(BLT_I18NCONTEXT_ID_CURVE_LEGACY);

  PanelDeclarationBuilder &despill_panel = b.add_panel("Despill").default_closed(true);
  despill_panel.add_input<decl::Float>("Strength", "Despill Strength")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Specifies the strength of the despill");
  despill_panel.add_input<decl::Float>("Balance", "Despill Balance")
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Defines the channel used for despill limiting. Balances between the two non primary "
          "color channels that the primary channel compares against. 0 means the latter channel "
          "of the two is used, while 1 means the former of the two is used");
}

static void node_composit_init_keying(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, only kept for forward compatibility. */
  NodeKeyingData *data = MEM_callocN<NodeKeyingData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class KeyingOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    Result &output_matte = get_result("Matte");
    Result &output_edges = get_result("Edges");
    if (input_image.is_single_value()) {
      if (output_image.should_compute()) {
        output_image.share_data(input_image);
      }
      if (output_matte.should_compute()) {
        output_matte.allocate_invalid();
      }
      if (output_edges.should_compute()) {
        output_edges.allocate_invalid();
      }
      return;
    }

    Result blurred_input = compute_blurred_input();

    Result matte = compute_matte(blurred_input);
    blurred_input.release();

    /* This also computes the edges output if needed. */
    Result tweaked_matte = compute_tweaked_matte(matte);
    matte.release();

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
    else {
      tweaked_matte.release();
    }
  }

  Result compute_blurred_input()
  {
    /* No blur needed, return the original matte. We also increment the reference count of the
     * input because the caller will release it after the call, and we want to extend its life
     * since it is now returned as the output. */
    const float blur_size = this->get_preprocess_blur_size();
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

  int get_preprocess_blur_size()
  {
    return math::max(0, this->get_input("Preprocess Blur Size").get_single_value_default(0));
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
    gpu::Shader *shader = context().get_shader("compositor_keying_extract_chroma");
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
      const Color color = input.load_pixel<Color>(texel);
      float4 color_ycca;
      rgb_to_ycc(color.r,
                 color.g,
                 color.b,
                 &color_ycca.x,
                 &color_ycca.y,
                 &color_ycca.z,
                 BLI_YCC_ITU_BT709);
      color_ycca /= 255.0f;
      color_ycca.w = color.a;

      output.store_pixel(texel, Color(color_ycca));
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
    gpu::Shader *shader = context().get_shader("compositor_keying_replace_chroma");
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
      const Color color = input.load_pixel<Color>(texel);
      float4 color_ycca;
      rgb_to_ycc(color.r,
                 color.g,
                 color.b,
                 &color_ycca.x,
                 &color_ycca.y,
                 &color_ycca.z,
                 BLI_YCC_ITU_BT709);

      const float2 new_chroma_cb_cr = float4(new_chroma.load_pixel<Color>(texel)).yz();
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
      color_rgba.w = color.a;

      output.store_pixel(texel, Color(color_rgba));
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
    gpu::Shader *shader = context().get_shader("compositor_keying_compute_matte");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "key_balance", this->get_key_balance());

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
    const float key_balance = this->get_key_balance();

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
      float4 input_color = float4(input.load_pixel<Color>(texel));

      /* We assume that the keying screen will not be overexposed in the image, so if the input
       * brightness is high, we assume the pixel is opaque. */
      if (math::reduce_min(input_color) > 1.0f) {
        output.store_pixel(texel, 1.0f);
        return;
      }

      float4 key_color = float4(key.load_pixel<Color, true>(texel));
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

      output.store_pixel(texel, matte);
    });

    return output;
  }

  float get_key_balance()
  {
    return math::clamp(this->get_input("Key Balance").get_single_value_default(0.5f), 0.0f, 1.0f);
  }

  Result compute_tweaked_matte(Result &input_matte)
  {
    const float black_level = this->get_black_level();
    const float white_level = this->get_black_level();
    const Result &garbage_matte = this->get_input("Garbage Matte");
    const Result &core_matte = this->get_input("Core Matte");

    /* The edges output is not needed and the matte is not tweaked, so return the original matte.
     * We also increment the reference count of the input because the caller will release it after
     * the call, and we want to extend its life since it is now returned as the output. */
    Result &output_edges = get_result("Edges");
    if (!output_edges.should_compute() && black_level == 0.0f && white_level == 1.0f &&
        core_matte.get_single_value_default(1.0f) == 0.0f &&
        garbage_matte.get_single_value_default(1.0f) == 0.0f)
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
    gpu::Shader *shader = context().get_shader(this->get_tweak_matte_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "edge_search_radius", this->get_edge_search_size());
    GPU_shader_uniform_1f(shader, "edge_tolerance", this->get_edge_tolerance());
    GPU_shader_uniform_1f(shader, "black_level", this->get_black_level());
    GPU_shader_uniform_1f(shader, "white_level", this->get_white_level());

    input_matte.bind_as_texture(shader, "input_matte_tx");

    Result &garbage_matte = get_input("Garbage Matte");
    garbage_matte.bind_as_texture(shader, "garbage_matte_tx");

    Result &core_matte = get_input("Core Matte");
    core_matte.bind_as_texture(shader, "core_matte_tx");

    Result output_matte = context().create_result(ResultType::Float);
    output_matte.allocate_texture(input_matte.domain());
    output_matte.bind_as_image(shader, "output_matte_img");

    Result &output_edges = get_result("Edges");
    if (output_edges.should_compute()) {
      output_edges.allocate_texture(input_matte.domain());
      output_edges.bind_as_image(shader, "output_edges_img");
    }

    compute_dispatch_threads_at_least(shader, input_matte.domain().size);

    GPU_shader_unbind();
    input_matte.unbind_as_texture();
    garbage_matte.unbind_as_texture();
    core_matte.unbind_as_texture();
    output_matte.unbind_as_image();
    if (output_edges.should_compute()) {
      output_edges.unbind_as_image();
    }

    return output_matte;
  }

  const char *get_tweak_matte_shader_name()
  {
    Result &output_edges = get_result("Edges");
    return output_edges.should_compute() ? "compositor_keying_tweak_matte_with_edges" :
                                           "compositor_keying_tweak_matte_without_edges";
  }

  Result compute_tweaked_matte_cpu(Result &input_matte)
  {
    const int edge_search_radius = this->get_edge_search_size();
    const float edge_tolerance = this->get_edge_tolerance();
    const float black_level = this->get_black_level();
    const float white_level = this->get_white_level();

    Result &garbage_matte_image = get_input("Garbage Matte");
    Result &core_matte_image = get_input("Core Matte");

    Result output_matte = context().create_result(ResultType::Float);
    output_matte.allocate_texture(input_matte.domain());

    Result &output_edges = this->get_result("Edges");
    const bool compute_edges = output_edges.should_compute();
    if (compute_edges) {
      output_edges.allocate_texture(input_matte.domain());
    }

    parallel_for(input_matte.domain().size, [&](const int2 texel) {
      float matte = input_matte.load_pixel<float>(texel);

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
            float neighbor_matte = input_matte.load_pixel_extended<float>(texel + int2(i, j));
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
      float garbage_matte = garbage_matte_image.load_pixel<float, true>(texel);
      tweaked_matte = math::min(tweaked_matte, 1.0f - garbage_matte);

      /* Include wanted areas that were incorrectly keyed using the provided core matte. */
      float core_matte = core_matte_image.load_pixel<float, true>(texel);
      tweaked_matte = math::max(tweaked_matte, core_matte);

      output_matte.store_pixel(texel, tweaked_matte);
      if (compute_edges) {
        output_edges.store_pixel(texel, is_edge ? 1.0f : 0.0f);
      }
    });

    return output_matte;
  }

  int get_edge_search_size()
  {
    return math::max(0, this->get_input("Edge Search Size").get_single_value_default(3));
  }

  float get_edge_tolerance()
  {
    return math::clamp(
        this->get_input("Edge Tolerance").get_single_value_default(0.1f), 0.0f, 1.0f);
  }

  float get_black_level()
  {
    return math::clamp(this->get_input("Black Level").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  float get_white_level()
  {
    return math::clamp(this->get_input("White Level").get_single_value_default(1.0f), 0.0f, 1.0f);
  }

  Result compute_blurred_matte(Result &input_matte)
  {
    const float blur_size = this->get_postprocess_blur_size();
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

  int get_postprocess_blur_size()
  {
    return math::max(0, this->get_input("Postprocess Blur Size").get_single_value_default(0));
  }

  Result compute_morphed_matte(Result &input_matte)
  {
    const int distance = this->get_postprocess_dilate_size();
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

  int get_postprocess_dilate_size()
  {
    return this->get_input("Postprocess Dilate Size").get_single_value_default(0);
  }

  Result compute_feathered_matte(Result &input_matte)
  {
    const int distance = this->get_postprocess_feather_size();
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
        context(), input_matte, feathered_matte, distance, this->get_feather_falloff());

    return feathered_matte;
  }

  int get_postprocess_feather_size()
  {
    return this->get_input("Postprocess Feather Size").get_single_value_default(0);
  }

  int get_feather_falloff()
  {
    const Result &input = this->get_input("Feather Falloff");
    const MenuValue default_menu_value = MenuValue(PROP_SMOOTH);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return menu_value.value;
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
    gpu::Shader *shader = context().get_shader("compositor_keying_compute_image");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "despill_factor", this->get_despill_strength());
    GPU_shader_uniform_1f(shader, "despill_balance", this->get_despill_balance());

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
    const float despill_factor = this->get_despill_strength();
    const float despill_balance = this->get_despill_balance();

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
      float4 key_color = float4(key.load_pixel<Color, true>(texel));
      float4 color = float4(input.load_pixel<Color>(texel));
      float matte = matte_image.load_pixel<float>(texel);

      /* Alpha multiply the matte to the image. */
      color *= matte;

      /* Color despill. */
      int3 indices = compute_saturation_indices(key_color.xyz());
      float weighted_average = math::interpolate(
          color[indices.y], color[indices.z], despill_balance);
      color[indices.x] -= math::max(0.0f, (color[indices.x] - weighted_average) * despill_factor);

      output.store_pixel(texel, Color(color));
    });
  }

  float get_despill_strength()
  {
    return math::max(0.0f, this->get_input("Despill Strength").get_single_value_default(1.0f));
  }

  float get_despill_balance()
  {
    return math::clamp(
        this->get_input("Despill Balance").get_single_value_default(0.5f), 0.0f, 1.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new KeyingOperation(context, node);
}

}  // namespace blender::nodes::node_composite_keying_cc

static void register_node_type_cmp_keying()
{
  namespace file_ns = blender::nodes::node_composite_keying_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeKeying", CMP_NODE_KEYING);
  ntype.ui_name = "Keying";
  ntype.ui_description =
      "Perform both chroma keying (to remove the backdrop) and despill (to correct color cast "
      "from the backdrop)";
  ntype.enum_name_legacy = "KEYING";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_keying_declare;
  ntype.initfunc = file_ns::node_composit_init_keying;
  blender::bke::node_type_storage(
      ntype, "NodeKeyingData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 155, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_keying)
