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

#include "BKE_global.h"

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

  reflection_options_ = sce_eevee.reflection_options;
  refraction_options_ = sce_eevee.refraction_options;
  tracing_method_ = RaytraceEEVEE_Method(sce_eevee.ray_tracing_method);

  if (sce_eevee.ray_split_settings == 0) {
    refraction_options_ = reflection_options_;
  }
}

void RayTraceModule::sync()
{
  Texture &depth_tx = inst_.render_buffers.depth_tx;

  /* Setup. */
  {
    PassSimple &pass = tile_classify_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(RAY_TILE_CLASSIFY));
    pass.bind_image("tile_mask_img", &tile_mask_tx_);
    pass.bind_ssbo("ray_dispatch_buf", &ray_dispatch_buf_);
    pass.bind_ssbo("denoise_dispatch_buf", &denoise_dispatch_buf_);
    inst_.bind_uniform_data(&pass);
    inst_.gbuffer.bind_resources(&pass);
    pass.dispatch(&tile_classify_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS | GPU_BARRIER_SHADER_STORAGE);
  }
  {
    PassSimple &pass = tile_compact_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(RAY_TILE_COMPACT));
    pass.bind_image("tile_mask_img", &tile_mask_tx_);
    pass.bind_ssbo("ray_dispatch_buf", &ray_dispatch_buf_);
    pass.bind_ssbo("denoise_dispatch_buf", &denoise_dispatch_buf_);
    pass.bind_ssbo("ray_tiles_buf", &ray_tiles_buf_);
    pass.bind_ssbo("denoise_tiles_buf", &denoise_tiles_buf_);
    inst_.bind_uniform_data(&pass);
    pass.dispatch(&tile_compact_dispatch_size_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  for (auto type : IndexRange(2)) {
    PassSimple &pass = (type == 0) ? generate_reflect_ps_ : generate_refract_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get((type == 0) ? RAY_GENERATE_REFLECT :
                                                                  RAY_GENERATE_REFRACT));
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_image("out_ray_data_img", &ray_data_tx_);
    pass.bind_ssbo("tiles_coord_buf", &ray_tiles_buf_);
    inst_.sampling.bind_resources(&pass);
    inst_.gbuffer.bind_resources(&pass);
    pass.dispatch(ray_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_STORAGE | GPU_BARRIER_TEXTURE_FETCH |
                 GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  /* Tracing. */
  for (auto type : IndexRange(2)) {
    PassSimple &pass = (type == 0) ? trace_reflect_ps_ : trace_refract_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get((type == 0) ? RAY_TRACE_SCREEN_REFLECT :
                                                                  RAY_TRACE_SCREEN_REFRACT));
    pass.bind_ssbo("tiles_coord_buf", &ray_tiles_buf_);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_texture("screen_radiance_tx", &screen_radiance_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    inst_.bind_uniform_data(&pass);
    inst_.hiz_buffer.bind_resources(&pass);
    inst_.sampling.bind_resources(&pass);
    inst_.reflection_probes.bind_resources(&pass);
    pass.dispatch(ray_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = trace_fallback_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(RAY_TRACE_FALLBACK));
    pass.bind_ssbo("tiles_coord_buf", &ray_tiles_buf_);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    inst_.bind_uniform_data(&pass);
    inst_.reflection_probes.bind_resources(&pass);
    pass.dispatch(ray_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  /* Denoise. */
  for (auto type : IndexRange(2)) {
    PassSimple &pass = (type == 0) ? denoise_spatial_reflect_ps_ : denoise_spatial_refract_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get((type == 0) ? RAY_DENOISE_SPATIAL_REFLECT :
                                                                  RAY_DENOISE_SPATIAL_REFRACT));
    pass.bind_ssbo("tiles_coord_buf", &denoise_tiles_buf_);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("ray_data_img", &ray_data_tx_);
    pass.bind_image("ray_time_img", &ray_time_tx_);
    pass.bind_image("ray_radiance_img", &ray_radiance_tx_);
    pass.bind_image("out_radiance_img", &denoised_spatial_tx_);
    pass.bind_image("out_variance_img", &hit_variance_tx_);
    pass.bind_image("out_hit_depth_img", &hit_depth_tx_);
    pass.bind_image("tile_mask_img", &tile_mask_tx_);
    inst_.bind_uniform_data(&pass);
    inst_.sampling.bind_resources(&pass);
    inst_.gbuffer.bind_resources(&pass);
    pass.dispatch(denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  {
    PassSimple &pass = denoise_temporal_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get(RAY_DENOISE_TEMPORAL));
    inst_.bind_uniform_data(&pass);
    pass.bind_texture("radiance_history_tx", &radiance_history_tx_);
    pass.bind_texture("variance_history_tx", &variance_history_tx_);
    pass.bind_texture("tilemask_history_tx", &tilemask_history_tx_);
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("hit_depth_img", &hit_depth_tx_);
    pass.bind_image("in_radiance_img", &denoised_spatial_tx_);
    pass.bind_image("out_radiance_img", &denoised_temporal_tx_);
    pass.bind_image("in_variance_img", &hit_variance_tx_);
    pass.bind_image("out_variance_img", &denoise_variance_tx_);
    pass.bind_ssbo("tiles_coord_buf", &denoise_tiles_buf_);
    inst_.sampling.bind_resources(&pass);
    pass.dispatch(denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
  for (auto type : IndexRange(2)) {
    PassSimple &pass = (type == 0) ? denoise_bilateral_reflect_ps_ : denoise_bilateral_refract_ps_;
    pass.init();
    pass.shader_set(inst_.shaders.static_shader_get((type == 0) ? RAY_DENOISE_BILATERAL_REFLECT :
                                                                  RAY_DENOISE_BILATERAL_REFRACT));
    pass.bind_texture("depth_tx", &depth_tx);
    pass.bind_image("in_radiance_img", &denoised_temporal_tx_);
    pass.bind_image("out_radiance_img", &denoised_bilateral_tx_);
    pass.bind_image("in_variance_img", &denoise_variance_tx_);
    pass.bind_image("tile_mask_img", &tile_mask_tx_);
    pass.bind_ssbo("tiles_coord_buf", &denoise_tiles_buf_);
    inst_.bind_uniform_data(&pass);
    inst_.sampling.bind_resources(&pass);
    inst_.gbuffer.bind_resources(&pass);
    pass.dispatch(denoise_dispatch_buf_);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
}

void RayTraceModule::debug_pass_sync() {}

void RayTraceModule::debug_draw(View & /* view */, GPUFrameBuffer * /* view_fb */) {}

RayTraceResult RayTraceModule::trace(RayTraceBuffer &rt_buffer,
                                     GPUTexture *screen_radiance_tx,
                                     const float4x4 &screen_radiance_persmat,
                                     eClosureBits active_closures,
                                     eClosureBits raytrace_closure,
                                     /* TODO(fclem): Maybe wrap these two in some other class. */
                                     View &main_view,
                                     View &render_view,
                                     bool force_no_tracing)
{
  BLI_assert_msg(count_bits_i(raytrace_closure) == 1,
                 "Only one closure type can be raytraced at a time.");
  BLI_assert_msg(raytrace_closure ==
                     (raytrace_closure & (CLOSURE_REFLECTION | CLOSURE_REFRACTION)),
                 "Only reflection and refraction are implemented.");

  if (tracing_method_ == RAYTRACE_EEVEE_METHOD_NONE) {
    force_no_tracing = true;
  }

  screen_radiance_tx_ = screen_radiance_tx;

  RaytraceEEVEE options;
  PassSimple *generate_ray_ps = nullptr;
  PassSimple *trace_ray_ps = nullptr;
  PassSimple *denoise_spatial_ps = nullptr;
  PassSimple *denoise_bilateral_ps = nullptr;
  RayTraceBuffer::DenoiseBuffer *denoise_buf = nullptr;

  if (raytrace_closure == CLOSURE_REFLECTION) {
    options = reflection_options_;
    generate_ray_ps = &generate_reflect_ps_;
    trace_ray_ps = force_no_tracing ? &trace_fallback_ps_ : &trace_reflect_ps_;
    denoise_spatial_ps = &denoise_spatial_reflect_ps_;
    denoise_bilateral_ps = &denoise_bilateral_reflect_ps_;
    denoise_buf = &rt_buffer.reflection;
  }
  else if (raytrace_closure == CLOSURE_REFRACTION) {
    options = refraction_options_;
    generate_ray_ps = &generate_refract_ps_;
    trace_ray_ps = force_no_tracing ? &trace_fallback_ps_ : &trace_refract_ps_;
    denoise_spatial_ps = &denoise_spatial_refract_ps_;
    denoise_bilateral_ps = &denoise_bilateral_refract_ps_;
    denoise_buf = &rt_buffer.refraction;
  }

  if ((active_closures & raytrace_closure) == 0) {
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
  const int2 dummy_extent(1, 1);

  tile_classify_dispatch_size_ = int3(math::divide_ceil(extent, int2(RAYTRACE_GROUP_SIZE)), 1);
  const int denoise_tile_count = tile_classify_dispatch_size_.x * tile_classify_dispatch_size_.y;
  const int2 tile_mask_extent = tile_classify_dispatch_size_.xy();

  const int2 ray_tiles = math::divide_ceil(tracing_res, int2(RAYTRACE_GROUP_SIZE));
  const int ray_tile_count = ray_tiles.x * ray_tiles.y;
  tile_compact_dispatch_size_ = int3(math::divide_ceil(ray_tiles, int2(RAYTRACE_GROUP_SIZE)), 1);

  renderbuf_stencil_view_ = inst_.render_buffers.depth_tx.stencil_view();
  renderbuf_depth_view_ = inst_.render_buffers.depth_tx;

  bool use_denoise = (options.flag & RAYTRACE_EEVEE_USE_DENOISE);
  bool use_spatial_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_SPATIAL) &&
                             use_denoise;
  bool use_temporal_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_TEMPORAL) &&
                              use_spatial_denoise;
  bool use_bilateral_denoise = (options.denoise_stages & RAYTRACE_EEVEE_DENOISE_BILATERAL) &&
                               use_temporal_denoise;

  DRW_stats_group_start("Raytracing");

  data_.thickness = options.screen_trace_thickness;
  data_.quality = 1.0f - 0.95f * options.screen_trace_quality;
  data_.brightness_clamp = (options.sample_clamp > 0.0) ? options.sample_clamp : 1e20;
  data_.max_trace_roughness = 1.0f;

  data_.resolution_scale = resolution_scale;
  data_.closure_active = raytrace_closure;
  data_.resolution_bias = int2(inst_.sampling.rng_2d_get(SAMPLING_RAYTRACE_V) * resolution_scale);
  data_.history_persmat = denoise_buf->history_persmat;
  data_.radiance_persmat = screen_radiance_persmat;
  data_.full_resolution = extent;
  data_.full_resolution_inv = 1.0f / float2(extent);
  data_.skip_denoise = !use_spatial_denoise;
  inst_.push_uniform_data();

  tile_mask_tx_.acquire(tile_mask_extent, RAYTRACE_TILEMASK_FORMAT);
  denoise_tiles_buf_.resize(ceil_to_multiple_u(denoise_tile_count, 512));
  ray_tiles_buf_.resize(ceil_to_multiple_u(ray_tile_count, 512));

  /* Ray setup. */
  inst_.manager->submit(tile_classify_ps_);
  inst_.manager->submit(tile_compact_ps_);

  {
    /* Tracing rays. */
    ray_data_tx_.acquire(tracing_res, GPU_RGBA16F);
    ray_time_tx_.acquire(tracing_res, GPU_R32F);
    ray_radiance_tx_.acquire(tracing_res, RAYTRACE_RADIANCE_FORMAT);

    inst_.manager->submit(*generate_ray_ps, render_view);
    inst_.manager->submit(*trace_ray_ps, render_view);
  }

  RayTraceResult result;

  /* Spatial denoise pass is required to resolve at least one ray per pixel. */
  {
    denoise_buf->denoised_spatial_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT);
    hit_variance_tx_.acquire(use_temporal_denoise ? extent : dummy_extent,
                             RAYTRACE_VARIANCE_FORMAT);
    hit_depth_tx_.acquire(use_temporal_denoise ? extent : dummy_extent, GPU_R32F);
    denoised_spatial_tx_ = denoise_buf->denoised_spatial_tx;

    inst_.manager->submit(*denoise_spatial_ps, render_view);

    result = {denoise_buf->denoised_spatial_tx};
  }

  ray_data_tx_.release();
  ray_time_tx_.release();
  ray_radiance_tx_.release();

  if (use_temporal_denoise) {
    denoise_buf->denoised_temporal_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT);
    denoise_variance_tx_.acquire(use_bilateral_denoise ? extent : dummy_extent,
                                 RAYTRACE_VARIANCE_FORMAT);
    denoise_buf->variance_history_tx.ensure_2d(RAYTRACE_VARIANCE_FORMAT,
                                               use_bilateral_denoise ? extent : dummy_extent);
    denoise_buf->tilemask_history_tx.ensure_2d(RAYTRACE_TILEMASK_FORMAT, tile_mask_extent);
    if (denoise_buf->radiance_history_tx.ensure_2d(RAYTRACE_RADIANCE_FORMAT, extent) ||
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

    /* Swap after last use. */
    TextureFromPool::swap(tile_mask_tx_, denoise_buf->tilemask_history_tx);
    /* Save view-projection matrix for next reprojection. */
    denoise_buf->history_persmat = main_view.persmat();
    /* Radiance will be swapped with history in #RayTraceResult::release().
     * Variance is swapped with history after bilateral denoise.
     * It keeps data-flow easier to follow. */
    result = {denoise_buf->denoised_temporal_tx, denoise_buf->radiance_history_tx};
    /* Not referenced by result anymore. */
    denoise_buf->denoised_spatial_tx.release();
  }

  /* Only use history buffer for the next frame if temporal denoise was used by the current one. */
  denoise_buf->valid_history = use_temporal_denoise;

  hit_variance_tx_.release();
  hit_depth_tx_.release();

  if (use_bilateral_denoise) {
    denoise_buf->denoised_bilateral_tx.acquire(extent, RAYTRACE_RADIANCE_FORMAT);
    denoised_bilateral_tx_ = denoise_buf->denoised_bilateral_tx;

    inst_.manager->submit(*denoise_bilateral_ps, render_view);

    /* Swap after last use. */
    TextureFromPool::swap(denoise_buf->denoised_temporal_tx, denoise_buf->radiance_history_tx);
    TextureFromPool::swap(denoise_variance_tx_, denoise_buf->variance_history_tx);

    result = {denoise_buf->denoised_bilateral_tx};
    /* Not referenced by result anymore. */
    denoise_buf->denoised_temporal_tx.release();
  }

  tile_mask_tx_.release();
  denoise_variance_tx_.release();

  DRW_stats_group_end();

  return result;
}

/** \} */

}  // namespace blender::eevee
