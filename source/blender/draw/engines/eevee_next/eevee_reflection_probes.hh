/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
class CaptureView;

/* -------------------------------------------------------------------- */
/** \name Reflection Probes
 * \{ */

class ReflectionProbeModule {
 private:
  /** The max number of probes to track. */
  static constexpr int max_probes_ = 1;

  /**
   * The maximum resolution of a cubemap side.
   *
   * Must be a power of two; intension to be used as a cubemap atlas.
   */
  static constexpr int max_resolution_ = 2048;

  Instance &instance_;

  /** Texture containing a cubemap used for updating #probes_tx_. */
  Texture cubemap_tx_ = {"Probe.Cubemap"};
  /** Probes texture stored in octahedral mapping. */
  Texture probes_tx_ = {"Probes"};

  PassSimple remap_ps_ = {"Probe.CubemapToOctahedral"};

  bool initialized_ = false;

  bool do_world_update_ = false;

 public:
  ReflectionProbeModule(Instance &instance) : instance_(instance) {}

  void init();

  template<typename T> void bind_resources(draw::detail::PassBase<T> *pass)
  {
    pass->bind_texture(REFLECTION_PROBE_TEX_SLOT, probes_tx_);
  }

  void do_world_update_set(bool value)
  {
    do_world_update_ = value;
  }

 private:
  bool do_world_update_get() const
  {
    return do_world_update_;
  }

  void remap_to_octahedral_projection();

  /* Capture View requires access to the cubemaps texture for framebuffer configuration. */
  friend class CaptureView;
};

}  // namespace blender::eevee
