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
 */

/** \file
 * \ingroup edmesh
 */

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_scene.h"

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h" /* own include */

#include "ED_transform.h"

#include "ED_gizmo_library.h"
#include "ED_undo.h"

/**
 * Orient the handles towards the selection (can be slow with high-poly mesh!).
 */
// Disable for now, issues w/ refresh and '+' icons overlap.
// #define USE_SELECT_CENTER

#ifdef USE_SELECT_CENTER
#  include "BKE_editmesh.h"
#endif

static const float dial_angle_partial = M_PI / 2;
static const float dial_angle_partial_margin = 0.92f;

#define ORTHO_AXIS_OFFSET 2

/* -------------------------------------------------------------------- */
/** \name Spin Tool Gizmo
 * \{ */

typedef struct GizmoGroupData_SpinInit {
  struct {
    wmGizmo *xyz_view[4];
    wmGizmo *icon_button[3][2];
  } gizmos;

  /* Only for view orientation. */
  struct {
    float viewinv_m3[3][3];
  } prev;

  /* We could store more vars here! */
  struct {
    wmOperatorType *ot_spin;
    PropertyRNA *gzgt_axis_prop;
    float orient_mat[3][3];
#ifdef USE_SELECT_CENTER
    float select_center[3];
    float select_center_ortho_axis[3][3];
    bool use_select_center;
#endif
  } data;

  /* Store data for invoke. */
  struct {
    int ortho_axis_active;
  } invoke;

} GizmoGroupData_SpinInit;

/* Use dials only as a visualization when hovering over the icons. */
#define USE_DIAL_HOVER

#define INIT_SCALE_BASE 2.3f
#define INIT_SCALE_BUTTON 0.15f

static const uchar shape_plus[] = {
    0x5f, 0xfb, 0x40, 0xee, 0x25, 0xda, 0x11, 0xbf, 0x4,  0xa0, 0x0,  0x80, 0x4,  0x5f, 0x11, 0x40,
    0x25, 0x25, 0x40, 0x11, 0x5f, 0x4,  0x7f, 0x0,  0xa0, 0x4,  0xbf, 0x11, 0xda, 0x25, 0xee, 0x40,
    0xfb, 0x5f, 0xff, 0x7f, 0xfb, 0xa0, 0xee, 0xbf, 0xda, 0xda, 0xbf, 0xee, 0xa0, 0xfb, 0x80, 0xff,
    0x6e, 0xd7, 0x92, 0xd7, 0x92, 0x90, 0xd8, 0x90, 0xd8, 0x6d, 0x92, 0x6d, 0x92, 0x27, 0x6e, 0x27,
    0x6e, 0x6d, 0x28, 0x6d, 0x28, 0x90, 0x6e, 0x90, 0x6e, 0xd7, 0x80, 0xff, 0x5f, 0xfb, 0x5f, 0xfb,
};

static void gizmo_mesh_spin_init_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  /* alpha values for normal/highlighted states */
  const float alpha = 0.6f;
  const float alpha_hi = 1.0f;
  const float scale_base = INIT_SCALE_BASE;
  const float scale_button = INIT_SCALE_BUTTON;

  GizmoGroupData_SpinInit *ggd = MEM_callocN(sizeof(*ggd), __func__);
  gzgroup->customdata = ggd;
  const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);
  const wmGizmoType *gzt_button = WM_gizmotype_find("GIZMO_GT_button_2d", true);

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 2; j++) {
      wmGizmo *gz = WM_gizmo_new_ptr(gzt_button, gzgroup, NULL);
      PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "shape");
      RNA_property_string_set_bytes(
          gz->ptr, prop, (const char *)shape_plus, ARRAY_SIZE(shape_plus));

      float color[4];
      UI_GetThemeColor3fv(TH_AXIS_X + i, color);
      color[3] = alpha;
      WM_gizmo_set_color(gz, color);

      WM_gizmo_set_scale(gz, scale_button);
      gz->color[3] = 0.6f;

      gz->flag |= WM_GIZMO_DRAW_OFFSET_SCALE | WM_GIZMO_OPERATOR_TOOL_INIT;

      ggd->gizmos.icon_button[i][j] = gz;
    }
  }

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.xyz_view); i++) {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE | WM_GIZMO_HIDDEN_SELECT, true);
    ggd->gizmos.xyz_view[i] = gz;
  }

  for (int i = 0; i < 3; i++) {
    wmGizmo *gz = ggd->gizmos.xyz_view[i];
#ifndef USE_DIAL_HOVER
    RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
#endif
    WM_gizmo_set_line_width(gz, 2.0f);
    float color[4];
    UI_GetThemeColor3fv(TH_AXIS_X + i, color);
    color[3] = alpha;
    WM_gizmo_set_color(gz, color);
    color[3] = alpha_hi;
    WM_gizmo_set_color_highlight(gz, color);
    WM_gizmo_set_scale(gz, INIT_SCALE_BASE);
    RNA_float_set(gz->ptr,
                  "arc_partial_angle",
                  (M_PI * 2) - (dial_angle_partial * dial_angle_partial_margin));
  }

  {
    wmGizmo *gz = ggd->gizmos.xyz_view[3];
    WM_gizmo_set_line_width(gz, 2.0f);
    float color[4];
    copy_v3_fl(color, 1.0f);
    color[3] = alpha;
    WM_gizmo_set_color(gz, color);
    color[3] = alpha_hi;
    WM_gizmo_set_color_highlight(gz, color);
    WM_gizmo_set_scale(gz, scale_base);
  }

