/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_extract_alpha.hh"

namespace blender::compositor {

static void extract_alpha_gpu(Context &context, Result &input, Result &output)
{
  gpu::Shader *shader = context.get_shader("compositor_convert_color_to_alpha");
  GPU_shader_bind(shader);

  input.bind_as_texture(shader, "input_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  input.unbind_as_texture();
  output.unbind_as_image();
}

static void extract_alpha_cpu(Result &input, Result &output)
{
  output.allocate_texture(input.domain());
  parallel_for(input.domain().size, [&](const int2 texel) {
    output.store_pixel(texel, input.load_pixel<Color>(texel).a);
  });
}

void extract_alpha(Context &context, Result &input, Result &output)
{
  if (context.use_gpu()) {
    extract_alpha_gpu(context, input, output);
  }
  else {
    extract_alpha_cpu(input, output);
  }
}

}  // namespace blender::compositor
