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
/** \name Planar Probe Module
 * \{ */

class PlanarProbeModule {
  friend class Instance;
  friend class HiZBuffer;
  friend class PlanarProbePipeline;

 private:
  Instance &inst_;

  struct PlanarResources : NonCopyable {
    Framebuffer combined_fb = {"planar.combined_fb"};
    Framebuffer gbuffer_fb = {"planar.gbuffer_fb"};
    draw::View view = {"planar.view"};
  };

  std::array<PlanarResources, PLANAR_PROBE_MAX> resources_;

  Texture radiance_tx_ = {"planar.radiance_tx"};
  Texture depth_tx_ = {"planar.depth_tx"};

  ClipPlaneBuf world_clip_buf_ = {"world_clip_buf"};
  PlanarProbeDataBuf probe_planar_buf_ = {"probe_planar_buf"};

  bool update_probes_ = false;

  /** Viewport data display drawing. */
  bool do_display_draw_ = false;
  PlanarProbeDisplayDataBuf display_data_buf_;
  PassSimple viewport_display_ps_ = {"PlanarProbeModule.Viewport Display"};

 public:
  PlanarProbeModule(Instance &instance) : inst_(instance) {}

  void init();
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
};

/** \} */

}  // namespace blender::eevee
