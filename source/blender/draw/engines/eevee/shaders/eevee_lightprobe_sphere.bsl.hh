/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_math_geom_lib.glsl"
#include "eevee_defines.hh"
#include "eevee_lightprobe_shared.hh"
#include "eevee_octahedron_lib.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee {

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

}  // namespace eevee

namespace eevee::lightprobe::sphere {

/* local_texel is the texel coordinate inside the probe area [0..texel_area.extent) range.
 * Returned vector is not normalized. */
float3 texel_to_direction(float2 local_texel, SphereProbePixelArea texel_area, float2 &sampling_uv)
{
  /* UV in sampling area. No half pixel bias to texel as the octahedral map edges area lined up
   * with texel center. Note that we don't use the last row & column of pixel, hence the -2 instead
   * of -1. See miplvl_scale_bias. */
  sampling_uv = local_texel / float2(texel_area.extent - 2);
  /* Direction in world space. */
  return octahedral_uv_to_direction(sampling_uv);
}

/* local_texel is the texel coordinate inside the probe area [0..texel_area.extent) range.
 * Returned vector is not normalized. */
float3 texel_to_direction(float2 local_texel, SphereProbePixelArea texel_area)
{
  float2 sampling_uv_unused;
  return texel_to_direction(local_texel, texel_area, sampling_uv_unused);
}

/* Apply correct bias and scale for the given level of detail. */
float2 miplvl_scale_bias(float mip_lvl, SphereProbeUvArea uv_area, float2 uv)
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

void direction_to_uv(float3 L,
                     float lod_min,
                     float lod_max,
                     SphereProbeUvArea uv_area,
                     float2 &altas_uv_min,
                     float2 &altas_uv_max)
{
  float2 octahedral_uv = octahedral_uv_from_direction(L);
  /* We use a custom per mip level scaling and bias. This avoid some projection artifact and
   * padding border waste. But we need to do the mipmap interpolation ourself. */
  float2 local_uv_min = miplvl_scale_bias(lod_min, uv_area, octahedral_uv);
  float2 local_uv_max = miplvl_scale_bias(lod_max, uv_area, octahedral_uv);
  /* Remap into atlas location. */
  altas_uv_min = local_uv_min * uv_area.scale + uv_area.offset;
  altas_uv_max = local_uv_max * uv_area.scale + uv_area.offset;
}

/* Single mip variant. */
float2 direction_to_uv(float3 L, float lod, SphereProbeUvArea uv_area)
{
  float2 altas_uv_min, altas_uv_max_unused;
  direction_to_uv(L, lod, 0.0f, uv_area, altas_uv_min, altas_uv_max_unused);
  return altas_uv_min;
}

float roughness_to_mix_fac(float roughness)
{
  constexpr float scale = 1.0f /
                          (SPHERE_PROBE_MIX_END_ROUGHNESS - SPHERE_PROBE_MIX_START_ROUGHNESS);
  constexpr float bias = scale * SPHERE_PROBE_MIX_START_ROUGHNESS;
  return square(saturate(roughness * scale - bias));
}

/* Input roughness is linear roughness (UI roughness). */
float roughness_to_lod(float roughness)
{
  /* From "Moving Frostbite to Physically Based Rendering 3.0" eq 53. */
  float ratio = saturate(roughness / SPHERE_PROBE_MIP_MAX_ROUGHNESS);
  float ratio_sqrt = sqrt_fast(ratio);
  /* Mix with linear to avoid mip 1 being too sharp. */
  float mip_ratio = mix(ratio, ratio_sqrt, 0.4f);
  return mip_ratio * float(SPHERE_PROBE_MIPMAP_LEVELS - 1);
}

/* Return linear roughness (UI roughness). */
float lod_to_roughness(float lod)
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

/* Return the best parallax corrected ray direction from the probe center. */
float3 parallax_eval(SphereProbeData probe, float3 P, float3 L)
{
  bool is_world = (probe.influence_scale == 0.0f);
  if (is_world) {
    return L;
  }
  /* Correct reflection ray using parallax volume intersection. */
  float3 lP = float4(P, 1.0f) * probe.world_to_probe_transposed;
  float3 lL = (L * to_float3x3(probe.world_to_probe_transposed)) / probe.parallax_distance;

  float dist = (probe.parallax_shape == SHAPE_ELIPSOID) ? line_unit_sphere_intersect_dist(lP, lL) :
                                                          line_unit_box_intersect_dist(lP, lL);

  /* Use distance in world space directly to recover intersection.
   * This works because we assume no shear in the probe matrix. */
  float3 L_new = P + L * dist - probe.location;

  /* TODO(fclem): Roughness adjustment. */

  return L_new;
}

ReflectionProbeLowFreqLight extract_low_freq_lighting(SphericalHarmonicL1<float4> sh)
{
  /* To avoid color shift and negative values, we reduce saturation and directionality. */
  ReflectionProbeLowFreqLight result;
  result.ambient = sh.L0.M0.r + sh.L0.M0.g + sh.L0.M0.b;
  /* Bias to avoid division by zero. */
  result.ambient += 1e-6f;

  float3x4 L1_per_band;
  L1_per_band[0] = sh.L1.Mn1;
  L1_per_band[1] = sh.L1.M0;
  L1_per_band[2] = sh.L1.Mp1;

  float4x3 L1_per_comp = transpose(L1_per_band);
  result.direction = L1_per_comp[0] + L1_per_comp[1] + L1_per_comp[2];

  return result;
}

float normalization_factor(float3 /*L*/,
                           ReflectionProbeLowFreqLight numerator,
                           ReflectionProbeLowFreqLight denominator)
{
  /* TODO(fclem): Adjusting directionality is tricky.
   * Needs to be revisited later on. For now only use the ambient term. */
  return saturate(numerator.ambient / denominator.ambient);
}

}  // namespace eevee::lightprobe::sphere

