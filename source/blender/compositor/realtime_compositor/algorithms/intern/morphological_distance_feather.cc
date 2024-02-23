/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_algorithm_morphological_distance_feather.hh" /* Own include. */
#include "COM_context.hh"
#include "COM_morphological_distance_feather_weights.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

static const char *get_shader_name(int distance)
{
  if (distance > 0) {
    return "compositor_morphological_distance_feather_dilate";
  }
  return "compositor_morphological_distance_feather_erode";
}

static Result horizontal_pass(Context &context, Result &input, int distance, int falloff_type)
{
  GPUShader *shader = context.get_shader(get_shader_name(distance));
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));
  weights.bind_weights_as_texture(shader, "weights_tx");
  weights.bind_distance_falloffs_as_texture(shader, "falloffs_tx");

  /* We allocate an output image of a transposed size, that is, with a height equivalent to the
   * width of the input and vice versa. This is done as a performance optimization. The shader
   * will process the image horizontally and write it to the intermediate output transposed. Then
   * the vertical pass will execute the same horizontal pass shader, but since its input is
   * transposed, it will effectively do a vertical pass and write to the output transposed,
   * effectively undoing the transposition in the horizontal pass. This is done to improve
   * spatial cache locality in the shader and to avoid having two separate shaders for each of
   * the passes. */
  const Domain domain = input.domain();
  const int2 transposed_domain = int2(domain.size.y, domain.size.x);

  Result output = context.create_temporary_result(ResultType::Float);
  output.allocate_texture(transposed_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  weights.unbind_weights_as_texture();
  weights.unbind_distance_falloffs_as_texture();
  output.unbind_as_image();

  return output;
}

static void vertical_pass(Context &context,
                          Result &original_input,
                          Result &horizontal_pass_result,
                          Result &output,
                          int distance,
                          int falloff_type)
{
  GPUShader *shader = context.get_shader(get_shader_name(distance));
  GPU_shader_bind(shader);

  horizontal_pass_result.bind_as_texture(shader, "input_tx");

  const MorphologicalDistanceFeatherWeights &weights =
      context.cache_manager().morphological_distance_feather_weights.get(
          context, falloff_type, math::abs(distance));
  weights.bind_weights_as_texture(shader, "weights_tx");
  weights.bind_distance_falloffs_as_texture(shader, "falloffs_tx");

  const Domain domain = original_input.domain();
  output.allocate_texture(domain);
  output.bind_as_image(shader, "output_img");

  /* Notice that the domain is transposed, see the note on the horizontal pass function for more
   * information on the reasoning behind this. */
  compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

  GPU_shader_unbind();
  horizontal_pass_result.unbind_as_texture();
  weights.unbind_weights_as_texture();
  weights.unbind_distance_falloffs_as_texture();
  output.unbind_as_image();
}

void morphological_distance_feather(
    Context &context, Result &input, Result &output, int distance, int falloff_type)
{
  Result horizontal_pass_result = horizontal_pass(context, input, distance, falloff_type);
  vertical_pass(context, input, horizontal_pass_result, output, distance, falloff_type);
  horizontal_pass_result.release();
}

}  // namespace blender::realtime_compositor
