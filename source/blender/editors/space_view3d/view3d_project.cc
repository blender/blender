/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_camera.h"
#include "BKE_screen.hh"

#include "GPU_matrix.hh"

#include "ED_view3d.hh" /* own include */

#define BL_ZERO_CLIP 0.001

/* Non Clipping Projection Functions
 * ********************************* */

blender::float2 ED_view3d_project_float_v2_m4(const ARegion *region,
                                              const float co[3],
                                              const blender::float4x4 &mat)
{
  float vec4[4];

  copy_v3_v3(vec4, co);
  vec4[3] = 1.0;
  // co_region[0] = IS_CLIPPED; /* Always overwritten. */

  mul_m4_v4(mat.ptr(), vec4);

  blender::float2 co_region;
  if (vec4[3] > FLT_EPSILON) {
    co_region[0] = float(region->winx / 2.0f) + (region->winx / 2.0f) * vec4[0] / vec4[3];
    co_region[1] = float(region->winy / 2.0f) + (region->winy / 2.0f) * vec4[1] / vec4[3];
  }
  else {
    zero_v2(co_region);
  }
  return co_region;
}

void ED_view3d_project_float_v3_m4(const ARegion *region,
                                   const float co[3],
                                   float r_co[3],
                                   const float mat[4][4])
{
  float vec4[4];

  copy_v3_v3(vec4, co);
  vec4[3] = 1.0;
  // r_co[0] = IS_CLIPPED; /* Always overwritten. */

  mul_m4_v4(mat, vec4);

  if (vec4[3] > FLT_EPSILON) {
    r_co[0] = float(region->winx / 2.0f) + (region->winx / 2.0f) * vec4[0] / vec4[3];
    r_co[1] = float(region->winy / 2.0f) + (region->winy / 2.0f) * vec4[1] / vec4[3];
    r_co[2] = vec4[2] / vec4[3];
  }
  else {
    zero_v3(r_co);
  }
}

/* Clipping Projection Functions
 * ***************************** */

eV3DProjStatus ED_view3d_project_base(const ARegion *region, Base *base, float r_co[2])
{
  eV3DProjStatus ret = ED_view3d_project_float_global(
      region, base->object->object_to_world().location(), r_co, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Prevent uninitialized values when projection fails,
   * although the callers should check the return value. */
  if (ret != V3D_PROJ_RET_OK) {
    r_co[0] = -1.0;
    r_co[1] = -1.0;
  }

  return ret;
}

/**
 * `perspmat` is typically either:
 * - 'rv3d->perspmat',  is_local == false.
 * - 'rv3d->persmatob', is_local == true.
 */
static eV3DProjStatus ed_view3d_project__internal(const ARegion *region,
                                                  const float perspmat[4][4],
                                                  const bool is_local, /* normally hidden */
                                                  const float co[3],
                                                  float r_co[2],
                                                  const eV3DProjTest flag)
{
  float vec4[4];

  /* check for bad flags */
  BLI_assert((flag & V3D_PROJ_TEST_ALL) == flag);

  if (flag & V3D_PROJ_TEST_CLIP_BB) {
    const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);
    if (rv3d->rflag & RV3D_CLIPPING) {
      if (ED_view3d_clipping_test(rv3d, co, is_local)) {
        return V3D_PROJ_RET_CLIP_BB;
      }
    }
  }

  copy_v3_v3(vec4, co);
  vec4[3] = 1.0;
  mul_m4_v4(perspmat, vec4);
  const float w = fabsf(vec4[3]);

  if ((flag & V3D_PROJ_TEST_CLIP_ZERO) && (w <= float(BL_ZERO_CLIP))) {
    return V3D_PROJ_RET_CLIP_ZERO;
  }

  if ((flag & V3D_PROJ_TEST_CLIP_NEAR) && (vec4[2] <= -w)) {
    return V3D_PROJ_RET_CLIP_NEAR;
  }

  if ((flag & V3D_PROJ_TEST_CLIP_FAR) && (vec4[2] >= w)) {
    return V3D_PROJ_RET_CLIP_FAR;
  }

  const float scalar = (w != 0.0f) ? (1.0f / w) : 0.0f;
  const float fx = (float(region->winx) / 2.0f) * (1.0f + (vec4[0] * scalar));
  const float fy = (float(region->winy) / 2.0f) * (1.0f + (vec4[1] * scalar));

  if ((flag & V3D_PROJ_TEST_CLIP_WIN) &&
      (fx <= 0.0f || fy <= 0.0f || fx >= float(region->winx) || fy >= float(region->winy)))
  {
    return V3D_PROJ_RET_CLIP_WIN;
  }

  r_co[0] = fx;
  r_co[1] = fy;

  return V3D_PROJ_RET_OK;
}

