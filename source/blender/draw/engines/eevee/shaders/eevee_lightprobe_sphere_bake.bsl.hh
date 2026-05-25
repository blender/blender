/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_light_shared.hh"
#include "eevee_lightprobe_sphere.bsl.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::lightprobe::sphere {

struct Remap {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[specialization_constant(true)]] bool extract_sh;
  [[specialization_constant(true)]] bool extract_sun;

  [[storage(0, write)]] SphereProbeHarmonic (&out_sh)[];
  [[storage(1, write)]] SphereProbeSunLight (&out_sun)[];

  [[push_constant]] const bool do_remap_mip0;
  [[push_constant]] const int4 probe_coord_packed;
  [[push_constant]] const int4 write_coord_packed;
  [[push_constant]] const int4 world_coord_packed;

  [[sampler(0)]] samplerCube cubemap_tx;
  [[sampler(1)]] sampler2DArray atlas_tx;

  [[image(0, write, SPHERE_PROBE_FORMAT)]] image2DArray atlas_img;

  [[shared]] float4 local_radiance[SPHERE_PROBE_REMAP_GROUP_SIZE * SPHERE_PROBE_REMAP_GROUP_SIZE];
};

float triangle_solid_angle(float3 A, float3 B, float3 C)
{
  return 2.0f * atan(abs(dot(A, cross(B, C))), (1.0f + dot(B, C) + dot(A, C) + dot(A, B)));
}

float quad_solid_angle(float3 A, float3 B, float3 C, float3 D)
{
  return triangle_solid_angle(A, B, C) + triangle_solid_angle(C, B, D);
}

float octahedral_texel_solid_angle(int2 local_texel, SphereProbePixelArea write_co)
{
  if (any(equal(local_texel, int2(write_co.extent - 1)))) {
    /* Do not weight these border pixels that are redundant. */
    return 0.0f;
  }
  /* Since we are putting texel centers on the edges of the octahedron, the shape of a texel can be
   * anything from a simple quad (at the Z=0 poles), to a 4 pointed start (at the Z=+-1 poles)
   * passing by arrow tail shapes (at the X=0 and Y=0 edges).
   *
   * But we can leverage the symmetries of the octahedral mapping. Given that all oddly shaped
   * texels are on the X=0 and Y=0 planes, we first fold all texels to the first quadrant.
   *
   * The texel footprint clipped to a quadrant is a well defined spherical quad. We then multiply
   * by the number of clipped shape the real texel sustains. This number is 2 at the X=0 and Y=0
   * edges (the arrow shaped pixels) and 4 at the Z=+-1 poles (4 pointed start shaped pixels).
   *
   * The sum of all texels solid angle should be 4 PI (area of sphere). */

  /* Wrap to bottom left quadrant. */
  int half_size = write_co.extent >> 1;
  int padded_size = write_co.extent - 2;
  int2 wrapped_texel;
  wrapped_texel.x = (local_texel.x >= half_size) ? (padded_size - local_texel.x) : local_texel.x;
  wrapped_texel.y = (local_texel.y >= half_size) ? (padded_size - local_texel.y) : local_texel.y;

  float2 texel_corner_v00 = float2(wrapped_texel) + float2(-0.5f, -0.5f);
  float2 texel_corner_v10 = float2(wrapped_texel) + float2(+0.5f, -0.5f);
  float2 texel_corner_v01 = float2(wrapped_texel) + float2(-0.5f, +0.5f);
  float2 texel_corner_v11 = float2(wrapped_texel) + float2(+0.5f, +0.5f);
  /* Clamp to well defined shape in spherical domain. */
  texel_corner_v00 = clamp(texel_corner_v00, float2(0.0f), float2(half_size - 1));
  texel_corner_v10 = clamp(texel_corner_v10, float2(0.0f), float2(half_size - 1));
  texel_corner_v01 = clamp(texel_corner_v01, float2(0.0f), float2(half_size - 1));
  texel_corner_v11 = clamp(texel_corner_v11, float2(0.0f), float2(half_size - 1));
  /* Convert to point on sphere. */
  float3 v00 = texel_to_direction(texel_corner_v00, write_co);
  float3 v10 = texel_to_direction(texel_corner_v10, write_co);
  float3 v01 = texel_to_direction(texel_corner_v01, write_co);
  float3 v11 = texel_to_direction(texel_corner_v11, write_co);
  /* The solid angle functions expect normalized vectors. */
  v00 = normalize(v00);
  v10 = normalize(v10);
  v01 = normalize(v01);
  v11 = normalize(v11);
  /* Compute solid angle of the spherical quad. */
  float texel_clipped_solid_angle = quad_solid_angle(v00, v10, v01, v11);
  /* Multiply by the symmetric halves that we omitted.
   * Also important to note that we avoid weighting the same pixel more than it's total sampled
   * footprint if it is duplicated in another pixel of the map. So border pixels do not require any
   * special treatment. Only the center cross needs it. */
  if (wrapped_texel.x == half_size - 1) {
    texel_clipped_solid_angle *= 2.0f;
  }
  if (wrapped_texel.y == half_size - 1) {
    texel_clipped_solid_angle *= 2.0f;
  }
  return texel_clipped_solid_angle;
}

