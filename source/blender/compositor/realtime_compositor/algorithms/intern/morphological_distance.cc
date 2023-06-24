/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_morphological_distance.hh"

namespace blender::realtime_compositor {

static const char *get_shader_name(int distance)
{
  if (distance > 0) {
    return "compositor_morphological_distance_dilate";
  }
  return "compositor_morphological_distance_erode";
}

void morphological_distance(Context &context, Result &input, Result &output, int distance)
{
  GPUShader *shader = context.shader_manager().get(get_shader_name(distance));
  GPU_shader_bind(shader);

  /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
  GPU_shader_uniform_1i(shader, "radius", math::abs(distance));

  input.bind_as_texture(shader, "input_tx");

  output.allocate_texture(input.domain());
  output.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, input.domain().size);

  GPU_shader_unbind();
  output.unbind_as_image();
  input.unbind_as_texture();
}

}  // namespace blender::realtime_compositor