eV3DProjStatus ED_view3d_project_short_ex(const ARegion *region,
                                          float perspmat[4][4],
                                          const bool is_local,
                                          const float co[3],
                                          short r_co[2],
                                          const eV3DProjTest flag)
{
  float tvec[2];
  eV3DProjStatus ret = ed_view3d_project__internal(region, perspmat, is_local, co, tvec, flag);
  if (ret == V3D_PROJ_RET_OK) {
    if ((tvec[0] > -32700.0f && tvec[0] < 32700.0f) && (tvec[1] > -32700.0f && tvec[1] < 32700.0f))
    {
      r_co[0] = short(floorf(tvec[0]));
      r_co[1] = short(floorf(tvec[1]));
    }
    else {
      ret = V3D_PROJ_RET_OVERFLOW;
    }
  }
  return ret;
}

eV3DProjStatus ED_view3d_project_int_ex(const ARegion *region,
                                        float perspmat[4][4],
                                        const bool is_local,
                                        const float co[3],
                                        int r_co[2],
                                        const eV3DProjTest flag)
{
  float tvec[2];
  eV3DProjStatus ret = ed_view3d_project__internal(region, perspmat, is_local, co, tvec, flag);
  if (ret == V3D_PROJ_RET_OK) {
    if ((tvec[0] > -2140000000.0f && tvec[0] < 2140000000.0f) &&
        (tvec[1] > -2140000000.0f && tvec[1] < 2140000000.0f))
    {
      r_co[0] = int(floorf(tvec[0]));
      r_co[1] = int(floorf(tvec[1]));
    }
    else {
      ret = V3D_PROJ_RET_OVERFLOW;
    }
  }
  return ret;
}

eV3DProjStatus ED_view3d_project_float_ex(const ARegion *region,
                                          float perspmat[4][4],
                                          const bool is_local,
                                          const float co[3],
                                          float r_co[2],
                                          const eV3DProjTest flag)
{
  float tvec[2];
  eV3DProjStatus ret = ed_view3d_project__internal(region, perspmat, is_local, co, tvec, flag);
  if (ret == V3D_PROJ_RET_OK) {
    if (isfinite(tvec[0]) && isfinite(tvec[1])) {
      copy_v2_v2(r_co, tvec);
    }
    else {
      ret = V3D_PROJ_RET_OVERFLOW;
    }
  }
  return ret;
}

eV3DProjStatus ED_view3d_project_short_global(const ARegion *region,
                                              const float co[3],
                                              short r_co[2],
                                              const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  return ED_view3d_project_short_ex(region, rv3d->persmat, false, co, r_co, flag);
}
eV3DProjStatus ED_view3d_project_short_object(const ARegion *region,
                                              const float co[3],
                                              short r_co[2],
                                              const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ED_view3d_check_mats_rv3d(rv3d);
  return ED_view3d_project_short_ex(region, rv3d->persmatob, true, co, r_co, flag);
}

eV3DProjStatus ED_view3d_project_int_global(const ARegion *region,
                                            const float co[3],
                                            int r_co[2],
                                            const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  return ED_view3d_project_int_ex(region, rv3d->persmat, false, co, r_co, flag);
}
eV3DProjStatus ED_view3d_project_int_object(const ARegion *region,
                                            const float co[3],
                                            int r_co[2],
                                            const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ED_view3d_check_mats_rv3d(rv3d);
  return ED_view3d_project_int_ex(region, rv3d->persmatob, true, co, r_co, flag);
}