/* Sample cubemap and remap into an octahedral texture. */
[[compute, local_size(SPHERE_PROBE_REMAP_GROUP_SIZE, SPHERE_PROBE_REMAP_GROUP_SIZE)]]
void remap_cubemap_to_octahedral([[resource_table]] Remap &srt,
                                 [[global_invocation_id]] const uint3 global_id,
                                 [[work_group_id]] const uint3 group_id,
                                 [[local_invocation_index]] const uint local_index)
{
  uint work_group_index = SPHERE_PROBE_REMAP_GROUP_SIZE * group_id.y + group_id.x;
  constexpr uint group_size = SPHERE_PROBE_REMAP_GROUP_SIZE * SPHERE_PROBE_REMAP_GROUP_SIZE;

  SphereProbeUvArea world_coord = reinterpret_as_atlas_coord(srt.world_coord_packed);
  SphereProbePixelArea write_coord = reinterpret_as_write_coord(srt.write_coord_packed);

  /* Texel in probe. */
  int2 local_texel = int2(global_id.xy);

  float2 wrapped_uv;
  float3 direction = texel_to_direction(float2(local_texel), write_coord, wrapped_uv);
  float4 radiance_and_transmittance = texture(srt.cubemap_tx, direction);
  float3 radiance = radiance_and_transmittance.xyz;

  float opacity = 1.0f - radiance_and_transmittance.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(srt.probe_coord_packed, srt.world_coord_packed));
  if (!is_world && opacity != 1.0f) {
    float2 biased_uv = miplvl_scale_bias(0.0f, world_coord, saturate(wrapped_uv));
    float2 world_uv = biased_uv * world_coord.scale + world_coord.offset;
    float4 world_radiance = textureLod(srt.atlas_tx, float3(world_uv, world_coord.layer), 0.0f);
    radiance.rgb = mix(world_radiance.rgb, radiance.rgb, opacity);
  }

  float sun_threshold = uniform_buf.clamp.sun_threshold;
  float3 radiance_clamped = colorspace::brightness_clamp_max(radiance, sun_threshold);
  float3 radiance_sun = radiance - radiance_clamped;
  radiance = radiance_clamped;

  if (srt.do_remap_mip0 && !any(greaterThanEqual(local_texel, int2(write_coord.extent)))) {
    float clamp_indirect = uniform_buf.clamp.surface_indirect;
    float3 out_radiance = colorspace::brightness_clamp_max(radiance, clamp_indirect);

    int3 texel = int3(local_texel + write_coord.offset, write_coord.layer);
    imageStore(srt.atlas_img, texel, float4(out_radiance, 1.0f));
  }

  float sample_weight = octahedral_texel_solid_angle(local_texel, write_coord);

  if (srt.extract_sun) {
    /* Parallel sum. Result is stored inside local_radiance[0]. */
    srt.local_radiance[local_index] = radiance_sun.xyzz * sample_weight;
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    /* Vulkan validation layers detects a data race on `local_radiance[local_index] +=
     * local_radiance[local_index + stride]`. This is a false positive. Even when doing a manual
     * unroll or make the variable `shared coherent` doesn't work around it. */
    for (uint i = 0; i < 10; i++) [[unroll]] {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        srt.local_radiance[local_index] += srt.local_radiance[local_index + stride];
      }
    }
    barrier();

    if (local_index == 0u) {
      srt.out_sun[work_group_index].radiance = srt.local_radiance[0].xyz;
    }
    barrier();

    /* Reusing local_radiance for directions. */
    auto &local_direction = srt.local_radiance;

    local_direction[local_index] = float4(normalize(direction), 1.0f) * sample_weight *
                                   length(radiance_sun.xyz);
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    /* Vulkan validation layers detects a data race on `local_direction[local_index] +=
     * local_direction[local_index + stride]`. This is a false positive. Even when doing a manual
     * unroll or make the variable `shared coherent` doesn't work around it. */
    for (uint i = 0; i < 10; i++) [[unroll]] {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        local_direction[local_index] += local_direction[local_index + stride];
      }
    }
    barrier();

    if (local_index == 0u) {
      srt.out_sun[work_group_index].direction = local_direction[0];
    }
    barrier();
  }

  if (srt.extract_sh) {
    /* Parallel sum. Result is stored inside local_radiance[0]. */
    srt.local_radiance[local_index] = radiance.xyzz * sample_weight;
    /* OpenGL/Intel drivers have known issues where it isn't able to compile barriers inside for
     * loops. Unroll is needed as driver might decide to not unroll in shaders with more
     * complexity. */
    /* Vulkan validation layers detects a data race on `local_radiance[local_index] +=
     * local_radiance[local_index + stride]`. This is a false positive. Even when doing a manual
     * unroll or make the variable `shared coherent` doesn't work around it. */
    for (uint i = 0; i < 10; i++) [[unroll]] {
      barrier();
      uint stride = group_size >> (i + 1u);
      if (local_index < stride) {
        srt.local_radiance[local_index] += srt.local_radiance[local_index + stride];
      }
    }
    barrier();

    if (local_index == 0u) {
      /* Find the middle point of the whole thread-group. Use it as light vector.
       * Note that this is an approximation since the footprint of a thread-group is not
       * necessarily a convex polygons (with center of gravity at midpoint).
       * But the actual error introduce by this approximation is not perceivable. */
      /* FIXME(fclem): The error IS very perceivable for resolution lower than a quadrant. */
      int2 max_group_texel = local_texel + int2(SPHERE_PROBE_REMAP_GROUP_SIZE);
      /* Min direction is the local direction since this is only ran by thread 0. */
      float3 min_direction = normalize(direction);
      float3 max_direction = normalize(texel_to_direction(float2(max_group_texel), write_coord));
      float3 L = normalize(min_direction + max_direction);
      /* Convert radiance to spherical harmonics. */
      SphericalHarmonicL1<float4> sh = {};
      sh.encode_signal_sample(L, srt.local_radiance[0]);
      /* Outputs one SH for each thread-group. */
      srt.out_sh[work_group_index].L0_M0 = sh.L0.M0;
      srt.out_sh[work_group_index].L1_Mn1 = sh.L1.Mn1;
      srt.out_sh[work_group_index].L1_M0 = sh.L1.M0;
      srt.out_sh[work_group_index].L1_Mp1 = sh.L1.Mp1;
    }
  }
}

