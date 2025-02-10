/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_gamma_correct.hh"

namespace blender::compositor {

static void gamma_correct_gpu(Context &context, const Result &input, Result &output)
{
  GPUShader *shader = context.get_shader("compositor_gamma_correct");
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  output.unbind_as_image();
}

static void gamma_correct_cpu(const Result &input, Result &output)
{
  output.allocate_texture(input.domain());
  parallel_for(input.domain().size, [&](const int2 texel) {
    float4 color = input.load_pixel<float4>(texel);
    float alpha = color.w > 0.0f ? color.w : 1.0f;
    float3 corrected_color = math::square(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
    output.store_pixel(texel, float4(corrected_color, color.w));
  });
}

void gamma_correct(Context &context, const Result &input, Result &output)
{
  if (context.use_gpu()) {
    gamma_correct_gpu(context, input, output);
  }
  else {
    gamma_correct_cpu(input, output);
  }
}

static void gamma_uncorrect_gpu(Context &context, const Result &input, Result &output)
{
  GPUShader *shader = context.get_shader("compositor_gamma_uncorrect");
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  output.unbind_as_image();
}

static void gamma_uncorrect_cpu(const Result &input, Result &output)
{
  output.allocate_texture(input.domain());
  parallel_for(input.domain().size, [&](const int2 texel) {
    float4 color = input.load_pixel<float4>(texel);
    float alpha = color.w > 0.0f ? color.w : 1.0f;
    float3 corrected_color = math::sqrt(math::max(color.xyz() / alpha, float3(0.0f))) * alpha;
    output.store_pixel(texel, float4(corrected_color, color.w));
  });
}

void gamma_uncorrect(Context &context, const Result &input, Result &output)
{
  if (context.use_gpu()) {
    gamma_uncorrect_gpu(context, input, output);
  }
  else {
    gamma_uncorrect_cpu(input, output);
  }
}

}  // namespace blender::compositor
