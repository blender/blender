/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * 3D View checks and manipulation (no operators).
 */

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "RNA_path.hh"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_matrix.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_keyframing.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_view3d.hh"

#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "UI_resources.hh"

#include "view3d_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Data Access Utilities
 * \{ */

void ED_view3d_background_color_get(const Scene *scene, const View3D *v3d, float r_color[3])
{
  if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD) {
    if (scene->world) {
      copy_v3_v3(r_color, &scene->world->horr);
      return;
    }
  }
  else if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_VIEWPORT) {
    copy_v3_v3(r_color, v3d->shading.background_color);
    return;
  }

  UI_GetThemeColor3fv(TH_BACK, r_color);
}

void ED_view3d_text_colors_get(const Scene *scene,
                               const View3D *v3d,
                               float r_text_color[4],
                               float r_shadow_color[4])
{
  /* Text fully opaque, shadow slightly transparent. */
  r_text_color[3] = 1.0f;
  r_shadow_color[3] = 0.8f;

  /* Default text color from TH_TEXT_HI. If it is too close
   * to the background color, darken or lighten it. */
  UI_GetThemeColor3fv(TH_TEXT_HI, r_text_color);
  float text_lightness = srgb_to_grayscale(r_text_color);
  float bg_color[3];
  ED_view3d_background_color_get(scene, v3d, bg_color);
  const float distance = len_v3v3(r_text_color, bg_color);
  if (distance < 0.5f) {
    if (text_lightness > 0.5f) {
      mul_v3_fl(r_text_color, 0.33f);
    }
    else {
      mul_v3_fl(r_text_color, 3.0f);
    }
    clamp_v3(r_text_color, 0.0f, 1.0f);
  }

  /* Shadow color is black or white depending on final text lightness. */
  text_lightness = srgb_to_grayscale(r_text_color);
  if (text_lightness > 0.4f) {
    copy_v3_fl(r_shadow_color, 0.0f);
  }
  else {
    copy_v3_fl(r_shadow_color, 1.0f);
  }
}

bool ED_view3d_has_workbench_in_texture_color(const Scene *scene,
                                              const Object *ob,
                                              const View3D *v3d)
{
  if (v3d->shading.type == OB_SOLID) {
    if (v3d->shading.color_type == V3D_SHADING_TEXTURE_COLOR) {
      return true;
    }
    if (ob && ob->mode == OB_MODE_TEXTURE_PAINT) {
      return true;
    }
  }
  else if (v3d->shading.type == OB_RENDER) {
    if (BKE_scene_uses_blender_workbench(scene)) {
      return scene->display.shading.color_type == V3D_SHADING_TEXTURE_COLOR;
    }
  }
  return false;
}

Camera *ED_view3d_camera_data_get(View3D *v3d, RegionView3D *rv3d)
{
  /* establish the camera object,
   * so we can default to view mapping if anything is wrong with it */
  if ((rv3d->persp == RV3D_CAMOB) && v3d->camera && (v3d->camera->type == OB_CAMERA)) {
    return static_cast<Camera *>(v3d->camera->data);
  }
  return nullptr;
}

float ED_view3d_dist_soft_min_get(const View3D *v3d, const bool use_persp_range)
{
  return use_persp_range ? (v3d->clip_start * 1.5f) : v3d->grid * 0.001f;
}

blender::Bounds<float> ED_view3d_dist_soft_range_get(const View3D *v3d, const bool use_persp_range)
{
  return {
      ED_view3d_dist_soft_min_get(v3d, use_persp_range),
      v3d->clip_end * 10.0f,
  };
}

bool ED_view3d_clip_range_get(const Depsgraph *depsgraph,
                              const View3D *v3d,
                              const RegionView3D *rv3d,
                              const bool use_ortho_factor,
                              float *r_clip_start,
                              float *r_clip_end)
{
  CameraParams params;

  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);

  if (use_ortho_factor && params.is_ortho) {
    const float fac = 2.0f / (params.clip_end - params.clip_start);
    params.clip_start *= fac;
    params.clip_end *= fac;
  }

  if (r_clip_start) {
    *r_clip_start = params.clip_start;
  }
  if (r_clip_end) {
    *r_clip_end = params.clip_end;
  }

  return params.is_ortho;
}

bool ED_view3d_viewplane_get(const Depsgraph *depsgraph,
                             const View3D *v3d,
                             const RegionView3D *rv3d,
                             int winx,
                             int winy,
                             rctf *r_viewplane,
                             float *r_clip_start,
                             float *r_clip_end,
                             float *r_pixsize)
{
  CameraParams params;

  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);
  BKE_camera_params_compute_viewplane(&params, winx, winy, 1.0f, 1.0f);

  if (r_viewplane) {
    *r_viewplane = params.viewplane;
  }
  if (r_clip_start) {
    *r_clip_start = params.clip_start;
  }
  if (r_clip_end) {
    *r_clip_end = params.clip_end;
  }
  if (r_pixsize) {
    *r_pixsize = params.viewdx;
  }

  return params.is_ortho;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View State/Context Utilities
 * \{ */

void view3d_operator_needs_gpu(const bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  view3d_region_operator_needs_gpu(region);
}

void view3d_region_operator_needs_gpu(ARegion *region)
{
  /* for debugging purpose, context should always be OK */
  if ((region == nullptr) || (region->regiontype != RGN_TYPE_WINDOW)) {
    printf("view3d_region_operator_needs_gpu error, wrong region\n");
  }
  else {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

    wmViewport(&region->winrct); /* TODO: bad */
    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);
  }
}

void ED_view3d_polygon_offset(const RegionView3D *rv3d, const float dist)
{
  if (rv3d->rflag & RV3D_ZOFFSET_DISABLED) {
    return;
  }

  float viewdist = rv3d->dist;

  /* Special exception for orthographic camera (`viewdist` isn't used for perspective cameras). */
  if (dist != 0.0f) {
    if (rv3d->persp == RV3D_CAMOB) {
      if (rv3d->is_persp == false) {
        viewdist = 1.0f / max_ff(fabsf(rv3d->winmat[0][0]), fabsf(rv3d->winmat[1][1]));
      }
    }
  }

  GPU_polygon_offset(viewdist, dist);
}

