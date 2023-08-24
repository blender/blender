/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* From the paper "Hashed Alpha Testing" by Chris Wyman and Morgan McGuire. */
float transparency_hash(vec2 a)
{
  return fract(1e4 * sin(17.0 * a.x + 0.1 * a.y) * (0.1 + abs(sin(13.0 * a.y + a.x))));
}

float transparency_hash_3d(vec3 a)
{
  return transparency_hash(vec2(transparency_hash(a.xy), a.z));
}

float transparency_hashed_alpha_threshold(float hash_scale, float hash_offset, vec3 P)
{
  /* Find the discretized derivatives of our coordinates. */
  float max_deriv = max(length(dFdx(P)), length(dFdy(P)));
  float pix_scale = 1.0 / (hash_scale * max_deriv);
  /* Find two nearest log-discretized noise scales. */
  float pix_scale_log = log2(pix_scale);
  vec2 pix_scales;
  pix_scales.x = exp2(floor(pix_scale_log));
  pix_scales.y = exp2(ceil(pix_scale_log));
  /* Compute alpha thresholds at our two noise scales. */
  vec2 alpha;
  alpha.x = transparency_hash_3d(floor(pix_scales.x * P));
  alpha.y = transparency_hash_3d(floor(pix_scales.y * P));
  /* Factor to interpolate lerp with. */
  float fac = fract(log2(pix_scale));
  /* Interpolate alpha threshold from noise at two scales. */
  float x = mix(alpha.x, alpha.y, fac);
  /* Pass into CDF to compute uniformly distributed threshold. */
  float a = min(fac, 1.0 - fac);
  float one_a = 1.0 - a;
  float denom = 1.0 / (2 * a * one_a);
  float one_x = (1 - x);
  vec3 cases = vec3((x * x) * denom, (x - 0.5 * a) / one_a, 1.0 - (one_x * one_x * denom));
  /* Find our final, uniformly distributed alpha threshold. */
  float threshold = (x < one_a) ? ((x < a) ? cases.x : cases.y) : cases.z;
  /* Jitter the threshold for TAA accumulation. */
  threshold = fract(threshold + hash_offset);
  /* Avoids threshold == 0. */
  threshold = clamp(threshold, 1.0e-6, 1.0);
  return threshold;
}
