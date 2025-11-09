/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_pad.hh"

namespace blender::compositor {

static const char *get_shader_name(const ResultType type, const PaddingMethod padding_method)
{
  switch (padding_method) {
    case PaddingMethod::Zero:
      switch (type) {
        case ResultType::Color:
          return "compositor_pad_zero_float4";
        default:
          break;
      }
      break;
    case PaddingMethod::Extend:
      switch (type) {
        case ResultType::Float2:
          return "compositor_pad_extend_float2";
        case ResultType::Float:
          return "compositor_pad_extend_float";
        default:
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return "";
}

static void zero_pad_gpu(Context &context,
                         const Result &input,
                         Result &output,
                         const int2 size,
                         const PaddingMethod padding_method)
{
  gpu::Shader *shader = context.get_shader(get_shader_name(input.type(), padding_method));
  GPU_shader_bind(shader);

  GPU_shader_uniform_2iv(shader, "size", size);

  input.bind_as_texture(shader, "input_tx");

  Domain extended_domain = input.domain();
  extended_domain.size += size * 2;
  output.allocate_texture(extended_domain);
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, extended_domain.size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  output.unbind_as_image();
}

static void zero_pad_cpu(const Result &input,
                         Result &output,
                         const int2 size,
                         const PaddingMethod padding_method)
{
  Domain extended_domain = input.domain();
  extended_domain.size += size * 2;
  output.allocate_texture(extended_domain);

  switch (padding_method) {
    case PaddingMethod::Zero:
      switch (input.type()) {
        case ResultType::Color:
          parallel_for(extended_domain.size, [&](const int2 texel) {
            output.store_pixel(texel, input.load_pixel_zero<Color>(texel - size));
          });
          break;
        default:
          BLI_assert_unreachable();
      }
      break;
    case PaddingMethod::Extend:
      switch (input.type()) {
        case ResultType::Float:
          parallel_for(extended_domain.size, [&](const int2 texel) {
            output.store_pixel(texel, input.load_pixel_extended<float>(texel - size));
          });
          break;
        case ResultType::Float2:
          parallel_for(extended_domain.size, [&](const int2 texel) {
            output.store_pixel(texel, input.load_pixel_extended<float2>(texel - size));
          });
          break;
        default:
          BLI_assert_unreachable();
      }
      break;
  }
}

void pad(Context &context,
         const Result &input,
         Result &output,
         const int2 size,
         const PaddingMethod padding_method)
{
  if (input.is_single_value()) {
    output.share_data(input);
    return;
  }

  if (context.use_gpu()) {
    zero_pad_gpu(context, input, output, size, padding_method);
  }
  else {
    zero_pad_cpu(input, output, size, padding_method);
  }
}

}  // namespace blender::compositor
