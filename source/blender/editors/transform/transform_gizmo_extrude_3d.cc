/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Extrude Gizmo
 * \{ */

enum {
  EXTRUDE_AXIS_NORMAL = 0,
  EXTRUDE_AXIS_XYZ = 1,
};

static const float extrude_button_scale = 0.15f;
static const float extrude_button_offset_scale = 1.5f;
static const float extrude_outer_scale = 1.2f;
static const float extrude_arrow_scale = 0.7f;
static const float extrude_arrow_xyz_axis_scale = 0.6666f;
static const float extrude_arrow_normal_axis_scale = 0.6666f;
static const float extrude_dial_scale = 0.2;

static const uchar shape_plus[] = {
    0x73, 0x73, 0x73, 0x36, 0x8c, 0x36, 0x8c, 0x73, 0xc9, 0x73, 0xc9, 0x8c, 0x8c,
    0x8c, 0x8c, 0xc9, 0x73, 0xc9, 0x73, 0x8c, 0x36, 0x8c, 0x36, 0x73, 0x36, 0x73,
};

struct GizmoExtrudeGroup {

  /* XYZ & normal. */
  wmGizmo *invoke_xyz_no[4];
  /* Only visible when 'drag' tool option is disabled. */
  wmGizmo *invoke_view;
  /* Constrained & unconstrained (arrow & circle). */
  wmGizmo *adjust[2];
  int adjust_axis;

  /* Copied from the transform operator,
   * use to redo with the same settings. */
  struct {
    float orient_matrix[3][3];
    bool constraint_axis[3];
    float value[4];
    int orient_type;
  } redo_xform;

  /* Depends on object type. */
  int normal_axis;

  struct {
    float normal_mat3[3][3]; /* use Z axis for normal. */
    int orientation_index;
  } data;

  wmOperatorType *ot_extrude;
  PropertyRNA *gzgt_axis_type_prop;
};

static void gizmo_mesh_extrude_orientation_matrix_set(GizmoExtrudeGroup *ggd,
                                                      const float mat[3][3])
{
  for (int i = 0; i < 3; i++) {
    mul_v3_v3fl(ggd->invoke_xyz_no[i]->matrix_offset[3],
                mat[i],
                (extrude_arrow_xyz_axis_scale * extrude_button_offset_scale) /
                    extrude_button_scale);
  }
}

static void gizmo_mesh_extrude_orientation_matrix_set_for_adjust(GizmoExtrudeGroup *ggd,
                                                                 const float mat[3][3])
{
  /* Set orientation without location. */
  for (int j = 0; j < 3; j++) {
    copy_v3_v3(ggd->adjust[0]->matrix_basis[j], mat[j]);
  }
  /* NOP when (i == 2). */
  swap_v3_v3(ggd->adjust[0]->matrix_basis[ggd->adjust_axis], ggd->adjust[0]->matrix_basis[2]);
}

static void gizmo_mesh_extrude_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoExtrudeGroup *ggd = static_cast<GizmoExtrudeGroup *>(
      MEM_callocN(sizeof(GizmoExtrudeGroup), __func__));
  gzgroup->customdata = ggd;

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_move = WM_gizmotype_find("GIZMO_GT_button_2d", true);
  const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);

  ggd->adjust[0] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->adjust[1] = WM_gizmo_new_ptr(gzt_dial, gzgroup, nullptr);
  RNA_enum_set(ggd->adjust[1]->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_FILL_SELECT);

  for (int i = 0; i < 4; i++) {
    ggd->invoke_xyz_no[i] = WM_gizmo_new_ptr(gzt_move, gzgroup, nullptr);
    ggd->invoke_xyz_no[i]->flag |= WM_GIZMO_DRAW_OFFSET_SCALE;
  }

  {
    ggd->invoke_view = WM_gizmo_new_ptr(gzt_dial, gzgroup, nullptr);
    ggd->invoke_view->select_bias = -2.0f;
    RNA_enum_set(ggd->invoke_view->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_FILL_SELECT);
  }

  {
    PropertyRNA *prop = RNA_struct_find_property(ggd->invoke_xyz_no[3]->ptr, "shape");
    for (int i = 0; i < 4; i++) {
      RNA_property_string_set_bytes(
          ggd->invoke_xyz_no[i]->ptr, prop, (const char *)shape_plus, ARRAY_SIZE(shape_plus));
    }
  }

  {
    const char *op_idname = nullptr;
    /* Grease pencil does not use `obedit`. */
    /* GPXX: Remove if OB_MODE_EDIT_GPENCIL_LEGACY is merged with OB_MODE_EDIT */
    const Object *obact = CTX_data_active_object(C);
    if (obact->type == OB_GPENCIL_LEGACY) {
      op_idname = "GPENCIL_OT_extrude_move";
    }
    else if (obact->type == OB_MESH) {
      op_idname = "MESH_OT_extrude_context_move";
      ggd->normal_axis = 2;
    }
    else if (obact->type == OB_ARMATURE) {
      op_idname = "ARMATURE_OT_extrude_move";
      ggd->normal_axis = 1;
    }
    else if (obact->type == OB_CURVES_LEGACY) {
      op_idname = "CURVE_OT_extrude_move";
      ggd->normal_axis = 2;
    }
    else {
      BLI_assert(0);
    }
    ggd->ot_extrude = WM_operatortype_find(op_idname, true);
    ggd->gzgt_axis_type_prop = RNA_struct_type_find_property(gzgroup->type->srna, "axis_type");
  }

  for (int i = 0; i < 3; i++) {
    UI_GetThemeColor3fv(TH_AXIS_X + i, ggd->invoke_xyz_no[i]->color);
  }
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->invoke_xyz_no[3]->color);
  ggd->invoke_view->color[3] = 0.5f;

  for (int i = 0; i < 2; i++) {
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->adjust[i]->color);
  }

  for (int i = 0; i < 4; i++) {
    WM_gizmo_set_scale(ggd->invoke_xyz_no[i], extrude_button_scale);
  }
  WM_gizmo_set_scale(ggd->invoke_view, extrude_outer_scale);
  ggd->invoke_view->line_width = 2.0f;

  WM_gizmo_set_scale(ggd->adjust[0], extrude_arrow_scale);
  WM_gizmo_set_scale(ggd->adjust[1], extrude_dial_scale);
  ggd->adjust[1]->line_width = 2.0f;

  /* XYZ & normal axis extrude. */
  for (int i = 0; i < 4; i++) {
    PointerRNA *ptr = WM_gizmo_operator_set(ggd->invoke_xyz_no[i], 0, ggd->ot_extrude, nullptr);
    {
      bool constraint[3] = {0, 0, 0};
      constraint[(i < 3) ? i : ggd->normal_axis] = true;
      PointerRNA macroptr = RNA_pointer_get(ptr, "TRANSFORM_OT_translate");
      RNA_boolean_set(&macroptr, "release_confirm", true);
      RNA_boolean_set_array(&macroptr, "constraint_axis", constraint);
    }
  }

  {
    PointerRNA *ptr = WM_gizmo_operator_set(ggd->invoke_view, 0, ggd->ot_extrude, nullptr);
    PointerRNA macroptr = RNA_pointer_get(ptr, "TRANSFORM_OT_translate");
    RNA_boolean_set(&macroptr, "release_confirm", true);

    const bool constraint[3] = {0, 0, 0};
    RNA_boolean_set_array(&macroptr, "constraint_axis", constraint);
  }

  /* Adjust extrude. */
  for (int i = 0; i < 2; i++) {
    wmGizmo *gz = ggd->adjust[i];
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ggd->ot_extrude, nullptr);
    PointerRNA macroptr = RNA_pointer_get(ptr, "TRANSFORM_OT_translate");
    RNA_boolean_set(&macroptr, "release_confirm", true);
    wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
    gzop->is_redo = true;
  }
}