#ifdef USE_DIAL_HOVER
  for (int i = 0; i < 4; i++) {
    wmGizmo *gz = ggd->gizmos.xyz_view[i];
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
  }
#endif

  ggd->data.ot_spin = WM_operatortype_find("MESH_OT_spin", true);
  ggd->data.gzgt_axis_prop = RNA_struct_type_find_property(gzgroup->type->srna, "axis");
}

static void gizmo_mesh_spin_init_refresh(const bContext *C, wmGizmoGroup *gzgroup);

static void gizmo_mesh_spin_init_refresh_axis_orientation(wmGizmoGroup *gzgroup,
                                                          int axis_index,
                                                          const float axis_vec[3],
                                                          const float axis_tan[3])
{
  GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
  wmGizmo *gz = ggd->gizmos.xyz_view[axis_index];
  if (axis_tan != NULL) {
    WM_gizmo_set_matrix_rotation_from_yz_axis(gz, axis_tan, axis_vec);
  }
  else {
    WM_gizmo_set_matrix_rotation_from_z_axis(gz, axis_vec);
  }

  /* Only for display, use icons to access. */
#ifndef USE_DIAL_HOVER
  {
    PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ggd->data.ot_spin, NULL);
    RNA_float_set_array(ptr, "axis", axis_vec);
  }
#endif
  if (axis_index < 3) {
    for (int j = 0; j < 2; j++) {
      gz = ggd->gizmos.icon_button[axis_index][j];
      PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ggd->data.ot_spin, NULL);
      float axis_vec_flip[3];
      if (0 == j) {
        negate_v3_v3(axis_vec_flip, axis_vec);
      }
      else {
        copy_v3_v3(axis_vec_flip, axis_vec);
      }
      RNA_float_set_array(ptr, "axis", axis_vec_flip);
    }
  }
}

static void gizmo_mesh_spin_init_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  float viewinv_m3[3][3];
  copy_m3_m4(viewinv_m3, rv3d->viewinv);

  {
    Scene *scene = CTX_data_scene(C);
    const TransformOrientationSlot *orient_slot = BKE_scene_orientation_slot_get(
        scene, SCE_ORIENT_ROTATE);
    switch (orient_slot->type) {
      case V3D_ORIENT_VIEW: {
        if (!equals_m3m3(viewinv_m3, ggd->prev.viewinv_m3)) {
          /* Take care calling refresh from draw_prepare,
           * this should be OK because it's only adjusting the cage orientation. */
          gizmo_mesh_spin_init_refresh(C, gzgroup);
        }
        break;
      }
    }
  }

  /* Refresh handled above when using view orientation. */
  if (!equals_m3m3(viewinv_m3, ggd->prev.viewinv_m3)) {
    gizmo_mesh_spin_init_refresh_axis_orientation(gzgroup, 3, rv3d->viewinv[2], NULL);
    copy_m3_m4(ggd->prev.viewinv_m3, rv3d->viewinv);
  }

  /* Hack! highlight XYZ dials based on buttons */
