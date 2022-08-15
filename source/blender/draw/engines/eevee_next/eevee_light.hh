/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

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

#include "BLI_bitmap.h"
#include "BLI_vector.hh"
#include "DNA_light_types.h"

#include "eevee_camera.hh"
#include "eevee_sampling.hh"
#include "eevee_shader.hh"
#include "eevee_shader_shared.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

struct Light : public LightData {
 public:
  bool initialized = false;
  bool used = false;

 public:
  Light()
  {
    shadow_id = LIGHT_NO_SHADOW;
  }

  void sync(/* ShadowModule &shadows, */ const Object *ob, float threshold);

  // void shadow_discard_safe(ShadowModule &shadows);

  void debug_draw();

 private:
  float attenuation_radius_get(const ::Light *la, float light_threshold, float light_power);
  void shape_parameters_set(const ::Light *la, const float scale[3]);
  float shape_power_get(const ::Light *la);
  float point_power_get(const ::Light *la);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

/**
 * The light module manages light data buffers and light culling system.
 */
class LightModule {
  // friend ShadowModule;

 private:
  /* Keep tile count reasonable for memory usage and 2D culling performance. */
  static constexpr uint max_memory_threshold = 32 * 1024 * 1024; /* 32 MiB */
  static constexpr uint max_word_count_threshold = max_memory_threshold / sizeof(uint);
  static constexpr uint max_tile_count_threshold = 8192;

  Instance &inst_;

  /** Map of light objects data. Converted to flat array each frame. */
  Map<ObjectKey, Light> light_map_;
  /** Flat array sent to GPU, populated from light_map_. Source buffer for light culling. */
  LightDataBuf light_buf_ = {"Lights_no_cull"};
  /** Recorded size of light_map_ (after pruning) to detect deletion. */
  int64_t light_map_size_ = 0;
  /** Luminous intensity to consider the light boundary at. Used for culling. */
  float light_threshold_ = 0.01f;
  /** If false, will prevent all scene light from being synced. */
  bool use_scene_lights_ = false;
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
  /** Culling infos. */
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
  DRWPass *culling_select_ps_ = nullptr;
  DRWPass *culling_sort_ps_ = nullptr;
  DRWPass *culling_zbin_ps_ = nullptr;
  DRWPass *culling_tile_ps_ = nullptr;
  /** Total number of words the tile buffer needs to contain for the render resolution. */
  uint total_word_count_ = 0;

  /** Debug Culling visualization. */
  DRWPass *debug_draw_ps_ = nullptr;
  /* GPUTexture *input_depth_tx_ = nullptr; */

 public:
  LightModule(Instance &inst) : inst_(inst){};
  ~LightModule(){};

  void begin_sync();
  void sync_light(const Object *ob, ObjectHandle &handle);
  void end_sync();

  /**
   * Update acceleration structure for the given view.
   */
  void set_view(const DRWView *view, const int2 extent);

  void debug_draw(GPUFrameBuffer *view_fb);

  void bind_resources(DRWShadingGroup *grp)
  {
    DRW_shgroup_storage_block_ref(grp, "light_buf", &culling_light_buf_);
    DRW_shgroup_storage_block_ref(grp, "light_cull_buf", &culling_data_buf_);
    DRW_shgroup_storage_block_ref(grp, "light_zbin_buf", &culling_zbin_buf_);
    DRW_shgroup_storage_block_ref(grp, "light_tile_buf", &culling_tile_buf_);
#if 0
    DRW_shgroup_uniform_texture(grp, "shadow_atlas_tx", inst_.shadows.atlas_tx_get());
    DRW_shgroup_uniform_texture(grp, "shadow_tilemaps_tx", inst_.shadows.tilemap_tx_get());
#endif
  }

 private:
  void culling_pass_sync();
  void debug_pass_sync();
};

/** \} */

}  // namespace blender::eevee
