/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup draw
 *
 * View description and states.
 *
 * A `draw::View` object is required for drawing geometry using the DRW api and its internal
 * culling system.
 *
 * One `View` object can actually contain multiple view matrices if the template parameter
 * `view_len` is greater than 1. This is called multi-view rendering and the vertex shader must
 * setting `drw_view_id` accordingly.
 */

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_shader_shared.h"

namespace blender::draw {

class Manager;

/* TODO: de-duplicate. */
using ObjectBoundsBuf = StorageArrayBuffer<ObjectBounds, 128>;
using VisibilityBuf = StorageArrayBuffer<uint, 4, true>;

class View {
  friend Manager;

 protected:
  /** TODO(fclem): Maybe try to reduce the minimum cost if the number of view is lower. */

  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> data_;
  UniformArrayBuffer<ViewCullingData, DRW_VIEW_MAX> culling_;
  /** Frozen version of data_ used for debugging culling. */
  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> data_freeze_;
  UniformArrayBuffer<ViewCullingData, DRW_VIEW_MAX> culling_freeze_;
  /** Result of the visibility computation. 1 bit or 1 or 2 word per resource ID per view. */
  VisibilityBuf visibility_buf_;

  const char *debug_name_;

  int view_len_ = 0;

  bool is_inverted_ = false;
  bool do_visibility_ = true;
  bool dirty_ = true;
  bool frozen_ = false;
  bool procedural_ = false;

 public:
  View(const char *name, int view_len = 1, bool procedural = false)
      : visibility_buf_(name), debug_name_(name), view_len_(view_len), procedural_(procedural)
  {
    BLI_assert(view_len <= DRW_VIEW_MAX);
  }

  /* For compatibility with old system. Will be removed at some point. */
  View(const char *name, const DRWView *view)
      : visibility_buf_(name), debug_name_(name), view_len_(1)
  {
    this->sync(view);
  }

  void sync(const float4x4 &view_mat, const float4x4 &win_mat, int view_id = 0);

  /* For compatibility with old system. Will be removed at some point. */
  void sync(const DRWView *view);

  /** Disable a range in the multi-view array. Disabled view will not produce any instances. */
  void disable(IndexRange range);

  /** Enable or disable every visibility test (frustum culling, HiZ culling). */
  void visibility_test(bool enable)
  {
    do_visibility_ = enable;
  }

  /**
   * Update culling data using a compute shader.
   * This is to be used if the matrices were updated externally
   * on the GPU (not using the `sync()` method).
   **/
  void compute_procedural_bounds();

  bool is_persp(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].winmat[3][3] == 0.0f;
  }

  bool is_inverted(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    UNUSED_VARS_NDEBUG(view_id);
    return is_inverted_;
  }

  float far_clip(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    if (is_persp(view_id)) {
      return -data_[view_id].winmat[3][2] / (data_[view_id].winmat[2][2] + 1.0f);
    }
    return -(data_[view_id].winmat[3][2] - 1.0f) / data_[view_id].winmat[2][2];
  }

  float near_clip(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    if (is_persp(view_id)) {
      return -data_[view_id].winmat[3][2] / (data_[view_id].winmat[2][2] - 1.0f);
    }
    return -(data_[view_id].winmat[3][2] + 1.0f) / data_[view_id].winmat[2][2];
  }

  const float4x4 &viewmat(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].viewmat;
  }

  const float4x4 &viewinv(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].viewinv;
  }

  const float4x4 &winmat(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].winmat;
  }

  const float4x4 &wininv(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].wininv;
  }

  int visibility_word_per_draw() const
  {
    return (view_len_ == 1) ? 0 : divide_ceil_u(view_len_, 32);
  }

  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> &matrices_ubo_get()
  {
    return data_;
  }

 protected:
  /** Called from draw manager. */
  void bind();
  virtual void compute_visibility(ObjectBoundsBuf &bounds, uint resource_len, bool debug_freeze);
  virtual VisibilityBuf &get_visibility_buffer();

  void update_viewport_size();

  /* WARNING: These 3 functions must be called in order */
  void frustum_boundbox_calc(int view_id);
  void frustum_culling_planes_calc(int view_id);
  void frustum_culling_sphere_calc(int view_id);
};

}  // namespace blender::draw
