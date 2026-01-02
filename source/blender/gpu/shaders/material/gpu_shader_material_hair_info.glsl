/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

[[node]]
void node_hair_info(float hair_intercept,
                    float hair_length,
                    float &is_strand,
                    float &out_intercept,
                    float &out_length,
                    float &thickness,
                    float3 &normal,
                    float &random)
{
  is_strand = float(g_data.is_strand);
  out_intercept = hair_intercept;
  out_length = hair_length;
  thickness = g_data.hair_diameter;
  normal = g_data.curve_N;
  /* TODO: could be precomputed per strand instead. */
  random = wang_hash_noise(uint(g_data.hair_strand_id));
}