#ifdef USE_DIAL_HOVER
  {
    PointerRNA ptr;
    bToolRef *tref = WM_toolsystem_ref_from_context((bContext *)C);
    WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup->type, &ptr);
    const int axis_flag = RNA_property_enum_get(&ptr, ggd->data.gzgt_axis_prop);
    for (int i = 0; i < 4; i++) {
      bool hide = (axis_flag & (1 << i)) == 0;
      wmGizmo *gz = ggd->gizmos.xyz_view[i];
      WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, hide);
      if (!hide) {
        RNA_float_set(gz->ptr,
                      "arc_partial_angle",
                      (M_PI * 2) - (dial_angle_partial * dial_angle_partial_margin));
      }
    }

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 2; j++) {
        wmGizmo *gz = ggd->gizmos.icon_button[i][j];
        if (gz->state & WM_GIZMO_STATE_HIGHLIGHT) {
          WM_gizmo_set_flag(ggd->gizmos.xyz_view[i], WM_GIZMO_HIDDEN, false);
          RNA_float_set(ggd->gizmos.xyz_view[i]->ptr, "arc_partial_angle", 0.0f);
          i = 3;
          break;
        }
      }
    }
  }
#endif
}

static void gizmo_mesh_spin_init_invoke_prepare(const bContext *UNUSED(C),
                                                wmGizmoGroup *gzgroup,
                                                wmGizmo *gz,
                                                const wmEvent *UNUSED(event))
{
  /* Set the initial ortho axis. */
  GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
  ggd->invoke.ortho_axis_active = -1;
  for (int i = 0; i < 3; i++) {
    if (ELEM(gz, UNPACK2(ggd->gizmos.icon_button[i]))) {
      ggd->invoke.ortho_axis_active = i;
      break;
    }
  }
}

static void gizmo_mesh_spin_init_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
  RegionView3D *rv3d = ED_view3d_context_rv3d((bContext *)C);
  const float *gizmo_center = NULL;
  {
    Scene *scene = CTX_data_scene(C);
    const View3DCursor *cursor = &scene->cursor;
    gizmo_center = cursor->location;
  }

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.xyz_view); i++) {
    wmGizmo *gz = ggd->gizmos.xyz_view[i];
    WM_gizmo_set_matrix_location(gz, gizmo_center);
  }

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.icon_button); i++) {
    for (int j = 0; j < 2; j++) {
      wmGizmo *gz = ggd->gizmos.icon_button[i][j];
      WM_gizmo_set_matrix_location(gz, gizmo_center);
    }
  }

  ED_transform_calc_orientation_from_type(C, ggd->data.orient_mat);
  for (int i = 0; i < 3; i++) {
    const int axis_ortho = (i + ORTHO_AXIS_OFFSET) % 3;
    const float *axis_ortho_vec = ggd->data.orient_mat[axis_ortho];
#ifdef USE_SELECT_CENTER
    if (ggd->data.use_select_center) {
      float delta[3];
      sub_v3_v3v3(delta, ggd->data.select_center, gizmo_center);
      project_plane_normalized_v3_v3v3(
          ggd->data.select_center_ortho_axis[i], delta, ggd->data.orient_mat[i]);
      if (normalize_v3(ggd->data.select_center_ortho_axis[i]) != 0.0f) {
        axis_ortho_vec = ggd->data.select_center_ortho_axis[i];
      }
    }
#endif
    gizmo_mesh_spin_init_refresh_axis_orientation(
        gzgroup, i, ggd->data.orient_mat[i], axis_ortho_vec);
  }

  {
    gizmo_mesh_spin_init_refresh_axis_orientation(gzgroup, 3, rv3d->viewinv[2], NULL);
  }

#ifdef USE_SELECT_CENTER
  {
    Object *obedit = CTX_data_edit_object(C);
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    float select_center[3] = {0};
    int totsel = 0;

    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;

    BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
      if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          totsel++;
          add_v3_v3(select_center, eve->co);
        }
      }
    }
    if (totsel) {
      mul_v3_fl(select_center, 1.0f / totsel);
      mul_m4_v3(obedit->obmat, select_center);
      copy_v3_v3(ggd->data.select_center, select_center);
      ggd->data.use_select_center = true;
    }
    else {
      ggd->data.use_select_center = false;
    }
  }
