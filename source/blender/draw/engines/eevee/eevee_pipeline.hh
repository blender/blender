/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shading passes contain draw-calls specific to shading pipelines.
 * They are shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#pragma once

#include "BLI_math_bits.h"

#include "DRW_render.hh"

#include "eevee_lut.hh"
#include "eevee_material.hh"
#include "eevee_raytrace.hh"
#include "eevee_subsurface.hh"
#include "eevee_uniform_shared.hh"

struct Camera;

namespace blender::eevee {

class Instance;
struct RayTraceBuffer;

/* -------------------------------------------------------------------- */
/** \name World Background Pipeline
 *
 * Render world background values.
 * \{ */

class BackgroundPipeline {
 private:
  Instance &inst_;

  PassSimple clear_ps_ = {"World.Background.Clear"};
  PassSimple world_ps_ = {"World.Background"};

 public:
  BackgroundPipeline(Instance &inst) : inst_(inst) {};

  void sync(GPUMaterial *gpumat, float background_opacity, float background_blur);
  void clear(View &view);
  void render(View &view, Framebuffer &combined_fb);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name World Probe Pipeline
 *
 * Renders a single side for the world reflection probe.
 * \{ */

class WorldPipeline {
 private:
  Instance &inst_;

  /* Dummy textures: required to reuse background shader and avoid another shader variation. */
  Texture dummy_renderpass_tx_;
  Texture dummy_cryptomatte_tx_;
  Texture dummy_aov_color_tx_;
  Texture dummy_aov_value_tx_;

  PassSimple cubemap_face_ps_ = {"World.Probe"};

 public:
  WorldPipeline(Instance &inst) : inst_(inst) {};

  void sync(GPUMaterial *gpumat);
  void render(View &view);

};  // namespace blender::eevee

/** \} */

/* -------------------------------------------------------------------- */
/** \name World Volume Pipeline
 *
 * \{ */

class WorldVolumePipeline {
 private:
  Instance &inst_;
  bool is_valid_;

  PassSimple world_ps_ = {"World.Volume"};

 public:
  WorldVolumePipeline(Instance &inst) : inst_(inst) {};

  void sync(GPUMaterial *gpumat);
  void render(View &view);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Pass
 *
 * \{ */

class ShadowPipeline {
 private:
  Instance &inst_;

  /* Shadow update pass. */
  PassMain render_ps_ = {"Shadow.Surface"};
  /* Shadow surface render sub-passes. */
  PassMain::Sub *surface_double_sided_ps_ = nullptr;
  PassMain::Sub *surface_single_sided_ps_ = nullptr;

 public:
  ShadowPipeline(Instance &inst) : inst_(inst) {};

  PassMain::Sub *surface_material_add(::Material *material, GPUMaterial *gpumat);

  void sync();

  void render(View &view);
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

  PassMain prepass_ps_ = {"Prepass"};
  PassMain::Sub *prepass_single_sided_static_ps_ = nullptr;
  PassMain::Sub *prepass_single_sided_moving_ps_ = nullptr;
  PassMain::Sub *prepass_double_sided_static_ps_ = nullptr;
  PassMain::Sub *prepass_double_sided_moving_ps_ = nullptr;

  PassMain opaque_ps_ = {"Shading"};
  PassMain::Sub *opaque_single_sided_ps_ = nullptr;
  PassMain::Sub *opaque_double_sided_ps_ = nullptr;

  PassSortable transparent_ps_ = {"Forward.Transparent"};
  float3 camera_forward_;

  bool has_opaque_ = false;
  bool has_transparent_ = false;

 public:
  ForwardPipeline(Instance &inst) : inst_(inst) {};

  void sync();

  PassMain::Sub *prepass_opaque_add(::Material *blender_mat, GPUMaterial *gpumat, bool has_motion);
  PassMain::Sub *material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat);

  PassMain::Sub *prepass_transparent_add(const Object *ob,
                                         ::Material *blender_mat,
                                         GPUMaterial *gpumat);
  PassMain::Sub *material_transparent_add(const Object *ob,
                                          ::Material *blender_mat,
                                          GPUMaterial *gpumat);

  void render(View &view, Framebuffer &prepass_fb, Framebuffer &combined_fb, int2 extent);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred lighting.
 * \{ */

struct DeferredLayerBase {
  PassMain prepass_ps_ = {"Prepass"};
  PassMain::Sub *prepass_single_sided_static_ps_ = nullptr;
  PassMain::Sub *prepass_single_sided_moving_ps_ = nullptr;
  PassMain::Sub *prepass_double_sided_static_ps_ = nullptr;
  PassMain::Sub *prepass_double_sided_moving_ps_ = nullptr;

  PassMain gbuffer_ps_ = {"Shading"};
  /* Shaders that use the ClosureToRGBA node needs to be rendered first.
   * Consider they hybrid forward and deferred. */
  PassMain::Sub *gbuffer_single_sided_hybrid_ps_ = nullptr;
  PassMain::Sub *gbuffer_double_sided_hybrid_ps_ = nullptr;
  PassMain::Sub *gbuffer_single_sided_ps_ = nullptr;
  PassMain::Sub *gbuffer_double_sided_ps_ = nullptr;

  /* Closures bits from the materials in this pass. */
  eClosureBits closure_bits_ = CLOSURE_NONE;
  /* Maximum closure count considering all material in this pass. */
  int closure_count_ = 0;

  /* Stencil values used during the deferred pipeline. */
  enum class StencilBits : uint8_t {
    /* Bits 0 to 1 are reserved for closure count [0..3]. */
    CLOSURE_COUNT_0 = (1u << 0u),
    CLOSURE_COUNT_1 = (1u << 1u),
    /* Set for pixels have a transmission closure. */
    TRANSMISSION = (1u << 2u),
    /** Bits set by the StencilClassify pass. Set per pixel from gbuffer header data. */
    HEADER_BITS = CLOSURE_COUNT_0 | CLOSURE_COUNT_1 | TRANSMISSION,

    /* Set for materials that uses the shadow amend pass. */
    THICKNESS_FROM_SHADOW = (1u << 3u),
    /** Bits set by the material gbuffer pass. Set per materials. */
    MATERIAL_BITS = THICKNESS_FROM_SHADOW,
  };

  /* Return the amount of gbuffer layer needed. */
  int header_layer_count() const
  {
    /* Default header. */
    int count = 1;
    /* SSS, light linking, shadow offset all require an additional layer to store the object ID.
     * Since tracking these are not part of the closure bits and are rather common features,
     * always require one layer for it. */
    count += 1;
    return count;
  }

  /* Return the amount of gbuffer layer needed. */
  int closure_layer_count() const
  {
    /* Always allocate 2 layer per closure for interleaved closure data packing in the gbuffer. */
    return 2 * to_gbuffer_bin_count(closure_bits_);
  }

  /* Return the amount of gbuffer layer needed. */
  int normal_layer_count() const
  {
    /* TODO(fclem): We could count the number of different tangent frame in the shader and use
     * min(tangent_frame_count, closure_count) once we have the normal reuse optimization.
     * For now, allocate a custom normal layer for each Closure. */
    int count = to_gbuffer_bin_count(closure_bits_);
    /* Count the additional information layer needed by some closures. */
    count += count_bits_i(closure_bits_ &
                          (CLOSURE_SSS | CLOSURE_TRANSLUCENT | CLOSURE_REFRACTION));
    return count;
  }

  eClosureBits closure_bits_get() const
  {
    return closure_bits_;
  }

  void gbuffer_pass_sync(Instance &inst);
};

class DeferredPipeline;

class DeferredLayer : DeferredLayerBase {
  friend DeferredPipeline;

 private:
  Instance &inst_;

  static constexpr int max_lighting_tile_count_ = 128 * 128;

  /* Evaluate all light objects contribution. */
  PassSimple eval_light_ps_ = {"EvalLights"};
  /* Combine direct and indirect light contributions and apply BSDF color. */
  PassSimple combine_ps_ = {"Combine"};

  /**
   * Accumulation textures for all stages of lighting evaluation (Light, SSR, SSSS, SSGI ...).
   * These are split and separate from the main radiance buffer in order to accumulate light for
   * the render passes and avoid too much bandwidth waste. Otherwise, we would have to load the
   * BSDF color and do additive blending for each of the lighting step.
   *
   * NOTE: Not to be confused with the render passes.
   * NOTE: Using array of texture instead of texture array to allow to use TextureFromPool.
   */
  TextureFromPool direct_radiance_txs_[3] = {
      {"direct_radiance_1"}, {"direct_radiance_2"}, {"direct_radiance_3"}};
  /* NOTE: Only used when `use_split_radiance` is true. */
  TextureFromPool indirect_radiance_txs_[3] = {
      {"indirect_radiance_1"}, {"indirect_radiance_2"}, {"indirect_radiance_3"}};
  /* Used when there is no indirect radiance buffer. */
  Texture dummy_black = {"dummy_black"};
  /* Reference to ray-tracing results. */
  gpu::Texture *radiance_feedback_tx_ = nullptr;

  /**
   * Tile texture containing several bool per tile indicating presence of feature.
   * It is used to select specialized shader for each tile.
   */
  Texture tile_mask_tx_ = {"tile_mask_tx_"};

  RayTraceResult indirect_result_;

  bool use_split_radiance_ = true;
  /* Output radiance from the combine shader instead of copy. Allow passing unclamped result. */
  bool use_feedback_output_ = false;
  bool use_raytracing_ = false;
  bool use_screen_transmission_ = false;
  bool use_screen_reflection_ = false;
  bool use_clamp_direct_ = false;
  bool use_clamp_indirect_ = false;

 public:
  DeferredLayer(Instance &inst) : inst_(inst)
  {
    float4 data(0.0f);
    dummy_black.ensure_2d(gpu::TextureFormat::RAYTRACE_RADIANCE_FORMAT,
                          int2(1),
                          GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE,
                          data);
  }

  void begin_sync();
  void end_sync(bool is_first_pass, bool is_last_pass, bool next_layer_has_transmission);

  PassMain::Sub *prepass_add(::Material *blender_mat, GPUMaterial *gpumat, bool has_motion);
  PassMain::Sub *material_add(::Material *blender_mat, GPUMaterial *gpumat);

  bool is_empty() const
  {
    return closure_count_ == 0;
  }

  bool has_transmission() const
  {
    return closure_bits_ & CLOSURE_TRANSMISSION;
  }

  /* Do we compute indirect lighting inside the light eval pass. */
  static bool do_merge_direct_indirect_eval(const Instance &inst);
  /* Is the radiance split for the lighting pass. */
  static bool do_split_direct_indirect_radiance(const Instance &inst);

  /* Returns the radiance buffer to feed the next layer. */
  gpu::Texture *render(View &main_view,
                       View &render_view,
                       Framebuffer &prepass_fb,
                       Framebuffer &combined_fb,
                       Framebuffer &gbuffer_fb,
                       int2 extent,
                       RayTraceBuffer &rt_buffer,
                       gpu::Texture *radiance_behind_tx);
};

class DeferredPipeline {
  friend DeferredLayer;

 private:
  /* Gbuffer filling passes. We could have an arbitrary number of them but for now we just have
   * a hardcoded number of them. */
  DeferredLayer opaque_layer_;
  DeferredLayer refraction_layer_;
  DeferredLayer volumetric_layer_;

  PassSimple debug_draw_ps_ = {"debug_gbuffer"};

  bool use_combined_lightprobe_eval;

 public:
  DeferredPipeline(Instance &inst)
      : opaque_layer_(inst), refraction_layer_(inst), volumetric_layer_(inst) {};

  void begin_sync();
  void end_sync();

  PassMain::Sub *prepass_add(::Material *blender_mat, GPUMaterial *gpumat, bool has_motion);
  PassMain::Sub *material_add(::Material *blender_mat, GPUMaterial *gpumat);

  void render(View &main_view,
              View &render_view,
              Framebuffer &prepass_fb,
              Framebuffer &combined_fb,
              Framebuffer &gbuffer_fb,
              int2 extent,
              RayTraceBuffer &rt_buffer_opaque_layer,
              RayTraceBuffer &rt_buffer_refract_layer);

  /* Return the maximum amount of gbuffer layer needed. */
  int header_layer_count() const
  {
    return max_ii(opaque_layer_.header_layer_count(), refraction_layer_.header_layer_count());
  }

  /* Return the maximum amount of gbuffer layer needed. */
  int closure_layer_count() const
  {
    return max_ii(opaque_layer_.closure_layer_count(), refraction_layer_.closure_layer_count());
  }

  /* Return the maximum amount of gbuffer layer needed. */
  int normal_layer_count() const
  {
    return max_ii(opaque_layer_.normal_layer_count(), refraction_layer_.normal_layer_count());
  }

  void debug_draw(draw::View &view, gpu::FrameBuffer *combined_fb);

  bool is_empty() const
  {
    return opaque_layer_.is_empty() && refraction_layer_.is_empty();
  }

  eClosureBits closure_bits_get() const
  {
    return opaque_layer_.closure_bits_get() | refraction_layer_.closure_bits_get();
  }

 private:
  void debug_pass_sync();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Pass
 *
 * \{ */

struct VolumeObjectBounds {
  /* Screen 2D bounds for layer intersection check. */
  std::optional<Bounds<float2>> screen_bounds;
  /* Combined bounds in Z. Allow tighter integration bounds. */
  std::optional<Bounds<float>> z_range;

  VolumeObjectBounds(const Camera &camera, Object *ob);
};

/**
 * A volume layer contains a list of non-overlapping volume objects.
 */
class VolumeLayer {
 public:
  bool use_hit_list = false;
  bool is_empty = true;
  bool finalized = false;
  bool has_scatter = false;
  bool has_absorption = false;

 private:
  Instance &inst_;

  PassMain volume_layer_ps_ = {"Volume.Layer"};
  /* Sub-passes of volume_layer_ps. */
  PassMain::Sub *occupancy_ps_;
  PassMain::Sub *material_ps_;
  /* List of bounds from all objects contained inside this pass. */
  Vector<std::optional<Bounds<float2>>> object_bounds_;
  /* Combined bounds from object_bounds_. */
  std::optional<Bounds<float2>> combined_screen_bounds_;

 public:
  VolumeLayer(Instance &inst) : inst_(inst)
  {
    this->sync();
  }

  PassMain::Sub *occupancy_add(const Object *ob,
                               const ::Material *blender_mat,
                               GPUMaterial *gpumat);
  PassMain::Sub *material_add(const Object *ob,
                              const ::Material *blender_mat,
                              GPUMaterial *gpumat);

  /* Return true if the given bounds overlaps any of the contained object in this layer. */
  bool bounds_overlaps(const VolumeObjectBounds &object_bounds) const;

  void add_object_bound(const VolumeObjectBounds &object_bounds);

  void sync();
  void render(View &view, Texture &occupancy_tx);
};

class VolumePipeline {
 private:
  Instance &inst_;

  Vector<std::unique_ptr<VolumeLayer>> layers_;

  /* Combined bounds in Z. Allow tighter integration bounds. */
  std::optional<Bounds<float>> object_integration_range_;
  /* Aggregated properties of all volume objects. */
  bool has_scatter_ = false;
  bool has_absorption_ = false;

 public:
  VolumePipeline(Instance &inst) : inst_(inst) {};

  void sync();
  void render(View &view, Texture &occupancy_tx);

  /**
   * Returns correct volume layer for a given object and add the object to the layer.
   * Returns nullptr if the object is not visible at all.
   */
  VolumeLayer *register_and_get_layer(Object *ob);

  std::optional<Bounds<float>> object_integration_range() const;

  bool has_scatter() const
  {
    for (const auto &layer : layers_) {
      if (layer->has_scatter) {
        return true;
      }
    }
    return false;
  }
  bool has_absorption() const
  {
    for (const auto &layer : layers_) {
      if (layer->has_absorption) {
        return true;
      }
    }
    return false;
  }

  /* Returns true if any volume layer uses the hist list. */
  bool use_hit_list() const;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Probe Capture.
 * \{ */

class DeferredProbePipeline {
 private:
  Instance &inst_;

  DeferredLayerBase opaque_layer_;

  PassSimple eval_light_ps_ = {"EvalLights"};

 public:
  DeferredProbePipeline(Instance &inst) : inst_(inst) {};

  void begin_sync();
  void end_sync();

  PassMain::Sub *prepass_add(::Material *blender_mat, GPUMaterial *gpumat);
  PassMain::Sub *material_add(::Material *blender_mat, GPUMaterial *gpumat);

  void render(View &view,
              Framebuffer &prepass_fb,
              Framebuffer &combined_fb,
              Framebuffer &gbuffer_fb,
              int2 extent);

  /* Return the maximum amount of gbuffer layer needed. */
  int header_layer_count() const
  {
    return opaque_layer_.header_layer_count();
  }

  /* Return the maximum amount of gbuffer layer needed. */
  int closure_layer_count() const
  {
    return opaque_layer_.closure_layer_count();
  }

  /* Return the maximum amount of gbuffer layer needed. */
  int normal_layer_count() const
  {
    return opaque_layer_.normal_layer_count();
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Planar Probe Capture.
 * \{ */

class PlanarProbePipeline : DeferredLayerBase {
 private:
  Instance &inst_;

  PassSimple eval_light_ps_ = {"EvalLights"};

 public:
  PlanarProbePipeline(Instance &inst) : inst_(inst) {};

  void begin_sync();
  void end_sync();

  PassMain::Sub *prepass_add(::Material *blender_mat, GPUMaterial *gpumat);
  PassMain::Sub *material_add(::Material *blender_mat, GPUMaterial *gpumat);

  void render(View &view,
              gpu::Texture *depth_layer_tx,
              Framebuffer &gbuffer,
              Framebuffer &combined_fb,
              int2 extent);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Capture Pipeline
 *
 * \{ */

class CapturePipeline {
 private:
  Instance &inst_;

  PassMain surface_ps_ = {"Capture.Surface"};

 public:
  CapturePipeline(Instance &inst) : inst_(inst) {};

  PassMain::Sub *surface_material_add(::Material *blender_mat, GPUMaterial *gpumat);

  void sync();
  void render(View &view);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility texture
 *
 * 64x64 2D array texture containing LUT tables and blue noises.
 * \{ */

class UtilityTexture : public Texture {
  struct Layer {
    float4 data[UTIL_TEX_SIZE][UTIL_TEX_SIZE];
  };

  static constexpr int lut_size = UTIL_TEX_SIZE;
  static constexpr int lut_size_sqr = lut_size * lut_size;
  static constexpr int layer_count = UTIL_BTDF_LAYER + UTIL_BTDF_LAYER_COUNT;

 public:
  UtilityTexture()
      : Texture("UtilityTx",
                gpu::TextureFormat::SFLOAT_16_16_16_16,
                GPU_TEXTURE_USAGE_SHADER_READ,
                int2(lut_size),
                layer_count,
                nullptr)
  {
    Vector<Layer> data(layer_count);
    {
      Layer &layer = data[UTIL_BLUE_NOISE_LAYER];
      memcpy(layer.data, lut::blue_noise, sizeof(layer));
    }
    {
      Layer &layer = data[UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER];
      for (auto y : IndexRange(lut_size)) {
        for (auto x : IndexRange(lut_size)) {
          /* Repeatedly stored on every row for correct interpolation. */
          layer.data[y][x][0] = lut::burley_sss_profile[x][0];
          layer.data[y][x][1] = lut::random_walk_sss_profile[x][0];
          layer.data[y][x][2] = 0.0f;
          layer.data[y][x][UTIL_DISK_INTEGRAL_COMP] = lut::ltc_disk_integral[y][x][0];
        }
      }
      BLI_assert(UTIL_SSS_TRANSMITTANCE_PROFILE_LAYER == UTIL_DISK_INTEGRAL_LAYER);
    }
    {
      Layer &layer = data[UTIL_LTC_MAT_LAYER];
      memcpy(layer.data, lut::ltc_mat_ggx, sizeof(layer));
    }
    {
      Layer &layer = data[UTIL_BSDF_LAYER];
      for (auto x : IndexRange(lut_size)) {
        for (auto y : IndexRange(lut_size)) {
          layer.data[y][x][0] = lut::brdf_ggx[y][x][0];
          layer.data[y][x][1] = lut::brdf_ggx[y][x][1];
          layer.data[y][x][2] = lut::brdf_ggx[y][x][2];
          layer.data[y][x][3] = 0.0f;
        }
      }
    }
    {
      for (auto layer_id : IndexRange(16)) {
        Layer &layer = data[UTIL_BTDF_LAYER + layer_id];
        for (auto x : IndexRange(lut_size)) {
          for (auto y : IndexRange(lut_size)) {
            layer.data[y][x][0] = lut::bsdf_ggx[layer_id][y][x][0];
            layer.data[y][x][1] = lut::bsdf_ggx[layer_id][y][x][1];
            layer.data[y][x][2] = lut::bsdf_ggx[layer_id][y][x][2];
            layer.data[y][x][3] = lut::btdf_ggx[layer_id][y][x][0];
          }
        }
      }
    }
    GPU_texture_update_mipmap(*this, 0, GPU_DATA_FLOAT, data.data());
  }

  ~UtilityTexture() = default;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipelines
 *
 * Contains Shading passes. Shared between views. Objects will subscribe to at least one of them.
 * \{ */

class PipelineModule {
 public:
  BackgroundPipeline background;
  WorldPipeline world;
  WorldVolumePipeline world_volume;
  DeferredProbePipeline probe;
  PlanarProbePipeline planar;
  DeferredPipeline deferred;
  ForwardPipeline forward;
  ShadowPipeline shadow;
  VolumePipeline volume;
  CapturePipeline capture;

  UtilityTexture utility_tx;
  PipelineInfoData &data;

  PipelineModule(Instance &inst, PipelineInfoData &data)
      : background(inst),
        world(inst),
        world_volume(inst),
        probe(inst),
        planar(inst),
        deferred(inst),
        forward(inst),
        shadow(inst),
        volume(inst),
        capture(inst),
        data(data) {};

  void begin_sync()
  {
    data.is_sphere_probe = false;
    probe.begin_sync();
    planar.begin_sync();
    deferred.begin_sync();
    forward.sync();
    shadow.sync();
    volume.sync();
    capture.sync();
  }

  void end_sync()
  {
    probe.end_sync();
    planar.end_sync();
    deferred.end_sync();
  }

  PassMain::Sub *material_add(Object * /*ob*/ /* TODO remove. */,
                              ::Material *blender_mat,
                              GPUMaterial *gpumat,
                              eMaterialPipeline pipeline_type,
                              eMaterialProbe probe_capture)
  {
    if (probe_capture == MAT_PROBE_REFLECTION) {
      switch (pipeline_type) {
        case MAT_PIPE_PREPASS_DEFERRED:
          return probe.prepass_add(blender_mat, gpumat);
        case MAT_PIPE_DEFERRED:
          return probe.material_add(blender_mat, gpumat);
        default:
          BLI_assert_unreachable();
          break;
      }
    }
    if (probe_capture == MAT_PROBE_PLANAR) {
      switch (pipeline_type) {
        case MAT_PIPE_PREPASS_PLANAR:
          return planar.prepass_add(blender_mat, gpumat);
        case MAT_PIPE_DEFERRED:
          return planar.material_add(blender_mat, gpumat);
        default:
          BLI_assert_unreachable();
          break;
      }
    }

    switch (pipeline_type) {
      case MAT_PIPE_PREPASS_DEFERRED:
        return deferred.prepass_add(blender_mat, gpumat, false);
      case MAT_PIPE_PREPASS_FORWARD:
        return forward.prepass_opaque_add(blender_mat, gpumat, false);
      case MAT_PIPE_PREPASS_OVERLAP:
        BLI_assert_msg(0, "Overlap prepass should register to the forward pipeline directly.");
        return nullptr;

      case MAT_PIPE_PREPASS_DEFERRED_VELOCITY:
        return deferred.prepass_add(blender_mat, gpumat, true);
      case MAT_PIPE_PREPASS_FORWARD_VELOCITY:
        return forward.prepass_opaque_add(blender_mat, gpumat, true);

      case MAT_PIPE_DEFERRED:
        return deferred.material_add(blender_mat, gpumat);
      case MAT_PIPE_FORWARD:
        return forward.material_opaque_add(blender_mat, gpumat);
      case MAT_PIPE_SHADOW:
        return shadow.surface_material_add(blender_mat, gpumat);
      case MAT_PIPE_CAPTURE:
        return capture.surface_material_add(blender_mat, gpumat);

      case MAT_PIPE_VOLUME_OCCUPANCY:
      case MAT_PIPE_VOLUME_MATERIAL:
        BLI_assert_msg(0, "Volume shaders must register to the volume pipeline directly.");
        return nullptr;

      case MAT_PIPE_PREPASS_PLANAR:
        /* Should be handled by the `probe_capture == MAT_PROBE_PLANAR` case. */
        BLI_assert_unreachable();
        return nullptr;
    }
    return nullptr;
  }
};

/** \} */

}  // namespace blender::eevee