bool ED_view3d_context_activate(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);

  /* area can be nullptr when called from python */
  if (area == nullptr || area->spacetype != SPACE_VIEW3D) {
    area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
  }

  if (area == nullptr) {
    return false;
  }

  ARegion *region = BKE_area_find_region_active_win(area);
  if (region == nullptr) {
    return false;
  }

  /* Bad context switch. */
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Clipping Utilities
 * \{ */

void ED_view3d_clipping_calc_from_boundbox(float clip[4][4],
                                           const BoundBox *bb,
                                           const bool is_flip)
{
  for (int val = 0; val < 4; val++) {
    normal_tri_v3(clip[val], bb->vec[val], bb->vec[val == 3 ? 0 : val + 1], bb->vec[val + 4]);
    if (UNLIKELY(is_flip)) {
      negate_v3(clip[val]);
    }

    clip[val][3] = -dot_v3v3(clip[val], bb->vec[val]);
  }
}

void ED_view3d_clipping_calc(
    BoundBox *bb, float planes[4][4], const ARegion *region, const Object *ob, const rcti *rect)
{
  /* init in case unproject fails */
  memset(bb->vec, 0, sizeof(bb->vec));

  /* four clipping planes and bounding volume */
  /* first do the bounding volume */
  for (int val = 0; val < 4; val++) {
    float xs = ELEM(val, 0, 3) ? rect->xmin : rect->xmax;
    float ys = ELEM(val, 0, 1) ? rect->ymin : rect->ymax;

    ED_view3d_unproject_v3(region, xs, ys, 0.0, bb->vec[val]);
    ED_view3d_unproject_v3(region, xs, ys, 1.0, bb->vec[4 + val]);
  }

  /* optionally transform to object space */
  if (ob) {
    float imat[4][4];
    invert_m4_m4(imat, ob->object_to_world().ptr());

    for (int val = 0; val < 8; val++) {
      mul_m4_v3(imat, bb->vec[val]);
    }
  }

  /* verify if we have negative scale. doing the transform before cross
   * product flips the sign of the vector compared to doing cross product
   * before transform then, so we correct for that. */
  int flip_sign = (ob) ? is_negative_m4(ob->object_to_world().ptr()) : false;

  ED_view3d_clipping_calc_from_boundbox(planes, bb, flip_sign);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Clipping Clamp Min/Max
 * \{ */

struct PointsInPlanesMinMax_UserData {
  float min[3];
  float max[3];
};

static void points_in_planes_minmax_fn(
    const float co[3], int /*i*/, int /*j*/, int /*k*/, void *user_data_p)
{
  PointsInPlanesMinMax_UserData *user_data = static_cast<PointsInPlanesMinMax_UserData *>(
      user_data_p);
  minmax_v3v3_v3(user_data->min, user_data->max, co);
}

bool ED_view3d_clipping_clamp_minmax(const RegionView3D *rv3d, float min[3], float max[3])
{
  /* 6 planes for the cube, 4..6 for the current view clipping planes. */
  float planes[6 + 6][4];

  /* Convert the min/max to 6 planes. */
  for (int i = 0; i < 3; i++) {
    float *plane_min = planes[(i * 2) + 0];
    float *plane_max = planes[(i * 2) + 1];
    zero_v3(plane_min);
    zero_v3(plane_max);
    plane_min[i] = -1.0f;
    plane_min[3] = +min[i];
    plane_max[i] = +1.0f;
    plane_max[3] = -max[i];
  }

  /* Copy planes from the viewport & flip. */
  int planes_len = 6;
  int clip_len = (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) ? 4 : 6;
  for (int i = 0; i < clip_len; i++) {
    negate_v4_v4(planes[planes_len], rv3d->clip[i]);
    planes_len += 1;
  }

  /* Calculate points intersecting all planes (effectively intersecting two bounding boxes). */
  PointsInPlanesMinMax_UserData user_data;
  INIT_MINMAX(user_data.min, user_data.max);

  const float eps_coplanar = 1e-4f;
  const float eps_isect = 1e-6f;
  if (isect_planes_v3_fn(
          planes, planes_len, eps_coplanar, eps_isect, points_in_planes_minmax_fn, &user_data))
  {
    copy_v3_v3(min, user_data.min);
    copy_v3_v3(max, user_data.max);
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Bound-Box Utilities
 * \{ */

static bool view3d_boundbox_clip_m4(const BoundBox *bb, const float persmatob[4][4])
{
  int a, flag = -1, fl;

  for (a = 0; a < 8; a++) {
    float vec[4], min, max;
    copy_v3_v3(vec, bb->vec[a]);
    vec[3] = 1.0;
    mul_m4_v4(persmatob, vec);
    max = vec[3];
    min = -vec[3];

    fl = 0;
    if (vec[0] < min) {
      fl += 1;
    }
    if (vec[0] > max) {
      fl += 2;
    }
    if (vec[1] < min) {
      fl += 4;
    }
    if (vec[1] > max) {
      fl += 8;
    }
    if (vec[2] < min) {
      fl += 16;
    }
    if (vec[2] > max) {
      fl += 32;
    }

    flag &= fl;
    if (flag == 0) {
      return true;
    }
  }

  return false;
}

bool ED_view3d_boundbox_clip_ex(const RegionView3D *rv3d, const BoundBox *bb, float obmat[4][4])
{
  /* return 1: draw */

  float persmatob[4][4];

  if (bb == nullptr) {
    return true;
  }

  mul_m4_m4m4(persmatob, (float (*)[4])rv3d->persmat, obmat);

  return view3d_boundbox_clip_m4(bb, persmatob);
}

bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const BoundBox *bb)
{
  if (bb == nullptr) {
    return true;
  }
  return view3d_boundbox_clip_m4(bb, rv3d->persmatob);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Perspective & Mode Switching
 *
 * Misc view utility functions.
 * \{ */

bool ED_view3d_offset_lock_check(const View3D *v3d, const RegionView3D *rv3d)
{
  return (rv3d->persp != RV3D_CAMOB) && (v3d->ob_center_cursor || v3d->ob_center);
}

void ED_view3d_lastview_store(RegionView3D *rv3d)
{
  copy_qt_qt(rv3d->lviewquat, rv3d->viewquat);
  rv3d->lview = rv3d->view;
  rv3d->lview_axis_roll = rv3d->view_axis_roll;
  if (rv3d->persp != RV3D_CAMOB) {
    rv3d->lpersp = rv3d->persp;
  }
}

void ED_view3d_lock_clear(View3D *v3d)
{
  v3d->ob_center = nullptr;
  v3d->ob_center_bone[0] = '\0';
  v3d->ob_center_cursor = false;

  v3d->flag2 &= ~V3D_LOCK_CAMERA;
}

void ED_view3d_persp_switch_from_camera(const Depsgraph *depsgraph,
                                        View3D *v3d,
                                        RegionView3D *rv3d,
                                        const char persp)
{
  BLI_assert(rv3d->persp == RV3D_CAMOB);
  BLI_assert(persp != RV3D_CAMOB);

  if (v3d->camera) {
    Object *ob_camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
    rv3d->dist = ED_view3d_offset_distance(
        ob_camera_eval->object_to_world().ptr(), rv3d->ofs, VIEW3D_DIST_FALLBACK);
    ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, nullptr);
    WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, v3d);
  }

  if (!ED_view3d_camera_lock_check(v3d, rv3d)) {
    rv3d->persp = persp;
  }
}
bool ED_view3d_persp_ensure(const Depsgraph *depsgraph, View3D *v3d, ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const bool autopersp = (U.uiflag & USER_AUTOPERSP) != 0;

  BLI_assert((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0);

  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    return false;
  }

  if (rv3d->persp != RV3D_PERSP) {
    if (rv3d->persp == RV3D_CAMOB) {
      /* If autopersp and previous view was an axis one,
       * switch back to PERSP mode, else reuse previous mode. */
      char persp = (autopersp && RV3D_VIEW_IS_AXIS(rv3d->lview)) ? char(RV3D_PERSP) : rv3d->lpersp;
      ED_view3d_persp_switch_from_camera(depsgraph, v3d, rv3d, persp);
    }
    else if (autopersp && RV3D_VIEW_IS_AXIS(rv3d->view)) {
      rv3d->persp = RV3D_PERSP;
    }
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera View Utilities
 *
 * Utilities for manipulating the camera-view.
 * \{ */

bool ED_view3d_camera_view_zoom_scale(RegionView3D *rv3d, const float scale)
{
  const float camzoom_init = rv3d->camzoom;
  float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  /* Clamp both before and after conversion to prevent NAN on negative values. */

  zoomfac = zoomfac * scale;
  CLAMP(zoomfac, RV3D_CAMZOOM_MIN_FACTOR, RV3D_CAMZOOM_MAX_FACTOR);
  rv3d->camzoom = BKE_screen_view3d_zoom_from_fac(zoomfac);
  CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
  return (rv3d->camzoom != camzoom_init);
}

bool ED_view3d_camera_view_pan(ARegion *region, const float event_ofs[2])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  const float camdxy_init[2] = {rv3d->camdx, rv3d->camdy};
  const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom) * 2.0f;
  rv3d->camdx += event_ofs[0] / (region->winx * zoomfac);
  rv3d->camdy += event_ofs[1] / (region->winy * zoomfac);
  CLAMP(rv3d->camdx, -1.0f, 1.0f);
  CLAMP(rv3d->camdy, -1.0f, 1.0f);
  return (camdxy_init[0] != rv3d->camdx) || (camdxy_init[1] != rv3d->camdy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Lock API
 *
 * Lock the camera to the 3D Viewport, allowing view manipulation to transform the camera.
 * \{ */

bool ED_view3d_camera_lock_check(const View3D *v3d, const RegionView3D *rv3d)
{
  return ((v3d->camera) && ID_IS_EDITABLE(v3d->camera) && (v3d->flag2 & V3D_LOCK_CAMERA) &&
          (rv3d->persp == RV3D_CAMOB));
}

void ED_view3d_camera_lock_init_ex(const Depsgraph *depsgraph,
                                   View3D *v3d,
                                   RegionView3D *rv3d,
                                   const bool calc_dist)
{
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    Object *ob_camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
    if (calc_dist) {
      /* using a fallback dist is OK here since ED_view3d_from_object() compensates for it */
      rv3d->dist = ED_view3d_offset_distance(
          ob_camera_eval->object_to_world().ptr(), rv3d->ofs, VIEW3D_DIST_FALLBACK);
    }
    ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, nullptr);
  }
}

void ED_view3d_camera_lock_init(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
{
  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, true);
}

bool ED_view3d_camera_lock_sync(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
{
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    ObjectTfmProtectedChannels obtfm;
    Object *root_parent;

    if (v3d->camera->transflag & OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK &&
        (root_parent = v3d->camera->parent))
    {
      Object *ob_update;
      float tmat[4][4];
      float imat[4][4];
      float view_mat[4][4];
      float diff_mat[4][4];
      float parent_mat[4][4];

      while (root_parent->parent) {
        root_parent = root_parent->parent;
      }
      Object *ob_camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
      Object *root_parent_eval = DEG_get_evaluated(depsgraph, root_parent);

      ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);

      normalize_m4_m4(tmat, ob_camera_eval->object_to_world().ptr());

      invert_m4_m4(imat, tmat);
      mul_m4_m4m4(diff_mat, view_mat, imat);

      mul_m4_m4m4(parent_mat, diff_mat, root_parent_eval->object_to_world().ptr());

      BKE_object_tfm_protected_backup(root_parent, &obtfm);
      BKE_object_apply_mat4(root_parent, parent_mat, true, false);
      BKE_object_tfm_protected_restore(root_parent, &obtfm, root_parent->protectflag);

      ob_update = v3d->camera;
      while (ob_update) {
        DEG_id_tag_update(&ob_update->id, ID_RECALC_TRANSFORM);
        WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, ob_update);
        ob_update = ob_update->parent;
      }
    }
    else {
      /* always maintain the same scale */
      const short protect_scale_all = (OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ);
      BKE_object_tfm_protected_backup(v3d->camera, &obtfm);
      ED_view3d_to_object(depsgraph, v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);
      BKE_object_tfm_protected_restore(
          v3d->camera, &obtfm, v3d->camera->protectflag | protect_scale_all);

      DEG_id_tag_update(&v3d->camera->id, ID_RECALC_TRANSFORM);
      WM_main_add_notifier(NC_OBJECT | ND_TRANSFORM, v3d->camera);
    }

    return true;
  }
  return false;
}

bool ED_view3d_camera_autokey(
    const Scene *scene, ID *id_key, bContext *C, const bool do_rotate, const bool do_translate)
{
  BLI_assert(GS(id_key->name) == ID_OB);
  using namespace blender;

  /* While `autokeyframe_object` does already call `autokeyframe_cfra_can_key` we need this here
   * because at the time of writing this it returns void. Once the keying result is returned, like
   * implemented for `blender::animrig::insert_keyframes`, this `if` can be removed. */
  if (!animrig::autokeyframe_cfra_can_key(scene, id_key)) {
    return false;
  }

  Object *camera_object = reinterpret_cast<Object *>(id_key);

  Vector<RNAPath> rna_paths;

  if (do_rotate) {
    switch (camera_object->rotmode) {
      case ROT_MODE_QUAT:
        rna_paths.append({"rotation_quaternion"});
        break;

      case ROT_MODE_AXISANGLE:
        rna_paths.append({"rotation_axis_angle"});
        break;

      case ROT_MODE_EUL:
        rna_paths.append({"rotation_euler"});
        break;

      default:
        break;
    }
  }
  if (do_translate) {
    rna_paths.append({"location"});
  }

  animrig::autokeyframe_object(C, scene, camera_object, rna_paths);
  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  return true;
}

bool ED_view3d_camera_lock_autokey(
    View3D *v3d, RegionView3D *rv3d, bContext *C, const bool do_rotate, const bool do_translate)
{
  /* similar to ED_view3d_cameracontrol_update */
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    Scene *scene = CTX_data_scene(C);
    ID *id_key;
    Object *root_parent;
    if (v3d->camera->transflag & OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK &&
        (root_parent = v3d->camera->parent))
    {
      while (root_parent->parent) {
        root_parent = root_parent->parent;
      }
      id_key = &root_parent->id;
    }
    else {
      id_key = &v3d->camera->id;
    }

    return ED_view3d_camera_autokey(scene, id_key, C, do_rotate, do_translate);
  }
  return false;
}

bool ED_view3d_camera_lock_undo_test(const View3D *v3d, const RegionView3D *rv3d, bContext *C)
{
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    if (ED_undo_is_memfile_compatible(C)) {
      return true;
    }
  }
  return false;
}

/**
 * Create a MEMFILE undo-step for locked camera movement when transforming the view.
 * Edit and texture paint mode don't use MEMFILE undo so undo push is skipped for them.
 * NDOF and trackpad navigation would create an undo step on every gesture and we may end up with
 * unnecessary undo steps so undo push for them is not supported for now.
 * Operators that use smooth view for navigation are supported via an optional parameter field,
 * see: #V3D_SmoothParams.undo_str.
 */
static bool view3d_camera_lock_undo_ex(const char *str,
                                       const View3D *v3d,
                                       const RegionView3D *rv3d,
                                       bContext *C,
                                       const bool undo_group)
{
  if (ED_view3d_camera_lock_undo_test(v3d, rv3d, C)) {
    if (undo_group) {
      ED_undo_grouped_push(C, str);
    }
    else {
      ED_undo_push(C, str);
    }
    return true;
  }
  return false;
}

bool ED_view3d_camera_lock_undo_push(const char *str,
                                     const View3D *v3d,
                                     const RegionView3D *rv3d,
                                     bContext *C)
{
  return view3d_camera_lock_undo_ex(str, v3d, rv3d, C, false);
}

bool ED_view3d_camera_lock_undo_grouped_push(const char *str,
                                             const View3D *v3d,
                                             const RegionView3D *rv3d,
                                             bContext *C)
{
  return view3d_camera_lock_undo_ex(str, v3d, rv3d, C, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box View Support
 *
 * Use with quad-split so each view is clipped by the bounds of each view axis.
 * \{ */

static void view3d_boxview_clip(ScrArea *area)
{
  BoundBox *bb = MEM_callocN<BoundBox>("clipbb");
  float clip[6][4];
  float x1 = 0.0f, y1 = 0.0f, z1 = 0.0f, ofs[3] = {0.0f, 0.0f, 0.0f};

  /* create bounding box */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

      if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) {
        if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
          if (region->winx > region->winy) {
            x1 = rv3d->dist;
          }
          else {
            x1 = region->winx * rv3d->dist / region->winy;
          }

          if (region->winx > region->winy) {
            y1 = region->winy * rv3d->dist / region->winx;
          }
          else {
            y1 = rv3d->dist;
          }
          copy_v2_v2(ofs, rv3d->ofs);
        }
        else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
          ofs[2] = rv3d->ofs[2];

          if (region->winx > region->winy) {
            z1 = region->winy * rv3d->dist / region->winx;
          }
          else {
            z1 = rv3d->dist;
          }
        }
      }
    }
  }

  for (int val = 0; val < 8; val++) {
    if (ELEM(val, 0, 3, 4, 7)) {
      bb->vec[val][0] = -x1 - ofs[0];
    }
    else {
      bb->vec[val][0] = x1 - ofs[0];
    }

    if (ELEM(val, 0, 1, 4, 5)) {
      bb->vec[val][1] = -y1 - ofs[1];
    }
    else {
      bb->vec[val][1] = y1 - ofs[1];
    }

    if (val > 3) {
      bb->vec[val][2] = -z1 - ofs[2];
    }
    else {
      bb->vec[val][2] = z1 - ofs[2];
    }
  }

  /* normals for plane equations */
  normal_tri_v3(clip[0], bb->vec[0], bb->vec[1], bb->vec[4]);
  normal_tri_v3(clip[1], bb->vec[1], bb->vec[2], bb->vec[5]);
  normal_tri_v3(clip[2], bb->vec[2], bb->vec[3], bb->vec[6]);
  normal_tri_v3(clip[3], bb->vec[3], bb->vec[0], bb->vec[7]);
  normal_tri_v3(clip[4], bb->vec[4], bb->vec[5], bb->vec[6]);
  normal_tri_v3(clip[5], bb->vec[0], bb->vec[2], bb->vec[1]);

  /* then plane equations */
  for (int val = 0; val < 6; val++) {
    clip[val][3] = -dot_v3v3(clip[val], bb->vec[val % 5]);
  }

  /* create bounding box */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

      if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) {
        rv3d->rflag |= RV3D_CLIPPING;
        memcpy(rv3d->clip, clip, sizeof(clip));
        if (rv3d->clipbb) {
          MEM_freeN(rv3d->clipbb);
        }
        rv3d->clipbb = static_cast<BoundBox *>(MEM_dupallocN(bb));
      }
    }
  }
  MEM_freeN(bb);
}