static void gizmo_mesh_extrude_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoExtrudeGroup *ggd = static_cast<GizmoExtrudeGroup *>(gzgroup->customdata);

  for (int i = 0; i < 4; i++) {
    WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, true);
  }
  WM_gizmo_set_flag(ggd->invoke_view, WM_GIZMO_HIDDEN, true);
  for (int i = 0; i < 2; i++) {
    WM_gizmo_set_flag(ggd->adjust[i], WM_GIZMO_HIDDEN, true);
  }

  if (G.moving) {
    return;
  }

  Scene *scene = CTX_data_scene(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(CTX_wm_region_data(C));

  int axis_type;
  {
    PointerRNA ptr;
    bToolRef *tref = WM_toolsystem_ref_from_context((bContext *)C);
    WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup->type, &ptr);
    axis_type = RNA_property_enum_get(&ptr, ggd->gzgt_axis_type_prop);
  }

  ggd->data.orientation_index = BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);
  const bool use_normal = ((ggd->data.orientation_index != V3D_ORIENT_NORMAL) ||
                           (axis_type == EXTRUDE_AXIS_NORMAL));
  const int axis_len_used = use_normal ? 4 : 3;

  TransformBounds tbounds;

  if (use_normal) {
    TransformBounds tbounds_normal;
    TransformCalcParams params{};
    params.orientation_index = V3D_ORIENT_NORMAL + 1;
    if (!ED_transform_calc_gizmo_stats(C, &params, &tbounds_normal, rv3d)) {
      unit_m3(tbounds_normal.axis);
    }
    copy_m3_m3(ggd->data.normal_mat3, tbounds_normal.axis);
  }

  /* TODO(@ideasman42): run second since this modifies the 3D view, it should not. */
  TransformCalcParams params{};
  params.orientation_index = ggd->data.orientation_index + 1;
  if (!ED_transform_calc_gizmo_stats(C, &params, &tbounds, rv3d)) {
    return;
  }

  /* Main axis is normal. */
  if (!use_normal) {
    copy_m3_m3(ggd->data.normal_mat3, tbounds.axis);
  }

  /* Offset the add icon. */
  mul_v3_v3fl(ggd->invoke_xyz_no[3]->matrix_offset[3],
              ggd->data.normal_mat3[ggd->normal_axis],
              (extrude_arrow_normal_axis_scale * extrude_button_offset_scale) /
                  extrude_button_scale);

  /* Adjust current operator. */
  /* Don't use 'WM_operator_last_redo' because selection actions will be ignored. */
  wmOperator *op = static_cast<wmOperator *>(CTX_wm_manager(C)->operators.last);
  bool has_redo = (op && op->type == ggd->ot_extrude);
  wmOperator *op_xform = static_cast<wmOperator *>(has_redo ? op->macro.last : nullptr);

  bool adjust_is_flip = false;
  wmGizmo *gz_adjust = nullptr;

  if (has_redo) {
    gz_adjust = ggd->adjust[1];
    /* We can't access this from 'ot->last_properties'
     * because some properties use skip-save. */
    RNA_float_get_array(op_xform->ptr, "orient_matrix", &ggd->redo_xform.orient_matrix[0][0]);
    RNA_boolean_get_array(op_xform->ptr, "constraint_axis", ggd->redo_xform.constraint_axis);
    RNA_float_get_array(op_xform->ptr, "value", ggd->redo_xform.value);
    ggd->redo_xform.orient_type = RNA_enum_get(op_xform->ptr, "orient_type");

    /* Set properties for redo. */
    for (int i = 0; i < 3; i++) {
      if (ggd->redo_xform.constraint_axis[i]) {
        adjust_is_flip = ggd->redo_xform.value[i] < 0.0f;
        ggd->adjust_axis = i;
        gz_adjust = ggd->adjust[0];
        break;
      }
    }
  }

  /* Needed for normal orientation. */
  gizmo_mesh_extrude_orientation_matrix_set(ggd, tbounds.axis);

  /* Location. */
  for (int i = 0; i < axis_len_used; i++) {
    WM_gizmo_set_matrix_location(ggd->invoke_xyz_no[i], tbounds.center);
  }
  WM_gizmo_set_matrix_location(ggd->invoke_view, tbounds.center);
  /* Un-hide. */
  for (int i = 0; i < axis_len_used; i++) {
    WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, false);
  }

  if (has_redo) {
    if (gz_adjust == ggd->adjust[0]) {
      gizmo_mesh_extrude_orientation_matrix_set_for_adjust(ggd, ggd->redo_xform.orient_matrix);
      if (adjust_is_flip) {
        negate_v3(ggd->adjust[0]->matrix_basis[2]);
      }
    }
    WM_gizmo_set_matrix_location(gz_adjust, tbounds.center);
    WM_gizmo_set_flag(gz_adjust, WM_GIZMO_HIDDEN, false);
  }

  /* Redo with current settings. */
  if (has_redo) {
    for (int i = 0; i < 4; i++) {
      RNA_enum_set(ggd->invoke_xyz_no[i]->ptr,
                   "draw_options",
                   ED_GIZMO_BUTTON_SHOW_BACKDROP |
                       (((gz_adjust == ggd->adjust[0]) &&
                         dot_v3v3(ggd->adjust[0]->matrix_basis[2],
                                  ggd->invoke_xyz_no[i]->matrix_offset[3]) > 0.98f) ?
                            0 :
                            ED_GIZMO_BUTTON_SHOW_HELPLINE));
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      RNA_enum_set(ggd->invoke_xyz_no[i]->ptr,
                   "draw_options",
                   ED_GIZMO_BUTTON_SHOW_BACKDROP | ED_GIZMO_BUTTON_SHOW_HELPLINE);
    }
  }

  /* TODO: skip calculating axis which won't be used (above). */
  switch (axis_type) {
    case EXTRUDE_AXIS_NORMAL:
      for (int i = 0; i < 3; i++) {
        WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, true);
      }
      break;
    case EXTRUDE_AXIS_XYZ:
      WM_gizmo_set_flag(ggd->invoke_xyz_no[3], WM_GIZMO_HIDDEN, true);
      break;
  }

  if (scene->toolsettings->workspace_tool_type == SCE_WORKSPACE_TOOL_FALLBACK) {
    WM_gizmo_set_flag(ggd->invoke_view, WM_GIZMO_HIDDEN, false);
  }
  else {
    WM_gizmo_set_flag(ggd->invoke_view, WM_GIZMO_HIDDEN, true);
  }
}