eV3DProjStatus ED_view3d_project_float_global(const ARegion *region,
                                              const float co[3],
                                              float r_co[2],
                                              const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  return ED_view3d_project_float_ex(region, rv3d->persmat, false, co, r_co, flag);
}
eV3DProjStatus ED_view3d_project_float_object(const ARegion *region,
                                              const float co[3],
                                              float r_co[2],
                                              const eV3DProjTest flag)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ED_view3d_check_mats_rv3d(rv3d);
  return ED_view3d_project_float_ex(region, rv3d->persmatob, true, co, r_co, flag);
}

/* More Generic Window/Ray/Vector projection functions
 * *************************************************** */

float ED_view3d_pixel_size(const RegionView3D *rv3d, const float co[3])
{
  return mul_project_m4_v3_zfac(rv3d->persmat, co) * rv3d->pixsize * U.pixelsize;
}

float ED_view3d_pixel_size_no_ui_scale(const RegionView3D *rv3d, const float co[3])
{
  return mul_project_m4_v3_zfac(rv3d->persmat, co) * rv3d->pixsize;
}

float ED_view3d_calc_zfac_ex(const RegionView3D *rv3d, const float co[3], bool *r_flip)
{
  float zfac = mul_project_m4_v3_zfac(rv3d->persmat, co);

  if (r_flip) {
    *r_flip = (zfac < 0.0f);
  }

  /* if x,y,z is exactly the viewport offset, zfac is 0 and we don't want that
   * (accounting for near zero values) */
  if (zfac < 1.e-6f && zfac > -1.e-6f) {
    zfac = 1.0f;
  }

  /* Negative zfac means x, y, z was behind the camera (in perspective).
   * This gives flipped directions, so revert back to ok default case. */
  if (zfac < 0.0f) {
    zfac = -zfac;
  }

  return zfac;
}

float ED_view3d_calc_zfac(const RegionView3D *rv3d, const float co[3])
{
  return ED_view3d_calc_zfac_ex(rv3d, co, nullptr);
}

float ED_view3d_calc_depth_for_comparison(const RegionView3D *rv3d, const float co[3])
{
  if (rv3d->is_persp) {
    return mul_project_m4_v3_zfac(rv3d->persmat, co);
  }
  return -dot_v3v3(rv3d->viewinv[2], co);
}

static void view3d_win_to_ray_segment(const Depsgraph *depsgraph,
                                      const ARegion *region,
                                      const View3D *v3d,
                                      const float mval[2],
                                      float r_ray_co[3],
                                      float r_ray_dir[3],
                                      float r_ray_start[3],
                                      float r_ray_end[3])
{
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);
  float _ray_co[3], _ray_dir[3], start_offset, end_offset;

  if (!r_ray_co) {
    r_ray_co = _ray_co;
  }
  if (!r_ray_dir) {
    r_ray_dir = _ray_dir;
  }

  ED_view3d_win_to_origin(region, mval, r_ray_co);
  ED_view3d_win_to_vector(region, mval, r_ray_dir);

  if ((rv3d->is_persp == false) && (rv3d->persp != RV3D_CAMOB)) {
    end_offset = v3d->clip_end / 2.0f;
    start_offset = -end_offset;
  }
  else {
    ED_view3d_clip_range_get(depsgraph, v3d, rv3d, false, &start_offset, &end_offset);
  }

  if (r_ray_start) {
    madd_v3_v3v3fl(r_ray_start, r_ray_co, r_ray_dir, start_offset);
  }
  if (r_ray_end) {
    madd_v3_v3v3fl(r_ray_end, r_ray_co, r_ray_dir, end_offset);
  }
}

bool ED_view3d_clip_segment(const RegionView3D *rv3d, float ray_start[3], float ray_end[3])
{
  if ((rv3d->rflag & RV3D_CLIPPING) &&
      (clip_segment_v3_plane_n(ray_start, ray_end, rv3d->clip, 6, ray_start, ray_end) == false))
  {
    return false;
  }
  return true;
}

