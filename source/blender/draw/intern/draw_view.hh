/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup draw
 */

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"

#include "draw_shader_shared.h"

namespace blender::draw {

class Manager;

/* TODO: de-duplicate. */
using ObjectBoundsBuf = StorageArrayBuffer<ObjectBounds, 128>;
/** \note Using uint4 for declaration but bound as uint. */
using VisibilityBuf = StorageArrayBuffer<uint4, 1, true>;

class View {
  friend Manager;

 private:
  UniformBuffer<ViewInfos> data_;
  /** Frozen version of data_ used for debugging culling. */
  UniformBuffer<ViewInfos> data_freeze_;
  /** Result of the visibility computation. 1 bit per resource ID. */
  VisibilityBuf visibility_buf_;

  const char *debug_name_;

  bool do_visibility_ = true;
  bool dirty_ = true;
  bool frozen_ = false;

 public:
  View(const char *name) : visibility_buf_(name), debug_name_(name){};
  /* For compatibility with old system. Will be removed at some point. */
  View(const char *name, const DRWView *view) : visibility_buf_(name), debug_name_(name)
  {
    float4x4 view_mat, win_mat;
    DRW_view_viewmat_get(view, view_mat.ptr(), false);
    DRW_view_winmat_get(view, win_mat.ptr(), false);
    this->sync(view_mat, win_mat);
  }

  void set_clip_planes(Span<float4> planes);

  void sync(const float4x4 &view_mat, const float4x4 &win_mat);

  bool is_persp() const
  {
    return data_.winmat[3][3] == 0.0f;
  }

  bool is_inverted() const
  {
    return data_.is_inverted;
  }

  float far_clip() const
  {
    if (is_persp()) {
      return -data_.winmat[3][2] / (data_.winmat[2][2] + 1.0f);
    }
    return -(data_.winmat[3][2] - 1.0f) / data_.winmat[2][2];
  }

  float near_clip() const
  {
    if (is_persp()) {
      return -data_.winmat[3][2] / (data_.winmat[2][2] - 1.0f);
    }
    return -(data_.winmat[3][2] + 1.0f) / data_.winmat[2][2];
  }

 private:
  /** Called from draw manager. */
  void bind();
  void compute_visibility(ObjectBoundsBuf &bounds, uint resource_len, bool debug_freeze);

  void update_view_vectors();
  void update_viewport_size();

  void frustum_boundbox_calc(BoundBox &bbox);
  void frustum_culling_planes_calc();
  void frustum_culling_sphere_calc(const BoundBox &bbox, BoundSphere &bsphere);
};

}  // namespace blender::draw
