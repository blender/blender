/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"
#include "eevee_closure.bsl.hh"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_gbuffer_read.bsl.hh"
#include "eevee_lightprobe.bsl.hh"
#include "eevee_lightprobe_plane.bsl.hh"
#include "eevee_ray_trace_screen_lib.bsl.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_spherical_harmonics.bsl.hh"
#include "eevee_uniform.bsl.hh"

namespace eevee::raytrace {

namespace screen {

struct Resources {
  [[specialization_constant(0)]] int closure_index;
  [[specialization_constant(true)]] bool trace_refraction;

  [[storage(5, read)]] const uint (&tiles_coord_buf)[];

  [[sampler(0)]] sampler2DDepth depth_tx;
  [[sampler(1)]] sampler2D radiance_front_tx;
  [[sampler(2)]] sampler2D radiance_back_tx;
  [[sampler(4)]] sampler2D hiz_front_tx;
  [[sampler(5)]] sampler2D hiz_back_tx;

  [[image(0, read, SFLOAT_16_16_16_16)]] const image2D ray_data_img;
  [[image(1, write, RAYTRACE_RAYTIME_FORMAT)]] image2D ray_time_img;
  [[image(2, write, RAYTRACE_RADIANCE_FORMAT)]] image2D ray_radiance_img;
};

/**
 * Use screen space tracing against depth buffer to find intersection with the scene.
 */
[[compute,
  local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE),
  metal_max_total_threads_per_threadgroup(400)]]
void trace([[resource_table]] Resources &srt,
           [[resource_table]] const LightprobeRenderData &lightprobes,
           [[resource_table]] const Uniform &uni,
           [[resource_table]] const draw::View &views,
           [[resource_table]] const Sampling &sampling,
           [[resource_table]] const gbuffer::Reader &reader,
           [[work_group_id]] const uint3 group_id,
           [[local_invocation_id]] const uint3 local_id)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(srt.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  /* Check whether texel is out of bounds for all cases, so we can utilize fast
   * texture functions and early exit if not. */
  if (any(greaterThanEqual(texel, imageSize(srt.ray_data_img).xy)) ||
      any(lessThan(texel, int2(0))))
  {
    return;
  }

  float4 ray_data_im = imageLoadFast(srt.ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv < 0.0f) {
    /* Ray destined to planar trace. */
    return;
  }

  if (ray_pdf_inv == 0.0f) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(srt.ray_time_img, texel, float4(0.0f));
    imageStoreFast(srt.ray_radiance_img, texel, float4(0.0f));
    return;
  }

  const ViewMatrices view = views.get(0);

  int2 texel_fullres = texel * uni.raytrace_buf.trace_pixel_scale +
                       uni.raytrace_buf.trace_pixel_offset;

  gbuffer::Header gbuf_header = reader.read_header(texel_fullres);
  ClosureType closure_type = gbuffer::mode_to_closure_type(
      gbuf_header.bin_type(srt.closure_index));

  float depth = reverse_z::read(texelFetch(srt.depth_tx, texel_fullres, 0).r);
  float2 uv = (float2(texel_fullres) + 0.5f) * uni.raytrace_buf.full_resolution_inv;

  float3 P = view.point_screen_to_world(float3(uv, depth));
  float3 V = view.world_incident_vector(P);
  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (srt.closure_index == 0) {
    const Thickness thickness = reader.read_thickness(gbuf_header, texel_fullres);
    if (thickness.value() != 0.0f) {
      ClosureUndetermined cl = reader.read_bin(texel_fullres, srt.closure_index);
      ray = raytrace_thickness_ray_amend(ray, cl, V, thickness);
    }
  }

  float3 radiance = float3(0.0f);
  float noise_offset = sampling.rng_1D_get(SAMPLING_RAYTRACE_W);
  float rand_trace = interleaved_gradient_noise(float2(texel), 5.0f, noise_offset);

  ClosureUndetermined cl = reader.read_bin(texel_fullres, srt.closure_index);
  float roughness = closure_apparent_roughness_get(cl);

  /* Transform the ray into view-space. */
  Ray ray_view;
  ray_view.origin = transform_point(view.viewmat, ray.origin);
  ray_view.direction = transform_direction(view.viewmat, ray.direction);
  /* Extend the ray to cover the whole view. */
  ray_view.max_time = abs(view.far() - view.near());

