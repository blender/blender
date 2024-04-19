/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The ray-tracing module class handles ray generation, scheduling, tracing and denoising.
 */

#include <fstream>
#include <iostream>

#include "BKE_global.hh"

#include "eevee_instance.hh"

#include "eevee_raytrace.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Raytracing
 *
 * \{ */

void RayTraceModule::init()
{
  const SceneEEVEE &sce_eevee = inst_.scene->eevee;

  ray_tracing_options_ = sce_eevee.ray_tracing_options;
  tracing_method_ = RaytraceEEVEE_Method(sce_eevee.ray_tracing_method);
}

void RayTraceModule::sync()
{
  Texture &depth_tx = inst_.render_buffers.depth_tx;

#define PASS_VARIATION(_pass_name, _index, _suffix) \
  ((_index == 0) ? _pass_name##reflect##_suffix : \
   (_index == 1) ? _pass_name##refract##_suffix : \
                   _pass_name##diffuse##_suffix)

  /* Setup. */
  {
    PassSimple &pass = tile_classify_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(RAY_TILE_CLASSIFY));
    pass.bind_image("tile_raytrace_denoise_img", &tile_raytrace_denoise_tx_);
    pass.bind_image("tile_raytrace_tracing_img", &tile_raytrace_tracing_tx_);
    pass.bind_image("tile_horizon_denoise_img", &tile_horizon_denoise_tx_);
    pass.bind_image("tile_horizon_tracing_img", &tile_horizon_tracing_tx_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(&tile_classify_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_SHADER_STORAGE);
  }
  {
    PassSimple &pass = tile_compact_ps_;
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_TILE_COMPACT);
    pass.init();
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.specialize_constant(sh, "resolution_scale", &data_.resolution_scale);
    pass.shader_set(sh);
    pass.bind_image("tile_raytrace_denoise_img", &tile_raytrace_denoise_tx_);
    pass.bind_image("tile_raytrace_tracing_img", &tile_raytrace_tracing_tx_);
    pass.bind_ssbo("raytrace_tracing_dispatch_buf", &raytrace_tracing_dispatch_buf_);
    pass.bind_ssbo("raytrace_denoise_dispatch_buf", &raytrace_denoise_dispatch_buf_);
    pass.bind_ssbo("raytrace_tracing_tiles_buf", &raytrace_tracing_tiles_buf_);
    pass.bind_ssbo("raytrace_denoise_tiles_buf", &raytrace_denoise_tiles_buf_);
    pass.bind_resources(inst_.uniform_data);
    pass.dispatch(&tile_compact_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    PassSimple &pass = generate_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_GENERATE);
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_image("out_ray_data_img", &ray_data_tx_);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_tracing_tiles_buf_);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(raytrace_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_TEXTURE_FETCH |
                 GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  /* Tracing. */
  {
    PassSimple &pass = trace_planar_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_TRACE_PLANAR);
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_tracing_tiles_buf_);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.planar_probes);
    pass.bind_resources(inst_.volume_probes);
    pass.bind_resources(inst_.sphere_probes);
    pass.bind_resources(inst_.gbuffer);
    /* TODO(@fclem): Use another dispatch with only tiles that touches planar captures. */
    pass.dispatch(raytrace_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = trace_screen_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_TRACE_SCREEN);
    pass.specialize_constant(
        sh, "trace_refraction", reinterpret_cast<bool *>(&data_.trace_refraction));
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_tracing_tiles_buf_);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_texture("radiance_front_tx", &screen_radiance_front_tx_);
    pass.bind_texture("radiance_back_tx", &screen_radiance_back_tx_);
    pass.bind_texture("hiz_front_tx", &inst_.hiz_buffer.front.ref_tx_);
    pass.bind_texture("hiz_back_tx", &inst_.hiz_buffer.back.ref_tx_);
    /* Still bind front to hiz_tx for validation layers. */
    pass.bind_resources(inst_.hiz_buffer.front);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.volume_probes);
    pass.bind_resources(inst_.sphere_probes);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(raytrace_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = trace_fallback_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_TRACE_FALLBACK);
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_tracing_tiles_buf_);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.volume_probes);
    pass.bind_resources(inst_.sphere_probes);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(raytrace_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  /* Denoise. */
  {
    PassSimple &pass = denoise_spatial_ps_;
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_DENOISE_SPATIAL);
    pass.init();
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.specialize_constant(sh, "raytrace_resolution_scale", &data_.resolution_scale);
    pass.specialize_constant(sh, "skip_denoise", reinterpret_cast<bool *>(&data_.skip_denoise));
    pass.shader_set(sh);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_denoise_tiles_buf_);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_image("out_radiance_img", &denoised_spatial_tx_);
    pass.bind_image("out_variance_img", &hit_variance_tx_);
    pass.bind_image("out_hit_depth_img", &hit_depth_tx_);
    pass.bind_image("tile_mask_img", &tile_raytrace_denoise_tx_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(raytrace_denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = denoise_temporal_ps_;
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_DENOISE_TEMPORAL);
    pass.init();
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_texture("radiance_history_tx", &radiance_history_tx_);
    pass.bind_texture("variance_history_tx", &variance_history_tx_);
    pass.bind_texture("tilemask_history_tx", &tilemask_history_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("hit_depth_img", &hit_depth_tx_);
    pass.bind_image("in_radiance_img", &denoised_spatial_tx_);
    pass.bind_image("out_radiance_img", &denoised_temporal_tx_);
    pass.bind_image("in_variance_img", &hit_variance_tx_);
    pass.bind_image("out_variance_img", &denoise_variance_tx_);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_denoise_tiles_buf_);
    pass.bind_resources(inst_.sampling);
    pass.dispatch(raytrace_denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = denoise_bilateral_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_DENOISE_BILATERAL);
    pass.specialize_constant(sh, "closure_index", &data_.closure_index);
    pass.shader_set(sh);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("in_radiance_img", &denoised_temporal_tx_);
    pass.bind_image("out_radiance_img", &denoised_bilateral_tx_);
    pass.bind_image("in_variance_img", &denoise_variance_tx_);
    pass.bind_image("tile_mask_img", &tile_raytrace_denoise_tx_);
    pass.bind_ssbo("tiles_coord_buf", &raytrace_denoise_tiles_buf_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(raytrace_denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = horizon_schedule_ps_;
    /* Reuse tile compaction shader but feed it with horizon scan specific buffers. */
    GPUShader *sh = inst_.shaders.static_shader_get(RAY_TILE_COMPACT);
    pass.init();
    pass.specialize_constant(sh, "closure_index", 0);
    pass.specialize_constant(sh, "resolution_scale", &data_.horizon_resolution_scale);
    pass.shader_set(sh);
    pass.bind_image("tile_raytrace_denoise_img", &tile_horizon_denoise_tx_);
    pass.bind_image("tile_raytrace_tracing_img", &tile_horizon_tracing_tx_);
    pass.bind_ssbo("raytrace_tracing_dispatch_buf", &horizon_tracing_dispatch_buf_);
    pass.bind_ssbo("raytrace_denoise_dispatch_buf", &horizon_denoise_dispatch_buf_);
    pass.bind_ssbo("raytrace_tracing_tiles_buf", &horizon_tracing_tiles_buf_);
    pass.bind_ssbo("raytrace_denoise_tiles_buf", &horizon_denoise_tiles_buf_);
    pass.bind_resources(inst_.uniform_data);
    pass.dispatch(&horizon_schedule_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    PassSimple &pass = horizon_setup_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(HORIZON_SETUP));
    pass.bind_resources(inst_.uniform_data);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_texture(
        "in_radiance_tx", &screen_radiance_front_tx_, GPUSamplerState::default_sampler());
    pass.bind_image("out_radiance_img", &downsampled_in_radiance_tx_);
    pass.bind_image("out_normal_img", &downsampled_in_normal_tx_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(&horizon_tracing_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = horizon_scan_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(HORIZON_SCAN);
    pass.shader_set(sh);
    pass.bind_texture("screen_radiance_tx", &downsampled_in_radiance_tx_);
    pass.bind_texture("screen_normal_tx", &downsampled_in_normal_tx_);
    pass.bind_image("horizon_radiance_0_img", &horizon_radiance_tx_[0]);
    pass.bind_image("horizon_radiance_1_img", &horizon_radiance_tx_[1]);
    pass.bind_image("horizon_radiance_2_img", &horizon_radiance_tx_[2]);
    pass.bind_image("horizon_radiance_3_img", &horizon_radiance_tx_[3]);
    pass.bind_ssbo("tiles_coord_buf", &horizon_tracing_tiles_buf_);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.hiz_buffer.front);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.dispatch(horizon_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    PassSimple &pass = horizon_denoise_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(HORIZON_DENOISE);
    pass.shader_set(sh);
    pass.bind_texture("in_sh_0_tx", &horizon_radiance_tx_[0]);
    pass.bind_texture("in_sh_1_tx", &horizon_radiance_tx_[1]);
    pass.bind_texture("in_sh_2_tx", &horizon_radiance_tx_[2]);
    pass.bind_texture("in_sh_3_tx", &horizon_radiance_tx_[3]);
    pass.bind_texture("screen_normal_tx", &downsampled_in_normal_tx_);
    pass.bind_image("out_sh_0_img", &horizon_radiance_denoised_tx_[0]);
    pass.bind_image("out_sh_1_img", &horizon_radiance_denoised_tx_[1]);
    pass.bind_image("out_sh_2_img", &horizon_radiance_denoised_tx_[2]);
    pass.bind_image("out_sh_3_img", &horizon_radiance_denoised_tx_[3]);
    pass.bind_ssbo("tiles_coord_buf", &horizon_tracing_tiles_buf_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.hiz_buffer.front);
    pass.dispatch(horizon_tracing_dispatch_buf_);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
  {
    PassSimple &pass = horizon_resolve_ps_;
    pass.init();
    GPUShader *sh = inst_.shaders.static_shader_get(HORIZON_RESOLVE);
    pass.shader_set(sh);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_texture("horizon_radiance_0_tx", &horizon_radiance_denoised_tx_[0]);
    pass.bind_texture("horizon_radiance_1_tx", &horizon_radiance_denoised_tx_[1]);
    pass.bind_texture("horizon_radiance_2_tx", &horizon_radiance_denoised_tx_[2]);
    pass.bind_texture("horizon_radiance_3_tx", &horizon_radiance_denoised_tx_[3]);
    pass.bind_texture("screen_normal_tx", &downsampled_in_normal_tx_);
    pass.bind_image("closure0_img", &horizon_scan_output_tx_[0]);
    pass.bind_image("closure1_img", &horizon_scan_output_tx_[1]);
    pass.bind_image("closure2_img", &horizon_scan_output_tx_[2]);
    pass.bind_ssbo("tiles_coord_buf", &horizon_denoise_tiles_buf_);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.gbuffer);
    pass.bind_resources(inst_.volume_probes);
    pass.bind_resources(inst_.sphere_probes);
    pass.dispatch(horizon_denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
}

void RayTraceModule::debug_pass_sync() {}

void RayTraceModule::debug_draw(View & /*view*/, GPUFrameBuffer * /*view_fb*/) {}

RayTraceResult RayTraceModule::render(RayTraceBuffer &rt_buffer,
                                      GPUTexture *screen_radiance_back_tx,
                                      GPUTexture *screen_radiance_front_tx,
                                      const float4x4 &screen_radiance_persmat,
                                      eClosureBits active_closures,
                                      /* TODO(fclem): Maybe wrap these two in some other class. */
                                      View &main_view,
                                      View &render_view,
                                      bool do_refraction_tracing)
{
  using namespace blender::math;

  RaytraceEEVEE options = ray_tracing_options_;

  bool use_horizon_scan = options.trace_max_roughness < 1.0f;

  const int resolution_scale = max_ii(1, power_of_2_max_i(options.resolution_scale));
  const int horizon_resolution_scale = max_ii(
      1, power_of_2_max_i(inst_.scene->eevee.gtao_resolution));

  const int2 extent = inst_.film.render_extent_get();
  const int2 tracing_res = math::divide_ceil(extent, int2(resolution_scale));
  const int2 tracing_res_horizon = math::divide_ceil(extent, int2(horizon_resolution_scale));
  const int2 dummy_extent(1, 1);
  const int2 group_size(RAYTRACE_GROUP_SIZE);

  const int2 denoise_tiles = divide_ceil(extent, group_size);
  const int2 raytrace_tiles = divide_ceil(tracing_res, group_size);
  const int2 raytrace_tiles_horizon = divide_ceil(tracing_res_horizon, group_size);
  const int denoise_tile_count = denoise_tiles.x * denoise_tiles.y;
  const int raytrace_tile_count = raytrace_tiles.x * raytrace_tiles.y;
  const int raytrace_tile_count_horizon = raytrace_tiles_horizon.x * raytrace_tiles_horizon.y;
  tile_classify_dispatch_size_ = int3(denoise_tiles, 1);
  horizon_schedule_dispatch_size_ = int3(divide_ceil(raytrace_tiles_horizon, group_size), 1);
  tile_compact_dispatch_size_ = int3(divide_ceil(raytrace_tiles, group_size), 1);
  tracing_dispatch_size_ = int3(raytrace_tiles, 1);
  horizon_tracing_dispatch_size_ = int3(raytrace_tiles_horizon, 1);

  /* TODO(fclem): Use real max closure count from shader. */
  const int closure_count = 3;
  eGPUTextureFormat format = RAYTRACE_TILEMASK_FORMAT;
  eGPUTextureUsage usage_rw = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
  tile_raytrace_denoise_tx_.ensure_2d_array(format, denoise_tiles, closure_count, usage_rw);
  tile_raytrace_tracing_tx_.ensure_2d_array(format, raytrace_tiles, closure_count, usage_rw);
  /* Kept as 2D array for compatibility with the tile compaction shader. */
  tile_horizon_denoise_tx_.ensure_2d_array(format, denoise_tiles, 1, usage_rw);
  tile_horizon_tracing_tx_.ensure_2d_array(format, raytrace_tiles_horizon, 1, usage_rw);

  tile_raytrace_denoise_tx_.clear(uint4(0u));
  tile_raytrace_tracing_tx_.clear(uint4(0u));
  tile_horizon_denoise_tx_.clear(uint4(0u));
  tile_horizon_tracing_tx_.clear(uint4(0u));

  horizon_tracing_tiles_buf_.resize(ceil_to_multiple_u(raytrace_tile_count_horizon, 512));
  horizon_denoise_tiles_buf_.resize(ceil_to_multiple_u(denoise_tile_count, 512));
  raytrace_tracing_tiles_buf_.resize(ceil_to_multiple_u(raytrace_tile_count, 512));
  raytrace_denoise_tiles_buf_.resize(ceil_to_multiple_u(denoise_tile_count, 512));

  /* Data for tile classification. */
  float roughness_mask_start = options.trace_max_roughness;
  float roughness_mask_fade = 0.2f;
  data_.roughness_mask_scale = 1.0 / roughness_mask_fade;
  data_.roughness_mask_bias = data_.roughness_mask_scale * roughness_mask_start;

  /* Data for the radiance setup. */
  data_.brightness_clamp = (options.sample_clamp > 0.0) ? options.sample_clamp : 1e20;
  data_.resolution_scale = resolution_scale;
  data_.resolution_bias = int2(inst_.sampling.rng_2d_get(SAMPLING_RAYTRACE_V) * resolution_scale);
  data_.radiance_persmat = screen_radiance_persmat;
  data_.full_resolution = extent;
  data_.full_resolution_inv = 1.0f / float2(extent);

  data_.horizon_resolution_scale = horizon_resolution_scale;
  data_.horizon_resolution_bias = int2(inst_.sampling.rng_2d_get(SAMPLING_RAYTRACE_V) *
                                       horizon_resolution_scale);
  /* TODO(fclem): Eventually all uniform data is setup here. */

  inst_.uniform_data.push_update();

  RayTraceResult result;

  DRW_stats_group_start("Raytracing");

  const bool has_active_closure = active_closures != CLOSURE_NONE;

  if (has_active_closure) {
    inst_.manager->submit(tile_classify_ps_);
  }

  data_.trace_refraction = do_refraction_tracing;

  for (int i = 0; i < 3; i++) {
    result.closures[i] = trace(i,
                               (closure_count > i),
                               options,
                               rt_buffer,
                               screen_radiance_back_tx,
                               screen_radiance_front_tx,
                               screen_radiance_persmat,
                               main_view,
                               render_view);
  }

  if (has_active_closure) {
    if (use_horizon_scan) {
      DRW_stats_group_start("Horizon Scan");

      screen_radiance_front_tx_ = screen_radiance_front_tx;

      downsampled_in_radiance_tx_.acquire(tracing_res_horizon, RAYTRACE_RADIANCE_FORMAT, usage_rw);
      downsampled_in_normal_tx_.acquire(tracing_res_horizon, GPU_RGB10_A2, usage_rw);

      horizon_radiance_tx_[0].acquire(tracing_res_horizon, GPU_RGBA16F, usage_rw);
      horizon_radiance_denoised_tx_[0].acquire(tracing_res_horizon, GPU_RGBA16F, usage_rw);
      for (int i : IndexRange(1, 3)) {
        horizon_radiance_tx_[i].acquire(tracing_res_horizon, GPU_RGBA8, usage_rw);
        horizon_radiance_denoised_tx_[i].acquire(tracing_res_horizon, GPU_RGBA8, usage_rw);
      }
      for (int i : IndexRange(3)) {
        horizon_scan_output_tx_[i] = result.closures[i].get();
      }

      horizon_tracing_dispatch_buf_.clear_to_zero();
      horizon_denoise_dispatch_buf_.clear_to_zero();
      inst_.manager->submit(horizon_schedule_ps_);

      inst_.manager->submit(horizon_setup_ps_, render_view);
      inst_.manager->submit(horizon_scan_ps_, render_view);
      inst_.manager->submit(horizon_denoise_ps_, render_view);
      inst_.manager->submit(horizon_resolve_ps_, render_view);

      for (int i : IndexRange(4)) {
        horizon_radiance_tx_[i].release();
        horizon_radiance_denoised_tx_[i].release();
      }
      downsampled_in_radiance_tx_.release();
      downsampled_in_normal_tx_.release();

      DRW_stats_group_end();
    }
  }

  DRW_stats_group_end();

  return result;
}

RayTraceResultTexture RayTraceModule::trace(
    int closure_index,
    bool active_layer,
    RaytraceEEVEE options,
    RayTraceBuffer &rt_buffer,
    GPUTexture *screen_radiance_back_tx,
    GPUTexture *screen_radiance_front_tx,
    const float4x4 &screen_radiance_persmat,
    /* TODO(fclem): Maybe wrap these two in some other class. */
    View &main_view,
    View &render_view)
{
  RayTraceBuffer::DenoiseBuffer *denoise_buf = &rt_buffer.closures[closure_index];

  if (!active_layer) {
    /* Early out. Release persistent buffers. Still acquire one dummy resource for validation. */
    denoise_buf->denoised_spatial_tx.acquire(int2(1), RAYTRACE_RADIANCE_FORMAT);
    denoise_buf->radiance_history_tx.free();
    denoise_buf->variance_history_tx.free();
    denoise_buf->tilemask_history_tx.free();
    return {denoise_buf->denoised_spatial_tx};
  }

  const int resolution_scale = max_ii(1, power_of_2_max_i(options.resolution_scale));

  const int2 extent = inst_.film.render_extent_get();
  const int2 tracing_res = math::divide_ceil(extent, int2(resolution_scale));

  renderbuf_depth_view_ = inst_.render_buffers.depth_tx;

  const bool use_denoise = (options.flag & RAYTRACE_EEVEE_USE_DENOISE);
  const bool use_spatial_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_SPATIAL) &&
                                   use_denoise;
  const bool use_temporal_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_TEMPORAL) &&
                                    use_spatial_denoise;
  const bool use_bilateral_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_BILATERAL) &&
                                     use_temporal_denoise;

  eGPUTextureUsage usage_rw = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;

  DRW_stats_group_start("Raytracing");

  data_.thickness = options.screen_trace_thickness;
  data_.quality = 1.0f - 0.95f * options.screen_trace_quality;
  data_.brightness_clamp = (options.sample_clamp > 0.0) ? options.sample_clamp : 1e20;

  float roughness_mask_start = options.trace_max_roughness;
  float roughness_mask_fade = 0.2f;
  data_.roughness_mask_scale = 1.0 / roughness_mask_fade;
  data_.roughness_mask_bias = data_.roughness_mask_scale * roughness_mask_start;

  data_.resolution_scale = resolution_scale;
  data_.resolution_bias = int2(inst_.sampling.rng_2d_get(SAMPLING_RAYTRACE_V) * resolution_scale);
  data_.history_persmat = denoise_buf->history_persmat;
  data_.radiance_persmat = screen_radiance_persmat;
  data_.full_resolution = extent;
  data_.full_resolution_inv = 1.0f / float2(extent);
  data_.skip_denoise = !use_spatial_denoise;
  data_.closure_index = closure_index;
  inst_.uniform_data.push_update();

  /* Ray setup. */
  raytrace_tracing_dispatch_buf_.clear_to_zero();
  raytrace_denoise_dispatch_buf_.clear_to_zero();
  inst_.manager->submit(tile_compact_ps_);

  {
    /* Tracing rays. */
    ray_data_tx_.acquire(tracing_res, GPU_RGBA16F);
    ray_time_tx_.acquire(tracing_res, RAYTRACE_RAYTIME_FORMAT);
    ray_radiance_tx_.acquire(tracing_res, RAYTRACE_RADIANCE_FORMAT);

    screen_radiance_front_tx_ = screen_radiance_front_tx;
    screen_radiance_back_tx_ = screen_radiance_back_tx;

    inst_.manager->submit(generate_ps_, render_view);
    if (tracing_method_ == RAYTRACE_EEVEE_METHOD_SCREEN) {
      if (inst_.planar_probes.enabled()) {
        inst_.manager->submit(trace_planar_ps_, render_view);
      }
      inst_.manager->submit(trace_screen_ps_, render_view);
    }
    else {
      inst_.manager->submit(trace_fallback_ps_, render_view);
    }
  }

  RayTraceResultTexture result;

  /* Spatial denoise pass is required to resolve at least one ray per pixel. */
  {
    denoise_buf->denoised_spatial_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT);
    hit_variance_tx_.acquire(use_temporal_denoise ? extent : int2(1), RAYTRACE_VARIANCE_FORMAT);
    hit_depth_tx_.acquire(use_temporal_denoise ? extent : int2(1), GPU_R32F);
    denoised_spatial_tx_ = denoise_buf->denoised_spatial_tx;

    inst_.manager->submit(denoise_spatial_ps_, render_view);

    result = {denoise_buf->denoised_spatial_tx};
  }

  ray_data_tx_.release();
  ray_time_tx_.release();
  ray_radiance_tx_.release();

  if (use_temporal_denoise) {
    denoise_buf->denoised_temporal_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT, usage_rw);
    denoise_variance_tx_.acquire(
        use_bilateral_denoise ? extent : int2(1), RAYTRACE_VARIANCE_FORMAT, usage_rw);
    denoise_buf->variance_history_tx.ensure_2d(
        RAYTRACE_VARIANCE_FORMAT, use_bilateral_denoise ? extent : int2(1), usage_rw);
    denoise_buf->tilemask_history_tx.ensure_2d_array(RAYTRACE_TILEMASK_FORMAT,
                                                     tile_raytrace_denoise_tx_.size().xy(),
                                                     tile_raytrace_denoise_tx_.size().z,
                                                     usage_rw);

    if (denoise_buf->radiance_history_tx.ensure_2d(RAYTRACE_RADIANCE_FORMAT, extent, usage_rw) ||
        denoise_buf->valid_history == false)
    {
      /* If viewport resolution changes, do not try to use history. */
      denoise_buf->tilemask_history_tx.clear(uint4(0u));
    }

    radiance_history_tx_ = denoise_buf->radiance_history_tx;
    variance_history_tx_ = denoise_buf->variance_history_tx;
    tilemask_history_tx_ = denoise_buf->tilemask_history_tx;
    denoised_temporal_tx_ = denoise_buf->denoised_temporal_tx;

    inst_.manager->submit(denoise_temporal_ps_, render_view);

    /* Save view-projection matrix for next reprojection. */
    denoise_buf->history_persmat = main_view.persmat();
    /* Radiance will be swapped with history in #RayTraceResult::release().
     * Variance is swapped with history after bilateral denoise.
     * It keeps data-flow easier to follow. */
    result = {denoise_buf->denoised_temporal_tx, denoise_buf->radiance_history_tx};
    /* Not referenced by result anymore. */
    denoise_buf->denoised_spatial_tx.release();

    GPU_texture_copy(denoise_buf->tilemask_history_tx, tile_raytrace_denoise_tx_);
  }

  /* Only use history buffer for the next frame if temporal denoise was used by the current one. */
  denoise_buf->valid_history = use_temporal_denoise;

  hit_variance_tx_.release();
  hit_depth_tx_.release();

  if (use_bilateral_denoise) {
    denoise_buf->denoised_bilateral_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT, usage_rw);
    denoised_bilateral_tx_ = denoise_buf->denoised_bilateral_tx;

    inst_.manager->submit(denoise_bilateral_ps_, render_view);

    /* Swap after last use. */
    TextureFromPool::swap(denoise_buf->denoised_temporal_tx, denoise_buf->radiance_history_tx);
    TextureFromPool::swap(denoise_variance_tx_, denoise_buf->variance_history_tx);

    result = {denoise_buf->denoised_bilateral_tx};
    /* Not referenced by result anymore. */
    denoise_buf->denoised_temporal_tx.release();
  }

  denoise_variance_tx_.release();

  DRW_stats_group_end();

  return result;
}

/** \} */

}  // namespace blender::eevee
