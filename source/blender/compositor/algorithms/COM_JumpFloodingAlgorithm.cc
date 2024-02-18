/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>
#include <utility>

#include "BLI_array.hh"
#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "COM_JumpFloodingAlgorithm.h"

/* Exact copies of the functions in gpu_shader_compositor_jump_flooding_lib.glsl and
 * jump_flooding.cc but adapted for CPU. See those files for more information. */

namespace blender::compositor {

int2 encode_jump_flooding_value(int2 closest_seed_texel, bool is_flooded)
{
  return is_flooded ? closest_seed_texel : JUMP_FLOODING_NON_FLOODED_VALUE;
}

int2 initialize_jump_flooding_value(int2 texel, bool is_seed)
{
  return encode_jump_flooding_value(texel, is_seed);
}

static int2 load_jump_flooding(Span<int2> input, int2 texel, int2 size, int2 fallback)
{
  if (texel.x < 0 || texel.x >= size.x || texel.y < 0 || texel.y >= size.y) {
    return fallback;
  }
  return input[size_t(texel.y) * size.x + texel.x];
}

static void jump_flooding_pass(Span<int2> input,
                               MutableSpan<int2> output,
                               int2 size,
                               int step_size)
{
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        int2 texel = int2(x, y);

        int2 closest_seed_texel = int2(0);
        float minimum_squared_distance = std::numeric_limits<float>::max();
        for (int j = -1; j <= 1; j++) {
          for (int i = -1; i <= 1; i++) {
            int2 offset = int2(i, j) * step_size;

            int2 fallback = JUMP_FLOODING_NON_FLOODED_VALUE;
            int2 jump_flooding_value = load_jump_flooding(input, texel + offset, size, fallback);

            if (jump_flooding_value == JUMP_FLOODING_NON_FLOODED_VALUE) {
              continue;
            }

            int2 closest_seed_texel_to_neighbor = jump_flooding_value;

            float squared_distance = math::distance_squared(float2(closest_seed_texel_to_neighbor),
                                                            float2(texel));

            if (squared_distance < minimum_squared_distance) {
              minimum_squared_distance = squared_distance;
              closest_seed_texel = closest_seed_texel_to_neighbor;
            }
          }
        }

        bool flooding_happened = minimum_squared_distance != std::numeric_limits<float>::max();
        int2 jump_flooding_value = encode_jump_flooding_value(closest_seed_texel,
                                                              flooding_happened);

        output[size_t(texel.y) * size.x + texel.x] = jump_flooding_value;
      }
    }
  });
}

Array<int2> jump_flooding(Span<int2> input, int2 size)
{
  Array<int2> initial_flooded_result(size_t(size.x) * size.y);
  jump_flooding_pass(input, initial_flooded_result, size, 1);

  Array<int2> *result_to_flood = &initial_flooded_result;
  Array<int2> intermediate_result(size_t(size.x) * size.y);
  Array<int2> *result_after_flooding = &intermediate_result;

  const int max_size = math::max(size.x, size.y);
  int step_size = power_of_2_max_i(max_size) / 2;

  while (step_size != 0) {
    jump_flooding_pass(*result_to_flood, *result_after_flooding, size, step_size);
    std::swap(result_to_flood, result_after_flooding);
    step_size /= 2;
  }

  return *result_to_flood;
}

}  // namespace blender::compositor