bool ED_view3d_win_to_ray_clipped_ex(Depsgraph *depsgraph,
                                     const ARegion *region,
                                     const View3D *v3d,
                                     const float mval[2],
                                     const bool do_clip_planes,
                                     float r_ray_co[3],
                                     float r_ray_normal[3],
                                     float r_ray_start[3],
                                     float r_ray_end[3])
{
  view3d_win_to_ray_segment(
      depsgraph, region, v3d, mval, r_ray_co, r_ray_normal, r_ray_start, r_ray_end);

  /* bounds clipping */
  if (do_clip_planes) {
    return ED_view3d_clip_segment(
        static_cast<const RegionView3D *>(region->regiondata), r_ray_start, r_ray_end);
  }

  return true;
}

bool ED_view3d_win_to_ray_clipped(Depsgraph *depsgraph,
                                  const ARegion *region,
                                  const View3D *v3d,
                                  const float mval[2],
                                  float r_ray_start[3],
                                  float r_ray_normal[3],
                                  const bool do_clip_planes)
{
  float ray_end_dummy[3];
  return ED_view3d_win_to_ray_clipped_ex(depsgraph,
                                         region,
                                         v3d,
                                         mval,
                                         do_clip_planes,
                                         nullptr,
                                         r_ray_normal,
                                         r_ray_start,
                                         ray_end_dummy);
}

void ED_view3d_win_to_ray(const ARegion *region,
                          const float mval[2],
                          float r_ray_start[3],
                          float r_ray_normal[3])
{
  ED_view3d_win_to_origin(region, mval, r_ray_start);
  ED_view3d_win_to_vector(region, mval, r_ray_normal);
}

void ED_view3d_global_to_vector(const RegionView3D *rv3d, const float coord[3], float r_out[3])
{
  if (rv3d->is_persp) {
    float p1[4], p2[4];

    copy_v3_v3(p1, coord);
    p1[3] = 1.0f;
    copy_v3_v3(p2, p1);
    p2[3] = 1.0f;
    mul_m4_v4(rv3d->viewmat, p2);

    mul_v3_fl(p2, 2.0f);

    mul_m4_v4(rv3d->viewinv, p2);

    sub_v3_v3v3(r_out, p1, p2);
  }
  else {
    copy_v3_v3(r_out, rv3d->viewinv[2]);
  }
  normalize_v3(r_out);
}

/* very similar to ED_view3d_win_to_3d() but has no advantage, de-duplicating */
#if 0
bool view3d_get_view_aligned_coordinate(ARegion *region,
                                        float fp[3],
                                        const int mval[2],
                                        const bool do_fallback)
{
  RegionView3D *rv3d = region->regiondata;
  float dvec[3];
  int mval_cpy[2];
  eV3DProjStatus ret;

  ret = ED_view3d_project_int_global(region, fp, mval_cpy, V3D_PROJ_TEST_NOP);

  if (ret == V3D_PROJ_RET_OK) {
    const float mval_f[2] = {float(mval_cpy[0] - mval[0]), float(mval_cpy[1] - mval[1])};
    const float zfac = ED_view3d_calc_zfac(rv3d, fp);
    ED_view3d_win_to_delta(region, mval_f, zfac, dvec);
    sub_v3_v3(fp, dvec);

    return true;
  }
  else {
    /* fallback to the view center */
    if (do_fallback) {
      negate_v3_v3(fp, rv3d->ofs);
      return view3d_get_view_aligned_coordinate(region, fp, mval, false);
    }
    else {
      return false;
    }
  }
}
#endif

