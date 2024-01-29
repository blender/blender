/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "eevee_shader_shared.hh"

#include "BKE_cryptomatte.hh"

extern "C" {
struct Material;
}

namespace blender::eevee {

class Instance;
struct ObjectHandle;
struct WorldHandle;
class CaptureView;
struct ReflectionProbeUpdateInfo;

/* -------------------------------------------------------------------- */
/** \name Reflection Probe
 * \{ */

struct ReflectionProbeAtlasCoordinate {
  /** On which layer of the texture array is this reflection probe stored. */
  int layer = -1;
  /**
   * Subdivision of the layer. 0 = no subdivision and resolution would be
   * ReflectionProbeModule::MAX_RESOLUTION.
   */
  int layer_subdivision = -1;
  /**
   * Which area of the subdivided layer is the reflection probe located.
   *
   * A layer has (2^layer_subdivision)^2 areas.
   */
  int area_index = -1;

  /* Return the area extent in pixel. */
  int area_extent(int atlas_extent) const
  {
    return atlas_extent >> layer_subdivision;
  }

  /* Coordinate of the area in [0..area_count_per_dimension[ range. */
  int2 area_location() const
  {
    const int area_count_per_dimension = 1 << layer_subdivision;
    return int2(area_index % area_count_per_dimension, area_index / area_count_per_dimension);
  }

  /* Coordinate of the bottom left corner of the area in [0..atlas_extent[ range. */
  int2 area_offset(int atlas_extent) const
  {
    return area_location() * area_extent(atlas_extent);
  }

  ReflectionProbeCoordinate as_sampling_coord(int atlas_extent) const
  {
    /**
     * We want to cover the last mip exactly at the pixel center to reduce padding texels and
     * interpolation artifacts.
     * This is a diagram of a 2px^2 map with `c` being the texels corners and `x` the pixels
     * centers.
     *
     * c-------c-------c
     * |       |       |
     * |   x   |   x   | <
     * |       |       |  |
     * c-------c-------c  | sampling area
     * |       |       |  |
     * |   x   |   x   | <
     * |       |       |
     * c-------c-------c
     *     ^-------^
     *       sampling area
     */
    /* First level only need half a pixel of padding around the sampling area. */
    const int mip_max_lvl_padding = 1;
    const int mip_min_lvl_padding = mip_max_lvl_padding << REFLECTION_PROBE_MIPMAP_LEVELS;
    /* Extent and offset in mip 0 texels. */
    const int sampling_area_extent = area_extent(atlas_extent) - mip_min_lvl_padding;
    const int2 sampling_area_offset = area_offset(atlas_extent) + mip_min_lvl_padding / 2;
    /* Convert to atlas UVs. */
    ReflectionProbeCoordinate coord;
    coord.scale = sampling_area_extent / float(atlas_extent);
    coord.offset = float2(sampling_area_offset) / float(atlas_extent);
    coord.layer = layer;
    return coord;
  }

  ReflectionProbeWriteCoordinate as_write_coord(int atlas_extent, int mip_lvl) const
  {
    ReflectionProbeWriteCoordinate coord;
    coord.extent = atlas_extent >> (layer_subdivision + mip_lvl);
    coord.offset = (area_location() * coord.extent) >> mip_lvl;
    coord.layer = layer;
    return coord;
  }
};

struct ReflectionProbe : ReflectionProbeData {
 public:
  enum class Type {
    WORLD,
    PROBE,
  } type;

  /* Used to sort the probes by priority. */
  float volume;

  /* Should the area in the probes_tx_ be updated? */
  bool do_render = false;
  bool do_world_irradiance_update = false;

  /**
   * Probes that aren't used during a draw can be cleared.
   *
   * Only valid when type == Type::Probe.
   */
  bool is_probe_used = false;

  /**
   * Far and near clipping distances for rendering
   */
  float2 clipping_distances;

  /** Display debug spheres in the viewport. */
  bool viewport_display;
  float viewport_display_size;

  ReflectionProbeAtlasCoordinate atlas_coord;

