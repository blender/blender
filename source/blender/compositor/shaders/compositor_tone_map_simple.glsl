/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Tone mapping based on equation (3) from Reinhard, Erik, et al. "Photographic tone reproduction
 * for digital images." Proceedings of the 29th annual conference on Computer graphics and
 * interactive techniques. 2002. */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 input_color = texture_load(input_tx, texel);

  /* Equation (2) from Reinhard's 2002 paper. */
  float4 scaled_color = input_color * luminance_scale;

  /* Equation (3) from Reinhard's 2002 paper, but with the 1 replaced with the blend factor for
   * more flexibility. See ToneMapOperation::compute_luminance_scale_blend_factor. */
  float4 denominator = luminance_scale_blend_factor + scaled_color;
  float4 tone_mapped_color = safe_divide(scaled_color, denominator);

  if (inverse_gamma != 0.0f) {
    tone_mapped_color = pow(max(tone_mapped_color, float4(0.0f)), float4(inverse_gamma));
  }

  imageStore(output_img, texel, float4(tone_mapped_color.rgb, input_color.a));
}
