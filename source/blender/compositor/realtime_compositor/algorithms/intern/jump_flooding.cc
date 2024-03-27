/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BLI_assert.h"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_jump_flooding.hh"

namespace blender::realtime_compositor {

static void jump_flooding_pass(Context &context, Result &input, Result &output, int step_size)
{
  GPUShader *shader = context.get_shader("compositor_jump_flooding", ResultPrecision::Half);
  GPU_shader_bind(shader);

  GPU_shader_uniform_1i(shader, "step_size", step_size);

  input.bind_as_texture(shader, "input_tx");
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  output.unbind_as_image();
}

void jump_flooding(Context &context, Result &input, Result &output)
{
  BLI_assert(input.type() == ResultType::Int2);
  BLI_assert(output.type() == ResultType::Int2);

  /* First, run a jump flooding pass with a step size of 1. This initial pass is proposed by the
   * 1+FJA variant to improve accuracy. */
  Result initial_flooded_result = context.create_temporary_result(ResultType::Int2,
                                                                  ResultPrecision::Half);
  initial_flooded_result.allocate_texture(input.domain());
  jump_flooding_pass(context, input, initial_flooded_result, 1);

  /* We compute the result using a ping-pong buffer, so create an intermediate result. */
  Result *result_to_flood = &initial_flooded_result;
  Result intermediate_result = context.create_temporary_result(ResultType::Int2,
                                                               ResultPrecision::Half);
  intermediate_result.allocate_texture(input.domain());
  Result *result_after_flooding = &intermediate_result;

  /* The algorithm starts with a step size that is half the size of the image. However, the
   * algorithm assumes a square image that is a power of two in width without loss of generality.
   * To generalize that, we use half the next power of two of the maximum dimension. */
  const int max_size = math::max(input.domain().size.x, input.domain().size.y);
  int step_size = power_of_2_max_i(max_size) / 2;

  /* Successively apply a jump flooding pass, halving the step size every time and swapping the
   * ping-pong buffers. */
  while (step_size != 0) {
    jump_flooding_pass(context, *result_to_flood, *result_after_flooding, step_size);
    std::swap(result_to_flood, result_after_flooding);
    step_size /= 2;
  }

  /* Notice that the output of the last pass is stored in result_to_flood due to the last swap, so
   * steal the data from it and release the other buffer. */
  result_after_flooding->release();
  output.steal_data(*result_to_flood);
}

}  // namespace blender::realtime_compositor