#endif

  for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.icon_button); i++) {
    const int axis_ortho = (i + ORTHO_AXIS_OFFSET) % 3;
    const float *axis_ortho_vec = ggd->data.orient_mat[axis_ortho];
    float offset = INIT_SCALE_BASE / INIT_SCALE_BUTTON;
    float offset_vec[3];

#ifdef USE_SELECT_CENTER
    if (ggd->data.use_select_center && !is_zero_v3(ggd->data.select_center_ortho_axis[i])) {
      axis_ortho_vec = ggd->data.select_center_ortho_axis[i];
    }
#endif

    mul_v3_v3fl(offset_vec, axis_ortho_vec, offset);
    for (int j = 0; j < 2; j++) {
      wmGizmo *gz = ggd->gizmos.icon_button[i][j];
      float mat3[3][3];
      axis_angle_to_mat3(mat3, ggd->data.orient_mat[i], dial_angle_partial * (j ? -0.5f : 0.5f));
      mul_v3_m3v3(gz->matrix_offset[3], mat3, offset_vec);
    }
  }

  {
    PointerRNA ptr;
    bToolRef *tref = WM_toolsystem_ref_from_context((bContext *)C);
    WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup->type, &ptr);
    const int axis_flag = RNA_property_enum_get(&ptr, ggd->data.gzgt_axis_prop);
    for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.icon_button); i++) {
      for (int j = 0; j < 2; j++) {
        wmGizmo *gz = ggd->gizmos.icon_button[i][j];
        WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, (axis_flag & (1 << i)) == 0);
      }
    }
  }

  /* Needed to test view orientation changes. */
  copy_m3_m4(ggd->prev.viewinv_m3, rv3d->viewinv);
}

static void gizmo_mesh_spin_init_message_subscribe(const bContext *C,
                                                   wmGizmoGroup *gzgroup,
                                                   struct wmMsgBus *mbus)
{
  GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
  Scene *scene = CTX_data_scene(C);
  ARegion *ar = CTX_wm_region(C);

  /* Subscribe to view properties */
  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
      .owner = ar,
      .user_data = gzgroup->parent_gzmap,
      .notify = WM_gizmo_do_msg_notify_tag_refresh,
  };

  PointerRNA cursor_ptr;
  RNA_pointer_create(&scene->id, &RNA_View3DCursor, &scene->cursor, &cursor_ptr);
  /* All cursor properties. */
  WM_msg_subscribe_rna(mbus, &cursor_ptr, NULL, &msg_sub_value_gz_tag_refresh, __func__);

  WM_msg_subscribe_rna_params(mbus,
                              &(const wmMsgParams_RNA){
                                  .ptr =
                                      (PointerRNA){
                                          .type = gzgroup->type->srna,
                                      },
                                  .prop = ggd->data.gzgt_axis_prop,
                              },
                              &msg_sub_value_gz_tag_refresh,
                              __func__);
}

void MESH_GGT_spin(struct wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Spin Init";
  gzgt->idname = "MESH_GGT_spin";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = gizmo_mesh_spin_init_setup;
  gzgt->refresh = gizmo_mesh_spin_init_refresh;
  gzgt->message_subscribe = gizmo_mesh_spin_init_message_subscribe;
  gzgt->draw_prepare = gizmo_mesh_spin_init_draw_prepare;
  gzgt->invoke_prepare = gizmo_mesh_spin_init_invoke_prepare;

  RNA_def_enum_flag(gzgt->srna, "axis", rna_enum_axis_flag_xyz_items, (1 << 2), "Axis", "");
}

#undef INIT_SCALE_BASE
#undef INIT_SCALE_BUTTON

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spin Redo Gizmo
 * \{ */

/**
 * Orient the dial so the 'arc' starts where the mouse cursor is,
 * this is simply to keep the gizmo displaying where the cursor starts.
 * It's not needed for practical functionality.
 */
#define USE_ANGLE_Z_ORIENT

typedef struct GizmoGroupData_SpinRedo {
  /* Translate XYZ. */
  struct wmGizmo *translate_c;
  /* Spin angle */
  struct wmGizmo *angle_z;

  /* Translate XY constrained ('orient_mat'). */
  struct wmGizmo *translate_xy[2];
  /* Rotate XY constrained ('orient_mat'). */
  struct wmGizmo *rotate_xy[2];

  /* Rotate on view axis. */
  struct wmGizmo *rotate_view;

  struct {
    float plane_co[3];
    float plane_no[3];
  } prev;

  bool is_init;

  /* We could store more vars here! */
  struct {
    bContext *context;
    wmOperatorType *ot;
    wmOperator *op;
    PropertyRNA *prop_axis_co;
    PropertyRNA *prop_axis_no;
    PropertyRNA *prop_angle;

    float rotate_axis[3];
#ifdef USE_ANGLE_Z_ORIENT
    /* Apply 'orient_mat' for the final value. */
    float orient_axis_relative[3];
#endif
    /* The orientation, since the operator doesn't store this, we store our own.
     * this is kept in sync with the operator,
     * rotating the orientation when it doesn't match.
     *
     * Initialize to a sensible value where possible.
     */
    float orient_mat[3][3];

  } data;
} GizmoGroupData_SpinRedo;

/**
 * XXX. calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void gizmo_spin_exec(GizmoGroupData_SpinRedo *ggd)
{
  if (ggd->is_init) {
    wmGizmo *gz = ggd->angle_z;
    PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "click_value");
    RNA_property_unset(gz->ptr, prop);
    ggd->is_init = false;
  }

  wmOperator *op = ggd->data.op;
  if (op == WM_operator_last_redo((bContext *)ggd->data.context)) {
    ED_undo_operator_repeat((bContext *)ggd->data.context, op);
  }
}

static void gizmo_mesh_spin_redo_update_orient_axis(GizmoGroupData_SpinRedo *ggd,
                                                    const float plane_no[3])
{
  float mat[3][3];
  rotation_between_vecs_to_mat3(mat, ggd->data.orient_mat[2], plane_no);
  mul_m3_m3m3(ggd->data.orient_mat, mat, ggd->data.orient_mat);
  /* Not needed, just set for numeric stability. */
  copy_v3_v3(ggd->data.orient_mat[2], plane_no);
}

