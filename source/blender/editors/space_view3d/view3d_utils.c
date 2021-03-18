/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 *
 * 3D View checks and manipulation (no operators).
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BIF_glutil.h"

#include "GPU_matrix.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "view3d_intern.h" /* own include */

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
    if (STREQ(scene->r.engine, RE_engine_id_BLENDER_WORKBENCH)) {
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
    return v3d->camera->data;
  }
  return NULL;
}

void ED_view3d_dist_range_get(const View3D *v3d, float r_dist_range[2])
{
  r_dist_range[0] = v3d->grid * 0.001f;
  r_dist_range[1] = v3d->clip_end * 10.0f;
}

/**
 * \note copies logic of #ED_view3d_viewplane_get(), keep in sync.
 */
bool ED_view3d_clip_range_get(Depsgraph *depsgraph,
                              const View3D *v3d,
                              const RegionView3D *rv3d,
                              float *r_clipsta,
                              float *r_clipend,
                              const bool use_ortho_factor)
{
  CameraParams params;

  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, rv3d);

  if (use_ortho_factor && params.is_ortho) {
    const float fac = 2.0f / (params.clip_end - params.clip_start);
    params.clip_start *= fac;
    params.clip_end *= fac;
  }

  if (r_clipsta) {
    *r_clipsta = params.clip_start;
  }
  if (r_clipend) {
    *r_clipend = params.clip_end;
  }

  return params.is_ortho;
}

bool ED_view3d_viewplane_get(Depsgraph *depsgraph,
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

/**
 * Use this call when executing an operator,
 * event system doesn't set for each event the OpenGL drawing context.
 */
void view3d_operator_needs_opengl(const bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);

  view3d_region_operator_needs_opengl(win, region);
}

void view3d_region_operator_needs_opengl(wmWindow *UNUSED(win), ARegion *region)
{
  /* for debugging purpose, context should always be OK */
  if ((region == NULL) || (region->regiontype != RGN_TYPE_WINDOW)) {
    printf("view3d_region_operator_needs_opengl error, wrong region\n");
  }
  else {
    RegionView3D *rv3d = region->regiondata;

    wmViewport(&region->winrct); /* TODO: bad */
    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);
  }
}

/**
 * Use instead of: `GPU_polygon_offset(rv3d->dist, ...)` see bug T37727.
 */
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

  /* area can be NULL when called from python */
  if (area == NULL || area->spacetype != SPACE_VIEW3D) {
    area = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
  }

  if (area == NULL) {
    return false;
  }

  ARegion *region = BKE_area_find_region_active_win(area);
  if (region == NULL) {
    return false;
  }

  /* bad context switch .. */
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
    float xs = (ELEM(val, 0, 3)) ? rect->xmin : rect->xmax;
    float ys = (ELEM(val, 0, 1)) ? rect->ymin : rect->ymax;

    ED_view3d_unproject(region, xs, ys, 0.0, bb->vec[val]);
    ED_view3d_unproject(region, xs, ys, 1.0, bb->vec[4 + val]);
  }

  /* optionally transform to object space */
  if (ob) {
    float imat[4][4];
    invert_m4_m4(imat, ob->obmat);

    for (int val = 0; val < 8; val++) {
      mul_m4_v3(imat, bb->vec[val]);
    }
  }

  /* verify if we have negative scale. doing the transform before cross
   * product flips the sign of the vector compared to doing cross product
   * before transform then, so we correct for that. */
  int flip_sign = (ob) ? is_negative_m4(ob->obmat) : false;

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
    const float co[3], int UNUSED(i), int UNUSED(j), int UNUSED(k), void *user_data_p)
{
  struct PointsInPlanesMinMax_UserData *user_data = user_data_p;
  minmax_v3v3_v3(user_data->min, user_data->max, co);
}

