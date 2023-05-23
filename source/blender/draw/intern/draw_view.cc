/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "GPU_compute.h"
#include "GPU_debug.h"

#include "draw_debug.hh"
#include "draw_shader.h"
#include "draw_view.hh"

namespace blender::draw {

void View::sync(const float4x4 &view_mat, const float4x4 &win_mat, int view_id)
{
  data_[view_id].viewmat = view_mat;
  data_[view_id].viewinv = math::invert(view_mat);
  data_[view_id].winmat = win_mat;
  data_[view_id].wininv = math::invert(win_mat);

  is_inverted_ = (is_negative_m4(view_mat.ptr()) == is_negative_m4(win_mat.ptr()));

  frustum_boundbox_calc(view_id);
  frustum_culling_planes_calc(view_id);
  frustum_culling_sphere_calc(view_id);

  dirty_ = true;
}

void View::sync(const DRWView *view)
{
  float4x4 view_mat, win_mat;
  DRW_view_viewmat_get(view, view_mat.ptr(), false);
  DRW_view_winmat_get(view, win_mat.ptr(), false);
  this->sync(view_mat, win_mat);
}

void View::frustum_boundbox_calc(int view_id)
{
  /* Extract the 8 corners from a Projection Matrix. */
#if 0 /* Equivalent to this but it has accuracy problems. */
  BKE_boundbox_init_from_minmax(&bbox, float3(-1.0f),float3(1.0f));
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(data_.wininv.ptr(), bbox.vec[i]);
  }
#endif

  MutableSpan<float4> corners = {culling_[view_id].frustum_corners.corners,
                                 int64_t(ARRAY_SIZE(culling_[view_id].frustum_corners.corners))};

  float left, right, bottom, top, near, far;
  bool is_persp = data_[view_id].winmat[3][3] == 0.0f;

  projmat_dimensions(data_[view_id].winmat.ptr(), &left, &right, &bottom, &top, &near, &far);