static void gizmo_mesh_spin_redo_update_from_op(GizmoGroupData_SpinRedo *ggd)
{
  wmOperator *op = ggd->data.op;
  float plane_co[3], plane_no[3];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
  if (UNLIKELY(normalize_v3(plane_no) == 0.0f)) {
    return;
  }
  const bool is_plane_co_eq = equals_v3v3(plane_co, ggd->prev.plane_co);
  const bool is_plane_no_eq = equals_v3v3(plane_no, ggd->prev.plane_no);
  if (is_plane_co_eq && is_plane_no_eq) {
    return;
  }
  copy_v3_v3(ggd->prev.plane_co, plane_co);
  copy_v3_v3(ggd->prev.plane_no, plane_no);

  if (is_plane_no_eq == false) {
    gizmo_mesh_spin_redo_update_orient_axis(ggd, plane_no);
  }

  for (int i = 0; i < 2; i++) {
    WM_gizmo_set_matrix_location(ggd->rotate_xy[i], plane_co);
    WM_gizmo_set_matrix_location(ggd->translate_xy[i], plane_co);
  }
  WM_gizmo_set_matrix_location(ggd->angle_z, plane_co);
  WM_gizmo_set_matrix_location(ggd->rotate_view, plane_co);
  /* translate_c location comes from the property. */

  for (int i = 0; i < 2; i++) {
    WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_xy[i], ggd->data.orient_mat[i]);
    WM_gizmo_set_matrix_rotation_from_z_axis(ggd->rotate_xy[i], ggd->data.orient_mat[i]);
  }
#ifdef USE_ANGLE_Z_ORIENT
  {
    float plane_tan[3];
    float orient_axis[3];
    mul_v3_m3v3(orient_axis, ggd->data.orient_mat, ggd->data.orient_axis_relative);
    project_plane_normalized_v3_v3v3(plane_tan, orient_axis, plane_no);
    if (normalize_v3(plane_tan) != 0.0f) {
      WM_gizmo_set_matrix_rotation_from_yz_axis(ggd->angle_z, plane_tan, plane_no);
    }
    else {
      WM_gizmo_set_matrix_rotation_from_z_axis(ggd->angle_z, plane_no);
    }
  }
#else
  WM_gizmo_set_matrix_rotation_from_z_axis(ggd->angle_z, plane_no);
#endif
}

/* depth callbacks */
static void gizmo_spin_prop_depth_get(const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  const float *plane_no = gz->matrix_basis[2];
  float plane_co[3];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);

  value[0] = dot_v3v3(plane_no, plane_co) - dot_v3v3(plane_no, gz->matrix_basis[3]);
}

static void gizmo_spin_prop_depth_set(const wmGizmo *gz,
                                      wmGizmoProperty *gz_prop,
                                      const void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  const float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_co[3], plane[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
  normalize_v3_v3(plane, gz->matrix_basis[2]);

  plane[3] = -value[0] - dot_v3v3(plane, gz->matrix_basis[3]);

  /* Keep our location, may be offset simply to be inside the viewport. */
  closest_to_plane_normalized_v3(plane_co, plane, plane_co);

  RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_co, plane_co);

  gizmo_spin_exec(ggd);
}

