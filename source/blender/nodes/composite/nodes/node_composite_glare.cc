/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <array>
#include <complex>
#include <memory>

#include "MEM_guardedalloc.h"

#if defined(WITH_FFTW3)
#  include <fftw3.h>
#endif

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_fftw.hh"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"
#include "COM_utilities_diagonals.hh"

#include "node_composite_util.hh"

#define MAX_GLARE_ITERATIONS 5
#define MAX_GLARE_SIZE 9

namespace blender::nodes::node_composite_glare_cc {

NODE_STORAGE_FUNCS(NodeGlare)

static void cmp_node_glare_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_glare(bNodeTree * /*ntree*/, bNode *node)
{
  NodeGlare *ndg = MEM_cnew<NodeGlare>(__func__);
  ndg->quality = 1;
  ndg->type = CMP_NODE_GLARE_STREAKS;
  ndg->iter = 3;
  ndg->colmod = 0.25;
  ndg->mix = 0;
  ndg->threshold = 1;
  ndg->star_45 = true;
  ndg->streaks = 4;
  ndg->angle_ofs = 0.0f;
  ndg->fade = 0.9;
  ndg->size = 8;
  node->storage = ndg;
}

static void node_composit_buts_glare(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const int glare_type = RNA_enum_get(ptr, "glare_type");
#ifndef WITH_FFTW3
  if (glare_type == CMP_NODE_GLARE_FOG_GLOW) {
    uiItemL(layout, RPT_("Disabled, built without FFTW"), ICON_ERROR);
  }
#endif

  uiItemR(layout, ptr, "glare_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "quality", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (ELEM(glare_type, CMP_NODE_GLARE_SIMPLE_STAR, CMP_NODE_GLARE_GHOST, CMP_NODE_GLARE_STREAKS)) {
    uiItemR(layout, ptr, "iterations", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }

  if (ELEM(glare_type, CMP_NODE_GLARE_GHOST, CMP_NODE_GLARE_STREAKS)) {
    uiItemR(layout,
            ptr,
            "color_modulation",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
            std::nullopt,
            ICON_NONE);
  }

  uiItemR(layout, ptr, "mix", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(layout, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  if (glare_type == CMP_NODE_GLARE_STREAKS) {
    uiItemR(layout, ptr, "streaks", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
    uiItemR(layout, ptr, "angle_offset", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }

  if (ELEM(glare_type, CMP_NODE_GLARE_SIMPLE_STAR, CMP_NODE_GLARE_STREAKS)) {
    uiItemR(layout,
            ptr,
            "fade",
            UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
            std::nullopt,
            ICON_NONE);
  }

  if (glare_type == CMP_NODE_GLARE_SIMPLE_STAR) {
    uiItemR(layout, ptr, "use_rotate_45", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }

  if (ELEM(glare_type, CMP_NODE_GLARE_FOG_GLOW, CMP_NODE_GLARE_BLOOM)) {
    uiItemR(layout, ptr, "size", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
}

using namespace blender::compositor;

class GlareOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    Result highlights_result = execute_highlights();
    Result glare_result = execute_glare(highlights_result);
    highlights_result.release();
    execute_mix(glare_result);
    glare_result.release();
  }

  bool is_identity()
  {
    if (get_input("Image").is_single_value()) {
      return true;
    }

    /* A mix factor of -1 indicates that the original image is returned as is. See the execute_mix
     * method for more information. */
    if (node_storage(bnode()).mix == -1.0f) {
      return true;
    }

    return false;
  }

  Result execute_glare(Result &highlights_result)
  {
    switch (node_storage(bnode()).type) {
      case CMP_NODE_GLARE_SIMPLE_STAR:
        return execute_simple_star(highlights_result);
      case CMP_NODE_GLARE_FOG_GLOW:
        return execute_fog_glow(highlights_result);
      case CMP_NODE_GLARE_STREAKS:
        return execute_streaks(highlights_result);
      case CMP_NODE_GLARE_GHOST:
        return execute_ghost(highlights_result);
      case CMP_NODE_GLARE_BLOOM:
        return execute_bloom(highlights_result);
      default:
        BLI_assert_unreachable();
        return context().create_result(ResultType::Color);
    }
  }

  /* -----------------
   * Glare Highlights.
   * ----------------- */

  Result execute_highlights()
  {
    if (this->context().use_gpu()) {
      return this->execute_highlights_gpu();
    }
    return this->execute_highlights_cpu();
  }

  Result execute_highlights_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_glare_highlights");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "threshold", node_storage(bnode()).threshold);

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    input_image.bind_as_texture(shader, "input_tx");

    const int2 glare_size = get_glare_size();
    Result highlights_result = context().create_result(ResultType::Color);
    highlights_result.allocate_texture(glare_size);
    highlights_result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, glare_size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    highlights_result.unbind_as_image();

    return highlights_result;
  }

  Result execute_highlights_cpu()
  {
    const float threshold = node_storage(bnode()).threshold;

    const Result &input = get_input("Image");

    const int2 glare_size = this->get_glare_size();
    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(glare_size);

    /* The dispatch domain covers the output image size, which might be a fraction of the input
     * image size, so you will notice the glare size used throughout the code instead of the input
     * one. */
    parallel_for(glare_size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
       * size to get the coordinates into the sampler's expected [0, 1] range. */
      float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(glare_size);

      float4 hsva;
      rgb_to_hsv_v(input.sample_bilinear_extended(normalized_coordinates), hsva);

      /* The pixel whose luminance value is less than the threshold luminance is not considered
       * part of the highlights and is given a value of zero. Otherwise, the pixel is considered
       * part of the highlights, whose luminance value is the difference to the threshold. */
      hsva.z = math::max(0.0f, hsva.z - threshold);

      float4 rgba;
      hsv_to_rgb_v(hsva, rgba);

      output.store_pixel(texel, float4(rgba.xyz(), 1.0f));
    });

    return output;
  }

  /* ------------------
   * Simple Star Glare.
   * ------------------ */

  Result execute_simple_star(const Result &highlights)
  {
    if (node_storage(bnode()).star_45) {
      return execute_simple_star_diagonal(highlights);
    }
    return execute_simple_star_axis_aligned(highlights);
  }

  Result execute_simple_star_axis_aligned(const Result &highlights)
  {
    Result horizontal_pass_result = execute_simple_star_horizontal_pass(highlights);
    Result vertical_pass_result = this->execute_simple_star_vertical_pass(highlights,
                                                                          horizontal_pass_result);
    horizontal_pass_result.release();
    return vertical_pass_result;
  }

  Result execute_simple_star_vertical_pass(const Result &highlights,
                                           const Result &horizontal_pass_result)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_vertical_pass_gpu(highlights, horizontal_pass_result);
    }
    return this->execute_simple_star_vertical_pass_cpu(highlights, horizontal_pass_result);
  }

  Result execute_simple_star_vertical_pass_gpu(const Result &highlights,
                                               const Result &horizontal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result vertical_pass_result = context().create_result(ResultType::Color);
    vertical_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(vertical_pass_result, highlights);

    GPUShader *shader = context().get_shader("compositor_glare_simple_star_vertical_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    horizontal_pass_result.bind_as_texture(shader, "horizontal_tx");

    vertical_pass_result.bind_as_image(shader, "vertical_img");

    /* Dispatch a thread for each column in the image. */
    const int width = size.x;
    compute_dispatch_threads_at_least(shader, int2(width, 1));

    horizontal_pass_result.unbind_as_texture();
    vertical_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return vertical_pass_result;
  }

  Result execute_simple_star_vertical_pass_cpu(const Result &highlights,
                                               const Result &horizontal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, highlights.load_pixel<float4>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = node_storage(this->bnode()).fade;

    /* Dispatch a thread for each column in the image. */
    const int width = size.x;
    threading::parallel_for(IndexRange(width), 1, [&](const IndexRange sub_range) {
      for (const int64_t x : sub_range) {
        int height = size.y;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * column mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from bottom to top by mixing the value of
           * the pixel in the column with the average value of the previous output and next input
           * in the same column. */
          for (int y = 0; y < height; y++) {
            int2 texel = int2(x, y);
            float4 previous_output = output.load_pixel_zero<float4>(texel - int2(0, i));
            float4 current_input = output.load_pixel<float4>(texel);
            float4 next_input = output.load_pixel_zero<float4>(texel + int2(0, i));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, causal_output);
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from top to bottom by mixing the value
           * of the pixel in the column with the average value of the previous output and next
           * input in the same column. */
          for (int y = height - 1; y >= 0; y--) {
            int2 texel = int2(x, y);
            float4 previous_output = output.load_pixel_zero<float4>(texel + int2(0, i));
            float4 current_input = output.load_pixel<float4>(texel);
            float4 next_input = output.load_pixel_zero<float4>(texel - int2(0, i));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, non_causal_output);
          }
        }

        /* For each pixel in the column mapped to the current invocation thread, add the result of
         * the horizontal pass to the vertical pass. */
        for (int y = 0; y < height; y++) {
          int2 texel = int2(x, y);
          float4 horizontal = horizontal_pass_result.load_pixel<float4>(texel);
          float4 vertical = output.load_pixel<float4>(texel);
          output.store_pixel(texel, horizontal + vertical);
        }
      }
    });

    return output;
  }

  Result execute_simple_star_horizontal_pass(const Result &highlights)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_horizontal_pass_gpu(highlights);
    }
    return this->execute_simple_star_horizontal_pass_cpu(highlights);
  }

