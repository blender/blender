/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)

#include "draw_view_lib.glsl"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "gpu_shader_math_matrix_transform_lib.glsl"

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

struct UnifiedVolumeProperties {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;

  [[image(VOLUME_PROP_SCATTERING_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_scattering_img;
  [[image(VOLUME_PROP_EXTINCTION_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_extinction_img;
  [[image(VOLUME_PROP_EMISSION_IMG_SLOT, read, UFLOAT_11_11_10)]] image3D in_emission_img;
  [[image(VOLUME_PROP_PHASE_IMG_SLOT, read, SFLOAT_16)]] image3D in_phase_img;
  [[image(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, read, SFLOAT_16)]] image3D in_phase_weight_img;
};

/* Per froxel jitter to break slices and flickering.
 * Wrapped so that changing it is easier. */
float volume_froxel_jitter(int2 froxel, float offset)
{
  return interleaved_gradient_noise(float2(froxel), 0.0f, offset);
}

/* Volume froxel texture normalized linear Z to view space Z.
 * Not dependent on projection matrix (as long as drw_view_is_perspective is consistent). */
float volume_z_to_view_z(float z)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  if (drw_view_is_perspective()) {
    /* Exponential distribution. */
    return (exp2(z / distribution) - near) / far;
  }
  else {
    /* Linear distribution. */
    return near + (far - near) * z;
  }
}

/* View space Z to volume froxel texture normalized linear Z.
 * Not dependent on projection matrix (as long as drw_view_is_perspective is consistent). */
float view_z_to_volume_z(float depth, float near, float far, float distribution)
{
  if (drw_view_is_perspective()) {
    /* Exponential distribution. */
    return distribution * log2(depth * far + near);
  }
  else {
    /* Linear distribution. */
    return (depth - near) / (far - near);
  }
}
float view_z_to_volume_z(float depth)
{
  return view_z_to_volume_z(depth,
                            uniform_buf.volumes.depth_near,
                            uniform_buf.volumes.depth_far,
                            uniform_buf.volumes.depth_distribution);
}

/* Jittered volume texture normalized coordinates to view space position. */
float3 volume_jitter_to_view(float3 coord)
{
  /* Since we use an infinite projection matrix for rendering inside the jittered volumes,
   * we need to use a different matrix to reconstruct positions as the infinite matrix is not
   * always invertible. */
  float4x4 winmat = uniform_buf.volumes.winmat_finite;
  float4x4 wininv = uniform_buf.volumes.wininv_finite;
  /* Input coordinates are in jittered volume texture space. */
  float view_z = volume_z_to_view_z(coord.z);
  /* We need to recover the NDC position for correct perspective divide. */
  float ndc_z = drw_perspective_divide(winmat * float4(0.0f, 0.0f, view_z, 1.0f)).z;
  float2 ndc_xy = drw_screen_to_ndc(coord.xy);
  /* NDC to view. */
  return drw_perspective_divide(wininv * float4(ndc_xy, ndc_z, 1.0f)).xyz;
}

/* View space position to jittered volume texture normalized coordinates. */
float3 volume_view_to_jitter(float3 vP)
{
  /* Since we use an infinite projection matrix for rendering inside the jittered volumes,
   * we need to use a different matrix to reconstruct positions as the infinite matrix is not
   * always invertible. */
  float4x4 winmat = uniform_buf.volumes.winmat_finite;
  /* View to ndc. */
  float3 ndc_P = drw_perspective_divide(winmat * float4(vP, 1.0f));
  /* Here, screen is the same as volume texture UVW space. */
  return float3(drw_ndc_to_screen(ndc_P.xy), view_z_to_volume_z(vP.z));
}

/* Volume texture normalized coordinates (UVW) to render screen (UV).
 * Expect active view to be the main view. */
float3 volume_resolve_to_screen(float3 coord)
{
  coord.z = volume_z_to_view_z(coord.z);
  coord.z = drw_depth_view_to_screen(coord.z);
  coord.xy /= uniform_buf.volumes.coord_scale;
  return coord;
}
/* Render screen (UV) to volume texture normalized coordinates (UVW).
 * Expect active view to be the main view. */
float3 volume_screen_to_resolve(float3 coord)
{
  coord.xy *= uniform_buf.volumes.coord_scale;
  coord.z = drw_depth_screen_to_view(coord.z);
  coord.z = view_z_to_volume_z(coord.z);
  return coord;
}

/* Returns the uvw (normalized coordinate) of a froxel in the previous frame.
 * Returns float3(-1) if history is unavailable. */
float3 volume_history_uvw_get(int3 froxel)
{
  float4x4 wininv = uniform_buf.volumes.wininv_stable;
  float4x4 winmat = uniform_buf.volumes.winmat_stable;
  /* We can't re-project by a simple matrix multiplication. We first need to remap to the view Z,
   * then transform, then remap back to Volume range. */
  float3 uvw = (float3(froxel) + 0.5f) * uniform_buf.volumes.inv_tex_size;
  float3 ndc_P = drw_screen_to_ndc(uvw);
  /* We need to recover the NDC position for correct perspective divide. */
  float view_z = volume_z_to_view_z(uvw.z);
  ndc_P.z = drw_perspective_divide(winmat * float4(0.0f, 0.0f, view_z, 1.0f)).z;
  /* NDC to view. */
  float3 vs_P = project_point(wininv, ndc_P);

  /* Transform to previous camera view space. */
  float3 vs_P_history = transform_point(uniform_buf.volumes.curr_view_to_past_view, vs_P);

  /* View to NDC. */
  float4 hs_P_history = uniform_buf.volumes.history_winmat_stable * float4(vs_P_history, 1.0f);
  float3 ndc_P_history = drw_perspective_divide(hs_P_history);

  if (hs_P_history.w < 0.0f || any(greaterThan(abs(ndc_P_history.xy), float2(1.0f)))) {
    return float3(-1.0f);
  }

  float3 uvw_history;
  uvw_history.xy = drw_ndc_to_screen(ndc_P_history.xy);
  uvw_history.z = view_z_to_volume_z(vs_P_history.z,
                                     uniform_buf.volumes.history_depth_near,
                                     uniform_buf.volumes.history_depth_far,
                                     uniform_buf.volumes.history_depth_distribution);

  if (uvw_history.z < 0.0f || uvw_history.z > 1.0f) {
    return float3(-1.0f);
  }
  return uvw_history;
}

float volume_phase_function_isotropic()
{
  return 1.0f / (4.0f * M_PI);
}

float volume_phase_function(float3 V, float3 L, float g)
{
  /* Henyey-Greenstein. */
  float cos_theta = dot(V, L);
  g = clamp(g, -1.0f + 1e-3f, 1.0f - 1e-3f);
  float sqr_g = g * g;
  return (1 - sqr_g) / max(1e-8f, 4.0f * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0f / 2.0f));
}

SphericalHarmonicL1<float4> volume_phase_function_as_sh_L1(float3 V, float g)
{
  /* Compute rotated zonal harmonic.
   * From Bartlomiej Wronsky
   * "Volumetric Fog: Unified compute shader based solution to atmospheric scattering" page 55
   * SIGGRAPH 2014
   * https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
   */
  SphericalHarmonicL1<float4> sh;
  sh.L0.M0 = spherical_harmonics::L0_M0_coef(V) * float4(1.0f);
  sh.L1.Mn1 = spherical_harmonics::L1_Mn1_coef(V) * float4(g);
  sh.L1.M0 = spherical_harmonics::L1_M0_coef(V) * float4(g);
  sh.L1.Mp1 = spherical_harmonics::L1_Mp1_coef(V) * float4(g);
  return sh;
}

struct VolumeResolveSample {
  float3 transmittance;
  float3 scattering;
};

struct UnifiedVolumeData {
  [[legacy_info]] ShaderCreateInfo eevee_global_ubo;
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[sampler(VOLUME_TRANSMITTANCE_TEX_SLOT)]] sampler3D volume_transmittance_tx;
  [[sampler(VOLUME_SCATTERING_TEX_SLOT)]] sampler3D volume_scattering_tx;

  VolumeResolveSample resolve(float3 ndc_P) const
  {
    float3 coord = volume_screen_to_resolve(ndc_P);

    /* Volumes objects have the same aliasing problems has shadow maps.
     * To fix this we need a quantization bias (the size of a step in Z) and a slope bias
     * (multiplied by the size of a froxel in 2D). */
    coord.z -= uniform_buf.volumes.inv_tex_size.z;
    /* TODO(fclem): Slope bias. */

    VolumeResolveSample volume;
    volume.scattering = texture(volume_scattering_tx, coord).rgb;
    volume.transmittance = texture(volume_transmittance_tx, coord).rgb;
    return volume;
  }
};
