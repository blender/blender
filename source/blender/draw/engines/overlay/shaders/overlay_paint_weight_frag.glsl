/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float contours(float value, float steps, float width_px, float max_rel_width, float gradient)
{
  /* Minimum visible and minimum full strength line width in screen space for fade out. */
  const float min_width_px = 1.3, fade_width_px = 2.3;
  /* Line is thinner towards the increase in the weight gradient by this factor. */
  const float hi_bias = 2.0;

  /* Don't draw lines at 0 or 1. */
  float rel_value = value * steps;

  if (rel_value < 0.5 || rel_value > steps - 0.5) {
    return 0.0;
  }

  /* Check if completely invisible due to fade out. */
  float rel_gradient = gradient * steps;
  float rel_min_width = min_width_px * rel_gradient;

  if (max_rel_width <= rel_min_width) {
    return 0.0;
  }

  /* Main shape of the line, accounting for width bias and maximum weight space width. */
  float rel_width = width_px * rel_gradient;

  float offset = fract(rel_value + 0.5) - 0.5;

  float base_alpha = 1.0 - max(offset * hi_bias, -offset) / min(max_rel_width, rel_width);

  /* Line fadeout when too thin in screen space. */
  float rel_fade_width = fade_width_px * rel_gradient;

  float fade_alpha = (max_rel_width - rel_min_width) / (rel_fade_width - rel_min_width);

  return clamp(base_alpha, 0.0, 1.0) * clamp(fade_alpha, 0.0, 1.0);
}

vec4 contour_grid(float weight, float weight_gradient)
{
  /* Fade away when the gradient is too low to avoid big fills and noise. */
  float flt_eps = max(1e-8, 1e-6 * weight);

  if (weight_gradient <= flt_eps) {
    return vec4(0.0);
  }

  /* Three levels of grid lines */
  float grid10 = contours(weight, 10.0, 5.0, 0.3, weight_gradient);
  float grid100 = contours(weight, 100.0, 3.5, 0.35, weight_gradient) * 0.6;
  float grid1000 = contours(weight, 1000.0, 2.5, 0.4, weight_gradient) * 0.25;

  /* White lines for 0.1 and 0.01, and black for 0.001 */
  vec4 grid = vec4(1.0) * max(grid10, grid100);

  grid.a = max(grid.a, grid1000);

  return grid * clamp((weight_gradient - flt_eps) / flt_eps, 0.0, 1.0);
}

vec4 apply_color_fac(vec4 color_in)
{
  vec4 color = color_in;
  color.rgb = max(vec3(0.005), color_in.rgb) * color_fac;
  return color;
}

void main()
{
  float alert = weight_interp.y;
  vec4 color;

  /* Missing vertex group alert color. Uniform in practice. */
  if (alert > 1.1) {
    color = apply_color_fac(colorVertexMissingData);
  }
  /* Weights are available */
  else {
    float weight = weight_interp.x;
    vec4 weight_color = texture(colorramp, weight, 0);
    weight_color = apply_color_fac(weight_color);

    /* Contour display */
    if (drawContours) {
      /* This must be executed uniformly for all fragments */
      float weight_gradient = length(vec2(dFdx(weight), dFdy(weight)));

      vec4 grid = contour_grid(weight, weight_gradient);

      weight_color = grid + weight_color * (1 - grid.a);
    }

    /* Zero weight alert color. Nonlinear blend to reduce impact. */
    vec4 color_unreferenced = apply_color_fac(colorVertexUnreferenced);
    color = mix(weight_color, color_unreferenced, alert * alert);
  }

  fragColor = vec4(color.rgb, opacity);
}
