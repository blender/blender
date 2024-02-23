/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 *
 * \name 3D Transform Gizmo
 *
 * Used for 3D View
 */

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_scene.hh"

#include "ED_gizmo_library.hh"
#include "ED_gizmo_utils.hh"
#include "ED_screen.hh"
#include "WM_api.hh"

#include "UI_resources.hh"

#include "RNA_access.hh"

/* local module include */
#include "transform.hh"
#include "transform_gizmo.hh"

/* -------------------------------------------------------------------- */
/** \name Transform Shear Gizmo
 * \{ */

struct XFormShearWidgetGroup {
  wmGizmo *gizmo[3][2];
  /** View aligned gizmos. */
  wmGizmo *gizmo_view[4];

  /* Only for view orientation. */
  struct {
    float viewinv_m3[3][3];
  } prev;
};

static bool WIDGETGROUP_xform_shear_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_TOOL)) {
    return false;
  }
  return true;
}

static void WIDGETGROUP_xform_shear_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  XFormShearWidgetGroup *xgzgroup = static_cast<XFormShearWidgetGroup *>(
      MEM_mallocN(sizeof(XFormShearWidgetGroup), __func__));
  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  wmOperatorType *ot_shear = WM_operatortype_find("TRANSFORM_OT_shear", true);

  float axis_color[3][3];
  for (int i = 0; i < 3; i++) {
    UI_GetThemeColor3fv(TH_AXIS_X + i, axis_color[i]);
  }

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      wmGizmo *gz = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
      RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_BOX);
      const int i_ortho_a = (i + j + 1) % 3;
      const int i_ortho_b = (i + (1 - j) + 1) % 3;
      interp_v3_v3v3(gz->color, axis_color[i_ortho_a], axis_color[i_ortho_b], 0.75f);
      gz->color[3] = 0.5f;
      PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot_shear, nullptr);
      RNA_boolean_set(ptr, "release_confirm", true);
      xgzgroup->gizmo[i][j] = gz;
    }
  }

  for (int i = 0; i < 4; i++) {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_BOX);
    RNA_enum_set(gz->ptr, "draw_options", 0); /* No stem. */
    copy_v3_fl(gz->color, 1.0f);
    gz->color[3] = 0.5f;
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_OFFSET_SCALE, true);
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ot_shear, nullptr);
    RNA_boolean_set(ptr, "release_confirm", true);
    xgzgroup->gizmo_view[i] = gz;

    /* Unlike the other gizmos, this never changes so can be set on setup. */
    wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
    RNA_enum_set(&gzop->ptr, "orient_type", V3D_ORIENT_VIEW);

    RNA_enum_set(&gzop->ptr, "orient_axis", 2);
    RNA_enum_set(&gzop->ptr, "orient_axis_ortho", ((i % 2) ? 0 : 1));
  }

  gzgroup->customdata = xgzgroup;
}

static void WIDGETGROUP_xform_shear_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  XFormShearWidgetGroup *xgzgroup = static_cast<XFormShearWidgetGroup *>(gzgroup->customdata);
  TransformBounds tbounds;

  /* Needed to test view orientation changes. */
  copy_m3_m4(xgzgroup->prev.viewinv_m3, rv3d->viewinv);

  TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get_from_flag(
      scene, SCE_ORIENT_ROTATE);
  const int orient_index = BKE_scene_orientation_slot_get_index(orient_slot);

  TransformCalcParams calc_params{};
  calc_params.use_local_axis = false;
  calc_params.orientation_index = orient_index + 1;
  if (ED_transform_calc_gizmo_stats(C, &calc_params, &tbounds, rv3d) == 0) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 2; j++) {
        wmGizmo *gz = xgzgroup->gizmo[i][j];
        WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
      }
    }

    for (int i = 0; i < 4; i++) {
      wmGizmo *gz = xgzgroup->gizmo_view[i];
      WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    }
  }
  else {
    gizmo_prepare_mat(C, rv3d, &tbounds);
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 2; j++) {
        wmGizmo *gz = xgzgroup->gizmo[i][j];
        WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
        WM_gizmo_set_flag(gz, WM_GIZMO_MOVE_CURSOR, true);

        wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
        const int i_ortho_a = (i + j + 1) % 3;
        const int i_ortho_b = (i + (1 - j) + 1) % 3;
        WM_gizmo_set_matrix_rotation_from_yz_axis(gz, rv3d->twmat[i_ortho_a], rv3d->twmat[i]);
        WM_gizmo_set_matrix_location(gz, rv3d->twmat[3]);

        RNA_float_set_array(&gzop->ptr, "orient_matrix", &tbounds.axis[0][0]);
        RNA_enum_set(&gzop->ptr, "orient_type", orient_slot->type);

        RNA_enum_set(&gzop->ptr, "orient_axis", i_ortho_b);
        RNA_enum_set(&gzop->ptr, "orient_axis_ortho", i_ortho_a);

        mul_v3_fl(gz->matrix_basis[0], 0.5f);
        mul_v3_fl(gz->matrix_basis[1], 6.0f);
      }
    }

    for (int i = 0; i < 4; i++) {
      wmGizmo *gz = xgzgroup->gizmo_view[i];
      WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
    }
  }
}