/* Bypass convolution cascade and projection logic. */
// #define ALWAYS_SAMPLE_CUBEMAP
/* Debugging texel alignment. */
// #define USE_PIXEL_CHECKERBOARD

float roughness_from_relative_mip(float prev_mip_roughness, float curr_mip_roughness)
{
#ifdef ALWAYS_SAMPLE_CUBEMAP
  /* For reference and debugging. */
  return curr_mip_roughness;
#else
  /* The exponent should be 2 but result is a bit less blurry than expected in practice. */
  constexpr float exponent = 3.0f;
  /* From linear roughness to GGX roughness input. */
  float m_prev = pow(prev_mip_roughness, exponent);
  float m_curr = pow(curr_mip_roughness, exponent);
  /* Given that spherical gaussians are very close to regular gaussian in 1D,
   * we reuse the same rule for successive convolution (i.e: `G(x,a) X G(x,b) = G(x,a+b)`).
   * While this isn't technically correct, this still works quite well in practice. */
  float m_target = m_curr - m_prev;
  /* From GGX roughness input to linear roughness. */
  return pow(m_target, 1.0f / exponent);
#endif
}

float cone_cosine_from_roughness(float linear_roughness)
{
  /* From linear roughness to GGX roughness input. */
  float m = square(linear_roughness);
  /* Chosen so that roughness of 1.0 maps to half pi cone aperture. */
  float cutoff_value = mix(0.01f, 0.14f, m);
  /* Inversion of the spherical gaussian. This gives the cutoff for the half angle from N.H. */
  float half_angle_cos = 1.0f + (log(cutoff_value) * square(m)) / 2.0f;
  float half_angle_sin = safe_sqrt(1.0f - square(half_angle_cos));
  /* Use cosine rule to avoid acos. Return cos(2 * half_angle). */
  return square(half_angle_cos) - square(half_angle_sin);
}

