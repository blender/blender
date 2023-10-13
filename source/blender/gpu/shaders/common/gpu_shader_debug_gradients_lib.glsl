/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * For debugging purpose mainly.
 * From https://www.shadertoy.com/view/4dsSzr
 * By Morgan McGuire @morgan3d, http://graphicscodex.com
 * Reuse permitted under the BSD license.
 */
vec3 neon_gradient(float t)
{
  float tt = abs(0.43 - t) * 1.7;
  return clamp(vec3(t * 1.3 + 0.1, tt * tt, (1.0 - t) * 1.7), 0.0, 1.0);
}
vec3 heatmap_gradient(float t)
{
  float a = pow(t, 1.5) * 0.8 + 0.2;
  float b = smoothstep(0.0, 0.35, t) + t * 0.5;
  float c = smoothstep(0.5, 1.0, t);
  float d = max(1.0 - t * 1.7, t * 7.0 - 6.0);
  return clamp(a * vec3(b, c, d), vec3(0.0), vec3(1.0));
}
vec3 hue_gradient(float t)
{
  vec3 p = abs(fract(t + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
  return (clamp(p - 1.0, 0.0, 1.0));
}
