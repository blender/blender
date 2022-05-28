/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Shading passes contain drawcalls specific to shading pipelines.
 * They are shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#pragma once

#include "DRW_render.h"

/* TODO(fclem): Move it to GPU/DRAW. */
#include "../eevee/eevee_lut.h"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name World Pipeline
 *
 * Render world values.
 * \{ */

class WorldPipeline {
 private:
  Instance &inst_;

  DRWPass *world_ps_ = nullptr;

 public:
  WorldPipeline(Instance &inst) : inst_(inst){};

  void sync(GPUMaterial *gpumat);
  void render();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Pass
 *
 * Handles alpha blended surfaces and NPR materials (using Closure to RGBA).
 * \{ */

class ForwardPipeline {
 private:
  Instance &inst_;

  DRWPass *prepass_ps_ = nullptr;
  DRWPass *prepass_velocity_ps_ = nullptr;
  DRWPass *prepass_culled_ps_ = nullptr;
  DRWPass *prepass_culled_velocity_ps_ = nullptr;
  DRWPass *opaque_ps_ = nullptr;
  DRWPass *opaque_culled_ps_ = nullptr;
  DRWPass *transparent_ps_ = nullptr;

  // GPUTexture *input_screen_radiance_tx_ = nullptr;

 public:
  ForwardPipeline(Instance &inst) : inst_(inst){};

  void sync();

  DRWShadingGroup *material_add(::Material *blender_mat, GPUMaterial *gpumat)
  {
    return (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) ?
               material_transparent_add(blender_mat, gpumat) :
               material_opaque_add(blender_mat, gpumat);
  }

  DRWShadingGroup *prepass_add(::Material *blender_mat, GPUMaterial *gpumat, bool has_motion)
  {
    return (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) ?
               prepass_transparent_add(blender_mat, gpumat) :
               prepass_opaque_add(blender_mat, gpumat, has_motion);
  }

  DRWShadingGroup *material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat);
  DRWShadingGroup *prepass_opaque_add(::Material *blender_mat,
                                      GPUMaterial *gpumat,
                                      bool has_motion);
  DRWShadingGroup *material_transparent_add(::Material *blender_mat, GPUMaterial *gpumat);
  DRWShadingGroup *prepass_transparent_add(::Material *blender_mat, GPUMaterial *gpumat);

  void render(const DRWView *view,
              Framebuffer &prepass_fb,
              Framebuffer &combined_fb,
              GPUTexture *depth_tx,
              GPUTexture *combined_tx);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility texture
 *
 * 64x64 2D array texture containing LUT tables and blue noises.
 * \{ */

class UtilityTexture : public Texture {
  struct Layer {
    float data[UTIL_TEX_SIZE * UTIL_TEX_SIZE][4];
  };

  static constexpr int lut_size = UTIL_TEX_SIZE;
  static constexpr int lut_size_sqr = lut_size * lut_size;
  static constexpr int layer_count = 4 + UTIL_BTDF_LAYER_COUNT;

 public:
  UtilityTexture() : Texture("UtilityTx", GPU_RGBA16F, int2(lut_size), layer_count, nullptr)
  {
#ifdef RUNTIME_LUT_CREATION
    float *bsdf_ggx_lut = EEVEE_lut_update_ggx_brdf(lut_size);
    float(*btdf_ggx_lut)[lut_size_sqr * 2] = (float(*)[lut_size_sqr * 2])
        EEVEE_lut_update_ggx_btdf(lut_size, UTIL_BTDF_LAYER_COUNT);
#else
    const float *bsdf_ggx_lut = bsdf_split_sum_ggx;
    const float(*btdf_ggx_lut)[lut_size_sqr * 2] = btdf_split_sum_ggx;
#endif

    Vector<Layer> data(layer_count);
    {
      Layer &layer = data[UTIL_BLUE_NOISE_LAYER];
      memcpy(layer.data, blue_noise, sizeof(layer));
    }
    {
      Layer &layer = data[UTIL_LTC_MAT_LAYER];
      memcpy(layer.data, ltc_mat_ggx, sizeof(layer));
    }
    {
      Layer &layer = data[UTIL_LTC_MAG_LAYER];
      for (auto i : IndexRange(lut_size_sqr)) {
        layer.data[i][0] = bsdf_ggx_lut[i * 2 + 0];
        layer.data[i][1] = bsdf_ggx_lut[i * 2 + 1];
        layer.data[i][2] = ltc_mag_ggx[i * 2 + 0];
        layer.data[i][3] = ltc_mag_ggx[i * 2 + 1];
      }
      BLI_assert(UTIL_LTC_MAG_LAYER == UTIL_BSDF_LAYER);
    }
    {
      Layer &layer = data[UTIL_DISK_INTEGRAL_LAYER];
      for (auto i : IndexRange(lut_size_sqr)) {
        layer.data[i][UTIL_DISK_INTEGRAL_COMP] = ltc_disk_integral[i];
      }
    }
    {
      for (auto layer_id : IndexRange(16)) {
        Layer &layer = data[3 + layer_id];
        for (auto i : IndexRange(lut_size_sqr)) {
          layer.data[i][0] = btdf_ggx_lut[layer_id][i * 2 + 0];
          layer.data[i][1] = btdf_ggx_lut[layer_id][i * 2 + 1];
        }
      }
    }
    GPU_texture_update_mipmap(*this, 0, GPU_DATA_FLOAT, data.data());
  }

  ~UtilityTexture(){};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipelines
 *
 * Contains Shading passes. Shared between views. Objects will subscribe to at least one of them.
 * \{ */

class PipelineModule {
 public:
  WorldPipeline world;
  // DeferredPipeline deferred;
  ForwardPipeline forward;
  // ShadowPipeline shadow;
  // VelocityPipeline velocity;

  UtilityTexture utility_tx;

 public:
  PipelineModule(Instance &inst) : world(inst), forward(inst){};

  void sync()
  {
    // deferred.sync();
    forward.sync();
    // shadow.sync();
    // velocity.sync();
  }

  DRWShadingGroup *material_add(::Material *blender_mat,
                                GPUMaterial *gpumat,
                                eMaterialPipeline pipeline_type)
  {
    switch (pipeline_type) {
      case MAT_PIPE_DEFERRED_PREPASS:
        // return deferred.prepass_add(blender_mat, gpumat, false);
        break;
      case MAT_PIPE_DEFERRED_PREPASS_VELOCITY:
        // return deferred.prepass_add(blender_mat, gpumat, true);
        break;
      case MAT_PIPE_FORWARD_PREPASS:
        return forward.prepass_add(blender_mat, gpumat, false);
      case MAT_PIPE_FORWARD_PREPASS_VELOCITY:
        return forward.prepass_add(blender_mat, gpumat, true);
      case MAT_PIPE_DEFERRED:
        // return deferred.material_add(blender_mat, gpumat);
        break;
      case MAT_PIPE_FORWARD:
        return forward.material_add(blender_mat, gpumat);
      case MAT_PIPE_VOLUME:
        /* TODO(fclem) volume pass. */
        return nullptr;
      case MAT_PIPE_SHADOW:
        // return shadow.material_add(blender_mat, gpumat);
        break;
    }
    return nullptr;
  }
};

/** \} */

}  // namespace blender::eevee
