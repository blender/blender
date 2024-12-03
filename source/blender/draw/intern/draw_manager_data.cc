/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_pbvh.hh"

#include "draw_attributes.hh"
#include "draw_manager_c.hh"

#include "BKE_attribute.hh"
#include "BKE_curve.hh"
#include "BKE_customdata.hh"
#include "BKE_duplilist.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_volume.hh"

/* For debug cursor position. */
#include "WM_api.hh"
#include "wm_window.hh"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"

#include "BLI_array.hh"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"
#include "BLI_mempool.h"

#ifdef DRW_DEBUG_CULLING
#  include "BLI_math_bits.h"
#endif

#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_uniform_buffer.hh"

#include "intern/gpu_codegen.hh"

/* -------------------------------------------------------------------- */
/** \name Draw Call (DRW_calls)
 * \{ */

eDRWCommandType command_type_get(const uint64_t *command_type_bits, int index)
{
  return eDRWCommandType((command_type_bits[index / 16] >> ((index % 16) * 4)) & 0xF);
}

/* -------------------------------------------------------------------- */
/** \name View (DRW_view)
 * \{ */

/* Extract the 8 corners from a Projection Matrix.
 * Although less accurate, this solution can be simplified as follows:
 * BKE_boundbox_init_from_minmax(&bbox, (const float[3]){-1.0f, -1.0f, -1.0f}, (const
 * float[3]){1.0f, 1.0f, 1.0f}); for (int i = 0; i < 8; i++) {mul_project_m4_v3(projinv,
 * bbox.vec[i]);}
 */
static void draw_frustum_boundbox_calc(const float (*viewinv)[4],
                                       const float (*projmat)[4],
                                       BoundBox *r_bbox)
{
  float left, right, bottom, top, near, far;
  bool is_persp = projmat[3][3] == 0.0f;

#if 0 /* Equivalent to this but it has accuracy problems. */
  BKE_boundbox_init_from_minmax(
      &bbox, blender::float3{-1.0f, -1.0f, -1.0f}, blender::float3{1.0f, 1.0f, 1.0f});
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(projinv, bbox.vec[i]);
  }
#endif

  projmat_dimensions(projmat, &left, &right, &bottom, &top, &near, &far);

  r_bbox->vec[0][2] = r_bbox->vec[3][2] = r_bbox->vec[7][2] = r_bbox->vec[4][2] = -near;
  r_bbox->vec[0][0] = r_bbox->vec[3][0] = left;
  r_bbox->vec[4][0] = r_bbox->vec[7][0] = right;
  r_bbox->vec[0][1] = r_bbox->vec[4][1] = bottom;
  r_bbox->vec[7][1] = r_bbox->vec[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  r_bbox->vec[1][2] = r_bbox->vec[2][2] = r_bbox->vec[6][2] = r_bbox->vec[5][2] = -far;
  r_bbox->vec[1][0] = r_bbox->vec[2][0] = left;
  r_bbox->vec[6][0] = r_bbox->vec[5][0] = right;
  r_bbox->vec[1][1] = r_bbox->vec[5][1] = bottom;
  r_bbox->vec[2][1] = r_bbox->vec[6][1] = top;

  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    mul_m4_v3(viewinv, r_bbox->vec[i]);
  }
}

static void draw_frustum_culling_planes_calc(const float (*persmat)[4], float (*frustum_planes)[4])
{
  planes_from_projmat(persmat,
                      frustum_planes[0],
                      frustum_planes[5],
                      frustum_planes[1],
                      frustum_planes[3],
                      frustum_planes[4],
                      frustum_planes[2]);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    frustum_planes[p][3] /= normalize_v3(frustum_planes[p]);
  }
}

