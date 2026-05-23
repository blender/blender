/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Implementation of Horizon Based Global Illumination and Ambient Occlusion.
 *
 * This mostly follows the paper:
 * "Screen Space Indirect Lighting with Visibility Bitmask"
 * by Olivier Therrien, Yannick Levesque, Guillaume Gilet
 *
 * Expects `screen_radiance_tx` and `screen_normal_tx` to be bound if `HORIZON_OCCLUSION` is not
 * defined.
 */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_sampling_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_gbuffer_data)
VERTEX_SHADER_CREATE_INFO(eevee_sampling_data)
VERTEX_SHADER_CREATE_INFO(eevee_utility_texture)
VERTEX_SHADER_CREATE_INFO(eevee_global_ubo)
VERTEX_SHADER_CREATE_INFO(eevee_hiz_data)
VERTEX_SHADER_CREATE_INFO(draw_view)

#include "draw_view_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_filter_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_ray_types_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_spherical_harmonics.bsl.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee {

namespace fast_gi {

/**
 * Returns the bitmask for a given ordered pair of angle in [-pi/2..pi/2] range.
 * Clamps the inputs to the valid range.
 */
uint angles_to_bitmask(float2 theta)
{
  constexpr int bitmask_len = 32;
  /* Algorithm 1, line 18. Re-ordered to make sure to clamp to the hemisphere range. */
  float2 ratio = saturate(theta * M_1_PI + 0.5f);
  uint a = uint(floor(float(bitmask_len) * ratio.x));
  /* The paper is wrong here. The additional half Pi is not needed. */
  uint b = uint(ceil(float(bitmask_len) * (ratio.y - ratio.x)));
  /* Algorithm 1, line 19. */
  return (((b < 32u) ? 1u << b : 0u) - 1u) << a;
}

float bitmask_to_visibility_uniform(uint bitmask)
{
  constexpr int bitmask_len = 32;
  /* Algorithm 1, line 26. */
  return float(bitCount(bitmask)) / float(bitmask_len);
}

/**
 * For a given visibility bitmask storing locally occluded sectors,
 * returns the uniform (non-cosine weighted) occlusion (visibility).
 */
float bitmask_to_occlusion_uniform(uint bitmask)
{
  /* Occlusion is the opposite of visibility. */
  return 1.0f - bitmask_to_visibility_uniform(bitmask);
}

/**
 * For a given visibility bitmask storing locally occluded sectors,
 * returns the cosine weighted occlusion (visibility).
 */
float bitmask_to_occlusion_cosine(uint bitmask)
{
  /* This is not described in the paper. Another solution would be to change the sector
   * distribution in `angles_to_bitmask()` but that requires more computation per
   * samples. The quality difference does not justify it currently. */
#if 0 /* Reference. */
  constexpr int bitmask_len = 32;
  float visibility = 0.0f;
  for (int bit = 0; bit < bitmask_len; bit++) {
    float angle = (((float(bit) + 0.5f) / float(bitmask_len)) - 0.5f) * M_PI;
    /* Integrating over the hemisphere. */
    if (((bitmask >> bit) & 1u) == 0u) {
      visibility += cos(angle) * M_PI_2 / float(bitmask_len);
    }
  }
  return visibility;
#else
  /* The precomputed weights are the accumulated weights from the reference loop for each of the
   * samples in the mask. The weight is distributed evenly for each sample inside a mask.
   * This is like a 4 piecewise linear approximation of the cosine lobe. */
  constexpr float4 weights = float4(0.0095061f, 0.0270951f, 0.0405571f, 0.0478421f);
  constexpr uint4 masks = uint4(0xF000000Fu, 0x0F0000F0u, 0x00F00F00u, 0x000FF000u);
  return saturate(1.0f - dot(float4(bitCount(uint4(bitmask) & masks)), weights));
#endif
}

/**
 * Projects the normal `N` onto a plane defined by `V` and `T`.
 * V, T, B forms an orthonormal basis around V.
 * Returns the angle of the normal projected normal with `V` and its length.
 */
void projected_normal_to_plane_angle_and_length(
    float3 N, float3 V, float3 T, float3 B, float &N_proj_len, float &N_angle)
{
  /* Projected view normal onto the integration plane. */
  float3 N_proj = normalize_and_get_length(N - B * dot(N, B), N_proj_len);

  float N_sin = dot(N_proj, T);
  float N_cos = dot(N_proj, V);
  /* Angle between normalized projected normal and view vector. */
  N_angle = sign(N_sin) * acos_fast(N_cos);
}

/* -------------------------------------------------------------------- */
/** \name Buffer Sampling implementation
 * \{ */

template<typename T>
float3 sample_radiance(sampler2D /*screen_radiance_tx*/, float2 /*uv*/, float /*lod*/)
{
  return float3(0.0f);
}
template<typename T>
float3 sample_normal(sampler2D /*screen_normal_tx*/, float2 /*uv*/, float /*lod*/)
{
  return float3(0.0f);
}
template<typename T> T select_result(float /*occlusion*/, SphericalHarmonicL1<float4> /*sh*/)
{
  return T(0.0f);
}

/* AO only implementation. */
template float3 sample_radiance<float>(sampler2D screen_radiance_tx, float2 uv, float lod);
template float3 sample_normal<float>(sampler2D screen_normal_tx, float2 uv, float lod);
template<> float select_result<float>(float occlusion, SphericalHarmonicL1<float4> /*sh*/)
{
  return occlusion;
}

/* GI implementation. */
template<>
float3 sample_radiance<SphericalHarmonicL1<float4>>(sampler2D screen_radiance_tx,
                                                    float2 uv,
                                                    float lod)
{
  return textureLod(screen_radiance_tx, uv, lod).rgb;
}
template<>
float3 sample_normal<SphericalHarmonicL1<float4>>(sampler2D screen_normal_tx, float2 uv, float lod)
{
  return textureLod(screen_normal_tx, uv, lod).rgb * 2.0f - 1.0f;
}
template<>
SphericalHarmonicL1<float4> select_result<SphericalHarmonicL1<float4>>(
    float /*occlusion*/, SphericalHarmonicL1<float4> sh)
{
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fast GI scan implementation
 * \{ */

/**
 * Scans the horizon in many directions and returns the indirect lighting radiance.
 * Returned lighting is stored inside the context in `_accum` members already normalized.
 * If `reversed` is set to true, the input normal must be negated.
 */
template<typename ResultT>
ResultT eval(ScreenThicknessParameters thickness_params,
             sampler2D hiz_tx,
             sampler2D screen_radiance_tx,
             sampler2D screen_normal_tx,
             float3 vP,
             float3 vN,
             float4 noise,
             float2 pixel_size,
             float search_distance,
             float angle_bias,
             const int slice_count,
             const int sample_count,
             const bool reversed,
             const bool ao_only)
{
  float3 vV = drw_view_incident_vector(vP);

  float2 v_dir;
  if (slice_count <= 2) {
    /* We cover half the circle because we trace in both directions. */
    v_dir = sample_circle(noise.x / float(2 * slice_count));
  }

  float weight_accum = 0.0f;
  float occlusion_accum = 0.0f;
  SphericalHarmonicL1<float4> sh_accum = {};

#if defined(GPU_METAL)
/* NOTE: Full loop unroll hint increases performance on Apple Silicon. */
#  pragma clang loop unroll(full)
#endif
  for (int slice = 0; slice < slice_count; slice++) {
    if (slice_count > 2) {
      /* We cover half the circle because we trace in both directions. */
      v_dir = sample_circle(((float(slice) + noise.x) / float(2 * slice_count)));
    }

    /* Setup integration domain around V. */
    float3 vB = normalize(cross(vV, float3(v_dir, 0.0f)));
    float3 vT = cross(vB, vV);

    /* Bitmask representing the occluded sectors on the slice. */
    uint slice_bitmask = 0u;

    /* Angle between vN and the horizon slice plane. */
    float vN_angle;
    /* Length of vN projected onto the horizon slice plane. */
    float vN_length;

    projected_normal_to_plane_angle_and_length(vN, vV, vT, vB, vN_length, vN_angle);

    /* Jitter slice integration domain to avoid banding due to the bitmask. */
    vN_angle += (noise.z - 0.5f) * (M_PI / 32.0f) * angle_bias;

    SphericalHarmonicL1<float4> sh_slice = {};
    /* The 4th component contains visibility. Set visibility to 1 for the upper hemisphere. */
    sh_slice.encode_signal_sample(vN, float4(0.0f, 0.0f, 0.0f, 1.0f));

    /* For both sides of the view vector. */
    for (int side = 0; side < 2; side++) {
      Ray ray;
      ray.origin = vP;
      ray.direction = float3((side == 0) ? v_dir : -v_dir, 0.0f);
      ray.max_time = search_distance;

      /* TODO(fclem): Could save some computation here by computing entry and exit point on the
       * screen at once and just scan through. */
      ScreenSpaceRay ssray = ScreenSpaceRay::create(ray, pixel_size);

      ScreenThicknessEstimator thickness_estimator = ScreenThicknessEstimator::init(ray.origin.z);

#if defined(GPU_METAL) && defined(GPU_APPLE)
/* NOTE: Full loop unroll hint increases performance on Apple Silicon. */
#  pragma clang loop unroll(full)
#endif
      for (int j = 0; j < sample_count; j++) {
        /* Always cross at least one pixel. */
        float time = 1.0f + square((float(j) + noise.y) / float(sample_count)) * ssray.max_time;

        if (reversed) {
          /* We need to cross at least 2 pixels to avoid artifacts form the HiZ storing only the
           * max depth. The HiZ would need to contain the min depth instead to avoid this. */
          time += 1.0f;
        }

        const float lod = log2(time * uniform_buf.ao.lod_factor);

        const float2 sample_uv = ssray.origin.xy + ssray.direction.xy * time;
        float sample_depth =
            textureLod(hiz_tx, sample_uv * uniform_buf.hiz.uv_scale, floor(noise.w + lod)).r;

        if (sample_depth == 1.0f && !reversed) {
          /* Skip background. Avoids making shadow on the geometry near the far plane. */
          continue;
        }

        float sample_view_z = drw_depth_screen_to_view(sample_depth);
        float sample_ndc_min_thickness = thickness_params.pixel_depth_thickness_at(sample_view_z);

        float sample_thickness = thickness_estimator.thickness(
            sample_depth, time, sample_ndc_min_thickness);

        /* Bias depth a bit to avoid self shadowing issues. */
        constexpr float bias = 2.0f * 2.4e-7f;
        const float sample_depth_front = sample_depth + (reversed ? -bias : bias);
        const float sample_depth_back = sample_depth + (reversed ? 0.0 : sample_thickness);

        float3 vP_sample_front = drw_point_screen_to_view(float3(sample_uv, sample_depth_front));
        float3 vP_sample_back = drw_point_screen_to_view(float3(sample_uv, sample_depth_back));

        /* Mimic a sphere intersection check + clipping of the intersecting ray.
         * Assumes the ray is aligned with the view Z axis.
         * While this is exact for orthographic cameras it can distort the AO look with high FOV
         * angle. However it is cheaper than a full sphere clipping. */
        float3 ls_P_front = (vP_sample_front - vP) / search_distance;
        float3 ls_P_back = (vP_sample_back - vP) / search_distance;
        /* Simplification of `sin_from_cos(length(ls_P_front.xy))`. */
        float max_dist = sqrt_fast(saturate(1.0f - length_squared(ls_P_front.xy)));
        if (!reversed) {
          ls_P_front.z = clamp(ls_P_front.z, -max_dist, max_dist);
          ls_P_back.z = clamp(ls_P_back.z, -max_dist, max_dist);
          if (ls_P_front.z == ls_P_back.z) {
            continue;
          }
        }

        float3 vL_front = normalize(ls_P_front);
        float3 vL_back = normalize(ls_P_back);

        float LV_front = dot(vL_front, vV);
        float LV_back = dot(vL_back, vV);
        if (reversed) {
          /* In reverse mode we revert back to horizon scanning.
           * Occlude everything in front of this sample. */
          LV_front = 1.0f;
          /* If hitting the background consider the sample infinitely far away. */
          if (sample_depth == 1.0f) {
            LV_back = -1.0f;
          }
        }
        /* Ordered pair of angle. Minimum in X, Maximum in Y.
         * Front will always have the smallest angle here since it is the closest to the view. */
        float2 theta = acos_fast(float2(LV_front, LV_back));
        theta.y = max(theta.x, theta.y);
        if (reversed) {
          /* This is the main part of the reversed AO.
           * Rotate the angles 180 degrees to align with the front facing normal. */
          theta -= M_PI;
        }
        /* If we are tracing backward, the angles are negative. Swizzle to keep correct order. */
        theta = (side == 0) ? theta.xy : -theta.yx;

        const float2 sample_uv_data = sample_uv * raytrace_buf.fast_gi_uv_scale;
        /* Need to account for LOD0 of radiance texture being the tracing resolution. */
        float lod_data = lod - raytrace_buf.fast_gi_lod_bias;

        float3 radiance = sample_radiance<ResultT>(screen_radiance_tx, sample_uv_data, lod_data);
        /* Take emitter surface normal into consideration. */
        float3 normal = sample_normal<ResultT>(screen_normal_tx, sample_uv_data, lod_data);
        if (ao_only) {
          radiance = float3(0);
        }
        /* Discard back-facing samples. */
        float facing = dot(normal, -vL_front);
        if (facing < 0.0f) {
          radiance *= raytrace_buf.backface_hit_scale;
        }
        float facing_weight = abs(facing);

        /* Angular bias shrinks the visibility bitmask around the projected normal. */
        float2 biased_theta = (theta - vN_angle) * angle_bias;
        uint sample_bitmask = angles_to_bitmask(biased_theta);
        float weight_bitmask = bitmask_to_visibility_uniform(sample_bitmask & ~slice_bitmask);

        radiance *= facing_weight * weight_bitmask;
        sh_slice.encode_signal_sample(vL_front, float4(radiance, -weight_bitmask));

        slice_bitmask |= sample_bitmask;
      }
    }

    float occlusion_slice = bitmask_to_occlusion_cosine(slice_bitmask);
    /* Correct normal not on plane (Eq. 8 of GTAO paper). */
    occlusion_accum += occlusion_slice * vN_length;

    /* Use uniform visibility since this is what we use for near field lighting. */
    sh_accum = spherical_harmonics::madd(sh_slice, vN_length, sh_accum);

    weight_accum += vN_length;

    /* Rotate 90 degrees. */
    v_dir = orthogonal(v_dir);
  }

  float weight_rcp = safe_rcp(weight_accum);

  /* Weight by area of the sphere. This is expected for correct SH evaluation. */
  float sphere_weight = 2.0f * M_TAU;

  /* We store the energy for the whole sphere but we weighted G.I. from a biased cone (angle_bias).
   * So we need to normalize by the actual cone we weighted with.
   * `angle_bias` has range [1..2] */
  // float biased_solid_angle = M_TAU * (1.0f - cos(angle_bias * M_PI_2));
  // float hemisphere_solid_angle = M_TAU;
  // float gi_weight_factor = sphere_weight / (hemisphere_solid_angle + biased_solid_angle);
  /* The above simplifies to this. */
  float gi_weight_factor = 1.0f / ((3.0f / 2.0f) - cos(angle_bias * M_PI_2));

  sphere_weight = weight_rcp * sphere_weight;
  /* For some unknown reason, the GI and visibility do not need the same weighting. */
  float4 sh_weight = float4(float3(sphere_weight * gi_weight_factor), sphere_weight);

  sh_accum = spherical_harmonics::mul(sh_accum, sh_weight);
  occlusion_accum *= weight_rcp;
  return select_result<ResultT>(occlusion_accum, sh_accum);
}

template float eval<float>(ScreenThicknessParameters,
                           sampler2D,
                           sampler2D,
                           sampler2D,
                           float3,
                           float3,
                           float4,
                           float2,
                           float,
                           float,
                           int,
                           int,
                           bool,
                           bool);
template SphericalHarmonicL1<float4> eval<SphericalHarmonicL1<float4>>(ScreenThicknessParameters,
                                                                       sampler2D,
                                                                       sampler2D,
                                                                       sampler2D,
                                                                       float3,
                                                                       float3,
                                                                       float4,
                                                                       float2,
                                                                       float,
                                                                       float,
                                                                       int,
                                                                       int,
                                                                       bool,
                                                                       bool);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common resources / interfaces
 * \{ */

struct SampleInput {
  [[sampler(1)]] const sampler2DDepth depth_tx;
  /* utility_tx reserves slot 2. */
  /* hiz reserves slot 3. */
  [[sampler(4)]] const sampler2D fast_gi_radiance_0_tx;
  [[sampler(5)]] const sampler2D fast_gi_radiance_1_tx;
  /* lightprobe reserves slot 6-7. */
  [[sampler(8)]] const sampler2D fast_gi_radiance_2_tx;
  [[sampler(9)]] const sampler2D fast_gi_radiance_3_tx;
  [[sampler(10)]] const sampler2D screen_normal_tx;

  float3 sample_normal_get(int2 texel, bool &is_processed) const
  {
    float4 normal = texelFetch(screen_normal_tx, texel, 0);
    is_processed = (normal.w != 0.0f);
    return drw_normal_view_to_world(normal.xyz * 2.0f - 1.0f);
  }

  /* Used for denoise. */
  float sample_weight_get(sampler2D hiz_tx,
                          float3 center_N,
                          float3 center_P,
                          int2 sample_texel,
                          float2 sample_uv,
                          int2 sample_offset) const
  {
    int2 sample_texel_fullres = sample_texel * raytrace_buf.fast_gi_resolution_scale +
                                raytrace_buf.fast_gi_resolution_bias;
    float sample_depth = texelFetch(hiz_tx, sample_texel_fullres, 0).r;

    bool is_valid;
    float3 sample_N = sample_normal_get(sample_texel, is_valid);
    float3 sample_P = drw_point_screen_to_world(float3(sample_uv, sample_depth));

    if (!is_valid) {
      return 0.0f;
    }

    float gauss = filter_gaussian_factor(1.5f, 1.5f);

    /* TODO(fclem): Scene parameter. 100.0f is dependent on scene scale. */
    float depth_weight = filter_planar_weight(center_N, center_P, sample_P, 100.0f);
    float spatial_weight = filter_gaussian_weight(gauss, length_squared(float2(sample_offset)));
    float normal_weight = filter_angle_weight(center_N, sample_N);

    return max(1e-6f, depth_weight * spatial_weight * normal_weight);
  }

  /* Used for resolve. */
  float sample_weight_get(float3 center_N,
                          float3 center_P,
                          int2 center_texel,
                          int2 sample_offset) const
  {
    int2 sample_texel = center_texel + sample_offset;
    int2 sample_texel_fullres = sample_texel * raytrace_buf.fast_gi_resolution_scale +
                                raytrace_buf.fast_gi_resolution_bias;
    float2 sample_uv = (float2(sample_texel_fullres) + 0.5f) * raytrace_buf.full_resolution_inv;

    float sample_depth = reverse_z::read(texelFetch(depth_tx, sample_texel_fullres, 0).r);

    bool is_valid;
    float3 sample_N = sample_normal_get(sample_texel, is_valid);
    float3 sample_P = drw_point_screen_to_world(float3(sample_uv, sample_depth));

    if (!is_valid) {
      return 0.0f;
    }

    /* TODO(fclem): Scene parameter. 10000.0f is dependent on scene scale. */
    float depth_weight = filter_planar_weight(center_N, center_P, sample_P, 10000.0f);
    float normal_weight = filter_angle_weight(center_N, sample_N);
    /* Some pixels might have no correct weight (depth & normal weights being very small).
     * To avoid them have invalid energy (because of float precision),
     * we weight all valid samples by a very small amount. */
    float epsilon_weight = 1e-4f;

    return max(epsilon_weight, depth_weight * normal_weight);
  }

  SphericalHarmonicL1<float4> load_sh(int2 texel) const
  {
    SphericalHarmonicL1<float4> sh;
    sh.L0.M0 = texelFetch(fast_gi_radiance_0_tx, texel, 0);
    sh.L1.Mn1 = texelFetch(fast_gi_radiance_1_tx, texel, 0);
    sh.L1.M0 = texelFetch(fast_gi_radiance_2_tx, texel, 0);
    sh.L1.Mp1 = texelFetch(fast_gi_radiance_3_tx, texel, 0);
    sh = spherical_harmonics::decompress(sh);
    return sh;
  }

  SphericalHarmonicL1<float4> load_sh(int2 texel, bool valid) const
  {
    if (!valid) {
      /* We need to avoid sampling if there no weight as the texture values could be undefined
       * (is_valid is false). */
      return {};
    }
    return load_sh(texel);
  }
};

struct SampleOutput {
  [[image(2, write, SFLOAT_16_16_16_16)]] image2D sh_0_img;
  [[image(3, write, UNORM_8_8_8_8)]] image2D sh_1_img;
  [[image(4, write, UNORM_8_8_8_8)]] image2D sh_2_img;
  [[image(5, write, UNORM_8_8_8_8)]] image2D sh_3_img;

  void write(int2 texel, SphericalHarmonicL1<float4> result)
  {
    result = spherical_harmonics::compress(result);
    imageStore(sh_0_img, texel, result.L0.M0);
    imageStore(sh_1_img, texel, result.L1.Mn1);
    imageStore(sh_2_img, texel, result.L1.M0);
    imageStore(sh_3_img, texel, result.L1.Mp1);
  }
};

struct Tiles {
  [[storage(7, read)]] const uint (&tiles_coord_buf)[];
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Setup Stage
 * \{ */

struct Setup {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[sampler(0)]] const sampler2DDepth depth_tx;
  [[sampler(1)]] const sampler2D in_radiance_tx;

  [[shared]] float3 neigbhor_data[RAYTRACE_GROUP_SIZE][RAYTRACE_GROUP_SIZE];

  [[image(0, write, RAYTRACE_RADIANCE_FORMAT)]] image2D out_radiance_mip0;
  [[image(1, write, RAYTRACE_RADIANCE_FORMAT)]] image2D out_radiance_mip1;
  [[image(2, write, RAYTRACE_RADIANCE_FORMAT)]] image2D out_radiance_mip2;
  [[image(3, write, RAYTRACE_RADIANCE_FORMAT)]] image2D out_radiance_mip3;
  [[image(4, write, UNORM_10_10_10_2)]] image2D out_normal_mip0;
  [[image(5, write, UNORM_10_10_10_2)]] image2D out_normal_mip1;
  [[image(6, write, UNORM_10_10_10_2)]] image2D out_normal_mip2;
  [[image(7, write, UNORM_10_10_10_2)]] image2D out_normal_mip3;
};

[[compute, local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void setup([[global_invocation_id]] const uint3 global_id,
           [[local_invocation_id]] const uint3 local_id,
           [[resource_table]] Setup &srt)
{
  int2 texel = int2(global_id.xy);
  int2 texel_fullres = texel * raytrace_buf.fast_gi_resolution_scale +
                       raytrace_buf.fast_gi_resolution_bias;

  /* Avoid loading texels outside texture range. */
  int2 extent = textureSize(gbuf_header_tx, 0).xy;
  texel_fullres = min(texel_fullres, extent - 1);

  /* Load Gbuffer. */
  const gbuffer::Layers gbuf = gbuffer::read_layers(texel_fullres);

  /* Tag processed pixel in the normal buffer for denoising speed. */
  bool is_processed = !gbuf.header.is_empty();

  /* Export normal. */
  /* FIXME: This is zero for opaque layer when we are processing the refraction layer.
   * This is because the GBuffer header was cleared in between the layers. The refraction layer
   * currently have incorrect fast GI coming from opaque layer. */
  float3 vN = drw_normal_world_to_view(gbuf.surface_N());

  if (!is_processed) {
    vN = float3(0.0f, 0.0f, 1.0f);
  }

  /* Compress as the format is unsigned. */
  float3 vN_unit = vN * 0.5f + 0.5f;
  imageStoreFast(srt.out_normal_mip0, texel, float4(vN_unit, float(is_processed)));

  srt.neigbhor_data[local_id.y][local_id.x] = vN_unit;
  barrier();

  /* Downsample mip0 to the 3 other mips. */
  /* Note we have to manually unroll the loop because of lack of image array.
   * Using a macro isn't compatible with BSL processing. */
  {
    constexpr uint lod = 1;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 normal_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x] +
                           srt.neigbhor_data[local_id.y][local_id.x + stride] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                          0.25f;
      imageStoreFast(srt.out_normal_mip1, texel >> lod, float4(normal_avg, 0.0));
      srt.neigbhor_data[local_id.y][local_id.x] = normal_avg;
    }
  }
  barrier();
  {
    constexpr uint lod = 2;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 normal_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x] +
                           srt.neigbhor_data[local_id.y][local_id.x + stride] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                          0.25f;
      imageStoreFast(srt.out_normal_mip2, texel >> lod, float4(normal_avg, 0.0));
      srt.neigbhor_data[local_id.y][local_id.x] = normal_avg;
    }
  }
  barrier();
  {
    constexpr uint lod = 3;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 normal_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x] +
                           srt.neigbhor_data[local_id.y][local_id.x + stride] +
                           srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                          0.25f;
      imageStoreFast(srt.out_normal_mip3, texel >> lod, float4(normal_avg, 0.0));
    }
  }
  barrier();

  /* Re-project radiance. */
  float2 uv = (float2(texel_fullres) + 0.5f) / float2(textureSize(srt.depth_tx, 0).xy);
  float depth = reverse_z::read(texelFetch(srt.depth_tx, texel_fullres, 0).r);
  float3 P = drw_point_screen_to_world(float3(uv, depth));

  float3 ssP_prev = drw_ndc_to_screen(project_point(raytrace_buf.history_persmat, P));

  float4 radiance = textureLod(srt.in_radiance_tx, ssP_prev.xy, 0.0f);
  radiance = colorspace::brightness_clamp_max(radiance, uniform_buf.clamp.surface_indirect);
  imageStoreFast(srt.out_radiance_mip0, texel, radiance);

  srt.neigbhor_data[local_id.y][local_id.x] = radiance.rgb;
  barrier();

  /* Downsample mip0 to the 3 other mips. */
  {
    constexpr uint lod = 1;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 radiance_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x] +
                             srt.neigbhor_data[local_id.y][local_id.x + stride] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                            0.25f;
      imageStoreFast(srt.out_radiance_mip1, texel >> lod, float4(radiance_avg, 0.0));
      srt.neigbhor_data[local_id.y][local_id.x] = radiance_avg;
    }
  }
  barrier();
  {
    constexpr uint lod = 2;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 radiance_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x] +
                             srt.neigbhor_data[local_id.y][local_id.x + stride] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                            0.25f;
      imageStoreFast(srt.out_radiance_mip2, texel >> lod, float4(radiance_avg, 0.0));
      srt.neigbhor_data[local_id.y][local_id.x] = radiance_avg;
    }
  }
  barrier();
  {
    constexpr uint lod = 3;
    if (all(equal(local_id.xy & ((1u << lod) - 1u), uint2(0)))) {
      uint stride = (1u << (lod - 1u));
      float3 radiance_avg = (srt.neigbhor_data[local_id.y][local_id.x] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x] +
                             srt.neigbhor_data[local_id.y][local_id.x + stride] +
                             srt.neigbhor_data[local_id.y + stride][local_id.x + stride]) *
                            0.25f;
      imageStoreFast(srt.out_radiance_mip3, texel >> lod, float4(radiance_avg, 0.0));
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scan Stage
 * \{ */

struct Constants {
  [[specialization_constant(2)]] const int slice_count;
  [[specialization_constant(8)]] const int step_count;
  [[specialization_constant(false)]] const bool ao_only;
};

struct Scan {
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[sampler(0)]] const sampler2D screen_radiance_tx;
  [[sampler(1)]] const sampler2D screen_normal_tx;
};

[[metal_max_total_threads_per_threadgroup(400)]] /* Tweak performance on metal. */
[[compute]] [[local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void scan([[work_group_id]] const uint3 group_id,
          [[local_invocation_id]] const uint3 local_id,
          [[resource_table]] Scan &srt,
          [[resource_table]] const Tiles &tiles,
          [[resource_table]] SampleOutput &sh_out,
          [[resource_table]] Constants &constants)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  int2 texel_fullres = texel * raytrace_buf.fast_gi_resolution_scale +
                       raytrace_buf.fast_gi_resolution_bias;

  /* Avoid tracing the outside border if dispatch is too big. */
  int2 extent = textureSize(gbuf_header_tx, 0).xy;

  /* Avoid loading texels outside texture range.
   * This can happen even after the check above in non-power-of-2 textures. */
  texel_fullres = min(texel_fullres, extent - 1);

  /* Do not trace where nothing was rendered. */
  if (texelFetch(gbuf_header_tx, int3(texel_fullres, 0), 0).r == 0u) {
#if 0 /* This is not needed as the next stage doesn't do bilinear filtering. */
    imageStore(fast_gi_radiance_0_img, texel, float4(0.0f));
    imageStore(fast_gi_radiance_1_img, texel, float4(0.0f));
    imageStore(fast_gi_radiance_2_img, texel, float4(0.0f));
    imageStore(fast_gi_radiance_3_img, texel, float4(0.0f));
#endif
    return;
  }

  float2 uv = (float2(texel_fullres) + 0.5f) * raytrace_buf.full_resolution_inv;
  float depth = texelFetch(hiz_tx, texel_fullres, 0).r;
  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  float3 vN = texelFetch(srt.screen_normal_tx, texel, 0).rgb * 2.0f - 1.0f;

  float4 noise = utility_tx_fetch(utility_tx, float2(texel), UTIL_BLUE_NOISE_LAYER);
  noise = fract(noise + sampling_rng_3D_get(SAMPLING_AO_U).xyzx);

  SphericalHarmonicL1<float4> result = eevee::fast_gi::eval<SphericalHarmonicL1<float4>>(
      raytrace_buf.fast_gi_thickness,
      hiz_tx,
      srt.screen_radiance_tx,
      srt.screen_normal_tx,
      vP,
      vN,
      noise,
      uniform_buf.ao.pixel_size,
      uniform_buf.ao.gi_distance,
      uniform_buf.ao.angle_bias,
      constants.slice_count,
      constants.step_count,
      false,
      constants.ao_only);

  sh_out.write(texel, result);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Denoise
 * \{ */

struct Denoise {
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo draw_view;
};

[[metal_max_total_threads_per_threadgroup(400)]] /* Tweak performance on metal. */
[[compute]] [[local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void denoise([[work_group_id]] const uint3 group_id,
             [[local_invocation_id]] const uint3 local_id,
             [[resource_table]] Denoise & /*srt*/,
             [[resource_table]] SampleInput &sh_in,
             [[resource_table]] SampleOutput &sh_out,
             [[resource_table]] Tiles &tiles)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  float2 texel_size = 1.0f / float2(textureSize(sh_in.fast_gi_radiance_0_tx, 0).xy);
  int2 texel_fullres = texel * raytrace_buf.fast_gi_resolution_scale +
                       raytrace_buf.fast_gi_resolution_bias;

  bool is_valid;
  float center_depth = texelFetch(hiz_tx, texel_fullres, 0).r;
  float2 center_uv = float2(texel) * texel_size;
  float3 center_P = drw_point_screen_to_world(float3(center_uv, center_depth));
  float3 center_N = sh_in.sample_normal_get(texel, is_valid);

  if (!is_valid) {
#if 0 /* This is not needed as the next stage doesn't do bilinear filtering. */
    imageStore(sh_out.sh_0_img, texel, float4(0.0f));
    imageStore(sh_out.sh_1_img, texel, float4(0.0f));
    imageStore(sh_out.sh_2_img, texel, float4(0.0f));
    imageStore(sh_out.sh_3_img, texel, float4(0.0f));
#endif
    return;
  }

  SphericalHarmonicL1<float4> accum_sh = {};
  float accum_weight = 0.0f;
  /* 3x3 filter. */
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      int2 sample_offset = int2(x, y);
      int2 sample_texel = texel + sample_offset;
      float2 sample_uv = (float2(sample_texel) + 0.5f) * texel_size;
      float sample_weight = sh_in.sample_weight_get(
          hiz_tx, center_N, center_P, sample_texel, sample_uv, sample_offset);
      /* We need to avoid sampling if there no weight as the texture values could be undefined
       * (is_valid is false). */
      if (sample_weight > 0.0f) {
        SphericalHarmonicL1<float4> sample_sh = sh_in.load_sh(sample_texel);
        accum_sh = spherical_harmonics::madd(sample_sh, sample_weight, accum_sh);
        accum_weight += sample_weight;
      }
    }
  }
  accum_sh = spherical_harmonics::mul(accum_sh, safe_rcp(accum_weight));
  sh_out.write(texel, accum_sh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resolve
 * \{ */

struct Resolve {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo eevee_gbuffer_data;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[image(3, read_write, RAYTRACE_RADIANCE_FORMAT)]] image2D closure0_img;
  [[image(4, read_write, RAYTRACE_RADIANCE_FORMAT)]] image2D closure1_img;
  [[image(5, read_write, RAYTRACE_RADIANCE_FORMAT)]] image2D closure2_img;
};

[[metal_max_total_threads_per_threadgroup(400)]] /* Tweak performance on metal. */
[[compute]] [[local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void resolve([[work_group_id]] const uint3 group_id,
             [[local_invocation_id]] const uint3 local_id,
             [[resource_table]] const Tiles &tiles,
             [[resource_table]] const SampleInput &sh_in,
             [[resource_table]] Resolve &srt,
             [[resource_table]] const LightprobeRenderData &lightprobes)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles.tiles_coord_buf[group_id.x]);
  int2 texel_fullres = int2(local_id.xy + tile_coord * tile_size);

  int2 texel = max(int2(0), texel_fullres - raytrace_buf.fast_gi_resolution_bias) /
               raytrace_buf.fast_gi_resolution_scale;

  int2 extent = textureSize(gbuf_header_tx, 0).xy;
  if (any(greaterThanEqual(texel_fullres, extent))) {
    return;
  }

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel_fullres);

  if (gbuf.header.is_empty()) {
    return;
  }

  float2 center_uv = (float2(texel_fullres) + 0.5f) * raytrace_buf.full_resolution_inv;
  float center_depth = reverse_z::read(texelFetch(sh_in.depth_tx, texel_fullres, 0).r);
  float3 center_P = drw_point_screen_to_world(float3(center_uv, center_depth));
  float3 center_N = gbuf.surface_N();

  SphericalHarmonicL1<float4> accum_sh;
  if (raytrace_buf.fast_gi_resolution_scale == 1) {
    accum_sh = sh_in.load_sh(texel, true);
  }
  else {
    float2 interp = float2(texel_fullres - texel * raytrace_buf.fast_gi_resolution_scale -
                           raytrace_buf.fast_gi_resolution_bias) /
                    float2(raytrace_buf.fast_gi_resolution_scale);
    float4 interp4 = float4(interp, 1.0f - interp);
    float4 bilinear_weight = interp4.zxzx * interp4.wwyy;

    float4 bilateral_weights;
    bilateral_weights.x = sh_in.sample_weight_get(center_N, center_P, texel, int2(0, 0));
    bilateral_weights.y = sh_in.sample_weight_get(center_N, center_P, texel, int2(1, 0));
    bilateral_weights.z = sh_in.sample_weight_get(center_N, center_P, texel, int2(0, 1));
    bilateral_weights.w = sh_in.sample_weight_get(center_N, center_P, texel, int2(1, 1));

    float4 weights = bilateral_weights * bilinear_weight;

    SphericalHarmonicL1<float4> sh_00 = sh_in.load_sh(texel + int2(0, 0), weights.x > 0.0f);
    SphericalHarmonicL1<float4> sh_10 = sh_in.load_sh(texel + int2(1, 0), weights.y > 0.0f);
    SphericalHarmonicL1<float4> sh_01 = sh_in.load_sh(texel + int2(0, 1), weights.z > 0.0f);
    SphericalHarmonicL1<float4> sh_11 = sh_in.load_sh(texel + int2(1, 1), weights.w > 0.0f);

    /* Avoid another division at the end. Normalize the weights upfront. */
    weights *= safe_rcp(reduce_add(weights));

    accum_sh = spherical_harmonics::mul(sh_00, weights.x);
    accum_sh = spherical_harmonics::madd(sh_10, weights.y, accum_sh);
    accum_sh = spherical_harmonics::madd(sh_01, weights.z, accum_sh);
    accum_sh = spherical_harmonics::madd(sh_11, weights.w, accum_sh);
  }

  accum_sh = spherical_harmonics::rotate(to_float3x3(drw_view().viewinv), accum_sh);

  float3 P = center_P;
  float3 Ng = center_N;
  float3 V = drw_world_incident_vector(P);

  LightProbeSample samp = lightprobes.load(float2(texel_fullres), P, Ng, V);

  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                             clamp_indirect);

  const uchar closure_count = gbuf.header.closure_len();
  const uint3 bin_indices = gbuf.header.bin_index_per_layer();
  const Thickness thickness = gbuffer::read_thickness(gbuf.header, texel_fullres);

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);

    float roughness = closure_apparent_roughness_get(cl);

    float mix_fac = saturate(roughness * raytrace_buf.roughness_mask_scale -
                             raytrace_buf.roughness_mask_bias);
    bool use_raytrace = mix_fac < 1.0f;
    bool use_fast_gi = mix_fac > 0.0f;

    if (!use_fast_gi) {
      continue;
    }

    LightProbeRay ray = bxdf_lightprobe_ray(cl, P, V, thickness);

    float3 L = ray.dominant_direction;

    /* Evaluate lighting from fast GI scan. */
    float4 radiance_with_visibility = accum_sh.evaluate_lambert(L);
    float3 radiance = radiance_with_visibility.xyz;
    /* Evaluate occlusion from fast GI scan. */
    /* The energy amount from the visibility factor is supposed to be a pure lambertian visibility
     * (which integrate to PI over the hemisphere). However, the tracing step weight the incoming
     * radiance by 4 PI (and with it the visibility). So the expected computation should be
     * `accum_sh.evaluate(L).w / 4.0f`. But in order to save some complexity, we approximate using
     * the `evaluate_lambert` version even if not completely correct (max 3% errors). */
    float distant_radiance_visibility = saturate(radiance_with_visibility.w / 3.0f);

    if (closure_has_transmission(cl.type)) {
      /* We only recorded visibility and radiance for the upper hemisphere.
       * Discard result for transmission closures. */
      distant_radiance_visibility = 1.0f;
      radiance = float3(0.0);
    }

    /* Apply missing distant lighting. */
    radiance += distant_radiance_visibility * samp.volume_irradiance.evaluate_lambert(L).rgb;

    uchar layer_index = bin_indices[i];

    float4 radiance_fast_gi = float4(radiance, 0.0f);
    float4 radiance_raytrace = float4(0.0f);
    if (use_raytrace) {
      /* TODO(fclem): Layered texture. */
      if (layer_index == 0u) {
        radiance_raytrace = imageLoad(srt.closure0_img, texel_fullres);
      }
      else if (layer_index == 1u) {
        radiance_raytrace = imageLoad(srt.closure1_img, texel_fullres);
      }
      else if (layer_index == 2u) {
        radiance_raytrace = imageLoad(srt.closure2_img, texel_fullres);
      }
    }
    float4 radiance_mixed = mix(radiance_raytrace, radiance_fast_gi, mix_fac);

    /* TODO(fclem): Layered texture. */
    if (layer_index == 0u) {
      imageStore(srt.closure0_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 1u) {
      imageStore(srt.closure1_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 2u) {
      imageStore(srt.closure2_img, texel_fullres, radiance_mixed);
    }
  }
}

}  // namespace fast_gi

PipelineCompute fast_gi_setup(fast_gi::setup);
PipelineCompute fast_gi_scan(fast_gi::scan);
PipelineCompute fast_gi_denoise(fast_gi::denoise);
PipelineCompute fast_gi_resolve(fast_gi::resolve);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray-tracing
 * \{ */

struct AOPass {
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;
  [[legacy_info]] ShaderCreateInfo eevee_utility_texture;
  [[legacy_info]] ShaderCreateInfo eevee_hiz_data;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[image(0, read, SFLOAT_16_16_16_16)]] image2DArray in_normal_img;
  [[image(1, write, SFLOAT_16)]] image2DArray out_ao_img;

  [[specialization_constant(2)]] const int ao_slice_count;
  [[specialization_constant(8)]] const int ao_step_count;

  [[sampler(0)]] const sampler2D dummy_tx;

  [[push_constant]] const int in_normal_img_layer_index;
  [[push_constant]] const int out_ao_img_layer_index;
};

[[compute]] [[local_size(AMBIENT_OCCLUSION_PASS_TILE_SIZE, AMBIENT_OCCLUSION_PASS_TILE_SIZE)]]
void occlusion_pass([[global_invocation_id]] const uint3 global_id, [[resource_table]] AOPass &srt)
{
  int2 texel = int2(global_id.xy);
  int2 extent = imageSize(srt.in_normal_img).xy;
  if (any(greaterThanEqual(texel, extent))) {
    return;
  }

  float2 uv = (float2(texel) + float2(0.5f)) / float2(extent);
  float depth = texelFetch(hiz_tx, texel, 0).r;

  if (depth == 1.0f) {
    /* Do not trace for background */
    imageStoreFast(srt.out_ao_img, int3(texel, srt.out_ao_img_layer_index), float4(0.0f));
    return;
  }

  float3 vP = drw_point_screen_to_view(float3(uv, depth));
  float3 N = imageLoad(srt.in_normal_img, int3(texel, srt.in_normal_img_layer_index)).xyz;
  float3 vN = drw_normal_world_to_view(N);

  auto &lut_tx = sampler_get(eevee_utility_texture, utility_tx);
  float4 noise = utility_tx_fetch(lut_tx, float2(texel), UTIL_BLUE_NOISE_LAYER);
  noise = fract(noise + sampling_rng_3D_get(SAMPLING_AO_U).xyzx);

  float result = eevee::fast_gi::eval<float>(raytrace_buf.fast_gi_thickness,
                                             hiz_tx,
                                             srt.dummy_tx,
                                             srt.dummy_tx,
                                             vP,
                                             vN,
                                             noise,
                                             uniform_buf.ao.pixel_size,
                                             uniform_buf.ao.distance,
                                             uniform_buf.ao.angle_bias,
                                             srt.ao_slice_count,
                                             srt.ao_step_count,
                                             false,
                                             true);

  imageStoreFast(
      srt.out_ao_img, int3(texel, srt.out_ao_img_layer_index), float4(saturate(result)));
}

/** \} */

PipelineCompute ambient_occlusion_pass(occlusion_pass);

}  // namespace eevee