  void prepare_for_upload(int atlas_extent)
  {
    /* Compute LOD factor. */
    const int probe_resolution = atlas_coord.area_extent(atlas_extent);
    const float bias = 0.0;
    const float lod_factor = bias + 0.5 * log2f(square_i(probe_resolution));
    this->lod_factor = lod_factor;

    /* Compute sampling offset and scale. */
    static_cast<ReflectionProbeData *>(this)->atlas_coord = atlas_coord.as_sampling_coord(
        atlas_extent);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reflection Probe Module
 * \{ */

class ReflectionProbeModule {
  using ReflectionProbes = Map<uint64_t, ReflectionProbe>;

 private:
  /**
   * The maximum resolution of a cube-map side.
   *
   * Must be a power of two; intention to be used as a cube-map atlas.
   */
  static constexpr int max_resolution_ = 2048;

  static constexpr uint64_t world_object_key_ = 0;

  bool is_initialized = false;
  Instance &instance_;
  ReflectionProbeDataBuf data_buf_;
  ReflectionProbes probes_;

  /** Probes texture stored in octahedral mapping. */
  Texture probes_tx_ = {"Probes"};

  PassSimple remap_ps_ = {"Probe.CubemapToOctahedral"};
  PassSimple update_irradiance_ps_ = {"Probe.UpdateIrradiance"};
  PassSimple select_ps_ = {"Probe.Select"};

  int3 dispatch_probe_pack_ = int3(1);
  int3 dispatch_probe_select_ = int3(1);

  /**
   * Texture containing a cube-map where the probe should be rendering to.
   *
   * NOTE: TextureFromPool doesn't support cube-maps.
   */
  Texture cubemap_tx_ = {"Probe.Cubemap"};
  /** Index of the probe being updated. */
  int probe_index_ = 0;
  /** Mip level being sampled for remapping. */
  int probe_mip_level_ = 0;
  /** Updated Probe coordinates in the atlas. */
  ReflectionProbeCoordinate probe_sampling_coord_;
  ReflectionProbeWriteCoordinate probe_write_coord_;
  /** World coordinates in the atlas. */
  ReflectionProbeCoordinate world_sampling_coord_;
  /** Number of the probe to process in the select phase. */
  int reflection_probe_count_ = 0;

  bool update_probes_next_sample_ = false;
  bool update_probes_this_sample_ = false;

  /** Viewport data display drawing. */
  bool do_display_draw_ = false;
  ReflectionProbeDisplayDataBuf display_data_buf_;
  PassSimple viewport_display_ps_ = {"ReflectionProbeModule.Viewport Display"};

 public:
  ReflectionProbeModule(Instance &instance) : instance_(instance) {}

  void init();
  void begin_sync();
  void sync_world(::World *world);
  void sync_world_lookdev();
  void sync_object(Object *ob, ObjectHandle &ob_handle);
  void end_sync();

  void viewport_draw(View &view, GPUFrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(REFLECTION_PROBE_TEX_SLOT, &probes_tx_);
    pass.bind_ubo(REFLECTION_PROBE_BUF_SLOT, &data_buf_);
  }

  bool do_world_update_get() const;
  void do_world_update_set(bool value);
  void do_world_update_irradiance_set(bool value);

  void set_view(View &view);

  void debug_print() const;

  int atlas_extent() const
  {
    return probes_tx_.width();
  }

  /**
   * Get the resolution of a single cube-map side when rendering probes.
   *
   * The cube-maps are rendered half size of the size of the octahedral texture.
   */
  int probe_render_extent() const;

  ReflectionProbeAtlasCoordinate world_atlas_coord_get() const;

 private:
  /** Get the number of layers that is needed to store probes. */
  int needed_layers_get() const;

  bool remove_unused_probes();

  /**
   * Create a reflection probe data element that points to an empty spot in the cubemap that can
   * hold a texture with the given subdivision_level.
   */
  ReflectionProbeAtlasCoordinate find_empty_atlas_region(int subdivision_level) const;

  /**
   * Pop the next reflection probe that requires to be updated.
   */
  std::optional<ReflectionProbeUpdateInfo> update_info_pop(ReflectionProbe::Type probe_type);

  void remap_to_octahedral_projection(const ReflectionProbeAtlasCoordinate &atlas_coord);
  void update_probes_texture_mipmaps();
  void update_world_irradiance();

  bool has_only_world_probe() const;

  eLightProbeResolution reflection_probe_resolution() const;

  /* Capture View requires access to the cube-maps texture for frame-buffer configuration. */
  friend class CaptureView;
  /* Instance requires access to #update_probes_this_sample_ */
  friend class Instance;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reflection Probe Update Info
 * \{ */

struct ReflectionProbeUpdateInfo {
  float3 probe_pos;
  ReflectionProbe::Type probe_type;
  /**
   * Resolution of the cubemap to be rendered.
   */
  int resolution;

  float2 clipping_distances;

  ReflectionProbeAtlasCoordinate atlas_coord;

  bool do_render;
  bool do_world_irradiance_update;
};

/** \} */

}  // namespace blender::eevee
