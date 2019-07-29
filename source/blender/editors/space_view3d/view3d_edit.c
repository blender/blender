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
 * 3D view manipulation/operators.
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_font.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_transform_snap_object_context.h"

#include "UI_resources.h"

#include "PIL_time.h"

#include "view3d_intern.h" /* own include */

enum {
  HAS_TRANSLATE = (1 << 0),
  HAS_ROTATE = (1 << 0),
};

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Properties
 * \{ */

enum eV3D_OpPropFlag {
  V3D_OP_PROP_MOUSE_CO = (1 << 0),
  V3D_OP_PROP_DELTA = (1 << 1),
  V3D_OP_PROP_USE_ALL_REGIONS = (1 << 2),
  V3D_OP_PROP_USE_MOUSE_INIT = (1 << 3),
};

static void view3d_operator_properties_common(wmOperatorType *ot, const enum eV3D_OpPropFlag flag)
{
  if (flag & V3D_OP_PROP_MOUSE_CO) {
    PropertyRNA *prop;
    prop = RNA_def_int(ot->srna, "mx", 0, 0, INT_MAX, "Region Position X", "", 0, INT_MAX);
    RNA_def_property_flag(prop, PROP_HIDDEN);
    prop = RNA_def_int(ot->srna, "my", 0, 0, INT_MAX, "Region Position Y", "", 0, INT_MAX);
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }
  if (flag & V3D_OP_PROP_DELTA) {
    RNA_def_int(ot->srna, "delta", 0, INT_MIN, INT_MAX, "Delta", "", INT_MIN, INT_MAX);
  }
  if (flag & V3D_OP_PROP_USE_ALL_REGIONS) {
    PropertyRNA *prop;
    prop = RNA_def_boolean(
        ot->srna, "use_all_regions", 0, "All Regions", "View selected for all regions");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  }
  if (flag & V3D_OP_PROP_USE_MOUSE_INIT) {
    WM_operator_properties_use_cursor_init(ot);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic View Operator Custom-Data
 * \{ */

typedef struct ViewOpsData {
  /** Context pointers (assigned by #viewops_data_alloc). */
  Main *bmain;
  Scene *scene;
  ScrArea *sa;
  ARegion *ar;
  View3D *v3d;
  RegionView3D *rv3d;
  Depsgraph *depsgraph;

  /** Needed for continuous zoom. */
  wmTimer *timer;

  /** Viewport state on initialization, don't change afterwards. */
  struct {
    float dist;
    float camzoom;
    float quat[4];
    /** #wmEvent.x, y. */
    int event_xy[2];
    /** Offset to use when #VIEWOPS_FLAG_USE_MOUSE_INIT is not set.
     * so we can simulate pressing in the middle of the screen. */
    int event_xy_offset[2];
    /** #wmEvent.type that triggered the operator. */
    int event_type;
    float ofs[3];
    /** Initial distance to 'ofs'. */
    float zfac;

    /** Trackball rotation only. */
    float trackvec[3];
    /** Dolly only. */
    float mousevec[3];
  } init;

  /** Previous state (previous modal event handled). */
  struct {
    int event_xy[2];
    /** For operators that use time-steps (continuous zoom). */
    double time;
  } prev;

  /** Current state. */
  struct {
    /** Working copy of #RegionView3D.viewquat, needed for rotation calculation
     * so we can apply snap to the view-port while keeping the unsnapped rotation
     * here to use when snap is disabled and for continued calculation. */
    float viewquat[4];
  } curr;

  float reverse;
  bool axis_snap; /* view rotate only */

  /** Use for orbit selection and auto-dist. */
  float dyn_ofs[3];
  bool use_dyn_ofs;
} ViewOpsData;

#define TRACKBALLSIZE (1.1f)

static void calctrackballvec(const rcti *rect, const int event_xy[2], float vec[3])
{
  const float radius = TRACKBALLSIZE;
  const float t = radius / (float)M_SQRT2;
  float x, y, z, d;

  /* normalize x and y */
  x = BLI_rcti_cent_x(rect) - event_xy[0];
  x /= (float)(BLI_rcti_size_x(rect) / 4);
  y = BLI_rcti_cent_y(rect) - event_xy[1];
  y /= (float)(BLI_rcti_size_y(rect) / 2);
  d = sqrtf(x * x + y * y);
  if (d < t) { /* Inside sphere */
    z = sqrtf(radius * radius - d * d);
  }
  else { /* On hyperbola */
    z = t * t / d;
  }

  vec[0] = x;
  vec[1] = y;
  vec[2] = -z; /* yah yah! */
}

/**
 * Allocate and fill in context pointers for #ViewOpsData
 */
static void viewops_data_alloc(bContext *C, wmOperator *op)
{
  ViewOpsData *vod = MEM_callocN(sizeof(ViewOpsData), "viewops data");

  /* store data */
  op->customdata = vod;
  vod->bmain = CTX_data_main(C);
  vod->depsgraph = CTX_data_depsgraph(C);
  vod->scene = CTX_data_scene(C);
  vod->sa = CTX_wm_area(C);
  vod->ar = CTX_wm_region(C);
  vod->v3d = vod->sa->spacedata.first;
  vod->rv3d = vod->ar->regiondata;
}

void view3d_orbit_apply_dyn_ofs(float r_ofs[3],
                                const float ofs_init[3],
                                const float viewquat_old[4],
                                const float viewquat_new[4],
                                const float dyn_ofs[3])
{
  float q[4];
  invert_qt_qt_normalized(q, viewquat_old);
  mul_qt_qtqt(q, q, viewquat_new);

  invert_qt_normalized(q);

  sub_v3_v3v3(r_ofs, ofs_init, dyn_ofs);
  mul_qt_v3(q, r_ofs);
  add_v3_v3(r_ofs, dyn_ofs);
}

static bool view3d_orbit_calc_center(bContext *C, float r_dyn_ofs[3])
{
  static float lastofs[3] = {0, 0, 0};
  bool is_set = false;

  const Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  View3D *v3d = CTX_wm_view3d(C);
  Object *ob_act_eval = OBACT(view_layer_eval);
  Object *ob_act = DEG_get_original_object(ob_act_eval);

  if (ob_act && (ob_act->mode & OB_MODE_ALL_PAINT) &&
      /* with weight-paint + pose-mode, fall through to using calculateTransformCenter */
      ((ob_act->mode & OB_MODE_WEIGHT_PAINT) && BKE_object_pose_armature_get(ob_act)) == 0) {
    /* in case of sculpting use last average stroke position as a rotation
     * center, in other cases it's not clear what rotation center shall be
     * so just rotate around object origin
     */
    if (ob_act->mode &
        (OB_MODE_SCULPT | OB_MODE_TEXTURE_PAINT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
      float stroke[3];
      BKE_paint_stroke_get_average(scene, ob_act_eval, stroke);
      copy_v3_v3(lastofs, stroke);
    }
    else {
      copy_v3_v3(lastofs, ob_act_eval->obmat[3]);
    }
    is_set = true;
  }
  else if (ob_act && (ob_act->mode & OB_MODE_EDIT) && (ob_act->type == OB_FONT)) {
    Curve *cu = ob_act_eval->data;
    EditFont *ef = cu->editfont;
    int i;

    zero_v3(lastofs);
    for (i = 0; i < 4; i++) {
      add_v2_v2(lastofs, ef->textcurs[i]);
    }
    mul_v2_fl(lastofs, 1.0f / 4.0f);

    mul_m4_v3(ob_act_eval->obmat, lastofs);

    is_set = true;
  }
  else if (ob_act == NULL || ob_act->mode == OB_MODE_OBJECT) {
    /* object mode use boundbox centers */
    Base *base_eval;
    uint tot = 0;
    float select_center[3];

    zero_v3(select_center);
    for (base_eval = FIRSTBASE(view_layer_eval); base_eval; base_eval = base_eval->next) {
      if (BASE_SELECTED(v3d, base_eval)) {
        /* use the boundbox if we can */
        Object *ob_eval = base_eval->object;

        if (ob_eval->runtime.bb && !(ob_eval->runtime.bb->flag & BOUNDBOX_DIRTY)) {
          float cent[3];

          BKE_boundbox_calc_center_aabb(ob_eval->runtime.bb, cent);

          mul_m4_v3(ob_eval->obmat, cent);
          add_v3_v3(select_center, cent);
        }
        else {
          add_v3_v3(select_center, ob_eval->obmat[3]);
        }
        tot++;
      }
    }
    if (tot) {
      mul_v3_fl(select_center, 1.0f / (float)tot);
      copy_v3_v3(lastofs, select_center);
      is_set = true;
    }
  }
  else {
    /* If there's no selection, lastofs is unmodified and last value since static */
    is_set = calculateTransformCenter(C, V3D_AROUND_CENTER_MEDIAN, lastofs, NULL);
  }

  copy_v3_v3(r_dyn_ofs, lastofs);

  return is_set;
}

enum eViewOpsFlag {
  /** When enabled, rotate around the selection. */
  VIEWOPS_FLAG_ORBIT_SELECT = (1 << 0),
  /** When enabled, use the depth under the cursor for navigation. */
  VIEWOPS_FLAG_DEPTH_NAVIGATE = (1 << 1),
  /**
   * When enabled run #ED_view3d_persp_ensure this may switch out of
   * camera view when orbiting or switch from ortho to perspective when auto-persp is enabled.
   * Some operations don't require this (view zoom/pan or ndof where subtle rotation is common
   * so we don't want it to trigger auto-perspective). */
  VIEWOPS_FLAG_PERSP_ENSURE = (1 << 2),
  /** When set, ignore any options that depend on initial cursor location. */
  VIEWOPS_FLAG_USE_MOUSE_INIT = (1 << 3),
};

static enum eViewOpsFlag viewops_flag_from_args(bool use_select, bool use_depth)
{
  enum eViewOpsFlag flag = 0;
  if (use_select) {
    flag |= VIEWOPS_FLAG_ORBIT_SELECT;
  }
  if (use_depth) {
    flag |= VIEWOPS_FLAG_DEPTH_NAVIGATE;
  }

  return flag;
}

static enum eViewOpsFlag viewops_flag_from_prefs(void)
{
  return viewops_flag_from_args((U.uiflag & USER_ORBIT_SELECTION) != 0,
                                (U.uiflag & USER_DEPTH_NAVIGATE) != 0);
}

/**
 * Calculate the values for #ViewOpsData
 */
static void viewops_data_create(bContext *C,
                                wmOperator *op,
                                const wmEvent *event,
                                enum eViewOpsFlag viewops_flag)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  ViewOpsData *vod = op->customdata;
  RegionView3D *rv3d = vod->rv3d;

  /* Could do this more nicely. */
  if ((viewops_flag & VIEWOPS_FLAG_USE_MOUSE_INIT) == 0) {
    viewops_flag &= ~VIEWOPS_FLAG_DEPTH_NAVIGATE;
  }

  /* we need the depth info before changing any viewport options */
  if (viewops_flag & VIEWOPS_FLAG_DEPTH_NAVIGATE) {
    float fallback_depth_pt[3];

    view3d_operator_needs_opengl(C); /* needed for zbuf drawing */

    negate_v3_v3(fallback_depth_pt, rv3d->ofs);

    vod->use_dyn_ofs = ED_view3d_autodist(
        depsgraph, vod->ar, vod->v3d, event->mval, vod->dyn_ofs, true, fallback_depth_pt);
  }
  else {
    vod->use_dyn_ofs = false;
  }

  if (viewops_flag & VIEWOPS_FLAG_PERSP_ENSURE) {
    if (ED_view3d_persp_ensure(depsgraph, vod->v3d, vod->ar)) {
      /* If we're switching from camera view to the perspective one,
       * need to tag viewport update, so camera vuew and borders
       * are properly updated.
       */
      ED_region_tag_redraw(vod->ar);
    }
  }

  /* set the view from the camera, if view locking is enabled.
   * we may want to make this optional but for now its needed always */
  ED_view3d_camera_lock_init(depsgraph, vod->v3d, vod->rv3d);

  vod->init.dist = rv3d->dist;
  vod->init.camzoom = rv3d->camzoom;
  copy_qt_qt(vod->init.quat, rv3d->viewquat);
  vod->init.event_xy[0] = vod->prev.event_xy[0] = event->x;
  vod->init.event_xy[1] = vod->prev.event_xy[1] = event->y;

  if (viewops_flag & VIEWOPS_FLAG_USE_MOUSE_INIT) {
    vod->init.event_xy_offset[0] = 0;
    vod->init.event_xy_offset[1] = 0;
  }
  else {
    /* Simulate the event starting in the middle of the region. */
    vod->init.event_xy_offset[0] = BLI_rcti_cent_x(&vod->ar->winrct) - event->x;
    vod->init.event_xy_offset[1] = BLI_rcti_cent_y(&vod->ar->winrct) - event->y;
  }

  vod->init.event_type = event->type;
  copy_v3_v3(vod->init.ofs, rv3d->ofs);

  copy_qt_qt(vod->curr.viewquat, rv3d->viewquat);

  if (viewops_flag & VIEWOPS_FLAG_ORBIT_SELECT) {
    float ofs[3];
    if (view3d_orbit_calc_center(C, ofs) || (vod->use_dyn_ofs == false)) {
      vod->use_dyn_ofs = true;
      negate_v3_v3(vod->dyn_ofs, ofs);
      viewops_flag &= ~VIEWOPS_FLAG_DEPTH_NAVIGATE;
    }
  }

  if (viewops_flag & VIEWOPS_FLAG_DEPTH_NAVIGATE) {
    if (vod->use_dyn_ofs) {
      if (rv3d->is_persp) {
        float my_origin[3]; /* original G.vd->ofs */
        float my_pivot[3];  /* view */
        float dvec[3];

        /* locals for dist correction */
        float mat[3][3];
        float upvec[3];

        negate_v3_v3(my_origin, rv3d->ofs); /* ofs is flipped */

        /* Set the dist value to be the distance from this 3d point this means youll
         * always be able to zoom into it and panning wont go bad when dist was zero */

        /* remove dist value */
        upvec[0] = upvec[1] = 0;
        upvec[2] = rv3d->dist;
        copy_m3_m4(mat, rv3d->viewinv);

        mul_m3_v3(mat, upvec);
        sub_v3_v3v3(my_pivot, rv3d->ofs, upvec);
        negate_v3(my_pivot); /* ofs is flipped */

        /* find a new ofs value that is along the view axis
         * (rather than the mouse location) */
        closest_to_line_v3(dvec, vod->dyn_ofs, my_pivot, my_origin);
        vod->init.dist = rv3d->dist = len_v3v3(my_pivot, dvec);

        negate_v3_v3(rv3d->ofs, dvec);
      }
      else {
        const float mval_ar_mid[2] = {(float)vod->ar->winx / 2.0f, (float)vod->ar->winy / 2.0f};

        ED_view3d_win_to_3d(vod->v3d, vod->ar, vod->dyn_ofs, mval_ar_mid, rv3d->ofs);
        negate_v3(rv3d->ofs);
      }
      negate_v3(vod->dyn_ofs);
      copy_v3_v3(vod->init.ofs, rv3d->ofs);
    }
  }

  /* For dolly */
  ED_view3d_win_to_vector(vod->ar, (const float[2]){UNPACK2(event->mval)}, vod->init.mousevec);

  {
    const int event_xy_offset[2] = {
        event->x + vod->init.event_xy_offset[0],
        event->y + vod->init.event_xy_offset[1],
    };
    /* For rotation with trackball rotation. */
    calctrackballvec(&vod->ar->winrct, event_xy_offset, vod->init.trackvec);
  }

  {
    float tvec[3];
    negate_v3_v3(tvec, rv3d->ofs);
    vod->init.zfac = ED_view3d_calc_zfac(rv3d, tvec, NULL);
  }

  vod->reverse = 1.0f;
  if (rv3d->persmat[2][1] < 0.0f) {
    vod->reverse = -1.0f;
  }

  rv3d->rflag |= RV3D_NAVIGATING;
}

static void viewops_data_free(bContext *C, wmOperator *op)
{
  ARegion *ar;
#if 0
  Paint *p = BKE_paint_get_active_from_context(C);
#endif
  if (op->customdata) {
    ViewOpsData *vod = op->customdata;
    ar = vod->ar;
    vod->rv3d->rflag &= ~RV3D_NAVIGATING;

    if (vod->timer) {
      WM_event_remove_timer(CTX_wm_manager(C), vod->timer->win, vod->timer);
    }

    MEM_freeN(vod);
    op->customdata = NULL;
  }
  else {
    ar = CTX_wm_region(C);
  }

#if 0
  if (p && (p->flags & PAINT_FAST_NAVIGATE))
#endif
  {
    ED_region_tag_redraw(ar);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Rotate Operator
 * \{ */

enum {
  VIEW_PASS = 0,
  VIEW_APPLY,
  VIEW_CONFIRM,
};

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
  VIEW_MODAL_CONFIRM = 1, /* used for all view operations */
  VIEWROT_MODAL_AXIS_SNAP_ENABLE = 2,
  VIEWROT_MODAL_AXIS_SNAP_DISABLE = 3,
  VIEWROT_MODAL_SWITCH_ZOOM = 4,
  VIEWROT_MODAL_SWITCH_MOVE = 5,
  VIEWROT_MODAL_SWITCH_ROTATE = 6,
};

/* called in transform_ops.c, on each regeneration of keymaps  */
void viewrotate_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_AXIS_SNAP_ENABLE, "AXIS_SNAP_ENABLE", 0, "Axis Snap", ""},
      {VIEWROT_MODAL_AXIS_SNAP_DISABLE, "AXIS_SNAP_DISABLE", 0, "Axis Snap (Off)", ""},

      {VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Rotate Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_add(keyconf, "View3D Rotate Modal", modal_items);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_rotate");
}

static void viewrotate_apply_dyn_ofs(ViewOpsData *vod, const float viewquat_new[4])
{
  if (vod->use_dyn_ofs) {
    RegionView3D *rv3d = vod->rv3d;
    view3d_orbit_apply_dyn_ofs(
        rv3d->ofs, vod->init.ofs, vod->init.quat, viewquat_new, vod->dyn_ofs);
  }
}

static void viewrotate_apply_snap(ViewOpsData *vod)
{
  const float axis_limit = DEG2RADF(45 / 3);

  RegionView3D *rv3d = vod->rv3d;

  float viewquat_inv[4];
  float zaxis[3] = {0, 0, 1};
  float zaxis_best[3];
  int x, y, z;
  bool found = false;

  invert_qt_qt_normalized(viewquat_inv, vod->curr.viewquat);

  mul_qt_v3(viewquat_inv, zaxis);
  normalize_v3(zaxis);

  for (x = -1; x < 2; x++) {
    for (y = -1; y < 2; y++) {
      for (z = -1; z < 2; z++) {
        if (x || y || z) {
          float zaxis_test[3] = {x, y, z};

          normalize_v3(zaxis_test);

          if (angle_normalized_v3v3(zaxis_test, zaxis) < axis_limit) {
            copy_v3_v3(zaxis_best, zaxis_test);
            found = true;
          }
        }
      }
    }
  }

  if (found) {

    /* find the best roll */
    float quat_roll[4], quat_final[4], quat_best[4], quat_snap[4];
    float viewquat_align[4];     /* viewquat aligned to zaxis_best */
    float viewquat_align_inv[4]; /* viewquat aligned to zaxis_best */
    float best_angle = axis_limit;
    int j;

    /* viewquat_align is the original viewquat aligned to the snapped axis
     * for testing roll */
    rotation_between_vecs_to_quat(viewquat_align, zaxis_best, zaxis);
    normalize_qt(viewquat_align);
    mul_qt_qtqt(viewquat_align, vod->curr.viewquat, viewquat_align);
    normalize_qt(viewquat_align);
    invert_qt_qt_normalized(viewquat_align_inv, viewquat_align);

    vec_to_quat(quat_snap, zaxis_best, OB_NEGZ, OB_POSY);
    normalize_qt(quat_snap);
    invert_qt_normalized(quat_snap);

    /* check if we can find the roll */
    found = false;

    /* find best roll */
    for (j = 0; j < 8; j++) {
      float angle;
      float xaxis1[3] = {1, 0, 0};
      float xaxis2[3] = {1, 0, 0};
      float quat_final_inv[4];

      axis_angle_to_quat(quat_roll, zaxis_best, (float)j * DEG2RADF(45.0f));
      normalize_qt(quat_roll);

      mul_qt_qtqt(quat_final, quat_snap, quat_roll);
      normalize_qt(quat_final);

      /* compare 2 vector angles to find the least roll */
      invert_qt_qt_normalized(quat_final_inv, quat_final);
      mul_qt_v3(viewquat_align_inv, xaxis1);
      mul_qt_v3(quat_final_inv, xaxis2);
      angle = angle_v3v3(xaxis1, xaxis2);

      if (angle <= best_angle) {
        found = true;
        best_angle = angle;
        copy_qt_qt(quat_best, quat_final);
      }
    }

    if (found) {
      /* lock 'quat_best' to an axis view if we can */
      rv3d->view = ED_view3d_quat_to_axis_view(quat_best, 0.01f);
      if (rv3d->view != RV3D_VIEW_USER) {
        ED_view3d_quat_from_axis_view(rv3d->view, quat_best);
      }
    }
    else {
      copy_qt_qt(quat_best, viewquat_align);
    }

    copy_qt_qt(rv3d->viewquat, quat_best);

    viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
  }
}

static void viewrotate_apply(ViewOpsData *vod, const int event_xy[2])
{
  RegionView3D *rv3d = vod->rv3d;

  rv3d->view = RV3D_VIEW_USER; /* need to reset every time because of view snapping */

  if (U.flag & USER_TRACKBALL) {
    float axis[3], q1[4], dvec[3], newvec[3];
    float angle;

    {
      const int event_xy_offset[2] = {
          event_xy[0] + vod->init.event_xy_offset[0],
          event_xy[1] + vod->init.event_xy_offset[1],
      };
      calctrackballvec(&vod->ar->winrct, event_xy_offset, newvec);
    }

    sub_v3_v3v3(dvec, newvec, vod->init.trackvec);

    angle = (len_v3(dvec) / (2.0f * TRACKBALLSIZE)) * (float)M_PI;

    /* Allow for rotation beyond the interval [-pi, pi] */
    angle = angle_wrap_rad(angle);

    /* This relation is used instead of the actual angle between vectors
     * so that the angle of rotation is linearly proportional to
     * the distance that the mouse is dragged. */

    cross_v3_v3v3(axis, vod->init.trackvec, newvec);
    axis_angle_to_quat(q1, axis, angle);

    mul_qt_qtqt(vod->curr.viewquat, q1, vod->init.quat);

    viewrotate_apply_dyn_ofs(vod, vod->curr.viewquat);
  }
  else {
    /* New turntable view code by John Aughey */
    float quat_local_x[4], quat_global_z[4];
    float m[3][3];
    float m_inv[3][3];
    const float zvec_global[3] = {0.0f, 0.0f, 1.0f};
    float xaxis[3];

    /* Sensitivity will control how fast the viewport rotates.  0.007 was
     * obtained experimentally by looking at viewport rotation sensitivities
     * on other modeling programs. */
    /* Perhaps this should be a configurable user parameter. */
    const float sensitivity = 0.007f;

    /* Get the 3x3 matrix and its inverse from the quaternion */
    quat_to_mat3(m, vod->curr.viewquat);
    invert_m3_m3(m_inv, m);

    /* avoid gimble lock */
#if 1
    if (len_squared_v3v3(zvec_global, m_inv[2]) > 0.001f) {
      float fac;
      cross_v3_v3v3(xaxis, zvec_global, m_inv[2]);
      if (dot_v3v3(xaxis, m_inv[0]) < 0) {
        negate_v3(xaxis);
      }
      fac = angle_normalized_v3v3(zvec_global, m_inv[2]) / (float)M_PI;
      fac = fabsf(fac - 0.5f) * 2;
      fac = fac * fac;
      interp_v3_v3v3(xaxis, xaxis, m_inv[0], fac);
    }
    else {
      copy_v3_v3(xaxis, m_inv[0]);
    }
#else
    copy_v3_v3(xaxis, m_inv[0]);
#endif

    /* Determine the direction of the x vector (for rotating up and down) */
    /* This can likely be computed directly from the quaternion. */

    /* Perform the up/down rotation */
    axis_angle_to_quat(quat_local_x, xaxis, sensitivity * -(event_xy[1] - vod->prev.event_xy[1]));
    mul_qt_qtqt(quat_local_x, vod->curr.viewquat, quat_local_x);

    /* Perform the orbital rotation */
    axis_angle_to_quat_single(
        quat_global_z, 'Z', sensitivity * vod->reverse * (event_xy[0] - vod->prev.event_xy[0]));
    mul_qt_qtqt(vod->curr.viewquat, quat_local_x, quat_global_z);

    viewrotate_apply_dyn_ofs(vod, vod->curr.viewquat);
  }

  /* avoid precision loss over time */
  normalize_qt(vod->curr.viewquat);

  /* use a working copy so view rotation locking doesn't overwrite the locked
   * rotation back into the view we calculate with */
  copy_qt_qt(rv3d->viewquat, vod->curr.viewquat);

  /* check for view snap,
   * note: don't apply snap to vod->viewquat so the view wont jam up */
  if (vod->axis_snap) {
    viewrotate_apply_snap(vod);
  }
  vod->prev.event_xy[0] = event_xy[0];
  vod->prev.event_xy[1] = event_xy[1];

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, rv3d);

  ED_region_tag_redraw(vod->ar);
}

static int viewrotate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_AXIS_SNAP_ENABLE:
        vod->axis_snap = true;
        event_code = VIEW_APPLY;
        break;
      case VIEWROT_MODAL_AXIS_SNAP_DISABLE:
        vod->axis_snap = false;
        event_code = VIEW_APPLY;
        break;
      case VIEWROT_MODAL_SWITCH_ZOOM:
        WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewrotate_apply(vod, &event->x);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    ED_view3d_depth_tag_update(vod->rv3d);
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op);
  }

  return ret;
}

static int viewrotate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* makes op->customdata */
  viewops_data_alloc(C, op);
  vod = op->customdata;

  /* poll should check but in some cases fails, see poll func for details */
  if (vod->rv3d->viewlock & RV3D_LOCKED) {
    viewops_data_free(C, op);
    return OPERATOR_PASS_THROUGH;
  }

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  viewops_data_create(C,
                      op,
                      event,
                      viewops_flag_from_prefs() | VIEWOPS_FLAG_PERSP_ENSURE |
                          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));

  if (ELEM(event->type, MOUSEPAN, MOUSEROTATE)) {
    /* Rotate direction we keep always same */
    int event_xy[2];

    if (event->type == MOUSEPAN) {
      if (U.uiflag2 & USER_TRACKPAD_NATURAL) {
        event_xy[0] = 2 * event->x - event->prevx;
        event_xy[1] = 2 * event->y - event->prevy;
      }
      else {
        event_xy[0] = event->prevx;
        event_xy[1] = event->prevy;
      }
    }
    else {
      /* MOUSEROTATE performs orbital rotation, so y axis delta is set to 0 */
      event_xy[0] = event->prevx;
      event_xy[1] = event->y;
    }

    viewrotate_apply(vod, event_xy);
    ED_view3d_depth_tag_update(vod->rv3d);

    viewops_data_free(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
}

/* test for unlocked camera view in quad view */
static bool view3d_camera_user_poll(bContext *C)
{
  View3D *v3d;
  ARegion *ar;

  if (ED_view3d_context_user_region(C, &v3d, &ar)) {
    RegionView3D *rv3d = ar->regiondata;
    if (rv3d->persp == RV3D_CAMOB) {
      return 1;
    }
  }

  return 0;
}

static bool view3d_lock_poll(bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d) {
      return ED_view3d_offset_lock_check(v3d, rv3d);
    }
  }
  return false;
}