/* translate callbacks */
static void gizmo_spin_prop_translate_get(const wmGizmo *gz,
                                          wmGizmoProperty *gz_prop,
                                          void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 3);
  UNUSED_VARS_NDEBUG(gz_prop);

  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, value);
}

static void gizmo_spin_prop_translate_set(const wmGizmo *gz,
                                          wmGizmoProperty *gz_prop,
                                          const void *value)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;

  BLI_assert(gz_prop->type->array_length == 3);
  UNUSED_VARS_NDEBUG(gz_prop);

  RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_co, value);

  gizmo_spin_exec(ggd);
}

/* angle callbacks */
static void gizmo_spin_prop_axis_angle_get(const wmGizmo *gz,
                                           wmGizmoProperty *gz_prop,
                                           void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_no[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
  normalize_v3(plane_no);

  const float *rotate_axis = gz->matrix_basis[2];
  float rotate_up[3];
  ortho_v3_v3(rotate_up, rotate_axis);

  float plane_no_proj[3];
  project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, rotate_axis);

  if (!is_zero_v3(plane_no_proj)) {
    const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, rotate_up, rotate_axis);
    value[0] = angle;
  }
  else {
    value[0] = 0.0f;
  }
}

static void gizmo_spin_prop_axis_angle_set(const wmGizmo *gz,
                                           wmGizmoProperty *gz_prop,
                                           const void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  const float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_no[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
  normalize_v3(plane_no);

  const float *rotate_axis = gz->matrix_basis[2];
  float rotate_up[3];
  ortho_v3_v3(rotate_up, rotate_axis);

  float plane_no_proj[3];
  project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, rotate_axis);

  if (!is_zero_v3(plane_no_proj)) {
    const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, rotate_up, rotate_axis);
    const float angle_delta = angle - angle_compat_rad(value[0], angle);
    if (angle_delta != 0.0f) {
      float mat[3][3];
      axis_angle_normalized_to_mat3(mat, rotate_axis, angle_delta);
      mul_m3_v3(mat, plane_no);

      /* re-normalize - seems acceptable */
      RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_no, plane_no);

      gizmo_spin_exec(ggd);
    }
  }
}

/* angle callbacks */
static void gizmo_spin_prop_angle_get(const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  float *value = value_p;

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);
  value[0] = RNA_property_float_get(op->ptr, ggd->data.prop_angle);
}

static void gizmo_spin_prop_angle_set(const wmGizmo *gz,
                                      wmGizmoProperty *gz_prop,
                                      const void *value_p)
{
  GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
  wmOperator *op = ggd->data.op;
  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);
  const float *value = value_p;
  RNA_property_float_set(op->ptr, ggd->data.prop_angle, value[0]);

  gizmo_spin_exec(ggd);
}

static bool gizmo_mesh_spin_redo_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (ED_gizmo_poll_or_unlink_delayed_from_operator(C, gzgt, "MESH_OT_spin")) {
    if (ED_gizmo_poll_or_unlink_delayed_from_tool_ex(C, gzgt, "MESH_GGT_spin")) {
      return true;
    }
  }
  return false;
}

static void gizmo_mesh_spin_redo_modal_from_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  /* Start off dragging. */
  GizmoGroupData_SpinRedo *ggd = gzgroup->customdata;
  wmWindow *win = CTX_wm_window(C);
  wmGizmo *gz = ggd->angle_z;
  wmGizmoMap *gzmap = gzgroup->parent_gzmap;

  ggd->is_init = true;

  WM_gizmo_modal_set_from_setup(gzmap, (bContext *)C, gz, 0, win->eventstate);
}