int sample_count_get()
{
  /* After experimenting this is likely to be the best value if we keep the max resolution to 2048.
   * This isn't ideal, but the better solution would be to use multiple steps per mip which would
   * reduce the number of sample per step (use sum of gaussian per step). */
  return 196;
}

float sample_weight(float3 out_direction, float3 in_direction, float linear_roughness)
{
  out_direction = normalize(out_direction);
  in_direction = normalize(in_direction);

  /* From linear roughness to GGX roughness input. */
  float m = square(linear_roughness);
  /* Map GGX roughness to spherical gaussian sharpness.
   * From "SG Series Part 4: Specular Lighting From an SG Light Source" by MJP
   * https://therealmjp.github.io/posts/sg-series-part-4-specular-lighting-from-an-sg-light-source/
   */
  float3 N = out_direction;
  float3 H = normalize(out_direction + in_direction);
  float NH = saturate(dot(N, H));
  /* GGX. */
  // return exp(-square(acos(NH) / m));
  /* Spherical Gaussian. */
  return exp(2.0f * (NH - 1.0f) / square(m));
}

float3x3 tangent_basis(float3 N)
{
  /* TODO(fclem): This create a discontinuity at Z=0. */
  return from_up_axis(N);
}

struct Convolve {
  [[push_constant]] const int4 probe_coord_packed;
  [[push_constant]] const int4 write_coord_packed;
  [[push_constant]] const int4 read_coord_packed;
  [[push_constant]] const int read_lod;
  [[sampler(0)]] samplerCube cubemap_tx;
  [[sampler(1)]] sampler2DArray in_atlas_mip_tx;
  [[image(1, write, SFLOAT_16_16_16_16)]] image2DArray out_atlas_mip_img;
};

/* Convolve Mipmap chain recursively using a increasingly large spherical gaussian filters. */
[[compute, local_size(SPHERE_PROBE_GROUP_SIZE, SPHERE_PROBE_GROUP_SIZE)]]
void mip_convolve([[resource_table]] Convolve &srt,
                  [[global_invocation_id]] const uint3 global_id,
                  [[local_invocation_id]] const uint3 local_id,
                  [[local_invocation_index]] const uint local_index)
{
  SphereProbeUvArea sample_coord = reinterpret_as_atlas_coord(srt.probe_coord_packed);
  SphereProbePixelArea out_texel_area = reinterpret_as_write_coord(srt.write_coord_packed);

  /* Texel in probe. */
  int2 out_local_texel = int2(global_id.xy);

  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(out_local_texel, int2(out_texel_area.extent)))) {
    return;
  }

  /* From mip to linear roughness (same as UI). */
  float prev_mip_roughness = lod_to_roughness(float(srt.read_lod));
  float curr_mip_roughness = lod_to_roughness(float(srt.read_lod + 1));
  /* In order to reduce the sample count, we sample the content of previous mip level.
   * But this one has already been convolved. So we have to derive the equivalent roughness
   * that produces the same result. */
  float mip_roughness = roughness_from_relative_mip(prev_mip_roughness, curr_mip_roughness);
  /* Clamp to avoid numerical imprecision. */
  float mip_roughness_clamped = max(mip_roughness, BSDF_ROUGHNESS_THRESHOLD);
  float cone_cos = cone_cosine_from_roughness(mip_roughness_clamped);

  float3 out_direction = texel_to_direction(float2(out_local_texel), out_texel_area);
  out_direction = normalize(out_direction);

  float3x3 basis = tangent_basis(out_direction);

  int2 out_texel = out_texel_area.offset + out_local_texel;

  float weight_accum = 0.0f;
  float4 radiance_accum = float4(0.0f);

  int sample_count = sample_count_get();
  for (int i = 0; i < sample_count; i++) {
    float2 rand = hammersley_2d(i, sample_count);
    float3 in_direction = basis * sample_uniform_cone(rand, cone_cos);

#ifndef ALWAYS_SAMPLE_CUBEMAP
    float2 in_uv = direction_to_uv(in_direction, float(srt.read_lod), sample_coord);
    float4 radiance = texture(srt.in_atlas_mip_tx, float3(in_uv, sample_coord.layer));
#else /* For reference and debugging. */
    float4 radiance = texture(cubemap_tx, in_direction);
#endif

    float weight = sample_weight(out_direction, in_direction, mip_roughness_clamped);
    radiance_accum += radiance * weight;
    weight_accum += weight;
  }
  float4 out_radiance = radiance_accum * safe_rcp(weight_accum);

