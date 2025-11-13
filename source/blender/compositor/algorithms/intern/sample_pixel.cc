/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"

#include "COM_algorithm_sample_pixel.hh"

namespace blender::compositor {

static char const *get_pixel_sampler_shader_name(const Interpolation &interpolation)
{
  switch (interpolation) {
    case Interpolation::Anisotropic:
    case Interpolation::Bicubic:
      return "compositor_sample_pixel_bicubic";
    case Interpolation::Bilinear:
    case Interpolation::Nearest:
      return "compositor_sample_pixel";
  }
  BLI_assert_unreachable();
  return "compositor_sample_pixel";
}

static Color sample_pixel_gpu(Context &context,
                              const Result &input,
                              const Interpolation &interpolation,
                              const ExtensionMode &extension_mode_x,
                              const ExtensionMode &extension_mode_y,
                              const float2 coordinates)
{
  gpu::Shader *shader = context.get_shader(get_pixel_sampler_shader_name(interpolation));
  GPU_shader_bind(shader);

  GPU_shader_uniform_2fv(shader, "coordinates_u", coordinates);

  Result output = context.create_result(input.type());
  output.allocate_texture(int2(1));

  if (interpolation == Interpolation::Anisotropic) {
    GPU_texture_anisotropic_filter(input, true);
    GPU_texture_mipmap_mode(input, true, true);
  }
  else {
    const bool use_bilinear = ELEM(interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
    GPU_texture_filter_mode(input, use_bilinear);
  }

  GPU_texture_extend_mode_x(input, map_extension_mode_to_extend_mode(extension_mode_x));
  GPU_texture_extend_mode_y(input, map_extension_mode_to_extend_mode(extension_mode_y));

  input.bind_as_texture(shader, "input_tx");
  output.bind_as_image(shader, "output_img");

  GPU_compute_dispatch(shader, 1, 1, 1);

  input.unbind_as_texture();
  output.unbind_as_image();
  GPU_shader_unbind();

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float *pixel = static_cast<float *>(GPU_texture_read(output, GPU_DATA_FLOAT, 0));
  output.release();

  Color sampled_color = Color(pixel);
  MEM_freeN(pixel);

  return sampled_color;
}

static Color sample_pixel_cpu(const Result &input,
                              const Interpolation &interpolation,
                              const ExtensionMode &extension_mode_x,
                              const ExtensionMode &extension_mode_y,
                              const float2 coordinates)
{
  return input.sample<Color>(coordinates, interpolation, extension_mode_x, extension_mode_y);
}

/* Samples a pixel from a result. */
Color sample_pixel(Context &context,
                   const Result &input,
                   const Interpolation &interpolation,
                   const ExtensionMode &extension_mode_x,
                   const ExtensionMode &extension_mode_y,
                   const float2 coordinates)
{
  BLI_assert(input.type() == ResultType::Color);

  if (input.is_single_value()) {
    return input.get_single_value<Color>();
  }

  if (context.use_gpu()) {
    return sample_pixel_gpu(
        context, input, interpolation, extension_mode_x, extension_mode_y, coordinates);
  }

  return sample_pixel_cpu(input, interpolation, extension_mode_x, extension_mode_y, coordinates);
}

}  // namespace blender::compositor