static void gizmo_mesh_extrude_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoExtrudeGroup *ggd = static_cast<GizmoExtrudeGroup *>(gzgroup->customdata);
  switch (ggd->data.orientation_index) {
    case V3D_ORIENT_VIEW: {
      RegionView3D *rv3d = CTX_wm_region_view3d(C);
      float mat[3][3];
      copy_m3_m4(mat, rv3d->viewinv);
      normalize_m3(mat);
      gizmo_mesh_extrude_orientation_matrix_set(ggd, mat);
      break;
    }
  }

  /* Basic ordering for drawing only. */
  {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
      gz->temp.f = dot_v3v3(rv3d->viewinv[2], gz->matrix_offset[3]);
    }
    BLI_listbase_sort(&gzgroup->gizmos, WM_gizmo_cmp_temp_fl_reverse);

    if ((ggd->adjust[1]->flag & WM_GIZMO_HIDDEN) == 0) {
      copy_v3_v3(ggd->adjust[1]->matrix_basis[0], rv3d->viewinv[0]);
      copy_v3_v3(ggd->adjust[1]->matrix_basis[1], rv3d->viewinv[1]);
      copy_v3_v3(ggd->adjust[1]->matrix_basis[2], rv3d->viewinv[2]);
    }
    if ((ggd->invoke_view->flag & WM_GIZMO_HIDDEN) == 0) {
      copy_v3_v3(ggd->invoke_view->matrix_basis[0], rv3d->viewinv[0]);
      copy_v3_v3(ggd->invoke_view->matrix_basis[1], rv3d->viewinv[1]);
      copy_v3_v3(ggd->invoke_view->matrix_basis[2], rv3d->viewinv[2]);
    }
  }
}

