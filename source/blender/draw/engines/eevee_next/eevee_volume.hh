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

class VolumeModule {
 private:
  Instance &inst_;

  bool enabled_;

  VolumesInfoData &data_;

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

  /* Axis aligned bounding box in the volume grid.
   * Used for frustum culling and volumes overlapping detection. */
  struct GridAABB {
    int3 min, max;

    /* Returns true if visible. */
    bool init(Object *ob, const Camera &camera, const VolumesInfoData &data);

    bool overlaps(const GridAABB &aabb);
  };
  /* Stores a vector of volume AABBs for each material pass,
   * so we can detect overlapping volumes and place GPU barriers where needed
   * (Only stores the AABBs for the volumes rendered since the last barrier). */
  Map<GPUShader *, Vector<GridAABB>> subpass_aabbs_;

 public:
  VolumeModule(Instance &inst, VolumesInfoData &data) : inst_(inst), data_(data)
  {
    dummy_scatter_tx_.ensure_3d(GPU_RGBA8, int3(1), GPU_TEXTURE_USAGE_SHADER_READ, float4(0.0f));
    dummy_transmit_tx_.ensure_3d(GPU_RGBA8, int3(1), GPU_TEXTURE_USAGE_SHADER_READ, float4(1.0f));
  };

  ~VolumeModule(){};

  /* Bind resources needed by external passes to perform their own resolve. */
  template<typename PassType> void bind_resources(PassType &ps)
  {
    ps.bind_texture(VOLUME_SCATTERING_TEX_SLOT, &transparent_pass_scatter_tx_);
    ps.bind_texture(VOLUME_TRANSMITTANCE_TEX_SLOT, &transparent_pass_transmit_tx_);
  }

  /* Bind the common resources needed by all volumetric passes. */
  template<typename PassType> void bind_properties_buffers(PassType &ps)
  {
    ps.bind_image(VOLUME_PROP_SCATTERING_IMG_SLOT, &prop_scattering_tx_);
    ps.bind_image(VOLUME_PROP_EXTINCTION_IMG_SLOT, &prop_extinction_tx_);
    ps.bind_image(VOLUME_PROP_EMISSION_IMG_SLOT, &prop_emission_tx_);
    ps.bind_image(VOLUME_PROP_PHASE_IMG_SLOT, &prop_phase_tx_);
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
  void sync_object(Object *ob,
                   ObjectHandle &ob_handle,
                   ResourceHandle res_handle,
                   MaterialPass *material_pass = nullptr);

  void end_sync();

  /* Render material properties. */
  void draw_prepass(View &view);
  /* Compute scattering and integration. */
  void draw_compute(View &view);
  /* Final image compositing. */
  void draw_resolve(View &view);
};
}  // namespace blender::eevee
