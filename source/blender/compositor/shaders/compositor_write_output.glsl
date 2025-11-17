/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_write_output_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_write_output)

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 output_texel = texel + lower_bound;
  if (any(greaterThan(output_texel, upper_bound))) {
    return;
  }

  imageStore(output_img, texel + lower_bound, texture_load(input_tx, texel));
}
