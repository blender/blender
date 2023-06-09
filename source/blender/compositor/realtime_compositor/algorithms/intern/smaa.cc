/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "IMB_colormanagement.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_smaa.hh"

#include "COM_smaa_precomputed_textures.hh"

namespace blender::realtime_compositor {

static Result detect_edges(Context &context,
                           Result &input,
                           float threshold,
                           float local_contrast_adaptation_factor)
{
  GPUShader *shader = context.shader_manager().get("compositor_smaa_edge_detection");
  GPU_shader_bind(shader);

  switch (input.type()) {
    case ResultType::Color: {
      float luminance_coefficients[3];
      IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
      GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
      break;
    }
    case ResultType::Vector: {
      float luminance_coefficients[3] = {1.0f, 1.0f, 1.0f};
      GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
      break;
    }
    case ResultType::Float: {
      float luminance_coefficients[3] = {1.0f, 0.0f, 0.0f};
      GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);
      break;
    }
  }

  GPU_shader_uniform_1f(shader, "smaa_threshold", threshold);
  GPU_shader_uniform_1f(
      shader, "smaa_local_contrast_adaptation_factor", local_contrast_adaptation_factor);

  GPU_texture_filter_mode(input.texture(), true);
  input.bind_as_texture(shader, "input_tx");

  Result edges = Result::Temporary(ResultType::Color, context.texture_pool());
  edges.allocate_texture(input.domain());
  edges.bind_as_image(shader, "edges_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  edges.unbind_as_image();

  return edges;
}

static Result calculate_blending_weights(Context &context, Result &edges, int corner_rounding)
{
  GPUShader *shader = context.shader_manager().get("compositor_smaa_blending_weight_calculation");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1i(shader, "smaa_corner_rounding", corner_rounding);

  GPU_texture_filter_mode(edges.texture(), true);
  edges.bind_as_texture(shader, "edges_tx");

  const SMAAPrecomputedTextures &smaa_precomputed_textures =
      context.cache_manager().smaa_precomputed_textures.get();
  smaa_precomputed_textures.bind_area_texture(shader, "area_tx");
  smaa_precomputed_textures.bind_search_texture(shader, "search_tx");

  Result weights = Result::Temporary(ResultType::Color, context.texture_pool());
  weights.allocate_texture(edges.domain());
  weights.bind_as_image(shader, "weights_img");

  compute_dispatch_threads_at_least(shader, edges.domain().size);

  GPU_shader_unbind();
  edges.unbind_as_texture();
  smaa_precomputed_textures.unbind_area_texture();
  smaa_precomputed_textures.unbind_search_texture();
  weights.unbind_as_image();

  return weights;
}

static void blend_neighborhood(Context &context, Result &input, Result &weights, Result &output)
{
  GPUShader *shader = context.shader_manager().get(
      input.type() == ResultType::Float ? "compositor_smaa_neighborhood_blending_float" :
                                          "compositor_smaa_neighborhood_blending_color");
  GPU_shader_bind(shader);

  GPU_texture_filter_mode(input.texture(), true);
  input.bind_as_texture(shader, "input_tx");

  GPU_texture_filter_mode(weights.texture(), true);
  weights.bind_as_texture(shader, "weights_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_as_texture();
  output.unbind_as_image();
}

void smaa(Context &context,
          Result &input,
          Result &output,
          float threshold,
          float local_contrast_adaptation_factor,
          int corner_rounding)
{
  Result edges = detect_edges(context, input, threshold, local_contrast_adaptation_factor);
  Result weights = calculate_blending_weights(context, edges, corner_rounding);
  edges.release();
  blend_neighborhood(context, input, weights, output);
  weights.release();
}

}  // namespace blender::realtime_compositor