static void gizmo_mesh_spin_redo_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmOperatorType *ot = WM_operatortype_find("MESH_OT_spin", true);
  wmOperator *op = WM_operator_last_redo(C);

  if ((op == NULL) || (op->type != ot)) {
    return;
  }

  GizmoGroupData_SpinRedo *ggd = MEM_callocN(sizeof(*ggd), __func__);
  gzgroup->customdata = ggd;

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_move = WM_gizmotype_find("GIZMO_GT_move_3d", true);
  const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);

  /* Rotate View Axis (rotate_view) */
  {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    zero_v4(gz->color);
    copy_v3_fl(gz->color_hi, 1.0f);
    gz->color_hi[3] = 0.1f;
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE, true);
    RNA_enum_set(gz->ptr,
                 "draw_options",
                 ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR | ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y |
                     ED_GIZMO_DIAL_DRAW_FLAG_FILL);
    ggd->rotate_view = gz;
  }

  /* Translate Center (translate_c) */
  {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_move, gzgroup, NULL);
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    gz->color[3] = 0.6f;
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_RING_2D);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE, true);
    WM_gizmo_set_scale(gz, 0.15);
    WM_gizmo_set_line_width(gz, 2.0f);
    ggd->translate_c = gz;
  }

  /* Spin Angle (angle_z) */
  {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
    copy_v3_v3(gz->color, gz->color_hi);
    gz->color[3] = 0.5f;
    RNA_boolean_set(gz->ptr, "wrap_angle", false);
    RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE);
    RNA_float_set(gz->ptr, "arc_inner_factor", 0.9f);
    RNA_float_set(gz->ptr, "click_value", M_PI * 2);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE, true);
    WM_gizmo_set_scale(gz, 2.0f);
    WM_gizmo_set_line_width(gz, 1.0f);
    ggd->angle_z = gz;
  }

  /* Translate X/Y Tangents (translate_xy) */
  for (int i = 0; i < 2; i++) {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
    UI_GetThemeColor3fv(TH_AXIS_X + i, gz->color);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_NORMAL);
    RNA_enum_set(gz->ptr, "draw_options", 0);
    WM_gizmo_set_scale(gz, 1.2f);
    ggd->translate_xy[i] = gz;
  }

  /* Rotate X/Y Tangents (rotate_xy) */
  for (int i = 0; i < 2; i++) {
    wmGizmo *gz = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
    UI_GetThemeColor3fv(TH_AXIS_X + i, gz->color);
    gz->color[3] = 0.6f;
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE, true);
    WM_gizmo_set_line_width(gz, 3.0f);
    /* show the axis instead of mouse cursor */
    RNA_enum_set(gz->ptr,
                 "draw_options",
                 ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR | ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y |
                     ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
    ggd->rotate_xy[i] = gz;
  }

  {
    ggd->data.context = (bContext *)C;
    ggd->data.ot = ot;
    ggd->data.op = op;
    ggd->data.prop_axis_co = RNA_struct_type_find_property(ot->srna, "center");
    ggd->data.prop_axis_no = RNA_struct_type_find_property(ot->srna, "axis");
    ggd->data.prop_angle = RNA_struct_type_find_property(ot->srna, "angle");
  }

  /* The spin operator only knows about an axis,
   * while the manipulator has X/Y orientation for the gizmos.
   * Initialize the orientation from the spin gizmo if possible.
   */
  {
    ARegion *ar = CTX_wm_region(C);
    wmGizmoMap *gzmap = ar->gizmo_map;
    wmGizmoGroup *gzgroup_init = WM_gizmomap_group_find(gzmap, "MESH_GGT_spin");
    if (gzgroup_init) {
      GizmoGroupData_SpinInit *ggd_init = gzgroup_init->customdata;
      copy_m3_m3(ggd->data.orient_mat, ggd_init->data.orient_mat);
      if (ggd_init->invoke.ortho_axis_active != -1) {
        copy_v3_v3(ggd->data.orient_axis_relative,
                   ggd_init->gizmos.xyz_view[ggd_init->invoke.ortho_axis_active]->matrix_basis[1]);
        ggd_init->invoke.ortho_axis_active = -1;
      }
    }
    else {
      unit_m3(ggd->data.orient_mat);
    }
  }

#ifdef USE_ANGLE_Z_ORIENT
  {
    wmWindow *win = CTX_wm_window(C);
    View3D *v3d = CTX_wm_view3d(C);
    ARegion *ar = CTX_wm_region(C);
    const wmEvent *event = win->eventstate;
    float plane_co[3], plane_no[3];
    RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
    RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);

    gizmo_mesh_spin_redo_update_orient_axis(ggd, plane_no);

    /* Use cursor as fallback if it's not set by the 'ortho_axis_active'. */
    if (is_zero_v3(ggd->data.orient_axis_relative)) {
      float cursor_co[3];
      const int mval[2] = {event->x - ar->winrct.xmin, event->y - ar->winrct.ymin};
      float plane[4];
      plane_from_point_normal_v3(plane, plane_co, plane_no);
      if (UNLIKELY(!ED_view3d_win_to_3d_on_plane_int(ar, plane, mval, false, cursor_co))) {
        ED_view3d_win_to_3d_int(v3d, ar, plane, mval, cursor_co);
      }
      sub_v3_v3v3(ggd->data.orient_axis_relative, cursor_co, plane_co);
    }

    if (!is_zero_v3(ggd->data.orient_axis_relative)) {
      normalize_v3(ggd->data.orient_axis_relative);
      float imat3[3][3];
      invert_m3_m3(imat3, ggd->data.orient_mat);
      mul_m3_v3(imat3, ggd->data.orient_axis_relative);
    }
  }
