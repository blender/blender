/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BLT_translation.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_vfont.h"

#include "DEG_depsgraph_query.h"

#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "WM_api.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_resources.h"

#include "view3d_intern.h"

#include "view3d_navigate.hh" /* own include */

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
    {V3D_VIEW_STEPLEFT, "ORBITLEFT", 0, "Orbit Left", "Orbit the view around to the left"},
    {V3D_VIEW_STEPRIGHT, "ORBITRIGHT", 0, "Orbit Right", "Orbit the view around to the right"},
    {V3D_VIEW_STEPUP, "ORBITUP", 0, "Orbit Up", "Orbit the view up"},
    {V3D_VIEW_STEPDOWN, "ORBITDOWN", 0, "Orbit Down", "Orbit the view down"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int vieworbit_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  int orbitdir;
  char view_opposite;
  PropertyRNA *prop_angle = RNA_struct_find_property(op->ptr, "angle");
  float angle = RNA_property_is_set(op->ptr, prop_angle) ?
                    RNA_property_float_get(op->ptr, prop_angle) :
                    DEG2RADF(U.pad_rot_angle);

  /* no nullptr check is needed, poll checks */
  v3d = CTX_wm_view3d(C);
  region = CTX_wm_region(C);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  /* support for switching to the opposite view (even when in locked views) */
  view_opposite = (fabsf(angle) == float(M_PI)) ? ED_view3d_axis_view_opposite(rv3d->view) :
                                                  char(RV3D_VIEW_USER);
  orbitdir = RNA_enum_get(op->ptr, "type");

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) && (view_opposite == RV3D_VIEW_USER)) {
    /* no nullptr check is needed, poll checks */
    ED_view3d_context_user_region(C, &v3d, &region);
    rv3d = static_cast<RegionView3D *>(region->regiondata);
  }

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) == 0 || (view_opposite != RV3D_VIEW_USER)) {
    const bool is_camera_lock = ED_view3d_camera_lock_check(v3d, rv3d);
    if ((rv3d->persp != RV3D_CAMOB) || is_camera_lock) {
      if (is_camera_lock) {
        const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        ED_view3d_camera_lock_init(depsgraph, v3d, rv3d);
      }
      int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
      float quat_mul[4];
      float quat_new[4];

      if (view_opposite == RV3D_VIEW_USER) {
        const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        ED_view3d_persp_ensure(depsgraph, v3d, region);
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
        ED_view3d_quat_from_axis_view(view_opposite, rv3d->view_axis_roll, quat_new);
      }
      else {
        rv3d->view = RV3D_VIEW_USER;
      }

      float dyn_ofs[3], *dyn_ofs_pt = nullptr;

      if (U.uiflag & USER_ORBIT_SELECTION) {
        if (view3d_orbit_calc_center(C, dyn_ofs)) {
          negate_v3(dyn_ofs);
          dyn_ofs_pt = dyn_ofs;
        }
      }

      V3D_SmoothParams sview = {nullptr};
      sview.quat = quat_new;
      sview.dyn_ofs = dyn_ofs_pt;
      sview.lens = &v3d->lens;
      /* Group as successive orbit may run by holding a key. */
      sview.undo_str = op->type->name;
      sview.undo_grouped = true;

      ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);

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
