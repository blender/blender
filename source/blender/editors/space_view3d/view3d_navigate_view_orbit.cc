/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BLT_translation.hh"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_mesh.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_resources.hh"

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
  float angle;
  {
    PropertyRNA *prop_angle = RNA_struct_find_property(op->ptr, "angle");
    angle = RNA_property_is_set(op->ptr, prop_angle) ?
                RNA_property_float_get(op->ptr, prop_angle) :
                DEG2RADF(U.pad_rot_angle);
  }

  ViewOpsData vod = {};
  vod.init_context(C);

  ED_view3d_smooth_view_force_finish(C, vod.v3d, vod.region);

  /* support for switching to the opposite view (even when in locked views) */
  char view_opposite = (fabsf(angle) == float(M_PI)) ?
                           ED_view3d_axis_view_opposite(vod.rv3d->view) :
                           char(RV3D_VIEW_USER);

  if ((RV3D_LOCK_FLAGS(vod.rv3d) & RV3D_LOCK_ROTATION) && (view_opposite == RV3D_VIEW_USER)) {
    /* no nullptr check is needed, poll checks */
    ED_view3d_context_user_region(C, &vod.v3d, &vod.region);
    vod.rv3d = static_cast<RegionView3D *>(vod.region->regiondata);
  }

  if ((RV3D_LOCK_FLAGS(vod.rv3d) & RV3D_LOCK_ROTATION) && (view_opposite == RV3D_VIEW_USER)) {
    return OPERATOR_CANCELLED;
  }

  const bool is_camera_lock = ED_view3d_camera_lock_check(vod.v3d, vod.rv3d);
  if (vod.rv3d->persp == RV3D_CAMOB && !is_camera_lock) {
    return OPERATOR_CANCELLED;
  }

  vod.init_navigation(C, nullptr, &ViewOpsType_orbit, nullptr, false);

  int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  float quat_mul[4];
  float quat_new[4];

  int orbitdir = RNA_enum_get(op->ptr, "type");
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
    axis_angle_to_quat(quat_mul, vod.rv3d->viewinv[0], angle);
  }

  mul_qt_qtqt(quat_new, vod.curr.viewquat, quat_mul);

  /* avoid precision loss over time */
  normalize_qt(quat_new);

  if (view_opposite != RV3D_VIEW_USER) {
    vod.rv3d->view = view_opposite;
    /* avoid float in-precision, just get a new orientation */
    ED_view3d_quat_from_axis_view(view_opposite, vod.rv3d->view_axis_roll, quat_new);
  }
  else {
    vod.rv3d->view = RV3D_VIEW_USER;
  }

  V3D_SmoothParams sview = {nullptr};
  sview.quat = quat_new;
  sview.lens = &vod.v3d->lens;
  /* Group as successive orbit may run by holding a key. */
  sview.undo_str = op->type->name;
  sview.undo_grouped = true;

  if (vod.use_dyn_ofs) {
    sview.dyn_ofs = vod.dyn_ofs;
  }

  ED_view3d_smooth_view(C, vod.v3d, vod.region, smooth_viewtx, &sview);

  vod.end_navigation(C);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_orbit(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "View Orbit";
  ot->description = "Orbit the view";
  ot->idname = ViewOpsType_orbit.idname;

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

const ViewOpsType ViewOpsType_orbit = {
    /*flag*/ VIEWOPS_FLAG_ORBIT_SELECT,
    /*idname*/ "VIEW3D_OT_view_orbit",
    /*poll_fn*/ nullptr,
    /*init_fn*/ nullptr,
    /*apply_fn*/ nullptr,
};