static void viewrotate_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op);
}

void VIEW3D_OT_rotate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rotate View";
  ot->description = "Rotate the view";
  ot->idname = "VIEW3D_OT_rotate";

  /* api callbacks */
  ot->invoke = viewrotate_invoke;
  ot->modal = viewrotate_modal;
  ot->poll = ED_operator_region_view3d_active;
  ot->cancel = viewrotate_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Utility Functions
 * \{ */

#ifdef WITH_INPUT_NDOF
#  define NDOF_HAS_TRANSLATE ((!ED_view3d_offset_lock_check(v3d, rv3d)) && !is_zero_v3(ndof->tvec))
#  define NDOF_HAS_ROTATE (((rv3d->viewlock & RV3D_LOCKED) == 0) && !is_zero_v3(ndof->rvec))

/**
 * \param depth_pt: A point to calculate the depth (in perspective mode)
 */
static float view3d_ndof_pan_speed_calc_ex(RegionView3D *rv3d, const float depth_pt[3])
{
  float speed = rv3d->pixsize * NDOF_PIXELS_PER_SECOND;

  if (rv3d->is_persp) {
    speed *= ED_view3d_calc_zfac(rv3d, depth_pt, NULL);
  }

  return speed;
}

static float view3d_ndof_pan_speed_calc_from_dist(RegionView3D *rv3d, const float dist)
{
  float viewinv[4];
  float tvec[3];

  BLI_assert(dist >= 0.0f);

  copy_v3_fl3(tvec, 0.0f, 0.0f, dist);
  /* rv3d->viewinv isn't always valid */
#  if 0
  mul_mat3_m4_v3(rv3d->viewinv, tvec);
#  else
  invert_qt_qt_normalized(viewinv, rv3d->viewquat);
  mul_qt_v3(viewinv, tvec);
#  endif

  return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

static float view3d_ndof_pan_speed_calc(RegionView3D *rv3d)
{
  float tvec[3];
  negate_v3_v3(tvec, rv3d->ofs);

  return view3d_ndof_pan_speed_calc_ex(rv3d, tvec);
}

/**
 * Zoom and pan in the same function since sometimes zoom is interpreted as dolly (pan forward).
 *
 * \param has_zoom: zoom, otherwise dolly,
 * often `!rv3d->is_persp` since it doesn't make sense to dolly in ortho.
 */
static void view3d_ndof_pan_zoom(const struct wmNDOFMotionData *ndof,
                                 ScrArea *sa,
                                 ARegion *ar,
                                 const bool has_translate,
                                 const bool has_zoom)
{
  RegionView3D *rv3d = ar->regiondata;
  float view_inv[4];
  float pan_vec[3];

  if (has_translate == false && has_zoom == false) {
    return;
  }

  WM_event_ndof_pan_get(ndof, pan_vec, false);

  if (has_zoom) {
    /* zoom with Z */

    /* Zoom!
     * velocity should be proportional to the linear velocity attained by rotational motion
     * of same strength [got that?] proportional to `arclength = radius * angle`.
     */

    pan_vec[2] = 0.0f;

    /* "zoom in" or "translate"? depends on zoom mode in user settings? */
    if (ndof->tvec[2]) {
      float zoom_distance = rv3d->dist * ndof->dt * ndof->tvec[2];

      if (U.ndof_flag & NDOF_ZOOM_INVERT) {
        zoom_distance = -zoom_distance;
      }

      rv3d->dist += zoom_distance;
    }
  }
  else {
    /* dolly with Z */

    /* all callers must check */
    if (has_translate) {
      BLI_assert(ED_view3d_offset_lock_check((View3D *)sa->spacedata.first, rv3d) == false);
    }
  }

  if (has_translate) {
    const float speed = view3d_ndof_pan_speed_calc(rv3d);

    mul_v3_fl(pan_vec, speed * ndof->dt);

    /* transform motion from view to world coordinates */
    invert_qt_qt_normalized(view_inv, rv3d->viewquat);
    mul_qt_v3(view_inv, pan_vec);

    /* move center of view opposite of hand motion (this is camera mode, not object mode) */
    sub_v3_v3(rv3d->ofs, pan_vec);

    if (rv3d->viewlock & RV3D_BOXVIEW) {
      view3d_boxview_sync(sa, ar);
    }
  }
}

static void view3d_ndof_orbit(const struct wmNDOFMotionData *ndof,
                              ScrArea *sa,
                              ARegion *ar,
                              ViewOpsData *vod,
                              const bool apply_dyn_ofs)
{
  View3D *v3d = sa->spacedata.first;
  RegionView3D *rv3d = ar->regiondata;

  float view_inv[4];

  BLI_assert((rv3d->viewlock & RV3D_LOCKED) == 0);

  ED_view3d_persp_ensure(vod->depsgraph, v3d, ar);

  rv3d->view = RV3D_VIEW_USER;

  invert_qt_qt_normalized(view_inv, rv3d->viewquat);

  if (U.ndof_flag & NDOF_TURNTABLE) {
    float rot[3];

    /* turntable view code by John Aughey, adapted for 3D mouse by [mce] */
    float angle, quat[4];
    float xvec[3] = {1, 0, 0};

    /* only use XY, ignore Z */
    WM_event_ndof_rotate_get(ndof, rot);

    /* Determine the direction of the x vector (for rotating up and down) */
    mul_qt_v3(view_inv, xvec);

    /* Perform the up/down rotation */
    angle = ndof->dt * rot[0];
    axis_angle_to_quat(quat, xvec, angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);

    /* Perform the orbital rotation */
    angle = ndof->dt * rot[1];

    /* update the onscreen doo-dad */
    rv3d->rot_angle = angle;
    rv3d->rot_axis[0] = 0;
    rv3d->rot_axis[1] = 0;
    rv3d->rot_axis[2] = 1;

    axis_angle_to_quat_single(quat, 'Z', angle);
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }
  else {
    float quat[4];
    float axis[3];
    float angle = WM_event_ndof_to_axis_angle(ndof, axis);

    /* transform rotation axis from view to world coordinates */
    mul_qt_v3(view_inv, axis);

    /* update the onscreen doo-dad */
    rv3d->rot_angle = angle;
    copy_v3_v3(rv3d->rot_axis, axis);

    axis_angle_to_quat(quat, axis, angle);

    /* apply rotation */
    mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, quat);
  }

  if (apply_dyn_ofs) {
    viewrotate_apply_dyn_ofs(vod, rv3d->viewquat);
  }
}

/**
 * Called from both fly mode and walk mode,
 */
void view3d_ndof_fly(const wmNDOFMotionData *ndof,
                     View3D *v3d,
                     RegionView3D *rv3d,
                     const bool use_precision,
                     const short protectflag,
                     bool *r_has_translate,
                     bool *r_has_rotate)
{
  bool has_translate = NDOF_HAS_TRANSLATE;
  bool has_rotate = NDOF_HAS_ROTATE;

  float view_inv[4];
  invert_qt_qt_normalized(view_inv, rv3d->viewquat);

  rv3d->rot_angle = 0.0f; /* disable onscreen rotation doo-dad */

  if (has_translate) {
    /* ignore real 'dist' since fly has its own speed settings,
     * also its overwritten at this point. */
    float speed = view3d_ndof_pan_speed_calc_from_dist(rv3d, 1.0f);
    float trans[3], trans_orig_y;

    if (use_precision) {
      speed *= 0.2f;
    }

    WM_event_ndof_pan_get(ndof, trans, false);
    mul_v3_fl(trans, speed * ndof->dt);
    trans_orig_y = trans[1];

    if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
      trans[1] = 0.0f;
    }

    /* transform motion from view to world coordinates */
    mul_qt_v3(view_inv, trans);

    if (U.ndof_flag & NDOF_FLY_HELICOPTER) {
      /* replace world z component with device y (yes it makes sense) */
      trans[2] = trans_orig_y;
    }

    if (rv3d->persp == RV3D_CAMOB) {
      /* respect camera position locks */
      if (protectflag & OB_LOCK_LOCX) {
        trans[0] = 0.0f;
      }
      if (protectflag & OB_LOCK_LOCY) {
        trans[1] = 0.0f;
      }
      if (protectflag & OB_LOCK_LOCZ) {
        trans[2] = 0.0f;
      }
    }

    if (!is_zero_v3(trans)) {
      /* move center of view opposite of hand motion
       * (this is camera mode, not object mode) */
      sub_v3_v3(rv3d->ofs, trans);
      has_translate = true;
    }
    else {
      has_translate = false;
    }
  }

  if (has_rotate) {
    const float turn_sensitivity = 1.0f;

    float rotation[4];
    float axis[3];
    float angle = turn_sensitivity * WM_event_ndof_to_axis_angle(ndof, axis);

    if (fabsf(angle) > 0.0001f) {
      has_rotate = true;

      if (use_precision) {
        angle *= 0.2f;
      }

      /* transform rotation axis from view to world coordinates */
      mul_qt_v3(view_inv, axis);

      /* apply rotation to view */
      axis_angle_to_quat(rotation, axis, angle);
      mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);

      if (U.ndof_flag & NDOF_LOCK_HORIZON) {
        /* force an upright viewpoint
         * TODO: make this less... sudden */
        float view_horizon[3] = {1.0f, 0.0f, 0.0f};    /* view +x */
        float view_direction[3] = {0.0f, 0.0f, -1.0f}; /* view -z (into screen) */

        /* find new inverse since viewquat has changed */
        invert_qt_qt_normalized(view_inv, rv3d->viewquat);
        /* could apply reverse rotation to existing view_inv to save a few cycles */

        /* transform view vectors to world coordinates */
        mul_qt_v3(view_inv, view_horizon);
        mul_qt_v3(view_inv, view_direction);

        /* find difference between view & world horizons
         * true horizon lives in world xy plane, so look only at difference in z */
        angle = -asinf(view_horizon[2]);

        /* rotate view so view horizon = world horizon */
        axis_angle_to_quat(rotation, view_direction, angle);
        mul_qt_qtqt(rv3d->viewquat, rv3d->viewquat, rotation);
      }

      rv3d->view = RV3D_VIEW_USER;
    }
    else {
      has_rotate = false;
    }
  }

  *r_has_translate = has_translate;
  *r_has_rotate = has_rotate;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Orbit/Translate Operator
 * \{ */

static int ndof_orbit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const Depsgraph *depsgraph = CTX_data_depsgraph(C);
  ViewOpsData *vod;
  View3D *v3d;
  RegionView3D *rv3d;
  char xform_flag = 0;

  const wmNDOFMotionData *ndof = event->customdata;

  viewops_data_alloc(C, op);
  viewops_data_create(
      C, op, event, viewops_flag_from_args((U.uiflag & USER_ORBIT_SELECTION) != 0, false));
  vod = op->customdata;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  v3d = vod->v3d;
  rv3d = vod->rv3d;

  /* off by default, until changed later this function */
  rv3d->rot_angle = 0.0f;

  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, false);

  if (ndof->progress != P_FINISHING) {
    const bool has_rotation = NDOF_HAS_ROTATE;
    /* if we can't rotate, fallback to translate (locked axis views) */
    const bool has_translate = NDOF_HAS_TRANSLATE && (rv3d->viewlock & RV3D_LOCKED);
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }

    if (has_rotation) {
      view3d_ndof_orbit(ndof, vod->sa, vod->ar, vod, true);
      xform_flag |= HAS_ROTATE;
    }
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(
        v3d, rv3d, C, xform_flag & HAS_ROTATE, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(vod->ar);

  viewops_data_free(C, op);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_ndof_orbit(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Orbit View";
  ot->description = "Orbit the view using the 3D mouse";
  ot->idname = "VIEW3D_OT_ndof_orbit";

  /* api callbacks */
  ot->invoke = ndof_orbit_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Orbit/Zoom Operator
 * \{ */

static int ndof_orbit_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const Depsgraph *depsgraph = CTX_data_depsgraph(C);
  ViewOpsData *vod;
  View3D *v3d;
  RegionView3D *rv3d;
  char xform_flag = 0;

  const wmNDOFMotionData *ndof = event->customdata;

  viewops_data_alloc(C, op);
  viewops_data_create(
      C, op, event, viewops_flag_from_args((U.uiflag & USER_ORBIT_SELECTION) != 0, false));

  vod = op->customdata;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  v3d = vod->v3d;
  rv3d = vod->rv3d;

  /* off by default, until changed later this function */
  rv3d->rot_angle = 0.0f;

  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, false);

  if (ndof->progress == P_FINISHING) {
    /* pass */
  }
  else if ((rv3d->persp == RV3D_ORTHO) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
    /* if we can't rotate, fallback to translate (locked axis views) */
    const bool has_translate = NDOF_HAS_TRANSLATE;
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && ED_view3d_offset_lock_check(v3d, rv3d);

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, true);
      xform_flag |= HAS_TRANSLATE;
    }
  }
  else if ((U.ndof_flag & NDOF_MODE_ORBIT) || ED_view3d_offset_lock_check(v3d, rv3d)) {
    const bool has_rotation = NDOF_HAS_ROTATE;
    const bool has_zoom = (ndof->tvec[2] != 0.0f);

    if (has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, false, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }

    if (has_rotation) {
      view3d_ndof_orbit(ndof, vod->sa, vod->ar, vod, true);
      xform_flag |= HAS_ROTATE;
    }
  }
  else { /* free/explore (like fly mode) */
    const bool has_rotation = NDOF_HAS_ROTATE;
    const bool has_translate = NDOF_HAS_TRANSLATE;
    const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

    float dist_backup;

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, vod->sa, vod->ar, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }

    dist_backup = rv3d->dist;
    ED_view3d_distance_set(rv3d, 0.0f);

    if (has_rotation) {
      view3d_ndof_orbit(ndof, vod->sa, vod->ar, vod, false);
      xform_flag |= HAS_ROTATE;
    }

    ED_view3d_distance_set(rv3d, dist_backup);
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(
        v3d, rv3d, C, xform_flag & HAS_ROTATE, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(vod->ar);

  viewops_data_free(C, op);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_ndof_orbit_zoom(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Orbit View with Zoom";
  ot->description = "Orbit and zoom the view using the 3D mouse";
  ot->idname = "VIEW3D_OT_ndof_orbit_zoom";

  /* api callbacks */
  ot->invoke = ndof_orbit_zoom_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Pan/Zoom Operator
 * \{ */

static int ndof_pan_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  if (event->type != NDOF_MOTION) {
    return OPERATOR_CANCELLED;
  }

  const Depsgraph *depsgraph = CTX_data_depsgraph(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const wmNDOFMotionData *ndof = event->customdata;
  char xform_flag = 0;

  const bool has_translate = NDOF_HAS_TRANSLATE;
  const bool has_zoom = (ndof->tvec[2] != 0.0f) && !rv3d->is_persp;

  /* we're panning here! so erase any leftover rotation from other operators */
  rv3d->rot_angle = 0.0f;

  if (!(has_translate || has_zoom)) {
    return OPERATOR_CANCELLED;
  }

  ED_view3d_camera_lock_init_ex(depsgraph, v3d, rv3d, false);

  if (ndof->progress != P_FINISHING) {
    ScrArea *sa = CTX_wm_area(C);
    ARegion *ar = CTX_wm_region(C);

    if (has_translate || has_zoom) {
      view3d_ndof_pan_zoom(ndof, sa, ar, has_translate, has_zoom);
      xform_flag |= HAS_TRANSLATE;
    }
  }

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  if (xform_flag) {
    ED_view3d_camera_lock_autokey(v3d, rv3d, C, false, xform_flag & HAS_TRANSLATE);
  }

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_ndof_pan(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Pan View";
  ot->description = "Pan the view with the 3D mouse";
  ot->idname = "VIEW3D_OT_ndof_pan";

  /* api callbacks */
  ot->invoke = ndof_pan_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NDOF Transform All Operator
 * \{ */

/**
 * wraps #ndof_orbit_zoom but never restrict to orbit.
 */
static int ndof_all_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* weak!, but it works */
  const int ndof_flag = U.ndof_flag;
  int ret;

  U.ndof_flag &= ~NDOF_MODE_ORBIT;

  ret = ndof_orbit_zoom_invoke(C, op, event);

  U.ndof_flag = ndof_flag;

  return ret;
}

void VIEW3D_OT_ndof_all(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "NDOF Transform View";
  ot->description = "Pan and rotate the view with the 3D mouse";
  ot->idname = "VIEW3D_OT_ndof_all";

  /* api callbacks */
  ot->invoke = ndof_all_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

#endif /* WITH_INPUT_NDOF */

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Move (Pan) Operator
 * \{ */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */

/* called in transform_ops.c, on each regeneration of keymaps  */
void viewmove_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ZOOM, "SWITCH_TO_ZOOM", 0, "Switch to Zoom"},
      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Move Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_add(keyconf, "View3D Move Modal", modal_items);

  /* items for modal map */
  WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_RELEASE, KM_ANY, 0, VIEW_MODAL_CONFIRM);
  WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, VIEW_MODAL_CONFIRM);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ZOOM);
  WM_modalkeymap_add_item(
      keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_move");
}

static void viewmove_apply(ViewOpsData *vod, int x, int y)
{
  if (ED_view3d_offset_lock_check(vod->v3d, vod->rv3d)) {
    vod->rv3d->ofs_lock[0] -= ((vod->prev.event_xy[0] - x) * 2.0f) / (float)vod->ar->winx;
    vod->rv3d->ofs_lock[1] -= ((vod->prev.event_xy[1] - y) * 2.0f) / (float)vod->ar->winy;
  }
  else if ((vod->rv3d->persp == RV3D_CAMOB) && !ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) {
    const float zoomfac = BKE_screen_view3d_zoom_to_fac(vod->rv3d->camzoom) * 2.0f;
    vod->rv3d->camdx += (vod->prev.event_xy[0] - x) / (vod->ar->winx * zoomfac);
    vod->rv3d->camdy += (vod->prev.event_xy[1] - y) / (vod->ar->winy * zoomfac);
    CLAMP(vod->rv3d->camdx, -1.0f, 1.0f);
    CLAMP(vod->rv3d->camdy, -1.0f, 1.0f);
  }
  else {
    float dvec[3];
    float mval_f[2];

    mval_f[0] = x - vod->prev.event_xy[0];
    mval_f[1] = y - vod->prev.event_xy[1];
    ED_view3d_win_to_delta(vod->ar, mval_f, dvec, vod->init.zfac);

    add_v3_v3(vod->rv3d->ofs, dvec);

    if (vod->rv3d->viewlock & RV3D_BOXVIEW) {
      view3d_boxview_sync(vod->sa, vod->ar);
    }
  }

  vod->prev.event_xy[0] = x;
  vod->prev.event_xy[1] = y;

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->ar);
}

static int viewmove_modal(bContext *C, wmOperator *op, const wmEvent *event)
{

  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ZOOM:
        WM_operator_name_call(C, "VIEW3D_OT_zoom", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewmove_apply(vod, event->x, event->y);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    ED_view3d_depth_tag_update(vod->rv3d);
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op);
  }

  return ret;
}

static int viewmove_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* makes op->customdata */
  viewops_data_alloc(C, op);
  viewops_data_create(C,
                      op,
                      event,
                      (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT) |
                          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));
  vod = op->customdata;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  if (event->type == MOUSEPAN) {
    /* invert it, trackpad scroll follows same principle as 2d windows this way */
    viewmove_apply(vod, 2 * event->x - event->prevx, 2 * event->y - event->prevy);
    ED_view3d_depth_tag_update(vod->rv3d);

    viewops_data_free(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
}

static void viewmove_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op);
}

