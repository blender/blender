/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#include "BLI_math_geom.h"
#include "GPU_compute.h"
#include "GPU_debug.h"

#include "draw_debug.hh"
#include "draw_shader.h"
#include "draw_view.hh"

namespace blender::draw {

void View::sync(const float4x4 &view_mat, const float4x4 &win_mat)
{
  data_.viewmat = view_mat;
  data_.viewinv = view_mat.inverted();
  data_.winmat = win_mat;
  data_.wininv = win_mat.inverted();
  data_.persmat = data_.winmat * data_.viewmat;
  data_.persinv = data_.persmat.inverted();
  /* Should not be used anymore. */
  data_.viewcamtexcofac = float4(1.0f, 1.0f, 0.0f, 0.0f);

  data_.is_inverted = (is_negative_m4(view_mat.ptr()) == is_negative_m4(win_mat.ptr()));

  update_view_vectors();

  BoundBox &bound_box = *reinterpret_cast<BoundBox *>(&data_.frustum_corners);
  BoundSphere &bound_sphere = *reinterpret_cast<BoundSphere *>(&data_.frustum_bound_sphere);
  frustum_boundbox_calc(bound_box);
  frustum_culling_planes_calc();
  frustum_culling_sphere_calc(bound_box, bound_sphere);

  dirty_ = true;
}

void View::frustum_boundbox_calc(BoundBox &bbox)
{
  /* Extract the 8 corners from a Projection Matrix. */
#if 0 /* Equivalent to this but it has accuracy problems. */
  BKE_boundbox_init_from_minmax(&bbox, float3(-1.0f),float3(1.0f));
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(data_.wininv.ptr(), bbox.vec[i]);
  }
#endif

  float left, right, bottom, top, near, far;
  bool is_persp = data_.winmat[3][3] == 0.0f;

  projmat_dimensions(data_.winmat.ptr(), &left, &right, &bottom, &top, &near, &far);

  bbox.vec[0][2] = bbox.vec[3][2] = bbox.vec[7][2] = bbox.vec[4][2] = -near;
  bbox.vec[0][0] = bbox.vec[3][0] = left;
  bbox.vec[4][0] = bbox.vec[7][0] = right;
  bbox.vec[0][1] = bbox.vec[4][1] = bottom;
  bbox.vec[7][1] = bbox.vec[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  bbox.vec[1][2] = bbox.vec[2][2] = bbox.vec[6][2] = bbox.vec[5][2] = -far;
  bbox.vec[1][0] = bbox.vec[2][0] = left;
  bbox.vec[6][0] = bbox.vec[5][0] = right;
  bbox.vec[1][1] = bbox.vec[5][1] = bottom;
  bbox.vec[2][1] = bbox.vec[6][1] = top;

  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    mul_m4_v3(data_.viewinv.ptr(), bbox.vec[i]);
  }
}

void View::frustum_culling_planes_calc()
{
  planes_from_projmat(data_.persmat.ptr(),
                      data_.frustum_planes[0],
                      data_.frustum_planes[5],
                      data_.frustum_planes[1],
                      data_.frustum_planes[3],
                      data_.frustum_planes[4],
                      data_.frustum_planes[2]);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    data_.frustum_planes[p].w /= normalize_v3(data_.frustum_planes[p]);
  }
}

void View::frustum_culling_sphere_calc(const BoundBox &bbox, BoundSphere &bsphere)
{
  /* Extract Bounding Sphere */
  if (data_.winmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    const float *nearpoint = bbox.vec[0];
    const float *farpoint = bbox.vec[6];

    /* just use median point */
    mid_v3_v3v3(bsphere.center, farpoint, nearpoint);
    bsphere.radius = len_v3v3(bsphere.center, farpoint);
  }
  else if (data_.winmat[2][0] == 0.0f && data_.winmat[2][1] == 0.0f) {
    /* Perspective with symmetrical frustum. */

    /* We obtain the center and radius of the circumscribed circle of the
     * isosceles trapezoid composed by the diagonals of the near and far clipping plane */

    /* center of each clipping plane */
    float mid_min[3], mid_max[3];
    mid_v3_v3v3(mid_min, bbox.vec[3], bbox.vec[4]);
    mid_v3_v3v3(mid_max, bbox.vec[2], bbox.vec[5]);

    /* square length of the diagonals of each clipping plane */
    float a_sq = len_squared_v3v3(bbox.vec[3], bbox.vec[4]);
    float b_sq = len_squared_v3v3(bbox.vec[2], bbox.vec[5]);

    /* distance squared between clipping planes */
    float h_sq = len_squared_v3v3(mid_min, mid_max);

    float fac = (4 * h_sq + b_sq - a_sq) / (8 * h_sq);

    /* The goal is to get the smallest sphere,
     * not the sphere that passes through each corner */
    CLAMP(fac, 0.0f, 1.0f);

    interp_v3_v3v3(bsphere.center, mid_min, mid_max, fac);

    /* distance from the center to one of the points of the far plane (1, 2, 5, 6) */
    bsphere.radius = len_v3v3(bsphere.center, bbox.vec[1]);
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
      mul_v3_project_m4_v3(point, data_.wininv.ptr(), corner);
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
    mul_v3_project_m4_v3(nearpoint, data_.wininv.ptr(), nfar);
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
    mul_m4_v3(data_.viewinv.ptr(), bsphere.center); /* Transform to world space. */
    mul_m4_v3(data_.viewinv.ptr(), farpoint);
    bsphere.radius = len_v3v3(bsphere.center, farpoint);
  }
}

