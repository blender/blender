/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

void node_composite_color_correction(float4 color,
                                     float mask,
                                     float master_saturation,
                                     float master_contrast,
                                     float master_gamma,
                                     float master_gain,
                                     float master_offset,
                                     float highlights_saturation,
                                     float highlights_contrast,
                                     float highlights_gamma,
                                     float highlights_gain,
                                     float highlights_offset,
                                     float midtones_saturation,
                                     float midtones_contrast,
                                     float midtones_gamma,
                                     float midtones_gain,
                                     float midtones_offset,
                                     float shadows_saturation,
                                     float shadows_contrast,
                                     float shadows_gamma,
                                     float shadows_gain,
                                     float shadows_offset,
                                     float start_midtones,
                                     float end_midtones,
                                     float apply_on_red,
                                     float apply_on_green,
                                     float apply_on_blue,
                                     const float3 luminance_coefficients,
                                     out float4 result)
{
  constexpr float margin = 0.10f;
  constexpr float margin_divider = 0.5f / margin;
  float level = (color.r + color.g + color.b) / 3.0f;
  float level_shadows = 0.0f;
  float level_midtones = 0.0f;
  float level_highlights = 0.0f;
  if (level < (start_midtones - margin)) {
    level_shadows = 1.0f;
  }
  else if (level < (start_midtones + margin)) {
    level_midtones = ((level - start_midtones) * margin_divider) + 0.5f;
    level_shadows = 1.0f - level_midtones;
  }
  else if (level < (end_midtones - margin)) {
    level_midtones = 1.0f;
  }
  else if (level < (end_midtones + margin)) {
    level_highlights = ((level - end_midtones) * margin_divider) + 0.5f;
    level_midtones = 1.0f - level_highlights;
  }
  else {
    level_highlights = 1.0f;
  }

  float contrast = level_shadows * shadows_contrast;
  contrast += level_midtones * midtones_contrast;
  contrast += level_highlights * highlights_contrast;
  contrast *= master_contrast;
  float saturation = level_shadows * shadows_saturation;
  saturation += level_midtones * midtones_saturation;
  saturation += level_highlights * highlights_saturation;
  saturation *= master_saturation;
  float gamma = level_shadows * shadows_gamma;
  gamma += level_midtones * midtones_gamma;
  gamma += level_highlights * highlights_gamma;
  gamma *= master_gamma;
  float gain = level_shadows * shadows_gain;
  gain += level_midtones * midtones_gain;
  gain += level_highlights * highlights_gain;
  gain *= master_gain;
  float offset = level_shadows * shadows_offset;
  offset += level_midtones * midtones_offset;
  offset += level_highlights * highlights_offset;
  offset += master_offset;

  float inverse_gamma = 1.0f / gamma;
  float luma = get_luminance(color.rgb, luminance_coefficients);

  float3 corrected = luma + saturation * (color.rgb - luma);
  corrected = 0.5f + (corrected - 0.5f) * contrast;
  corrected = fallback_pow(corrected * gain + offset, inverse_gamma, corrected);
  corrected = mix(color.rgb, corrected, min(mask, 1.0f));

  float3 enabled_channels = float3(apply_on_red, apply_on_green, apply_on_blue);
  result.rgb = mix(corrected, color.rgb, equal(enabled_channels, float3(0.0f)));
  result.a = color.a;
}
