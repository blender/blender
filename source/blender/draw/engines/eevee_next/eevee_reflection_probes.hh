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
struct ObjectHandle;
struct WorldHandle;
class CaptureView;

/* -------------------------------------------------------------------- */
/** \name Reflection Probes
 * \{ */

struct ReflectionProbe {
  enum Type { Unused, World, Probe };

  Type type = Type::Unused;

  /* Probe data needs to be updated. */
  bool do_update_data = false;
  /* Should the area in the probes_tx_ be updated? */
  bool do_render = false;

  /**
   * Probes that aren't used during a draw can be cleared.
   *
   * Only valid when type == Type::Probe.
   */
  bool is_probe_used = false;

  /**
   * Index into ReflectionProbeDataBuf.
   * -1 = not added yet
   */
  int index = -1;

  /**
   * Check if the probe needs to be updated during this sample.
   */
  bool needs_update() const
  {
    switch (type) {
      case Type::Unused:
        return false;
      case Type::World:
        return do_update_data || do_render;
      case Type::Probe:
        return (do_update_data || do_render) && is_probe_used;
    }
    return false;
  }
};

class ReflectionProbeModule {
 private:
  /** The max number of probes to initially allocate. */
  static constexpr int init_num_probes_ = 1;

  /**
   * The maximum resolution of a cube-map side.
   *
   * Must be a power of two; intention to be used as a cube-map atlas.
   */
  static constexpr int max_resolution_ = 2048;

  static constexpr uint64_t world_object_key_ = 0;

  Instance &instance_;
  ReflectionProbeDataBuf data_buf_;
  Map<uint64_t, ReflectionProbe> probes_;

  /** Texture containing a cubemap used as input for updating #probes_tx_. */
  Texture cubemap_tx_ = {"Probe.Cubemap"};
  /** Probes texture stored in octahedral mapping. */
  Texture probes_tx_ = {"Probes"};

  PassSimple remap_ps_ = {"Probe.CubemapToOctahedral"};

  bool do_world_update_ = false;

  int3 dispatch_probe_pack_ = int3(0);

 public:
  ReflectionProbeModule(Instance &instance) : instance_(instance) {}

  void init();
  void begin_sync();
  void sync_world(::World *world, WorldHandle &ob_handle);
  void sync_object(Object *ob, ObjectHandle &ob_handle);
  void end_sync();

  template<typename T> void bind_resources(draw::detail::PassBase<T> *pass)
  {
    pass->bind_texture(REFLECTION_PROBE_TEX_SLOT, probes_tx_);
    pass->bind_ssbo(REFLECTION_PROBE_BUF_SLOT, data_buf_);
  }

  void do_world_update_set(bool value)
  {
    do_world_update_ = value;
  }

  void debug_print() const;

 private:
  void sync(const ReflectionProbe &cubemap);
  ReflectionProbe &find_or_insert(ObjectHandle &ob_handle, int subdivision_level);

  /** Get the number of layers that is needed to store probes. */
  int needed_layers_get() const;

  void remove_unused_probes();
  void recalc_lod_factors();

  /* TODO: also add _len() which is a max + 1. */
  /* Get the number of reflection probe data elements. */
  int reflection_probe_data_index_max() const;

  /**
   * Remove reflection probe data from the module.
   * Ensures that data_buf is sequential and cube-maps are relinked to its corresponding data.
   */
  void remove_reflection_probe_data(int reflection_probe_data_index);

  /**
   * Create a reflection probe data element that points to an empty spot in the cubemap that can
   * hold a texture with the given subdivision_level.
   */
  ReflectionProbeData find_empty_reflection_probe_data(int subdivision_level) const;

  void upload_dummy_texture(const ReflectionProbe &probe);

  bool do_world_update_get() const
  {
    return do_world_update_;
  }

  void remap_to_octahedral_projection();

  /* Capture View requires access to the cube-maps texture for frame-buffer configuration. */
  friend class CaptureView;
};

std::ostream &operator<<(std::ostream &os, const ReflectionProbeModule &module);
std::ostream &operator<<(std::ostream &os, const ReflectionProbeData &probe_data);
std::ostream &operator<<(std::ostream &os, const ReflectionProbe &probe);

}  // namespace blender::eevee
