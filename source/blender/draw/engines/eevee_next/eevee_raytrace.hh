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

#include "DRW_render.h"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Ray-tracing Buffers
 *
 * Contain persistent data used for temporal denoising. Similar to \class GBuffer but only contains
 * persistent data.
 * \{ */

/**
 * Contain persistent buffer that need to be stored per view.
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
   * One for each closure type. Not to be mistaken with deferred layer type.
   * For instance the opaque deferred layer will only used the reflection history buffer.
   */
  DenoiseBuffer reflection, refraction, diffuse;
};

/**
 * Contains the result texture.
 * The result buffer is usually short lived and is kept in a TextureFromPool managed by the mode.
 * This structure contains a reference to it so that it can be freed after use by the caller.
 */
class RayTraceResult {
 private:
  /** Result is in a temporary texture that needs to be released. */
  TextureFromPool *result_ = nullptr;
  /** History buffer to swap the temporary texture that does not need to be released. */
  Texture *history_ = nullptr;

 public:
  RayTraceResult() = default;
  RayTraceResult(TextureFromPool &result) : result_(result.ptr()){};
  RayTraceResult(TextureFromPool &result, Texture &history)
      : result_(result.ptr()), history_(history.ptr()){};

  GPUTexture *get()
  {
    return *result_;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ray-tracing
 * \{ */

class RayTraceModule {
 private:
  Instance &inst_;

  draw::PassSimple tile_classify_ps_ = {"TileClassify"};
  draw::PassSimple tile_compact_ps_ = {"TileCompact"};
  draw::PassSimple generate_diffuse_ps_ = {"RayGenerate.Diffuse"};
  draw::PassSimple generate_reflect_ps_ = {"RayGenerate.Reflection"};
  draw::PassSimple generate_refract_ps_ = {"RayGenerate.Refraction"};
  draw::PassSimple trace_diffuse_ps_ = {"Trace.Diffuse"};
  draw::PassSimple trace_reflect_ps_ = {"Trace.Reflection"};
  draw::PassSimple trace_refract_ps_ = {"Trace.Refraction"};
  draw::PassSimple trace_fallback_ps_ = {"Trace.Fallback"};
  draw::PassSimple denoise_spatial_diffuse_ps_ = {"DenoiseSpatial.Diffuse"};
  draw::PassSimple denoise_spatial_reflect_ps_ = {"DenoiseSpatial.Reflection"};
  draw::PassSimple denoise_spatial_refract_ps_ = {"DenoiseSpatial.Refraction"};
  draw::PassSimple denoise_temporal_ps_ = {"DenoiseTemporal"};
  draw::PassSimple denoise_bilateral_diffuse_ps_ = {"DenoiseBilateral.Diffuse"};
  draw::PassSimple denoise_bilateral_reflect_ps_ = {"DenoiseBilateral.Reflection"};
  draw::PassSimple denoise_bilateral_refract_ps_ = {"DenoiseBilateral.Refraction"};

  /** Dispatch with enough tiles for the whole screen. */
  int3 tile_classify_dispatch_size_ = int3(1);
  /** Dispatch with enough tiles for the tile mask. */
  int3 tile_compact_dispatch_size_ = int3(1);
  /** 2D tile mask to check which unused adjacent tile we need to clear. */
  TextureFromPool tile_mask_tx_ = {"tile_mask_tx"};
  /** Indirect dispatch rays. Avoid dispatching work-groups that will not trace anything.*/
  DispatchIndirectBuf ray_dispatch_buf_ = {"ray_dispatch_buf_"};
  /** Indirect dispatch denoise full-resolution tiles. */
  DispatchIndirectBuf denoise_dispatch_buf_ = {"denoise_dispatch_buf_"};
  /** Tile buffer that contains tile coordinates. */
  RayTraceTileBuf ray_tiles_buf_ = {"ray_tiles_buf_"};
  RayTraceTileBuf denoise_tiles_buf_ = {"denoise_tiles_buf_"};
  /** Texture containing the ray direction and PDF. */
  TextureFromPool ray_data_tx_ = {"ray_data_tx"};
  /** Texture containing the ray hit time. */
  TextureFromPool ray_time_tx_ = {"ray_data_tx"};
  /** Texture containing the ray hit radiance (tracing-res). */
  TextureFromPool ray_radiance_tx_ = {"ray_radiance_tx"};
  /** Textures containing the ray hit radiance denoised (full-res). One of them is result_tx. */
  GPUTexture *denoised_spatial_tx_ = nullptr;
  GPUTexture *denoised_temporal_tx_ = nullptr;
  GPUTexture *denoised_bilateral_tx_ = nullptr;
  /** Ray hit depth for temporal denoising. Output of spatial denoise. */
  TextureFromPool hit_depth_tx_ = {"hit_depth_tx_"};
  /** Ray hit variance for temporal denoising. Output of spatial denoise. */
  TextureFromPool hit_variance_tx_ = {"hit_variance_tx_"};
  /** Temporally stable variance for temporal denoising. Output of temporal denoise. */
  TextureFromPool denoise_variance_tx_ = {"denoise_variance_tx_"};
  /** Persistent texture reference for temporal denoising input. */
  GPUTexture *radiance_history_tx_ = nullptr;
  GPUTexture *variance_history_tx_ = nullptr;
  GPUTexture *tilemask_history_tx_ = nullptr;
  /** Radiance input for screen space tracing. */
  GPUTexture *screen_radiance_tx_ = nullptr;

  /** Dummy texture when the tracing is disabled. */
  TextureFromPool dummy_result_tx_ = {"dummy_result_tx"};
  /** Pointer to `inst_.render_buffers.depth_tx.stencil_view()` updated before submission. */
  GPUTexture *renderbuf_stencil_view_ = nullptr;
  /** Pointer to `inst_.render_buffers.depth_tx` updated before submission. */
  GPUTexture *renderbuf_depth_view_ = nullptr;

  /** Copy of the scene options to avoid changing parameters during motion blur. */
  RaytraceEEVEE reflection_options_;
  RaytraceEEVEE refraction_options_;

  RaytraceEEVEE_Method tracing_method_ = RAYTRACE_EEVEE_METHOD_NONE;

  RayTraceData &data_;

 public:
  RayTraceModule(Instance &inst, RayTraceData &data) : inst_(inst), data_(data){};

  void init();

  void sync();

  /**
   * RayTrace the scene and resolve a radiance buffer for the corresponding `closure_bit` into the
   * given `out_radiance_tx`.
   *
   * IMPORTANT: Should not be conditionally executed as it manages the RayTraceResult.
   * IMPORTANT: The screen tracing will use the Hierarchical-Z Buffer in its current state.
   *
   * \arg screen_radiance_tx is the texture used for screen space rays.
   * \arg screen_radiance_persmat is the view projection matrix used to render screen_radiance_tx.
   * \arg active_closures is a mask of all active closures in a deferred layer.
   * \arg raytrace_closure is type of closure the rays are to be casted for.
   * \arg main_view is the un-jittered view.
   * \arg render_view is the TAA jittered view.
   * \arg force_no_tracing will run the pipeline without any tracing, relying only on local probes.
   */
  RayTraceResult trace(RayTraceBuffer &rt_buffer,
                       GPUTexture *screen_radiance_tx,
                       const float4x4 &screen_radiance_persmat,
                       eClosureBits active_closures,
                       eClosureBits raytrace_closure,
                       View &main_view,
                       View &render_view,
                       bool force_no_tracing = false);

  void debug_pass_sync();
  void debug_draw(View &view, GPUFrameBuffer *view_fb);
};

/** \} */

}  // namespace blender::eevee
