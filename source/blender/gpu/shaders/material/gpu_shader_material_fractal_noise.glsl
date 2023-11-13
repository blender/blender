/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_material_noise.glsl)

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(float p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 15.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = snoise(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec2 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 15.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = snoise(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec3 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 15.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = snoise(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}

/* The fractal_noise functions are all exactly the same except for the input type. */
float fractal_noise(vec4 p, float octaves, float roughness, float lacunarity, bool normalize)
{
  float fscale = 1.0;
  float amp = 1.0;
  float maxamp = 0.0;
  float sum = 0.0;
  octaves = clamp(octaves, 0.0, 15.0);
  int n = int(octaves);
  for (int i = 0; i <= n; i++) {
    float t = snoise(fscale * p);
    sum += t * amp;
    maxamp += amp;
    amp *= clamp(roughness, 0.0, 1.0);
    fscale *= lacunarity;
  }
  float rmd = octaves - floor(octaves);
  if (rmd != 0.0) {
    float t = snoise(fscale * p);
    float sum2 = sum + t * amp;
    return normalize ? mix(0.5 * sum / maxamp + 0.5, 0.5 * sum2 / (maxamp + amp) + 0.5, rmd) :
                       mix(sum, sum2, rmd);
  }
  else {
    return normalize ? 0.5 * sum / maxamp + 0.5 : sum;
  }
}