static void gizmo_mesh_extrude_invoke_prepare(const bContext * /*C*/,
                                              wmGizmoGroup *gzgroup,
                                              wmGizmo *gz,
                                              const wmEvent * /*event*/)
{
  GizmoExtrudeGroup *ggd = static_cast<GizmoExtrudeGroup *>(gzgroup->customdata);
  if (ELEM(gz, ggd->adjust[0], ggd->adjust[1])) {
    /* Set properties for redo. */
    wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
    PointerRNA macroptr = RNA_pointer_get(&gzop->ptr, "TRANSFORM_OT_translate");
    if (gz == ggd->adjust[0]) {
      RNA_boolean_set_array(&macroptr, "constraint_axis", ggd->redo_xform.constraint_axis);
      RNA_float_set_array(&macroptr, "orient_matrix", &ggd->redo_xform.orient_matrix[0][0]);
      RNA_enum_set(&macroptr, "orient_matrix_type", ggd->redo_xform.orient_type);
      RNA_enum_set(&macroptr, "orient_type", ggd->redo_xform.orient_type);
    }
    RNA_float_set_array(&macroptr, "value", ggd->redo_xform.value);
  }
  else if (gz == ggd->invoke_view) {
    /* pass */
  }
  else {
    /* Workaround for extrude action modifying normals. */
    const int i = BLI_array_findindex(ggd->invoke_xyz_no, ARRAY_SIZE(ggd->invoke_xyz_no), &gz);
    BLI_assert(i != -1);
    bool use_normal_matrix = false;
    if (i == 3) {
      use_normal_matrix = true;
    }
    else if (ggd->data.orientation_index == V3D_ORIENT_NORMAL) {
      use_normal_matrix = true;
    }
    if (use_normal_matrix) {
      wmGizmoOpElem *gzop = WM_gizmo_operator_get(gz, 0);
      PointerRNA macroptr = RNA_pointer_get(&gzop->ptr, "TRANSFORM_OT_translate");
      RNA_float_set_array(&macroptr, "orient_matrix", &ggd->data.normal_mat3[0][0]);
      RNA_enum_set(&macroptr, "orient_type", V3D_ORIENT_NORMAL);
    }
  }
}