void View::set_clip_planes(Span<float4> planes)
{
  BLI_assert(planes.size() <= ARRAY_SIZE(data_.clip_planes));
  int i = 0;
  for (const auto &plane : planes) {
    data_.clip_planes[i++] = plane;
  }
}

void View::update_viewport_size()
{
  float4 viewport;
  GPU_viewport_size_get_f(viewport);
  float2 viewport_size = float2(viewport.z, viewport.w);
  if (assign_if_different(data_.viewport_size, viewport_size)) {
    dirty_ = true;
  }
}

void View::update_view_vectors()
{
  bool is_persp = data_.winmat[3][3] == 0.0f;

  /* Near clip distance. */
  data_.viewvecs[0][3] = (is_persp) ? -data_.winmat[3][2] / (data_.winmat[2][2] - 1.0f) :
                                      -(data_.winmat[3][2] + 1.0f) / data_.winmat[2][2];

  /* Far clip distance. */
  data_.viewvecs[1][3] = (is_persp) ? -data_.winmat[3][2] / (data_.winmat[2][2] + 1.0f) :
                                      -(data_.winmat[3][2] - 1.0f) / data_.winmat[2][2];

  /* View vectors for the corners of the view frustum.
   * Can be used to recreate the world space position easily */
  float3 view_vecs[4] = {
      {-1.0f, -1.0f, -1.0f},
      {1.0f, -1.0f, -1.0f},
      {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f},
  };

  /* Convert the view vectors to view space */
  for (int i = 0; i < 4; i++) {
    mul_project_m4_v3(data_.wininv.ptr(), view_vecs[i]);
    /* Normalized trick see:
     * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
    if (is_persp) {
      view_vecs[i].x /= view_vecs[i].z;
      view_vecs[i].y /= view_vecs[i].z;
    }
  }

  /**
   * - If orthographic:
   *   `view_vecs[0]` is the near-bottom-left corner of the frustum and
   *   `view_vecs[1]` is the vector going from the near-bottom-left corner to
   *   the far-top-right corner.
   * - If perspective:
   *   `view_vecs[0].xy` and `view_vecs[1].xy` are respectively the bottom-left corner
   *   when `Z = 1`, and top-left corner if `Z = 1`.
   *   `view_vecs[0].z` the near clip distance and `view_vecs[1].z` is the (signed)
   *   distance from the near plane to the far clip plane.
   */
  copy_v3_v3(data_.viewvecs[0], view_vecs[0]);

  /* we need to store the differences */
  data_.viewvecs[1][0] = view_vecs[1][0] - view_vecs[0][0];
  data_.viewvecs[1][1] = view_vecs[2][1] - view_vecs[0][1];
  data_.viewvecs[1][2] = view_vecs[3][2] - view_vecs[0][2];
}

void View::bind()
{
  update_viewport_size();

  if (dirty_) {
    dirty_ = false;
    data_.push_update();
  }

  GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
}

void View::compute_visibility(ObjectBoundsBuf &bounds, uint resource_len, bool debug_freeze)
{
  if (debug_freeze && frozen_ == false) {
    data_freeze_ = static_cast<ViewInfos>(data_);
    data_freeze_.push_update();
  }
#ifdef DEBUG
  if (debug_freeze) {
    drw_debug_matrix_as_bbox(data_freeze_.persinv, float4(0, 1, 0, 1));
  }
#endif
  frozen_ = debug_freeze;

  GPU_debug_group_begin("View.compute_visibility");

  /* TODO(fclem): Early out if visibility hasn't changed. */
  /* TODO(fclem): Resize to nearest pow2 to reduce fragmentation. */
  visibility_buf_.resize(divide_ceil_u(resource_len, 128));

  uint32_t data = 0xFFFFFFFFu;
  GPU_storagebuf_clear(visibility_buf_, GPU_R32UI, GPU_DATA_UINT, &data);

  if (do_visibility_) {
    GPUShader *shader = DRW_shader_draw_visibility_compute_get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1i(shader, "resource_len", resource_len);
    GPU_storagebuf_bind(bounds, GPU_shader_get_ssbo(shader, "bounds_buf"));
    GPU_storagebuf_bind(visibility_buf_, GPU_shader_get_ssbo(shader, "visibility_buf"));
    GPU_uniformbuf_bind((frozen_) ? data_freeze_ : data_, DRW_VIEW_UBO_SLOT);
    GPU_compute_dispatch(shader, divide_ceil_u(resource_len, DRW_VISIBILITY_GROUP_SIZE), 1, 1);
    GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
  }

  if (frozen_) {
    /* Bind back the non frozen data. */
    GPU_uniformbuf_bind(data_, DRW_VIEW_UBO_SLOT);
  }

  GPU_debug_group_end();
}

}  // namespace blender::draw
