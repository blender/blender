/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

#include "eevee_octahedron_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

SphereProbePixelArea reinterpret_as_write_coord(int4 packed_coord)
{
  SphereProbePixelArea unpacked;
  unpacked.offset = packed_coord.xy;
  unpacked.extent = packed_coord.z;
  unpacked.layer = packed_coord.w;
  return unpacked;
}

SphereProbeUvArea reinterpret_as_atlas_coord(int4 packed_coord)
{
  SphereProbeUvArea unpacked;
  unpacked.offset = intBitsToFloat(packed_coord.xy);
  unpacked.scale = intBitsToFloat(packed_coord.z);
  unpacked.layer = intBitsToFloat(packed_coord.w);
  return unpacked;
}

/* local_texel is the texel coordinate inside the probe area [0..texel_area.extent) range.
 * Returned vector is not normalized. */
float3 sphere_probe_texel_to_direction(float2 local_texel,
                                       SphereProbePixelArea texel_area,
                                       SphereProbeUvArea uv_area,
                                       out float2 sampling_uv)
{
  /* UV in sampling area. No half pixel bias to texel as the octahedral map edges area lined up
   * with texel center. Note that we don't use the last row & column of pixel, hence the -2 instead
   * of -1. See sphere_probe_miplvl_scale_bias. */
  sampling_uv = local_texel / float2(texel_area.extent - 2);
  /* Direction in world space. */
  return octahedral_uv_to_direction(sampling_uv);
}

/* local_texel is the texel coordinate inside the probe area [0..texel_area.extent) range.
 * Returned vector is not normalized. */
float3 sphere_probe_texel_to_direction(float2 local_texel,
                                       SphereProbePixelArea texel_area,
                                       SphereProbeUvArea uv_area)
{
  float2 sampling_uv_unused;
  return sphere_probe_texel_to_direction(local_texel, texel_area, uv_area, sampling_uv_unused);
}

/* Apply correct bias and scale for the given level of detail. */
float2 sphere_probe_miplvl_scale_bias(float mip_lvl, SphereProbeUvArea uv_area, float2 uv)
{
  /* Add 0.5 to avoid rounding error. */
  int mip_0_res = int(float(SPHERE_PROBE_ATLAS_RES) * uv_area.scale + 0.5f);
  float mip_lvl_res = float(mip_0_res >> int(mip_lvl));
  float mip_lvl_res_inv = 1.0f / mip_lvl_res;
  /* We place texel centers at the edges of the octahedron, to avoid artifacts caused by
   * interpolating across the edges.
   * The first pixel scaling aligns all the border edges (half pixel border).
   * The second pixel scaling aligns the center edges (odd number of pixel). */
  float scale = (mip_lvl_res - 2.0f) * mip_lvl_res_inv;
  float offset = 0.5f * mip_lvl_res_inv;
  return uv * scale + offset;
}

void sphere_probe_direction_to_uv(float3 L,
                                  float lod_min,
                                  float lod_max,
                                  SphereProbeUvArea uv_area,
                                  out float2 altas_uv_min,
                                  out float2 altas_uv_max)
{
  float2 octahedral_uv = octahedral_uv_from_direction(L);
  /* We use a custom per mip level scaling and bias. This avoid some projection artifact and
   * padding border waste. But we need to do the mipmap interpolation ourself. */
  float2 local_uv_min = sphere_probe_miplvl_scale_bias(lod_min, uv_area, octahedral_uv);
  float2 local_uv_max = sphere_probe_miplvl_scale_bias(lod_max, uv_area, octahedral_uv);
  /* Remap into atlas location. */
  altas_uv_min = local_uv_min * uv_area.scale + uv_area.offset;
  altas_uv_max = local_uv_max * uv_area.scale + uv_area.offset;
}

/* Single mip variant. */
float2 sphere_probe_direction_to_uv(float3 L, float lod, SphereProbeUvArea uv_area)
{
  float2 altas_uv_min, altas_uv_max_unused;
  sphere_probe_direction_to_uv(L, lod, 0.0f, uv_area, altas_uv_min, altas_uv_max_unused);
  return altas_uv_min;
}

float sphere_probe_roughness_to_mix_fac(float roughness)
{
  constexpr float scale = 1.0f /
                          (SPHERE_PROBE_MIX_END_ROUGHNESS - SPHERE_PROBE_MIX_START_ROUGHNESS);
  constexpr float bias = scale * SPHERE_PROBE_MIX_START_ROUGHNESS;
  return square(saturate(roughness * scale - bias));
}

/* Input roughness is linear roughness (UI roughness). */
float sphere_probe_roughness_to_lod(float roughness)
{
  /* From "Moving Frostbite to Physically Based Rendering 3.0" eq 53. */
  float ratio = saturate(roughness / SPHERE_PROBE_MIP_MAX_ROUGHNESS);
  float ratio_sqrt = sqrt_fast(ratio);
  /* Mix with linear to avoid mip 1 being too sharp. */
  float mip_ratio = mix(ratio, ratio_sqrt, 0.4f);
  return mip_ratio * float(SPHERE_PROBE_MIPMAP_LEVELS - 1);
}

/* Return linear roughness (UI roughness). */
float sphere_probe_lod_to_roughness(float lod)
{
  /* Inverse of sphere_probe_roughness_to_lod. */
  float mip_ratio = lod / float(SPHERE_PROBE_MIPMAP_LEVELS - 1);
  float a = mip_ratio;
  constexpr float b = 0.6f; /* Factor of ratio. */
  constexpr float c = 0.4f; /* Factor of ratio_sqrt. */
  float b2 = square(b);
  float c2 = square(c);
  float c4 = square(c2);
  /* In wolfram alpha we trust. */
  float ratio = (-sqrt(4.0f * a * b * c2 + c4) + 2.0f * a * b + c2) / (2.0f * b2);
  return ratio * SPHERE_PROBE_MIP_MAX_ROUGHNESS;
}