#ifdef USE_PIXEL_CHECKERBOARD
  int2 a = out_texel % 2;
  out_radiance = float4(a.x == a.y);
#endif

  imageStore(srt.out_atlas_mip_img, int3(out_texel, out_texel_area.layer), out_radiance);
}

struct IrradianceSum {
  [[storage(0, read)]] const SphereProbeHarmonic (&in_sh)[];
  [[storage(1, write)]] SphereProbeHarmonic &out_sh;

  [[push_constant]] const int3 probe_remap_dispatch_size;

  [[shared]] float4 local_sh_coefs[SPHERE_PROBE_SH_GROUP_SIZE][4];
};

[[compute, local_size(SPHERE_PROBE_SH_GROUP_SIZE)]]
void irradiance_sum([[resource_table]] IrradianceSum &srt,
                    [[global_invocation_id]] const uint3 global_id,
                    [[local_invocation_id]] const uint3 local_id,
                    [[local_invocation_index]] const uint local_index)
{
  constexpr uint group_size = SPHERE_PROBE_SH_GROUP_SIZE;

  SphericalHarmonicL1<float4> sh;
  sh.L0.M0 = float4(0.0f);
  sh.L1.Mn1 = float4(0.0f);
  sh.L1.M0 = float4(0.0f);
  sh.L1.Mp1 = float4(0.0f);

  /* First sum onto the local memory. */
  uint valid_data_len = uint(srt.probe_remap_dispatch_size.x * srt.probe_remap_dispatch_size.y);
  constexpr uint iter_count = uint(SPHERE_PROBE_MAX_HARMONIC) / group_size;
  for (uint i = 0; i < iter_count; i++) {
    uint index = gl_WorkGroupSize.x * i + local_index;
    if (index >= valid_data_len) {
      break;
    }
    SphericalHarmonicL1<float4> sh_sample;
    sh_sample.L0.M0 = srt.in_sh[index].L0_M0;
    sh_sample.L1.Mn1 = srt.in_sh[index].L1_Mn1;
    sh_sample.L1.M0 = srt.in_sh[index].L1_M0;
    sh_sample.L1.Mp1 = srt.in_sh[index].L1_Mp1;
    sh = spherical_harmonics::add(sh, sh_sample);
  }

  /* Then sum across invocations. */
  srt.local_sh_coefs[local_index][0] = sh.L0.M0;
  srt.local_sh_coefs[local_index][1] = sh.L1.Mn1;
  srt.local_sh_coefs[local_index][2] = sh.L1.M0;
  srt.local_sh_coefs[local_index][3] = sh.L1.Mp1;

  /* Parallel sum. */
  uint stride = group_size / 2;
  for (int i = 0; i < 10; i++) {
    barrier();
    if (local_index < stride) {
      for (int i = 0; i < 4; i++) {
        srt.local_sh_coefs[local_index][i] += srt.local_sh_coefs[local_index + stride][i];
      }
    }
    stride /= 2;
  }

  barrier();
  if (local_index == 0u) {
    srt.out_sh.L0_M0 = srt.local_sh_coefs[0][0];
    srt.out_sh.L1_Mn1 = srt.local_sh_coefs[0][1];
    srt.out_sh.L1_M0 = srt.local_sh_coefs[0][2];
    srt.out_sh.L1_Mp1 = srt.local_sh_coefs[0][3];
  }
}

struct SunExtraction {
  [[storage(0, read)]] const SphereProbeSunLight (&in_sun)[];
  [[storage(1, write)]] LightData (&sunlight_buf)[2];
  [[push_constant]] const int sun_id;
  [[push_constant]] const int3 probe_remap_dispatch_size;

