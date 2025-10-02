/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The ray-tracing module class handles ray generation, scheduling, tracing and denoising.
 */

#pragma once

#include "DNA_scene_types.h"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "eevee_raytrace_shared.hh"

namespace blender::eevee {

class Instance;

using RayTraceTileBuf = draw::StorageArrayBuffer<uint, 1024, true>;

/* -------------------------------------------------------------------- */
/** \name Ray-tracing Buffers
 *
 * Contain persistent data used for temporal denoising. Similar to \class GBuffer but only contains
 * persistent data.
 * \{ */

/**
 * Contain persistent buffer that need to be stored per view, per deferred layer.
 */
struct RayTraceBuffer {
  /** Set of buffers that need to be allocated for each ray type. */
  struct DenoiseBuffer {
    /* Persistent history buffers. */
    Texture radiance_history_tx = {"radiance_tx"};
    Texture variance_history_tx = {"variance_tx"};
    /* Map of tiles that were processed inside the history buffer. */
    Texture tilemask_history_tx = {"tilemask_tx"};
    /** Perspective matrix for which the history buffers were recorded. */
    float4x4 history_persmat;
    /** True if history buffer was used last frame and can be re-projected. */
    bool valid_history = false;
    /**
     * Textures containing the ray hit radiance denoised (full-res). One of them is result_tx.
     * One might become result buffer so it need instantiation by closure type to avoid reuse.
     */
    TextureFromPool denoised_spatial_tx = {"denoised_spatial_tx"};
    TextureFromPool denoised_temporal_tx = {"denoised_temporal_tx"};
    TextureFromPool denoised_bilateral_tx = {"denoised_bilateral_tx"};
  };
  /**
   * One for each closure. Not to be mistaken with deferred layer type.
   */
  DenoiseBuffer closures[3];

  /**
   * Radiance feedback of the deferred layer for next sample's reflection or next layer's
   * transmission.
   */
  Texture radiance_feedback_tx = {"radiance_feedback_tx"};
  /**
   * Perspective matrix for which the radiance feedback buffer was recorded.
   * Can be different from de-noise buffer's history matrix.
   */
  float4x4 history_persmat;

  gpu::Texture *feedback_ensure(bool is_dummy, int2 extent)
  {
    eGPUTextureUsage usage_rw = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
    if (radiance_feedback_tx.ensure_2d(
            gpu::TextureFormat::SFLOAT_16_16_16_16, is_dummy ? int2(1) : extent, usage_rw))
    {
      radiance_feedback_tx.clear(float4(0.0f));
    }
    return radiance_feedback_tx;
  }
};

/**
 * Contains the result texture.
 * The result buffer is usually short lived and is kept in a TextureFromPool managed by the mode.
 * This structure contains a reference to it so that it can be freed after use by the caller.
 */
class RayTraceResultTexture {
 private:
  /** Result is in a temporary texture that needs to be released. */
  TextureFromPool *result_ = nullptr;
  /** Value of `result_->tx_` that can be referenced in advance. */
  gpu::Texture *tx_ = nullptr;
  /** History buffer to swap the temporary texture that does not need to be released. */
  Texture *history_ = nullptr;

 public:
  RayTraceResultTexture() = default;
  RayTraceResultTexture(TextureFromPool &result) : result_(result.ptr()), tx_(result) {};
  RayTraceResultTexture(TextureFromPool &result, Texture &history)
      : result_(result.ptr()), tx_(result), history_(history.ptr()) {};

  operator gpu::Texture *() const
  {
    BLI_assert(tx_ != nullptr);
    return tx_;
  }

  gpu::Texture **operator&()
  {
    return &tx_;
  }

  void release()
  {
    if (history_) {
      /* Swap after last use. */
      TextureFromPool::swap(*result_, *history_);
    }
    /* NOTE: This releases the previous history. */
    result_->release();
  }
};

struct RayTraceResult {
  RayTraceResultTexture closures[3];