/**
 * Find which axis values are shared between both views and copy to \a rv3d_dst
 * taking axis flipping into account.
 */
static void view3d_boxview_sync_axis(RegionView3D *rv3d_dst, RegionView3D *rv3d_src)
{
  /* absolute axis values above this are considered to be set (will be ~1.0f) */
  const float axis_eps = 0.5f;
  float viewinv[4];

  /* use the view rotation to identify which axis to sync on */
  float view_axis_all[4][3] = {
      {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

  float *view_src_x = &view_axis_all[0][0];
  float *view_src_y = &view_axis_all[1][0];

  float *view_dst_x = &view_axis_all[2][0];
  float *view_dst_y = &view_axis_all[3][0];
  int i;

  /* we could use rv3d->viewinv, but better not depend on view matrix being updated */
  if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_src->view, rv3d_src->view_axis_roll, viewinv) ==
               false))
  {
    return;
  }
  invert_qt_normalized(viewinv);
  mul_qt_v3(viewinv, view_src_x);
  mul_qt_v3(viewinv, view_src_y);

  if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_dst->view, rv3d_dst->view_axis_roll, viewinv) ==
               false))
  {
    return;
  }
  invert_qt_normalized(viewinv);
  mul_qt_v3(viewinv, view_dst_x);
  mul_qt_v3(viewinv, view_dst_y);

  /* Check source and destination have a matching axis. */
  for (i = 0; i < 3; i++) {
    if (((fabsf(view_src_x[i]) > axis_eps) || (fabsf(view_src_y[i]) > axis_eps)) &&
        ((fabsf(view_dst_x[i]) > axis_eps) || (fabsf(view_dst_y[i]) > axis_eps)))
    {
      rv3d_dst->ofs[i] = rv3d_src->ofs[i];
    }
  }
}