  [[shared]] float3 local_radiance[SPHERE_PROBE_SH_GROUP_SIZE];
  [[shared]] float4 local_direction[SPHERE_PROBE_SH_GROUP_SIZE];
};

[[compute, local_size(SPHERE_PROBE_SH_GROUP_SIZE)]]
void sun_extraction([[resource_table]] SunExtraction &srt,
                    [[global_invocation_id]] const uint3 global_id,
                    [[local_invocation_id]] const uint3 local_id,
                    [[local_invocation_index]] const uint local_index)
{
  constexpr uint group_size = SPHERE_PROBE_SH_GROUP_SIZE;

  SphereProbeSunLight sun;
  sun.radiance = float3(0.0f);
  sun.direction = float4(0.0f);

  /* First sum onto the local memory. */
  uint valid_data_len = uint(srt.probe_remap_dispatch_size.x * srt.probe_remap_dispatch_size.y);
  constexpr uint iter_count = uint(SPHERE_PROBE_MAX_HARMONIC) / group_size;
  for (uint i = 0; i < iter_count; i++) {
    uint index = SPHERE_PROBE_SH_GROUP_SIZE * i + local_index;
    if (index >= valid_data_len) {
      break;
    }
    sun.radiance += srt.in_sun[index].radiance;
    sun.direction += srt.in_sun[index].direction;
  }

  /* Then sum across invocations. */
  srt.local_radiance[local_index] = sun.radiance;
  srt.local_direction[local_index] = sun.direction;

  /* Parallel sum. */
  uint stride = group_size / 2;
  for (int i = 0; i < 10; i++) {
    barrier();
    if (local_index < stride) {
      srt.local_radiance[local_index] += srt.local_radiance[local_index + stride];
      srt.local_direction[local_index] += srt.local_direction[local_index + stride];
    }
    stride /= 2;
  }

  barrier();
  if (local_index == 0u) {
    srt.sunlight_buf[srt.sun_id].color = srt.local_radiance[0];

    /* Normalize the sum to get the mean direction. The length of the vector gives us the size of
     * the sun light. */
    float len;
    float3 direction = safe_normalize_and_get_length(
        srt.local_direction[0].xyz / srt.local_direction[0].w, len);

    float3x3 tx = transpose(from_up_axis(direction));
    /* Convert to transform. */
    srt.sunlight_buf[srt.sun_id].object_to_world.x = float4(tx[0], 0.0f);
    srt.sunlight_buf[srt.sun_id].object_to_world.y = float4(tx[1], 0.0f);
    srt.sunlight_buf[srt.sun_id].object_to_world.z = float4(tx[2], 0.0f);

    /* Auto sun angle. */
    float sun_angle_cos = 2.0f * len - 1.0f;
    /* Compute tangent from cosine. */
    float sun_angle_tan = sqrt(-1.0f + 1.0f / square(sun_angle_cos));
    /* Clamp value to avoid float imprecision artifacts. */
    float sun_radius = clamp(sun_angle_tan, 0.001f, 20.0f);

    /* Convert irradiance to radiance. */
    float shape_power = M_1_PI * (1.0f + 1.0f / square(sun_radius));
    float point_power = 1.0f;

    srt.sunlight_buf[srt.sun_id].power[LIGHT_DIFFUSE] = shape_power;
    srt.sunlight_buf[srt.sun_id].power[LIGHT_SPECULAR] = shape_power;
    srt.sunlight_buf[srt.sun_id].power[LIGHT_TRANSMISSION] = shape_power;
    srt.sunlight_buf[srt.sun_id].power[LIGHT_VOLUME] = point_power;

    /* NOTE: Use the radius from UI instead of auto sun size for now. */
  }
}

}  // namespace eevee::lightprobe::sphere

PipelineCompute eevee_lightprobe_sphere_remap(
    eevee::lightprobe::sphere::remap_cubemap_to_octahedral);
PipelineCompute eevee_lightprobe_sphere_sunlight(eevee::lightprobe::sphere::sun_extraction);
PipelineCompute eevee_lightprobe_sphere_convolve(eevee::lightprobe::sphere::mip_convolve);
PipelineCompute eevee_lightprobe_sphere_irradiance(eevee::lightprobe::sphere::irradiance_sum);