  ScreenTraceHitData hit;
  hit.valid = false;
  /* This huge branch is likely to be a huge issue for performance.
   * We could split the shader but that would mean to dispatch some area twice for the same closure
   * index. Another idea is to put both HiZ buffer in the same texture and dynamically access one
   * or the other. But that might also impact performance. */
  if (!closure_has_transmission(closure_type)) {
    hit = raytrace_screen(view,
                          uni.raytrace_buf,
                          uni.uniform_buf.hiz,
                          srt.hiz_front_tx,
                          rand_trace,
                          roughness,
                          false, /* allow_self_intersection */
                          ray_view);

    if (hit.valid) {
      float3 hit_P = transform_point(view.viewinv, hit.v_hit_P);
      /* TODO(@fclem): Split matrix multiply for precision. */
      float2 history_ndc_hit_P = project_point(uni.raytrace_buf.history_persmat, hit_P).xy;
      /* Make sure to tag hits that _were_ out of view as no hit. Otherwise the history is sampled
       * with clamp to border mode, which can introduce too much energy if the border pixels are
       * bright. */
      hit.valid = all(lessThan(abs(history_ndc_hit_P), float2(1.0f)));

      float2 history_ss_hit_P = history_ndc_hit_P * 0.5f + 0.5f;

      /* Fetch radiance at hit-point. */
      radiance = raytrace_sample_screen(
          view, srt.radiance_front_tx, uni.raytrace_buf, hit, roughness, history_ss_hit_P);

      if (hit.hit_backface) {
        radiance *= uni.raytrace_buf.backface_hit_scale;

        if (!uni.raytrace_buf.use_backface_hit) {
          hit.valid = false;
        }
      }
    }
  }
  else if (srt.trace_refraction) {
    hit = raytrace_screen(view,
                          uni.raytrace_buf,
                          uni.uniform_buf.hiz,
                          srt.hiz_back_tx,
                          rand_trace,
                          roughness,
                          true, /* allow_self_intersection */
                          ray_view);

    if (hit.valid) {
      /* Fetch radiance at hit-point. */
      radiance = raytrace_sample_screen(
          view, srt.radiance_back_tx, uni.raytrace_buf, hit, roughness, hit.ss_hit_P.xy);
    }
  }

  if (!hit.valid) {
    /* Using ray direction as geometric normal to bias the sampling position.
     * This is faster than loading the gbuffer again and averages between reflected and normal
     * direction over many rays. */
    float3 Ng = ray.direction;
    /* Fall back to nearest light-probe. */
    LightProbeSample samp = lightprobes.load(float2(texel), ray.origin, Ng, V);
    /* Clamp SH to have parity with forward evaluation. */
    float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;
    samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                               clamp_indirect);
    radiance = lightprobes.eval_direction(samp, ray.origin, ray.direction, roughness);
    /* Set point really far for correct reprojection of background. */
    hit.time = 10000.0f;
  }

  radiance = colorspace::brightness_clamp_max(radiance, uni.uniform_buf.clamp.surface_indirect);

  imageStoreFast(srt.ray_time_img, texel, float4(hit.time));
  imageStoreFast(srt.ray_radiance_img, texel, float4(radiance, 0.0f));
}

}  // namespace screen

namespace planar {

struct Resources {
  [[specialization_constant(0)]] int closure_index;

  [[storage(5, read)]] const uint (&tiles_coord_buf)[];

  [[sampler(2)]] sampler2DDepth depth_tx;