static void draw_frustum_bound_sphere_calc(const BoundBox *bbox,
                                           const float (*viewinv)[4],
                                           const float (*projmat)[4],
                                           const float (*projinv)[4],
                                           BoundSphere *bsphere)
{
  /* Extract Bounding Sphere */
  if (projmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    const float *nearpoint = bbox->vec[0];
    const float *farpoint = bbox->vec[6];

    /* just use median point */
    mid_v3_v3v3(bsphere->center, farpoint, nearpoint);
    bsphere->radius = len_v3v3(bsphere->center, farpoint);
  }
  else if (projmat[2][0] == 0.0f && projmat[2][1] == 0.0f) {
    /* Perspective with symmetrical frustum. */

    /* We obtain the center and radius of the circumscribed circle of the
     * isosceles trapezoid composed by the diagonals of the near and far clipping plane */

    /* center of each clipping plane */
    float mid_min[3], mid_max[3];
    mid_v3_v3v3(mid_min, bbox->vec[3], bbox->vec[4]);
    mid_v3_v3v3(mid_max, bbox->vec[2], bbox->vec[5]);

    /* square length of the diagonals of each clipping plane */
    float a_sq = len_squared_v3v3(bbox->vec[3], bbox->vec[4]);
    float b_sq = len_squared_v3v3(bbox->vec[2], bbox->vec[5]);

    /* distance squared between clipping planes */
    float h_sq = len_squared_v3v3(mid_min, mid_max);

    float fac = (4 * h_sq + b_sq - a_sq) / (8 * h_sq);

    /* The goal is to get the smallest sphere,
     * not the sphere that passes through each corner */
    CLAMP(fac, 0.0f, 1.0f);

    interp_v3_v3v3(bsphere->center, mid_min, mid_max, fac);

    /* distance from the center to one of the points of the far plane (1, 2, 5, 6) */
    bsphere->radius = len_v3v3(bsphere->center, bbox->vec[1]);
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
      mul_v3_project_m4_v3(point, projinv, corner);
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
    mul_v3_project_m4_v3(nearpoint, projinv, nfar);
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

    bsphere->center[0] = farcenter[0] * z / e;
    bsphere->center[1] = farcenter[1] * z / e;
    bsphere->center[2] = z;

    /* For XR, the view matrix may contain a scale factor. Then, transforming only the center
     * into world space after calculating the radius will result in incorrect behavior. */
    mul_m4_v3(viewinv, bsphere->center); /* Transform to world space. */
    mul_m4_v3(viewinv, farpoint);
    bsphere->radius = len_v3v3(bsphere->center, farpoint);
  }
}

static void draw_view_matrix_state_update(DRWView *view,
                                          const float viewmat[4][4],
                                          const float winmat[4][4])
{
  ViewMatrices *storage = &view->storage;

  copy_m4_m4(storage->viewmat.ptr(), viewmat);
  invert_m4_m4(storage->viewinv.ptr(), storage->viewmat.ptr());

  copy_m4_m4(storage->winmat.ptr(), winmat);
  invert_m4_m4(storage->wininv.ptr(), storage->winmat.ptr());

  mul_m4_m4m4(view->persmat.ptr(), winmat, viewmat);
  invert_m4_m4(view->persinv.ptr(), view->persmat.ptr());
}

DRWView *DRW_view_create(const float viewmat[4][4],
                         const float winmat[4][4],
                         const float (*culling_viewmat)[4],
                         const float (*culling_winmat)[4])
{
  DRWView *view = static_cast<DRWView *>(BLI_memblock_alloc(DST.vmempool->views));

  if (DST.primary_view_num < MAX_CULLED_VIEWS) {
    view->culling_mask = 1u << DST.primary_view_num++;
  }
  else {
    BLI_assert(0);
    view->culling_mask = 0u;
  }
  view->clip_planes_len = 0;
  view->parent = nullptr;

  DRW_view_update(view, viewmat, winmat, culling_viewmat, culling_winmat);

  return view;
}

DRWView *DRW_view_create_sub(const DRWView *parent_view,
                             const float viewmat[4][4],
                             const float winmat[4][4])
{
  /* Search original parent. */
  const DRWView *ori_view = parent_view;
  while (ori_view->parent != nullptr) {
    ori_view = ori_view->parent;
  }

  DRWView *view = static_cast<DRWView *>(BLI_memblock_alloc(DST.vmempool->views));

  /* Perform copy. */
  *view = *ori_view;
  view->parent = (DRWView *)ori_view;

  DRW_view_update_sub(view, viewmat, winmat);

  return view;
}

/* DRWView Update:
 * This is meant to be done on existing views when rendering in a loop and there is no
 * need to allocate more DRWViews. */

void DRW_view_update_sub(DRWView *view, const float viewmat[4][4], const float winmat[4][4])
{
  BLI_assert(view->parent != nullptr);

  view->is_dirty = true;
  view->is_inverted = (is_negative_m4(viewmat) == is_negative_m4(winmat));

  draw_view_matrix_state_update(view, viewmat, winmat);
}

