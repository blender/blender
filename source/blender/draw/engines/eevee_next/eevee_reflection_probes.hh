/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#pragma once

#include "eevee_lightprobe.hh"
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

/* -------------------------------------------------------------------- */
/** \name Reflection Probe Module
 * \{ */

class SphereProbeModule {
  friend LightProbeModule;
  /* Capture View requires access to the cube-maps texture for frame-buffer configuration. */
  friend class CaptureView;
  /* Instance requires access to #update_probes_this_sample_ */
  friend class Instance;

 private:
  /**
   * The maximum resolution of a cube-map side.
   *
   * Must be a power of two; intention to be used as a cube-map atlas.
   */
  static constexpr int max_resolution_ = 2048;

  Instance &instance_;
  SphereProbeDataBuf data_buf_;

  /** Probes texture stored in octahedral mapping. */
  Texture probes_tx_ = {"Probes"};

  /** Copy the rendered cube-map to the atlas texture. */
  PassSimple remap_ps_ = {"Probe.CubemapToOctahedral"};
  /** Extract irradiance information from the world. */
  PassSimple update_irradiance_ps_ = {"Probe.UpdateIrradiance"};
  /** Copy volume probe irradiance for the center of sphere probes. */
  PassSimple select_ps_ = {"Probe.Select"};
  /** Convolve the octahedral map to fill the Mip-map levels. */
  PassSimple convolve_ps_ = {"Probe.Convolve"};
  /** Input mip level for the convolution. */
  GPUTexture *convolve_input_ = nullptr;
  /** Output mip level for the convolution. */
  GPUTexture *convolve_output_ = nullptr;

  int3 dispatch_probe_pack_ = int3(1);
  int3 dispatch_probe_convolve_ = int3(1);
  int3 dispatch_probe_select_ = int3(1);

  /**
   * Texture containing a cube-map where the probe should be rendering to.
   *
   * NOTE: TextureFromPool doesn't support cube-maps.
   */
  Texture cubemap_tx_ = {"Probe.Cubemap"};
  /** Index of the probe being updated. */
  int probe_index_ = 0;
  /** Updated Probe coordinates in the atlas. */
  SphereProbeUvArea probe_sampling_coord_;
  SphereProbePixelArea probe_write_coord_;
  /** World coordinates in the atlas. */
  SphereProbeUvArea world_sampling_coord_;
  /** Number of the probe to process in the select phase. */
  int reflection_probe_count_ = 0;

  /**
   * True if the next redraw will trigger a light-probe sphere update.
   * As syncing the draw passes for rendering has a significant overhead,
   * we only trigger this sync path if we detect updates. But we only know
   * this after `end_sync` which is too late to sync objects for light-probe
   * rendering. So we tag the next redraw (or sample) to do the sync.
   */
  bool update_probes_next_sample_ = false;
  /** True if the this redraw will trigger a light-probe sphere update. */
  bool update_probes_this_sample_ = false;
  /** Compute world irradiance coefficient and store them into the volume probe atlas. */
  bool do_world_irradiance_update = true;

  /** Viewport data display drawing. */
  bool do_display_draw_ = false;
  SphereProbeDisplayDataBuf display_data_buf_;
  PassSimple viewport_display_ps_ = {"ProbeSphereModule.Viewport Display"};

 public:
  SphereProbeModule(Instance &instance) : instance_(instance){};

  void init();
  void begin_sync();
  void end_sync();

  void viewport_draw(View &view, GPUFrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(SPHERE_PROBE_TEX_SLOT, &probes_tx_);
    pass.bind_ubo(SPHERE_PROBE_BUF_SLOT, &data_buf_);
  }

  void set_view(View &view);

  /**
   * Get the resolution of a single cube-map side when rendering probes.
   *
   * The cube-maps are rendered half size of the size of the octahedral texture.
   */
  int probe_render_extent() const;

  void tag_world_irradiance_for_update()
  {
    do_world_irradiance_update = true;
  }

 private:
  /* Return the subdivision level for the requested probe resolution.
   * Result is safely clamped to max resolution. */
  int subdivision_level_get(const eLightProbeResolution probe_resolution)
  {
    return max_ii(int(log2(max_resolution_)) - int(probe_resolution), 0);
  }

  /**
   * Ensure atlas texture is the right size.
   * Returns true if the texture has been cleared and all probes needs to be rendered again.
   */
  bool ensure_atlas();

  /**
   * Ensure the cube-map target texture for rendering the probe is allocated.
   */
  void ensure_cubemap_render_target(int resolution);

  struct UpdateInfo {
    float3 probe_pos;
    /** Resolution of the cube-map to be rendered. */
    int resolution;

    float2 clipping_distances;

    SphereProbeAtlasCoord atlas_coord;

    bool do_render;
    bool do_world_irradiance_update;
  };

  UpdateInfo update_info_from_probe(const SphereProbe &probe);

  /**
   * Pop the next reflection probe that requires to be updated.
   */
  std::optional<UpdateInfo> world_update_info_pop();
  std::optional<UpdateInfo> probe_update_info_pop();

  /**
   * Internal processing passes.
   */
  void remap_to_octahedral_projection(const SphereProbeAtlasCoord &atlas_coord);
  void update_world_irradiance();

  void sync_display(Vector<SphereProbe *> &probe_active);
};

/** \} */

}  // namespace blender::eevee