void view3d_boxview_sync(ScrArea *area, ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  short clip = 0;

  LISTBASE_FOREACH (ARegion *, region_test, &area->regionbase) {
    if (region_test != region && region_test->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3dtest = static_cast<RegionView3D *>(region_test->regiondata);

      if (RV3D_LOCK_FLAGS(rv3dtest) & RV3D_LOCK_ROTATION) {
        rv3dtest->dist = rv3d->dist;
        view3d_boxview_sync_axis(rv3dtest, rv3d);
        clip |= RV3D_LOCK_FLAGS(rv3dtest) & RV3D_BOXCLIP;

        ED_region_tag_redraw(region_test);
      }
    }
  }

  if (clip) {
    view3d_boxview_clip(area);
  }
}

void view3d_boxview_copy(ScrArea *area, ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  bool clip = false;

  LISTBASE_FOREACH (ARegion *, region_test, &area->regionbase) {
    if (region_test != region && region_test->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3dtest = static_cast<RegionView3D *>(region_test->regiondata);

      if (RV3D_LOCK_FLAGS(rv3dtest)) {
        rv3dtest->dist = rv3d->dist;
        copy_v3_v3(rv3dtest->ofs, rv3d->ofs);
        ED_region_tag_redraw(region_test);

        clip |= ((RV3D_LOCK_FLAGS(rv3dtest) & RV3D_BOXCLIP) != 0);
      }
    }
  }

  if (clip) {
    view3d_boxview_clip(area);
  }
}