  Result execute_simple_star_horizontal_pass_gpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(horizontal_pass_result, highlights);

    GPUShader *shader = context().get_shader("compositor_glare_simple_star_horizontal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    horizontal_pass_result.bind_as_image(shader, "horizontal_img");

    /* Dispatch a thread for each row in the image. */
    compute_dispatch_threads_at_least(shader, int2(size.y, 1));

    horizontal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return horizontal_pass_result;
  }

  Result execute_simple_star_horizontal_pass_cpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      horizontal_pass_result.store_pixel(texel, highlights.load_pixel<float4>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = node_storage(this->bnode()).fade;

    /* Dispatch a thread for each row in the image. */
    const int width = size.x;
    threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_range) {
      for (const int64_t y : sub_range) {
        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * row mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from left to right by mixing the value of
           * the pixel in the row with the average value of the previous output and next input in
           * the same row. */
          for (int x = 0; x < width; x++) {
            int2 texel = int2(x, y);
            float4 previous_output = horizontal_pass_result.load_pixel_zero<float4>(texel -
                                                                                    int2(i, 0));
            float4 current_input = horizontal_pass_result.load_pixel<float4>(texel);
            float4 next_input = horizontal_pass_result.load_pixel_zero<float4>(texel + int2(i, 0));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            horizontal_pass_result.store_pixel(texel, causal_output);
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from right to left by mixing the
           * value of the pixel in the row with the average value of the previous output and next
           * input in the same row. */
          for (int x = width - 1; x >= 0; x--) {
            int2 texel = int2(x, y);
            float4 previous_output = horizontal_pass_result.load_pixel_zero<float4>(texel +
                                                                                    int2(i, 0));
            float4 current_input = horizontal_pass_result.load_pixel<float4>(texel);
            float4 next_input = horizontal_pass_result.load_pixel_zero<float4>(texel - int2(i, 0));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            horizontal_pass_result.store_pixel(texel, non_causal_output);
          }
        }
      }
    });

    return horizontal_pass_result;
  }

  Result execute_simple_star_diagonal(const Result &highlights)
  {
    Result diagonal_pass_result = execute_simple_star_diagonal_pass(highlights);
    Result anti_diagonal_pass_result = this->execute_simple_star_anti_diagonal_pass(
        highlights, diagonal_pass_result);
    diagonal_pass_result.release();
    return anti_diagonal_pass_result;
  }

  Result execute_simple_star_anti_diagonal_pass(const Result &highlights,
                                                const Result &diagonal_pass_result)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_anti_diagonal_pass_gpu(highlights, diagonal_pass_result);
    }
    return this->execute_simple_star_anti_diagonal_pass_cpu(highlights, diagonal_pass_result);
  }

  Result execute_simple_star_anti_diagonal_pass_gpu(const Result &highlights,
                                                    const Result &diagonal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 glare_size = get_glare_size();
    Result anti_diagonal_pass_result = context().create_result(ResultType::Color);
    anti_diagonal_pass_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(anti_diagonal_pass_result, highlights);

    GPUShader *shader = context().get_shader("compositor_glare_simple_star_anti_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    diagonal_pass_result.bind_as_texture(shader, "diagonal_tx");

    anti_diagonal_pass_result.bind_as_image(shader, "anti_diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_number_of_diagonals(glare_size), 1));

    diagonal_pass_result.unbind_as_texture();
    anti_diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return anti_diagonal_pass_result;
  }

  Result execute_simple_star_anti_diagonal_pass_cpu(const Result &highlights,
                                                    const Result &diagonal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, highlights.load_pixel<float4>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = node_storage(this->bnode()).fade;

    /* Dispatch a thread for each diagonal in the image. */
    const int diagonals_count = compute_number_of_diagonals(size);
    threading::parallel_for(IndexRange(diagonals_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t index : sub_range) {
        int anti_diagonal_length = compute_anti_diagonal_length(size, index);
        int2 start = compute_anti_diagonal_start(size, index);
        int2 direction = get_anti_diagonal_direction();
        int2 end = start + (anti_diagonal_length - 1) * direction;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * anti diagonal mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from the start of the anti diagonal to
           * its end by mixing the value of the pixel in the anti diagonal with the average value
           * of the previous output and next input in the same anti diagonal. */
          for (int j = 0; j < anti_diagonal_length; j++) {
            int2 texel = start + j * direction;
            float4 previous_output = output.load_pixel_zero<float4>(texel - i * direction);
            float4 current_input = output.load_pixel<float4>(texel);
            float4 next_input = output.load_pixel_zero<float4>(texel + i * direction);

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, causal_output);
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from the end of the diagonal to its
           * start by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < anti_diagonal_length; j++) {
            int2 texel = end - j * direction;
            float4 previous_output = output.load_pixel_zero<float4>(texel + i * direction);
            float4 current_input = output.load_pixel<float4>(texel);
            float4 next_input = output.load_pixel_zero<float4>(texel - i * direction);

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, non_causal_output);
          }
        }

        /* For each pixel in the anti diagonal mapped to the current invocation thread, add the
         * result of the diagonal pass to the vertical pass. */
        for (int j = 0; j < anti_diagonal_length; j++) {
          int2 texel = start + j * direction;
          float4 horizontal = diagonal_pass_result.load_pixel<float4>(texel);
          float4 vertical = output.load_pixel<float4>(texel);
          output.store_pixel(texel, horizontal + vertical);
        }
      }
    });

    return output;
  }

  Result execute_simple_star_diagonal_pass(const Result &highlights)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_diagonal_pass_gpu(highlights);
    }
    return this->execute_simple_star_diagonal_pass_cpu(highlights);
  }

  Result execute_simple_star_diagonal_pass_gpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 glare_size = get_glare_size();
    Result diagonal_pass_result = context().create_result(ResultType::Color);
    diagonal_pass_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(diagonal_pass_result, highlights);

    GPUShader *shader = context().get_shader("compositor_glare_simple_star_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    diagonal_pass_result.bind_as_image(shader, "diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_number_of_diagonals(glare_size), 1));

    diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return diagonal_pass_result;
  }

  Result execute_simple_star_diagonal_pass_cpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = get_glare_size();
    Result diagonal_pass_result = this->context().create_result(ResultType::Color);
    diagonal_pass_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      diagonal_pass_result.store_pixel(texel, highlights.load_pixel<float4>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = node_storage(this->bnode()).fade;

    /* Dispatch a thread for each diagonal in the image. */
    const int diagonals_count = compute_number_of_diagonals(size);
    threading::parallel_for(IndexRange(diagonals_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t index : sub_range) {
        int diagonal_length = compute_diagonal_length(size, index);
        int2 start = compute_diagonal_start(size, index);
        int2 direction = get_diagonal_direction();
        int2 end = start + (diagonal_length - 1) * direction;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * diagonal mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from the start of the diagonal to its end
           * by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < diagonal_length; j++) {
            int2 texel = start + j * direction;
            float4 previous_output = diagonal_pass_result.load_pixel_zero<float4>(texel -
                                                                                  i * direction);
            float4 current_input = diagonal_pass_result.load_pixel<float4>(texel);
            float4 next_input = diagonal_pass_result.load_pixel_zero<float4>(texel +
                                                                             i * direction);

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            diagonal_pass_result.store_pixel(texel, causal_output);
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from the end of the diagonal to its
           * start by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < diagonal_length; j++) {
            int2 texel = end - j * direction;
            float4 previous_output = diagonal_pass_result.load_pixel_zero<float4>(texel +
                                                                                  i * direction);
            float4 current_input = diagonal_pass_result.load_pixel<float4>(texel);
            float4 next_input = diagonal_pass_result.load_pixel_zero<float4>(texel -
                                                                             i * direction);

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            diagonal_pass_result.store_pixel(texel, non_causal_output);
          }
        }
      }
    });

    return diagonal_pass_result;
  }

  /* --------------
   * Streaks Glare.
   * -------------- */

  Result execute_streaks(const Result &highlights)
  {
    /* Create an initially zero image where streaks will be accumulated. */
    const int2 size = get_glare_size();
    Result accumulated_streaks_result = context().create_result(ResultType::Color);
    accumulated_streaks_result.allocate_texture(size);
    if (this->context().use_gpu()) {
      const float4 zero_color = float4(0.0f);
      GPU_texture_clear(accumulated_streaks_result, GPU_DATA_FLOAT, zero_color);
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        accumulated_streaks_result.store_pixel(texel, float4(0.0f));
      });
    }

    /* For each streak, compute its direction and apply a streak filter in that direction, then
     * accumulate the result into the accumulated streaks result. */
    for (const int streak_index : IndexRange(get_number_of_streaks())) {
      const float2 streak_direction = compute_streak_direction(streak_index);
      Result streak_result = apply_streak_filter(highlights, streak_direction);
      this->accumulate_streak(streak_result, accumulated_streaks_result);
      streak_result.release();
    }

    return accumulated_streaks_result;
  }

  Result apply_streak_filter(const Result &highlights, const float2 &streak_direction)
  {
    if (this->context().use_gpu()) {
      return this->apply_streak_filter_gpu(highlights, streak_direction);
    }
    return this->apply_streak_filter_cpu(highlights, streak_direction);
  }

  Result apply_streak_filter_gpu(const Result &highlights, const float2 &streak_direction)
  {
    GPUShader *shader = context().get_shader("compositor_glare_streaks_filter");
    GPU_shader_bind(shader);

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    const int2 glare_size = get_glare_size();
    Result input_streak_result = context().create_result(ResultType::Color);
    input_streak_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(input_streak_result, highlights);

    Result output_streak_result = context().create_result(ResultType::Color);
    output_streak_result.allocate_texture(glare_size);

    /* For the given number of iterations, apply the streak filter in the given direction. The
     * result of the previous iteration is used as the input of the current iteration. */
    const IndexRange iterations_range = IndexRange(get_number_of_iterations());
    for (const int iteration : iterations_range) {
      const float color_modulator = compute_streak_color_modulator(iteration);
      const float iteration_magnitude = compute_streak_iteration_magnitude(iteration);
      const float3 fade_factors = compute_streak_fade_factors(iteration_magnitude);
      const float2 streak_vector = streak_direction * iteration_magnitude;

      GPU_shader_uniform_1f(shader, "color_modulator", color_modulator);
      GPU_shader_uniform_3fv(shader, "fade_factors", fade_factors);
      GPU_shader_uniform_2fv(shader, "streak_vector", streak_vector);

      GPU_texture_filter_mode(input_streak_result, true);
      GPU_texture_extend_mode(input_streak_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
      input_streak_result.bind_as_texture(shader, "input_streak_tx");

      output_streak_result.bind_as_image(shader, "output_streak_img");

      compute_dispatch_threads_at_least(shader, glare_size);

      input_streak_result.unbind_as_texture();
      output_streak_result.unbind_as_image();

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (iteration != iterations_range.last()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        GPU_texture_copy(input_streak_result, output_streak_result);
      }
    }

    input_streak_result.release();
    GPU_shader_unbind();

    return output_streak_result;
  }

  Result apply_streak_filter_cpu(const Result &highlights, const float2 &streak_direction)
  {
    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    const int2 size = this->get_glare_size();
    Result input = this->context().create_result(ResultType::Color);
    input.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      input.store_pixel(texel, highlights.load_pixel<float4>(texel));
    });

    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);

    /* For the given number of iterations, apply the streak filter in the given direction. The
     * result of the previous iteration is used as the input of the current iteration. */
    const IndexRange iterations_range = IndexRange(this->get_number_of_iterations());
    for (const int iteration : iterations_range) {
      const float color_modulator = this->compute_streak_color_modulator(iteration);
      const float iteration_magnitude = this->compute_streak_iteration_magnitude(iteration);
      const float3 fade_factors = this->compute_streak_fade_factors(iteration_magnitude);
      const float2 streak_vector = streak_direction * iteration_magnitude;

      parallel_for(size, [&](const int2 texel) {
        /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
         * size to get the coordinates into the sampler's expected [0, 1] range. Similarly,
         * transform the vector into the sampler's space by dividing by the input size. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);
        float2 vector = streak_vector / float2(size);

        /* Load three equally spaced neighbors to the current pixel in the direction of the streak
         * vector. */
        float4 neighbors[3];
        neighbors[0] = input.sample_bilinear_zero(coordinates + vector);
        neighbors[1] = input.sample_bilinear_zero(coordinates + vector * 2.0f);
        neighbors[2] = input.sample_bilinear_zero(coordinates + vector * 3.0f);

        /* Attenuate the value of two of the channels for each of the neighbors by multiplying by
         * the color modulator. The particular channels for each neighbor were chosen to be
         * visually similar to the modulation pattern of chromatic aberration. */
        neighbors[0] *= float4(1.0f, color_modulator, color_modulator, 1.0f);
        neighbors[1] *= float4(color_modulator, color_modulator, 1.0f, 1.0f);
        neighbors[2] *= float4(color_modulator, 1.0f, color_modulator, 1.0f);

        /* Compute the weighted sum of all neighbors using the given fade factors as weights. The
         * weights are expected to be lower for neighbors that are further away. */
        float4 weighted_neighbors_sum = float4(0.0f);
        for (int i = 0; i < 3; i++) {
          weighted_neighbors_sum += fade_factors[i] * neighbors[i];
        }

        /* The output is the average between the center color and the weighted sum of the
         * neighbors. Which intuitively mean that highlights will spread in the direction of the
         * streak, which is the desired result. */
        float4 center_color = input.sample_bilinear_zero(coordinates);
        float4 output_color = (center_color + weighted_neighbors_sum) / 2.0f;
        output.store_pixel(texel, output_color);
      });

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (iteration != iterations_range.last()) {
        parallel_for(size, [&](const int2 texel) {
          input.store_pixel(texel, output.load_pixel<float4>(texel));
        });
      }
    }

    input.release();
    return output;
  }

  void accumulate_streak(const Result &streak_result, Result &accumulated_streaks_result)
  {
    if (this->context().use_gpu()) {
      this->accumulate_streak_gpu(streak_result, accumulated_streaks_result);
    }
    else {
      this->accumulate_streak_cpu(streak_result, accumulated_streaks_result);
    }
  }

  void accumulate_streak_gpu(const Result &streak_result, Result &accumulated_streaks_result)
  {
    GPUShader *shader = this->context().get_shader("compositor_glare_streaks_accumulate");
    GPU_shader_bind(shader);

    const float attenuation_factor = this->compute_streak_attenuation_factor();
    GPU_shader_uniform_1f(shader, "attenuation_factor", attenuation_factor);

    streak_result.bind_as_texture(shader, "streak_tx");
    accumulated_streaks_result.bind_as_image(shader, "accumulated_streaks_img", true);

    const int2 glare_size = get_glare_size();
    compute_dispatch_threads_at_least(shader, glare_size);

    streak_result.unbind_as_texture();
    accumulated_streaks_result.unbind_as_image();
    GPU_shader_unbind();
  }

  void accumulate_streak_cpu(const Result &streak, Result &accumulated_streaks)
  {
    const float attenuation_factor = this->compute_streak_attenuation_factor();

    const int2 size = get_glare_size();
    parallel_for(size, [&](const int2 texel) {
      float4 attenuated_streak = streak.load_pixel<float4>(texel) * attenuation_factor;
      float4 current_accumulated_streaks = accumulated_streaks.load_pixel<float4>(texel);
      accumulated_streaks.store_pixel(texel, current_accumulated_streaks + attenuated_streak);
    });
  }

  /* As the number of iterations increase, the streaks spread farther and their intensity decrease.
   * To maintain similar intensities regardless of the number of iterations, streaks with lower
   * number of iteration are linearly attenuated. When the number of iterations is maximum, we need
   * not attenuate, so the denominator should be one, and when the number of iterations is one, we
   * need the attenuation to be maximum. This can be modeled as a simple decreasing linear equation
   * by substituting the two aforementioned cases. */
  float compute_streak_attenuation_factor()
  {
    return 1.0f / (MAX_GLARE_ITERATIONS + 1 - get_number_of_iterations());
  }

  /* Given the index of the streak in the [0, Number Of Streaks - 1] range, compute the unit
   * direction vector defining the streak. The streak directions should make angles with the x-axis
   * that are equally spaced and covers the whole two pi range, starting with the user supplied
   * angle. */
  float2 compute_streak_direction(int streak_index)
  {
    const int number_of_streaks = get_number_of_streaks();
    const float start_angle = get_streaks_start_angle();
    const float angle = start_angle + (float(streak_index) / number_of_streaks) * (M_PI * 2.0f);
    return float2(math::cos(angle), math::sin(angle));
  }

  /* Different color channels of the streaks can be modulated by being multiplied by the color
   * modulator computed by this method. The color modulation is expected to be maximum when the
   * modulation factor is 1 and non existent when it is zero. But since the color modulator is
   * multiplied to the channel and the multiplicative identity is 1, we invert the modulation
   * factor. Moreover, color modulation should be less visible on higher iterations because they
   * produce the farther more faded away parts of the streaks. To achieve that, the modulation
   * factor is raised to the power of the iteration, noting that the modulation value is in the
   * [0, 1] range so the higher the iteration the lower the resulting modulation factor. The plus
   * one makes sure the power starts at one. */
  float compute_streak_color_modulator(int iteration)
  {
    return 1.0f - std::pow(get_color_modulation_factor(), iteration + 1);
  }

  /* Streaks are computed by iteratively applying a filter that samples 3 neighboring pixels in the
   * direction of the streak. Those neighboring pixels are then combined using a weighted sum. The
   * weights of the neighbors are the fade factors computed by this method. Farther neighbors are
   * expected to have lower weights because they contribute less to the combined result. Since the
   * iteration magnitude represents how far the neighbors are, as noted in the description of the
   * compute_streak_iteration_magnitude method, the fade factor for the closest neighbor is
   * computed as the user supplied fade parameter raised to the power of the magnitude, noting that
   * the fade value is in the [0, 1] range while the magnitude is larger than or equal one, so the
   * higher the power the lower the resulting fade factor. Furthermore, the other two neighbors are
   * just squared and cubed versions of the fade factor for the closest neighbor to get even lower
   * fade factors for those farther neighbors. */
  float3 compute_streak_fade_factors(float iteration_magnitude)
  {
    const float fade_factor = std::pow(node_storage(bnode()).fade, iteration_magnitude);
    return float3(fade_factor, std::pow(fade_factor, 2.0f), std::pow(fade_factor, 3.0f));
  }

  /* Streaks are computed by iteratively applying a filter that samples the neighboring pixels in
   * the direction of the streak. Each higher iteration samples pixels that are farther away, the
   * magnitude computed by this method describes how farther away the neighbors are sampled. The
   * magnitude exponentially increase with the iteration. A base of 4, was chosen as compromise
   * between better quality and performance, since a lower base corresponds to more tightly spaced
   * neighbors but would require more iterations to produce a streak of the same length. */
  float compute_streak_iteration_magnitude(int iteration)
  {
    return std::pow(4.0f, iteration);
  }

  float get_streaks_start_angle()
  {
    return node_storage(bnode()).angle_ofs;
  }

  int get_number_of_streaks()
  {
    return node_storage(bnode()).streaks;
  }

  /* ------------
   * Ghost Glare.
   * ------------ */

  Result execute_ghost(const Result &highlights)
  {
    Result base_ghost_result = compute_base_ghost(highlights);
    Result accumulated_ghosts_result = context().create_result(ResultType::Color);
    if (this->context().use_gpu()) {
      this->accumulate_ghosts_gpu(base_ghost_result, accumulated_ghosts_result);
    }
    else {
      this->accumulate_ghosts_cpu(base_ghost_result, accumulated_ghosts_result);
    }

    base_ghost_result.release();
    return accumulated_ghosts_result;
  }

  void accumulate_ghosts_gpu(const Result &base_ghost_result, Result &accumulated_ghosts_result)
  {
    GPUShader *shader = context().get_shader("compositor_glare_ghost_accumulate");
    GPU_shader_bind(shader);

    /* Color modulators are constant across iterations. */
    std::array<float4, 4> color_modulators = compute_ghost_color_modulators();
    GPU_shader_uniform_4fv_array(shader,
                                 "color_modulators",
                                 color_modulators.size(),
                                 (const float(*)[4])color_modulators.data());

    /* Zero initialize output image where ghosts will be accumulated. */
    const float4 zero_color = float4(0.0f);
    const int2 glare_size = get_glare_size();
    accumulated_ghosts_result.allocate_texture(glare_size);
    GPU_texture_clear(accumulated_ghosts_result, GPU_DATA_FLOAT, zero_color);

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    Result input_ghost_result = context().create_result(ResultType::Color);
    input_ghost_result.allocate_texture(glare_size);
    GPU_texture_copy(input_ghost_result, base_ghost_result);

    /* For the given number of iterations, accumulate four ghosts with different scales and color
     * modulators. The result of the previous iteration is used as the input of the current
     * iteration. We start from index 1 because we are not interested in the scales produced for
     * the first iteration according to visual judgment, see the compute_ghost_scales method. */
    const IndexRange iterations_range = IndexRange(get_number_of_iterations()).drop_front(1);
    for (const int i : iterations_range) {
      std::array<float, 4> scales = compute_ghost_scales(i);
      GPU_shader_uniform_4fv(shader, "scales", scales.data());

      input_ghost_result.bind_as_texture(shader, "input_ghost_tx");
      accumulated_ghosts_result.bind_as_image(shader, "accumulated_ghost_img", true);

      compute_dispatch_threads_at_least(shader, glare_size);

      input_ghost_result.unbind_as_texture();
      accumulated_ghosts_result.unbind_as_image();

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (i != iterations_range.last()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        GPU_texture_copy(input_ghost_result, accumulated_ghosts_result);
      }
    }

    GPU_shader_unbind();
    input_ghost_result.release();
  }

  void accumulate_ghosts_cpu(const Result &base_ghost, Result &accumulated_ghosts_result)
  {
    /* Color modulators are constant across iterations. */
    std::array<float4, 4> color_modulators = this->compute_ghost_color_modulators();

    /* Zero initialize output image where ghosts will be accumulated. */
    const int2 size = get_glare_size();
    accumulated_ghosts_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      accumulated_ghosts_result.store_pixel(texel, float4(0.0f));
    });

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    Result input = context().create_result(ResultType::Color);
    input.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      input.store_pixel(texel, base_ghost.load_pixel<float4>(texel));
    });

    /* For the given number of iterations, accumulate four ghosts with different scales and color
     * modulators. The result of the previous iteration is used as the input of the current
     * iteration. We start from index 1 because we are not interested in the scales produced for
     * the first iteration according to visual judgment, see the compute_ghost_scales method. */
    const IndexRange iterations_range = IndexRange(this->get_number_of_iterations()).drop_front(1);
    for (const int i : iterations_range) {
      std::array<float, 4> scales = compute_ghost_scales(i);

      parallel_for(size, [&](const int2 texel) {
        /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
         * size to get the coordinates into the sampler's expected [0, 1] range. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

        /* We accumulate four variants of the input ghost texture, each is scaled by some amount
         * and possibly multiplied by some color as a form of color modulation. */
        float4 accumulated_ghost = float4(0.0f);
        for (int i = 0; i < 4; i++) {
          float scale = scales[i];
          float4 color_modulator = color_modulators[i];

          /* Scale the coordinates for the ghost, pre subtract 0.5 and post add 0.5 to use 0.5 as
           * the origin of the scaling. */
          float2 scaled_coordinates = (coordinates - 0.5f) * scale + 0.5f;

          /* The value of the ghost is attenuated by a scalar multiple of the inverse distance to
           * the center, such that it is maximum at the center and become zero further from the
           * center, making sure to take the scale into account. The scalar multiple of 1 / 4 is
           * chosen using visual judgment. */
          float distance_to_center = math::distance(coordinates, float2(0.5f)) * 2.0f;
          float attenuator = math::max(0.0f, 1.0f - distance_to_center * math::abs(scale)) / 4.0f;

          /* Accumulate the scaled ghost after attenuating and color modulating its value. */
          float4 multiplier = attenuator * color_modulator;
          accumulated_ghost += input.sample_bilinear_zero(scaled_coordinates) * multiplier;
        }

        float4 current_accumulated_ghost = accumulated_ghosts_result.load_pixel<float4>(texel);
        accumulated_ghosts_result.store_pixel(texel,
                                              current_accumulated_ghost + accumulated_ghost);
      });

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (i != iterations_range.last()) {
        parallel_for(size, [&](const int2 texel) {
          input.store_pixel(texel, accumulated_ghosts_result.load_pixel<float4>(texel));
        });
      }
    }

    input.release();
  }

  /* Computes two ghosts by blurring the highlights with two different radii, then adds them into a
   * single base ghost image after scaling them by some factor and flipping the bigger ghost along
   * the center of the image. */
  Result compute_base_ghost(const Result &highlights)
  {
    Result small_ghost_result = context().create_result(ResultType::Color);
    symmetric_separable_blur(context(),
                             highlights,
                             small_ghost_result,
                             float2(get_small_ghost_radius()),
                             R_FILTER_GAUSS,
                             false);

    Result big_ghost_result = context().create_result(ResultType::Color);
    symmetric_separable_blur(context(),
                             highlights,
                             big_ghost_result,
                             float2(get_big_ghost_radius()),
                             R_FILTER_GAUSS,
                             false);

    Result base_ghost_result = context().create_result(ResultType::Color);
    if (this->context().use_gpu()) {
      this->compute_base_ghost_gpu(small_ghost_result, big_ghost_result, base_ghost_result);
    }
    else {
      this->compute_base_ghost_cpu(small_ghost_result, big_ghost_result, base_ghost_result);
    }

    small_ghost_result.release();
    big_ghost_result.release();

    return base_ghost_result;
  }

  void compute_base_ghost_gpu(const Result &small_ghost_result,
                              const Result &big_ghost_result,
                              Result &base_ghost_result)
  {
    GPUShader *shader = context().get_shader("compositor_glare_ghost_base");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(small_ghost_result, true);
    GPU_texture_extend_mode(small_ghost_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    small_ghost_result.bind_as_texture(shader, "small_ghost_tx");

    GPU_texture_filter_mode(big_ghost_result, true);
    GPU_texture_extend_mode(big_ghost_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    big_ghost_result.bind_as_texture(shader, "big_ghost_tx");

    const int2 glare_size = get_glare_size();
    base_ghost_result.allocate_texture(glare_size);
    base_ghost_result.bind_as_image(shader, "combined_ghost_img");

    compute_dispatch_threads_at_least(shader, glare_size);

    GPU_shader_unbind();
    small_ghost_result.unbind_as_texture();
    big_ghost_result.unbind_as_texture();
    base_ghost_result.unbind_as_image();
  }

  void compute_base_ghost_cpu(const Result &small_ghost_result,
                              const Result &big_ghost_result,
                              Result &combined_ghost)
  {
    const int2 size = get_glare_size();
    combined_ghost.allocate_texture(size);

    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
       * size to get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* The small ghost is scaled down with the origin as the center of the image by a factor
       * of 2.13, while the big ghost is flipped and scaled up with the origin as the center of the
       * image by a factor of 0.97. Note that 1) The negative scale implements the flipping. 2)
       * Factors larger than 1 actually scales down the image since the factor multiplies the
       * coordinates and not the images itself. 3) The values are arbitrarily chosen using visual
       * judgment. */
      float small_ghost_scale = 2.13f;
      float big_ghost_scale = -0.97f;

      /* Scale the coordinates for the small and big ghosts, pre subtract 0.5 and post add 0.5 to
       * use 0.5 as the origin of the scaling. Notice that the big ghost is flipped due to the
       * negative scale. */
      float2 small_ghost_coordinates = (coordinates - 0.5f) * small_ghost_scale + 0.5f;
      float2 big_ghost_coordinates = (coordinates - 0.5f) * big_ghost_scale + 0.5f;

      /* The values of the ghosts are attenuated by the inverse distance to the center, such that
       * they are maximum at the center and become zero further from the center, making sure to
       * take the aforementioned scale into account. */
      float distance_to_center = math::distance(coordinates, float2(0.5f)) * 2.0f;
      float small_ghost_attenuator = math::max(0.0f,
                                               1.0f - distance_to_center * small_ghost_scale);
      float big_ghost_attenuator = math::max(
          0.0f, 1.0f - distance_to_center * math::abs(big_ghost_scale));

      float4 small_ghost = small_ghost_result.sample_bilinear_zero(small_ghost_coordinates) *
                           small_ghost_attenuator;
      float4 big_ghost = big_ghost_result.sample_bilinear_zero(big_ghost_coordinates) *
                         big_ghost_attenuator;

      combined_ghost.store_pixel(texel, small_ghost + big_ghost);
    });
  }

  /* In each iteration of ghost accumulation, four ghosts are accumulated, each of which might be
   * modulated by multiplying by some color modulator, this function generates a color modulator
   * for each of the four ghosts. The first ghost is always unmodulated, so is the multiplicative
   * identity of 1. The second ghost gets only its green and blue channels modulated, the third
   * ghost gets only its red and green channels modulated, and the fourth ghost gets only its red
   * and blue channels modulated. */
  std::array<float4, 4> compute_ghost_color_modulators()
  {
    const float color_modulation_factor = get_ghost_color_modulation_factor();

    std::array<float4, 4> color_modulators;
    color_modulators[0] = float4(1.0f);
    color_modulators[1] = float4(1.0f, color_modulation_factor, color_modulation_factor, 1.0f);
    color_modulators[2] = float4(color_modulation_factor, color_modulation_factor, 1.0f, 1.0f);
    color_modulators[3] = float4(color_modulation_factor, 1.0f, color_modulation_factor, 1.0f);

    return color_modulators;
  }

  /* In each iteration of ghost accumulation, four ghosts with different scales are accumulated.
   * Given the index of a certain iteration, this method computes the 4 scales for it. Assuming we
   * have n number of iterations, that means the total number of accumulations is 4 * n. To get a
   * variety of scales, we generate an arithmetic progression that starts from 2.1 and ends at zero
   * exclusive, containing 4 * n elements. The start scale of 2.1 is chosen arbitrarily using
   * visual judgment. To get more scale variations, every other scale is inverted with a slight
   * change in scale such that it alternates between scaling down and up, additionally every other
   * ghost is flipped across the image center by negating its scale. Finally, to get variations
   * across the number of iterations, a shift of 0.5 is introduced when the number of iterations is
   * odd, that way, the user will get variations when changing the number of iterations as opposed
   * to just getting less or more ghosts. */
  std::array<float, 4> compute_ghost_scales(int iteration)
  {
    /* Shift scales by 0.5 for odd number of iterations as discussed in the method description.
     */
    const float offset = (get_number_of_iterations() % 2 == 1) ? 0.5f : 0.0f;

    std::array<float, 4> scales;
    for (const int i : IndexRange(scales.size())) {
      /* Global index in all accumulations. */
      const int global_i = iteration * 4 + i;
      /* Arithmetic progression in the range [0, 1) + offset. */
      const float progression = (global_i + offset) / (get_number_of_iterations() * 4);
      /* Remap range [0, 1) to [1, 0) and multiply to remap to [2.1, 0). */
      scales[i] = 2.1f * (1.0f - progression);

      /* Invert the scale with a slight variation and flip it across the image center through
       * negation for odd scales as discussed in the method description. */
      if (i % 2 == 1) {
        scales[i] = -0.99f / scales[i];
      }
    }

    return scales;
  }

  /* The operation computes two base ghosts by blurring the highlights with two different radii,
   * this method computes the blur radius for the smaller one. The value is chosen using visual
   * judgment. Make sure to take the quality factor into account, see the get_quality_factor method
   * for more information. */
  float get_small_ghost_radius()
  {
    return 16.0f / get_quality_factor();
  }

  /* Computes the blur radius of the bigger ghost, which is double the blur radius if the smaller
   * one, see the get_small_ghost_radius for more information. */
  float get_big_ghost_radius()
  {
    return get_small_ghost_radius() * 2.0f;
  }

  /* The color channels of the glare can be modulated by being multiplied by this factor. In the
   * user interface, 0 means no modulation and 1 means full modulation. But since the factor is
   * multiplied, 1 corresponds to no modulation and 0 corresponds to full modulation, so we
   * subtract from one. */
  float get_ghost_color_modulation_factor()
  {
    return 1.0f - get_color_modulation_factor();
  }

  /* ------------
   * Bloom Glare.
   * ------------ */

  /* Bloom is computed by first progressively half-down-sampling the highlights down to a certain
   * size, then progressively double-up-sampling the last down-sampled result up to the original
   * size of the highlights, adding the down-sampled result of the same size in each up-sampling
   * step. This can be illustrated as follows:
   *
   *             Highlights   ---+--->  Bloom
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   ^
   *                 ...                  |
   *            Down-sampled  ------------'
   *
   * The smooth down-sampling followed by smooth up-sampling can be thought of as a cheap way to
   * approximate a large radius blur, and adding the corresponding down-sampled result while
   * up-sampling is done to counter the attenuation that happens during down-sampling.
   *
   * Smaller down-sampled results contribute to larger glare size, so controlling the size can be
   * done by stopping down-sampling down to a certain size, where the maximum possible size is
   * achieved when down-sampling happens down to the smallest size of 2. */
  Result execute_bloom(Result &highlights)
  {
    /* The maximum possible glare size is achieved when we down-sampled down to the smallest size
     * of 2, which would result in a down-sampling chain length of the binary logarithm of the
     * smaller dimension of the size of the highlights.
     *
     * However, as users might want a smaller glare size, we reduce the chain length by the
     * halving count supplied by the user. */
    const int2 glare_size = get_glare_size();
    const int smaller_glare_dimension = math::min(glare_size.x, glare_size.y);
    const int chain_length = int(std::log2(smaller_glare_dimension)) -
                             compute_bloom_size_halving_count();

    /* If the chain length is less than 2, that means no down-sampling will happen, so we just
     * return a copy of the highlights. This is a sanitization of a corner case, so no need to
     * worry about optimizing the copy away. */
    if (chain_length < 2) {
      Result bloom_result = context().create_result(ResultType::Color);
      bloom_result.allocate_texture(highlights.domain());
      if (this->context().use_gpu()) {
        GPU_texture_copy(bloom_result, highlights);
      }
      else {
        parallel_for(bloom_result.domain().size, [&](const int2 texel) {
          bloom_result.store_pixel(texel, highlights.load_pixel<float4>(texel));
        });
      }
      return bloom_result;
    }

    Array<Result> downsample_chain = compute_bloom_downsample_chain(highlights, chain_length);

    /* Notice that for a chain length of n, we need (n - 1) up-sampling passes. */
    const IndexRange upsample_passes_range(chain_length - 1);

    for (const int i : upsample_passes_range) {
      Result &input = downsample_chain[upsample_passes_range.last() - i + 1];
      Result &output = downsample_chain[upsample_passes_range.last() - i];
      if (this->context().use_gpu()) {
        this->compute_bloom_upsample_gpu(input, output);
      }
      else {
        this->compute_bloom_upsample_cpu(input, output);
      }
      input.release();
    }

    return downsample_chain[0];
  }

  void compute_bloom_upsample_gpu(const Result &input, Result &output)
  {
    GPUShader *shader = context().get_shader("compositor_glare_bloom_upsample");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(input, true);
    input.bind_as_texture(shader, "input_tx");

    output.bind_as_image(shader, "output_img", true);

    compute_dispatch_threads_at_least(shader, output.domain().size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_bloom_upsample_cpu(const Result &input, Result &output)
  {
    /* Each invocation corresponds to one output pixel, where the output has twice the size of the
     * input. */
    const int2 size = output.domain().size;
    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to
       * get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* All the offsets in the following code section are in the normalized pixel space of the
       * output image, so compute its normalized pixel size. */
      float2 pixel_size = 1.0f / float2(size);

      /* Upsample by applying a 3x3 tent filter on the bi-linearly interpolated values evaluated at
       * the center of neighboring output pixels. As more tent filter upsampling passes are
       * applied, the result approximates a large sized Gaussian filter. This upsampling strategy
       * is described in the talk:
       *
       *   Next Generation Post Processing in Call of Duty: Advanced Warfare
       *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
       *
       * In particular, the upsampling strategy is described and illustrated in slide 162 titled
       * "Upsampling - Our Solution". */
      float4 upsampled = float4(0.0f);
      upsampled += (4.0f / 16.0f) * input.sample_bilinear_extended(coordinates);
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, 0.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(0.0f, 1.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, 0.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(0.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, 1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, 1.0f));

      output.store_pixel(texel, output.load_pixel<float4>(texel) + upsampled);
    });
  }

  /* Progressively down-sample the given result into a result with half the size for the given
   * chain length, returning an array containing the chain of down-sampled results. The first
   * result of the chain is the given result itself for easier handling. The chain length is
   * expected not to exceed the binary logarithm of the smaller dimension of the given result,
   * because that would result in down-sampling passes that produce useless textures with just
   * one pixel. */
  Array<Result> compute_bloom_downsample_chain(const Result &highlights, int chain_length)
  {
    const Result downsampled_result = context().create_result(ResultType::Color);
    Array<Result> downsample_chain(chain_length, downsampled_result);

    /* We copy the original highlights result to the first result of the chain to make the code
     * easier. */
    Result &base_layer = downsample_chain[0];
    base_layer.allocate_texture(highlights.domain());
    if (this->context().use_gpu()) {
      GPU_texture_copy(base_layer, highlights);
    }
    else {
      parallel_for(base_layer.domain().size, [&](const int2 texel) {
        base_layer.store_pixel(texel, highlights.load_pixel<float4>(texel));
      });
    }

    /* In turn, the number of passes is one less than the chain length, because the first result
     * needn't be computed. */
    const IndexRange downsample_passes_range(chain_length - 1);

    for (const int i : downsample_passes_range) {
      /* For the first down-sample pass, we use a special "Karis" down-sample pass that applies a
       * form of local tone mapping to reduce the contributions of fireflies, see the shader for
       * more information. Later passes use a simple average down-sampling filter because
       * fireflies doesn't service the first pass. */
      const bool use_karis_average = i == downsample_passes_range.first();
      if (this->context().use_gpu()) {
        this->compute_bloom_downsample_gpu(
            downsample_chain[i], downsample_chain[i + 1], use_karis_average);
      }
      else {
        if (use_karis_average) {
          this->compute_bloom_downsample_cpu<true>(downsample_chain[i], downsample_chain[i + 1]);
        }
        else {
          this->compute_bloom_downsample_cpu<false>(downsample_chain[i], downsample_chain[i + 1]);
        }
      }
    }

    return downsample_chain;
  }

  void compute_bloom_downsample_gpu(const Result &input,
                                    Result &output,
                                    const bool use_karis_average)
  {
    GPUShader *shader = context().get_shader(
        use_karis_average ? "compositor_glare_bloom_downsample_karis_average" :
                            "compositor_glare_bloom_downsample_simple_average");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(input, true);
    input.bind_as_texture(shader, "input_tx");

    output.allocate_texture(input.domain().size / 2);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  template<bool UseKarisAverage>
  void compute_bloom_downsample_cpu(const Result &input, Result &output)
  {
    const int2 size = input.domain().size / 2;
    output.allocate_texture(size);

    /* Each invocation corresponds to one output pixel, where the output has half the size of the
     * input. */
    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to
       * get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* All the offsets in the following code section are in the normalized pixel space of the
       * input texture, so compute its normalized pixel size. */
      float2 pixel_size = 1.0f / float2(input.domain().size);

      /* Each invocation downsamples a 6x6 area of pixels around the center of the corresponding
       * output pixel, but instead of sampling each of the 36 pixels in the area, we only sample 13
       * positions using bilinear fetches at the center of a number of overlapping square 4-pixel
       * groups. This downsampling strategy is described in the talk:
       *
       *   Next Generation Post Processing in Call of Duty: Advanced Warfare
       *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
       *
       * In particular, the downsampling strategy is described and illustrated in slide 153 titled
       * "Downsampling - Our Solution". This is employed as it significantly improves the stability
       * of the glare as can be seen in the videos in the talk. */
      float4 center = input.sample_bilinear_extended(coordinates);
      float4 upper_left_near = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(-1.0f, 1.0f));
      float4 upper_right_near = input.sample_bilinear_extended(coordinates +
                                                               pixel_size * float2(1.0f, 1.0f));
      float4 lower_left_near = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(-1.0f, -1.0f));
      float4 lower_right_near = input.sample_bilinear_extended(coordinates +
                                                               pixel_size * float2(1.0f, -1.0f));
      float4 left_far = input.sample_bilinear_extended(coordinates +
                                                       pixel_size * float2(-2.0f, 0.0f));
      float4 right_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(2.0f, 0.0f));
      float4 upper_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(0.0f, 2.0f));
      float4 lower_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(0.0f, -2.0f));
      float4 upper_left_far = input.sample_bilinear_extended(coordinates +
                                                             pixel_size * float2(-2.0f, 2.0f));
      float4 upper_right_far = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(2.0f, 2.0f));
      float4 lower_left_far = input.sample_bilinear_extended(coordinates +
                                                             pixel_size * float2(-2.0f, -2.0f));
      float4 lower_right_far = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(2.0f, -2.0f));

      float4 result;
      if constexpr (!UseKarisAverage) {
        /* The original weights equation mentioned in slide 153 is:
         *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
         * The 0.5 corresponds to the center group of pixels and the 0.125 corresponds to the other
         * groups of pixels. The center is sampled 4 times, the far non corner pixels are sampled 2
         * times, the near corner pixels are sampled only once; but their weight is quadruple the
         * weights of other groups; so they count as sampled 4 times, finally the far corner pixels
         * are sampled only once, essentially totaling 32 samples. So the weights are as used in
         * the following code section. */
        result = (4.0f / 32.0f) * center +
                 (4.0f / 32.0f) *
                     (upper_left_near + upper_right_near + lower_left_near + lower_right_near) +
                 (2.0f / 32.0f) * (left_far + right_far + upper_far + lower_far) +
                 (1.0f / 32.0f) *
                     (upper_left_far + upper_right_far + lower_left_far + lower_right_far);
      }
      else {
        /* Reduce the contributions of fireflies on the result by reducing each group of pixels
         * using a Karis brightness weighted sum. This is described in slide 168 titled "Fireflies
         * - Partial Karis Average".
         *
         * This needn't be done on all downsampling passes, but only the first one, since fireflies
         * will not survive the first pass, later passes can use the weighted average. */
        float4 center_weighted_sum = this->karis_brightness_weighted_sum(
            upper_left_near, upper_right_near, lower_right_near, lower_left_near);
        float4 upper_left_weighted_sum = this->karis_brightness_weighted_sum(
            upper_left_far, upper_far, center, left_far);
        float4 upper_right_weighted_sum = this->karis_brightness_weighted_sum(
            upper_far, upper_right_far, right_far, center);
        float4 lower_right_weighted_sum = this->karis_brightness_weighted_sum(
            center, right_far, lower_right_far, lower_far);
        float4 lower_left_weighted_sum = this->karis_brightness_weighted_sum(
            left_far, center, lower_far, lower_left_far);

        /* The original weights equation mentioned in slide 153 is:
         *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
         * Multiply both sides by 8 and you get:
         *   4 + 1 + 1 + 1 + 1 = 8
         * So the weights are as used in the following code section. */
        result = (4.0f / 8.0f) * center_weighted_sum +
                 (1.0f / 8.0f) * (upper_left_weighted_sum + upper_right_weighted_sum +
                                  lower_left_weighted_sum + lower_right_weighted_sum);
      }

      output.store_pixel(texel, result);
    });
  }

  /* Computes the weighted average of the given four colors, which are assumed to the colors of
   * spatially neighboring pixels. The weights are computed so as to reduce the contributions of
   * fireflies on the result by applying a form of local tone mapping as described by Brian Karis
   * in the article "Graphic Rants: Tone Mapping".
   *
   * https://graphicrants.blogspot.com/2013/12/tone-mapping.html */
  float4 karis_brightness_weighted_sum(const float4 &color1,
                                       const float4 &color2,
                                       const float4 &color3,
                                       const float4 &color4)
  {
    float4 brightness = float4(math::reduce_max(color1.xyz()),
                               math::reduce_max(color2.xyz()),
                               math::reduce_max(color3.xyz()),
                               math::reduce_max(color4.xyz()));
    float4 weights = 1.0f / (brightness + 1.0f);
    return (color1 * weights.x + color2 * weights.y + color3 * weights.z + color4 * weights.w) *
           math::safe_rcp(math::reduce_add(weights));
  }

  /* The bloom has a maximum possible size when the bloom size is equal to MAX_GLARE_SIZE and
   * halves for every unit decrement of the bloom size. This method computes the number of halving
   * that should take place, which is simply the difference to MAX_GLARE_SIZE. */
  int compute_bloom_size_halving_count()
  {
    return MAX_GLARE_SIZE - get_bloom_size();
  }

  /* The size of the bloom relative to its maximum possible size, see the
   * compute_bloom_size_halving_count() method for more information. */
  int get_bloom_size()
  {
    return node_storage(bnode()).size;
  }

  /* ---------------
   * Fog Glow Glare.
   * --------------- */

  Result execute_fog_glow(const Result &highlights)
  {
#if defined(WITH_FFTW3)
    fftw::initialize_float();

    const int kernel_size = compute_fog_glow_kernel_size();

    /* Since we will be doing a circular convolution, we need to zero pad our input image by half
     * the kernel size to avoid the kernel affecting the pixels at the other side of image.
     * Therefore, zero boundary is assumed. */
    const int needed_padding_amount = kernel_size / 2;
    const int2 image_size = highlights.domain().size;
    const int2 needed_spatial_size = image_size + needed_padding_amount;
    const int2 spatial_size = fftw::optimal_size_for_real_transform(needed_spatial_size);

    /* The FFTW real to complex transforms utilizes the hermitian symmetry of real transforms and
     * stores only half the output since the other half is redundant, so we only allocate half of
     * the first dimension. See Section 4.3.4 Real-data DFT Array Format in the FFTW manual for
     * more information. */
    const int2 frequency_size = int2(spatial_size.x / 2 + 1, spatial_size.y);

    /* We only process the color channels, the alpha channel is written to the output as is. */
    const int channels_count = 3;
    const int image_channels_count = 4;
    const int64_t spatial_pixels_per_channel = int64_t(spatial_size.x) * spatial_size.y;
    const int64_t frequency_pixels_per_channel = int64_t(frequency_size.x) * frequency_size.y;
    const int64_t spatial_pixels_count = spatial_pixels_per_channel * channels_count;
    const int64_t frequency_pixels_count = frequency_pixels_per_channel * channels_count;

    float *image_spatial_domain = fftwf_alloc_real(spatial_pixels_count);
    std::complex<float> *image_frequency_domain = reinterpret_cast<std::complex<float> *>(
        fftwf_alloc_complex(frequency_pixels_count));

    /* Create a real to complex plan to transform the image to the frequency domain. */
    fftwf_plan forward_plan = fftwf_plan_dft_r2c_2d(
        spatial_size.y,
        spatial_size.x,
        image_spatial_domain,
        reinterpret_cast<fftwf_complex *>(image_frequency_domain),
        FFTW_ESTIMATE);

    float *highlights_buffer = nullptr;
    if (this->context().use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      highlights_buffer = static_cast<float *>(GPU_texture_read(highlights, GPU_DATA_FLOAT, 0));
    }
    else {
      highlights_buffer = highlights.float_texture();
    }

    /* Zero pad the image to the required spatial domain size, storing each channel in planar
     * format for better cache locality, that is, RRRR...GGGG...BBBB. */
    threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(spatial_size.x)) {
          const bool is_inside_image = x < image_size.x && y < image_size.y;
          for (const int64_t channel : IndexRange(channels_count)) {
            const int64_t base_index = y * spatial_size.x + x;
            const int64_t output_index = base_index + spatial_pixels_per_channel * channel;
            if (is_inside_image) {
              const int64_t image_index = (y * image_size.x + x) * image_channels_count + channel;
              image_spatial_domain[output_index] = highlights_buffer[image_index];
            }
            else {
              image_spatial_domain[output_index] = 0.0f;
            }
          }
        }
      }
    });

    threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t channel : sub_range) {
        fftwf_execute_dft_r2c(forward_plan,
                              image_spatial_domain + spatial_pixels_per_channel * channel,
                              reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                  frequency_pixels_per_channel * channel);
      }
    });

    const FogGlowKernel &fog_glow_kernel = context().cache_manager().fog_glow_kernels.get(
        kernel_size, spatial_size);

    /* Multiply the kernel and the image in the frequency domain to perform the convolution. The
     * FFT is not normalized, meaning the result of the FFT followed by an inverse FFT will result
     * in an image that is scaled by a factor of the product of the width and height, so we take
     * that into account by dividing by that scale. See Section 4.8.6 Multi-dimensional Transforms
     * of the FFTW manual for more information. */
    const float normalization_scale = float(spatial_size.x) * spatial_size.y *
                                      fog_glow_kernel.normalization_factor();
    threading::parallel_for(IndexRange(frequency_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t channel : IndexRange(channels_count)) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : IndexRange(frequency_size.x)) {
            const int64_t base_index = x + y * frequency_size.x;
            const int64_t output_index = base_index + frequency_pixels_per_channel * channel;
            const std::complex<float> kernel_value = fog_glow_kernel.frequencies()[base_index];
            image_frequency_domain[output_index] *= kernel_value / normalization_scale;
          }
        }
      }
    });

    /* Create a complex to real plan to transform the image to the real domain. */
    fftwf_plan backward_plan = fftwf_plan_dft_c2r_2d(
        spatial_size.y,
        spatial_size.x,
        reinterpret_cast<fftwf_complex *>(image_frequency_domain),
        image_spatial_domain,
        FFTW_ESTIMATE);

    threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t channel : sub_range) {
        fftwf_execute_dft_c2r(backward_plan,
                              reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                  frequency_pixels_per_channel * channel,
                              image_spatial_domain + spatial_pixels_per_channel * channel);
      }
    });

    Result fog_glow_result = context().create_result(ResultType::Color);
    fog_glow_result.allocate_texture(highlights.domain());

    /* For GPU, write the output to the exist highlights_buffer then upload to the result after,
     * while for CPU, write to the result directly. */
    float *output = this->context().use_gpu() ? highlights_buffer :
                                                fog_glow_result.float_texture();

    /* Copy the result to the output. */
    threading::parallel_for(IndexRange(image_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(image_size.x)) {
          for (const int64_t channel : IndexRange(channels_count)) {
            const int64_t output_index = (x + y * image_size.x) * image_channels_count;
            const int64_t base_index = x + y * spatial_size.x;
            const int64_t input_index = base_index + spatial_pixels_per_channel * channel;
            output[output_index + channel] = image_spatial_domain[input_index];
            output[output_index + 3] = highlights_buffer[output_index + 3];
          }
        }
      }
    });

    if (this->context().use_gpu()) {
      GPU_texture_update(fog_glow_result, GPU_DATA_FLOAT, output);
      /* CPU writes to the output directly, so no need to free it. */
      MEM_freeN(output);
    }

    fftwf_destroy_plan(forward_plan);
    fftwf_destroy_plan(backward_plan);
    fftwf_free(image_spatial_domain);
    fftwf_free(image_frequency_domain);