namespace eevee {

struct LightProbeSample {
  SphericalHarmonicL1<float4> volume_irradiance;
  int spherical_id;
};

struct LightprobeSphereRenderData {
  [[uniform(SPHERE_PROBE_BUF_SLOT)]] SphereProbeData (&lightprobe_sphere_buf)[SPHERE_PROBE_MAX];
  [[sampler(SPHERE_PROBE_TEX_SLOT)]] sampler2DArray lightprobe_spheres_tx;

  int select_probe(float3 P, float random_probe) const
  {
    for (int index = 0; index < SPHERE_PROBE_MAX; index++) {
      SphereProbeData probe_data = lightprobe_sphere_buf[index];
      /* SphereProbeData doesn't contain any gap, exit at first item that is invalid. */
      if (probe_data.atlas_coord.layer == -1) {
        /* We hit the end of the array. Return last valid index. */
        return index - 1;
      }
      /* NOTE: The vector-matrix multiplication swapped on purpose to cancel the matrix transpose.
       */
      float3 lP = float4(P, 1.0f) * probe_data.world_to_probe_transposed;
      float gradient = (probe_data.influence_shape == SHAPE_ELIPSOID) ?
                           length(lP) :
                           max(max(abs(lP.x), abs(lP.y)), abs(lP.z));
      float score = saturate(probe_data.influence_bias - gradient * probe_data.influence_scale);
      if (score > random_probe) {
        return index;
      }
    }
    /* This should never happen (world probe is always last). */
    return SPHERE_PROBE_MAX - 1;
  }

  float4 sample_probe(float3 L, float lod, SphereProbeUvArea uv_area) const
  {
    float lod_min = floor(lod);
    float lod_max = ceil(lod);
    float mix_fac = lod - lod_min;

    float2 altas_uv_min, altas_uv_max;
    lightprobe::sphere::direction_to_uv(L, lod_min, lod_max, uv_area, altas_uv_min, altas_uv_max);

    float4 color_min = textureLod(
        lightprobe_spheres_tx, float3(altas_uv_min, uv_area.layer), lod_min);
    float4 color_max = textureLod(
        lightprobe_spheres_tx, float3(altas_uv_max, uv_area.layer), lod_max);
    return mix(color_min, color_max, mix_fac);
  }

  /**
   * Return spherical sample normalized by irradiance at sample position.
   * This avoid most of light leaking and reduce the need for many local probes.
   */
  float3 spherical_sample_normalized_with_parallax(LightProbeSample samp,
                                                   float3 P,
                                                   float3 L,
                                                   float lod) const
  {
    SphereProbeData probe = lightprobe_sphere_buf[samp.spherical_id];
    ReflectionProbeLowFreqLight shading_sh = lightprobe::sphere::extract_low_freq_lighting(
        samp.volume_irradiance);
    float normalization_fac = lightprobe::sphere::normalization_factor(
        L, shading_sh, probe.low_freq_light);
    L = lightprobe::sphere::parallax_eval(probe, P, L);
    return normalization_fac * this->sample_probe(L, lod, probe.atlas_coord).rgb;
  }
};

}  // namespace eevee
