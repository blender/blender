/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "GPU_compute.hh"
#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"

#include "COM_algorithm_summed_area_table.hh"

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Summed Area Table
 *
 * An implementation of the summed area table algorithm from the paper:
 *
 *   Nehab, Diego, et al. "GPU-efficient recursive filtering and summed-area tables."
 *
 * This file is a straightforward implementation of each of the four passes described in
 * Algorithm SAT in section 6 of the paper. Note that we use Blender's convention of first
 * quadrant images, so we call prologues horizontal or X prologues, and we call transposed
 * prologues vertical or Y prologues. See each of the functions for more details. */

static const char *get_compute_incomplete_prologues_shader(SummedAreaTableOperation operation)
{
  switch (operation) {
    case SummedAreaTableOperation::Identity:
      return "compositor_summed_area_table_compute_incomplete_prologues_identity";
    case SummedAreaTableOperation::Square:
      return "compositor_summed_area_table_compute_incomplete_prologues_square";
  }

  BLI_assert_unreachable();
  return "";
}

/* Computes the horizontal and vertical incomplete prologues from the given input using equations
 * (42) and (43) to implement the first pass of Algorithm SAT. Those equations accumulatively sum
 * each row in each block, writing the final sum to the X incomplete block, then sum each column in
 * the X accumulatively summed block, writing the final sum to the Y incomplete block. The output
 * is the prologues along the horizontal and vertical directions, where the accumulation axis is
 * stored along the vertical axis, so the X prologues are stored transposed for better cache
 * locality. */
