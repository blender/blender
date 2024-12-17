/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <utility>

#include "BLI_assert.h"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_jump_flooding.hh"

namespace blender::compositor {

static void jump_flooding_pass_gpu(Context &context, Result &input, Result &output, int step_size)
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

/* This function implements a single pass of the Jump Flooding algorithm described in sections 3.1
 * and 3.2 of the paper:
 *
 *   Rong, Guodong, and Tiow-Seng Tan. "Jump flooding in GPU with applications to Voronoi diagram
 *   and distance transform." Proceedings of the 2006 symposium on Interactive 3D graphics and
 *   games. 2006.
 *
 * The function is a straightforward implementation of the aforementioned sections of the paper,
 * noting that the nil special value in the paper is equivalent to JUMP_FLOODING_NON_FLOODED_VALUE.
 *
 * The `COM_algorithm_jump_flooding.hh` header contains the necessary utility functions to
 * initialize and encode the jump flooding values. */
static void jump_flooding_pass_cpu(Result &input, Result &output, int step_size)
{
  parallel_for(input.domain().size, [&](const int2 texel) {
    /* For each of the previously flooded pixels in the 3x3 window of the given step size around
     * the center pixel, find the position of the closest seed pixel that is closest to the current
     * center pixel. */
    int2 closest_seed_texel = int2(0);
    float minimum_squared_distance = std::numeric_limits<float>::max();
    for (int j = -1; j <= 1; j++) {
      for (int i = -1; i <= 1; i++) {
        int2 offset = int2(i, j) * step_size;

        /* Use #JUMP_FLOODING_NON_FLOODED_VALUE as a fallback value to exempt out of bound pixels
         * from the loop as can be seen in the following continue condition. */
        int2 fallback = JUMP_FLOODING_NON_FLOODED_VALUE;
        int2 jump_flooding_value = input.load_pixel_fallback(texel + offset, fallback);

        /* The pixel is either not flooded yet or is out of bound, so skip it. */
        if (jump_flooding_value == JUMP_FLOODING_NON_FLOODED_VALUE) {
          continue;
        }

        /* The neighboring pixel is flooded, so its flooding value is the texel of the closest seed
         * pixel to this neighboring pixel. */
        int2 closest_seed_texel_to_neighbor = jump_flooding_value;

        /* Compute the squared distance to the neighbor's closest seed pixel. */
        float squared_distance = math::distance_squared(float2(closest_seed_texel_to_neighbor),
                                                        float2(texel));

        if (squared_distance < minimum_squared_distance) {
          minimum_squared_distance = squared_distance;
          closest_seed_texel = closest_seed_texel_to_neighbor;
        }
      }
    }

    /* If the minimum squared distance is still #std::numeric_limits<float>::max(), that means the
     * loop never got past the continue condition and thus no flooding happened. If flooding
     * happened, we encode the closest seed texel in the format expected by the algorithm. */
    bool flooding_happened = minimum_squared_distance != std::numeric_limits<float>::max();
    int2 jump_flooding_value = encode_jump_flooding_value(closest_seed_texel, flooding_happened);

    output.store_pixel(texel, jump_flooding_value);
  });
}

static void jump_flooding_pass(Context &context, Result &input, Result &output, int step_size)
{
  if (context.use_gpu()) {
    jump_flooding_pass_gpu(context, input, output, step_size);
  }
  else {
    jump_flooding_pass_cpu(input, output, step_size);
  }
}

void jump_flooding(Context &context, Result &input, Result &output)
{
  BLI_assert(input.type() == ResultType::Int2);
  BLI_assert(output.type() == ResultType::Int2);

  /* First, run a jump flooding pass with a step size of 1. This initial pass is proposed by the
   * 1+FJA variant to improve accuracy. */
  Result initial_flooded_result = context.create_result(ResultType::Int2, ResultPrecision::Half);
  initial_flooded_result.allocate_texture(input.domain());
  jump_flooding_pass(context, input, initial_flooded_result, 1);

  /* We compute the result using a ping-pong buffer, so create an intermediate result. */
  Result *result_to_flood = &initial_flooded_result;
  Result intermediate_result = context.create_result(ResultType::Int2, ResultPrecision::Half);
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

}  // namespace blender::compositor
