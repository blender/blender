/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void node_composite_color_correction(vec4 color,
                                     float mask,
                                     const vec3 enabled_channels,
                                     float start_midtones,
                                     float end_midtones,
                                     float master_saturation,
                                     float master_contrast,
                                     float master_gamma,
                                     float master_gain,
                                     float master_lift,
                                     float shadows_saturation,
                                     float shadows_contrast,
                                     float shadows_gamma,
                                     float shadows_gain,
                                     float shadows_lift,
                                     float midtones_saturation,
                                     float midtones_contrast,
                                     float midtones_gamma,
                                     float midtones_gain,
                                     float midtones_lift,
                                     float highlights_saturation,
                                     float highlights_contrast,
                                     float highlights_gamma,
                                     float highlights_gain,
                                     float highlights_lift,
                                     const vec3 luminance_coefficients,
                                     out vec4 result)
{
  const float margin = 0.10;
  const float margin_divider = 0.5 / margin;
  float level = (color.r + color.g + color.b) / 3.0;
  float level_shadows = 0.0;
  float level_midtones = 0.0;
  float level_highlights = 0.0;
  if (level < (start_midtones - margin)) {
    level_shadows = 1.0;
  }
  else if (level < (start_midtones + margin)) {
    level_midtones = ((level - start_midtones) * margin_divider) + 0.5;
    level_shadows = 1.0 - level_midtones;
  }
  else if (level < (end_midtones - margin)) {
    level_midtones = 1.0;
  }
  else if (level < (end_midtones + margin)) {
    level_highlights = ((level - end_midtones) * margin_divider) + 0.5;
    level_midtones = 1.0 - level_highlights;
  }
  else {
    level_highlights = 1.0;
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
  float lift = level_shadows * shadows_lift;
  lift += level_midtones * midtones_lift;
  lift += level_highlights * highlights_lift;
  lift += master_lift;

  float inverse_gamma = 1.0 / gamma;
  float luma = get_luminance(color.rgb, luminance_coefficients);

  vec3 corrected = luma + saturation * (color.rgb - luma);
  corrected = 0.5 + (corrected - 0.5) * contrast;
  corrected = fallback_pow(corrected * gain + lift, inverse_gamma, corrected);
  corrected = mix(color.rgb, corrected, min(mask, 1.0));

  result.rgb = mix(corrected, color.rgb, equal(enabled_channels, vec3(0.0)));
  result.a = color.a;
}
