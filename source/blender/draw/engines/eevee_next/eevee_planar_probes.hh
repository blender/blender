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

/* -------------------------------------------------------------------- */
/** \name Planar Probe
 * \{ */

struct PlanarProbe {
  /* Copy of object matrices. */
  float4x4 plane_to_world;
  float4x4 world_to_plane;
  /* Offset to the clipping plane in the normal direction. */
  float clipping_offset;
  /* Index in the resource array. */
  int resource_index;
  /* Pruning flag. */
  bool is_probe_used = false;
};

struct PlanarProbeResources : NonCopyable {
  Framebuffer combined_fb = {"planar.combined_fb"};
  draw::View view = {"planar.view"};
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Planar Probe Module
 * \{ */

class PlanarProbeModule {
  using PlanarProbes = Map<uint64_t, PlanarProbe>;
  using Resources = Array<PlanarProbeResources>;

 private:
  Instance &instance_;

  PlanarProbes probes_;
  Resources resources_;

  Texture color_tx_ = {"planar.color_tx"};
  Texture depth_tx_ = {"planar.depth_tx"};

  ClipPlaneBuf world_clip_buf_ = {"world_clip_buf"};

  bool update_probes_ = false;

 public:
  PlanarProbeModule(Instance &instance) : instance_(instance) {}

  void init();
  void begin_sync();
  void sync_object(Object *ob, ObjectHandle &ob_handle);
  void end_sync();

  void set_view(const draw::View &main_view, int2 main_view_extent);

  template<typename T> void bind_resources(draw::detail::PassBase<T> * /*pass*/) {}

 private:
  PlanarProbe &find_or_insert(ObjectHandle &ob_handle);
  void remove_unused_probes();

  /**
   * Create the reflection matrix that reflect along the XY plane of the given transform.
   * The transform does not need to be normalized but is expected to be orthogonal.
   */
  float4x4 reflection_matrix_get(const float4x4 &plane_to_world, const float4x4 &world_to_plane);

  /**
   * Create the reflection clip plane equation that clips along the XY plane of the given
   * transform. The `clip_offset` will push the clip plane a bit further to avoid missing pixels in
   * reflections. The transform does not need to be normalized but is expected to be orthogonal.
   */
  float4 reflection_clip_plane_get(const float4x4 &plane_to_world, float clip_offset);

  friend class Instance;
  friend class PlanarProbePipeline;
};

/** \} */

}  // namespace blender::eevee
