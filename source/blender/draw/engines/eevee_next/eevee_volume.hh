/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Volumetric effects rendering using Frostbite's Physically-based & Unified Volumetric Rendering
 * approach.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
 *
 * The rendering is separated in 4 stages:
 *
 * - Material Parameters : we collect volume properties of
 *   all participating media in the scene and store them in
 *   a 3D texture aligned with the 3D frustum.
 *   This is done in 2 passes, one that clear the texture
 *   and/or evaluate the world volumes, and the 2nd one that
 *   additively render object volumes.
 *
 * - Light Scattering : the volume properties then are sampled
 *   and light scattering is evaluated for each froxel of the
 *   volume texture. Temporal super-sampling (if enabled) occurs here.
 *
 * - Volume Integration : the scattered light and extinction is
 *   integrated (accumulated) along the view-rays. The result is stored
 *   for every froxel in another texture.
 *
 * - Full-screen Resolve : From the previous stage, we get two
 *   3D textures that contains integrated scattered light and extinction
 *   for "every" positions in the frustum. We only need to sample
 *   them and blend the scene color with those factors. This also
 *   work for alpha blended materials.
 */

#pragma once

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;
class VolumePipeline;

class VolumeModule {
  friend VolumePipeline;

 private:
  Instance &inst_;

  bool enabled_;

  VolumesInfoData &data_;

  /**
   * Occupancy map that allows to fill froxels that are inside the geometry.
   * It is filled during a pre-pass using atomic operations.
   * Using a 3D bit-field, we only allocate one bit per froxel.
   */
  Texture occupancy_tx_ = {"occupancy_tx"};
  /**
   * List of surface hit for correct occupancy determination.
   * One texture holds the number of hit count and the other the depth and
   * the facing of each hit.
   */
  Texture hit_count_tx_ = {"hit_count_tx"};
  Texture hit_depth_tx_ = {"hit_depth_tx"};
  /** Empty frame-buffer for occupancy pass. */
  Framebuffer occupancy_fb_ = {"occupancy_fb"};

  /* Material Parameters */
  Texture prop_scattering_tx_;
  Texture prop_extinction_tx_;
  Texture prop_emission_tx_;
  Texture prop_phase_tx_;

  /* Light Scattering. */
  PassSimple scatter_ps_ = {"Volumes.Scatter"};
  Texture scatter_tx_;
  Texture extinction_tx_;

  /* Volume Integration */
  PassSimple integration_ps_ = {"Volumes.Integration"};
  Texture integrated_scatter_tx_;
  Texture integrated_transmit_tx_;

  /* Full-screen Resolve */
  PassSimple resolve_ps_ = {"Volumes.Resolve"};
  Framebuffer resolve_fb_;
  /* Used in the forward transparent pass (ForwardPipeline).
   * The forward transparent pass must perform its own resolve step for correct composition between
   * volumes and transparent surfaces. */
  GPUTexture *transparent_pass_scatter_tx_;
  GPUTexture *transparent_pass_transmit_tx_;
  Texture dummy_scatter_tx_;
  Texture dummy_transmit_tx_;

 public:
  VolumeModule(Instance &inst, VolumesInfoData &data) : inst_(inst), data_(data)
  {
    dummy_scatter_tx_.ensure_3d(GPU_RGBA8, int3(1), GPU_TEXTURE_USAGE_SHADER_READ, float4(0.0f));
    dummy_transmit_tx_.ensure_3d(GPU_RGBA8, int3(1), GPU_TEXTURE_USAGE_SHADER_READ, float4(1.0f));
  };

  ~VolumeModule(){};

  /* Bind resources needed by external passes to perform their own resolve. */
  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture(VOLUME_SCATTERING_TEX_SLOT, &transparent_pass_scatter_tx_);
    pass.bind_texture(VOLUME_TRANSMITTANCE_TEX_SLOT, &transparent_pass_transmit_tx_);
  }

  /* Bind the common resources needed by all volumetric passes. */
  template<typename PassType> void bind_properties_buffers(PassType &pass)
  {
    pass.bind_image(VOLUME_PROP_SCATTERING_IMG_SLOT, &prop_scattering_tx_);
    pass.bind_image(VOLUME_PROP_EXTINCTION_IMG_SLOT, &prop_extinction_tx_);
    pass.bind_image(VOLUME_PROP_EMISSION_IMG_SLOT, &prop_emission_tx_);
    pass.bind_image(VOLUME_PROP_PHASE_IMG_SLOT, &prop_phase_tx_);
    pass.bind_image(VOLUME_OCCUPANCY_SLOT, &occupancy_tx_);
  }

  template<typename PassType> void bind_occupancy_buffers(PassType &pass)
  {
    pass.bind_image(VOLUME_OCCUPANCY_SLOT, &occupancy_tx_);
    pass.bind_image(VOLUME_HIT_DEPTH_SLOT, &hit_depth_tx_);
    pass.bind_image(VOLUME_HIT_COUNT_SLOT, &hit_count_tx_);
  }

  bool needs_shadow_tagging()
  {
    return enabled_ && data_.use_lights;
  }

  int3 grid_size()
  {
    return data_.tex_size;
  }

  void init();

  void begin_sync();

  void sync_world();

  void material_call(MaterialPass &material_pass, Object *ob, ResourceHandle res_handle);

  void end_sync();

  /* Render material properties. */
  void draw_prepass(View &view);
  /* Compute scattering and integration. */
  void draw_compute(View &view);
  /* Final image compositing. */
  void draw_resolve(View &view);
};
}  // namespace blender::eevee