void VIEW3D_OT_move(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Pan View";
  ot->description = "Move the view";
  ot->idname = "VIEW3D_OT_move";

  /* api callbacks */
  ot->invoke = viewmove_invoke;
  ot->modal = viewmove_modal;
  ot->poll = ED_operator_region_view3d_active;
  ot->cancel = viewmove_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Zoom Operator
 * \{ */

/* viewdolly_modal_keymap has an exact copy of this, apply fixes to both */
/* called in transform_ops.c, on each regeneration of keymaps  */
void viewzoom_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Zoom Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_add(keyconf, "View3D Zoom Modal", modal_items);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
  WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom");
}

/**
 * \param zoom_xy: Optionally zoom to window location
 * (coords compatible w/ #wmEvent.x, y). Use when not NULL.
 */
static void view_zoom_to_window_xy_camera(
    Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *ar, float dfac, const int zoom_xy[2])
{
  RegionView3D *rv3d = ar->regiondata;
  const float zoomfac = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  const float zoomfac_new = clamp_f(
      zoomfac * (1.0f / dfac), RV3D_CAMZOOM_MIN_FACTOR, RV3D_CAMZOOM_MAX_FACTOR);
  const float camzoom_new = BKE_screen_view3d_zoom_from_fac(zoomfac_new);

  if (zoom_xy != NULL) {
    float zoomfac_px;
    rctf camera_frame_old;
    rctf camera_frame_new;

    const float pt_src[2] = {zoom_xy[0], zoom_xy[1]};
    float pt_dst[2];
    float delta_px[2];

    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &camera_frame_old, false);
    BLI_rctf_translate(&camera_frame_old, ar->winrct.xmin, ar->winrct.ymin);

    rv3d->camzoom = camzoom_new;
    CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);

    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &camera_frame_new, false);
    BLI_rctf_translate(&camera_frame_new, ar->winrct.xmin, ar->winrct.ymin);

    BLI_rctf_transform_pt_v(&camera_frame_new, &camera_frame_old, pt_dst, pt_src);
    sub_v2_v2v2(delta_px, pt_dst, pt_src);

    /* translate the camera offset using pixel space delta
     * mapped back to the camera (same logic as panning in camera view) */
    zoomfac_px = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom) * 2.0f;

    rv3d->camdx += delta_px[0] / (ar->winx * zoomfac_px);
    rv3d->camdy += delta_px[1] / (ar->winy * zoomfac_px);
    CLAMP(rv3d->camdx, -1.0f, 1.0f);
    CLAMP(rv3d->camdy, -1.0f, 1.0f);
  }
  else {
    rv3d->camzoom = camzoom_new;
    CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
  }
}

/**
 * \param zoom_xy: Optionally zoom to window location
 * (coords compatible w/ #wmEvent.x, y). Use when not NULL.
 */
static void view_zoom_to_window_xy_3d(ARegion *ar, float dfac, const int zoom_xy[2])
{
  RegionView3D *rv3d = ar->regiondata;
  const float dist_new = rv3d->dist * dfac;

  if (zoom_xy != NULL) {
    float dvec[3];
    float tvec[3];
    float tpos[3];
    float mval_f[2];

    float zfac;

    negate_v3_v3(tpos, rv3d->ofs);

    mval_f[0] = (float)(((zoom_xy[0] - ar->winrct.xmin) * 2) - ar->winx) / 2.0f;
    mval_f[1] = (float)(((zoom_xy[1] - ar->winrct.ymin) * 2) - ar->winy) / 2.0f;

    /* Project cursor position into 3D space */
    zfac = ED_view3d_calc_zfac(rv3d, tpos, NULL);
    ED_view3d_win_to_delta(ar, mval_f, dvec, zfac);

    /* Calculate view target position for dolly */
    add_v3_v3v3(tvec, tpos, dvec);
    negate_v3(tvec);

    /* Offset to target position and dolly */
    copy_v3_v3(rv3d->ofs, tvec);
    rv3d->dist = dist_new;

    /* Calculate final offset */
    madd_v3_v3v3fl(rv3d->ofs, tvec, dvec, dfac);
  }
  else {
    rv3d->dist = dist_new;
  }
}