#endif

  gizmo_mesh_spin_redo_update_from_op(ggd);

  /* Setup property callbacks */
  {
    WM_gizmo_target_property_def_func(ggd->translate_c,
                                      "offset",
                                      &(const struct wmGizmoPropertyFnParams){
                                          .value_get_fn = gizmo_spin_prop_translate_get,
                                          .value_set_fn = gizmo_spin_prop_translate_set,
                                          .range_get_fn = NULL,
                                          .user_data = NULL,
                                      });

    WM_gizmo_target_property_def_func(ggd->rotate_view,
                                      "offset",
                                      &(const struct wmGizmoPropertyFnParams){
                                          .value_get_fn = gizmo_spin_prop_axis_angle_get,
                                          .value_set_fn = gizmo_spin_prop_axis_angle_set,
                                          .range_get_fn = NULL,
                                          .user_data = NULL,
                                      });

    for (int i = 0; i < 2; i++) {
      WM_gizmo_target_property_def_func(ggd->rotate_xy[i],
                                        "offset",
                                        &(const struct wmGizmoPropertyFnParams){
                                            .value_get_fn = gizmo_spin_prop_axis_angle_get,
                                            .value_set_fn = gizmo_spin_prop_axis_angle_set,
                                            .range_get_fn = NULL,
                                            .user_data = NULL,
                                        });
      WM_gizmo_target_property_def_func(ggd->translate_xy[i],
                                        "offset",
                                        &(const struct wmGizmoPropertyFnParams){
                                            .value_get_fn = gizmo_spin_prop_depth_get,
                                            .value_set_fn = gizmo_spin_prop_depth_set,
                                            .range_get_fn = NULL,
                                            .user_data = NULL,
                                        });
    }

    WM_gizmo_target_property_def_func(ggd->angle_z,
                                      "offset",
                                      &(const struct wmGizmoPropertyFnParams){
                                          .value_get_fn = gizmo_spin_prop_angle_get,
                                          .value_set_fn = gizmo_spin_prop_angle_set,
                                          .range_get_fn = NULL,
                                          .user_data = NULL,
                                      });
  }

  /* Become modal as soon as it's started. */
  gizmo_mesh_spin_redo_modal_from_setup(C, gzgroup);
}

static void gizmo_mesh_spin_redo_draw_prepare(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  GizmoGroupData_SpinRedo *ggd = gzgroup->customdata;
  if (ggd->data.op->next) {
    ggd->data.op = WM_operator_last_redo((bContext *)ggd->data.context);
  }

  /* Not essential, just avoids feedback loop where matrices
   * could shift because of float precision.
   * Updates in this case are also redundant. */
  bool is_modal = false;
  for (wmGizmo *gz = gzgroup->gizmos.first; gz; gz = gz->next) {
    if (gz->state & WM_GIZMO_STATE_MODAL) {
      is_modal = true;
      break;
    }
  }
  if (!is_modal) {
    gizmo_mesh_spin_redo_update_from_op(ggd);
  }

  RegionView3D *rv3d = ED_view3d_context_rv3d(ggd->data.context);
  WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_c, rv3d->viewinv[2]);
  {
    float view_up[3];
    project_plane_normalized_v3_v3v3(view_up, ggd->data.orient_mat[2], rv3d->viewinv[2]);
    if (normalize_v3(view_up) != 0.0f) {
      WM_gizmo_set_matrix_rotation_from_yz_axis(ggd->rotate_view, view_up, rv3d->viewinv[2]);
    }
    else {
      WM_gizmo_set_matrix_rotation_from_z_axis(ggd->rotate_view, rv3d->viewinv[2]);
    }
  }
}

void MESH_GGT_spin_redo(struct wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Spin Redo";
  gzgt->idname = "MESH_GGT_spin_redo";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = gizmo_mesh_spin_redo_poll;
  gzgt->setup = gizmo_mesh_spin_redo_setup;
  gzgt->draw_prepare = gizmo_mesh_spin_redo_draw_prepare;
}

/** \} */
