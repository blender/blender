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

struct ReflectionProbe : ReflectionProbeData {
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

  ReflectionProbe()
  {
    this->atlas_coord.layer_subdivision = -1;
    this->atlas_coord.layer = -1;
    this->atlas_coord.area_index = -1;
  }

  void recalc_lod_factors(int atlas_resolution)
  {
    const float probe_resolution = atlas_resolution >> atlas_coord.layer_subdivision;
    const float bias = 0.0;
    const float lod_factor = bias + 0.5 * log(float(square_i(probe_resolution))) / log(2.0);
    this->lod_factor = lod_factor;
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

  ReflectionProbe world_probe_data;

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
  int reflection_probe_index_ = 0;
  /** Updated Probe coordinates in the atlas. */
  ReflectionProbeAtlasCoordinate reflection_probe_coord_;
  /** World coordinates in the atlas. */
  ReflectionProbeAtlasCoordinate world_probe_coord_;
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
  void sync_world(::World *world, WorldHandle &ob_handle);
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