static float viewzoom_scale_value(const rcti *winrct,
                                  const short viewzoom,
                                  const bool zoom_invert,
                                  const bool zoom_invert_force,
                                  const int xy_curr[2],
                                  const int xy_init[2],
                                  const float val,
                                  const float val_orig,
                                  double *r_timer_lastdraw)
{
  float zfac;

  if (viewzoom == USER_ZOOM_CONT) {
    double time = PIL_check_seconds_timer();
    float time_step = (float)(time - *r_timer_lastdraw);
    float fac;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      fac = (float)(xy_init[0] - xy_curr[0]);
    }
    else {
      fac = (float)(xy_init[1] - xy_curr[1]);
    }

    if (zoom_invert != zoom_invert_force) {
      fac = -fac;
    }

    /* oldstyle zoom */
    zfac = 1.0f + ((fac / 20.0f) * time_step);
    *r_timer_lastdraw = time;
  }
  else if (viewzoom == USER_ZOOM_SCALE) {
    /* method which zooms based on how far you move the mouse */

    const int ctr[2] = {
        BLI_rcti_cent_x(winrct),
        BLI_rcti_cent_y(winrct),
    };
    float len_new = 5 + len_v2v2_int(ctr, xy_curr);
    float len_old = 5 + len_v2v2_int(ctr, xy_init);

    /* intentionally ignore 'zoom_invert' for scale */
    if (zoom_invert_force) {
      SWAP(float, len_new, len_old);
    }

    zfac = val_orig * (len_old / max_ff(len_new, 1.0f)) / val;
  }
  else { /* USER_ZOOM_DOLLY */
    float len_new = 5;
    float len_old = 5;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      len_new += (winrct->xmax - (xy_curr[0]));
      len_old += (winrct->xmax - (xy_init[0]));
    }
    else {
      len_new += (winrct->ymax - (xy_curr[1]));
      len_old += (winrct->ymax - (xy_init[1]));
    }

    if (zoom_invert != zoom_invert_force) {
      SWAP(float, len_new, len_old);
    }

    zfac = val_orig * (2.0f * ((len_new / max_ff(len_old, 1.0f)) - 1.0f) + 1.0f) / val;
  }

  return zfac;
}

static float viewzoom_scale_value_offset(const rcti *winrct,
                                         const short viewzoom,
                                         const bool zoom_invert,
                                         const bool zoom_invert_force,
                                         const int xy_curr[2],
                                         const int xy_init[2],
                                         const int xy_offset[2],
                                         const float val,
                                         const float val_orig,
                                         double *r_timer_lastdraw)
{
  const int xy_curr_offset[2] = {
      xy_curr[0] + xy_offset[0],
      xy_curr[1] + xy_offset[1],
  };
  const int xy_init_offset[2] = {
      xy_init[0] + xy_offset[0],
      xy_init[1] + xy_offset[1],
  };
  return viewzoom_scale_value(winrct,
                              viewzoom,
                              zoom_invert,
                              zoom_invert_force,
                              xy_curr_offset,
                              xy_init_offset,
                              val,
                              val_orig,
                              r_timer_lastdraw);
}

static void viewzoom_apply_camera(ViewOpsData *vod,
                                  const int xy[2],
                                  const short viewzoom,
                                  const bool zoom_invert,
                                  const bool zoom_to_pos)
{
  float zfac;
  float zoomfac_prev = BKE_screen_view3d_zoom_to_fac(vod->init.camzoom) * 2.0f;
  float zoomfac = BKE_screen_view3d_zoom_to_fac(vod->rv3d->camzoom) * 2.0f;

  zfac = viewzoom_scale_value_offset(&vod->ar->winrct,
                                     viewzoom,
                                     zoom_invert,
                                     true,
                                     xy,
                                     vod->init.event_xy,
                                     vod->init.event_xy_offset,
                                     zoomfac,
                                     zoomfac_prev,
                                     &vod->prev.time);

  if (zfac != 1.0f && zfac != 0.0f) {
    /* calculate inverted, then invert again (needed because of camera zoom scaling) */
    zfac = 1.0f / zfac;
    view_zoom_to_window_xy_camera(vod->scene,
                                  vod->depsgraph,
                                  vod->v3d,
                                  vod->ar,
                                  zfac,
                                  zoom_to_pos ? vod->prev.event_xy : NULL);
  }

  ED_region_tag_redraw(vod->ar);
}

static void viewzoom_apply_3d(ViewOpsData *vod,
                              const int xy[2],
                              const short viewzoom,
                              const bool zoom_invert,
                              const bool zoom_to_pos)
{
  float zfac;
  float dist_range[2];

  ED_view3d_dist_range_get(vod->v3d, dist_range);

  zfac = viewzoom_scale_value_offset(&vod->ar->winrct,
                                     viewzoom,
                                     zoom_invert,
                                     false,
                                     xy,
                                     vod->init.event_xy,
                                     vod->init.event_xy_offset,
                                     vod->rv3d->dist,
                                     vod->init.dist,
                                     &vod->prev.time);

  if (zfac != 1.0f) {
    const float zfac_min = dist_range[0] / vod->rv3d->dist;
    const float zfac_max = dist_range[1] / vod->rv3d->dist;
    CLAMP(zfac, zfac_min, zfac_max);

    view_zoom_to_window_xy_3d(vod->ar, zfac, zoom_to_pos ? vod->prev.event_xy : NULL);
  }

  /* these limits were in old code too */
  CLAMP(vod->rv3d->dist, dist_range[0], dist_range[1]);

  if (vod->rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->sa, vod->ar);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->ar);
}

static void viewzoom_apply(ViewOpsData *vod,
                           const int xy[2],
                           const short viewzoom,
                           const bool zoom_invert,
                           const bool zoom_to_pos)
{
  if ((vod->rv3d->persp == RV3D_CAMOB) &&
      (vod->rv3d->is_persp && ED_view3d_camera_lock_check(vod->v3d, vod->rv3d)) == 0) {
    viewzoom_apply_camera(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
  else {
    viewzoom_apply_3d(vod, xy, viewzoom, zoom_invert, zoom_to_pos);
  }
}

static int viewzoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == TIMER && event->customdata == vod->timer) {
    /* continuous zoom */
    event_code = VIEW_APPLY;
  }
  else if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");
    viewzoom_apply(vod,
                   &event->x,
                   U.viewzoom,
                   (U.uiflag & USER_ZOOM_INVERT) != 0,
                   (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    ED_view3d_depth_tag_update(vod->rv3d);
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op);
  }

  return ret;
}

static int viewzoom_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d;
  RegionView3D *rv3d;
  ScrArea *sa;
  ARegion *ar;
  bool use_cam_zoom;
  float dist_range[2];

  const int delta = RNA_int_get(op->ptr, "delta");
  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  if (op->customdata) {
    ViewOpsData *vod = op->customdata;

    sa = vod->sa;
    ar = vod->ar;
  }
  else {
    sa = CTX_wm_area(C);
    ar = CTX_wm_region(C);
  }

  v3d = sa->spacedata.first;
  rv3d = ar->regiondata;

  use_cam_zoom = (rv3d->persp == RV3D_CAMOB) &&
                 !(rv3d->is_persp && ED_view3d_camera_lock_check(v3d, rv3d));

  int zoom_xy_buf[2];
  const int *zoom_xy = NULL;
  if (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) {
    zoom_xy_buf[0] = RNA_struct_property_is_set(op->ptr, "mx") ? RNA_int_get(op->ptr, "mx") :
                                                                 ar->winx / 2;
    zoom_xy_buf[1] = RNA_struct_property_is_set(op->ptr, "my") ? RNA_int_get(op->ptr, "my") :
                                                                 ar->winy / 2;
    zoom_xy = zoom_xy_buf;
  }

  ED_view3d_dist_range_get(v3d, dist_range);

  if (delta < 0) {
    const float step = 1.2f;
    /* this min and max is also in viewmove() */
    if (use_cam_zoom) {
      view_zoom_to_window_xy_camera(scene, depsgraph, v3d, ar, step, zoom_xy);
    }
    else {
      if (rv3d->dist < dist_range[1]) {
        view_zoom_to_window_xy_3d(ar, step, zoom_xy);
      }
    }
  }
  else {
    const float step = 1.0f / 1.2f;
    if (use_cam_zoom) {
      view_zoom_to_window_xy_camera(scene, depsgraph, v3d, ar, step, zoom_xy);
    }
    else {
      if (rv3d->dist > dist_range[0]) {
        view_zoom_to_window_xy_3d(ar, step, zoom_xy);
      }
    }
  }

  if (rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(sa, ar);
  }

  ED_view3d_depth_tag_update(rv3d);

  ED_view3d_camera_lock_sync(depsgraph, v3d, rv3d);
  ED_view3d_camera_lock_autokey(v3d, rv3d, C, false, true);

  ED_region_tag_redraw(ar);

  viewops_data_free(C, op);

  return OPERATOR_FINISHED;
}

/* viewdolly_invoke() copied this function, changes here may apply there */
static int viewzoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* makes op->customdata */
  viewops_data_alloc(C, op);
  viewops_data_create(C,
                      op,
                      event,
                      (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT) |
                          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));
  vod = op->customdata;

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  /* if one or the other zoom position aren't set, set from event */
  if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
    RNA_int_set(op->ptr, "mx", event->x);
    RNA_int_set(op->ptr, "my", event->y);
  }

  if (RNA_struct_property_is_set(op->ptr, "delta")) {
    viewzoom_exec(C, op);
  }
  else {
    if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {

      if (U.uiflag & USER_ZOOM_HORIZ) {
        vod->init.event_xy[0] = vod->prev.event_xy[0] = event->x;
      }
      else {
        /* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
        vod->init.event_xy[1] = vod->prev.event_xy[1] = vod->init.event_xy[1] + event->x -
                                                        event->prevx;
      }
      viewzoom_apply(vod,
                     &event->prevx,
                     USER_ZOOM_DOLLY,
                     (U.uiflag & USER_ZOOM_INVERT) != 0,
                     (use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)));
      ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);

      ED_view3d_depth_tag_update(vod->rv3d);

      viewops_data_free(C, op);
      return OPERATOR_FINISHED;
    }
    else {
      if (U.viewzoom == USER_ZOOM_CONT) {
        /* needs a timer to continue redrawing */
        vod->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
        vod->prev.time = PIL_check_seconds_timer();
      }

      /* add temp handler */
      WM_event_add_modal_handler(C, op);

      return OPERATOR_RUNNING_MODAL;
    }
  }
  return OPERATOR_FINISHED;
}

static void viewzoom_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op);
}

void VIEW3D_OT_zoom(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom View";
  ot->description = "Zoom in/out in the view";
  ot->idname = "VIEW3D_OT_zoom";

  /* api callbacks */
  ot->invoke = viewzoom_invoke;
  ot->exec = viewzoom_exec;
  ot->modal = viewzoom_modal;
  ot->poll = ED_operator_region_view3d_active;
  ot->cancel = viewzoom_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(
      ot, V3D_OP_PROP_DELTA | V3D_OP_PROP_MOUSE_CO | V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Dolly Operator
 *
 * Like zoom but translates the view offset along the view direction
 * which avoids #RegionView3D.dist approaching zero.
 * \{ */

/* this is an exact copy of viewzoom_modal_keymap */
/* called in transform_ops.c, on each regeneration of keymaps  */
void viewdolly_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {VIEW_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},

      {VIEWROT_MODAL_SWITCH_ROTATE, "SWITCH_TO_ROTATE", 0, "Switch to Rotate"},
      {VIEWROT_MODAL_SWITCH_MOVE, "SWITCH_TO_MOVE", 0, "Switch to Move"},

      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "View3D Dolly Modal");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_add(keyconf, "View3D Dolly Modal", modal_items);

  /* disabled mode switching for now, can re-implement better, later on */
#if 0
  WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
  WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, VIEWROT_MODAL_SWITCH_ROTATE);
  WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, VIEWROT_MODAL_SWITCH_MOVE);
#endif

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_dolly");
}

static bool viewdolly_offset_lock_check(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (ED_view3d_offset_lock_check(v3d, rv3d)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot dolly when the view offset is locked");
    return true;
  }
  else {
    return false;
  }
}

static void view_dolly_to_vector_3d(ARegion *ar, float orig_ofs[3], float dvec[3], float dfac)
{
  RegionView3D *rv3d = ar->regiondata;
  madd_v3_v3v3fl(rv3d->ofs, orig_ofs, dvec, -(1.0f - dfac));
}

static void viewdolly_apply(ViewOpsData *vod, const int xy[2], const short zoom_invert)
{
  float zfac = 1.0;

  {
    float len1, len2;

    if (U.uiflag & USER_ZOOM_HORIZ) {
      len1 = (vod->ar->winrct.xmax - xy[0]) + 5;
      len2 = (vod->ar->winrct.xmax - vod->init.event_xy[0]) + 5;
    }
    else {
      len1 = (vod->ar->winrct.ymax - xy[1]) + 5;
      len2 = (vod->ar->winrct.ymax - vod->init.event_xy[1]) + 5;
    }
    if (zoom_invert) {
      SWAP(float, len1, len2);
    }

    zfac = 1.0f + ((len1 - len2) * 0.01f * vod->rv3d->dist);
  }

  if (zfac != 1.0f) {
    view_dolly_to_vector_3d(vod->ar, vod->init.ofs, vod->init.mousevec, zfac);
  }

  if (vod->rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->sa, vod->ar);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->ar);
}

static int viewdolly_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewdolly_apply(vod, &event->x, (U.uiflag & USER_ZOOM_INVERT) != 0);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    ED_view3d_depth_tag_update(vod->rv3d);
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, false, true);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op);
  }

  return ret;
}

static int viewdolly_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  RegionView3D *rv3d;
  ScrArea *sa;
  ARegion *ar;
  float mousevec[3];

  const int delta = RNA_int_get(op->ptr, "delta");

  if (op->customdata) {
    ViewOpsData *vod = op->customdata;

    sa = vod->sa;
    ar = vod->ar;
    copy_v3_v3(mousevec, vod->init.mousevec);
  }
  else {
    sa = CTX_wm_area(C);
    ar = CTX_wm_region(C);
    negate_v3_v3(mousevec, ((RegionView3D *)ar->regiondata)->viewinv[2]);
    normalize_v3(mousevec);
  }

  v3d = sa->spacedata.first;
  rv3d = ar->regiondata;

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  /* overwrite the mouse vector with the view direction (zoom into the center) */
  if ((use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) == 0) {
    normalize_v3_v3(mousevec, rv3d->viewinv[2]);
  }

  view_dolly_to_vector_3d(ar, rv3d->ofs, mousevec, delta < 0 ? 0.2f : 1.8f);

  if (rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(sa, ar);
  }

  ED_view3d_depth_tag_update(rv3d);

  ED_view3d_camera_lock_sync(CTX_data_depsgraph(C), v3d, rv3d);

  ED_region_tag_redraw(ar);

  viewops_data_free(C, op);

  return OPERATOR_FINISHED;
}

