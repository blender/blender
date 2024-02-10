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
#include "BKE_global.hh"
#include "BKE_scene.h"

#include "ED_gizmo_library.hh"
#include "ED_gizmo_utils.hh"
#include "ED_screen.hh"
#include "WM_api.hh"

#include "RNA_access.hh"

/* local module include */
#include "transform.hh"
#include "transform_gizmo.hh"

/* -------------------------------------------------------------------- */
/** \name Scale Cage Gizmo
 * \{ */

struct XFormCageWidgetGroup {
  wmGizmo *gizmo;
  /* Only for view orientation. */
  struct {
    float viewinv_m3[3][3];
  } prev;
};

static bool WIDGETGROUP_xform_cage_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_TOOL)) {
    return false;
  }
  if (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) {
    return false;
  }
  return true;
}

static void WIDGETGROUP_xform_cage_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  XFormCageWidgetGroup *xgzgroup = static_cast<XFormCageWidgetGroup *>(
      MEM_mallocN(sizeof(XFormCageWidgetGroup), __func__));
  const wmGizmoType *gzt_cage = WM_gizmotype_find("GIZMO_GT_cage_3d", true);
  xgzgroup->gizmo = WM_gizmo_new_ptr(gzt_cage, gzgroup, nullptr);
  wmGizmo *gz = xgzgroup->gizmo;

  RNA_enum_set(
      gz->ptr, "transform", ED_GIZMO_CAGE_XFORM_FLAG_SCALE | ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE);

  gz->color[0] = 1;
  gz->color_hi[0] = 1;

  gzgroup->customdata = xgzgroup;

  {
    wmOperatorType *ot_resize = WM_operatortype_find("TRANSFORM_OT_resize", true);
    PointerRNA *ptr;

    /* assign operator */
    PropertyRNA *prop_release_confirm = nullptr;
    PropertyRNA *prop_constraint_axis = nullptr;

    int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
    for (int x = 0; x < 3; x++) {
      for (int y = 0; y < 3; y++) {
        for (int z = 0; z < 3; z++) {
          const bool constraint[3] = {x != 1, y != 1, z != 1};
          ptr = WM_gizmo_operator_set(gz, i, ot_resize, nullptr);
          if (prop_release_confirm == nullptr) {
            prop_release_confirm = RNA_struct_find_property(ptr, "release_confirm");
            prop_constraint_axis = RNA_struct_find_property(ptr, "constraint_axis");
          }
          RNA_property_boolean_set(ptr, prop_release_confirm, true);
          RNA_property_boolean_set_array(ptr, prop_constraint_axis, constraint);
          i++;
        }
      }
    }
  }
}

static void WIDGETGROUP_xform_cage_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  Scene *scene = CTX_data_scene(C);

  XFormCageWidgetGroup *xgzgroup = static_cast<XFormCageWidgetGroup *>(gzgroup->customdata);
  wmGizmo *gz = xgzgroup->gizmo;

  TransformBounds tbounds;

  const int orient_index = BKE_scene_orientation_get_index_from_flag(scene, SCE_ORIENT_SCALE);

  TransformCalcParams calc_params{};
  calc_params.use_local_axis = true;
  calc_params.orientation_index = orient_index + 1;
  if ((ED_transform_calc_gizmo_stats(C, &calc_params, &tbounds, rv3d) == 0) ||
      equals_v3v3(rv3d->tw_axis_min, rv3d->tw_axis_max))
  {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }
  else {
    if (tbounds.use_matrix_space) {
      copy_m4_m4(gz->matrix_space, tbounds.matrix_space);
    }
    else {
      unit_m4(gz->matrix_space);
    }

    gizmo_prepare_mat(C, rv3d, &tbounds);

    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
    WM_gizmo_set_flag(gz, WM_GIZMO_MOVE_CURSOR, true);

    float dims[3];
    sub_v3_v3v3(dims, rv3d->tw_axis_max, rv3d->tw_axis_min);
    RNA_float_set_array(gz->ptr, "dimensions", dims);
    mul_v3_fl(dims, 0.5f);

    copy_m4_m3(gz->matrix_offset, rv3d->tw_axis_matrix);
    mid_v3_v3v3(gz->matrix_offset[3], rv3d->tw_axis_max, rv3d->tw_axis_min);
    mul_m3_v3(rv3d->tw_axis_matrix, gz->matrix_offset[3]);

    float matrix_offset_global[4][4];
    mul_m4_m4m4(matrix_offset_global, gz->matrix_space, gz->matrix_offset);

    PropertyRNA *prop_center_override = nullptr;
    float center[3];
    float center_global[3];
    int i = ED_GIZMO_CAGE3D_PART_SCALE_MIN_X_MIN_Y_MIN_Z;
    for (int x = 0; x < 3; x++) {
      center[0] = float(1 - x) * dims[0];
      for (int y = 0; y < 3; y++) {
        center[1] = float(1 - y) * dims[1];
        for (int z = 0; z < 3; z++) {
          center[2] = float(1 - z) * dims[2];
          wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, i);
          if (prop_center_override == nullptr) {
            prop_center_override = RNA_struct_find_property(&gzop->ptr, "center_override");
          }
          mul_v3_m4v3(center_global, matrix_offset_global, center);
          RNA_property_float_set_array(&gzop->ptr, prop_center_override, center_global);
          i++;
        }
      }
    }
  }

  /* Needed to test view orientation changes. */
  copy_m3_m4(xgzgroup->prev.viewinv_m3, rv3d->viewinv);
}

static void WIDGETGROUP_xform_cage_message_subscribe(const bContext *C,
                                                     wmGizmoGroup *gzgroup,
                                                     wmMsgBus *mbus)
{
  Scene *scene = CTX_data_scene(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  gizmo_xform_message_subscribe(gzgroup, mbus, scene, screen, area, region, VIEW3D_GGT_xform_cage);
}

static void WIDGETGROUP_xform_cage_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  XFormCageWidgetGroup *xgzgroup = static_cast<XFormCageWidgetGroup *>(gzgroup->customdata);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  {
    Scene *scene = CTX_data_scene(C);
    const TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get(scene,
                                                                                 SCE_ORIENT_SCALE);
    switch (orient_slot->type) {
      case V3D_ORIENT_VIEW: {
        float viewinv_m3[3][3];
        copy_m3_m4(viewinv_m3, rv3d->viewinv);
        if (!equals_m3m3(viewinv_m3, xgzgroup->prev.viewinv_m3)) {
          /* Take care calling refresh from draw_prepare,
           * this should be OK because it's only adjusting the cage orientation. */
          WIDGETGROUP_xform_cage_refresh(C, gzgroup);
        }
        break;
      }
    }
  }
}

void VIEW3D_GGT_xform_cage(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Transform Cage";
  gzgt->idname = "VIEW3D_GGT_xform_cage";

  gzgt->flag |= WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE |
                WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP | WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_xform_cage_poll;
  gzgt->setup = WIDGETGROUP_xform_cage_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_xform_cage_refresh;
  gzgt->message_subscribe = WIDGETGROUP_xform_cage_message_subscribe;
  gzgt->draw_prepare = WIDGETGROUP_xform_cage_draw_prepare;
}

/** \} */