void ED_view3d_quadview_update(ScrArea *area, ARegion *region, bool do_clip)
{
  ARegion *region_sync = nullptr;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  /* this function copies flags from the first of the 3 other quadview
   * regions to the 2 other, so it assumes this is the region whose
   * properties are always being edited, weak */
  short viewlock = rv3d->viewlock;

  if ((viewlock & RV3D_LOCK_ROTATION) == 0) {
    do_clip = (viewlock & RV3D_BOXCLIP) != 0;
    viewlock = 0;
  }
  else if ((viewlock & RV3D_BOXVIEW) == 0 && (viewlock & RV3D_BOXCLIP) != 0) {
    do_clip = true;
    viewlock &= ~RV3D_BOXCLIP;
  }

  for (; region; region = region->prev) {
    if (region->alignment == RGN_ALIGN_QSPLIT) {
      rv3d = static_cast<RegionView3D *>(region->regiondata);
      rv3d->viewlock = viewlock;

      if (do_clip && (viewlock & RV3D_BOXCLIP) == 0) {
        rv3d->rflag &= ~RV3D_BOXCLIP;
      }

      /* use region_sync so we sync with one of the aligned views below
       * else the view jumps on changing view settings like 'clip'
       * since it copies from the perspective view */
      region_sync = region;
    }
  }

  if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXVIEW) {
    view3d_boxview_sync(area,
                        static_cast<ARegion *>(region_sync ? region_sync : area->regionbase.last));
  }

  /* ensure locked regions have an axis, locked user views don't make much sense */
  if (viewlock & RV3D_LOCK_ROTATION) {
    int index_qsplit = 0;
    LISTBASE_FOREACH (ARegion *, region_iter, &area->regionbase) {
      if (region_iter->alignment == RGN_ALIGN_QSPLIT) {
        rv3d = static_cast<RegionView3D *>(region_iter->regiondata);
        if (rv3d->viewlock) {
          if (!RV3D_VIEW_IS_AXIS(rv3d->view) || (rv3d->view_axis_roll != RV3D_VIEW_AXIS_ROLL_0)) {
            rv3d->view = ED_view3d_lock_view_from_index(index_qsplit);
            rv3d->view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;
            rv3d->persp = RV3D_ORTHO;
            ED_view3d_lock(rv3d);
          }
        }
        index_qsplit++;
      }
    }
  }

  ED_area_tag_redraw(area);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Auto-Depth Last State Access
 *
 * Calling consecutive trackpad gestures reuses the previous offset to prevent
 * each trackpad event using a different offset, see: #103263.
 * \{ */

static const char *view3d_autodepth_last_id = "view3d_autodist_last";

/**
 * Auto-depth values for #ED_view3d_autodist_last_check and related functions.
 */
struct View3D_AutoDistLast {
  float ofs[3];
  bool has_depth;
};

bool ED_view3d_autodist_last_check(wmWindow *win, const wmEvent *event)
{
  if (event->flag & WM_EVENT_IS_CONSECUTIVE) {
    const View3D_AutoDistLast *autodepth_last = static_cast<const View3D_AutoDistLast *>(
        WM_event_consecutive_data_get(win, view3d_autodepth_last_id));
    if (autodepth_last) {
      return true;
    }
  }
  return false;
}

void ED_view3d_autodist_last_clear(wmWindow *win)
{
  WM_event_consecutive_data_free(win);
}

void ED_view3d_autodist_last_set(wmWindow *win,
                                 const wmEvent *event,
                                 const float ofs[3],
                                 const bool has_depth)
{
  ED_view3d_autodist_last_clear(win);

  if (WM_event_consecutive_gesture_test(event)) {
    View3D_AutoDistLast *autodepth_last = static_cast<View3D_AutoDistLast *>(
        MEM_callocN(sizeof(*autodepth_last), __func__));

    autodepth_last->has_depth = has_depth;
    copy_v3_v3(autodepth_last->ofs, ofs);

    WM_event_consecutive_data_set(win, view3d_autodepth_last_id, autodepth_last);
  }
}

bool ED_view3d_autodist_last_get(wmWindow *win, float r_ofs[3])
{
  const View3D_AutoDistLast *autodepth_last = static_cast<const View3D_AutoDistLast *>(
      WM_event_consecutive_data_get(win, view3d_autodepth_last_id));
  /* #ED_view3d_autodist_last_check should be called first. */
  BLI_assert(autodepth_last);
  if (autodepth_last == nullptr) {
    return false;
  }

  if (autodepth_last->has_depth == false) {
    zero_v3(r_ofs);
    return false;
  }
  copy_v3_v3(r_ofs, autodepth_last->ofs);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Auto-Depth Utilities
 * \{ */

static float view_autodist_depth_margin(ARegion *region, const int mval[2], int margin)
{
  rcti rect;
  if (margin == 0) {
    /* Get Z Depths, needed for perspective, nice for ortho */
    rect.xmin = mval[0];
    rect.ymin = mval[1];
    rect.xmax = mval[0] + 1;
    rect.ymax = mval[1] + 1;
  }
  else {
    BLI_rcti_init_pt_radius(&rect, mval, margin);
  }

  ViewDepths depth_temp = {0};
  view3d_depths_rect_create(region, &rect, &depth_temp);
  float depth_close = view3d_depth_near(&depth_temp);
  MEM_SAFE_FREE(depth_temp.depths);
  return depth_close;
}

bool ED_view3d_autodist(ARegion *region,
                        View3D *v3d,
                        const int mval[2],
                        float mouse_worldloc[3],
                        const float fallback_depth_pt[3])
{
  float depth_close;
  int margin_arr[] = {0, 2, 4};
  bool depth_ok = false;

  /* Attempt with low margin's first */
  int i = 0;
  do {
    depth_close = view_autodist_depth_margin(region, mval, margin_arr[i++] * U.pixelsize);
    depth_ok = (depth_close != FLT_MAX);
  } while ((depth_ok == false) && (i < ARRAY_SIZE(margin_arr)));

  if (depth_ok) {
    float centx = float(mval[0]) + 0.5f;
    float centy = float(mval[1]) + 0.5f;

    if (ED_view3d_unproject_v3(region, centx, centy, depth_close, mouse_worldloc)) {
      return true;
    }
  }

  if (fallback_depth_pt) {
    ED_view3d_win_to_3d_int(v3d, region, fallback_depth_pt, mval, mouse_worldloc);
    return true;
  }
  return false;
}

bool ED_view3d_autodist_simple(ARegion *region,
                               const int mval[2],
                               float mouse_worldloc[3],
                               int margin,
                               const float *force_depth)
{
  /* Get Z Depths, needed for perspective, nice for ortho */
  float depth;
  if (force_depth) {
    depth = *force_depth;
  }
  else {
    depth = view_autodist_depth_margin(region, mval, margin);
  }

  if (depth == FLT_MAX) {
    return false;
  }

  float centx = float(mval[0]) + 0.5f;
  float centy = float(mval[1]) + 0.5f;
  return ED_view3d_unproject_v3(region, centx, centy, depth, mouse_worldloc);
}

static bool depth_segment_cb(int x, int y, void *user_data)
{
  struct UserData {
    const ViewDepths *vd;
    int margin;
    float depth;
  } *data = static_cast<UserData *>(user_data);
  int mval[2];
  float depth;

  mval[0] = x;
  mval[1] = y;

  if (ED_view3d_depth_read_cached(data->vd, mval, data->margin, &depth)) {
    data->depth = depth;
    return false;
  }
  return true;
}

bool ED_view3d_depth_read_cached_seg(
    const ViewDepths *vd, const int mval_sta[2], const int mval_end[2], int margin, float *r_depth)
{
  struct {
    const ViewDepths *vd;
    int margin;
    float depth;
  } data = {nullptr};
  int p1[2];
  int p2[2];

  data.vd = vd;
  data.margin = margin;
  data.depth = 1.0f;

  copy_v2_v2_int(p1, mval_sta);
  copy_v2_v2_int(p2, mval_end);

  BLI_bitmap_draw_2d_line_v2v2i(p1, p2, depth_segment_cb, &data);

  *r_depth = data.depth;

  return (*r_depth != 1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Radius/Distance Utilities
 *
 * Use to calculate a distance to a point based on its radius.
 * \{ */

float ED_view3d_radius_to_dist_persp(const float angle, const float radius)
{
  return radius * (1.0f / tanf(angle / 2.0f));
}

float ED_view3d_radius_to_dist_ortho(const float lens, const float radius)
{
  return radius / (DEFAULT_SENSOR_WIDTH / lens);
}

float ED_view3d_radius_to_dist(const View3D *v3d,
                               const ARegion *region,
                               const Depsgraph *depsgraph,
                               const char persp,
                               const bool use_aspect,
                               const float radius)
{
  float dist;

  BLI_assert(ELEM(persp, RV3D_ORTHO, RV3D_PERSP, RV3D_CAMOB));
  BLI_assert((persp != RV3D_CAMOB) || v3d->camera);

  if (persp == RV3D_ORTHO) {
    dist = ED_view3d_radius_to_dist_ortho(v3d->lens, radius);
  }
  else {
    float lens, sensor_size, zoom;

    if (persp == RV3D_CAMOB) {
      CameraParams params;
      BKE_camera_params_init(&params);
      params.clip_start = v3d->clip_start;
      params.clip_end = v3d->clip_end;
      Object *camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
      BKE_camera_params_from_object(&params, camera_eval);

      lens = params.lens;
      sensor_size = BKE_camera_sensor_size(params.sensor_fit, params.sensor_x, params.sensor_y);

      /* ignore 'rv3d->camzoom' because we want to fit to the cameras frame */
      zoom = CAMERA_PARAM_ZOOM_INIT_CAMOB;
    }
    else {
      lens = v3d->lens;
      sensor_size = DEFAULT_SENSOR_WIDTH;
      zoom = CAMERA_PARAM_ZOOM_INIT_PERSP;
    }

    float angle = focallength_to_fov(lens, sensor_size);

    /* zoom influences lens, correct this by scaling the angle as a distance
     * (by the zoom-level) */
    angle = atanf(tanf(angle / 2.0f) * zoom) * 2.0f;

    dist = ED_view3d_radius_to_dist_persp(angle, radius);
  }

  if (use_aspect) {
    const RegionView3D *rv3d = static_cast<const RegionView3D *>(region->regiondata);

    float winx, winy;

    if (persp == RV3D_CAMOB) {
      /* camera frame x/y in pixels */
      winx = region->winx / rv3d->viewcamtexcofac[0];
      winy = region->winy / rv3d->viewcamtexcofac[1];
    }
    else {
      winx = region->winx;
      winy = region->winy;
    }

    if (winx && winy) {
      float aspect = winx / winy;
      if (aspect < 1.0f) {
        aspect = 1.0f / aspect;
      }
      dist *= aspect;
    }
  }

  return dist;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Distance Utilities
 * \{ */

float ED_view3d_offset_distance(const float mat[4][4],
                                const float ofs[3],
                                const float fallback_dist)
{
  float pos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float dir[4] = {0.0f, 0.0f, 1.0f, 0.0f};

  mul_m4_v4(mat, pos);
  add_v3_v3(pos, ofs);
  mul_m4_v4(mat, dir);
  normalize_v3(dir);

  float dist = dot_v3v3(pos, dir);

  if ((dist < FLT_EPSILON) && (fallback_dist != 0.0f)) {
    dist = fallback_dist;
  }

  return dist;
}

void ED_view3d_distance_set(RegionView3D *rv3d, const float dist)
{
  float viewinv[4];
  float tvec[3];

  BLI_assert(dist >= 0.0f);

  copy_v3_fl3(tvec, 0.0f, 0.0f, rv3d->dist - dist);
/* rv3d->viewinv isn't always valid */
#if 0
  mul_mat3_m4_v3(rv3d->viewinv, tvec);
#else
  invert_qt_qt_normalized(viewinv, rv3d->viewquat);
  mul_qt_v3(viewinv, tvec);
#endif
  sub_v3_v3(rv3d->ofs, tvec);

  rv3d->dist = dist;
}

bool ED_view3d_distance_set_from_location(RegionView3D *rv3d,
                                          const float dist_co[3],
                                          const float dist_min)
{
  float viewinv[4];
  invert_qt_qt_normalized(viewinv, rv3d->viewquat);

  float tvec[3] = {0.0f, 0.0f, -1.0f};
  mul_qt_v3(viewinv, tvec);

  float dist_co_local[3];
  negate_v3_v3(dist_co_local, rv3d->ofs);
  sub_v3_v3v3(dist_co_local, dist_co, dist_co_local);
  const float delta = dot_v3v3(tvec, dist_co_local);
  const float dist_new = rv3d->dist + delta;
  if (dist_new >= dist_min) {
    madd_v3_v3fl(rv3d->ofs, tvec, -delta);
    rv3d->dist = dist_new;
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Axis Utilities
 * \{ */

/**
 * Lookup by axis-view, axis-roll.
 */
static float view3d_quat_axis[6][4][4] = {
    /* RV3D_VIEW_FRONT */
    {
        {M_SQRT1_2, -M_SQRT1_2, 0.0f, 0.0f},
        {0.5f, -0.5f, -0.5f, 0.5f},
        {0, 0, -M_SQRT1_2, M_SQRT1_2},
        {-0.5f, 0.5f, -0.5f, 0.5f},
    }, /* RV3D_VIEW_BACK */
    {
        {0.0f, 0.0f, -M_SQRT1_2, -M_SQRT1_2},
        {0.5f, 0.5f, -0.5f, -0.5f},
        {M_SQRT1_2, M_SQRT1_2, 0, 0},
        {0.5f, 0.5f, 0.5f, 0.5f},
    }, /* RV3D_VIEW_LEFT */
    {
        {0.5f, -0.5f, 0.5f, 0.5f},
        {0, -M_SQRT1_2, 0.0f, M_SQRT1_2},
        {-0.5f, -0.5f, -0.5f, 0.5f},
        {-M_SQRT1_2, 0, -M_SQRT1_2, 0},
    },

    /* RV3D_VIEW_RIGHT */
    {
        {0.5f, -0.5f, -0.5f, -0.5f},
        {M_SQRT1_2, 0, -M_SQRT1_2, 0},
        {0.5f, 0.5f, -0.5f, 0.5f},
        {0, M_SQRT1_2, 0, M_SQRT1_2},
    }, /* RV3D_VIEW_TOP */
    {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {M_SQRT1_2, 0, 0, M_SQRT1_2},
        {0, 0, 0, 1},
        {-M_SQRT1_2, 0, 0, M_SQRT1_2},
    }, /* RV3D_VIEW_BOTTOM */
    {
        {0.0f, -1.0f, 0.0f, 0.0f},
        {0, -M_SQRT1_2, -M_SQRT1_2, 0},
        {0, 0, -1, 0},
        {0, M_SQRT1_2, -M_SQRT1_2, 0},
    },

};

bool ED_view3d_quat_from_axis_view(const char view, const char view_axis_roll, float r_quat[4])
{
  BLI_assert(view_axis_roll <= RV3D_VIEW_AXIS_ROLL_270);
  if (RV3D_VIEW_IS_AXIS(view)) {
    copy_qt_qt(r_quat, view3d_quat_axis[view - RV3D_VIEW_FRONT][view_axis_roll]);
    return true;
  }
  return false;
}

bool ED_view3d_quat_to_axis_view(const float quat[4],
                                 const float epsilon,
                                 char *r_view,
                                 char *r_view_axis_roll)
{
  *r_view = RV3D_VIEW_USER;
  *r_view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;

  /* Quaternion values are all unit length. */

  if (epsilon < M_PI_4) {
    /* Under 45 degrees, just pick the closest value. */
    for (int view = RV3D_VIEW_FRONT; view <= RV3D_VIEW_BOTTOM; view++) {
      for (int view_axis_roll = RV3D_VIEW_AXIS_ROLL_0; view_axis_roll <= RV3D_VIEW_AXIS_ROLL_270;
           view_axis_roll++)
      {
        if (fabsf(angle_signed_qtqt(
                quat, view3d_quat_axis[view - RV3D_VIEW_FRONT][view_axis_roll])) < epsilon)
        {
          *r_view = view;
          *r_view_axis_roll = view_axis_roll;
          return true;
        }
      }
    }
  }
  else {
    /* Epsilon over 45 degrees, check all & find use the closest. */
    float delta_best = FLT_MAX;
    for (int view = RV3D_VIEW_FRONT; view <= RV3D_VIEW_BOTTOM; view++) {
      for (int view_axis_roll = RV3D_VIEW_AXIS_ROLL_0; view_axis_roll <= RV3D_VIEW_AXIS_ROLL_270;
           view_axis_roll++)
      {
        const float delta_test = fabsf(
            angle_signed_qtqt(quat, view3d_quat_axis[view - RV3D_VIEW_FRONT][view_axis_roll]));
        if (delta_best > delta_test) {
          delta_best = delta_test;
          *r_view = view;
          *r_view_axis_roll = view_axis_roll;
        }
      }
    }
    if (*r_view != RV3D_VIEW_USER) {
      return true;
    }
  }

  return false;
}

bool ED_view3d_quat_to_axis_view_and_reset_quat(float quat[4],
                                                const float epsilon,
                                                char *r_view,
                                                char *r_view_axis_roll)
{
  const bool is_axis_view = ED_view3d_quat_to_axis_view(quat, epsilon, r_view, r_view_axis_roll);
  if (is_axis_view) {
    /* Reset `quat` to it's view axis, so axis-aligned views are always *exactly* aligned. */
    BLI_assert(*r_view != RV3D_VIEW_USER);
    ED_view3d_quat_from_axis_view(*r_view, *r_view_axis_roll, quat);
  }
  return is_axis_view;
}

char ED_view3d_lock_view_from_index(int index)
{
  switch (index) {
    case 0:
      return RV3D_VIEW_FRONT;
    case 1:
      return RV3D_VIEW_TOP;
    case 2:
      return RV3D_VIEW_RIGHT;
    default:
      return RV3D_VIEW_USER;
  }
}

char ED_view3d_axis_view_opposite(char view)
{
  switch (view) {
    case RV3D_VIEW_FRONT:
      return RV3D_VIEW_BACK;
    case RV3D_VIEW_BACK:
      return RV3D_VIEW_FRONT;
    case RV3D_VIEW_LEFT:
      return RV3D_VIEW_RIGHT;
    case RV3D_VIEW_RIGHT:
      return RV3D_VIEW_LEFT;
    case RV3D_VIEW_TOP:
      return RV3D_VIEW_BOTTOM;
    case RV3D_VIEW_BOTTOM:
      return RV3D_VIEW_TOP;
  }

  return RV3D_VIEW_USER;
}

bool ED_view3d_lock(RegionView3D *rv3d)
{
  return ED_view3d_quat_from_axis_view(rv3d->view, rv3d->view_axis_roll, rv3d->viewquat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Transform Utilities
 * \{ */

void ED_view3d_from_m4(const float mat[4][4], float ofs[3], float quat[4], const float *dist)
{
  float nmat[3][3];

  /* dist depends on offset */
  BLI_assert(dist == nullptr || ofs != nullptr);

  copy_m3_m4(nmat, mat);
  normalize_m3(nmat);

  /* Offset */
  if (ofs) {
    negate_v3_v3(ofs, mat[3]);
  }

  /* Quat */
  if (quat) {
    mat3_normalized_to_quat(quat, nmat);
    invert_qt_normalized(quat);
  }

  if (ofs && dist) {
    madd_v3_v3fl(ofs, nmat[2], *dist);
  }
}

void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], const float dist)
{
  const float iviewquat[4] = {-quat[0], quat[1], quat[2], quat[3]};
  float dvec[3] = {0.0f, 0.0f, dist};

  quat_to_mat4(mat, iviewquat);
  mul_mat3_m4_v3(mat, dvec);
  sub_v3_v3v3(mat[3], dvec, ofs);
}

void ED_view3d_from_object(
    const Object *ob, float ofs[3], float quat[4], const float *dist, float *lens)
{
  ED_view3d_from_m4(ob->object_to_world().ptr(), ofs, quat, dist);

  if (lens) {
    CameraParams params;

    BKE_camera_params_init(&params);
    BKE_camera_params_from_object(&params, ob);
    *lens = params.lens;
  }
}

void ED_view3d_to_object(const Depsgraph *depsgraph,
                         Object *ob,
                         const float ofs[3],
                         const float quat[4],
                         const float dist)
{
  float mat[4][4];
  ED_view3d_to_m4(mat, ofs, quat, dist);

  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  BKE_object_apply_mat4_ex(ob, mat, ob_eval->parent, ob_eval->parentinv, true);
}

static bool view3d_camera_to_view_selected_impl(Main *bmain,
                                                Depsgraph *depsgraph,
                                                const Scene *scene,
                                                Object *camera_ob,
                                                float *r_clip_start,
                                                float *r_clip_end)
{
  Object *camera_ob_eval = DEG_get_evaluated(depsgraph, camera_ob);
  float co[3]; /* the new location to apply */
  float scale; /* only for ortho cameras */

  if (BKE_camera_view_frame_fit_to_scene(
          depsgraph, scene, camera_ob_eval, co, &scale, r_clip_start, r_clip_end))
  {
    ObjectTfmProtectedChannels obtfm;
    float obmat_new[4][4];
    bool is_ortho_camera = false;

    if ((camera_ob_eval->type == OB_CAMERA) &&
        (((Camera *)camera_ob_eval->data)->type == CAM_ORTHO))
    {
      ((Camera *)camera_ob->data)->ortho_scale = scale;
      is_ortho_camera = true;
    }

    copy_m4_m4(obmat_new, camera_ob_eval->object_to_world().ptr());
    copy_v3_v3(obmat_new[3], co);

    /* only touch location */
    BKE_object_tfm_protected_backup(camera_ob, &obtfm);
    BKE_object_apply_mat4(camera_ob, obmat_new, true, true);
    BKE_object_tfm_protected_restore(camera_ob, &obtfm, OB_LOCK_SCALE | OB_LOCK_ROT4D);

    /* notifiers */
    DEG_id_tag_update_ex(bmain, &camera_ob->id, ID_RECALC_TRANSFORM);
    if (is_ortho_camera) {
      DEG_id_tag_update_ex(bmain, static_cast<ID *>(camera_ob->data), ID_RECALC_PARAMETERS);
    }

    return true;
  }

  return false;
}

bool ED_view3d_camera_to_view_selected(Main *bmain,
                                       Depsgraph *depsgraph,
                                       const Scene *scene,
                                       Object *camera_ob)
{
  return view3d_camera_to_view_selected_impl(bmain, depsgraph, scene, camera_ob, nullptr, nullptr);
}

bool ED_view3d_camera_to_view_selected_with_set_clipping(Main *bmain,
                                                         Depsgraph *depsgraph,
                                                         const Scene *scene,
                                                         Object *camera_ob)
{
  float clip_start;
  float clip_end;
  if (view3d_camera_to_view_selected_impl(
          bmain, depsgraph, scene, camera_ob, &clip_start, &clip_end))
  {

    ((Camera *)camera_ob->data)->clip_start = clip_start;
    ((Camera *)camera_ob->data)->clip_end = clip_end;

    /* TODO: Support update via #ID_RECALC_PARAMETERS. */
    Object *camera_ob_eval = DEG_get_evaluated(depsgraph, camera_ob);
    ((Camera *)camera_ob_eval->data)->clip_start = clip_start;
    ((Camera *)camera_ob_eval->data)->clip_end = clip_end;

    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth Buffer Utilities
 * \{ */

struct ReadData {
  int count;
  int count_max;
  float r_depth;
};

static bool depth_read_test_fn(const void *value, void *userdata)
{
  ReadData *data = static_cast<ReadData *>(userdata);
  float depth = *(float *)value;
  data->r_depth = std::min(depth, data->r_depth);

  if ((++data->count) >= data->count_max) {
    /* Outside the margin. */
    return true;
  }
  return false;
}

bool ED_view3d_depth_read_cached(const ViewDepths *vd,
                                 const int mval[2],
                                 int margin,
                                 float *r_depth)
{
  *r_depth = 1.0f;

  if (!vd || !vd->depths) {
    return false;
  }

  BLI_assert(1.0 <= vd->depth_range[1]);
  int x = mval[0];
  int y = mval[1];
  if (x < 0 || y < 0 || x >= vd->w || y >= vd->h) {
    return false;
  }

  float depth = 1.0f;
  if (margin) {
    int shape[2] = {vd->w, vd->h};
    int pixel_count = (min_ii(x + margin + 1, shape[1]) - max_ii(x - margin, 0)) *
                      (min_ii(y + margin + 1, shape[0]) - max_ii(y - margin, 0));

    ReadData data;
    data.count = 0;
    data.count_max = pixel_count;
    data.r_depth = 1.0f;

    /* TODO: No need to go spiral. */
    BLI_array_iter_spiral_square(vd->depths, shape, mval, depth_read_test_fn, &data);
    depth = data.r_depth;
  }
  else {
    depth = vd->depths[y * vd->w + x];
  }

  if (depth != 1.0f) {
    *r_depth = depth;
    return true;
  }

  return false;
}

bool ED_view3d_depth_read_cached_normal(const ARegion *region,
                                        const ViewDepths *depths,
                                        const int mval[2],
                                        float r_normal[3])
{
  /* NOTE: we could support passing in a radius.
   * For now just read 9 pixels. */

  /* pixels surrounding */
  bool depths_valid[9] = {false};
  float coords[9][3] = {{0}};

  for (int x = 0, i = 0; x < 2; x++) {
    for (int y = 0; y < 2; y++) {
      const int mval_ofs[2] = {mval[0] + (x - 1), mval[1] + (y - 1)};

      float depth_fl = 1.0f;
      ED_view3d_depth_read_cached(depths, mval_ofs, 0, &depth_fl);
      const double depth = double(depth_fl);
      if ((depth > depths->depth_range[0]) && (depth < depths->depth_range[1])) {
        if (ED_view3d_depth_unproject_v3(region, mval_ofs, depth, coords[i])) {
          depths_valid[i] = true;
        }
      }
      i++;
    }
  }

  const int edges[2][6][2] = {
      /* x edges */
      {{0, 1}, {1, 2}, {3, 4}, {4, 5}, {6, 7}, {7, 8}}, /* y edges */
      {{0, 3}, {3, 6}, {1, 4}, {4, 7}, {2, 5}, {5, 8}},
  };

  float cross[2][3] = {{0.0f}};

  for (int i = 0; i < 6; i++) {
    for (int axis = 0; axis < 2; axis++) {
      if (depths_valid[edges[axis][i][0]] && depths_valid[edges[axis][i][1]]) {
        float delta[3];
        sub_v3_v3v3(delta, coords[edges[axis][i][0]], coords[edges[axis][i][1]]);
        add_v3_v3(cross[axis], delta);
      }
    }
  }

  cross_v3_v3v3(r_normal, cross[0], cross[1]);

  if (normalize_v3(r_normal) != 0.0f) {
    return true;
  }
  return false;
}

bool ED_view3d_depth_unproject_v3(const ARegion *region,
                                  const int mval[2],
                                  const double depth,
                                  float r_location_world[3])
{
  float centx = float(mval[0]) + 0.5f;
  float centy = float(mval[1]) + 0.5f;
  return ED_view3d_unproject_v3(region, centx, centy, depth, r_location_world);
}

/** \} */
