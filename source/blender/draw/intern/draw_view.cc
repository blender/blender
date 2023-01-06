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

void View::sync(const float4x4 &view_mat, const float4x4 &win_mat, int view_id)
{
  data_[view_id].viewmat = view_mat;
  data_[view_id].viewinv = view_mat.inverted();
  data_[view_id].winmat = win_mat;
  data_[view_id].wininv = win_mat.inverted();

  is_inverted_ = (is_negative_m4(view_mat.ptr()) == is_negative_m4(win_mat.ptr()));

  BoundBox &bound_box = *reinterpret_cast<BoundBox *>(&culling_[view_id].corners);
  BoundSphere &bound_sphere = *reinterpret_cast<BoundSphere *>(&culling_[view_id].bound_sphere);
  frustum_boundbox_calc(bound_box, view_id);
  frustum_culling_planes_calc(view_id);
  frustum_culling_sphere_calc(bound_box, bound_sphere, view_id);

  dirty_ = true;
}

void View::frustum_boundbox_calc(BoundBox &bbox, int view_id)
{
  /* Extract the 8 corners from a Projection Matrix. */
#if 0 /* Equivalent to this but it has accuracy problems. */
  BKE_boundbox_init_from_minmax(&bbox, float3(-1.0f),float3(1.0f));
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(data_.wininv.ptr(), bbox.vec[i]);
  }
#endif

  float left, right, bottom, top, near, far;
  bool is_persp = data_[view_id].winmat[3][3] == 0.0f;

  projmat_dimensions(data_[view_id].winmat.ptr(), &left, &right, &bottom, &top, &near, &far);

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
    mul_m4_v3(data_[view_id].viewinv.ptr(), bbox.vec[i]);
  }
}

void View::frustum_culling_planes_calc(int view_id)
{
  float4x4 persmat = data_[view_id].winmat * data_[view_id].viewmat;
  planes_from_projmat(persmat.ptr(),
                      culling_[view_id].planes[0],
                      culling_[view_id].planes[5],
                      culling_[view_id].planes[1],
                      culling_[view_id].planes[3],
                      culling_[view_id].planes[4],
                      culling_[view_id].planes[2]);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    culling_[view_id].planes[p].w /= normalize_v3(culling_[view_id].planes[p]);
  }
}

void View::frustum_culling_sphere_calc(const BoundBox &bbox, BoundSphere &bsphere, int view_id)
{
  /* Extract Bounding Sphere */
  if (data_[view_id].winmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    const float *nearpoint = bbox.vec[0];
    const float *farpoint = bbox.vec[6];

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
    drw_debug_matrix_as_bbox(persmat.inverted(), float4(0, 1, 0, 1));
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

  uint32_t data = 0xFFFFFFFFu;
  GPU_storagebuf_clear(visibility_buf_, GPU_R32UI, GPU_DATA_UINT, &data);

  if (do_visibility_) {
    GPUShader *shader = DRW_shader_draw_visibility_compute_get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1i(shader, "resource_len", resource_len);
    GPU_shader_uniform_1i(shader, "view_len", view_len_);
    GPU_shader_uniform_1i(shader, "visibility_word_per_draw", word_per_draw);
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
