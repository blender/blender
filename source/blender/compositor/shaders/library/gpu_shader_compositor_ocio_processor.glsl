/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_ocio_infos.hh"

#include "gpu_shader_compositor_texture_utilities.glsl"

#include "gpu_shader_compositor_ocio_processor_lib.glsl"

COMPUTE_SHADER_CREATE_INFO(OCIO_Processor)

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  imageStore(output_img, texel, OCIOMain(texture_load(input_tx, texel)));
}