/**
 * Clamp min/max by the viewport clipping.
 *
 * \note This is an approximation, with the limitation that the bounding box from the (mix, max)
 * calculation might not have any geometry inside the clipped region.
 * Performing a clipping test on each vertex would work well enough for most cases,
 * although it's not perfect either as edges/faces may intersect the clipping without having any
 * of their vertices inside it.
 * A more accurate result would be quite involved.
 *
 * \return True when the arguments were clamped.
 */
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
  struct PointsInPlanesMinMax_UserData user_data;
  INIT_MINMAX(user_data.min, user_data.max);

  const float eps_coplanar = 1e-4f;
  const float eps_isect = 1e-6f;
  if (isect_planes_v3_fn(
          planes, planes_len, eps_coplanar, eps_isect, points_in_planes_minmax_fn, &user_data)) {
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

  if (bb == NULL) {
    return true;
  }
  if (bb->flag & BOUNDBOX_DISABLED) {
    return true;
  }

  mul_m4_m4m4(persmatob, (float(*)[4])rv3d->persmat, obmat);

  return view3d_boundbox_clip_m4(bb, persmatob);
}

bool ED_view3d_boundbox_clip(RegionView3D *rv3d, const BoundBox *bb)
{
  if (bb == NULL) {
    return true;
  }
  if (bb->flag & BOUNDBOX_DISABLED) {
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

/**
 * Use to store the last view, before entering camera view.
 */
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
  v3d->ob_center = NULL;
  v3d->ob_center_bone[0] = '\0';
  v3d->ob_center_cursor = false;

  v3d->flag2 &= ~V3D_LOCK_CAMERA;
}

/**
 * For viewport operators that exit camera perspective.
 *
 * \note This differs from simply setting ``rv3d->persp = persp`` because it
 * sets the ``ofs`` and ``dist`` values of the viewport so it matches the camera,
 * otherwise switching out of camera view may jump to a different part of the scene.
 */
void ED_view3d_persp_switch_from_camera(const Depsgraph *depsgraph,
                                        View3D *v3d,
                                        RegionView3D *rv3d,
                                        const char persp)
{
  BLI_assert(rv3d->persp == RV3D_CAMOB);
  BLI_assert(persp != RV3D_CAMOB);

  if (v3d->camera) {
    Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
    rv3d->dist = ED_view3d_offset_distance(ob_camera_eval->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
    ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
  }

  if (!ED_view3d_camera_lock_check(v3d, rv3d)) {
    rv3d->persp = persp;
  }
}
/**
 * Action to take when rotating the view,
 * handle auto-persp and logic for switching out of views.
 *
 * shared with NDOF.
 */
bool ED_view3d_persp_ensure(const Depsgraph *depsgraph, View3D *v3d, ARegion *region)
{
  RegionView3D *rv3d = region->regiondata;
  const bool autopersp = (U.uiflag & USER_AUTOPERSP) != 0;

  BLI_assert((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0);

  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    return false;
  }

  if (rv3d->persp != RV3D_PERSP) {
    if (rv3d->persp == RV3D_CAMOB) {
      /* If autopersp and previous view was an axis one,
       * switch back to PERSP mode, else reuse previous mode. */
      char persp = (autopersp && RV3D_VIEW_IS_AXIS(rv3d->lview)) ? RV3D_PERSP : rv3d->lpersp;
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
/** \name Camera Lock API
 *
 * Lock the camera to the 3D Viewport, allowing view manipulation to transform the camera.
 * \{ */

/**
 * \return true when the 3D Viewport is locked to its camera.
 */
bool ED_view3d_camera_lock_check(const View3D *v3d, const RegionView3D *rv3d)
{
  return ((v3d->camera) && (!ID_IS_LINKED(v3d->camera)) && (v3d->flag2 & V3D_LOCK_CAMERA) &&
          (rv3d->persp == RV3D_CAMOB));
}

/**
 * Apply the camera object transformation to the 3D Viewport.
 * (needed so we can use regular 3D Viewport manipulation operators, that sync back to the camera).
 */
void ED_view3d_camera_lock_init_ex(const Depsgraph *depsgraph,
                                   View3D *v3d,
                                   RegionView3D *rv3d,
                                   const bool calc_dist)
{
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
    if (calc_dist) {
      /* using a fallback dist is OK here since ED_view3d_from_object() compensates for it */
      rv3d->dist = ED_view3d_offset_distance(
          ob_camera_eval->obmat, rv3d->ofs, VIEW3D_DIST_FALLBACK);
    }
    ED_view3d_from_object(ob_camera_eval, rv3d->ofs, rv3d->viewquat, &rv3d->dist, NULL);
  }
}

void ED_view3d_camera_lock_init(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
{
  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, true);
}

/**
 * Apply the 3D Viewport transformation back to the camera object.
 *
 * \return true if the camera is moved.
 */
bool ED_view3d_camera_lock_sync(const Depsgraph *depsgraph, View3D *v3d, RegionView3D *rv3d)
{
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    ObjectTfmProtectedChannels obtfm;
    Object *root_parent;

    if (v3d->camera->transflag & OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK &&
        (root_parent = v3d->camera->parent)) {
      Object *ob_update;
      float tmat[4][4];
      float imat[4][4];
      float view_mat[4][4];
      float diff_mat[4][4];
      float parent_mat[4][4];

      while (root_parent->parent) {
        root_parent = root_parent->parent;
      }
      Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
      Object *root_parent_eval = DEG_get_evaluated_object(depsgraph, root_parent);

      ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);

      normalize_m4_m4(tmat, ob_camera_eval->obmat);

      invert_m4_m4(imat, tmat);
      mul_m4_m4m4(diff_mat, view_mat, imat);

      mul_m4_m4m4(parent_mat, diff_mat, root_parent_eval->obmat);

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

bool ED_view3d_camera_autokey(const Scene *scene,
                              ID *id_key,
                              struct bContext *C,
                              const bool do_rotate,
                              const bool do_translate)
{
  if (autokeyframe_cfra_can_key(scene, id_key)) {
    const float cfra = (float)CFRA;
    ListBase dsources = {NULL, NULL};

    /* add data-source override for the camera object */
    ANIM_relative_keyingset_add_source(&dsources, id_key, NULL, NULL);

    /* insert keyframes
     * 1) on the first frame
     * 2) on each subsequent frame
     *    TODO: need to check in future that frame changed before doing this
     */
    if (do_rotate) {
      struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_ROTATION_ID);
      ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
    }
    if (do_translate) {
      struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
      ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
    }

    /* free temp data */
    BLI_freelistN(&dsources);

    return true;
  }
  return false;
}

/**
 * Call after modifying a locked view.
 *
 * \note Not every view edit currently auto-keys (num-pad for eg),
 * this is complicated because of smooth-view.
 */
bool ED_view3d_camera_lock_autokey(View3D *v3d,
                                   RegionView3D *rv3d,
                                   struct bContext *C,
                                   const bool do_rotate,
                                   const bool do_translate)
{
  /* similar to ED_view3d_cameracontrol_update */
  if (ED_view3d_camera_lock_check(v3d, rv3d)) {
    Scene *scene = CTX_data_scene(C);
    ID *id_key;
    Object *root_parent;
    if (v3d->camera->transflag & OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK &&
        (root_parent = v3d->camera->parent)) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box View Support
 *
 * Use with quad-split so each view is clipped by the bounds of each view axis.
 * \{ */

static void view3d_boxview_clip(ScrArea *area)
{
  BoundBox *bb = MEM_callocN(sizeof(BoundBox), "clipbb");
  float clip[6][4];
  float x1 = 0.0f, y1 = 0.0f, z1 = 0.0f, ofs[3] = {0.0f, 0.0f, 0.0f};

  /* create bounding box */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = region->regiondata;

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
      RegionView3D *rv3d = region->regiondata;

      if (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) {
        rv3d->rflag |= RV3D_CLIPPING;
        memcpy(rv3d->clip, clip, sizeof(clip));
        if (rv3d->clipbb) {
          MEM_freeN(rv3d->clipbb);
        }
        rv3d->clipbb = MEM_dupallocN(bb);
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
               false)) {
    return;
  }
  invert_qt_normalized(viewinv);
  mul_qt_v3(viewinv, view_src_x);
  mul_qt_v3(viewinv, view_src_y);

  if (UNLIKELY(ED_view3d_quat_from_axis_view(rv3d_dst->view, rv3d_dst->view_axis_roll, viewinv) ==
               false)) {
    return;
  }
  invert_qt_normalized(viewinv);
  mul_qt_v3(viewinv, view_dst_x);
  mul_qt_v3(viewinv, view_dst_y);

  /* check source and dest have a matching axis */
  for (i = 0; i < 3; i++) {
    if (((fabsf(view_src_x[i]) > axis_eps) || (fabsf(view_src_y[i]) > axis_eps)) &&
        ((fabsf(view_dst_x[i]) > axis_eps) || (fabsf(view_dst_y[i]) > axis_eps))) {
      rv3d_dst->ofs[i] = rv3d_src->ofs[i];
    }
  }
}

/* sync center/zoom view of region to others, for view transforms */
void view3d_boxview_sync(ScrArea *area, ARegion *region)
{
  RegionView3D *rv3d = region->regiondata;
  short clip = 0;

  LISTBASE_FOREACH (ARegion *, region_test, &area->regionbase) {
    if (region_test != region && region_test->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3dtest = region_test->regiondata;

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

/* for home, center etc */
void view3d_boxview_copy(ScrArea *area, ARegion *region)
{
  RegionView3D *rv3d = region->regiondata;
  bool clip = false;

  LISTBASE_FOREACH (ARegion *, region_test, &area->regionbase) {
    if (region_test != region && region_test->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3dtest = region_test->regiondata;

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

/* 'clip' is used to know if our clip setting has changed */
void ED_view3d_quadview_update(ScrArea *area, ARegion *region, bool do_clip)
{
  ARegion *region_sync = NULL;
  RegionView3D *rv3d = region->regiondata;
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
      rv3d = region->regiondata;
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
    view3d_boxview_sync(area, region_sync ? region_sync : area->regionbase.last);
  }

  /* ensure locked regions have an axis, locked user views don't make much sense */
  if (viewlock & RV3D_LOCK_ROTATION) {
    int index_qsplit = 0;
    for (region = area->regionbase.first; region; region = region->next) {
      if (region->alignment == RGN_ALIGN_QSPLIT) {
        rv3d = region->regiondata;
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
  view3d_update_depths_rect(region, &depth_temp, &rect);
  float depth_close = view3d_depth_near(&depth_temp);
  MEM_SAFE_FREE(depth_temp.depths);
  return depth_close;
}

/**
 * Get the world-space 3d location from a screen-space 2d point.
 *
 * \param mval: Input screen-space pixel location.
 * \param mouse_worldloc: Output world-space location.
 * \param fallback_depth_pt: Use this points depth when no depth can be found.
 */
bool ED_view3d_autodist(Depsgraph *depsgraph,
                        ARegion *region,
                        View3D *v3d,
                        const int mval[2],
                        float mouse_worldloc[3],
                        const bool alphaoverride,
                        const float fallback_depth_pt[3])
{
  float depth_close;
  int margin_arr[] = {0, 2, 4};
  bool depth_ok = false;

  /* Get Z Depths, needed for perspective, nice for ortho */
  ED_view3d_draw_depth(depsgraph, region, v3d, alphaoverride);

  /* Attempt with low margin's first */
  int i = 0;
  do {
    depth_close = view_autodist_depth_margin(region, mval, margin_arr[i++] * U.pixelsize);
    depth_ok = (depth_close != FLT_MAX);
  } while ((depth_ok == false) && (i < ARRAY_SIZE(margin_arr)));

  if (depth_ok) {
    float centx = (float)mval[0] + 0.5f;
    float centy = (float)mval[1] + 0.5f;

    if (ED_view3d_unproject(region, centx, centy, depth_close, mouse_worldloc)) {
      return true;
    }
  }

  if (fallback_depth_pt) {
    ED_view3d_win_to_3d_int(v3d, region, fallback_depth_pt, mval, mouse_worldloc);
    return true;
  }
  return false;
}

void ED_view3d_autodist_init(Depsgraph *depsgraph, ARegion *region, View3D *v3d, int mode)
{
  /* Get Z Depths, needed for perspective, nice for ortho */
  switch (mode) {
    case 0:
      ED_view3d_draw_depth(depsgraph, region, v3d, true);
      break;
    case 1: {
      Scene *scene = DEG_get_evaluated_scene(depsgraph);
      ED_view3d_draw_depth_gpencil(depsgraph, scene, region, v3d);
      break;
    }
  }
}

/* no 4x4 sampling, run #ED_view3d_autodist_init first */
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

  float centx = (float)mval[0] + 0.5f;
  float centy = (float)mval[1] + 0.5f;
  return ED_view3d_unproject(region, centx, centy, depth, mouse_worldloc);
}

bool ED_view3d_autodist_depth(ARegion *region, const int mval[2], int margin, float *depth)
{
  *depth = view_autodist_depth_margin(region, mval, margin);

  return (*depth != FLT_MAX);
}

static bool depth_segment_cb(int x, int y, void *userData)
{
  struct {
    ARegion *region;
    int margin;
    float depth;
  } *data = userData;
  int mval[2];
  float depth;

  mval[0] = x;
  mval[1] = y;

  depth = view_autodist_depth_margin(data->region, mval, data->margin);

  if (depth != FLT_MAX) {
    data->depth = depth;
    return false;
  }
  return true;
}

bool ED_view3d_autodist_depth_seg(
    ARegion *region, const int mval_sta[2], const int mval_end[2], int margin, float *depth)
{
  struct {
    ARegion *region;
    int margin;
    float depth;
  } data = {NULL};
  int p1[2];
  int p2[2];

  data.region = region;
  data.margin = margin;
  data.depth = FLT_MAX;

  copy_v2_v2_int(p1, mval_sta);
  copy_v2_v2_int(p2, mval_end);

  BLI_bitmap_draw_2d_line_v2v2i(p1, p2, depth_segment_cb, &data);

  *depth = data.depth;

  return (*depth != FLT_MAX);
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

/**
 * Return a new RegionView3D.dist value to fit the \a radius.
 *
 * \note Depth isn't taken into account, this will fit a flat plane exactly,
 * but points towards the view (with a perspective projection),
 * may be within the radius but outside the view. eg:
 *
 * <pre>
 *           +
 * pt --> + /^ radius
 *         / |
 *        /  |
 * view  +   +
 *        \  |
 *         \ |
 *          \|
 *           +
 * </pre>
 *
 * \param region: Can be NULL if \a use_aspect is false.
 * \param persp: Allow the caller to tell what kind of perspective to use (ortho/view/camera)
 * \param use_aspect: Increase the distance to account for non 1:1 view aspect.
 * \param radius: The radius will be fitted exactly,
 * typically pre-scaled by a margin (#VIEW3D_MARGIN).
 */
float ED_view3d_radius_to_dist(const View3D *v3d,
                               const ARegion *region,
                               const struct Depsgraph *depsgraph,
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
      Object *camera_eval = DEG_get_evaluated_object(depsgraph, v3d->camera);
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
    const RegionView3D *rv3d = region->regiondata;

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

/**
 * This function solves the problem of having to switch between camera and non-camera views.
 *
 * When viewing from the perspective of \a mat, and having the view center \a ofs,
 * this calculates a distance from \a ofs to the matrix \a mat.
 * Using \a fallback_dist when the distance would be too small.
 *
 * \param mat: A matrix use for the view-point (typically the camera objects matrix).
 * \param ofs: Orbit center (negated), matching #RegionView3D.ofs, which is typically passed in.
 * \param fallback_dist: The distance to use if the object is too near or in front of \a ofs.
 * \returns A newly calculated distance or the fallback.
 */
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

/**
 * Set the dist without moving the view (compensate with #RegionView3D.ofs)
 *
 * \note take care that viewinv is up to date, #ED_view3d_update_viewmat first.
 */
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

/**
 * Change the distance & offset to match the depth of \a dist_co along the view axis.
 *
 * \param dist_co: A world-space location to use for the new depth.
 * \param dist_min: Resulting distances below this will be ignored.
 * \return Success if the distance was set.
 */
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
    },
    /* RV3D_VIEW_BACK */
    {
        {0.0f, 0.0f, -M_SQRT1_2, -M_SQRT1_2},
        {0.5f, 0.5f, -0.5f, -0.5f},
        {M_SQRT1_2, M_SQRT1_2, 0, 0},
        {0.5f, 0.5f, 0.5f, 0.5f},
    },
    /* RV3D_VIEW_LEFT */
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
    },
    /* RV3D_VIEW_TOP */
    {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {M_SQRT1_2, 0, 0, M_SQRT1_2},
        {0, 0, 0, 1},
        {-M_SQRT1_2, 0, 0, M_SQRT1_2},
    },
    /* RV3D_VIEW_BOTTOM */
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

  /* quat values are all unit length */
  for (int view = RV3D_VIEW_FRONT; view <= RV3D_VIEW_BOTTOM; view++) {
    for (int view_axis_roll = RV3D_VIEW_AXIS_ROLL_0; view_axis_roll <= RV3D_VIEW_AXIS_ROLL_270;
         view_axis_roll++) {
      if (fabsf(angle_signed_qtqt(
              quat, view3d_quat_axis[view - RV3D_VIEW_FRONT][view_axis_roll])) < epsilon) {
        *r_view = view;
        *r_view_axis_roll = view_axis_roll;
        return true;
      }
    }
  }

  return false;
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

/**
 * Set the view transformation from a 4x4 matrix.
 *
 * \param mat: The view 4x4 transformation matrix to assign.
 * \param ofs: The view offset, normally from RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist: The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_from_m4(const float mat[4][4], float ofs[3], float quat[4], const float *dist)
{
  float nmat[3][3];

  /* dist depends on offset */
  BLI_assert(dist == NULL || ofs != NULL);

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

/**
 * Calculate the view transformation matrix from RegionView3D input.
 * The resulting matrix is equivalent to RegionView3D.viewinv
 * \param mat: The view 4x4 transformation matrix to calculate.
 * \param ofs: The view offset, normally from RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist: The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_to_m4(float mat[4][4], const float ofs[3], const float quat[4], const float dist)
{
  const float iviewquat[4] = {-quat[0], quat[1], quat[2], quat[3]};
  float dvec[3] = {0.0f, 0.0f, dist};

  quat_to_mat4(mat, iviewquat);
  mul_mat3_m4_v3(mat, dvec);
  sub_v3_v3v3(mat[3], dvec, ofs);
}

/**
 * Set the RegionView3D members from an objects transformation and optionally lens.
 * \param ob: The object to set the view to.
 * \param ofs: The view offset to be set, normally from RegionView3D.ofs.
 * \param quat: The view rotation to be set, quaternion normally from RegionView3D.viewquat.
 * \param dist: The view distance from ofs to be set, normally from RegionView3D.dist.
 * \param lens: The view lens angle set for cameras and lights, normally from View3D.lens.
 */
void ED_view3d_from_object(const Object *ob, float ofs[3], float quat[4], float *dist, float *lens)
{
  ED_view3d_from_m4(ob->obmat, ofs, quat, dist);

  if (lens) {
    CameraParams params;

    BKE_camera_params_init(&params);
    BKE_camera_params_from_object(&params, ob);
    *lens = params.lens;
  }
}

/**
 * Set the object transformation from RegionView3D members.
 * \param depsgraph: The depsgraph to get the evaluated object parent
 * for the transformation calculation.
 * \param ob: The object which has the transformation assigned.
 * \param ofs: The view offset, normally from RegionView3D.ofs.
 * \param quat: The view rotation, quaternion normally from RegionView3D.viewquat.
 * \param dist: The view distance from ofs, normally from RegionView3D.dist.
 */
void ED_view3d_to_object(const Depsgraph *depsgraph,
                         Object *ob,
                         const float ofs[3],
                         const float quat[4],
                         const float dist)
{
  float mat[4][4];
  ED_view3d_to_m4(mat, ofs, quat, dist);

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  BKE_object_apply_mat4_ex(ob, mat, ob_eval->parent, ob_eval->parentinv, true);
}

bool ED_view3d_camera_to_view_selected(struct Main *bmain,
                                       Depsgraph *depsgraph,
                                       const Scene *scene,
                                       Object *camera_ob)
{
  Object *camera_ob_eval = DEG_get_evaluated_object(depsgraph, camera_ob);
  float co[3]; /* the new location to apply */
  float scale; /* only for ortho cameras */

  if (BKE_camera_view_frame_fit_to_scene(depsgraph, scene, camera_ob_eval, co, &scale)) {
    ObjectTfmProtectedChannels obtfm;
    float obmat_new[4][4];

    if ((camera_ob_eval->type == OB_CAMERA) &&
        (((Camera *)camera_ob_eval->data)->type == CAM_ORTHO)) {
      ((Camera *)camera_ob->data)->ortho_scale = scale;
    }

    copy_m4_m4(obmat_new, camera_ob_eval->obmat);
    copy_v3_v3(obmat_new[3], co);

    /* only touch location */
    BKE_object_tfm_protected_backup(camera_ob, &obtfm);
    BKE_object_apply_mat4(camera_ob, obmat_new, true, true);
    BKE_object_tfm_protected_restore(camera_ob, &obtfm, OB_LOCK_SCALE | OB_LOCK_ROT4D);

    /* notifiers */
    DEG_id_tag_update_ex(bmain, &camera_ob->id, ID_RECALC_TRANSFORM);

    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth Buffer Utilities
 * \{ */

float ED_view3d_depth_read_cached(const ViewContext *vc, const int mval[2])
{
  ViewDepths *vd = vc->rv3d->depths;

  int x = mval[0];
  int y = mval[1];

  if (vd && vd->depths && x > 0 && y > 0 && x < vd->w && y < vd->h) {
    return vd->depths[y * vd->w + x];
  }

  BLI_assert(1.0 <= vd->depth_range[1]);
  return 1.0f;
}

bool ED_view3d_depth_read_cached_normal(const ViewContext *vc,
                                        const int mval[2],
                                        float r_normal[3])
{
  /* Note: we could support passing in a radius.
   * For now just read 9 pixels. */

  /* pixels surrounding */
  bool depths_valid[9] = {false};
  float coords[9][3] = {{0}};

  ARegion *region = vc->region;
  const ViewDepths *depths = vc->rv3d->depths;

  for (int x = 0, i = 0; x < 2; x++) {
    for (int y = 0; y < 2; y++) {
      const int mval_ofs[2] = {mval[0] + (x - 1), mval[1] + (y - 1)};

      const double depth = (double)ED_view3d_depth_read_cached(vc, mval_ofs);
      if ((depth > depths->depth_range[0]) && (depth < depths->depth_range[1])) {
        if (ED_view3d_depth_unproject(region, mval_ofs, depth, coords[i])) {
          depths_valid[i] = true;
        }
      }
      i++;
    }
  }

  const int edges[2][6][2] = {
      /* x edges */
      {{0, 1}, {1, 2}, {3, 4}, {4, 5}, {6, 7}, {7, 8}},
      /* y edges */
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

bool ED_view3d_depth_unproject(const ARegion *region,
                               const int mval[2],
                               const double depth,
                               float r_location_world[3])
{
  float centx = (float)mval[0] + 0.5f;
  float centy = (float)mval[1] + 0.5f;
  return ED_view3d_unproject(region, centx, centy, depth, r_location_world);
}

void ED_view3d_depth_tag_update(RegionView3D *rv3d)
{
  if (rv3d->depths) {
    rv3d->depths->damaged = true;
  }
}

/** \} */
