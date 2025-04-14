/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

/* OCIOMain will be dynamically generated in the OCIOColorSpaceConversionShader class and appended
 * at the end of this file, so forward declare it. Such forward declarations are not supported nor
 * needed on Metal. */
#if !defined(GPU_METAL)
float4 OCIOMain(float4 inPixel);
#endif

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  imageStore(output_img, texel, OCIOMain(texture_load(input_tx, texel)));
}