void DRW_view_update(DRWView *view,
                     const float viewmat[4][4],
                     const float winmat[4][4],
                     const float (*culling_viewmat)[4],
                     const float (*culling_winmat)[4])
{
  /* DO NOT UPDATE THE DEFAULT VIEW.
   * Create sub-views instead, or a copy. */
  BLI_assert(view != DST.view_default);
  BLI_assert(view->parent == nullptr);

  view->is_dirty = true;
  view->is_inverted = (is_negative_m4(viewmat) == is_negative_m4(winmat));

  draw_view_matrix_state_update(view, viewmat, winmat);

  /* Prepare frustum culling. */

#ifdef DRW_DEBUG_CULLING
  static float mv[MAX_CULLED_VIEWS][4][4], mw[MAX_CULLED_VIEWS][4][4];

  /* Select view here. */
  if (view->culling_mask != 0) {
    uint index = bitscan_forward_uint(view->culling_mask);

    if (G.debug_value == 0) {
      copy_m4_m4(mv[index], culling_viewmat ? culling_viewmat : viewmat);
      copy_m4_m4(mw[index], culling_winmat ? culling_winmat : winmat);
    }
    else {
      culling_winmat = mw[index];
      culling_viewmat = mv[index];
    }
  }
#endif

  float wininv[4][4];
  if (culling_winmat) {
    winmat = culling_winmat;
    invert_m4_m4(wininv, winmat);
  }
  else {
    copy_m4_m4(wininv, view->storage.wininv.ptr());
  }

  float viewinv[4][4];
  if (culling_viewmat) {
    viewmat = culling_viewmat;
    invert_m4_m4(viewinv, viewmat);
  }
  else {
    copy_m4_m4(viewinv, view->storage.viewinv.ptr());
  }

  draw_frustum_boundbox_calc(viewinv, winmat, &view->frustum_corners);
  draw_frustum_culling_planes_calc(view->persmat.ptr(), view->frustum_planes);
  draw_frustum_bound_sphere_calc(
      &view->frustum_corners, viewinv, winmat, wininv, &view->frustum_bsphere);

#ifdef DRW_DEBUG_CULLING
  if (G.debug_value != 0) {
    DRW_debug_sphere(
        view->frustum_bsphere.center, view->frustum_bsphere.radius, blender::float4{1, 1, 0, 1});
    DRW_debug_bbox(&view->frustum_corners, blender::float4{1, 1, 0, 1});
  }
#endif
}

const DRWView *DRW_view_default_get()
{
  return DST.view_default;
}

void DRW_view_reset()
{
  DST.view_default = nullptr;
  DST.view_active = nullptr;
  DST.view_previous = nullptr;
}

void DRW_view_default_set(const DRWView *view)
{
  BLI_assert(DST.view_default == nullptr);
  DST.view_default = (DRWView *)view;
}

void DRW_view_clip_planes_set(DRWView *view, float (*planes)[4], int plane_len)
{
  BLI_assert(plane_len <= MAX_CLIP_PLANES);
  view->clip_planes_len = plane_len;
  if (plane_len > 0) {
    memcpy(view->clip_planes, planes, sizeof(float[4]) * plane_len);
  }
}

void DRW_view_frustum_corners_get(const DRWView *view, BoundBox *corners)
{
  memcpy(corners, &view->frustum_corners, sizeof(view->frustum_corners));
}

std::array<float4, 6> DRW_view_frustum_planes_get(const DRWView *view)
{
  std::array<float4, 6> planes;
  const blender::Span<float4> view_planes(reinterpret_cast<const float4 *>(&view->frustum_planes),
                                          6);
  std::copy(view_planes.begin(), view_planes.end(), planes.begin());
  return planes;
}

bool DRW_view_is_persp_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  return view->storage.winmat[3][3] == 0.0f;
}

float DRW_view_near_distance_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  const float4x4 &projmat = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] - 1.0f);
  }

  return -(projmat[3][2] + 1.0f) / projmat[2][2];
}

float DRW_view_far_distance_get(const DRWView *view)
{
  view = (view) ? view : DST.view_default;
  const float4x4 &projmat = view->storage.winmat;

  if (DRW_view_is_persp_get(view)) {
    return -projmat[3][2] / (projmat[2][2] + 1.0f);
  }

  return -(projmat[3][2] - 1.0f) / projmat[2][2];
}

void DRW_view_viewmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const ViewMatrices *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->viewinv.ptr() : storage->viewmat.ptr());
}

void DRW_view_winmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  const ViewMatrices *storage = &view->storage;
  copy_m4_m4(mat, (inverse) ? storage->wininv.ptr() : storage->winmat.ptr());
}

void DRW_view_persmat_get(const DRWView *view, float mat[4][4], bool inverse)
{
  view = (view) ? view : DST.view_default;
  copy_m4_m4(mat, (inverse) ? view->persinv.ptr() : view->persmat.ptr());
}

/** \} */
