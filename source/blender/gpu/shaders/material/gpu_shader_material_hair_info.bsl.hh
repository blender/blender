/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_common_hash.bsl.hh"
#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_hair_info(float hair_intercept,
                    float hair_length,
                    const ShadingData &sd,
                    float &is_strand,
                    float &out_intercept,
                    float &out_length,
                    float &thickness,
                    float3 &normal,
                    float &random)
{
  is_strand = float(sd.is_strand);
  out_intercept = hair_intercept;
  out_length = hair_length;
  thickness = sd.hair_diameter;
  normal = sd.curve_N;
  /* TODO: could be precomputed per strand instead. */
  random = wang_hash_noise(uint(sd.hair_strand_id));
}