  [[image(0, read_write, SFLOAT_16_16_16_16)]] image2D ray_data_img;
  [[image(1, write, RAYTRACE_RAYTIME_FORMAT)]] image2D ray_time_img;
  [[image(2, write, RAYTRACE_RADIANCE_FORMAT)]] image2D ray_radiance_img;
};

/**
 * Use screen space tracing against depth buffer of recorded planar capture to find intersection
 * with the scene and its radiance.
 * This pass runs before the screen trace and evaluates valid rays for planar probes. These rays
 * are then tagged to avoid re-evaluation by screen trace.
 */
[[compute, local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void trace([[resource_table]] Resources &srt,
           [[resource_table]] const LightprobeRenderData &lightprobes,
           [[resource_table]] const LightprobePlaneRenderData &lightprobe_planes,
           [[resource_table]] const Uniform &uni,
           [[resource_table]] const draw::View &views,
           [[resource_table]] const Sampling &sampling,
           [[resource_table]] const gbuffer::Reader &reader,
           [[work_group_id]] const uint3 group_id,
           [[local_invocation_id]] const uint3 local_id)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(srt.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  /* Check if texel is out of bounds,
   * so we can utilize fast texture functions and early-out if not. */
  if (any(greaterThanEqual(texel, imageSize(srt.ray_time_img).xy))) {
    return;
  }

  float4 ray_data_im = imageLoadFast(srt.ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv == 0.0f) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(srt.ray_time_img, texel, float4(0.0f));
    imageStoreFast(srt.ray_radiance_img, texel, float4(0.0f));
    return;
  }

  int2 texel_fullres = texel * uni.raytrace_buf.trace_pixel_scale +
                       uni.raytrace_buf.trace_pixel_offset;

  gbuffer::Header gbuf_header = reader.read_header(texel_fullres);
  ClosureType closure_type = gbuffer::mode_to_closure_type(
      gbuf_header.bin_type(srt.closure_index));

  if (closure_has_transmission(closure_type)) {
    /* Planar light-probes cannot trace refraction yet. */
    return;
  }

  const ViewMatrices view = views.get(0);

  ClosureUndetermined cl = reader.read_bin(texel_fullres, srt.closure_index);
  float roughness = closure_apparent_roughness_get(cl);

  float depth = reverse_z::read(texelFetch(srt.depth_tx, texel_fullres, 0).r);
  float2 uv = (float2(texel_fullres) + 0.5f) * uni.raytrace_buf.full_resolution_inv;

  float3 P = view.point_screen_to_world(float3(uv, depth));
  float3 V = view.world_incident_vector(P);

  int planar_id = lightprobe_planes.select_probe(P, ray_data_im.xyz);
  if (planar_id == -1) {
    return;
  }

  PlanarProbeData planar = lightprobe_planes.probe_planar_buf[planar_id];

  /* Tag the ray data so that screen trace will not try to evaluate it and override the result. */
  imageStoreFast(srt.ray_data_img, texel, float4(ray_data_im.xyz, -ray_data_im.w));

  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  float3 radiance = float3(0.0f);
  float noise_offset = sampling.rng_1D_get(SAMPLING_RAYTRACE_W);
  float rand_trace = interleaved_gradient_noise(float2(texel), 5.0f, noise_offset);

  /* TODO(fclem): Take IOR into account in the roughness LOD bias. */
  /* TODO(fclem): pdf to roughness mapping is a crude approximation. Find something better. */
  // float roughness = saturate(ray_pdf_inv);

  /* Transform the ray into planar view-space. */
  Ray ray_view;
  ray_view.origin = transform_point(planar.viewmat, ray.origin);
  ray_view.direction = transform_direction(planar.viewmat, ray.direction);
  /* Extend the ray to cover the whole view. */
  ray_view.max_time = 1000.0f;

  ScreenTraceHitData hit = raytrace_planar(
      view, uni.raytrace_buf, lightprobe_planes.planar_depth_tx, planar, rand_trace, ray_view);

  if (hit.valid) {
    /* Evaluate radiance at hit-point. */
    radiance = raytrace_sample_screen(view,
                                      lightprobe_planes.planar_radiance_tx,
                                      uni.raytrace_buf,
                                      hit,
                                      roughness,
                                      hit.ss_hit_P.xy,
                                      planar_id);
  }
  else {
    /* Using ray direction as geometric normal to bias the sampling position.
     * This is faster than loading the gbuffer again and averages between reflected and normal
     * direction over many rays. */
    float3 Ng = ray.direction;
    /* Fall back to nearest light-probe. */
    LightProbeSample samp = lightprobes.load(float2(texel), P, Ng, V);
    radiance = lightprobes.eval_direction(samp, P, ray.direction, roughness);
    /* Set point really far for correct reprojection of background. */
    hit.time = 10000.0f;
  }

  radiance = colorspace::brightness_clamp_max(radiance, uni.uniform_buf.clamp.surface_indirect);

  imageStoreFast(srt.ray_time_img, texel, float4(hit.time));
  imageStoreFast(srt.ray_radiance_img, texel, float4(radiance, 0.0f));
}

}  // namespace planar

namespace fallback {

struct Resources {
  [[specialization_constant(0)]] int closure_index;