static void compute_incomplete_prologues(Context &context,
                                         Result &input,
                                         SummedAreaTableOperation operation,
                                         Result &incomplete_x_prologues,
                                         Result &incomplete_y_prologues)
{
  gpu::Shader *shader = context.get_shader(get_compute_incomplete_prologues_shader(operation),
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  const int2 group_size = int2(16);
  const int2 input_size = input.domain().size;
  const int2 number_of_groups = math::divide_ceil(input_size, group_size);

  incomplete_x_prologues.allocate_texture(Domain(int2(input_size.y, number_of_groups.x)));
  incomplete_x_prologues.bind_as_image(shader, "incomplete_x_prologues_img");

  incomplete_y_prologues.allocate_texture(Domain(int2(input_size.x, number_of_groups.y)));
  incomplete_y_prologues.bind_as_image(shader, "incomplete_y_prologues_img");

  GPU_compute_dispatch(shader, number_of_groups.x, number_of_groups.y, 1);

  GPU_shader_unbind();
  input.unbind_as_texture();
  incomplete_x_prologues.unbind_as_image();
  incomplete_y_prologues.unbind_as_image();
}

/* Computes the complete X prologues and their sum from the incomplete X prologues using equation
 * (44) to implement the second pass of Algorithm SAT. That equation simply sum the incomplete
 * prologue and all incomplete prologues before it, writing the sum to the complete prologue. Then,
 * each of the complete prologues is summed using parallel reduction writing the sum to the output
 * sum for each block. The shader runs in parallel vertically, but serially horizontally. Note that
 * the input incomplete X prologues and output complete X prologues are stored transposed for
 * better cache locality, but the output sum is stored straight, not transposed. */
static void compute_complete_x_prologues(Context &context,
                                         Result &input,
                                         Result &incomplete_x_prologues,
                                         Result &complete_x_prologues,
                                         Result &complete_x_prologues_sum)
{
  gpu::Shader *shader = context.get_shader(
      "compositor_summed_area_table_compute_complete_x_prologues", ResultPrecision::Full);
  GPU_shader_bind(shader);

  incomplete_x_prologues.bind_as_texture(shader, "incomplete_x_prologues_tx");

  const int2 group_size = int2(16);
  const int2 input_size = input.domain().size;
  const int2 number_of_groups = math::divide_ceil(input_size, group_size);

  complete_x_prologues.allocate_texture(incomplete_x_prologues.domain());
  complete_x_prologues.bind_as_image(shader, "complete_x_prologues_img");

  complete_x_prologues_sum.allocate_texture(Domain(number_of_groups));
  complete_x_prologues_sum.bind_as_image(shader, "complete_x_prologues_sum_img");

  GPU_compute_dispatch(shader, number_of_groups.y, 1, 1);

  GPU_shader_unbind();
  incomplete_x_prologues.unbind_as_texture();
  complete_x_prologues.unbind_as_image();
  complete_x_prologues_sum.unbind_as_image();
}

/* Computes the complete Y prologues from the incomplete Y prologues using equation (45) to
 * implement the third pass of Algorithm SAT. That equation simply sum the incomplete prologue and
 * all incomplete prologues before it, then adds the sum of the complete X prologue for the same
 * block, writing the sum to the complete prologue. The shader runs in parallel horizontally, but
 * serially vertically. */
static void compute_complete_y_prologues(Context &context,
                                         Result &input,
                                         Result &incomplete_y_prologues,
                                         Result &complete_x_prologues_sum,
                                         Result &complete_y_prologues)
{
  gpu::Shader *shader = context.get_shader(
      "compositor_summed_area_table_compute_complete_y_prologues", ResultPrecision::Full);
  GPU_shader_bind(shader);

  incomplete_y_prologues.bind_as_texture(shader, "incomplete_y_prologues_tx");
  complete_x_prologues_sum.bind_as_texture(shader, "complete_x_prologues_sum_tx");

  const int2 group_size = int2(16);
  const int2 input_size = input.domain().size;
  const int2 number_of_groups = math::divide_ceil(input_size, group_size);

  complete_y_prologues.allocate_texture(incomplete_y_prologues.domain());
  complete_y_prologues.bind_as_image(shader, "complete_y_prologues_img");

  GPU_compute_dispatch(shader, number_of_groups.x, 1, 1);

  GPU_shader_unbind();
  incomplete_y_prologues.unbind_as_texture();
  complete_x_prologues_sum.unbind_as_texture();
  complete_y_prologues.unbind_as_image();
}

static const char *get_compute_complete_blocks_shader(SummedAreaTableOperation operation)
{
  switch (operation) {
    case SummedAreaTableOperation::Identity:
      return "compositor_summed_area_table_compute_complete_blocks_identity";
    case SummedAreaTableOperation::Square:
      return "compositor_summed_area_table_compute_complete_blocks_square";
  }

  BLI_assert_unreachable();
  return "";
}

/* Computes the final summed area table blocks from the complete X and Y prologues using equation
 * (41) to implement the fourth pass of Algorithm SAT. That equation simply uses an intermediate
 * shared memory to cascade the accumulation of rows and then column in each block using the
 * prologues as initial values and writes each step of the latter accumulation to the output. */
static void compute_complete_blocks(Context &context,
                                    Result &input,
                                    Result &complete_x_prologues,
                                    Result &complete_y_prologues,
                                    SummedAreaTableOperation operation,
                                    Result &output)
{
  gpu::Shader *shader = context.get_shader(get_compute_complete_blocks_shader(operation),
                                           ResultPrecision::Full);
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");
  complete_x_prologues.bind_as_texture(shader, "complete_x_prologues_tx");
  complete_y_prologues.bind_as_texture(shader, "complete_y_prologues_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img", true);

  const int2 group_size = int2(16);
  const int2 input_size = input.domain().size;
  const int2 number_of_groups = math::divide_ceil(input_size, group_size);

  GPU_compute_dispatch(shader, number_of_groups.x, number_of_groups.y, 1);

  GPU_shader_unbind();
  input.unbind_as_texture();
  complete_x_prologues.unbind_as_texture();
  complete_y_prologues.unbind_as_texture();
  output.unbind_as_image();
}

static void summed_area_table_gpu(Context &context,
                                  Result &input,
                                  Result &output,
                                  SummedAreaTableOperation operation)
{
  Result incomplete_x_prologues = context.create_result(ResultType::Color, ResultPrecision::Full);
  Result incomplete_y_prologues = context.create_result(ResultType::Color, ResultPrecision::Full);
  compute_incomplete_prologues(
      context, input, operation, incomplete_x_prologues, incomplete_y_prologues);

  Result complete_x_prologues = context.create_result(ResultType::Color, ResultPrecision::Full);
  Result complete_x_prologues_sum = context.create_result(ResultType::Color,
                                                          ResultPrecision::Full);
  compute_complete_x_prologues(
      context, input, incomplete_x_prologues, complete_x_prologues, complete_x_prologues_sum);
  incomplete_x_prologues.release();

  Result complete_y_prologues = context.create_result(ResultType::Color, ResultPrecision::Full);
  compute_complete_y_prologues(
      context, input, incomplete_y_prologues, complete_x_prologues_sum, complete_y_prologues);
  incomplete_y_prologues.release();
  complete_x_prologues_sum.release();

  compute_complete_blocks(
      context, input, complete_x_prologues, complete_y_prologues, operation, output);
  complete_x_prologues.release();
  complete_y_prologues.release();
}

/* Computes the summed area table as a cascade of a horizontal summing pass followed by a vertical
 * summing pass. */
static void summed_area_table_cpu(Result &input,
                                  Result &output,
                                  SummedAreaTableOperation operation)
{
  output.allocate_texture(input.domain());

  /* Horizontal summing pass. */
  const int2 size = input.domain().size;
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange range_y) {
    for (const int y : range_y) {
      float4 accumulated_color = float4(0.0f);
      for (const int x : IndexRange(size.x)) {
        const int2 texel = int2(x, y);
        const float4 color = float4(input.load_pixel<Color>(texel));
        accumulated_color += operation == SummedAreaTableOperation::Square ? color * color : color;
        output.store_pixel(texel, Color(accumulated_color));
      }
    }
  });

  /* Vertical summing pass. */
  threading::parallel_for(IndexRange(size.x), 1, [&](const IndexRange range_x) {
    for (const int x : range_x) {
      float4 accumulated_color = float4(0.0f);
      for (const int y : IndexRange(size.y)) {
        const int2 texel = int2(x, y);
        const float4 color = float4(output.load_pixel<Color>(texel));
        accumulated_color += color;
        output.store_pixel(texel, Color(accumulated_color));
      }
    }
  });
}

void summed_area_table(Context &context,
                       Result &input,
                       Result &output,
                       SummedAreaTableOperation operation)
{
  if (context.use_gpu()) {
    summed_area_table_gpu(context, input, output, operation);
  }
  else {
    summed_area_table_cpu(input, output, operation);
  }
}

}  // namespace blender::compositor
