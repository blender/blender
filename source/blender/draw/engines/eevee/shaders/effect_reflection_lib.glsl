/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on:
 * "Stochastic Screen Space Reflections"
 * by Tomasz Stachowiak.
 * https://www.ea.com/frostbite/news/stochastic-screen-space-reflections
 * and
 * "Stochastic all the things: raytracing in hybrid real-time rendering"
 * by Tomasz Stachowiak.
 * https://media.contentapi.ea.com/content/dam/ea/seed/presentations/dd18-seed-raytracing-in-hybrid-real-time-rendering.pdf
 */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

struct HitData {
  /** Hit direction scaled by intersection time. */
  vec3 hit_dir;
  /** Screen space [0..1] depth of the reflection hit position, or -1.0 for planar reflections. */
  float hit_depth;
  /** Inverse probability of ray spawning in this direction. */
  float ray_pdf_inv;
  /** True if ray has hit valid geometry. */
  bool is_hit;
  /** True if ray was generated from a planar reflection probe. */
  bool is_planar;
};

void encode_hit_data(HitData data, vec3 hit_sP, vec3 vP, out vec4 hit_data, out float hit_depth)
{
  vec3 hit_vP = get_view_space_from_depth(hit_sP.xy, hit_sP.z);
  hit_data.xyz = hit_vP - vP;
  hit_depth = data.is_planar ? -1.0 : hit_sP.z;
  /* Record 1.0 / pdf to reduce the computation in the resolve phase. */
  /* Encode hit validity in sign. */
  hit_data.w = data.ray_pdf_inv * ((data.is_hit) ? 1.0 : -1.0);
}

HitData decode_hit_data(vec4 hit_data, float hit_depth)
{
  HitData data;
  data.hit_dir.xyz = hit_data.xyz;
  data.hit_depth = hit_depth;
  data.is_planar = (hit_depth == -1.0);
  data.ray_pdf_inv = abs(hit_data.w);
  data.is_hit = (hit_data.w > 0.0);
  return data;
}

/* Blue noise categorized into 4 sets of samples.
 * See "Stochastic all the things" presentation slide 32-37. */
#define resolve_samples_count 9
