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
class HiZBuffer;
struct ObjectHandle;

/* -------------------------------------------------------------------- */
/** \name Planar Probe
 * \{ */

struct PlanarProbe : ProbePlanarData {
  /* Copy of object matrices. */
  float4x4 plane_to_world;
  float4x4 world_to_plane;
  /* Offset to the clipping plane in the normal direction. */
  float clipping_offset;
  /* Index in the resource array. */
  int resource_index;
  /* Pruning flag. */
  bool is_probe_used = false;
  /** Display a debug plane in the viewport. */
  bool viewport_display = false;

 public:
  void sync(const float4x4 &world_to_object,
            float clipping_offset,
            float influence_distance,
            bool viewport_display);

  /**
   * Update the ProbePlanarData part of the struct.
   * `view` is the view we want to render this probe with.
   */
  void set_view(const draw::View &view, int layer_id);

  /**
   * Create the reflection clip plane equation that clips along the XY plane of the given
   * transform. The `clip_offset` will push the clip plane a bit further to avoid missing pixels in
   * reflections. The transform does not need to be normalized but is expected to be orthogonal.
   * \note Only works after `set_view` was called.
   */
  float4 reflection_clip_plane_get();

 private:
  /**
   * Create the reflection matrix that reflect along the XY plane of the given transform.
   * The transform does not need to be normalized but is expected to be orthogonal.
   */
  float4x4 reflection_matrix_get();
};

struct PlanarProbeResources : NonCopyable {
  Framebuffer combined_fb = {"planar.combined_fb"};
  Framebuffer gbuffer_fb = {"planar.gbuffer_fb"};
  draw::View view = {"planar.view"};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planar Probe Module
 * \{ */

class PlanarProbeModule {
  using PlanarProbes = Map<uint64_t, PlanarProbe>;

 private:
  Instance &instance_;

  PlanarProbes probes_;
  std::array<PlanarProbeResources, PLANAR_PROBES_MAX> resources_;

  Texture radiance_tx_ = {"planar.radiance_tx"};
  Texture depth_tx_ = {"planar.depth_tx"};

  ClipPlaneBuf world_clip_buf_ = {"world_clip_buf"};
  ProbePlanarDataBuf probe_planar_buf_ = {"probe_planar_buf"};

  bool update_probes_ = false;

  /** Viewport data display drawing. */
  bool do_display_draw_ = false;
  ProbePlanarDisplayDataBuf display_data_buf_;
  PassSimple viewport_display_ps_ = {"PlanarProbeModule.Viewport Display"};

 public:
  PlanarProbeModule(Instance &instance) : instance_(instance) {}

  void init();
  void begin_sync();
  void sync_object(Object *ob, ObjectHandle &ob_handle);
  void end_sync();

  void set_view(const draw::View &main_view, int2 main_view_extent);

  void viewport_draw(View &view, GPUFrameBuffer *view_fb);

  template<typename PassType> void bind_resources(PassType &pass)
  {
    /* Disable filter to avoid interpolation with missing background. */
    GPUSamplerState no_filter = GPUSamplerState::default_sampler();
    pass.bind_ubo(PLANAR_PROBE_BUF_SLOT, &probe_planar_buf_);
    pass.bind_texture(PLANAR_PROBE_RADIANCE_TEX_SLOT, &radiance_tx_, no_filter);
    pass.bind_texture(PLANAR_PROBE_DEPTH_TEX_SLOT, &depth_tx_);
  }

  bool enabled() const
  {
    return update_probes_;
  }

 private:
  PlanarProbe &find_or_insert(ObjectHandle &ob_handle);

  friend class Instance;
  friend class HiZBuffer;
  friend class PlanarProbePipeline;
};

/** \} */

}  // namespace blender::eevee