void ED_view3d_win_to_3d(const View3D *v3d,
                         const ARegion *region,
                         const float depth_pt[3],
                         const float mval[2],
                         float r_out[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  float ray_origin[3];
  float ray_direction[3];
  float lambda;

  if (rv3d->is_persp) {
    copy_v3_v3(ray_origin, rv3d->viewinv[3]);
    ED_view3d_win_to_vector(region, mval, ray_direction);

    /* NOTE: we could use #isect_line_plane_v3()
     * however we want the intersection to be in front of the view no matter what,
     * so apply the unsigned factor instead. */
    isect_ray_plane_v3_factor(ray_origin, ray_direction, depth_pt, rv3d->viewinv[2], &lambda);

    lambda = fabsf(lambda);
  }
  else {
    float dx = (2.0f * mval[0] / float(region->winx)) - 1.0f;
    float dy = (2.0f * mval[1] / float(region->winy)) - 1.0f;

    if (rv3d->persp == RV3D_CAMOB) {
      /* ortho camera needs offset applied */
      const Camera *cam = static_cast<const Camera *>(v3d->camera->data);
      const int sensor_fit = BKE_camera_sensor_fit(cam->sensor_fit, region->winx, region->winy);
      const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom) * 4.0f;
      const float aspx = region->winx / float(region->winy);
      const float aspy = region->winy / float(region->winx);
      const float shiftx = cam->shiftx * 0.5f *
                           (sensor_fit == CAMERA_SENSOR_FIT_HOR ? 1.0f : aspy);
      const float shifty = cam->shifty * 0.5f *
                           (sensor_fit == CAMERA_SENSOR_FIT_HOR ? aspx : 1.0f);

      dx += (rv3d->camdx + shiftx) * zoomfac;
      dy += (rv3d->camdy + shifty) * zoomfac;
    }
    ray_origin[0] = (rv3d->persinv[0][0] * dx) + (rv3d->persinv[1][0] * dy) + rv3d->viewinv[3][0];
    ray_origin[1] = (rv3d->persinv[0][1] * dx) + (rv3d->persinv[1][1] * dy) + rv3d->viewinv[3][1];
    ray_origin[2] = (rv3d->persinv[0][2] * dx) + (rv3d->persinv[1][2] * dy) + rv3d->viewinv[3][2];

    copy_v3_v3(ray_direction, rv3d->viewinv[2]);
    lambda = ray_point_factor_v3(depth_pt, ray_origin, ray_direction);
  }

  madd_v3_v3v3fl(r_out, ray_origin, ray_direction, lambda);
}

void ED_view3d_win_to_3d_with_shift(const View3D *v3d,
                                    const ARegion *region,
                                    const float depth_pt[3],
                                    const float mval[2],
                                    float r_out[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  float ray_origin[3];
  float ray_direction[3];
  float lambda;

  if (rv3d->is_persp) {
    copy_v3_v3(ray_origin, rv3d->viewinv[3]);
    ED_view3d_win_to_vector(region, mval, ray_direction);

    /* NOTE: we could use #isect_line_plane_v3()
     * however we want the intersection to be in front of the view no matter what,
     * so apply the unsigned factor instead. */
    isect_ray_plane_v3_factor(ray_origin, ray_direction, depth_pt, rv3d->viewinv[2], &lambda);

    lambda = fabsf(lambda);
  }
  else {
    float dx = (2.0f * mval[0] / float(region->winx)) - 1.0f;
    float dy = (2.0f * mval[1] / float(region->winy)) - 1.0f;

    if (rv3d->persp == RV3D_CAMOB) {
      /* ortho camera needs offset applied */
      const Camera *cam = static_cast<const Camera *>(v3d->camera->data);
      const int sensor_fit = BKE_camera_sensor_fit(cam->sensor_fit, region->winx, region->winy);
      const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom) * 4.0f;
      const float aspx = region->winx / float(region->winy);
      const float aspy = region->winy / float(region->winx);
      const float shiftx = cam->shiftx * 0.5f *
                           (sensor_fit == CAMERA_SENSOR_FIT_HOR ? 1.0f : aspy);
      const float shifty = cam->shifty * 0.5f *
                           (sensor_fit == CAMERA_SENSOR_FIT_HOR ? aspx : 1.0f);

      dx += (rv3d->camdx + shiftx) * zoomfac;
      dy += (rv3d->camdy + shifty) * zoomfac;
    }
    ray_origin[0] = (rv3d->persinv[0][0] * dx) + (rv3d->persinv[1][0] * dy) + rv3d->persinv[3][0];
    ray_origin[1] = (rv3d->persinv[0][1] * dx) + (rv3d->persinv[1][1] * dy) + rv3d->persinv[3][1];
    ray_origin[2] = (rv3d->persinv[0][2] * dx) + (rv3d->persinv[1][2] * dy) + rv3d->persinv[3][2];

    copy_v3_v3(ray_direction, rv3d->viewinv[2]);
    lambda = ray_point_factor_v3(depth_pt, ray_origin, ray_direction);
  }

  madd_v3_v3v3fl(r_out, ray_origin, ray_direction, lambda);
}

