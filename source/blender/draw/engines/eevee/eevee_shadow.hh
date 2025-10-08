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

#include "GPU_batch.hh"

#include "eevee_camera.hh"
#include "eevee_material.hh"
#include "eevee_shadow_shared.hh"
#include "eevee_sync.hh"
#include "eevee_uniform_shared.hh"

namespace blender::eevee {

class Instance;
class ShadowModule;
class ShadowPipeline;
struct Light;

/* To be applied after view matrix. Follow same order as eCubeFace. */
constexpr static const float shadow_face_mat[6][3][3] = {
    {{+1, +0, +0}, {+0, +1, +0}, {+0, +0, +1}}, /* Z_NEG */
    {{+0, +0, -1}, {-1, +0, +0}, {+0, +1, +0}}, /* X_POS */
    {{+0, +0, +1}, {+1, +0, +0}, {+0, +1, +0}}, /* X_NEG */
    {{+1, +0, +0}, {+0, +0, -1}, {+0, +1, +0}}, /* Y_POS */
    {{-1, +0, +0}, {+0, +0, +1}, {+0, +1, +0}}, /* Y_NEG */
    {{+1, +0, +0}, {+0, -1, +0}, {+0, +0, -1}}, /* Z_POS */
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

using ShadowStatisticsBuf = draw::StorageBuffer<ShadowStatistics>;
using ShadowPagesInfoDataBuf = draw::StorageBuffer<ShadowPagesInfoData>;
using ShadowPageHeapBuf = draw::StorageVectorBuffer<uint, SHADOW_MAX_PAGE>;
using ShadowPageCacheBuf = draw::StorageArrayBuffer<uint2, SHADOW_MAX_PAGE, true>;
using ShadowTileMapDataBuf = draw::StorageVectorBuffer<ShadowTileMapData, SHADOW_MAX_TILEMAP>;
using ShadowTileMapClipBuf = draw::StorageArrayBuffer<ShadowTileMapClip, SHADOW_MAX_TILEMAP, true>;
using ShadowTileDataBuf = draw::StorageArrayBuffer<ShadowTileDataPacked, SHADOW_MAX_TILE, true>;
using ShadowRenderViewBuf = draw::StorageArrayBuffer<ShadowRenderView, SHADOW_VIEW_MAX, true>;

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
  float4x4 object_mat = float4x4::identity();

 public:
  ShadowTileMap(int tiles_index_) : ShadowTileMapData{}
  {
    tiles_index = tiles_index_;
    /* For now just the same index. */
    clip_data_index = tiles_index_ / SHADOW_TILEDATA_PER_TILEMAP;
    /* Avoid uninitialized data. */
    this->grid_offset = int2(0);
    this->grid_shift = int2(0);
    this->set_dirty();
  }

  void sync_orthographic(const float4x4 &object_mat_,
                         int2 origin_offset,
                         int clipmap_level,
                         eShadowProjectionType projection_type_,
                         uint2 shadow_set_membership_ = ~uint2(0));

  void sync_cubeface(eLightType light_type_,
                     const float4x4 &object_mat,
                     float near,
                     float far,
                     eCubeFace face,
                     uint2 shadow_set_membership_ = ~uint2(0));

  void debug_draw() const;

  void set_dirty()
  {
    is_dirty = true;
  }

  void set_updated()
  {
    is_dirty = false;
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
  ResourceHandleRange resource_handle = {};
  bool used = true;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShadowModule
 *
 * Manages shadow atlas and shadow region data.
 * \{ */

class ShadowPunctual;
class ShadowDirectional;

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

  /* Used to call caster_update_ps_ only once per sync (Initialized on begin_sync). */
  bool update_casters_ = false;

  /* -------------------------------------------------------------------- */
  /** \name Tile-map Management
   * \{ */

  PassSimple tilemap_setup_ps_ = {"TilemapSetup"};
  PassMain tilemap_usage_ps_ = {"TagUsage"};
  PassSimple tilemap_update_ps_ = {"TilemapUpdate"};

  PassMain::Sub *tilemap_usage_transparent_ps_ = nullptr;
  gpu::Batch *box_batch_ = nullptr;
  /* Source texture for depth buffer analysis. */
  gpu::Texture *src_depth_tx_ = nullptr;

  Framebuffer usage_tag_fb;

  PassSimple caster_update_ps_ = {"CasterUpdate"};
  PassSimple jittered_transparent_caster_update_ps_ = {"TransparentCasterUpdate"};
  /** List of Resource IDs (to get bounds) for tagging passes. */
  StorageVectorBuffer<uint, 128> past_casters_updated_ = {"PastCastersUpdated"};
  StorageVectorBuffer<uint, 128> curr_casters_updated_ = {"CurrCastersUpdated"};
  StorageVectorBuffer<uint, 128> jittered_transparent_casters_ = {"JitteredTransparentCasters"};
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
  /** View to viewport index mapping and other render-only related data. */
  ShadowRenderViewBuf render_view_buf_ = {"render_view_buf"};

  int3 dispatch_depth_scan_size_;
  int2 usage_tag_fb_resolution_;
  int usage_tag_fb_lod_ = 5;
  int max_view_per_tilemap_ = 1;
  int2 input_depth_extent_;

  /* Statistics that are read back to CPU after a few frame (to avoid stall). */
  SwapChain<ShadowStatisticsBuf, 5> statistics_buf_;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Page Management
   * \{ */

  static constexpr gpu::TextureFormat atlas_type = gpu::TextureFormat::UINT_32;
  /** Atlas containing all physical pages. */
  Texture atlas_tx_ = {"shadow_atlas_tx_"};

  /** Pool of unallocated pages waiting to be assigned to specific tiles in the tile-map atlas. */
  ShadowPageHeapBuf pages_free_data_ = {"PagesFreeBuf"};
  /** Pool of cached tiles waiting to be reused. */
  ShadowPageCacheBuf pages_cached_data_ = {"PagesCachedBuf"};
  /** Information for book keeping and debug. */
  ShadowPagesInfoDataBuf pages_infos_data_ = {"PagesInfosBuf"};

  int3 copy_dispatch_size_;
  int3 scan_dispatch_size_;
  int rendering_tilemap_;
  int rendering_lod_;
  bool do_full_update_ = true;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Rendering
   * \{ */

  class ShadowView : public View {
    Instance &inst_;
    ShadowRenderViewBuf &render_view_buf_;

   public:
    ShadowView(const char *name, Instance &inst, ShadowRenderViewBuf &render_view_buf)
        : View(name, SHADOW_VIEW_MAX, true), inst_(inst), render_view_buf_(render_view_buf)
    {
    }

   protected:
    /** Special culling pass to take shadow linking into consideration. */
    virtual void compute_visibility(ObjectBoundsBuf &bounds,
                                    ObjectInfosBuf &infos,
                                    uint resource_len,
                                    bool debug_freeze) override;
  };

  /** Multi-View containing a maximum of 64 view to be rendered with the shadow pipeline. */
  ShadowView shadow_multi_view_ = {"ShadowMultiView", inst_, render_view_buf_};
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

  /* Render setting that reduces the LOD for every light. */
  float global_lod_bias_ = 0.0f;
  /** For now, needs to be hardcoded. */
  int shadow_page_size_ = SHADOW_PAGE_RES;
  /** Maximum number of allocated pages. Maximum value is SHADOW_MAX_TILEMAP. */
  int shadow_page_len_ = SHADOW_MAX_TILEMAP;
  /** Global switch. */
  bool enabled_ = true;

 public:
  ShadowModule(Instance &inst, ShadowSceneData &data);

  ~ShadowModule()
  {
    GPU_BATCH_DISCARD_SAFE(box_batch_);
  }

  void init();

  void begin_sync();
  /** Register a shadow caster or receiver. */
  void sync_object(const Object *ob,
                   const ObjectHandle &handle,
                   const ResourceHandleRange &resource_handle,
                   bool is_alpha_blend,
                   bool has_transparent_shadows);
  void end_sync();

  void set_lights_data();

  /* Update all shadow regions visible inside the view.
   * If called multiple time for the same view, it will only do the depth buffer scanning
   * to check any new opaque surfaces.
   * Expect the HiZ buffer to be up to date.
   * Needs to be called after `LightModule::set_view();`. */
  void set_view(View &view, int2 extent);

  void debug_end_sync();
  void debug_draw(View &view, gpu::FrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(SHADOW_ATLAS_TEX_SLOT, &atlas_tx_);
    pass.bind_texture(SHADOW_TILEMAPS_TEX_SLOT, &tilemap_pool.tilemap_tx);
  }

  const ShadowSceneData &get_data()
  {
    return data_;
  }

  float global_lod_bias() const
  {
    return global_lod_bias_;
  }

  /* Set all shadows to update. To be called before `end_sync`. */
  void reset()
  {
    do_full_update_ = true;
  }

  /** Compute approximate screen pixel space radius (as world space radius). */
  static float screen_pixel_radius(const float4x4 &wininv,
                                   bool is_perspective,
                                   const int2 &extent);

 private:
  void remove_unused();
  bool shadow_update_finished(int loop_count);

  /** Compute approximate punctual shadow pixel world space radius, 1 unit away of the light. */
  float tilemap_pixel_radius();

  /* Returns the maximum number of view per shadow projection for a single update loop. */
  int max_view_per_tilemap();
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

 public:
  ShadowPunctual(ShadowModule &module) : shadows_(module) {};
  ShadowPunctual(ShadowPunctual &&other)
      : shadows_(other.shadows_), tilemaps_(std::move(other.tilemaps_)) {};

  ~ShadowPunctual()
  {
    shadows_.tilemap_pool.release(tilemaps_);
  }

  /**
   * Release the tile-maps that will not be used in the current frame.
   */
  void release_excess_tilemaps(const Light &light);

  /**
   * Allocate shadow tile-maps and setup views for rendering.
   */
  void end_sync(Light &light);
};

class ShadowDirectional : public NonCopyable, NonMovable {
 private:
  ShadowModule &shadows_;
  /** Tile-map for each clip-map level. */
  Vector<ShadowTileMap *> tilemaps_;
  /** Current range of clip-map / cascades levels covered by this shadow. */
  IndexRange levels_range = IndexRange(0);

 public:
  ShadowDirectional(ShadowModule &module) : shadows_(module) {};
  ShadowDirectional(ShadowDirectional &&other)
      : shadows_(other.shadows_), tilemaps_(std::move(other.tilemaps_)) {};

  ~ShadowDirectional()
  {
    shadows_.tilemap_pool.release(tilemaps_);
  }

  /**
   * Release the tile-maps that will not be used in the current frame.
   */
  void release_excess_tilemaps(const Light &light, const Camera &camera);

  /**
   * Allocate shadow tile-maps and setup views for rendering.
   */
  void end_sync(Light &light, const Camera &camera);

  /* Return coverage of the whole tile-map in world unit. */
  static float coverage_get(int lvl)
  {
    /* This function should be kept in sync with shadow_directional_level(). */
    /* \note If we would to introduce a global scaling option it would be here. */
    return exp2(lvl);
  }

  /* Return coverage of a single tile for a tile-map of this LOD in world unit. */
  static float tile_size_get(int lvl)
  {
    return coverage_get(lvl) / SHADOW_TILEMAP_RES;
  }

 private:
  IndexRange clipmap_level_range(const Camera &camera);
  IndexRange cascade_level_range(const Light &light, const Camera &camera);

  /**
   * Distribute tile-maps in a linear pattern along camera forward vector instead of a clipmap
   * centered on camera position.
   */
  void cascade_tilemaps_distribution(Light &light, const Camera &camera);
  void clipmap_tilemaps_distribution(Light &light, const Camera &camera);

  void cascade_tilemaps_distribution_near_far_points(const Camera &camera,
                                                     const Light &light,
                                                     float3 &near_point,
                                                     float3 &far_point);

  /* Choose between clip-map and cascade distribution of shadow-map precision depending on the
   * camera projection type and bounds. */
  static eShadowProjectionType directional_distribution_type_get(const Camera &camera);
};

/** \} */

}  // namespace blender::eevee
