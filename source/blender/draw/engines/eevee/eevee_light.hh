/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The light module manages light data buffers and light culling system.
 *
 * The culling follows the principles of Tiled Culling + Z binning from:
 * "Improved Culling for Tiled and Clustered Rendering"
 * by Michal Drobot
 * http://advances.realtimerendering.com/s2017/2017_Sig_Improved_Culling_final.pdf
 *
 * The culling is separated in 4 compute phases:
 * - View Culling (select pass): Create a z distance and a index buffer of visible lights.
 * - Light sorting: Outputs visible lights sorted by Z distance.
 * - Z binning: Compute the Z bins min/max light indices.
 * - Tile intersection: Fine grained 2D culling of each lights outputting a bitmap per tile.
 */

#pragma once

#include "DNA_light_types.h"

#include "DRW_gpu_wrapper.hh"

#include "eevee_camera.hh"
#include "eevee_light_shared.hh"
#include "eevee_sampling.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

class Instance;
class ShadowModule;
class ShadowDirectional;
class ShadowPunctual;

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

using LightCullingDataBuf = draw::StorageBuffer<LightCullingData>;
using LightCullingKeyBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingTileBuf = draw::StorageArrayBuffer<uint, LIGHT_CHUNK, true>;
using LightCullingZbinBuf = draw::StorageArrayBuffer<uint, CULLING_ZBIN_COUNT, true>;
using LightCullingZdistBuf = draw::StorageArrayBuffer<float, LIGHT_CHUNK, true>;
using LightDataBuf = draw::StorageArrayBuffer<LightData, LIGHT_CHUNK>;

struct Light : public LightData, NonCopyable {
 public:
  bool initialized = false;
  bool used = false;

  /** Pointers to source Shadow. Type depends on `LightData::type`. */
  ShadowDirectional *directional = nullptr;
  ShadowPunctual *punctual = nullptr;

  Light()
  {
    /* Avoid valgrind warning. */
    this->type = LIGHT_SUN;
  }

  /* Only used for debugging. */
#ifndef NDEBUG
  Light(Light &&other)
  {
    *static_cast<LightData *>(this) = other;
    this->initialized = other.initialized;
    this->used = other.used;
    this->directional = other.directional;
    this->punctual = other.punctual;
    other.directional = nullptr;
    other.punctual = nullptr;
  }

  ~Light()
  {
    BLI_assert(directional == nullptr);
    BLI_assert(punctual == nullptr);
  }
#endif

  void sync(ShadowModule &shadows,
            float4x4 object_to_world,
            char visibility_flag,
            const ::Light *la,
            const LightLinking *light_linking,
            float threshold);

  void shadow_ensure(ShadowModule &shadows);
  void shadow_discard_safe(ShadowModule &shadows);

  void debug_draw();

 private:
  float shadow_lod_min_get(const ::Light *la);
  float shadow_shape_size_get(const ::Light *la);
  float attenuation_radius_get(const ::Light *la, float light_threshold, float light_power);
  void shape_parameters_set(const ::Light *la,
                            const float3 &scale,
                            const float3 &z_axis,
                            float threshold,
                            bool use_jitter);
  float shape_radiance_get();
  float point_radiance_get();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

/**
 * The light module manages light data buffers and light culling system.
 */
class LightModule {
  friend ShadowModule;

 private:
  /* Keep tile count reasonable for memory usage and 2D culling performance. */
  static constexpr uint max_memory_threshold = 32 * 1024 * 1024; /* 32 MiB */
  static constexpr uint max_word_count_threshold = max_memory_threshold / sizeof(uint);
  static constexpr uint max_tile_count_threshold = 8192;

  Instance &inst_;

  /** Map of light objects data. Converted to flat array each frame. */
  Map<ObjectKey, Light> light_map_;
  ObjectKey world_sunlight_key;
  /** Flat array sent to GPU, populated from light_map_. Source buffer for light culling. */
  LightDataBuf light_buf_ = {"Lights_no_cull"};
  /** Luminous intensity to consider the light boundary at. Used for culling. */
  float light_threshold_ = 0.01f;
  /** If false, will prevent all scene lights from being synced. */
  bool use_scene_lights_ = false;
  /** If false, will prevent all sun lights from being synced. */
  bool use_sun_lights_ = false;
  /** Number of sun lights synced during the last sync. Used as offset. */
  int sun_lights_len_ = 0;
  int local_lights_len_ = 0;
  /** Sun plus local lights count for convenience. */
  int lights_len_ = 0;

  /**
   * Light Culling
   */

  /** LightData buffer used for rendering. Filled by the culling pass. */
  LightDataBuf culling_light_buf_ = {"Lights_culled"};
  /** Culling information. */
  LightCullingDataBuf culling_data_buf_ = {"LightCull_data"};
  /** Z-distance matching the key for each visible lights. Used for sorting. */
  LightCullingZdistBuf culling_zdist_buf_ = {"LightCull_zdist"};
  /** Key buffer containing only visible lights indices. Used for sorting. */
  LightCullingKeyBuf culling_key_buf_ = {"LightCull_key"};
  /** Zbins containing min and max light index for each Z bin. */
  LightCullingZbinBuf culling_zbin_buf_ = {"LightCull_zbin"};
  /** Bitmap of lights touching each tiles. */
  LightCullingTileBuf culling_tile_buf_ = {"LightCull_tile"};
  /** Culling compute passes. */
  PassSimple culling_ps_ = {"LightCulling"};
  /** Total number of words the tile buffer needs to contain for the render resolution. */
  uint total_word_count_ = 0;
  /** Flipped state of the view being processed. True for planar probe views. */
  bool view_is_flipped_ = false;

  /** Update light on the GPU after culling. Ran for each sample. */
  PassSimple update_ps_ = {"LightUpdate"};

  /** Debug Culling visualization. */
  PassSimple debug_draw_ps_ = {"LightCulling.Debug"};

 public:
  LightModule(Instance &inst) : inst_(inst) {};
  ~LightModule();

  void begin_sync();
  void sync_light(const Object *ob, ObjectHandle &handle);
  void end_sync();

  /**
   * Update acceleration structure for the given view.
   */
  void set_view(View &view, const int2 extent);

  void debug_draw(View &view, gpu::FrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_ssbo(LIGHT_CULL_BUF_SLOT, &culling_data_buf_);
    pass.bind_ssbo(LIGHT_BUF_SLOT, &culling_light_buf_);
    pass.bind_ssbo(LIGHT_ZBIN_BUF_SLOT, &culling_zbin_buf_);
    pass.bind_ssbo(LIGHT_TILE_BUF_SLOT, &culling_tile_buf_);
  }

 private:
  void culling_pass_sync();
  void update_pass_sync();
  void debug_pass_sync();
};

/** \} */

}  // namespace blender::eevee