void ED_view3d_win_to_3d_int(const View3D *v3d,
                             const ARegion *region,
                             const float depth_pt[3],
                             const int mval[2],
                             float r_out[3])
{
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  ED_view3d_win_to_3d(v3d, region, depth_pt, mval_fl, r_out);
}

bool ED_view3d_win_to_3d_on_plane(const ARegion *region,
                                  const float plane[4],
                                  const float mval[2],
                                  const bool do_clip,
                                  float r_out[3])
{
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);
  const bool ray_co_is_centered = rv3d->is_persp == false && rv3d->persp != RV3D_CAMOB;
  const bool do_clip_ray_plane = do_clip && !ray_co_is_centered;
  float ray_co[3], ray_no[3];
  ED_view3d_win_to_origin(region, mval, ray_co);
  ED_view3d_win_to_vector(region, mval, ray_no);
  float lambda;
  if (isect_ray_plane_v3(ray_co, ray_no, plane, &lambda, do_clip_ray_plane)) {
    madd_v3_v3v3fl(r_out, ray_co, ray_no, lambda);

    /* Handle clipping with an orthographic view differently,
     * check if the resulting point is behind the view instead of clipping the ray. */
    if (do_clip && (do_clip_ray_plane == false)) {
      /* The offset is unit length where over 1.0 is beyond the views clip-plane (near and far)
       * as non-camera orthographic views only use far distance in both directions.
       * Multiply `r_out` by `persmat` (with translation), and get it's Z value. */
      const float z_offset = fabsf(dot_m4_v3_row_z(rv3d->persmat, r_out) + rv3d->persmat[3][2]);
      if (z_offset > 1.0f) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool ED_view3d_win_to_3d_on_plane_int(const ARegion *region,
                                      const float plane[4],
                                      const int mval[2],
                                      const bool do_clip,
                                      float r_out[3])
{
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  return ED_view3d_win_to_3d_on_plane(region, plane, mval_fl, do_clip, r_out);
}

bool ED_view3d_win_to_3d_on_plane_with_fallback(const ARegion *region,
                                                const float plane[4],
                                                const float mval[2],
                                                const bool do_clip,
                                                const float plane_fallback[4],
                                                float r_out[3])
{
  float isect_co[3], isect_no[3];
  if (!isect_plane_plane_v3(plane, plane_fallback, isect_co, isect_no)) {
    return false;
  }
  normalize_v3(isect_no);

  /* Construct matrix to transform `plane_fallback` onto `plane`. */
  float mat4[4][4];
  {
    float mat3[3][3];
    rotation_between_vecs_to_mat3(mat3, plane, plane_fallback);
    copy_m4_m3(mat4, mat3);
    transform_pivot_set_m4(mat4, isect_co);
  }

  float co[3];
  if (!ED_view3d_win_to_3d_on_plane(region, plane_fallback, mval, do_clip, co)) {
    return false;
  }
  mul_m4_v3(mat4, co);

  /* While the point is already on the plane, there may be some small in-precision
   * so ensure the point is exactly on the plane. */
  closest_to_plane_v3(r_out, plane, co);

  return true;
}

void ED_view3d_win_to_delta(
    const ARegion *region, const float xy_delta[2], const float zfac, float r_out[3], bool precise)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  float dx, dy;

  dx = 2.0f * xy_delta[0] * zfac / region->winx;
  dy = 2.0f * xy_delta[1] * zfac / region->winy;

  if (precise) {
    /* Fix for operators that needs more precision. (see #103499) */
    float wininv[4][4];
    invert_m4_m4(wininv, rv3d->winmat);
    r_out[0] = (wininv[0][0] * dx + wininv[1][0] * dy);
    r_out[1] = (wininv[0][1] * dx + wininv[1][1] * dy);
    r_out[2] = (wininv[0][2] * dx + wininv[1][2] * dy);
    mul_mat3_m4_v3(rv3d->viewinv, r_out);
  }
  else {
    r_out[0] = (rv3d->persinv[0][0] * dx + rv3d->persinv[1][0] * dy);
    r_out[1] = (rv3d->persinv[0][1] * dx + rv3d->persinv[1][1] * dy);
    r_out[2] = (rv3d->persinv[0][2] * dx + rv3d->persinv[1][2] * dy);
  }
}

void ED_view3d_win_to_origin(const ARegion *region, const float mval[2], float r_out[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (rv3d->is_persp) {
    copy_v3_v3(r_out, rv3d->viewinv[3]);
  }
  else {
    r_out[0] = 2.0f * mval[0] / region->winx - 1.0f;
    r_out[1] = 2.0f * mval[1] / region->winy - 1.0f;

    if (rv3d->persp == RV3D_CAMOB) {
      r_out[2] = -1.0f;
    }
    else {
      r_out[2] = 0.0f;
    }

    mul_project_m4_v3(rv3d->persinv, r_out);
  }
}

void ED_view3d_win_to_vector(const ARegion *region, const float mval[2], float r_out[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  if (rv3d->is_persp) {
    r_out[0] = 2.0f * (mval[0] / region->winx) - 1.0f;
    r_out[1] = 2.0f * (mval[1] / region->winy) - 1.0f;
    r_out[2] = -0.5f;
    mul_project_m4_v3(rv3d->persinv, r_out);
    sub_v3_v3(r_out, rv3d->viewinv[3]);
  }
  else {
    negate_v3_v3(r_out, rv3d->viewinv[2]);
  }
  normalize_v3(r_out);
}

bool ED_view3d_win_to_segment_clipped(const Depsgraph *depsgraph,
                                      const ARegion *region,
                                      const View3D *v3d,
                                      const float mval[2],
                                      float r_ray_start[3],
                                      float r_ray_end[3],
                                      const bool do_clip_planes)
{
  view3d_win_to_ray_segment(
      depsgraph, region, v3d, mval, nullptr, nullptr, r_ray_start, r_ray_end);

  /* bounds clipping */
  if (do_clip_planes) {
    return ED_view3d_clip_segment((RegionView3D *)region->regiondata, r_ray_start, r_ray_end);
  }

  return true;
}

/* -------------------------------------------------------------------- */
/** \name Utility functions for projection
 * \{ */

blender::float4x4 ED_view3d_ob_project_mat_get(const RegionView3D *rv3d, const Object *ob)
{
  float vmat[4][4];
  blender::float4x4 pmat;

  mul_m4_m4m4(vmat, rv3d->viewmat, ob->object_to_world().ptr());
  mul_m4_m4m4(pmat.ptr(), rv3d->winmat, vmat);
  return pmat;
}

blender::float4x4 ED_view3d_ob_project_mat_get_from_obmat(const RegionView3D *rv3d,
                                                          const blender::float4x4 &obmat)
{
  return blender::float4x4_view(rv3d->winmat) * blender::float4x4_view(rv3d->viewmat) * obmat;
}

void ED_view3d_project_v3(const ARegion *region, const float world[3], float r_region_co[3])
{
  /* Viewport is set up to make coordinates relative to the region, not window. */
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const int viewport[4] = {0, 0, region->winx, region->winy};
  GPU_matrix_project_3fv(world, rv3d->viewmat, rv3d->winmat, viewport, r_region_co);
}

void ED_view3d_project_v2(const ARegion *region, const float world[3], float r_region_co[2])
{
  /* Viewport is set up to make coordinates relative to the region, not window. */
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const int viewport[4] = {0, 0, region->winx, region->winy};
  GPU_matrix_project_2fv(world, rv3d->viewmat, rv3d->winmat, viewport, r_region_co);
}

bool ED_view3d_unproject_v3(
    const ARegion *region, float regionx, float regiony, float regionz, float world[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const int viewport[4] = {0, 0, region->winx, region->winy};
  const float region_co[3] = {regionx, regiony, regionz};

  return GPU_matrix_unproject_3fv(region_co, rv3d->viewinv, rv3d->winmat, viewport, world);
}

/** \} */