  void release()
  {
    for (int i = 0; i < 3; i++) {
      closures[i].release();
    }
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray-tracing
 * \{ */

class RayTraceModule {
 private:
  Instance &inst_;

  draw::PassSimple tile_classify_ps_ = {"TileClassify"};
  draw::PassSimple tile_compact_ps_ = {"TileCompact"};
  draw::PassSimple generate_ps_ = {"RayGenerate"};
  draw::PassSimple trace_planar_ps_ = {"Trace.Planar"};
  draw::PassSimple trace_screen_ps_ = {"Trace.Screen"};
  draw::PassSimple trace_fallback_ps_ = {"Trace.Fallback"};
  draw::PassSimple denoise_spatial_ps_ = {"DenoiseSpatial"};
  draw::PassSimple denoise_temporal_ps_ = {"DenoiseTemporal"};
  draw::PassSimple denoise_bilateral_ps_ = {"DenoiseBilateral"};
  draw::PassSimple horizon_schedule_ps_ = {"HorizonScan.Schedule"};
  draw::PassSimple horizon_setup_ps_ = {"HorizonScan.Setup"};
  draw::PassSimple horizon_scan_ps_ = {"HorizonScan.Trace"};
  draw::PassSimple horizon_denoise_ps_ = {"HorizonScan.Denoise"};
  draw::PassSimple horizon_resolve_ps_ = {"HorizonScan.Resolve"};

  /** Dispatch with enough tiles for the whole screen. */
  int3 tile_classify_dispatch_size_ = int3(1);
  /** Dispatch with enough tiles for the tile mask. */
  int3 tile_compact_dispatch_size_ = int3(1);
  int3 horizon_schedule_dispatch_size_ = int3(1);
  /** Dispatch with enough tiles for the tracing resolution. */
  int3 tracing_dispatch_size_ = int3(1);
  int3 horizon_tracing_dispatch_size_ = int3(1);
  /** 2D tile mask to check which unused adjacent tile we need to clear and which tile we need to
   * dispatch for each work type. */
  Texture tile_raytrace_denoise_tx_ = {"tile_raytrace_denoise_tx_"};
  Texture tile_raytrace_tracing_tx_ = {"tile_raytrace_tracing_tx_"};
  Texture tile_horizon_denoise_tx_ = {"tile_horizon_denoise_tx_"};
  Texture tile_horizon_tracing_tx_ = {"tile_horizon_tracing_tx_"};
  /** Indirect dispatch rays. Avoid dispatching work-groups that will not trace anything. */
  DispatchIndirectBuf raytrace_tracing_dispatch_buf_ = {"raytrace_tracing_dispatch_buf_"};
  /** Indirect dispatch denoise full-resolution tiles. */
  DispatchIndirectBuf raytrace_denoise_dispatch_buf_ = {"raytrace_denoise_dispatch_buf_"};
  /** Indirect dispatch horizon scan. Avoid dispatching work-groups that will not scan anything. */
  DispatchIndirectBuf horizon_tracing_dispatch_buf_ = {"horizon_tracing_dispatch_buf_"};
  /** Indirect dispatch denoise full-resolution tiles. */
  DispatchIndirectBuf horizon_denoise_dispatch_buf_ = {"horizon_denoise_dispatch_buf_"};
  /** Pointer to the texture to store the result of horizon scan in. */
  gpu::Texture *horizon_scan_output_tx_[3] = {nullptr};
  /** Tile buffer that contains tile coordinates. */
  RayTraceTileBuf raytrace_tracing_tiles_buf_ = {"raytrace_tracing_tiles_buf_"};
  RayTraceTileBuf raytrace_denoise_tiles_buf_ = {"raytrace_denoise_tiles_buf_"};
  RayTraceTileBuf horizon_tracing_tiles_buf_ = {"horizon_tracing_tiles_buf_"};
  RayTraceTileBuf horizon_denoise_tiles_buf_ = {"horizon_denoise_tiles_buf_"};
  /** Texture containing the ray direction and PDF. */
  TextureFromPool ray_data_tx_ = {"ray_data_tx"};
  /** Texture containing the ray hit time. */
  TextureFromPool ray_time_tx_ = {"ray_data_tx"};
  /** Texture containing the ray hit radiance (tracing-res). */
  TextureFromPool ray_radiance_tx_ = {"ray_radiance_tx"};
  /** Texture containing the horizon local radiance. */
  TextureFromPool horizon_radiance_tx_[4] = {{"horizon_radiance_tx_"}};
  TextureFromPool horizon_radiance_denoised_tx_[4] = {{"horizon_radiance_denoised_tx_"}};
  /** Texture containing the input screen radiance but re-projected. */
  TextureFromPool downsampled_in_radiance_tx_ = {"downsampled_in_radiance_tx_"};
  /** Texture containing the view space normal. The BSDF normal is arbitrarily chosen. */
  TextureFromPool downsampled_in_normal_tx_ = {"downsampled_in_normal_tx_"};
  /** Textures containing the ray hit radiance denoised (full-res). One of them is result_tx. */
  gpu::Texture *denoised_spatial_tx_ = nullptr;
  gpu::Texture *denoised_temporal_tx_ = nullptr;
  gpu::Texture *denoised_bilateral_tx_ = nullptr;
  /** Ray hit depth for temporal denoising. Output of spatial denoise. */
  TextureFromPool hit_depth_tx_ = {"hit_depth_tx_"};
  /** Ray hit variance for temporal denoising. Output of spatial denoise. */
  TextureFromPool hit_variance_tx_ = {"hit_variance_tx_"};
  /** Temporally stable variance for temporal denoising. Output of temporal denoise. */
  TextureFromPool denoise_variance_tx_ = {"denoise_variance_tx_"};
  /** Persistent texture reference for temporal denoising input. */
  gpu::Texture *radiance_history_tx_ = nullptr;
  gpu::Texture *variance_history_tx_ = nullptr;
  gpu::Texture *tilemask_history_tx_ = nullptr;
  /** Radiance input for screen space tracing. */
  gpu::Texture *screen_radiance_front_tx_ = nullptr;
  gpu::Texture *screen_radiance_back_tx_ = nullptr;

  Texture radiance_dummy_black_tx_ = {"radiance_dummy_black_tx"};
  /** Dummy texture when the tracing is disabled. */
  TextureFromPool dummy_result_tx_ = {"dummy_result_tx"};
  /** Pointer to `inst_.render_buffers.depth_tx` updated before submission. */
  gpu::Texture *renderbuf_depth_view_ = nullptr;

  /** Copy of the scene options to avoid changing parameters during motion blur. */
  RaytraceEEVEE ray_tracing_options_;
  int fast_gi_ray_count_ = 0;
  int fast_gi_step_count_ = 0;
  bool fast_gi_ao_only_ = false;

  bool use_raytracing_ = false;

  RaytraceEEVEE_Method tracing_method_ = RAYTRACE_EEVEE_METHOD_PROBE;

  RayTraceData &data_;

 public:
  RayTraceModule(Instance &inst, RayTraceData &data) : inst_(inst), data_(data) {};

  void init();

  void sync();

  /**
   * RayTrace the scene and resolve radiance buffer for the corresponding `closure_bit`.
   *
   * IMPORTANT: Should not be conditionally executed as it manages the RayTraceResult.
   * IMPORTANT: The screen tracing will be using the front and back Hierarchical-Z Buffer in its
   * current state.
   *
   * \arg rt_buffer is the layer's permanent storage.
   * \arg screen_radiance_back_tx is the texture used for screen space transmission rays.
   * \arg screen_radiance_front_tx is the texture used for screen space reflection rays.
   * \arg screen_radiance_persmat is the view projection matrix used for screen_radiance_front_tx.
   * \arg active_closures is a mask of all active closures in a deferred layer.
   * \arg main_view is the un-jittered view.
   * \arg render_view is the TAA jittered view.
   * \arg force_no_tracing will run the pipeline without any tracing, relying only on local probes.
   */
  RayTraceResult render(RayTraceBuffer &rt_buffer,
                        gpu::Texture *screen_radiance_back_tx,
                        eClosureBits active_closures,
                        /* TODO(fclem): Maybe wrap these two in some other class. */
                        View &main_view,
                        View &render_view);

  /**
   * Only allocate the RayTraceResult results buffers to be used by other passes.
   */
  RayTraceResult alloc_only(RayTraceBuffer &rt_buffer);

  /**
   * Only allocate the RayTraceResult results buffers as dummy texture to ensure correct bindings.
   */
  RayTraceResult alloc_dummy(RayTraceBuffer &rt_buffer);

  void debug_pass_sync();
  void debug_draw(View &view, gpu::FrameBuffer *view_fb);

  bool use_raytracing() const
  {
    return use_raytracing_;
  }

  bool use_fast_gi() const
  {
    return use_raytracing() && ray_tracing_options_.trace_max_roughness < 1.0f;
  }

 private:
  RayTraceResultTexture trace(int closure_index,
                              bool active_layer,
                              RaytraceEEVEE options,
                              RayTraceBuffer &rt_buffer,
                              /* TODO(fclem): Maybe wrap these two in some other class. */
                              View &main_view,
                              View &render_view);
};

/** \} */

}  // namespace blender::eevee
