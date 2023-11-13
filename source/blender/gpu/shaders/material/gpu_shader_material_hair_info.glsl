/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)

void node_hair_info(float hair_length,
                    out float is_strand,
                    out float intercept,
                    out float out_length,
                    out float thickness,
                    out vec3 normal,
                    out float random)
{
  is_strand = float(g_data.is_strand);
  intercept = g_data.hair_time;
  thickness = g_data.hair_thickness;
  out_length = hair_length;
  normal = g_data.curve_N;
  /* TODO: could be precomputed per strand instead. */
  random = wang_hash_noise(uint(g_data.hair_strand_id));
}