#else
    Result fog_glow_result = context().create_result(ResultType::Color);
    fog_glow_result.allocate_texture(highlights.domain());
    if (this->context().use_gpu()) {
      GPU_texture_copy(fog_glow_result, highlights);
    }
    else {
      parallel_for(fog_glow_result.domain().size, [&](const int2 texel) {
        fog_glow_result.store_pixel(texel, highlights.load_pixel<float4>(texel));
      });
    }
#endif

    return fog_glow_result;
  }

  /* Computes the size of the fog glow kernel that will be convolved with the image, which is
   * essentially the extent of the glare in pixels. */
  int compute_fog_glow_kernel_size()
  {
    /* We use an odd sized kernel since an even one will typically introduce a tiny offset as it
     * has no exact center value. */
    return (1 << node_storage(bnode()).size) + 1;
  }

  /* ----------
   * Glare Mix.
   * ---------- */

  void execute_mix(const Result &glare_result)
  {
    if (this->context().use_gpu()) {
      this->execute_mix_gpu(glare_result);
    }
    else {
      this->execute_mix_cpu(glare_result);
    }
  }

  void execute_mix_gpu(const Result &glare_result)
  {
    GPUShader *shader = context().get_shader("compositor_glare_mix");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "mix_factor", node_storage(bnode()).mix);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    GPU_texture_filter_mode(glare_result, true);
    glare_result.bind_as_texture(shader, "glare_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    glare_result.unbind_as_texture();
  }

  void execute_mix_cpu(const Result &glare_result)
  {
    const float mix_factor = node_storage(bnode()).mix;

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the input
       * image size to get the relevant coordinates into the sampler's expected [0, 1] range.
       * Make sure the input color is not negative to avoid a subtractive effect when mixing the
       * glare.
       */
      float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(input.domain().size);
      float4 glare_color = glare_result.sample_bilinear_extended(normalized_coordinates);
      float4 input_color = math::max(float4(0.0f), input.load_pixel<float4>(texel));

      /* The mix factor is in the range [-1, 1] and linearly interpolate between the three values
       * such that: 1 => Glare only. 0 => Input + Glare. -1 => Input only. We implement that as a
       * weighted sum as follows. When the mix factor is 1, the glare weight should be 1 and the
       * input weight should be 0. When the mix factor is -1, the glare weight should be 0 and
       * the input weight should be 1. When the mix factor is 0, both weights should be 1. This
       * can be expressed using the following compact min max expressions. */
      float input_weight = 1.0f - math::max(0.0f, mix_factor);
      float glare_weight = 1.0f + math::min(0.0f, mix_factor);
      float3 highlights = input_weight * input_color.xyz() + glare_weight * glare_color.xyz();

      output.store_pixel(texel, float4(highlights, input_color.w));
    });
  }

  /* -------
   * Common.
   * ------- */

  /* As a performance optimization, the operation can compute the glare on a fraction of the input
   * image size, which is what this method returns. */
  int2 get_glare_size()
  {
    return compute_domain().size / get_quality_factor();
  }

  int get_number_of_iterations()
  {
    return node_storage(bnode()).iter;
  }

  float get_color_modulation_factor()
  {
    return node_storage(bnode()).colmod;
  }

  /* The glare node can compute the glare on a fraction of the input image size to improve
   * performance. The quality values and their corresponding quality factors are as follows:
   *
   * - High Quality   => Quality Value: 0 => Quality Factor: 1.
   * - Medium Quality => Quality Value: 1 => Quality Factor: 2.
   * - Low Quality    => Quality Value: 2 => Quality Factor: 4.
   *
   * Dividing the image size by the quality factor gives the size where the glare should be
   * computed. The glare algorithm should also take the quality factor into account to compensate
   * for the reduced sized, perhaps by dividing blur radii and similar values by the quality
   * factor. */
  int get_quality_factor()
  {
    return 1 << node_storage(bnode()).quality;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new GlareOperation(context, node);
}

}  // namespace blender::nodes::node_composite_glare_cc

void register_node_type_cmp_glare()
{
  namespace file_ns = blender::nodes::node_composite_glare_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_GLARE, "Glare", NODE_CLASS_OP_FILTER);
  ntype.enum_name_legacy = "GLARE";
  ntype.declare = file_ns::cmp_node_glare_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_glare;
  ntype.initfunc = file_ns::node_composit_init_glare;
  blender::bke::node_type_storage(
      &ntype, "NodeGlare", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
