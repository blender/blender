/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include <array>

#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "IMB_colormanagement.h"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

#define MAX_GLARE_ITERATIONS 5

namespace blender::nodes::node_composite_glare_cc {

NODE_STORAGE_FUNCS(NodeGlare)

static void cmp_node_glare_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image"))
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_init_glare(bNodeTree * /*ntree*/, bNode *node)
{
  NodeGlare *ndg = MEM_cnew<NodeGlare>(__func__);
  ndg->quality = 1;
  ndg->type = 2;
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
  uiItemR(layout, ptr, "glare_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "quality", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") != 1) {
    uiItemR(layout, ptr, "iterations", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") != 0) {
      uiItemR(layout,
              ptr,
              "color_modulation",
              UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER,
              nullptr,
              ICON_NONE);
    }
  }

  uiItemR(layout, ptr, "mix", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "threshold", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(layout, ptr, "streaks", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "angle_offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  if (RNA_enum_get(ptr, "glare_type") == 0 || RNA_enum_get(ptr, "glare_type") == 2) {
    uiItemR(
        layout, ptr, "fade", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);

    if (RNA_enum_get(ptr, "glare_type") == 0) {
      uiItemR(layout, ptr, "use_rotate_45", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
  }
  if (RNA_enum_get(ptr, "glare_type") == 1) {
    uiItemR(layout, ptr, "size", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

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
    execute_mix(glare_result);
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

    /* The fog glow mode is currently unsupported. */
    if (node_storage(bnode()).type == CMP_NODE_GLARE_FOG_GLOW) {
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
      default:
        BLI_assert_unreachable();
        return Result(ResultType::Color, texture_pool());
    }
  }

  /* -----------------
   * Glare Highlights.
   * ----------------- */

  Result execute_highlights()
  {
    GPUShader *shader = shader_manager().get("compositor_glare_highlights");
    GPU_shader_bind(shader);

    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
    GPU_shader_uniform_1f(shader, "threshold", node_storage(bnode()).threshold);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");
    GPU_texture_filter_mode(input_image.texture(), true);

    const int2 glare_size = get_glare_size();
    Result highlights_result = Result::Temporary(ResultType::Color, texture_pool());
    highlights_result.allocate_texture(glare_size);
    highlights_result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, glare_size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    highlights_result.unbind_as_image();

    return highlights_result;
  }

  /* ------------------
   * Simple Star Glare.
   * ------------------ */

  Result execute_simple_star(Result &highlights_result)
  {
    if (node_storage(bnode()).star_45) {
      return execute_simple_star_diagonal(highlights_result);
    }
    else {
      return execute_simple_star_axis_aligned(highlights_result);
    }
  }

  Result execute_simple_star_axis_aligned(Result &highlights_result)
  {
    Result horizontal_pass_result = execute_simple_star_horizontal_pass(highlights_result);

    /* The vertical pass is applied in-plane, but the highlights result is no longer needed,
     * so just use it as the pass result. */
    Result &vertical_pass_result = highlights_result;

    GPUShader *shader = shader_manager().get("compositor_glare_simple_star_vertical_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    horizontal_pass_result.bind_as_texture(shader, "horizontal_tx");

    vertical_pass_result.bind_as_image(shader, "vertical_img");

    /* Dispatch a thread for each column in the image. */
    const int width = get_glare_size().x;
    compute_dispatch_threads_at_least(shader, int2(width, 1));

    horizontal_pass_result.unbind_as_texture();
    vertical_pass_result.unbind_as_image();
    GPU_shader_unbind();

    horizontal_pass_result.release();

    return vertical_pass_result;
  }

  Result execute_simple_star_horizontal_pass(Result &highlights_result)
  {
    /* The horizontal pass is applied in-plane, so copy the highlights to a new image since the
     * highlights result is still needed by the vertical pass. */
    const int2 glare_size = get_glare_size();
    Result horizontal_pass_result = Result::Temporary(ResultType::Color, texture_pool());
    horizontal_pass_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(horizontal_pass_result.texture(), highlights_result.texture());

    GPUShader *shader = shader_manager().get("compositor_glare_simple_star_horizontal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    horizontal_pass_result.bind_as_image(shader, "horizontal_img");

    /* Dispatch a thread for each row in the image. */
    compute_dispatch_threads_at_least(shader, int2(glare_size.y, 1));

    horizontal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return horizontal_pass_result;
  }

  Result execute_simple_star_diagonal(Result &highlights_result)
  {
    Result diagonal_pass_result = execute_simple_star_diagonal_pass(highlights_result);

    /* The anti-diagonal pass is applied in-plane, but the highlights result is no longer needed,
     * so just use it as the pass result. */
    Result &anti_diagonal_pass_result = highlights_result;

    GPUShader *shader = shader_manager().get("compositor_glare_simple_star_anti_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    diagonal_pass_result.bind_as_texture(shader, "diagonal_tx");

    anti_diagonal_pass_result.bind_as_image(shader, "anti_diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_simple_star_diagonals_count(), 1));

    diagonal_pass_result.unbind_as_texture();
    anti_diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    diagonal_pass_result.release();

    return anti_diagonal_pass_result;
  }

  Result execute_simple_star_diagonal_pass(Result &highlights_result)
  {
    /* The diagonal pass is applied in-plane, so copy the highlights to a new image since the
     * highlights result is still needed by the anti-diagonal pass. */
    const int2 glare_size = get_glare_size();
    Result diagonal_pass_result = Result::Temporary(ResultType::Color, texture_pool());
    diagonal_pass_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(diagonal_pass_result.texture(), highlights_result.texture());

    GPUShader *shader = shader_manager().get("compositor_glare_simple_star_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", node_storage(bnode()).fade);

    diagonal_pass_result.bind_as_image(shader, "diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_simple_star_diagonals_count(), 1));

    diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return diagonal_pass_result;
  }

  /* The Star 45 option of the Simple Star mode of glare is applied on the diagonals of the image.
   * This method computes the number of diagonals in the glare image. For more information on the
   * used equation, see the compute_number_of_diagonals function in the following shader library
   * file: gpu_shader_compositor_image_diagonals.glsl */
  int compute_simple_star_diagonals_count()
  {
    const int2 size = get_glare_size();
    return size.x + size.y - 1;
  }

  /* --------------
   * Streaks Glare.
   * -------------- */

  Result execute_streaks(Result &highlights_result)
  {
    /* Create an initially zero image where streaks will be accumulated. */
    const float4 zero_color = float4(0.0f);
    const int2 glare_size = get_glare_size();
    Result accumulated_streaks_result = Result::Temporary(ResultType::Color, texture_pool());
    accumulated_streaks_result.allocate_texture(glare_size);
    GPU_texture_clear(accumulated_streaks_result.texture(), GPU_DATA_FLOAT, zero_color);

    /* For each streak, compute its direction and apply a streak filter in that direction, then
     * accumulate the result into the accumulated streaks result. */
    for (const int streak_index : IndexRange(get_number_of_streaks())) {
      const float2 streak_direction = compute_streak_direction(streak_index);
      Result streak_result = apply_streak_filter(highlights_result, streak_direction);

      GPUShader *shader = shader_manager().get("compositor_glare_streaks_accumulate");
      GPU_shader_bind(shader);

      const float attenuation_factor = compute_streak_attenuation_factor();
      GPU_shader_uniform_1f(shader, "attenuation_factor", attenuation_factor);

      streak_result.bind_as_texture(shader, "streak_tx");
      accumulated_streaks_result.bind_as_image(shader, "accumulated_streaks_img", true);

      compute_dispatch_threads_at_least(shader, glare_size);

      streak_result.unbind_as_texture();
      accumulated_streaks_result.unbind_as_image();

      streak_result.release();
      GPU_shader_unbind();
    }

    return accumulated_streaks_result;
  }

  Result apply_streak_filter(Result &highlights_result, const float2 &streak_direction)
  {
    GPUShader *shader = shader_manager().get("compositor_glare_streaks_filter");
    GPU_shader_bind(shader);

    /* Copy the highlights result into a new image because the output will be copied to the input
     * after each iteration and the highlights result is still needed to compute other streaks. */
    const int2 glare_size = get_glare_size();
    Result input_streak_result = Result::Temporary(ResultType::Color, texture_pool());
    input_streak_result.allocate_texture(glare_size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(input_streak_result.texture(), highlights_result.texture());

    Result output_streak_result = Result::Temporary(ResultType::Color, texture_pool());
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

      input_streak_result.bind_as_texture(shader, "input_streak_tx");
      GPU_texture_filter_mode(input_streak_result.texture(), true);
      GPU_texture_wrap_mode(input_streak_result.texture(), false, false);

      output_streak_result.bind_as_image(shader, "output_streak_img");

      compute_dispatch_threads_at_least(shader, glare_size);

      input_streak_result.unbind_as_texture();
      output_streak_result.unbind_as_image();

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (iteration != iterations_range.last()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        GPU_texture_copy(input_streak_result.texture(), output_streak_result.texture());
      }
    }

    input_streak_result.release();
    GPU_shader_unbind();

    return output_streak_result;
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
   * direction vector defining the streak. The streak directions should make angles with the
   * x-axis that are equally spaced and covers the whole two pi range, starting with the user
   * supplied angle. */
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

  /* Streaks are computed by iteratively applying a filter that samples 3 neighboring pixels in
   * the direction of the streak. Those neighboring pixels are then combined using a weighted sum.
   * The weights of the neighbors are the fade factors computed by this method. Farther neighbors
   * are expected to have lower weights because they contribute less to the combined result. Since
   * the iteration magnitude represents how far the neighbors are, as noted in the description of
   * the compute_streak_iteration_magnitude method, the fade factor for the closest neighbor is
   * computed as the user supplied fade parameter raised to the power of the magnitude, noting that
   * the fade value is in the [0, 1] range while the magnitude is larger than or equal one, so the
   * higher the power the lower the resulting fade factor. Furthermore, the other two neighbors
   * are just squared and cubed versions of the fade factor for the closest neighbor to get even
   * lower fade factors for those farther neighbors. */
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

  Result execute_ghost(Result &highlights_result)
  {
    Result base_ghost_result = compute_base_ghost(highlights_result);

    GPUShader *shader = shader_manager().get("compositor_glare_ghost_accumulate");
    GPU_shader_bind(shader);

    /* Color modulators are constant across iterations. */
    std::array<float4, 4> color_modulators = compute_ghost_color_modulators();
    GPU_shader_uniform_4fv_array(shader,
                                 "color_modulators",
                                 color_modulators.size(),
                                 (const float(*)[4])color_modulators.data());

    /* Create an initially zero image where ghosts will be accumulated. */
    const float4 zero_color = float4(0.0f);
    const int2 glare_size = get_glare_size();
    Result accumulated_ghosts_result = Result::Temporary(ResultType::Color, texture_pool());
    accumulated_ghosts_result.allocate_texture(glare_size);
    GPU_texture_clear(accumulated_ghosts_result.texture(), GPU_DATA_FLOAT, zero_color);

    /* For the given number of iterations, accumulate four ghosts with different scales and color
     * modulators. The result of the previous iteration is used as the input of the current
     * iteration. We start from index 1 because we are not interested in the scales produced for
     * the first iteration according to visual judgment, see the compute_ghost_scales method. */
    Result &input_ghost_result = base_ghost_result;
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
        GPU_texture_copy(input_ghost_result.texture(), accumulated_ghosts_result.texture());
      }
    }

    GPU_shader_unbind();
    input_ghost_result.release();

    return accumulated_ghosts_result;
  }

  /* Computes two ghosts by blurring the highlights with two different radii, then adds them into a
   * single base ghost image after scaling them by some factor and flipping the bigger ghost along
   * the center of the image. */
  Result compute_base_ghost(Result &highlights_result)
  {
    Result small_ghost_result = Result::Temporary(ResultType::Color, texture_pool());
    symmetric_separable_blur(context(),
                             highlights_result,
                             small_ghost_result,
                             float2(get_small_ghost_radius()),
                             R_FILTER_GAUSS,
                             false,
                             false);

    Result big_ghost_result = Result::Temporary(ResultType::Color, texture_pool());
    symmetric_separable_blur(context(),
                             highlights_result,
                             big_ghost_result,
                             float2(get_big_ghost_radius()),
                             R_FILTER_GAUSS,
                             false,
                             false);

    highlights_result.release();

    GPUShader *shader = shader_manager().get("compositor_glare_ghost_base");
    GPU_shader_bind(shader);

    small_ghost_result.bind_as_texture(shader, "small_ghost_tx");
    GPU_texture_filter_mode(small_ghost_result.texture(), true);
    GPU_texture_wrap_mode(small_ghost_result.texture(), false, false);

    big_ghost_result.bind_as_texture(shader, "big_ghost_tx");
    GPU_texture_filter_mode(big_ghost_result.texture(), true);
    GPU_texture_wrap_mode(big_ghost_result.texture(), false, false);

    const int2 glare_size = get_glare_size();
    Result base_ghost_result = Result::Temporary(ResultType::Color, texture_pool());
    base_ghost_result.allocate_texture(glare_size);
    base_ghost_result.bind_as_image(shader, "combined_ghost_img");

    compute_dispatch_threads_at_least(shader, glare_size);

    GPU_shader_unbind();
    small_ghost_result.unbind_as_texture();
    big_ghost_result.unbind_as_texture();
    base_ghost_result.unbind_as_image();

    small_ghost_result.release();
    big_ghost_result.release();

    return base_ghost_result;
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
   * variety of scales, we generate an arithmetic progression that starts from 2.1 and ends at
   * zero exclusive, containing 4 * n elements. The start scale of 2.1 is chosen arbitrarily using
   * visual judgment. To get more scale variations, every other scale is inverted with a slight
   * change in scale such that it alternates between scaling down and up, additionally every other
   * ghost is flipped across the image center by negating its scale. Finally, to get variations
   * across the number of iterations, a shift of 0.5 is introduced when the number of iterations is
   * odd, that way, the user will get variations when changing the number of iterations as opposed
   * to just getting less or more ghosts. */
  std::array<float, 4> compute_ghost_scales(int iteration)
  {
    /* Shift scales by 0.5 for odd number of iterations as discussed in the method description. */
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
   * judgment. Make sure to take the quality factor into account, see the get_quality_factor
   * method for more information. */
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

  /* ---------------
   * Fog Glow Glare.
   * --------------- */

  /* Not yet implemented. Unreachable code due to the is_identity method. */
  Result execute_fog_glow(Result & /*highlights_result*/)
  {
    BLI_assert_unreachable();
    return Result(ResultType::Color, texture_pool());
  }

  /* ----------
   * Glare Mix.
   * ---------- */

  void execute_mix(Result &glare_result)
  {
    GPUShader *shader = shader_manager().get("compositor_glare_mix");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "mix_factor", node_storage(bnode()).mix);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    glare_result.bind_as_texture(shader, "glare_tx");
    GPU_texture_filter_mode(glare_result.texture(), true);

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    glare_result.unbind_as_texture();

    glare_result.release();
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

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_GLARE, "Glare", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_glare_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_glare;
  ntype.initfunc = file_ns::node_composit_init_glare;
  node_type_storage(&ntype, "NodeGlare", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
