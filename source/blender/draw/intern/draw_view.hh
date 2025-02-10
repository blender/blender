/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "GPU_matrix.hh"

#include "draw_shader_shared.hh"
#include <atomic>
#include <cstdint>

namespace blender::draw {

class Manager;

/* TODO: de-duplicate. */
using ObjectBoundsBuf = StorageArrayBuffer<ObjectBounds, 128>;
using ObjectInfosBuf = StorageArrayBuffer<ObjectInfos, 128>;
using VisibilityBuf = StorageArrayBuffer<uint, 4, true>;

class View {
  friend Manager;

  /** Number of sync done by views. Used for fingerprint. */
  static std::atomic<uint32_t> global_sync_counter_;

  /* Local sync counter. Used for fingerprint. */
  uint32_t sync_counter_ = 0;

 protected:
  /** TODO(fclem): Maybe try to reduce the minimum cost if the number of view is lower. */

  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> data_;
  UniformArrayBuffer<ViewCullingData, DRW_VIEW_MAX> culling_;
  /** Frozen version of data_ used for debugging culling. */
  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> data_freeze_;
  UniformArrayBuffer<ViewCullingData, DRW_VIEW_MAX> culling_freeze_;
  /** Result of the visibility computation. 1 bit or 1 or 2 word per resource ID per view. */
  VisibilityBuf visibility_buf_;
  /* Fingerprint of the manager state when visibility was computed. */
  uint64_t manager_fingerprint_ = 0;

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

  virtual ~View() = default;

  void sync(const float4x4 &view_mat, const float4x4 &win_mat, int view_id = 0);

  /** Enable or disable every visibility test (frustum culling, HiZ culling). */
  void visibility_test(bool enable)
  {
    do_visibility_ = enable;
  }

  /**
   * Update culling data using a compute shader.
   * This is to be used if the matrices were updated externally
   * on the GPU (not using the `sync()` method).
   */
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

  const float3 &location(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].viewinv.location();
  }

  const float3 &forward(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].viewinv.z_axis();
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

  /* Compute and return the perspective matrix. */
  const float4x4 persmat(int view_id = 0) const
  {
    BLI_assert(view_id < view_len_);
    return data_[view_id].winmat * data_[view_id].viewmat;
  }

  int visibility_word_per_draw() const
  {
    return (view_len_ == 1) ? 0 : divide_ceil_u(view_len_, 32);
  }

  UniformArrayBuffer<ViewMatrices, DRW_VIEW_MAX> &matrices_ubo_get()
  {
    return data_;
  }

  /* TODO(fclem): Remove. Global DST access. */
  static View &default_get();
  static void default_set(const float4x4 &view_mat, const float4x4 &win_mat);

  /* Data to save per overlay to not rely on rv3d for rendering.
   * TODO(fclem): Compute offset directly from the view. */
  struct OffsetData {
    /* Copy of rv3d->dist. */
    float dist = 0.0f;
    /* Copy of rv3d->persp. */
    char persp = 0;
    /* Copy of rv3d->is_persp. */
    bool is_persp = false;

    OffsetData() = default;
    OffsetData(const RegionView3D &rv3d)
        : dist(rv3d.dist), persp(rv3d.persp), is_persp(rv3d.is_persp != 0)
    {
    }

    float4x4 winmat_polygon_offset(float4x4 winmat, float offset)
    {
      float view_dist = dist;
      /* Special exception for orthographic camera:
       * `view_dist` isn't used as the depth range isn't the same. */
      if (persp == RV3D_CAMOB && is_persp == false) {
        view_dist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
      }

      winmat[3][2] -= GPU_polygon_offset_calc(winmat.ptr(), view_dist, offset);
      return winmat;
    }

    /* Return unit offset to apply to `gl_Position.z`. To be scaled depending on purpose. */
    float polygon_offset_factor(float4x4 winmat)
    {
      float view_dist = dist;
      /* Special exception for orthographic camera:
       * `view_dist` isn't used as the depth range isn't the same. */
      if (persp == RV3D_CAMOB && is_persp == false) {
        view_dist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
      }

      return GPU_polygon_offset_calc(winmat.ptr(), view_dist, 1.0);
    }
  };

  /* Returns frustum planes equations. Available only after sync. */
  std::array<float4, 6> frustum_planes_get(int view_id = 0);
  /* Returns frustum corners positions in world space. Available only after sync. */
  std::array<float3, 8> frustum_corners_get(int view_id = 0);

 protected:
  /** Called from draw manager. */
  void bind();
  virtual void compute_visibility(ObjectBoundsBuf &bounds,
                                  ObjectInfosBuf &infos,
                                  uint resource_len,
                                  bool debug_freeze);
  virtual VisibilityBuf &get_visibility_buffer();

  bool has_computed_visibility() const
  {
    /* NOTE: Even though manager fingerprint is not enough to check for update, it is still
     * guaranteed to not be 0. So we can check weather or not this view has computed visibility
     * after sync. Asserts will catch invalid usage . */
    return manager_fingerprint_ != 0;
  }

  /* Fingerprint of the view for the current state.
   * Not reliable enough for general update detection. Only to be used for debugging assertion. */
  uint64_t fingerprint_get() const
  {
    BLI_assert_msg(sync_counter_ != 0, "View should be synced at least once before use");
    return sync_counter_;
  }

  void update_viewport_size();

  /* WARNING: These 3 functions must be called in order */
  void frustum_boundbox_calc(int view_id);
  void frustum_culling_planes_calc(int view_id);
  void frustum_culling_sphere_calc(int view_id);
};

}  // namespace blender::draw
