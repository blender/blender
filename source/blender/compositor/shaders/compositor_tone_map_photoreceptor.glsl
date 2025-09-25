/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Tone mapping based on equation (1) and the trilinear interpolation between equations (6) and (7)
 * from Reinhard, Erik, and Kate Devlin. "Dynamic range reduction inspired by photoreceptor
 * physiology." IEEE transactions on visualization and computer graphics 11.1 (2005): 13-24. */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 input_color = texture_load(input_tx, texel);
  float input_luminance = dot(input_color.rgb, luminance_coefficients);

  /* Trilinear interpolation between equations (6) and (7) from Reinhard's 2005 paper. */
  float4 local_adaptation_level = mix(float4(input_luminance), input_color, chromatic_adaptation);
  float4 adaptation_level = mix(global_adaptation_level, local_adaptation_level, light_adaptation);

  /* Equation (1) from Reinhard's 2005 paper, assuming Vmax is 1. */
  float4 semi_saturation = pow(intensity * adaptation_level, float4(contrast));
  float4 tone_mapped_color = safe_divide(input_color, input_color + semi_saturation);

  imageStore(output_img, texel, float4(tone_mapped_color.rgb, input_color.a));
}
