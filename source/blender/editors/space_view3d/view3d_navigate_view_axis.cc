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
/** \name View Axis Operator
 * \{ */

static const EnumPropertyItem prop_view_items[] = {
    {RV3D_VIEW_LEFT, "LEFT", ICON_TRIA_LEFT, "Left", "View from the left"},
    {RV3D_VIEW_RIGHT, "RIGHT", ICON_TRIA_RIGHT, "Right", "View from the right"},
    {RV3D_VIEW_BOTTOM, "BOTTOM", ICON_TRIA_DOWN, "Bottom", "View from the bottom"},
    {RV3D_VIEW_TOP, "TOP", ICON_TRIA_UP, "Top", "View from the top"},
    {RV3D_VIEW_FRONT, "FRONT", 0, "Front", "View from the front"},
    {RV3D_VIEW_BACK, "BACK", 0, "Back", "View from the back"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int view_axis_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  static int perspo = RV3D_PERSP;
  int viewnum;
  int view_axis_roll = RV3D_VIEW_AXIS_ROLL_0;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no nullptr check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  viewnum = RNA_enum_get(op->ptr, "type");

  float align_quat_buf[4];
  float *align_quat = nullptr;

  if (RNA_boolean_get(op->ptr, "align_active")) {
    /* align to active object */
    Object *obact = CTX_data_active_object(C);
    if (obact != nullptr) {
      float twmat[3][3];
      const Scene *scene = CTX_data_scene(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);
      Object *obedit = CTX_data_edit_object(C);
      /* same as transform gizmo when normal is set */
      ED_getTransformOrientationMatrix(
          scene, view_layer, v3d, obact, obedit, V3D_AROUND_ACTIVE, twmat);
      align_quat = align_quat_buf;
      mat3_to_quat(align_quat, twmat);
      invert_qt_normalized(align_quat);
    }
  }

  if (RNA_boolean_get(op->ptr, "relative")) {
    float quat_rotate[4];
    float quat_test[4];

    if (viewnum == RV3D_VIEW_LEFT) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[1], -M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_RIGHT) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[1], M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_TOP) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], -M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_BOTTOM) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], M_PI_2);
    }
    else if (viewnum == RV3D_VIEW_FRONT) {
      unit_qt(quat_rotate);
    }
    else if (viewnum == RV3D_VIEW_BACK) {
      axis_angle_to_quat(quat_rotate, rv3d->viewinv[0], M_PI);
    }
    else {
      BLI_assert(0);
    }

    mul_qt_qtqt(quat_test, rv3d->viewquat, quat_rotate);

    float angle_best = FLT_MAX;
    int view_best = -1;
    int view_axis_roll_best = -1;
    for (int i = RV3D_VIEW_FRONT; i <= RV3D_VIEW_BOTTOM; i++) {
      for (int j = RV3D_VIEW_AXIS_ROLL_0; j <= RV3D_VIEW_AXIS_ROLL_270; j++) {
        float quat_axis[4];
        ED_view3d_quat_from_axis_view(i, j, quat_axis);
        if (align_quat) {
          mul_qt_qtqt(quat_axis, quat_axis, align_quat);
        }
        const float angle_test = fabsf(angle_signed_qtqt(quat_axis, quat_test));
        if (angle_best > angle_test) {
          angle_best = angle_test;
          view_best = i;
          view_axis_roll_best = j;
        }
      }
    }
    if (view_best == -1) {
      view_best = RV3D_VIEW_FRONT;
      view_axis_roll_best = RV3D_VIEW_AXIS_ROLL_0;
    }

    /* Disallow non-upright views in turn-table modes,
     * it's too difficult to navigate out of them. */
    if ((U.flag & USER_TRACKBALL) == 0) {
      if (!ELEM(view_best, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        view_axis_roll_best = RV3D_VIEW_AXIS_ROLL_0;
      }
    }

    viewnum = view_best;
    view_axis_roll = view_axis_roll_best;
  }

  /* Use this to test if we started out with a camera */
  const int nextperspo = (rv3d->persp == RV3D_CAMOB) ? rv3d->lpersp : perspo;
  float quat[4];
  ED_view3d_quat_from_axis_view(viewnum, view_axis_roll, quat);
  axis_set_view(
      C, v3d, region, quat, viewnum, view_axis_roll, nextperspo, align_quat, smooth_viewtx);

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
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);

  prop = RNA_def_boolean(
      ot->srna, "align_active", false, "Align Active", "Align to the active object's axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "relative", false, "Relative", "Rotate relative to the current orientation");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */
