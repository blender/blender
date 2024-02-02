/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The shadow module manages shadow update tagging & shadow rendering.
 */

#pragma once

#include "BLI_pool.hh"
#include "BLI_vector.hh"

#include "GPU_batch.h"

#include "eevee_camera.hh"
#include "eevee_material.hh"
#include "eevee_shader.hh"
#include "eevee_shader_shared.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

class Instance;
class ShadowModule;
class ShadowPipeline;
struct Light;

enum eCubeFace {
  /* Ordering by culling order. If cone aperture is shallow, we cull the later view. */
  Z_NEG = 0,
  X_POS,
  X_NEG,
  Y_POS,
  Y_NEG,
  Z_POS,
};

/* To be applied after view matrix. Follow same order as eCubeFace. */
constexpr static const float shadow_face_mat[6][4][4] = {
    {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}},   /* Z_NEG */
    {{0, 0, -1, 0}, {-1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}}, /* X_POS */
    {{0, 0, 1, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},   /* X_NEG */
    {{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},  /* Y_POS */
    {{-1, 0, 0, 0}, {0, 0, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 1}},  /* Y_NEG */
    {{1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, -1, 0}, {0, 0, 0, 1}}, /* Z_POS */
};

/* Converts to [-SHADOW_TILEMAP_RES / 2..SHADOW_TILEMAP_RES / 2] for XY and [0..1] for Z. */
constexpr static const float shadow_clipmap_scale_mat[4][4] = {{SHADOW_TILEMAP_RES / 2, 0, 0, 0},
                                                               {0, SHADOW_TILEMAP_RES / 2, 0, 0},
                                                               {0, 0, 0.5, 0},
                                                               {0, 0, 0.5, 1}};

/* Technique used for updating the virtual shadow map contents. */
enum class ShadowTechnique {
  /* Default virtual shadow map update using large virtual framebuffer to rasterize geometry with
   * per-fragment textureAtomicMin to perform depth-test and indirectly store nearest depth value
   * in the shadow atlas. */
  ATOMIC_RASTER = 0,

  /* Tile-architecture optimized virtual shadow map update, leveraging on-tile memory for clearing
   * and depth-testing during geometry rasterization to avoid atomic operations, simplify mesh
   * depth shader and only perform a single storage operation per pixel. This technique performs
   * a 3-pass solution, first clearing tiles, updating depth and storing final results. */
  TILE_COPY = 1,
};

/* -------------------------------------------------------------------- */
/** \name Tile-Map
 *
 * Stores indirection table and states of each tile of a virtual shadow-map.
 * One tile-map has the effective resolution of `pagesize * tile_map_resolution`.
 * Each tile-map overhead is quite small if they do not have any pages allocated.
 *
 * \{ */

struct ShadowTileMap : public ShadowTileMapData {
  static constexpr int64_t tile_map_resolution = SHADOW_TILEMAP_RES;
  static constexpr int64_t tiles_count = tile_map_resolution * tile_map_resolution;

  /** Level of detail for clipmap. */
  int level = INT_MAX;
  /** Cube face index. */
  eCubeFace cubeface = Z_NEG;
  /** Cached, used for detecting updates. */
  float4x4 object_mat;

 public:
  ShadowTileMap(int tiles_index_)
  {
    tiles_index = tiles_index_;
    /* For now just the same index. */
    clip_data_index = tiles_index_ / SHADOW_TILEDATA_PER_TILEMAP;
    this->set_dirty();
  }

  void sync_orthographic(const float4x4 &object_mat_,
                         int2 origin_offset,
                         int clipmap_level,
                         float lod_bias_,
                         eShadowProjectionType projection_type_);

  void sync_cubeface(const float4x4 &object_mat,
                     float near,
                     float far,
                     float side,
                     float shift,
                     eCubeFace face,
                     float lod_bias_);

  void debug_draw() const;

  void set_dirty()
  {
    grid_shift = int2(SHADOW_TILEMAP_RES);
  }

  void set_updated()
  {
    grid_shift = int2(0);
  }
};

/**
 * The tile-maps are managed on CPU and associated with each light shadow object.
 *
 * The number of tile-maps & tiles is unbounded (to the limit of SSBOs), but the actual number
 * used for rendering is caped to 4096. This is to simplify tile-maps management on CPU.
 *
 * At sync end, all tile-maps are grouped by light inside the ShadowTileMapDataBuf so that each
 * light has a contiguous range of tile-maps to refer to.
 */
struct ShadowTileMapPool {
 public:
  /** Limit the width of the texture. */
  static constexpr int64_t maps_per_row = SHADOW_TILEMAP_PER_ROW;

  /** Vector containing available offset to tile range in the ShadowTileDataBuf. */
  Vector<uint> free_indices;
  /** Pool containing shadow tile structure on CPU. */
  Pool<ShadowTileMap> tilemap_pool;
  /** Sorted descriptions for each tile-map in the pool. Updated each frame. */
  ShadowTileMapDataBuf tilemaps_data = {"tilemaps_data"};
  /** Previously used tile-maps that needs to release their tiles/pages. Updated each frame. */
  ShadowTileMapDataBuf tilemaps_unused = {"tilemaps_unused"};
  /** All possible tiles. A range of tiles tile is referenced by a tile-map. */
  ShadowTileDataBuf tiles_data = {"tiles_data"};
  /** Clip range for directional shadows. Updated on GPU. Persistent. */
  ShadowTileMapClipBuf tilemaps_clip = {"tilemaps_clip"};
  /** Texture equivalent of ShadowTileDataBuf but grouped by light. */
  Texture tilemap_tx = {"tilemap_tx"};
  /** Number of free tile-maps at the end of the previous sync. */
  int64_t last_free_len = 0;

 public:
  ShadowTileMapPool();

  ShadowTileMap *acquire();

  /**
   * Push the given list of ShadowTileMap onto the free stack. Their pages will be free.
   */
  void release(Span<ShadowTileMap *> free_list);

  void end_sync(ShadowModule &module);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Casters & Receivers
 *
 * \{ */

/* Can be either a shadow caster or a shadow receiver. */
struct ShadowObject {
  ResourceHandle resource_handle = {0};
  bool used = true;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShadowModule
 *
 * Manages shadow atlas and shadow region data.
 * \{ */

class ShadowModule {
  friend ShadowPunctual;
  friend ShadowDirectional;
  friend ShadowPipeline;
  friend ShadowTileMapPool;

 public:
  /* Shadowing technique. */
  static ShadowTechnique shadow_technique;

  /** Need to be first because of destructor order. */
  ShadowTileMapPool tilemap_pool;

  Pool<ShadowPunctual> punctual_pool;
  Pool<ShadowDirectional> directional_pool;

 private:
  Instance &inst_;

  ShadowSceneData &data_;

  /** Map of shadow casters to track deletion & update of intersected shadows. */
  Map<ObjectKey, ShadowObject> objects_;

  /* -------------------------------------------------------------------- */
  /** \name Tile-map Management
   * \{ */

  PassSimple tilemap_setup_ps_ = {"TilemapSetup"};
  PassMain tilemap_usage_ps_ = {"TagUsage"};
  PassSimple tilemap_update_ps_ = {"TilemapUpdate"};

  PassMain::Sub *tilemap_usage_transparent_ps_ = nullptr;
  GPUBatch *box_batch_ = nullptr;
  /* Source texture for depth buffer analysis. */
  GPUTexture *src_depth_tx_ = nullptr;

  Framebuffer usage_tag_fb;

  PassSimple caster_update_ps_ = {"CasterUpdate"};
  /** List of Resource IDs (to get bounds) for tagging passes. */
  StorageVectorBuffer<uint, 128> past_casters_updated_ = {"PastCastersUpdated"};
  StorageVectorBuffer<uint, 128> curr_casters_updated_ = {"CurrCastersUpdated"};
  /** List of Resource IDs (to get bounds) for getting minimum clip-maps bounds. */
  StorageVectorBuffer<uint, 128> curr_casters_ = {"CurrCasters"};

  /** Indirect arguments for page clearing. */
  DispatchIndirectBuf clear_dispatch_buf_ = {"clear_dispatch_buf"};
  /** Indirect arguments for TBDR Tile Page passes. */
  DrawIndirectBuf tile_draw_buf_ = {"tile_draw_buf"};
  /** A compact stream of rendered tile coordinates in the shadow atlas. */
  StorageArrayBuffer<uint, SHADOW_RENDER_MAP_SIZE, true> dst_coord_buf_ = {"dst_coord_buf"};
  /** A compact stream of rendered tile coordinates in the framebuffer. */
  StorageArrayBuffer<uint, SHADOW_RENDER_MAP_SIZE, true> src_coord_buf_ = {"src_coord_buf"};
  /** Same as dst_coord_buf_ but is not compact. More like a linear texture. */
  StorageArrayBuffer<uint, SHADOW_RENDER_MAP_SIZE, true> render_map_buf_ = {"render_map_buf"};
  /** View to viewport index mapping. */
  StorageArrayBuffer<uint, SHADOW_VIEW_MAX, true> viewport_index_buf_ = {"viewport_index_buf"};

  int3 dispatch_depth_scan_size_;
  /* Ratio between tile-map pixel world "radius" and film pixel world "radius". */
  float tilemap_projection_ratio_;
  float pixel_world_radius_;
  int2 usage_tag_fb_resolution_;
  int usage_tag_fb_lod_ = 5;

  /* Statistics that are read back to CPU after a few frame (to avoid stall). */
  SwapChain<ShadowStatisticsBuf, 5> statistics_buf_;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Page Management
   * \{ */

  static constexpr eGPUTextureFormat atlas_type = GPU_R32UI;
  /** Atlas containing all physical pages. */
  Texture atlas_tx_ = {"shadow_atlas_tx_"};

  /** Pool of unallocated pages waiting to be assigned to specific tiles in the tile-map atlas. */
  ShadowPageHeapBuf pages_free_data_ = {"PagesFreeBuf"};
  /** Pool of cached tiles waiting to be reused. */
  ShadowPageCacheBuf pages_cached_data_ = {"PagesCachedBuf"};
  /** Infos for book keeping and debug. */
  ShadowPagesInfoDataBuf pages_infos_data_ = {"PagesInfosBuf"};

  int3 copy_dispatch_size_;
  int3 scan_dispatch_size_;
  int rendering_tilemap_;
  int rendering_lod_;
  bool do_full_update = true;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Rendering
   * \{ */

  /** Multi-View containing a maximum of 64 view to be rendered with the shadow pipeline. */
  View shadow_multi_view_ = {"ShadowMultiView", SHADOW_VIEW_MAX, true};
  /** Framebuffer with the atlas_tx attached. */
  Framebuffer render_fb_ = {"shadow_write_framebuffer"};

  /* NOTE(Metal): Metal requires memoryless textures to be created which represent attachments in
   * the shadow write frame-buffer. These textures do not occupy any physical memory, but require a
   * Texture object containing its parameters. */
  Texture shadow_depth_fb_tx_ = {"shadow_depth_fb_tx_"};
  Texture shadow_depth_accum_tx_ = {"shadow_depth_accum_tx_"};

  /** Arrays of viewports to rendering each tile to. */
  std::array<int4, 16> multi_viewports_;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Debugging
   * \{ */

  /** Display information about the virtual shadows. */
  PassSimple debug_draw_ps_ = {"Shadow.Debug"};

  /** \} */

  /** Scene immutable parameters. */

  /** For now, needs to be hardcoded. */
  int shadow_page_size_ = SHADOW_PAGE_RES;
  /** Amount of bias to apply to the LOD computed at the tile usage tagging stage. */
  float lod_bias_ = 0.0f;
  /** Maximum number of allocated pages. Maximum value is SHADOW_MAX_TILEMAP. */
  int shadow_page_len_ = SHADOW_MAX_TILEMAP;
  /** Global switch. */
  bool enabled_ = true;

 public:
  ShadowModule(Instance &inst, ShadowSceneData &data);
  ~ShadowModule(){};

  void init();

  void begin_sync();
  /** Register a shadow caster or receiver. */
  void sync_object(const Object *ob,
                   const ObjectHandle &handle,
                   const ResourceHandle &resource_handle,
                   bool is_alpha_blend);
  void end_sync();

  void set_lights_data();

  /* Update all shadow regions visible inside the view.
   * If called multiple time for the same view, it will only do the depth buffer scanning
   * to check any new opaque surfaces.
   * Needs to be called after `LightModule::set_view();`. */
  void set_view(View &view, GPUTexture *depth_tx = nullptr);

  void debug_end_sync();
  void debug_draw(View &view, GPUFrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(SHADOW_ATLAS_TEX_SLOT, &atlas_tx_);
    pass.bind_texture(SHADOW_TILEMAPS_TEX_SLOT, &tilemap_pool.tilemap_tx);
  }

  const ShadowSceneData &get_data()
  {
    return data_;
  }

 private:
  void remove_unused();
  void debug_page_map_call(DRWPass *pass);
  bool shadow_update_finished();

  /** Compute approximate screen pixel space radius. */
  float screen_pixel_radius(const View &view, const int2 &extent);
  /** Compute approximate punctual shadow pixel world space radius, 1 unit away of the light. */
  float tilemap_pixel_radius();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow
 *
 * A shadow component is associated to a `eevee::Light` and manages its associated Tile-maps.
 * \{ */

class ShadowPunctual : public NonCopyable, NonMovable {
 private:
  ShadowModule &shadows_;
  /** Tile-map for each cube-face needed (in eCubeFace order). */
  Vector<ShadowTileMap *> tilemaps_;
  /** Area light size. */
  float size_x_, size_y_;
  /** Shape type. */
  eLightType light_type_;
  /** Light position. */
  float3 position_;
  /** Used to compute near and far clip distances. */
  float max_distance_, light_radius_;
  /** Number of tile-maps needed to cover the light angular extents. */
  int tilemaps_needed_;
  /** Scaling factor to the light shape for shadow ray casting. */
  float softness_factor_;

 public:
  ShadowPunctual(ShadowModule &module) : shadows_(module){};
  ShadowPunctual(ShadowPunctual &&other)
      : shadows_(other.shadows_), tilemaps_(std::move(other.tilemaps_)){};

  ~ShadowPunctual()
  {
    shadows_.tilemap_pool.release(tilemaps_);
  }

  /**
   * Sync shadow parameters but do not allocate any shadow tile-maps.
   */
  void sync(eLightType light_type,
            const float4x4 &object_mat,
            float cone_aperture,
            float light_shape_radius,
            float max_distance,
            float softness_factor);

  /**
   * Release the tile-maps that will not be used in the current frame.
   */
  void release_excess_tilemaps();

  /**
   * Allocate shadow tile-maps and setup views for rendering.
   */
  void end_sync(Light &light, float lod_bias);

 private:
  /**
   * Compute the projection matrix inputs.
   * Make sure that the projection encompass all possible rays that can start in the projection
   * quadrant.
   */
  void compute_projection_boundaries(float light_radius,
                                     float shadow_radius,
                                     float max_lit_distance,
                                     float &near,
                                     float &far,
                                     float &side);
};

class ShadowDirectional : public NonCopyable, NonMovable {
 private:
  ShadowModule &shadows_;
  /** Tile-map for each clip-map level. */
  Vector<ShadowTileMap *> tilemaps_;
  /** User minimum resolution. */
  float min_resolution_;
  /** Copy of object matrix. Normalized. */
  float4x4 object_mat_;
  /** Current range of clip-map / cascades levels covered by this shadow. */
  IndexRange levels_range;
  /** Radius of the shadowed light shape. Might be scaled compared to the shading disk. */
  float disk_shape_angle_;
  /** Maximum distance a shadow map ray can be travel. */
  float trace_distance_;

 public:
  ShadowDirectional(ShadowModule &module) : shadows_(module){};
  ShadowDirectional(ShadowDirectional &&other)
      : shadows_(other.shadows_), tilemaps_(std::move(other.tilemaps_)){};

  ~ShadowDirectional()
  {
    shadows_.tilemap_pool.release(tilemaps_);
  }

  /**
   * Sync shadow parameters but do not allocate any shadow tile-maps.
   */
  void sync(const float4x4 &object_mat,
            float min_resolution,
            float shadow_disk_angle,
            float trace_distance);

  /**
   * Release the tile-maps that will not be used in the current frame.
   */
  void release_excess_tilemaps(const Camera &camera, float lod_bias);

  /**
   * Allocate shadow tile-maps and setup views for rendering.
   */
  void end_sync(Light &light, const Camera &camera, float lod_bias);

  /* Return coverage of the whole tile-map in world unit. */
  static float coverage_get(int lvl)
  {
    /* This function should be kept in sync with shadow_directional_level(). */
    /* \note: If we would to introduce a global scaling option it would be here. */
    return exp2(lvl);
  }

  /* Return coverage of a single tile for a tile-map of this LOD in world unit. */
  static float tile_size_get(int lvl)
  {
    return coverage_get(lvl) / SHADOW_TILEMAP_RES;
  }

 private:
  IndexRange clipmap_level_range(const Camera &camera);
  IndexRange cascade_level_range(const Camera &camera, float lod_bias);

  void cascade_tilemaps_distribution(Light &light, const Camera &camera);
  void clipmap_tilemaps_distribution(Light &light, const Camera &camera, float lod_bias);

  void cascade_tilemaps_distribution_near_far_points(const Camera &camera,
                                                     float3 &near_point,
                                                     float3 &far_point);

  /* Choose between clip-map and cascade distribution of shadow-map precision depending on the
   * camera projection type and bounds. */
  static eShadowProjectionType directional_distribution_type_get(const Camera &camera);
};

/** \} */

}  // namespace blender::eevee