/* copied from viewzoom_invoke(), changes here may apply there */
static int viewdolly_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  if (viewdolly_offset_lock_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  /* makes op->customdata */
  viewops_data_alloc(C, op);
  vod = op->customdata;

  /* poll should check but in some cases fails, see poll func for details */
  if (vod->rv3d->viewlock & RV3D_LOCKED) {
    viewops_data_free(C, op);
    return OPERATOR_PASS_THROUGH;
  }

  ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

  /* needs to run before 'viewops_data_create' so the backup 'rv3d->ofs' is correct */
  /* switch from camera view when: */
  if (vod->rv3d->persp != RV3D_PERSP) {
    if (vod->rv3d->persp == RV3D_CAMOB) {
      /* ignore rv3d->lpersp because dolly only makes sense in perspective mode */
      const Depsgraph *depsgraph = CTX_data_depsgraph(C);
      ED_view3d_persp_switch_from_camera(depsgraph, vod->v3d, vod->rv3d, RV3D_PERSP);
    }
    else {
      vod->rv3d->persp = RV3D_PERSP;
    }
    ED_region_tag_redraw(vod->ar);
  }

  const bool use_cursor_init = RNA_boolean_get(op->ptr, "use_cursor_init");

  viewops_data_create(C,
                      op,
                      event,
                      (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT) |
                          (use_cursor_init ? VIEWOPS_FLAG_USE_MOUSE_INIT : 0));

  /* if one or the other zoom position aren't set, set from event */
  if (!RNA_struct_property_is_set(op->ptr, "mx") || !RNA_struct_property_is_set(op->ptr, "my")) {
    RNA_int_set(op->ptr, "mx", event->x);
    RNA_int_set(op->ptr, "my", event->y);
  }

  if (RNA_struct_property_is_set(op->ptr, "delta")) {
    viewdolly_exec(C, op);
  }
  else {
    /* overwrite the mouse vector with the view direction (zoom into the center) */
    if ((use_cursor_init && (U.uiflag & USER_ZOOM_TO_MOUSEPOS)) == 0) {
      negate_v3_v3(vod->init.mousevec, vod->rv3d->viewinv[2]);
      normalize_v3(vod->init.mousevec);
    }

    if (event->type == MOUSEZOOM) {
      /* Bypass Zoom invert flag for track pads (pass false always) */

      if (U.uiflag & USER_ZOOM_HORIZ) {
        vod->init.event_xy[0] = vod->prev.event_xy[0] = event->x;
      }
      else {
        /* Set y move = x move as MOUSEZOOM uses only x axis to pass magnification value */
        vod->init.event_xy[1] = vod->prev.event_xy[1] = vod->init.event_xy[1] + event->x -
                                                        event->prevx;
      }
      viewdolly_apply(vod, &event->prevx, (U.uiflag & USER_ZOOM_INVERT) == 0);
      ED_view3d_depth_tag_update(vod->rv3d);

      viewops_data_free(C, op);
      return OPERATOR_FINISHED;
    }
    else {
      /* add temp handler */
      WM_event_add_modal_handler(C, op);

      return OPERATOR_RUNNING_MODAL;
    }
  }
  return OPERATOR_FINISHED;
}

static void viewdolly_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op);
}

void VIEW3D_OT_dolly(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dolly View";
  ot->description = "Dolly in/out in the view";
  ot->idname = "VIEW3D_OT_dolly";

  /* api callbacks */
  ot->invoke = viewdolly_invoke;
  ot->exec = viewdolly_exec;
  ot->modal = viewdolly_modal;
  ot->poll = ED_operator_region_view3d_active;
  ot->cancel = viewdolly_cancel;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_XY;

  /* properties */
  view3d_operator_properties_common(
      ot, V3D_OP_PROP_DELTA | V3D_OP_PROP_MOUSE_CO | V3D_OP_PROP_USE_MOUSE_INIT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View All Operator
 *
 * Move & Zoom the view to fit all of it's contents.
 * \{ */

static bool view3d_object_skip_minmax(const View3D *v3d,
                                      const RegionView3D *rv3d,
                                      const Object *ob,
                                      const bool skip_camera,
                                      bool *r_only_center)
{
  BLI_assert(ob->id.orig_id == NULL);
  *r_only_center = false;

  if (skip_camera && (ob == v3d->camera)) {
    return true;
  }

  if ((ob->type == OB_EMPTY) && (ob->empty_drawtype == OB_EMPTY_IMAGE) &&
      !BKE_object_empty_image_frame_is_visible_in_view3d(ob, rv3d)) {
    *r_only_center = true;
    return false;
  }

  return false;
}

static void view3d_from_minmax(bContext *C,
                               View3D *v3d,
                               ARegion *ar,
                               const float min[3],
                               const float max[3],
                               bool ok_dist,
                               const int smooth_viewtx)
{
  RegionView3D *rv3d = ar->regiondata;
  float afm[3];
  float size;

  ED_view3d_smooth_view_force_finish(C, v3d, ar);

  /* SMOOTHVIEW */
  float new_ofs[3];
  float new_dist;

  sub_v3_v3v3(afm, max, min);
  size = max_fff(afm[0], afm[1], afm[2]);

  if (ok_dist) {
    char persp;

    if (rv3d->is_persp) {
      if (rv3d->persp == RV3D_CAMOB && ED_view3d_camera_lock_check(v3d, rv3d)) {
        persp = RV3D_CAMOB;
      }
      else {
        persp = RV3D_PERSP;
      }
    }
    else { /* ortho */
      if (size < 0.0001f) {
        /* bounding box was a single point so do not zoom */
        ok_dist = false;
      }
      else {
        /* adjust zoom so it looks nicer */
        persp = RV3D_ORTHO;
      }
    }

    if (ok_dist) {
      new_dist = ED_view3d_radius_to_dist(
          v3d, ar, CTX_data_depsgraph(C), persp, true, (size / 2) * VIEW3D_MARGIN);
      if (rv3d->is_persp) {
        /* don't zoom closer than the near clipping plane */
        new_dist = max_ff(new_dist, v3d->clip_start * 1.5f);
      }
    }
  }

  mid_v3_v3v3(new_ofs, min, max);
  negate_v3(new_ofs);

  if (rv3d->persp == RV3D_CAMOB && !ED_view3d_camera_lock_check(v3d, rv3d)) {
    rv3d->persp = RV3D_PERSP;
    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .camera_old = v3d->camera,
                              .ofs = new_ofs,
                              .dist = ok_dist ? &new_dist : NULL,
                          });
  }
  else {
    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .ofs = new_ofs,
                              .dist = ok_dist ? &new_dist : NULL,
                          });
  }

  /* smooth view does viewlock RV3D_BOXVIEW copy */
}

/**
 * Same as #view3d_from_minmax but for all regions (except cameras).
 */
static void view3d_from_minmax_multi(bContext *C,
                                     View3D *v3d,
                                     const float min[3],
                                     const float max[3],
                                     const bool ok_dist,
                                     const int smooth_viewtx)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *ar;
  for (ar = sa->regionbase.first; ar; ar = ar->next) {
    if (ar->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = ar->regiondata;
      /* when using all regions, don't jump out of camera view,
       * but _do_ allow locked cameras to be moved */
      if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
        view3d_from_minmax(C, v3d, ar, min, max, ok_dist, smooth_viewtx);
      }
    }
  }
}