  corners[0][2] = corners[3][2] = corners[7][2] = corners[4][2] = -near;
  corners[0][0] = corners[3][0] = left;
  corners[4][0] = corners[7][0] = right;
  corners[0][1] = corners[4][1] = bottom;
  corners[7][1] = corners[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  corners[1][2] = corners[2][2] = corners[6][2] = corners[5][2] = -far;
  corners[1][0] = corners[2][0] = left;
  corners[6][0] = corners[5][0] = right;
  corners[1][1] = corners[5][1] = bottom;
  corners[2][1] = corners[6][1] = top;

  /* Transform into world space. */
  for (float4 &corner : corners) {
    mul_m4_v3(data_[view_id].viewinv.ptr(), corner);
    corner.w = 1.0;
  }
}

void View::frustum_culling_planes_calc(int view_id)
{
  float4x4 persmat = data_[view_id].winmat * data_[view_id].viewmat;
  planes_from_projmat(persmat.ptr(),
                      culling_[view_id].frustum_planes.planes[0],
                      culling_[view_id].frustum_planes.planes[5],
                      culling_[view_id].frustum_planes.planes[1],
                      culling_[view_id].frustum_planes.planes[3],
                      culling_[view_id].frustum_planes.planes[4],
                      culling_[view_id].frustum_planes.planes[2]);

  /* Normalize. */
  for (float4 &plane : culling_[view_id].frustum_planes.planes) {
    plane.w /= normalize_v3(plane);
  }
}

void View::frustum_culling_sphere_calc(int view_id)
{
  BoundSphere &bsphere = *reinterpret_cast<BoundSphere *>(&culling_[view_id].bound_sphere);
  Span<float4> corners = {culling_[view_id].frustum_corners.corners,
                          int64_t(ARRAY_SIZE(culling_[view_id].frustum_corners.corners))};

  /* Extract Bounding Sphere */
  if (data_[view_id].winmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    const float *nearpoint = corners[0];
    const float *farpoint = corners[6];

    /* just use median point */
    mid_v3_v3v3(bsphere.center, farpoint, nearpoint);
    bsphere.radius = len_v3v3(bsphere.center, farpoint);
  }
  else if (data_[view_id].winmat[2][0] == 0.0f && data_[view_id].winmat[2][1] == 0.0f) {
    /* Perspective with symmetrical frustum. */

    /* We obtain the center and radius of the circumscribed circle of the
     * isosceles trapezoid composed by the diagonals of the near and far clipping plane */

    /* center of each clipping plane */
    float mid_min[3], mid_max[3];
    mid_v3_v3v3(mid_min, corners[3], corners[4]);
    mid_v3_v3v3(mid_max, corners[2], corners[5]);

    /* square length of the diagonals of each clipping plane */
    float a_sq = len_squared_v3v3(corners[3], corners[4]);
    float b_sq = len_squared_v3v3(corners[2], corners[5]);

    /* distance squared between clipping planes */
    float h_sq = len_squared_v3v3(mid_min, mid_max);

    float fac = (4 * h_sq + b_sq - a_sq) / (8 * h_sq);

    /* The goal is to get the smallest sphere,
     * not the sphere that passes through each corner */
    CLAMP(fac, 0.0f, 1.0f);

    interp_v3_v3v3(bsphere.center, mid_min, mid_max, fac);

    /* distance from the center to one of the points of the far plane (1, 2, 5, 6) */
    bsphere.radius = len_v3v3(bsphere.center, corners[1]);
  }
  else {
    /* Perspective with asymmetrical frustum. */

    /* We put the sphere center on the line that goes from origin
     * to the center of the far clipping plane. */

    /* Detect which of the corner of the far clipping plane is the farthest to the origin */
    float nfar[4];               /* most extreme far point in NDC space */
    float farxy[2];              /* far-point projection onto the near plane */
    float farpoint[3] = {0.0f};  /* most extreme far point in camera coordinate */
    float nearpoint[3];          /* most extreme near point in camera coordinate */
    float farcenter[3] = {0.0f}; /* center of far clipping plane in camera coordinate */
    float F = -1.0f, N;          /* square distance of far and near point to origin */
    float f, n; /* distance of far and near point to z axis. f is always > 0 but n can be < 0 */
    float e, s; /* far and near clipping distance (<0) */
    float c;    /* slope of center line = distance of far clipping center
                 * to z axis / far clipping distance. */
    float z;    /* projection of sphere center on z axis (<0) */

    /* Find farthest corner and center of far clip plane. */
    float corner[3] = {1.0f, 1.0f, 1.0f}; /* in clip space */
    for (int i = 0; i < 4; i++) {
      float point[3];
      mul_v3_project_m4_v3(point, data_[view_id].wininv.ptr(), corner);
      float len = len_squared_v3(point);
      if (len > F) {
        copy_v3_v3(nfar, corner);
        copy_v3_v3(farpoint, point);
        F = len;
      }
      add_v3_v3(farcenter, point);
      /* rotate by 90 degree to walk through the 4 points of the far clip plane */
      float tmp = corner[0];
      corner[0] = -corner[1];
      corner[1] = tmp;
    }

    /* the far center is the average of the far clipping points */
    mul_v3_fl(farcenter, 0.25f);
    /* the extreme near point is the opposite point on the near clipping plane */
    copy_v3_fl3(nfar, -nfar[0], -nfar[1], -1.0f);
    mul_v3_project_m4_v3(nearpoint, data_[view_id].wininv.ptr(), nfar);
    /* this is a frustum projection */
    N = len_squared_v3(nearpoint);
    e = farpoint[2];
    s = nearpoint[2];
    /* distance to view Z axis */
    f = len_v2(farpoint);
    /* get corresponding point on the near plane */
    mul_v2_v2fl(farxy, farpoint, s / e);
    /* this formula preserve the sign of n */
    sub_v2_v2(nearpoint, farxy);
    n = f * s / e - len_v2(nearpoint);
    c = len_v2(farcenter) / e;
    /* the big formula, it simplifies to (F-N)/(2(e-s)) for the symmetric case */
    z = (F - N) / (2.0f * (e - s + c * (f - n)));

    bsphere.center[0] = farcenter[0] * z / e;
    bsphere.center[1] = farcenter[1] * z / e;
    bsphere.center[2] = z;

    /* For XR, the view matrix may contain a scale factor. Then, transforming only the center
     * into world space after calculating the radius will result in incorrect behavior. */
    mul_m4_v3(data_[view_id].viewinv.ptr(), bsphere.center); /* Transform to world space. */
    mul_m4_v3(data_[view_id].viewinv.ptr(), farpoint);
    bsphere.radius = len_v3v3(bsphere.center, farpoint);
  }
}

void View::disable(IndexRange range)
{
  /* Set bounding sphere to -1.0f radius will bypass the culling test and treat every instance as
   * invisible. */
  range = IndexRange(view_len_).intersect(range);
  for (auto view_id : range) {
    reinterpret_cast<BoundSphere *>(&culling_[view_id].bound_sphere)->radius = -1.0f;
  }
}

void View::bind()
{
  if (dirty_ && !procedural_) {
    dirty_ = false;
    data_.push_update();
    culling_.push_update();
  }

  GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
  GPU_uniformbuf_bind(culling_, DRW_VIEW_CULLING_UBO_SLOT);
}

void View::compute_procedural_bounds()
{
  GPU_debug_group_begin("View.compute_procedural_bounds");

  GPUShader *shader = DRW_shader_draw_view_finalize_get();
  GPU_shader_bind(shader);
  GPU_uniformbuf_bind_as_ssbo(culling_, GPU_shader_get_ssbo_binding(shader, "view_culling_buf"));
  GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
  GPU_compute_dispatch(shader, 1, 1, 1);
  GPU_memory_barrier(GPU_BARRIER_UNIFORM);

  GPU_debug_group_end();
}

void View::compute_visibility(ObjectBoundsBuf &bounds, uint resource_len, bool debug_freeze)
{
  if (debug_freeze && frozen_ == false) {
    data_freeze_[0] = static_cast<ViewMatrices>(data_[0]);
    data_freeze_.push_update();
    culling_freeze_[0] = static_cast<ViewCullingData>(culling_[0]);
    culling_freeze_.push_update();
  }
#ifdef DEBUG
  if (debug_freeze) {
    float4x4 persmat = data_freeze_[0].winmat * data_freeze_[0].viewmat;
    drw_debug_matrix_as_bbox(math::invert(persmat), float4(0, 1, 0, 1));
  }
#endif
  frozen_ = debug_freeze;

  GPU_debug_group_begin("View.compute_visibility");

  /* TODO(fclem): Early out if visibility hasn't changed. */

  uint word_per_draw = this->visibility_word_per_draw();
  /* Switch between tightly packed and set of whole word per instance. */
  uint words_len = (view_len_ == 1) ? divide_ceil_u(resource_len, 32) :
                                      resource_len * word_per_draw;
  words_len = ceil_to_multiple_u(max_ii(1, words_len), 4);
  /* TODO(fclem): Resize to nearest pow2 to reduce fragmentation. */
  visibility_buf_.resize(words_len);

  const uint32_t data = 0xFFFFFFFFu;
  GPU_storagebuf_clear(visibility_buf_, data);

  if (do_visibility_) {
    GPUShader *shader = DRW_shader_draw_visibility_compute_get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1i(shader, "resource_len", resource_len);
    GPU_shader_uniform_1i(shader, "view_len", view_len_);
    GPU_shader_uniform_1i(shader, "visibility_word_per_draw", word_per_draw);
    GPU_storagebuf_bind(bounds, GPU_shader_get_ssbo_binding(shader, "bounds_buf"));
    GPU_storagebuf_bind(visibility_buf_, GPU_shader_get_ssbo_binding(shader, "visibility_buf"));
    GPU_uniformbuf_bind(frozen_ ? data_freeze_ : data_, DRW_VIEW_UBO_SLOT);
    GPU_uniformbuf_bind(frozen_ ? culling_freeze_ : culling_, DRW_VIEW_CULLING_UBO_SLOT);
    GPU_compute_dispatch(shader, divide_ceil_u(resource_len, DRW_VISIBILITY_GROUP_SIZE), 1, 1);
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  }

  if (frozen_) {
    /* Bind back the non frozen data. */
    GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
    GPU_uniformbuf_bind(culling_, DRW_VIEW_CULLING_UBO_SLOT);
  }

  GPU_debug_group_end();
}

VisibilityBuf &View::get_visibility_buffer()
{
  return visibility_buf_;
}

}  // namespace blender::draw
