/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"

#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "view3d_intern.h" /* own include */

static const char *handle_normal_id;
static const char *handle_free_id;

static const float handle_normal_radius_default = 100.0f;
static const float handle_free_radius_default = 36.0f;

/* -------------------------------------------------------------------- */
/** \name Generic Tool
 * \{ */

static bool WIDGETGROUP_tool_generic_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }

  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }

  /* Without this, refreshing the gizmo jitters in some cases with edit-mesh smooth. See #72948. */
  if (G.moving & G_TRANSFORM_EDIT) {
    return false;
  }

  return true;
}

static wmGizmo *tool_generic_create_gizmo(const bContext *C, wmGizmoGroup *gzgroup)
{

  wmGizmo *gz = WM_gizmo_new("GIZMO_GT_button_2d", gzgroup, NULL);
  gz->flag |= WM_GIZMO_OPERATOR_TOOL_INIT;

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  unit_m4(gz->matrix_offset);

  RNA_enum_set(gz->ptr, "icon", ICON_NONE);

  bToolRef *tref = WM_toolsystem_ref_from_context((bContext *)C);
  PointerRNA gzgt_ptr;
  const bool gzgt_ptr_is_valid = WM_toolsystem_ref_properties_get_from_gizmo_group(
      tref, gzgroup->type, &gzgt_ptr);

  if (gzgroup->type->idname == handle_normal_id) {
    const float radius = (gzgt_ptr_is_valid ? RNA_float_get(&gzgt_ptr, "radius") :
                                              handle_normal_radius_default) /
                         12.0f;

    gz->scale_basis = radius / U.gizmo_size;
    gz->matrix_offset[3][2] -= 12.0;
    RNA_enum_set(gz->ptr,
                 "draw_options",
                 (ED_GIZMO_BUTTON_SHOW_BACKDROP | ED_GIZMO_BUTTON_SHOW_HELPLINE |
                  ED_GIZMO_BUTTON_SHOW_OUTLINE));
  }
  else {
    const float radius = gzgt_ptr_is_valid ? RNA_float_get(&gzgt_ptr, "radius") :
                                             handle_free_radius_default;

    gz->scale_basis = radius / U.gizmo_size;

    RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_BACKDROP);

    /* Make the center low alpha. */
    WM_gizmo_set_line_width(gz, 2.0f);
    RNA_float_set(gz->ptr,
                  "backdrop_fill_alpha",
                  gzgt_ptr_is_valid ? RNA_float_get(&gzgt_ptr, "backdrop_fill_alpha") : 0.125f);
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  struct wmKeyConfig *kc = wm->defaultconf;

  gz->keymap = WM_keymap_ensure(kc, tref->runtime->keymap, tref->space_type, RGN_TYPE_WINDOW);
  return gz;
}

static void WIDGETGROUP_tool_generic_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);
  wwrapper->gizmo = tool_generic_create_gizmo(C, gzgroup);
  gzgroup->customdata = wwrapper;
}

static void WIDGETGROUP_tool_generic_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = gzgroup->customdata;
  wmGizmo *gz = wwrapper->gizmo;

  ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts->workspace_tool_type != SCE_WORKSPACE_TOOL_FALLBACK) {
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
    return;
  }

  /* skip, we don't draw anything anyway */
  {
    int orientation;
    if (gzgroup->type->idname == handle_normal_id) {
      orientation = V3D_ORIENT_NORMAL;
    }
    else {
      orientation = V3D_ORIENT_GLOBAL; /* dummy, use view. */
    }

    RegionView3D *rv3d = CTX_wm_region_data(C);
    struct TransformBounds tbounds;
    const bool hide = ED_transform_calc_gizmo_stats(C,
                                                    &(struct TransformCalcParams){
                                                        .use_only_center = true,
                                                        .orientation_index = orientation + 1,
                                                    },
                                                    &tbounds,
                                                    rv3d) == 0;

    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, hide);
    if (hide) {
      return;
    }
    copy_m4_m3(gz->matrix_basis, tbounds.axis);
    copy_v3_v3(gz->matrix_basis[3], tbounds.center);
    negate_v3(gz->matrix_basis[2]);
  }

  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_OFFSET_SCALE, true);
}

static void WIDGETGROUP_gizmo_message_subscribe(const bContext *C,
                                                wmGizmoGroup *gzgroup,
                                                struct wmMsgBus *mbus)
{
  ARegion *region = CTX_wm_region(C);

  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
      .owner = region,
      .user_data = gzgroup->parent_gzmap,
      .notify = WM_gizmo_do_msg_notify_tag_refresh,
  };

  {
    const PropertyRNA *props[] = {
        &rna_ToolSettings_workspace_tool_type,
    };

    Scene *scene = CTX_data_scene(C);
    PointerRNA toolsettings_ptr;
    RNA_pointer_create(&scene->id, &RNA_ToolSettings, scene->toolsettings, &toolsettings_ptr);

    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(
          mbus, &toolsettings_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }
}

static const char *handle_normal_id = "VIEW3D_GGT_tool_generic_handle_normal";
static const char *handle_free_id = "VIEW3D_GGT_tool_generic_handle_free";

void VIEW3D_GGT_tool_generic_handle_normal(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Generic Tool Widget Normal";
  gzgt->idname = handle_normal_id;

  gzgt->flag |= (WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_tool_generic_poll;
  gzgt->setup = WIDGETGROUP_tool_generic_setup;
  gzgt->refresh = WIDGETGROUP_tool_generic_refresh;
  gzgt->message_subscribe = WIDGETGROUP_gizmo_message_subscribe;

  RNA_def_float(gzgt->srna,
                "radius",
                handle_normal_radius_default,
                0.0f,
                1000.0,
                "Radius",
                "Radius in pixels",
                0.0f,
                1000.0f);
}

void VIEW3D_GGT_tool_generic_handle_free(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Generic Tool Widget Free";
  gzgt->idname = handle_free_id;

  /* Don't use 'WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK' here since this style of gizmo
   * is better suited to being activated immediately. */
  gzgt->flag |= (WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP);

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_tool_generic_poll;
  gzgt->setup = WIDGETGROUP_tool_generic_setup;
  gzgt->refresh = WIDGETGROUP_tool_generic_refresh;
  gzgt->message_subscribe = WIDGETGROUP_gizmo_message_subscribe;

  RNA_def_float(gzgt->srna,
                "radius",
                handle_free_radius_default,
                0.0f,
                1000.0,
                "Radius",
                "Radius in pixels",
                0.0f,
                1000.0f);
  RNA_def_float(
      gzgt->srna, "backdrop_fill_alpha", 0.125, 0.0f, 1.0f, "Backdrop Alpha", "", 0.0f, 1.0f);
}

/** \} */
