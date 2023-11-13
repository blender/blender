/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

/* OCIOMain will be dynamically generated in the OCIOColorSpaceConversionShader class and appended
 * at the end of this file, so forward declare it. Such forward declarations are not supported nor
 * needed on Metal. */
#if !defined(GPU_METAL)
vec4 OCIOMain(vec4 inPixel);
#endif

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  imageStore(output_img, texel, OCIOMain(texture_load(input_tx, texel)));
}