  [[storage(5, read)]] const uint (&tiles_coord_buf)[];

  [[sampler(1)]] sampler2DDepth depth_tx;

  [[image(0, read, SFLOAT_16_16_16_16)]] const image2D ray_data_img;
  [[image(1, write, RAYTRACE_RAYTIME_FORMAT)]] image2D ray_time_img;
  [[image(2, write, RAYTRACE_RADIANCE_FORMAT)]] image2D ray_radiance_img;
};

/**
 * Does not use any tracing method. Only rely on local light probes to get the incoming radiance.
 */
[[compute, local_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)]]
void trace([[resource_table]] Resources &srt,
           [[resource_table]] const LightprobeRenderData &lightprobes,
           [[resource_table]] const gbuffer::Reader &reader,
           [[resource_table]] const Uniform &uni,
           [[resource_table]] const draw::View &views,
           [[work_group_id]] const uint3 group_id,
           [[local_invocation_id]] const uint3 local_id)
{
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(srt.tiles_coord_buf[group_id.x]);
  int2 texel = int2(local_id.xy + tile_coord * tile_size);

  int2 texel_fullres = texel * uni.raytrace_buf.trace_pixel_scale +
                       uni.raytrace_buf.trace_pixel_offset;

  /* Check if texel is out of bounds,
   * so we can utilize fast texture functions and early-out if not. */
  if (any(greaterThanEqual(texel, imageSize(srt.ray_time_img).xy))) {
    return;
  }

  ClosureUndetermined cl = reader.read_bin(texel_fullres, srt.closure_index);
  float roughness = closure_apparent_roughness_get(cl);

  float depth = reverse_z::read(texelFetch(srt.depth_tx, texel_fullres, 0).r);
  float2 uv = (float2(texel_fullres) + 0.5f) * uni.raytrace_buf.full_resolution_inv;

  float4 ray_data_im = imageLoadFast(srt.ray_data_img, texel);
  float ray_pdf_inv = ray_data_im.w;

  if (ray_pdf_inv == 0.0f) {
    /* Invalid ray or pixels without ray. Do not trace. */
    imageStoreFast(srt.ray_time_img, texel, float4(0.0f));
    imageStoreFast(srt.ray_radiance_img, texel, float4(0.0f));
    return;
  }

  const ViewMatrices view = views.get(0);

  float3 P = view.point_screen_to_world(float3(uv, depth));
  float3 V = view.world_incident_vector(P);

  Ray ray;
  ray.origin = P;
  ray.direction = ray_data_im.xyz;

  /* Only closure 0 can be a transmission closure. */
  if (srt.closure_index == 0) {
    const gbuffer::Header gbuf_header = reader.read_header(texel_fullres);
    const Thickness thickness = reader.read_thickness(gbuf_header, texel_fullres);
    if (thickness.value() != 0.0f) {
      ClosureUndetermined cl = reader.read_bin(texel_fullres, srt.closure_index);
      ray = raytrace_thickness_ray_amend(ray, cl, V, thickness);
    }
  }

  /* Using ray direction as geometric normal to bias the sampling position.
   * This is faster than loading the gbuffer again and averages between reflected and normal
   * direction over many rays. */
  float3 Ng = ray.direction;
  LightProbeSample samp = lightprobes.load(float2(texel), ray.origin, Ng, V);
  /* Clamp SH to have parity with forward evaluation. */
  float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics::clamp_energy(samp.volume_irradiance,
                                                             clamp_indirect);

  float3 radiance = lightprobes.eval_direction(samp, ray.origin, ray.direction, roughness);
  /* Set point really far for correct reprojection of background. */
  float hit_time = 1000.0f;

  radiance = colorspace::brightness_clamp_max(radiance, uni.uniform_buf.clamp.surface_indirect);

  imageStoreFast(srt.ray_time_img, texel, float4(hit_time));
  imageStoreFast(srt.ray_radiance_img, texel, float4(radiance, 0.0f));
}

}  // namespace fallback
}  // namespace eevee::raytrace

PipelineCompute eevee_ray_trace_screen(eevee::raytrace::screen::trace);
PipelineCompute eevee_ray_trace_planar(eevee::raytrace::planar::trace);
PipelineCompute eevee_ray_trace_fallback(eevee::raytrace::fallback::trace);
