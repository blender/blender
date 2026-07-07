/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_paint_weight)

float contours(float value, float steps, float width_px, float max_rel_width, float gradient)
{
  /* Minimum visible and minimum full strength line width in screen space for fade out. */
  constexpr float min_width_px = 1.3f, fade_width_px = 2.3f;
  /* Line is thinner towards the increase in the weight gradient by this factor. */
  constexpr float hi_bias = 2.0f;

  /* Don't draw lines at 0 or 1. */
  float rel_value = value * steps;

  if (rel_value < 0.5f || rel_value > steps - 0.5f) {
    return 0.0f;
  }

  /* Check if completely invisible due to fade out. */
  float rel_gradient = gradient * steps;
  float rel_min_width = min_width_px * rel_gradient;

  if (max_rel_width <= rel_min_width) {
    return 0.0f;
  }

  /* Main shape of the line, accounting for width bias and maximum weight space width. */
  float rel_width = width_px * rel_gradient;

  float offset = fract(rel_value + 0.5f) - 0.5f;

  float base_alpha = 1.0f - max(offset * hi_bias, -offset) / min(max_rel_width, rel_width);

  /* Line fade-out when too thin in screen-space. */
  float rel_fade_width = fade_width_px * rel_gradient;

  float fade_alpha = (max_rel_width - rel_min_width) / (rel_fade_width - rel_min_width);

  return clamp(base_alpha, 0.0f, 1.0f) * clamp(fade_alpha, 0.0f, 1.0f);
}

float4 contour_grid(float weight, float weight_gradient)
{
  /* Fade away when the gradient is too low to avoid big fills and noise. */
  float flt_eps = max(1e-8f, 1e-6f * weight);

  if (weight_gradient <= flt_eps) {
    return float4(0.0f);
  }

  /* Three levels of grid lines */
  float grid10 = contours(weight, 10.0f, 5.0f, 0.3f, weight_gradient);
  float grid100 = contours(weight, 100.0f, 3.5f, 0.35f, weight_gradient) * 0.6f;
  float grid1000 = contours(weight, 1000.0f, 2.5f, 0.4f, weight_gradient) * 0.25f;

  /* White lines for 0.1 and 0.01, and black for 0.001. */
  float4 grid = float4(1.0f) * max(grid10, grid100);

  grid.a = max(grid.a, grid1000);

  return grid * clamp((weight_gradient - flt_eps) / flt_eps, 0.0f, 1.0f);
}

float4 apply_color_fac(float4 color_in)
{
  float4 color = color_in;
  color.rgb = max(float3(0.005f), color_in.rgb) * color_fac;
  return color;
}

void main()
{
  float alert = weight_interp.y;
  float4 color;

  /* Missing vertex group alert color. Uniform in practice. */
  if (alert > 1.1f) {
    color = apply_color_fac(theme.colors.vert_missing_data);
  }
  /* Weights are available */
  else {
    float weight = weight_interp.x;
    float4 weight_color = texture(colorramp, weight);
    weight_color = apply_color_fac(weight_color);

    /* Contour display */
    if (draw_contours) {
      /* This must be executed uniformly for all fragments */
      float weight_gradient = length(float2(gpu_dfdx(weight), gpu_dfdy(weight)));

      float4 grid = contour_grid(weight, weight_gradient);

      weight_color = grid + weight_color * (1 - grid.a);
    }

    /* Zero weight alert color. Nonlinear blend to reduce impact. */
    float4 color_unreferenced = apply_color_fac(theme.colors.vert_unreferenced);
    color = mix(weight_color, color_unreferenced, alert * alert);
  }

  frag_color = float4(color.rgb, opacity);
  line_output = float4(0.0f);
}