static void gizmo_mesh_extrude_message_subscribe(const bContext *C,
                                                 wmGizmoGroup *gzgroup,
                                                 wmMsgBus *mbus)
{
  GizmoExtrudeGroup *ggd = static_cast<GizmoExtrudeGroup *>(gzgroup->customdata);
  ARegion *region = CTX_wm_region(C);

  /* Subscribe to view properties */
  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh{};
  msg_sub_value_gz_tag_refresh.owner = region;
  msg_sub_value_gz_tag_refresh.user_data = gzgroup->parent_gzmap;
  msg_sub_value_gz_tag_refresh.notify = WM_gizmo_do_msg_notify_tag_refresh;

  {
    WM_msg_subscribe_rna_anon_prop(
        mbus, TransformOrientationSlot, type, &msg_sub_value_gz_tag_refresh);
  }

  wmMsgParams_RNA params{};
  params.ptr.type = gzgroup->type->srna;
  params.prop = ggd->gzgt_axis_type_prop;
  WM_msg_subscribe_rna_params(mbus, &params, &msg_sub_value_gz_tag_refresh, __func__);

  {
    Scene *scene = CTX_data_scene(C);
    PointerRNA toolsettings_ptr;
    RNA_pointer_create(&scene->id, &RNA_ToolSettings, scene->toolsettings, &toolsettings_ptr);
    const PropertyRNA *props[] = {
        &rna_ToolSettings_workspace_tool_type,
    };
    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(
          mbus, &toolsettings_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }
}

void VIEW3D_GGT_xform_extrude(wmGizmoGroupType *gzgt)
{
  gzgt->name = "3D View Extrude";
  gzgt->idname = "VIEW3D_GGT_xform_extrude";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
               WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = gizmo_mesh_extrude_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = gizmo_mesh_extrude_refresh;
  gzgt->draw_prepare = gizmo_mesh_extrude_draw_prepare;
  gzgt->invoke_prepare = gizmo_mesh_extrude_invoke_prepare;
  gzgt->message_subscribe = gizmo_mesh_extrude_message_subscribe;

  static const EnumPropertyItem axis_type_items[] = {
      {EXTRUDE_AXIS_NORMAL, "NORMAL", 0, "Normal", "Only show normal axis"},
      {EXTRUDE_AXIS_XYZ, "XYZ", 0, "XYZ", "Follow scene orientation"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_enum(gzgt->srna, "axis_type", axis_type_items, 0, "Axis Type", "");
}

/** \} */