static int view3d_all_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);
  const Depsgraph *depsgraph = CTX_data_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Base *base_eval;
  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, ar->regiondata) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
  const bool center = RNA_boolean_get(op->ptr, "center");
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  float min[3], max[3];
  bool changed = false;

  if (center) {
    /* in 2.4x this also move the cursor to (0, 0, 0) (with shift+c). */
    View3DCursor *cursor = &scene->cursor;
    zero_v3(min);
    zero_v3(max);
    zero_v3(cursor->location);
    float mat3[3][3];
    unit_m3(mat3);
    BKE_scene_cursor_mat3_to_rot(cursor, mat3, false);
  }
  else {
    INIT_MINMAX(min, max);
  }

  for (base_eval = view_layer_eval->object_bases.first; base_eval; base_eval = base_eval->next) {
    if (BASE_VISIBLE(v3d, base_eval)) {
      bool only_center = false;
      Object *ob = DEG_get_original_object(base_eval->object);
      if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
        continue;
      }

      if (only_center) {
        minmax_v3v3_v3(min, max, base_eval->object->obmat[3]);
      }
      else {
        BKE_object_minmax(base_eval->object, min, max, false);
      }
      changed = true;
    }
  }

  if (center) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }

  if (!changed) {
    ED_region_tag_redraw(ar);
    /* TODO - should this be cancel?
     * I think no, because we always move the cursor, with or without
     * object, but in this case there is no change in the scene,
     * only the cursor so I choice a ED_region_tag like
     * view3d_smooth_view do for the center_cursor.
     * See bug #22640
     */
    return OPERATOR_FINISHED;
  }

  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, true, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, ar, min, max, true, smooth_viewtx);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View All";
  ot->description = "View all objects in scene";
  ot->idname = "VIEW3D_OT_view_all";

  /* api callbacks */
  ot->exec = view3d_all_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
  RNA_def_boolean(ot->srna, "center", 0, "Center", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Selected Operator
 *
 * Move & Zoom the view to fit selected contents.
 * \{ */

/* like a localview without local!, was centerview() in 2.4x */
static int viewselected_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Object *ob_eval = OBACT(view_layer_eval);
  Object *obedit = CTX_data_edit_object(C);
  const bGPdata *gpd_eval = ob_eval && (ob_eval->type == OB_GPENCIL) ? ob_eval->data : NULL;
  const bool is_gp_edit = gpd_eval ? GPENCIL_ANY_MODE(gpd_eval) : false;
  const bool is_face_map = ((is_gp_edit == false) && ar->gizmo_map &&
                            WM_gizmomap_is_any_selected(ar->gizmo_map));
  float min[3], max[3];
  bool ok = false, ok_dist = true;
  const bool use_all_regions = RNA_boolean_get(op->ptr, "use_all_regions");
  const bool skip_camera = (ED_view3d_camera_lock_check(v3d, ar->regiondata) ||
                            /* any one of the regions may be locked */
                            (use_all_regions && v3d->flag2 & V3D_LOCK_CAMERA));
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  INIT_MINMAX(min, max);
  if (is_face_map) {
    ob_eval = NULL;
  }

  if (ob_eval && (ob_eval->mode & OB_MODE_WEIGHT_PAINT)) {
    /* hard-coded exception, we look for the one selected armature */
    /* this is weak code this way, we should make a generic
     * active/selection callback interface once... */
    Base *base_eval;
    for (base_eval = view_layer_eval->object_bases.first; base_eval; base_eval = base_eval->next) {
      if (BASE_SELECTED_EDITABLE(v3d, base_eval)) {
        if (base_eval->object->type == OB_ARMATURE) {
          if (base_eval->object->mode & OB_MODE_POSE) {
            break;
          }
        }
      }
    }
    if (base_eval) {
      ob_eval = base_eval->object;
    }
  }

  if (is_gp_edit) {
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      /* we're only interested in selected points here... */
      if ((gps->flag & GP_STROKE_SELECT) && (gps->flag & GP_STROKE_3DSPACE)) {
        ok |= BKE_gpencil_stroke_minmax(gps, true, min, max);
      }
    }
    CTX_DATA_END;

    if ((ob_eval) && (ok)) {
      mul_m4_v3(ob_eval->obmat, min);
      mul_m4_v3(ob_eval->obmat, max);
    }
  }
  else if (is_face_map) {
    ok = WM_gizmomap_minmax(ar->gizmo_map, true, true, min, max);
  }
  else if (obedit) {
    /* only selected */
    FOREACH_OBJECT_IN_MODE_BEGIN (view_layer_eval, v3d, obedit->type, obedit->mode, ob_eval_iter) {
      ok |= ED_view3d_minmax_verts(ob_eval_iter, min, max);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_POSE)) {
    FOREACH_OBJECT_IN_MODE_BEGIN (
        view_layer_eval, v3d, ob_eval->type, ob_eval->mode, ob_eval_iter) {
      ok |= BKE_pose_minmax(ob_eval_iter, min, max, true, true);
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (BKE_paint_select_face_test(ob_eval)) {
    ok = paintface_minmax(ob_eval, min, max);
  }
  else if (ob_eval && (ob_eval->mode & OB_MODE_PARTICLE_EDIT)) {
    ok = PE_minmax(scene, view_layer_eval, min, max);
  }
  else if (ob_eval && (ob_eval->mode & (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT |
                                        OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT))) {
    BKE_paint_stroke_get_average(scene, ob_eval, min);
    copy_v3_v3(max, min);
    ok = true;
    ok_dist = 0; /* don't zoom */
  }
  else {
    Base *base_eval;
    for (base_eval = FIRSTBASE(view_layer_eval); base_eval; base_eval = base_eval->next) {
      if (BASE_SELECTED(v3d, base_eval)) {
        bool only_center = false;
        Object *ob = DEG_get_original_object(base_eval->object);
        if (view3d_object_skip_minmax(v3d, rv3d, ob, skip_camera, &only_center)) {
          continue;
        }

        /* account for duplis */
        if (BKE_object_minmax_dupli(depsgraph, scene, base_eval->object, min, max, false) == 0) {
          /* use if duplis not found */
          if (only_center) {
            minmax_v3v3_v3(min, max, base_eval->object->obmat[3]);
          }
          else {
            BKE_object_minmax(base_eval->object, min, max, false);
          }
        }

        ok = 1;
      }
    }
  }

  if (ok == 0) {
    return OPERATOR_FINISHED;
  }

  if (use_all_regions) {
    view3d_from_minmax_multi(C, v3d, min, max, ok_dist, smooth_viewtx);
  }
  else {
    view3d_from_minmax(C, v3d, ar, min, max, ok_dist, smooth_viewtx);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Selected";
  ot->description = "Move the view to the selection center";
  ot->idname = "VIEW3D_OT_view_selected";

  /* api callbacks */
  ot->exec = viewselected_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  view3d_operator_properties_common(ot, V3D_OP_PROP_USE_ALL_REGIONS);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Lock Clear Operator
 * \{ */

static int view_lock_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);

  if (v3d) {
    ED_view3d_lock_clear(v3d);

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void VIEW3D_OT_view_lock_clear(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "View Lock Clear";
  ot->description = "Clear all view locking";
  ot->idname = "VIEW3D_OT_view_lock_clear";

  /* api callbacks */
  ot->exec = view_lock_clear_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Lock to Active Operator
 * \{ */

static int view_lock_to_active_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  Object *obact = CTX_data_active_object(C);

  if (v3d) {
    ED_view3d_lock_clear(v3d);

    v3d->ob_centre = obact; /* can be NULL */

    if (obact && obact->type == OB_ARMATURE) {
      if (obact->mode & OB_MODE_POSE) {
        Object *obact_eval = DEG_get_evaluated_object(CTX_data_depsgraph(C), obact);
        bPoseChannel *pcham_act = BKE_pose_channel_active(obact_eval);
        if (pcham_act) {
          BLI_strncpy(v3d->ob_centre_bone, pcham_act->name, sizeof(v3d->ob_centre_bone));
        }
      }
      else {
        EditBone *ebone_act = ((bArmature *)obact->data)->act_edbone;
        if (ebone_act) {
          BLI_strncpy(v3d->ob_centre_bone, ebone_act->name, sizeof(v3d->ob_centre_bone));
        }
      }
    }

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void VIEW3D_OT_view_lock_to_active(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "View Lock to Active";
  ot->description = "Lock the view to the active object/bone";
  ot->idname = "VIEW3D_OT_view_lock_to_active";

  /* api callbacks */
  ot->exec = view_lock_to_active_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Center Cursor Operator
 * \{ */

static int viewcenter_cursor_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Scene *scene = CTX_data_scene(C);

  if (rv3d) {
    ARegion *ar = CTX_wm_region(C);
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, ar);

    /* non camera center */
    float new_ofs[3];
    negate_v3_v3(new_ofs, scene->cursor.location);
    ED_view3d_smooth_view(C, v3d, ar, smooth_viewtx, &(const V3D_SmoothParams){.ofs = new_ofs});

    /* smooth view does viewlock RV3D_BOXVIEW copy */
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Cursor";
  ot->description = "Center the view so that the cursor is in the middle of the view";
  ot->idname = "VIEW3D_OT_view_center_cursor";

  /* api callbacks */
  ot->exec = viewcenter_cursor_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Center Pick Operator
 * \{ */

static int viewcenter_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *ar = CTX_wm_region(C);

  if (rv3d) {
    struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
    float new_ofs[3];
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    ED_view3d_smooth_view_force_finish(C, v3d, ar);

    view3d_operator_needs_opengl(C);

    if (ED_view3d_autodist(depsgraph, ar, v3d, event->mval, new_ofs, false, NULL)) {
      /* pass */
    }
    else {
      /* fallback to simple pan */
      negate_v3_v3(new_ofs, rv3d->ofs);
      ED_view3d_win_to_3d_int(v3d, ar, new_ofs, event->mval, new_ofs);
    }
    negate_v3(new_ofs);
    ED_view3d_smooth_view(C, v3d, ar, smooth_viewtx, &(const V3D_SmoothParams){.ofs = new_ofs});
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Center View to Mouse";
  ot->description = "Center the view to the Z-depth position under the mouse cursor";
  ot->idname = "VIEW3D_OT_view_center_pick";

  /* api callbacks */
  ot->invoke = viewcenter_pick_invoke;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Camera Center Operator
 * \{ */

static int view3d_center_camera_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  float xfac, yfac;
  float size[2];

  View3D *v3d;
  ARegion *ar;
  RegionView3D *rv3d;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &ar);
  rv3d = ar->regiondata;

  rv3d->camdx = rv3d->camdy = 0.0f;

  ED_view3d_calc_camera_border_size(scene, depsgraph, ar, v3d, rv3d, size);

  /* 4px is just a little room from the edge of the area */
  xfac = (float)ar->winx / (float)(size[0] + 4);
  yfac = (float)ar->winy / (float)(size[1] + 4);

  rv3d->camzoom = BKE_screen_view3d_zoom_from_fac(min_ff(xfac, yfac));
  CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Camera Center";
  ot->description = "Center the camera view";
  ot->idname = "VIEW3D_OT_view_center_camera";

  /* api callbacks */
  ot->exec = view3d_center_camera_exec;
  ot->poll = view3d_camera_user_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Lock Center Operator
 * \{ */

static int view3d_center_lock_exec(bContext *C, wmOperator *UNUSED(op))
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  zero_v2(rv3d->ofs_lock);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_lock(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Lock Center";
  ot->description = "Center the view lock offset";
  ot->idname = "VIEW3D_OT_view_center_lock";

  /* api callbacks */
  ot->exec = view3d_center_lock_exec;
  ot->poll = view3d_lock_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Render Border Operator
 * \{ */

static int render_border_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  Scene *scene = CTX_data_scene(C);

  rcti rect;
  rctf vb, border;

  /* get box select values using rna */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* calculate range */

  if (rv3d->persp == RV3D_CAMOB) {
    Depsgraph *depsgraph = CTX_data_depsgraph(C);
    ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &vb, false);
  }
  else {
    vb.xmin = 0;
    vb.ymin = 0;
    vb.xmax = ar->winx;
    vb.ymax = ar->winy;
  }

  border.xmin = ((float)rect.xmin - vb.xmin) / BLI_rctf_size_x(&vb);
  border.ymin = ((float)rect.ymin - vb.ymin) / BLI_rctf_size_y(&vb);
  border.xmax = ((float)rect.xmax - vb.xmin) / BLI_rctf_size_x(&vb);
  border.ymax = ((float)rect.ymax - vb.ymin) / BLI_rctf_size_y(&vb);

  /* actually set border */
  CLAMP(border.xmin, 0.0f, 1.0f);
  CLAMP(border.ymin, 0.0f, 1.0f);
  CLAMP(border.xmax, 0.0f, 1.0f);
  CLAMP(border.ymax, 0.0f, 1.0f);

  if (rv3d->persp == RV3D_CAMOB) {
    scene->r.border = border;

    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    v3d->render_border = border;

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }

  /* drawing a border outside the camera view switches off border rendering */
  if ((border.xmin == border.xmax || border.ymin == border.ymax)) {
    if (rv3d->persp == RV3D_CAMOB) {
      scene->r.mode &= ~R_BORDER;
    }
    else {
      v3d->flag2 &= ~V3D_RENDER_BORDER;
    }
  }
  else {
    if (rv3d->persp == RV3D_CAMOB) {
      scene->r.mode |= R_BORDER;
    }
    else {
      v3d->flag2 |= V3D_RENDER_BORDER;
    }
  }

  if (rv3d->persp == RV3D_CAMOB) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Render Region";
  ot->description = "Set the boundaries of the border render and enable border render";
  ot->idname = "VIEW3D_OT_render_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = render_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Render Border Operator
 * \{ */

static int clear_render_border_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  Scene *scene = CTX_data_scene(C);
  rctf *border = NULL;

  if (rv3d->persp == RV3D_CAMOB) {
    scene->r.mode &= ~R_BORDER;
    border = &scene->r.border;

    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    v3d->flag2 &= ~V3D_RENDER_BORDER;
    border = &v3d->render_border;

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }

  border->xmin = 0.0f;
  border->ymin = 0.0f;
  border->xmax = 1.0f;
  border->ymax = 1.0f;

  if (rv3d->persp == RV3D_CAMOB) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_clear_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Region";
  ot->description = "Clear the boundaries of the border render and disable border render";
  ot->idname = "VIEW3D_OT_clear_render_border";

  /* api callbacks */
  ot->exec = clear_render_border_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Border Zoom Operator
 * \{ */

static int view3d_zoom_border_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* Zooms in on a border drawn by the user */
  rcti rect;
  float dvec[3], vb[2], xscale, yscale;
  float dist_range[2];

  /* SMOOTHVIEW */
  float new_dist;
  float new_ofs[3];

  /* ZBuffer depth vars */
  float depth_close = FLT_MAX;
  float cent[2], p[3];

  /* note; otherwise opengl won't work */
  view3d_operator_needs_opengl(C);

  /* get box select values using rna */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* check if zooming in/out view */
  const bool zoom_in = !RNA_boolean_get(op->ptr, "zoom_out");

  ED_view3d_dist_range_get(v3d, dist_range);

  /* Get Z Depths, needed for perspective, nice for ortho */
  ED_view3d_draw_depth(CTX_data_depsgraph(C), ar, v3d, true);

  {
    /* avoid allocating the whole depth buffer */
    ViewDepths depth_temp = {0};

    /* avoid view3d_update_depths() for speed. */
    view3d_update_depths_rect(ar, &depth_temp, &rect);

    /* find the closest Z pixel */
    depth_close = view3d_depth_near(&depth_temp);

    MEM_SAFE_FREE(depth_temp.depths);
  }

  cent[0] = (((float)rect.xmin) + ((float)rect.xmax)) / 2;
  cent[1] = (((float)rect.ymin) + ((float)rect.ymax)) / 2;

  if (rv3d->is_persp) {
    float p_corner[3];

    /* no depths to use, we cant do anything! */
    if (depth_close == FLT_MAX) {
      BKE_report(op->reports, RPT_ERROR, "Depth too large");
      return OPERATOR_CANCELLED;
    }
    /* convert border to 3d coordinates */
    if ((!ED_view3d_unproject(ar, cent[0], cent[1], depth_close, p)) ||
        (!ED_view3d_unproject(ar, rect.xmin, rect.ymin, depth_close, p_corner))) {
      return OPERATOR_CANCELLED;
    }

    sub_v3_v3v3(dvec, p, p_corner);
    negate_v3_v3(new_ofs, p);

    new_dist = len_v3(dvec);

    /* ignore dist_range min */
    dist_range[0] = v3d->clip_start * 1.5f;
  }
  else { /* othographic */
    /* find the current window width and height */
    vb[0] = ar->winx;
    vb[1] = ar->winy;

    new_dist = rv3d->dist;

    /* convert the drawn rectangle into 3d space */
    if (depth_close != FLT_MAX && ED_view3d_unproject(ar, cent[0], cent[1], depth_close, p)) {
      negate_v3_v3(new_ofs, p);
    }
    else {
      float mval_f[2];
      float zfac;

      /* We cant use the depth, fallback to the old way that dosnt set the center depth */
      copy_v3_v3(new_ofs, rv3d->ofs);

      {
        float tvec[3];
        negate_v3_v3(tvec, new_ofs);
        zfac = ED_view3d_calc_zfac(rv3d, tvec, NULL);
      }

      mval_f[0] = (rect.xmin + rect.xmax - vb[0]) / 2.0f;
      mval_f[1] = (rect.ymin + rect.ymax - vb[1]) / 2.0f;
      ED_view3d_win_to_delta(ar, mval_f, dvec, zfac);
      /* center the view to the center of the rectangle */
      sub_v3_v3(new_ofs, dvec);
    }

    /* work out the ratios, so that everything selected fits when we zoom */
    xscale = (BLI_rcti_size_x(&rect) / vb[0]);
    yscale = (BLI_rcti_size_y(&rect) / vb[1]);
    new_dist *= max_ff(xscale, yscale);
  }

  if (!zoom_in) {
    sub_v3_v3v3(dvec, new_ofs, rv3d->ofs);
    new_dist = rv3d->dist * (rv3d->dist / new_dist);
    add_v3_v3v3(new_ofs, rv3d->ofs, dvec);
  }

  /* clamp after because we may have been zooming out */
  CLAMP(new_dist, dist_range[0], dist_range[1]);

  /* TODO(campbell): 'is_camera_lock' not currently working well. */
  const bool is_camera_lock = ED_view3d_camera_lock_check(v3d, rv3d);
  if ((rv3d->persp == RV3D_CAMOB) && (is_camera_lock == false)) {
    Depsgraph *depsgraph = CTX_data_depsgraph(C);
    ED_view3d_persp_switch_from_camera(depsgraph, v3d, rv3d, RV3D_PERSP);
  }

  ED_view3d_smooth_view(C,
                        v3d,
                        ar,
                        smooth_viewtx,
                        &(const V3D_SmoothParams){
                            .ofs = new_ofs,
                            .dist = &new_dist,
                        });

  if (rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(CTX_wm_area(C), ar);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_zoom_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom to Border";
  ot->description = "Zoom in the view to the nearest object contained in the border";
  ot->idname = "VIEW3D_OT_zoom_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view3d_zoom_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  WM_operator_properties_gesture_box_zoom(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Camera Zoom 1:1 Operator
 *
 * Sets the view to 1:1 camera/render-pixel.
 * \{ */

static void view3d_set_1_to_1_viewborder(Scene *scene,
                                         Depsgraph *depsgraph,
                                         ARegion *ar,
                                         View3D *v3d)
{
  RegionView3D *rv3d = ar->regiondata;
  float size[2];
  int im_width = (scene->r.size * scene->r.xsch) / 100;

  ED_view3d_calc_camera_border_size(scene, depsgraph, ar, v3d, rv3d, size);

  rv3d->camzoom = BKE_screen_view3d_zoom_from_fac((float)im_width / size[0]);
  CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
}

static int view3d_zoom_1_to_1_camera_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);

  View3D *v3d;
  ARegion *ar;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &ar);

  view3d_set_1_to_1_viewborder(scene, depsgraph, ar, v3d);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_zoom_camera_1_to_1(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom Camera 1:1";
  ot->description = "Match the camera to 1:1 to the render output";
  ot->idname = "VIEW3D_OT_zoom_camera_1_to_1";

  /* api callbacks */
  ot->exec = view3d_zoom_1_to_1_camera_exec;
  ot->poll = view3d_camera_user_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Axis Operator
 * \{ */

static const EnumPropertyItem prop_view_items[] = {
    {RV3D_VIEW_LEFT, "LEFT", ICON_TRIA_LEFT, "Left", "View From the Left"},
    {RV3D_VIEW_RIGHT, "RIGHT", ICON_TRIA_RIGHT, "Right", "View From the Right"},
    {RV3D_VIEW_BOTTOM, "BOTTOM", ICON_TRIA_DOWN, "Bottom", "View From the Bottom"},
    {RV3D_VIEW_TOP, "TOP", ICON_TRIA_UP, "Top", "View From the Top"},
    {RV3D_VIEW_FRONT, "FRONT", 0, "Front", "View From the Front"},
    {RV3D_VIEW_BACK, "BACK", 0, "Back", "View From the Back"},
    {0, NULL, 0, NULL, NULL},
};

/* would like to make this a generic function - outside of transform */

/**
 * \param align_to_quat: When not NULL, set the axis relative to this rotation.
 */
static void axis_set_view(bContext *C,
                          View3D *v3d,
                          ARegion *ar,
                          const float quat_[4],
                          short view,
                          int perspo,
                          const float *align_to_quat,
                          const int smooth_viewtx)
{
  RegionView3D *rv3d = ar->regiondata; /* no NULL check is needed, poll checks */
  float quat[4];
  const short orig_persp = rv3d->persp;

  normalize_qt_qt(quat, quat_);

  if (align_to_quat) {
    mul_qt_qtqt(quat, quat, align_to_quat);
    rv3d->view = view = RV3D_VIEW_USER;
  }

  if (align_to_quat == NULL) {
    rv3d->view = view;
  }

  if (rv3d->viewlock & RV3D_LOCKED) {
    ED_region_tag_redraw(ar);
    return;
  }

  if (U.uiflag & USER_AUTOPERSP) {
    rv3d->persp = RV3D_VIEW_IS_AXIS(view) ? RV3D_ORTHO : perspo;
  }
  else if (rv3d->persp == RV3D_CAMOB) {
    rv3d->persp = perspo;
  }

  if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
    /* to camera */
    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .camera_old = v3d->camera,
                              .ofs = rv3d->ofs,
                              .quat = quat,
                          });
  }
  else if (orig_persp == RV3D_CAMOB && v3d->camera) {
    /* from camera */
    float ofs[3], dist;

    copy_v3_v3(ofs, rv3d->ofs);
    dist = rv3d->dist;

    /* so we animate _from_ the camera location */
    Object *camera_eval = DEG_get_evaluated_object(CTX_data_depsgraph(C), v3d->camera);
    ED_view3d_from_object(camera_eval, rv3d->ofs, NULL, &rv3d->dist, NULL);

    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .ofs = ofs,
                              .quat = quat,
                              .dist = &dist,
                          });
  }
  else {
    /* rotate around selection */
    const float *dyn_ofs_pt = NULL;
    float dyn_ofs[3];

    if (U.uiflag & USER_ORBIT_SELECTION) {
      if (view3d_orbit_calc_center(C, dyn_ofs)) {
        negate_v3(dyn_ofs);
        dyn_ofs_pt = dyn_ofs;
      }
    }

    /* no camera involved */
    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .quat = quat,
                              .dyn_ofs = dyn_ofs_pt,
                          });
  }
}

static int view_axis_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *ar;
  RegionView3D *rv3d;
  static int perspo = RV3D_PERSP;
  int viewnum;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &ar);
  rv3d = ar->regiondata;

  ED_view3d_smooth_view_force_finish(C, v3d, ar);

  viewnum = RNA_enum_get(op->ptr, "type");

  float align_quat_buf[4];
  float *align_quat = NULL;

  if (RNA_boolean_get(op->ptr, "align_active")) {
    /* align to active object */
    Object *obact = CTX_data_active_object(C);
    if (obact != NULL) {
      float twmat[3][3];
      /* same as transform gizmo when normal is set */
      ED_getTransformOrientationMatrix(C, twmat, V3D_AROUND_ACTIVE);
      align_quat = align_quat_buf;
      mat3_to_quat(align_quat, twmat);
      invert_qt_normalized(align_quat);
    }
  }

  if (RNA_boolean_get(op->ptr, "relative")) {
    float z_rel[3];

    if (viewnum == RV3D_VIEW_RIGHT) {
      negate_v3_v3(z_rel, rv3d->viewinv[0]);
    }
    else if (viewnum == RV3D_VIEW_LEFT) {
      copy_v3_v3(z_rel, rv3d->viewinv[0]);
    }
    else if (viewnum == RV3D_VIEW_TOP) {
      negate_v3_v3(z_rel, rv3d->viewinv[1]);
    }
    else if (viewnum == RV3D_VIEW_BOTTOM) {
      copy_v3_v3(z_rel, rv3d->viewinv[1]);
    }
    else if (viewnum == RV3D_VIEW_FRONT) {
      negate_v3_v3(z_rel, rv3d->viewinv[2]);
    }
    else if (viewnum == RV3D_VIEW_BACK) {
      copy_v3_v3(z_rel, rv3d->viewinv[2]);
    }
    else {
      BLI_assert(0);
    }

    float angle_max = FLT_MAX;
    int view_closest = -1;
    for (int i = RV3D_VIEW_FRONT; i <= RV3D_VIEW_BOTTOM; i++) {
      float quat[4];
      float mat[3][3];
      ED_view3d_quat_from_axis_view(i, quat);
      quat[0] *= -1.0f;
      quat_to_mat3(mat, quat);
      if (align_quat) {
        mul_qt_qtqt(quat, quat, align_quat);
      }
      const float angle_test = angle_normalized_v3v3(z_rel, mat[2]);
      if (angle_max > angle_test) {
        angle_max = angle_test;
        view_closest = i;
      }
    }
    if (view_closest == -1) {
      view_closest = RV3D_VIEW_FRONT;
    }
    viewnum = view_closest;
  }

  /* Use this to test if we started out with a camera */
  const int nextperspo = (rv3d->persp == RV3D_CAMOB) ? rv3d->lpersp : perspo;
  float quat[4];
  ED_view3d_quat_from_axis_view(viewnum, quat);
  axis_set_view(C, v3d, ar, quat, viewnum, nextperspo, align_quat, smooth_viewtx);

  perspo = rv3d->persp;

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_axis(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Axis";
  ot->description = "Use a preset viewpoint";
  ot->idname = "VIEW3D_OT_view_axis";

  /* api callbacks */
  ot->exec = view_axis_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_view_items, 0, "View", "Preset viewpoint to use");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "align_active", 0, "Align Active", "Align to the active object's axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "relative", 0, "Relative", "Rotate relative to the current orientation");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Camera Operator
 * \{ */

static int view_camera_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *ar;
  RegionView3D *rv3d;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &ar);
  rv3d = ar->regiondata;

  ED_view3d_smooth_view_force_finish(C, v3d, ar);

  if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
    /* lastview -  */

    ViewLayer *view_layer = CTX_data_view_layer(C);
    Scene *scene = CTX_data_scene(C);

    if (rv3d->persp != RV3D_CAMOB) {
      Object *ob = OBACT(view_layer);

      if (!rv3d->smooth_timer) {
        /* store settings of current view before allowing overwriting with camera view
         * only if we're not currently in a view transition */

        ED_view3d_lastview_store(rv3d);
      }

#if 0
      if (G.qual == LR_ALTKEY) {
        if (oldcamera && is_an_active_object(oldcamera)) {
          v3d->camera = oldcamera;
        }
        handle_view3d_lock();
      }
#endif

      /* first get the default camera for the view lock type */
      if (v3d->scenelock) {
        /* sets the camera view if available */
        v3d->camera = scene->camera;
      }
      else {
        /* use scene camera if one is not set (even though we're unlocked) */
        if (v3d->camera == NULL) {
          v3d->camera = scene->camera;
        }
      }

      /* if the camera isn't found, check a number of options */
      if (v3d->camera == NULL && ob && ob->type == OB_CAMERA) {
        v3d->camera = ob;
      }

      if (v3d->camera == NULL) {
        v3d->camera = BKE_view_layer_camera_find(view_layer);
      }

      /* couldn't find any useful camera, bail out */
      if (v3d->camera == NULL) {
        return OPERATOR_CANCELLED;
      }

      /* important these don't get out of sync for locked scenes */
      if (v3d->scenelock && scene->camera != v3d->camera) {
        scene->camera = v3d->camera;
        DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
      }

      /* finally do snazzy view zooming */
      rv3d->persp = RV3D_CAMOB;
      ED_view3d_smooth_view(C,
                            v3d,
                            ar,
                            smooth_viewtx,
                            &(const V3D_SmoothParams){
                                .camera = v3d->camera,
                                .ofs = rv3d->ofs,
                                .quat = rv3d->viewquat,
                                .dist = &rv3d->dist,
                                .lens = &v3d->lens,
                            });
    }
    else {
      /* return to settings of last view */
      /* does view3d_smooth_view too */
      axis_set_view(C, v3d, ar, rv3d->lviewquat, rv3d->lview, rv3d->lpersp, NULL, smooth_viewtx);
    }
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Camera";
  ot->description = "Toggle the camera view";
  ot->idname = "VIEW3D_OT_view_camera";

  /* api callbacks */
  ot->exec = view_camera_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Orbit Operator
 *
 * Rotate (orbit) in incremental steps. For interactive orbit see #VIEW3D_OT_rotate.
 * \{ */

enum {
  V3D_VIEW_STEPLEFT = 1,
  V3D_VIEW_STEPRIGHT,
  V3D_VIEW_STEPDOWN,
  V3D_VIEW_STEPUP,
};

static const EnumPropertyItem prop_view_orbit_items[] = {
    {V3D_VIEW_STEPLEFT, "ORBITLEFT", 0, "Orbit Left", "Orbit the view around to the Left"},
    {V3D_VIEW_STEPRIGHT, "ORBITRIGHT", 0, "Orbit Right", "Orbit the view around to the Right"},
    {V3D_VIEW_STEPUP, "ORBITUP", 0, "Orbit Up", "Orbit the view Up"},
    {V3D_VIEW_STEPDOWN, "ORBITDOWN", 0, "Orbit Down", "Orbit the view Down"},
    {0, NULL, 0, NULL, NULL},
};

static int vieworbit_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *ar;
  RegionView3D *rv3d;
  int orbitdir;
  char view_opposite;
  PropertyRNA *prop_angle = RNA_struct_find_property(op->ptr, "angle");
  float angle = RNA_property_is_set(op->ptr, prop_angle) ?
                    RNA_property_float_get(op->ptr, prop_angle) :
                    DEG2RADF(U.pad_rot_angle);

  /* no NULL check is needed, poll checks */
  v3d = CTX_wm_view3d(C);
  ar = CTX_wm_region(C);
  rv3d = ar->regiondata;

  /* support for switching to the opposite view (even when in locked views) */
  view_opposite = (fabsf(angle) == (float)M_PI) ? ED_view3d_axis_view_opposite(rv3d->view) :
                                                  RV3D_VIEW_USER;
  orbitdir = RNA_enum_get(op->ptr, "type");

  if ((rv3d->viewlock & RV3D_LOCKED) && (view_opposite == RV3D_VIEW_USER)) {
    /* no NULL check is needed, poll checks */
    ED_view3d_context_user_region(C, &v3d, &ar);
    rv3d = ar->regiondata;
  }

  ED_view3d_smooth_view_force_finish(C, v3d, ar);

  if ((rv3d->viewlock & RV3D_LOCKED) == 0 || (view_opposite != RV3D_VIEW_USER)) {
    if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {
      int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
      float quat_mul[4];
      float quat_new[4];

      if (view_opposite == RV3D_VIEW_USER) {
        const Depsgraph *depsgraph = CTX_data_depsgraph(C);
        ED_view3d_persp_ensure(depsgraph, v3d, ar);
      }

      if (ELEM(orbitdir, V3D_VIEW_STEPLEFT, V3D_VIEW_STEPRIGHT)) {
        if (orbitdir == V3D_VIEW_STEPRIGHT) {
          angle = -angle;
        }

        /* z-axis */
        axis_angle_to_quat_single(quat_mul, 'Z', angle);
      }
      else {

        if (orbitdir == V3D_VIEW_STEPDOWN) {
          angle = -angle;
        }

        /* horizontal axis */
        axis_angle_to_quat(quat_mul, rv3d->viewinv[0], angle);
      }

      mul_qt_qtqt(quat_new, rv3d->viewquat, quat_mul);

      /* avoid precision loss over time */
      normalize_qt(quat_new);

      if (view_opposite != RV3D_VIEW_USER) {
        rv3d->view = view_opposite;
        /* avoid float in-precision, just get a new orientation */
        ED_view3d_quat_from_axis_view(view_opposite, quat_new);
      }
      else {
        rv3d->view = RV3D_VIEW_USER;
      }

      float dyn_ofs[3], *dyn_ofs_pt = NULL;

      if (U.uiflag & USER_ORBIT_SELECTION) {
        if (view3d_orbit_calc_center(C, dyn_ofs)) {
          negate_v3(dyn_ofs);
          dyn_ofs_pt = dyn_ofs;
        }
      }

      ED_view3d_smooth_view(C,
                            v3d,
                            ar,
                            smooth_viewtx,
                            &(const V3D_SmoothParams){
                                .quat = quat_new,
                                .dyn_ofs = dyn_ofs_pt,
                            });

      return OPERATOR_FINISHED;
    }
  }

  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_view_orbit(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Orbit";
  ot->description = "Orbit the view";
  ot->idname = "VIEW3D_OT_view_orbit";

  /* api callbacks */
  ot->exec = vieworbit_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  prop = RNA_def_float(ot->srna, "angle", 0, -FLT_MAX, FLT_MAX, "Roll", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_view_orbit_items, 0, "Orbit", "Direction of View Orbit");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Roll Operator
 * \{ */

static void view_roll_angle(
    ARegion *ar, float quat[4], const float orig_quat[4], const float dvec[3], float angle)
{
  RegionView3D *rv3d = ar->regiondata;
  float quat_mul[4];

  /* camera axis */
  axis_angle_normalized_to_quat(quat_mul, dvec, angle);

  mul_qt_qtqt(quat, orig_quat, quat_mul);

  /* avoid precision loss over time */
  normalize_qt(quat);

  rv3d->view = RV3D_VIEW_USER;
}

static void viewroll_apply(ViewOpsData *vod, int x, int UNUSED(y))
{
  float angle = 0.0;

  {
    float len1, len2, tot;

    tot = vod->ar->winrct.xmax - vod->ar->winrct.xmin;
    len1 = (vod->ar->winrct.xmax - x) / tot;
    len2 = (vod->ar->winrct.xmax - vod->init.event_xy[0]) / tot;
    angle = (len1 - len2) * (float)M_PI * 4.0f;
  }

  if (angle != 0.0f) {
    view_roll_angle(vod->ar, vod->rv3d->viewquat, vod->init.quat, vod->init.mousevec, angle);
  }

  if (vod->use_dyn_ofs) {
    view3d_orbit_apply_dyn_ofs(
        vod->rv3d->ofs, vod->init.ofs, vod->init.quat, vod->rv3d->viewquat, vod->dyn_ofs);
  }

  if (vod->rv3d->viewlock & RV3D_BOXVIEW) {
    view3d_boxview_sync(vod->sa, vod->ar);
  }

  ED_view3d_camera_lock_sync(vod->depsgraph, vod->v3d, vod->rv3d);

  ED_region_tag_redraw(vod->ar);
}

static int viewroll_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod = op->customdata;
  short event_code = VIEW_PASS;
  bool use_autokey = false;
  int ret = OPERATOR_RUNNING_MODAL;

  /* execute the events */
  if (event->type == MOUSEMOVE) {
    event_code = VIEW_APPLY;
  }
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case VIEW_MODAL_CONFIRM:
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_MOVE:
        WM_operator_name_call(C, "VIEW3D_OT_move", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
      case VIEWROT_MODAL_SWITCH_ROTATE:
        WM_operator_name_call(C, "VIEW3D_OT_rotate", WM_OP_INVOKE_DEFAULT, NULL);
        event_code = VIEW_CONFIRM;
        break;
    }
  }
  else if (event->type == vod->init.event_type && event->val == KM_RELEASE) {
    event_code = VIEW_CONFIRM;
  }

  if (event_code == VIEW_APPLY) {
    viewroll_apply(vod, event->x, event->y);
    if (ED_screen_animation_playing(CTX_wm_manager(C))) {
      use_autokey = true;
    }
  }
  else if (event_code == VIEW_CONFIRM) {
    ED_view3d_depth_tag_update(vod->rv3d);
    use_autokey = true;
    ret = OPERATOR_FINISHED;
  }

  if (use_autokey) {
    ED_view3d_camera_lock_autokey(vod->v3d, vod->rv3d, C, true, false);
  }

  if (ret & OPERATOR_FINISHED) {
    viewops_data_free(C, op);
  }

  return ret;
}

static const EnumPropertyItem prop_view_roll_items[] = {
    {0, "ANGLE", 0, "Roll Angle", "Roll the view using an angle value"},
    {V3D_VIEW_STEPLEFT, "LEFT", 0, "Roll Left", "Roll the view around to the Left"},
    {V3D_VIEW_STEPRIGHT, "RIGHT", 0, "Roll Right", "Roll the view around to the Right"},
    {0, NULL, 0, NULL, NULL},
};

static int viewroll_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  RegionView3D *rv3d;
  ARegion *ar;

  if (op->customdata) {
    ViewOpsData *vod = op->customdata;
    ar = vod->ar;
    v3d = vod->v3d;
  }
  else {
    ED_view3d_context_user_region(C, &v3d, &ar);
  }

  rv3d = ar->regiondata;
  if ((rv3d->persp != RV3D_CAMOB) || ED_view3d_camera_lock_check(v3d, rv3d)) {

    ED_view3d_smooth_view_force_finish(C, v3d, ar);

    int type = RNA_enum_get(op->ptr, "type");
    float angle = (type == 0) ? RNA_float_get(op->ptr, "angle") : DEG2RADF(U.pad_rot_angle);
    float mousevec[3];
    float quat_new[4];

    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    if (type == V3D_VIEW_STEPLEFT) {
      angle = -angle;
    }

    normalize_v3_v3(mousevec, rv3d->viewinv[2]);
    negate_v3(mousevec);
    view_roll_angle(ar, quat_new, rv3d->viewquat, mousevec, angle);

    const float *dyn_ofs_pt = NULL;
    float dyn_ofs[3];
    if (U.uiflag & USER_ORBIT_SELECTION) {
      if (view3d_orbit_calc_center(C, dyn_ofs)) {
        negate_v3(dyn_ofs);
        dyn_ofs_pt = dyn_ofs;
      }
    }

    ED_view3d_smooth_view(C,
                          v3d,
                          ar,
                          smooth_viewtx,
                          &(const V3D_SmoothParams){
                              .quat = quat_new,
                              .dyn_ofs = dyn_ofs_pt,
                          });

    viewops_data_free(C, op);
    return OPERATOR_FINISHED;
  }
  else {
    viewops_data_free(C, op);
    return OPERATOR_CANCELLED;
  }
}

static int viewroll_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewOpsData *vod;

  bool use_angle = RNA_enum_get(op->ptr, "type") != 0;

  if (use_angle || RNA_struct_property_is_set(op->ptr, "angle")) {
    viewroll_exec(C, op);
  }
  else {
    /* makes op->customdata */
    viewops_data_alloc(C, op);
    viewops_data_create(C, op, event, viewops_flag_from_prefs());
    vod = op->customdata;

    ED_view3d_smooth_view_force_finish(C, vod->v3d, vod->ar);

    /* overwrite the mouse vector with the view direction */
    normalize_v3_v3(vod->init.mousevec, vod->rv3d->viewinv[2]);
    negate_v3(vod->init.mousevec);

    if (event->type == MOUSEROTATE) {
      vod->init.event_xy[0] = vod->prev.event_xy[0] = event->x;
      viewroll_apply(vod, event->prevx, event->prevy);
      ED_view3d_depth_tag_update(vod->rv3d);

      viewops_data_free(C, op);
      return OPERATOR_FINISHED;
    }
    else {
      /* add temp handler */
      WM_event_add_modal_handler(C, op);

      return OPERATOR_RUNNING_MODAL;
    }
  }
  return OPERATOR_FINISHED;
}

static void viewroll_cancel(bContext *C, wmOperator *op)
{
  viewops_data_free(C, op);
}

void VIEW3D_OT_view_roll(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Roll";
  ot->description = "Roll the view";
  ot->idname = "VIEW3D_OT_view_roll";

  /* api callbacks */
  ot->invoke = viewroll_invoke;
  ot->exec = viewroll_exec;
  ot->modal = viewroll_modal;
  ot->poll = ED_operator_rv3d_user_region_poll;
  ot->cancel = viewroll_cancel;

  /* flags */
  ot->flag = 0;

  /* properties */
  ot->prop = prop = RNA_def_float(
      ot->srna, "angle", 0, -FLT_MAX, FLT_MAX, "Roll", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "type",
                      prop_view_roll_items,
                      0,
                      "Roll Angle Source",
                      "How roll angle is calculated");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

enum {
  V3D_VIEW_PANLEFT = 1,
  V3D_VIEW_PANRIGHT,
  V3D_VIEW_PANDOWN,
  V3D_VIEW_PANUP,
};

static const EnumPropertyItem prop_view_pan_items[] = {
    {V3D_VIEW_PANLEFT, "PANLEFT", 0, "Pan Left", "Pan the view to the Left"},
    {V3D_VIEW_PANRIGHT, "PANRIGHT", 0, "Pan Right", "Pan the view to the Right"},
    {V3D_VIEW_PANUP, "PANUP", 0, "Pan Up", "Pan the view Up"},
    {V3D_VIEW_PANDOWN, "PANDOWN", 0, "Pan Down", "Pan the view Down"},
    {0, NULL, 0, NULL, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 *
 * Move (pan) in incremental steps. For interactive pan see #VIEW3D_OT_move.
 * \{ */

static int viewpan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int x = 0, y = 0;
  int pandir = RNA_enum_get(op->ptr, "type");

  if (pandir == V3D_VIEW_PANRIGHT) {
    x = -32;
  }
  else if (pandir == V3D_VIEW_PANLEFT) {
    x = 32;
  }
  else if (pandir == V3D_VIEW_PANUP) {
    y = -25;
  }
  else if (pandir == V3D_VIEW_PANDOWN) {
    y = 25;
  }

  viewops_data_alloc(C, op);
  viewops_data_create(C, op, event, (viewops_flag_from_prefs() & ~VIEWOPS_FLAG_ORBIT_SELECT));
  ViewOpsData *vod = op->customdata;

  viewmove_apply(vod, vod->prev.event_xy[0] + x, vod->prev.event_xy[1] + y);

  ED_view3d_depth_tag_update(vod->rv3d);
  viewops_data_free(C, op);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View Direction";
  ot->description = "Pan the view in a given direction";
  ot->idname = "VIEW3D_OT_view_pan";

  /* api callbacks */
  ot->invoke = viewpan_invoke;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* Properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_view_pan_items, 0, "Pan", "Direction of View Pan");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Toggle Perspective/Orthographic Operator
 * \{ */

static int viewpersportho_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d_dummy;
  ARegion *ar;
  RegionView3D *rv3d;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d_dummy, &ar);
  rv3d = ar->regiondata;

  if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
    if (rv3d->persp != RV3D_ORTHO) {
      rv3d->persp = RV3D_ORTHO;
    }
    else {
      rv3d->persp = RV3D_PERSP;
    }
    ED_region_tag_redraw(ar);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_persportho(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Persp/Ortho";
  ot->description = "Switch the current view from perspective/orthographic projection";
  ot->idname = "VIEW3D_OT_view_persportho";

  /* api callbacks */
  ot->exec = viewpersportho_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Navigate Operator
 *
 * Wraps walk/fly modes.
 * \{ */

static int view3d_navigate_invoke(bContext *C,
                                  wmOperator *UNUSED(op),
                                  const wmEvent *UNUSED(event))
{
  eViewNavigation_Method mode = U.navigation_mode;

  switch (mode) {
    case VIEW_NAVIGATION_FLY:
      WM_operator_name_call(C, "VIEW3D_OT_fly", WM_OP_INVOKE_DEFAULT, NULL);
      break;
    case VIEW_NAVIGATION_WALK:
    default:
      WM_operator_name_call(C, "VIEW3D_OT_walk", WM_OP_INVOKE_DEFAULT, NULL);
      break;
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_navigate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Navigation (Walk/Fly)";
  ot->description =
      "Interactively navigate around the scene (uses the mode (walk/fly) preference)";
  ot->idname = "VIEW3D_OT_navigate";

  /* api callbacks */
  ot->invoke = view3d_navigate_invoke;
  ot->poll = ED_operator_view3d_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Background Image Add Operator
 * \{ */

static Camera *background_image_camera_from_context(bContext *C)
{
  /* Needed to support drag-and-drop & camera buttons context. */
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d != NULL) {
    if (v3d->camera && v3d->camera->data && v3d->camera->type == OB_CAMERA) {
      return v3d->camera->data;
    }
    return NULL;
  }
  else {
    return CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
  }
}

static int background_image_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Camera *cam = background_image_camera_from_context(C);
  BKE_camera_background_image_new(cam);

  return OPERATOR_FINISHED;
}

static int background_image_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Camera *cam = background_image_camera_from_context(C);
  Image *ima;
  CameraBGImage *bgpic;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  /* may be NULL, continue anyway */

  bgpic = BKE_camera_background_image_new(cam);
  bgpic->ima = ima;

  cam->flag |= CAM_SHOW_BG_IMAGE;

  WM_event_add_notifier(C, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, cam);
  DEG_id_tag_update(&cam->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

static bool background_image_add_poll(bContext *C)
{
  return background_image_camera_from_context(C) != NULL;
}

void VIEW3D_OT_background_image_add(wmOperatorType *ot)
{
  /* identifiers */
  /* note: having key shortcut here is bad practice,
   * but for now keep because this displays when dragging an image over the 3D viewport */
  ot->name = "Add Background Image";
  ot->description = "Add a new background image";
  ot->idname = "VIEW3D_OT_background_image_add";

  /* api callbacks */
  ot->invoke = background_image_add_invoke;
  ot->exec = background_image_add_exec;
  ot->poll = background_image_add_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna, "name", "Image", MAX_ID_NAME - 2, "Name", "Image name to assign");
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Background Image Remove Operator
 * \{ */

static int background_image_remove_exec(bContext *C, wmOperator *op)
{
  Camera *cam = CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
  const int index = RNA_int_get(op->ptr, "index");
  CameraBGImage *bgpic_rem = BLI_findlink(&cam->bg_images, index);

  if (bgpic_rem) {
    if (bgpic_rem->source == CAM_BGIMG_SOURCE_IMAGE) {
      id_us_min((ID *)bgpic_rem->ima);
    }
    else if (bgpic_rem->source == CAM_BGIMG_SOURCE_MOVIE) {
      id_us_min((ID *)bgpic_rem->clip);
    }

    BKE_camera_background_image_remove(cam, bgpic_rem);

    WM_event_add_notifier(C, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, cam);
    DEG_id_tag_update(&cam->id, ID_RECALC_COPY_ON_WRITE);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void VIEW3D_OT_background_image_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Background Image";
  ot->description = "Remove a background image from the 3D view";
  ot->idname = "VIEW3D_OT_background_image_remove";

  /* api callbacks */
  ot->exec = background_image_remove_exec;
  ot->poll = ED_operator_camera;

  /* flags */
  ot->flag = 0;

  /* properties */
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Background image index to remove", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Clipping Planes Operator
 *
 * Draw border or toggle off.
 * \{ */

static void calc_local_clipping(float clip_local[6][4], BoundBox *clipbb, float mat[4][4])
{
  BoundBox clipbb_local;
  float imat[4][4];
  int i;

  invert_m4_m4(imat, mat);

  for (i = 0; i < 8; i++) {
    mul_v3_m4v3(clipbb_local.vec[i], imat, clipbb->vec[i]);
  }

  ED_view3d_clipping_calc_from_boundbox(clip_local, &clipbb_local, is_negative_m4(mat));
}

void ED_view3d_clipping_local(RegionView3D *rv3d, float mat[4][4])
{
  if (rv3d->rflag & RV3D_CLIPPING) {
    calc_local_clipping(rv3d->clip_local, rv3d->clipbb, mat);
  }
}

static int view3d_clipping_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  rcti rect;

  WM_operator_properties_border_to_rcti(op, &rect);

  rv3d->rflag |= RV3D_CLIPPING;
  rv3d->clipbb = MEM_callocN(sizeof(BoundBox), "clipbb");

  /* NULL object because we don't want it in object space */
  ED_view3d_clipping_calc(rv3d->clipbb, rv3d->clip, ar, NULL, &rect);

  return OPERATOR_FINISHED;
}

static int view3d_clipping_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *ar = CTX_wm_region(C);

  if (rv3d->rflag & RV3D_CLIPPING) {
    rv3d->rflag &= ~RV3D_CLIPPING;
    ED_region_tag_redraw(ar);
    if (rv3d->clipbb) {
      MEM_freeN(rv3d->clipbb);
    }
    rv3d->clipbb = NULL;
    return OPERATOR_FINISHED;
  }
  else {
    return WM_gesture_box_invoke(C, op, event);
  }
}

void VIEW3D_OT_clip_border(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Clipping Region";
  ot->description = "Set the view clipping region";
  ot->idname = "VIEW3D_OT_clip_border";

  /* api callbacks */
  ot->invoke = view3d_clipping_invoke;
  ot->exec = view3d_clipping_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Cursor Operator
 * \{ */

/* cursor position in vec, result in vec, mval in region coords */
/* note: cannot use event->mval here (called by object_add() */
void ED_view3d_cursor3d_position(bContext *C,
                                 const int mval[2],
                                 const bool use_depth,
                                 float cursor_co[3])
{
  ARegion *ar = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = ar->regiondata;
  bool flip;
  bool depth_used = false;

  /* normally the caller should ensure this,
   * but this is called from areas that aren't already dealing with the viewport */
  if (rv3d == NULL) {
    return;
  }

  ED_view3d_calc_zfac(rv3d, cursor_co, &flip);

  /* reset the depth based on the view offset (we _know_ the offset is infront of us) */
  if (flip) {
    negate_v3_v3(cursor_co, rv3d->ofs);
    /* re initialize, no need to check flip again */
    ED_view3d_calc_zfac(rv3d, cursor_co, NULL /* &flip */);
  }

  if (use_depth) { /* maybe this should be accessed some other way */
    struct Depsgraph *depsgraph = CTX_data_depsgraph(C);

    view3d_operator_needs_opengl(C);
    if (ED_view3d_autodist(depsgraph, ar, v3d, mval, cursor_co, true, NULL)) {
      depth_used = true;
    }
  }

  if (depth_used == false) {
    float depth_pt[3];
    copy_v3_v3(depth_pt, cursor_co);
    ED_view3d_win_to_3d_int(v3d, ar, depth_pt, mval, cursor_co);
  }
}

void ED_view3d_cursor3d_position_rotation(bContext *C,
                                          const int mval[2],
                                          const bool use_depth,
                                          enum eV3DCursorOrient orientation,
                                          float cursor_co[3],
                                          float cursor_quat[4])
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ar->regiondata;

  /* XXX, caller should check. */
  if (rv3d == NULL) {
    return;
  }

  ED_view3d_cursor3d_position(C, mval, use_depth, cursor_co);

  if (orientation == V3D_CURSOR_ORIENT_NONE) {
    /* pass */
  }
  else if (orientation == V3D_CURSOR_ORIENT_VIEW) {
    copy_qt_qt(cursor_quat, rv3d->viewquat);
    cursor_quat[0] *= -1.0f;
  }
  else if (orientation == V3D_CURSOR_ORIENT_XFORM) {
    float mat[3][3];
    ED_transform_calc_orientation_from_type(C, mat);
    mat3_to_quat(cursor_quat, mat);
  }
  else if (orientation == V3D_CURSOR_ORIENT_GEOM) {
    copy_qt_qt(cursor_quat, rv3d->viewquat);
    cursor_quat[0] *= -1.0f;

    const float mval_fl[2] = {UNPACK2(mval)};
    float ray_no[3];
    float ray_co[3];

    struct SnapObjectContext *snap_context = ED_transform_snap_object_context_create_view3d(
        bmain, scene, CTX_data_depsgraph(C), 0, ar, v3d);

    float obmat[4][4];
    Object *ob_dummy = NULL;
    float dist_px = 0;
    if (ED_transform_snap_object_project_view3d_ex(snap_context,
                                                   SCE_SNAP_MODE_FACE,
                                                   &(const struct SnapObjectParams){
                                                       .snap_select = SNAP_ALL,
                                                       .use_object_edit_cage = false,
                                                   },
                                                   mval_fl,
                                                   &dist_px,
                                                   ray_co,
                                                   ray_no,
                                                   NULL,
                                                   &ob_dummy,
                                                   obmat) != 0) {
      if (use_depth) {
        copy_v3_v3(cursor_co, ray_co);
      }

      float tquat[4];

      /* Math normal (Z). */
      {
        float z_src[3] = {0, 0, 1};
        mul_qt_v3(cursor_quat, z_src);
        rotation_between_vecs_to_quat(tquat, z_src, ray_no);
        mul_qt_qtqt(cursor_quat, tquat, cursor_quat);
      }

      /* Match object matrix (X). */
      {
        const float ortho_axis_dot[3] = {
            dot_v3v3(ray_no, obmat[0]),
            dot_v3v3(ray_no, obmat[1]),
            dot_v3v3(ray_no, obmat[2]),
        };
        const int ortho_axis = axis_dominant_v3_ortho_single(ortho_axis_dot);
        float x_src[3] = {1, 0, 0};
        float x_dst[3];
        mul_qt_v3(cursor_quat, x_src);
        project_plane_v3_v3v3(x_dst, obmat[ortho_axis], ray_no);
        normalize_v3(x_dst);
        rotation_between_vecs_to_quat(tquat, x_src, x_dst);
        mul_qt_qtqt(cursor_quat, tquat, cursor_quat);
      }
    }
    ED_transform_snap_object_context_destroy(snap_context);
  }
}

void ED_view3d_cursor3d_update(bContext *C,
                               const int mval[2],
                               const bool use_depth,
                               enum eV3DCursorOrient orientation)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ar->regiondata;

  View3DCursor *cursor_curr = &scene->cursor;
  View3DCursor cursor_prev = *cursor_curr;

  {
    float quat[4], quat_prev[4];
    BKE_scene_cursor_rot_to_quat(cursor_curr, quat);
    copy_qt_qt(quat_prev, quat);
    ED_view3d_cursor3d_position_rotation(
        C, mval, use_depth, orientation, cursor_curr->location, quat);

    if (!equals_v4v4(quat_prev, quat)) {
      if ((cursor_curr->rotation_mode == ROT_MODE_AXISANGLE) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
        float tmat[3][3], cmat[3][3];
        quat_to_mat3(tmat, quat);
        negate_v3_v3(cursor_curr->rotation_axis, tmat[2]);
        axis_angle_to_mat3(cmat, cursor_curr->rotation_axis, 0.0f);
        cursor_curr->rotation_angle = angle_signed_on_axis_v3v3_v3(
            cmat[0], tmat[0], cursor_curr->rotation_axis);
      }
      else {
        BKE_scene_cursor_quat_to_rot(cursor_curr, quat, true);
      }
    }
  }

  /* offset the cursor lock to avoid jumping to new offset */
  if (v3d->ob_centre_cursor) {
    if (U.uiflag & USER_LOCK_CURSOR_ADJUST) {

      float co_2d_curr[2], co_2d_prev[2];

      if ((ED_view3d_project_float_global(
               ar, cursor_prev.location, co_2d_prev, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) &&
          (ED_view3d_project_float_global(
               ar, cursor_curr->location, co_2d_curr, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK)) {
        rv3d->ofs_lock[0] += (co_2d_curr[0] - co_2d_prev[0]) / (ar->winx * 0.5f);
        rv3d->ofs_lock[1] += (co_2d_curr[1] - co_2d_prev[1]) / (ar->winy * 0.5f);
      }
    }
    else {
      /* Cursor may be outside of the view,
       * prevent it getting 'lost', see: T40353 & T45301 */
      zero_v2(rv3d->ofs_lock);
    }
  }

  if (v3d->localvd) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
  }
  else {
    WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
  }

  {
    struct wmMsgBus *mbus = CTX_wm_message_bus(C);
    wmMsgParams_RNA msg_key_params = {{{0}}};
    RNA_pointer_create(&scene->id, &RNA_View3DCursor, &scene->cursor, &msg_key_params.ptr);
    WM_msg_publish_rna_params(mbus, &msg_key_params);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
}

static int view3d_cursor3d_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bool use_depth = (U.uiflag & USER_DEPTH_CURSOR);
  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_depth");
    if (RNA_property_is_set(op->ptr, prop)) {
      use_depth = RNA_property_boolean_get(op->ptr, prop);
    }
    else {
      RNA_property_boolean_set(op->ptr, prop, use_depth);
    }
  }
  const enum eV3DCursorOrient orientation = RNA_enum_get(op->ptr, "orientation");
  ED_view3d_cursor3d_update(C, event->mval, use_depth, orientation);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_cursor3d(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Set 3D Cursor";
  ot->description = "Set the location of the 3D cursor";
  ot->idname = "VIEW3D_OT_cursor3d";

  /* api callbacks */
  ot->invoke = view3d_cursor3d_invoke;

  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  //  ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

  PropertyRNA *prop;
  static const EnumPropertyItem orientation_items[] = {
      {V3D_CURSOR_ORIENT_NONE, "NONE", 0, "None", "Leave orientation unchanged"},
      {V3D_CURSOR_ORIENT_VIEW, "VIEW", 0, "View", "Orient to the viewport"},
      {V3D_CURSOR_ORIENT_XFORM,
       "XFORM",
       0,
       "Transform",
       "Orient to the current transform setting"},
      {V3D_CURSOR_ORIENT_GEOM, "GEOM", 0, "Geometry", "Match the surface normal"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_boolean(
      ot->srna, "use_depth", true, "Surface Project", "Project onto the surface");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "orientation",
                      orientation_items,
                      V3D_CURSOR_ORIENT_VIEW,
                      "Orientation",
                      "Preset viewpoint to use");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Shading Operator
 * \{ */

static const EnumPropertyItem prop_shading_type_items[] = {
    {OB_WIRE, "WIREFRAME", 0, "Wireframe", "Toggle wireframe shading"},
    {OB_SOLID, "SOLID", 0, "Solid", "Toggle solid shading"},
    {OB_MATERIAL, "MATERIAL", 0, "LookDev", "Toggle lookdev shading"},
    {OB_RENDER, "RENDERED", 0, "Rendered", "Toggle rendered shading"},
    {0, NULL, 0, NULL, NULL},
};

static int toggle_shading_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);
  ScrArea *sa = CTX_wm_area(C);
  int type = RNA_enum_get(op->ptr, "type");

  if (type == OB_SOLID) {
    if (v3d->shading.type != type) {
      v3d->shading.type = type;
    }
    else if (v3d->shading.type == OB_WIRE) {
      v3d->shading.type = OB_SOLID;
    }
    else {
      v3d->shading.type = OB_WIRE;
    }
  }
  else {
    char *prev_type = ((type == OB_WIRE) ? &v3d->shading.prev_type_wire : &v3d->shading.prev_type);
    if (v3d->shading.type == type) {
      if (*prev_type == type || !ELEM(*prev_type, OB_WIRE, OB_SOLID, OB_MATERIAL, OB_RENDER)) {
        *prev_type = OB_SOLID;
      }
      v3d->shading.type = *prev_type;
    }
    else {
      *prev_type = v3d->shading.type;
      v3d->shading.type = type;
    }
  }

  ED_view3d_shade_update(bmain, v3d, sa);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_shading(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Toggle Shading Type";
  ot->description = "Toggle shading type in 3D viewport";
  ot->idname = "VIEW3D_OT_toggle_shading";

  /* api callbacks */
  ot->exec = toggle_shading_exec;
  ot->poll = ED_operator_view3d_active;

  prop = RNA_def_enum(
      ot->srna, "type", prop_shading_type_items, 0, "Type", "Shading type to toggle");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle XRay
 * \{ */

static int toggle_xray_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  ScrArea *sa = CTX_wm_area(C);
  Object *obact = CTX_data_active_object(C);

  if (obact && ((obact->mode & OB_MODE_POSE) ||
                ((obact->mode & OB_MODE_WEIGHT_PAINT) && BKE_object_pose_armature_get(obact)))) {
    v3d->overlay.flag ^= V3D_OVERLAY_BONE_SELECT;
  }
  else {
    const bool xray_active = ((obact && (obact->mode & OB_MODE_EDIT)) ||
                              ELEM(v3d->shading.type, OB_WIRE, OB_SOLID));

    if (v3d->shading.type == OB_WIRE) {
      v3d->shading.flag ^= V3D_SHADING_XRAY_WIREFRAME;
    }
    else {
      v3d->shading.flag ^= V3D_SHADING_XRAY;
    }
    if (!xray_active) {
      BKE_report(op->reports, RPT_INFO, "X-Ray not available in current mode");
    }
  }

  ED_area_tag_redraw(sa);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_xray(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle X-Ray";
  ot->idname = "VIEW3D_OT_toggle_xray";

  /* api callbacks */
  ot->exec = toggle_xray_exec;
  ot->poll = ED_operator_view3d_active;
}

/** \} */