static void WIDGETGROUP_xform_shear_message_subscribe(const bContext *C,
                                                      wmGizmoGroup *gzgroup,
                                                      wmMsgBus *mbus)
{
  Scene *scene = CTX_data_scene(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo_xform_message_subscribe(
      gzgroup, mbus, scene, screen, area, region, VIEW3D_GGT_xform_shear);
}

static void WIDGETGROUP_xform_shear_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  XFormShearWidgetGroup *xgzgroup = static_cast<XFormShearWidgetGroup *>(gzgroup->customdata);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  {
    Scene *scene = CTX_data_scene(C);
    /* Shear is like rotate, use the rotate setting. */
    const TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get(
        scene, SCE_ORIENT_ROTATE);
    switch (orient_slot->type) {
      case V3D_ORIENT_VIEW: {
        float viewinv_m3[3][3];
        copy_m3_m4(viewinv_m3, rv3d->viewinv);
        if (!equals_m3m3(viewinv_m3, xgzgroup->prev.viewinv_m3)) {
          /* Take care calling refresh from draw_prepare,
           * this should be OK because it's only adjusting the cage orientation. */
          WIDGETGROUP_xform_shear_refresh(C, gzgroup);
        }
        break;
      }
    }
  }

  for (int i = 0; i < 4; i++) {
    const float outer_thin = 0.3f;
    const float outer_offset = 1.0f / 0.3f;
    wmGizmo *gz = xgzgroup->gizmo_view[i];
    WM_gizmo_set_matrix_rotation_from_yz_axis(
        gz, rv3d->viewinv[(i + 1) % 2], rv3d->viewinv[i % 2]);
    if (i >= 2) {
      negate_v3(gz->matrix_basis[1]);
      negate_v3(gz->matrix_basis[2]);
    }

    /* No need for depth with view aligned gizmos. */
    mul_v3_fl(gz->matrix_basis[0], 0.0f);
    mul_v3_fl(gz->matrix_basis[1], 20.0f + ((1.0f / outer_thin) * 1.8f));
    mul_v3_fl(gz->matrix_basis[2], outer_thin);
    WM_gizmo_set_matrix_location(gz, rv3d->twmat[3]);
    gz->matrix_offset[3][2] = outer_offset;
  }

  /* Basic ordering for drawing only. */
  {
    LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
      /* Since we have two pairs of each axis,
       * bias the values so gizmos that are orthogonal to the view get priority.
       * This means we never default to shearing along
       * the view axis in the case of an overlap. */
      float axis_order[3], axis_bias[3];
      copy_v3_v3(axis_order, gz->matrix_basis[2]);
      copy_v3_v3(axis_bias, gz->matrix_basis[1]);
      if (dot_v3v3(axis_bias, rv3d->viewinv[2]) < 0.0f) {
        negate_v3(axis_bias);
      }
      madd_v3_v3fl(axis_order, axis_bias, 0.01f);
      gz->temp.f = dot_v3v3(rv3d->viewinv[2], axis_order);
    }
    BLI_listbase_sort(&gzgroup->gizmos, WM_gizmo_cmp_temp_fl_reverse);
  }
}

void VIEW3D_GGT_xform_shear(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Transform Shear";
  gzgt->idname = "VIEW3D_GGT_xform_shear";

  gzgt->flag |= WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE |
                WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP | WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_xform_shear_poll;
  gzgt->setup = WIDGETGROUP_xform_shear_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_xform_shear_refresh;
  gzgt->message_subscribe = WIDGETGROUP_xform_shear_message_subscribe;
  gzgt->draw_prepare = WIDGETGROUP_xform_shear_draw_prepare;
}

/** \} */
