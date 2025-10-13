/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"
#include "BKE_workspace.hh"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gizmo_utils.hh"
#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

#include "mesh_intern.hh" /* own include */

#define USE_GIZMO

#ifdef USE_GIZMO
#  include "ED_gizmo_library.hh"
#  include "ED_undo.hh"
#endif

using blender::Vector;

static wmOperatorStatus mesh_bisect_exec(bContext *C, wmOperator *op);

/* -------------------------------------------------------------------- */
/* Model Helpers */

struct BisectData {
  /* modal only */

  /* Aligned with objects array. */
  struct BisectDataBackup {
    BMBackup mesh_backup;
    bool is_valid;
    bool is_dirty;
  } *backup;
  int backup_len;
};

static void mesh_bisect_interactive_calc(bContext *C,
                                         wmOperator *op,
                                         float plane_co[3],
                                         float plane_no[3])
{
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  int x_start = RNA_int_get(op->ptr, "xstart");
  int y_start = RNA_int_get(op->ptr, "ystart");
  int x_end = RNA_int_get(op->ptr, "xend");
  int y_end = RNA_int_get(op->ptr, "yend");
  const bool use_flip = RNA_boolean_get(op->ptr, "flip");

  /* reference location (some point in front of the view) for finding a point on a plane */
  const float *co_ref = rv3d->ofs;
  float co_a_ss[2] = {float(x_start), float(y_start)};
  float co_b_ss[2] = {float(x_end), float(y_end)};
  float co_delta_ss[2];
  float co_a[3], co_b[3];
  const float zfac = ED_view3d_calc_zfac(rv3d, co_ref);

  /* view vector */
  ED_view3d_win_to_vector(region, co_a_ss, co_a);

  /* view delta */
  sub_v2_v2v2(co_delta_ss, co_a_ss, co_b_ss);
  ED_view3d_win_to_delta(region, co_delta_ss, zfac, co_b);

  /* cross both to get a normal */
  cross_v3_v3v3(plane_no, co_a, co_b);
  normalize_v3(plane_no); /* not needed but nicer for user */
  if (use_flip) {
    negate_v3(plane_no);
  }

  /* point on plane, can use either start or endpoint */
  ED_view3d_win_to_3d(v3d, region, co_ref, co_a_ss, plane_co);
}

static wmOperatorStatus mesh_bisect_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int valid_objects = 0;

  /* If the properties are set or there is no rv3d,
   * skip modal and exec immediately. */
  if ((CTX_wm_region_view3d(C) == nullptr) || (RNA_struct_property_is_set(op->ptr, "plane_co") &&
                                               RNA_struct_property_is_set(op->ptr, "plane_no")))
  {
    return mesh_bisect_exec(C, op);
  }

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totedgesel != 0) {
      valid_objects++;
    }
  }

  if (valid_objects == 0) {
    BKE_report(op->reports, RPT_ERROR, "Selected edges/faces required");
    return OPERATOR_CANCELLED;
  }

  /* Support flipping if side matters. */
  wmOperatorStatus ret;
  const bool clear_inner = RNA_boolean_get(op->ptr, "clear_inner");
  const bool clear_outer = RNA_boolean_get(op->ptr, "clear_outer");
  const bool use_fill = RNA_boolean_get(op->ptr, "use_fill");
  if ((clear_inner != clear_outer) || use_fill) {
    ret = WM_gesture_straightline_active_side_invoke(C, op, event);
  }
  else {
    ret = WM_gesture_straightline_invoke(C, op, event);
  }

  if (ret & OPERATOR_RUNNING_MODAL) {
    wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
    BisectData *opdata;

    opdata = MEM_mallocN<BisectData>("inset_operator_data");
    gesture->user_data.data = opdata;

    opdata->backup_len = objects.size();
    opdata->backup = static_cast<BisectData::BisectDataBackup *>(
        MEM_callocN(sizeof(*opdata->backup) * objects.size(), __func__));

    /* Store the mesh backups. */
    for (const int ob_index : objects.index_range()) {
      Object *obedit = objects[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(obedit);

      if (em->bm->totedgesel != 0) {
        opdata->backup[ob_index].is_valid = true;
        opdata->backup[ob_index].mesh_backup = EDBM_redo_state_store(em);
      }
    }

    /* Misc other vars. */
    G.moving = G_TRANSFORM_EDIT;

    /* Initialize modal callout. */
    WorkspaceStatus status(C);
    status.item(IFACE_("Cancel"), ICON_EVENT_ESC);
    status.item(IFACE_("Draw Cut Line"), ICON_MOUSE_LMB_DRAG);
  }
  return ret;
}

static void edbm_bisect_exit(BisectData *opdata)
{
  G.moving = 0;

  for (int ob_index = 0; ob_index < opdata->backup_len; ob_index++) {
    if (opdata->backup[ob_index].is_valid) {
      EDBM_redo_state_free(&opdata->backup[ob_index].mesh_backup);
    }
  }
  MEM_freeN(opdata->backup);
}

static wmOperatorStatus mesh_bisect_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
  BisectData *opdata = static_cast<BisectData *>(gesture->user_data.data);
  BisectData opdata_back = *opdata; /* annoyance, WM_gesture_straightline_modal, frees */
  wmOperatorStatus ret;

  ret = WM_gesture_straightline_modal(C, op, event);

  /* update or clear modal callout */
  WorkSpace *workspace = CTX_wm_workspace(C);

  if (workspace) {
    BKE_workspace_status_clear(workspace);
  }

  if (ret & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
    edbm_bisect_exit(&opdata_back);

#ifdef USE_GIZMO
    /* Setup gizmos */
    {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d && (v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
        WM_gizmo_group_type_ensure("MESH_GGT_bisect");
      }
    }
#endif
  }

  return ret;
}

/* End Model Helpers */
/* -------------------------------------------------------------------- */

static wmOperatorStatus mesh_bisect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  /* both can be nullptr, fallbacks values are used */
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  wmOperatorStatus ret = OPERATOR_CANCELLED;

  float plane_co[3];
  float plane_no[3];
  float imat[4][4];

  const float thresh = RNA_float_get(op->ptr, "threshold");
  const bool use_fill = RNA_boolean_get(op->ptr, "use_fill");
  const bool clear_inner = RNA_boolean_get(op->ptr, "clear_inner");
  const bool clear_outer = RNA_boolean_get(op->ptr, "clear_outer");

  PropertyRNA *prop_plane_co;
  PropertyRNA *prop_plane_no;

  prop_plane_co = RNA_struct_find_property(op->ptr, "plane_co");
  if (RNA_property_is_set(op->ptr, prop_plane_co)) {
    RNA_property_float_get_array(op->ptr, prop_plane_co, plane_co);
  }
  else {
    copy_v3_v3(plane_co, scene->cursor.location);
    RNA_property_float_set_array(op->ptr, prop_plane_co, plane_co);
  }

  prop_plane_no = RNA_struct_find_property(op->ptr, "plane_no");
  if (RNA_property_is_set(op->ptr, prop_plane_no)) {
    RNA_property_float_get_array(op->ptr, prop_plane_no, plane_no);
  }
  else {
    if (rv3d) {
      copy_v3_v3(plane_no, rv3d->viewinv[1]);
    }
    else {
      /* fallback... */
      plane_no[0] = plane_no[1] = 0.0f;
      plane_no[2] = 1.0f;
    }
    RNA_property_float_set_array(op->ptr, prop_plane_no, plane_no);
  }

  wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
  BisectData *opdata = static_cast<BisectData *>((gesture != nullptr) ? gesture->user_data.data :
                                                                        nullptr);

  /* -------------------------------------------------------------------- */
  /* Modal support */
  /* NOTE: keep this isolated, exec can work without this. */
  if (opdata != nullptr) {
    mesh_bisect_interactive_calc(C, op, plane_co, plane_no);
    /* Write back to the props. */
    RNA_property_float_set_array(op->ptr, prop_plane_no, plane_no);
    RNA_property_float_set_array(op->ptr, prop_plane_co, plane_co);
  }
  /* End Modal */
  /* -------------------------------------------------------------------- */

  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C));

  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if (opdata != nullptr) {
      if (opdata->backup[ob_index].is_dirty) {
        EDBM_redo_state_restore(&opdata->backup[ob_index].mesh_backup, em, false);
        opdata->backup[ob_index].is_dirty = false;
      }
    }

    if (bm->totedgesel == 0) {
      continue;
    }

    if (opdata != nullptr) {
      if (opdata->backup[ob_index].is_valid) {
        opdata->backup[ob_index].is_dirty = true;
      }
    }

    float plane_co_local[3];
    float plane_no_local[3];
    copy_v3_v3(plane_co_local, plane_co);
    copy_v3_v3(plane_no_local, plane_no);

    invert_m4_m4(imat, obedit->object_to_world().ptr());
    mul_m4_v3(imat, plane_co_local);
    mul_transposed_mat3_m4_v3(obedit->object_to_world().ptr(), plane_no_local);

    BMOperator bmop;
    EDBM_op_init(
        em,
        &bmop,
        op,
        "bisect_plane geom=%hvef plane_co=%v plane_no=%v dist=%f clear_inner=%b clear_outer=%b",
        BM_ELEM_SELECT,
        plane_co_local,
        plane_no_local,
        thresh,
        clear_inner,
        clear_outer);
    BMO_op_exec(bm, &bmop);

    EDBM_flag_disable_all(em, BM_ELEM_SELECT);

    if (use_fill) {
      float normal_fill[3];
      BMOperator bmop_fill;
      BMOperator bmop_attr;

      /* The fill normal sign is ignored as the face-winding is defined by surrounding faces.
       * The normal is passed so triangle fill won't have to calculate it. */
      normalize_v3_v3(normal_fill, plane_no_local);

      /* Fill */
      BMO_op_initf(bm,
                   &bmop_fill,
                   0,
                   "triangle_fill edges=%S normal=%v use_dissolve=%b",
                   &bmop,
                   "geom_cut.out",
                   normal_fill,
                   true);
      BMO_op_exec(bm, &bmop_fill);

      /* Copy Attributes */
      BMO_op_initf(bm,
                   &bmop_attr,
                   0,
                   "face_attribute_fill faces=%S use_normals=%b use_data=%b",
                   &bmop_fill,
                   "geom.out",
                   true,
                   true);
      BMO_op_exec(bm, &bmop_attr);

      BMO_slot_buffer_hflag_enable(
          bm, bmop_fill.slots_out, "geom.out", BM_FACE, BM_ELEM_SELECT, true);

      BMO_op_finish(bm, &bmop_attr);
      BMO_op_finish(bm, &bmop_fill);
    }

    BMO_slot_buffer_hflag_enable(
        bm, bmop.slots_out, "geom_cut.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

    if (EDBM_op_finish(em, &bmop, op, true)) {
      EDBMUpdate_Params params{};
      params.calc_looptris = true;
      params.calc_normals = false;
      params.is_destructive = true;
      EDBM_update(static_cast<Mesh *>(obedit->data), &params);

      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);

      ret = OPERATOR_FINISHED;
    }
  }
  return ret;
}

#ifdef USE_GIZMO
static void MESH_GGT_bisect(wmGizmoGroupType *gzgt);
#endif

void MESH_OT_bisect(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Bisect";
  ot->description = "Cut geometry along a plane (click-drag to define plane)";
  ot->idname = "MESH_OT_bisect";

  /* API callbacks. */
  ot->exec = mesh_bisect_exec;
  ot->invoke = mesh_bisect_invoke;
  ot->modal = mesh_bisect_modal;
  ot->cancel = WM_gesture_straightline_cancel;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_float_vector_xyz(ot->srna,
                                  "plane_co",
                                  3,
                                  nullptr,
                                  -1e12f,
                                  1e12f,
                                  "Plane Point",
                                  "A point on the plane",
                                  -1e4f,
                                  1e4f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float_vector(ot->srna,
                              "plane_no",
                              3,
                              nullptr,
                              -1.0f,
                              1.0f,
                              "Plane Normal",
                              "The direction the plane points",
                              -1.0f,
                              1.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna, "use_fill", false, "Fill", "Fill in the cut");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MASK);

  RNA_def_boolean(
      ot->srna, "clear_inner", false, "Clear Inner", "Remove geometry behind the plane");
  RNA_def_boolean(
      ot->srna, "clear_outer", false, "Clear Outer", "Remove geometry in front of the plane");

  prop = RNA_def_float(ot->srna,
                       "threshold",
                       0.0001,
                       0.0,
                       10.0,
                       "Axis Threshold",
                       "Preserves the existing geometry along the cut plane",
                       0.00001,
                       0.1);
  /* Without higher precision, the default value displays as zero. */
  RNA_def_property_ui_range(prop, 0.0, 10.0, 0.01, 5);

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);

#ifdef USE_GIZMO
  WM_gizmogrouptype_append(MESH_GGT_bisect);
#endif
}

#ifdef USE_GIZMO

/* -------------------------------------------------------------------- */
/** \name Bisect Gizmo
 * \{ */

struct GizmoGroup {
  /* Arrow to change plane depth. */
  wmGizmo *translate_z;
  /* Translate XYZ */
  wmGizmo *translate_c;
  /* For grabbing the gizmo and moving freely. */
  wmGizmo *rotate_c;

  /* We could store more vars here! */
  struct {
    bContext *context;
    wmOperator *op;
    PropertyRNA *prop_plane_co;
    PropertyRNA *prop_plane_no;

    float rotate_axis[3];
    float rotate_up[3];
  } data;
};

/**
 * XXX. calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void gizmo_bisect_exec(GizmoGroup *ggd)
{
  wmOperator *op = ggd->data.op;
  if (op == WM_operator_last_redo(ggd->data.context)) {
    ED_undo_operator_repeat(ggd->data.context, op);
  }
}

static void gizmo_mesh_bisect_update_from_op(GizmoGroup *ggd)
{
  wmOperator *op = ggd->data.op;

  float plane_co[3], plane_no[3];

  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_co, plane_co);
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_no, plane_no);

  WM_gizmo_set_matrix_location(ggd->translate_z, plane_co);
  WM_gizmo_set_matrix_location(ggd->rotate_c, plane_co);
  /* translate_c location comes from the property. */

  WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_z, plane_no);

  WM_gizmo_set_scale(ggd->translate_c, 0.2);

  RegionView3D *rv3d = ED_view3d_context_rv3d(ggd->data.context);
  if (rv3d) {
    normalize_v3_v3(ggd->data.rotate_axis, rv3d->viewinv[2]);
    normalize_v3_v3(ggd->data.rotate_up, rv3d->viewinv[1]);

    /* ensure its orthogonal */
    project_plane_normalized_v3_v3v3(
        ggd->data.rotate_up, ggd->data.rotate_up, ggd->data.rotate_axis);
    normalize_v3(ggd->data.rotate_up);

    WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_c, plane_no);
    WM_gizmo_set_matrix_rotation_from_z_axis(ggd->rotate_c, ggd->data.rotate_axis);
  }
}

/* depth callbacks */
static void gizmo_bisect_prop_depth_get(const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;
  float *value = static_cast<float *>(value_p);

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_co[3], plane_no[3];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_co, plane_co);
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_no, plane_no);

  value[0] = dot_v3v3(plane_no, plane_co) - dot_v3v3(plane_no, gz->matrix_basis[3]);
}

static void gizmo_bisect_prop_depth_set(const wmGizmo *gz,
                                        wmGizmoProperty *gz_prop,
                                        const void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;
  const float *value = static_cast<const float *>(value_p);

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_co[3], plane[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_co, plane_co);
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_no, plane);
  normalize_v3(plane);

  plane[3] = -value[0] - dot_v3v3(plane, gz->matrix_basis[3]);

  /* Keep our location, may be offset simply to be inside the viewport. */
  closest_to_plane_normalized_v3(plane_co, plane, plane_co);

  RNA_property_float_set_array(op->ptr, ggd->data.prop_plane_co, plane_co);

  gizmo_bisect_exec(ggd);
}

/* translate callbacks */
static void gizmo_bisect_prop_translate_get(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;

  BLI_assert(gz_prop->type->array_length == 3);
  UNUSED_VARS_NDEBUG(gz_prop);

  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_co, static_cast<float *>(value_p));
}

static void gizmo_bisect_prop_translate_set(const wmGizmo *gz,
                                            wmGizmoProperty *gz_prop,
                                            const void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;

  BLI_assert(gz_prop->type->array_length == 3);
  UNUSED_VARS_NDEBUG(gz_prop);

  RNA_property_float_set_array(
      op->ptr, ggd->data.prop_plane_co, static_cast<const float *>(value_p));

  gizmo_bisect_exec(ggd);
}

/* angle callbacks */
static void gizmo_bisect_prop_angle_get(const wmGizmo *gz, wmGizmoProperty *gz_prop, void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;
  float *value = static_cast<float *>(value_p);

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_no[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_no, plane_no);
  normalize_v3(plane_no);

  float plane_no_proj[3];
  project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, ggd->data.rotate_axis);

  if (!is_zero_v3(plane_no_proj)) {
    const float angle = -angle_signed_on_axis_v3v3_v3(
        plane_no_proj, ggd->data.rotate_up, ggd->data.rotate_axis);
    value[0] = angle;
  }
  else {
    value[0] = 0.0f;
  }
}

static void gizmo_bisect_prop_angle_set(const wmGizmo *gz,
                                        wmGizmoProperty *gz_prop,
                                        const void *value_p)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gz->parent_gzgroup->customdata);
  wmOperator *op = ggd->data.op;
  const float *value = static_cast<const float *>(value_p);

  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  float plane_no[4];
  RNA_property_float_get_array(op->ptr, ggd->data.prop_plane_no, plane_no);
  normalize_v3(plane_no);

  float plane_no_proj[3];
  project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, ggd->data.rotate_axis);

  if (!is_zero_v3(plane_no_proj)) {
    const float angle = -angle_signed_on_axis_v3v3_v3(
        plane_no_proj, ggd->data.rotate_up, ggd->data.rotate_axis);
    const float angle_delta = angle - angle_compat_rad(value[0], angle);
    if (angle_delta != 0.0f) {
      float mat[3][3];
      axis_angle_normalized_to_mat3(mat, ggd->data.rotate_axis, angle_delta);
      mul_m3_v3(mat, plane_no);

      /* re-normalize - seems acceptable */
      RNA_property_float_set_array(op->ptr, ggd->data.prop_plane_no, plane_no);

      gizmo_bisect_exec(ggd);
    }
  }
}

static bool gizmo_mesh_bisect_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  return ED_gizmo_poll_or_unlink_delayed_from_operator(C, gzgt, "MESH_OT_bisect");
}

static void gizmo_mesh_bisect_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmOperator *op = WM_operator_last_redo(C);

  if (op == nullptr || !STREQ(op->type->idname, "MESH_OT_bisect")) {
    return;
  }

  GizmoGroup *ggd = MEM_callocN<GizmoGroup>(__func__);
  gzgroup->customdata = ggd;

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
  const wmGizmoType *gzt_move = WM_gizmotype_find("GIZMO_GT_move_3d", true);
  const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);

  ggd->translate_z = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
  ggd->translate_c = WM_gizmo_new_ptr(gzt_move, gzgroup, nullptr);
  ggd->rotate_c = WM_gizmo_new_ptr(gzt_dial, gzgroup, nullptr);

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->translate_z->color);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->translate_c->color);
  UI_GetThemeColor3fv(TH_GIZMO_SECONDARY, ggd->rotate_c->color);

  RNA_enum_set(ggd->translate_z->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_NORMAL);
  RNA_enum_set(ggd->translate_c->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_RING_2D);

  WM_gizmo_set_flag(ggd->translate_c, WM_GIZMO_DRAW_VALUE, true);
  WM_gizmo_set_flag(ggd->rotate_c, WM_GIZMO_DRAW_VALUE, true);

  {
    ggd->data.context = (bContext *)C;
    ggd->data.op = op;
    ggd->data.prop_plane_co = RNA_struct_find_property(op->ptr, "plane_co");
    ggd->data.prop_plane_no = RNA_struct_find_property(op->ptr, "plane_no");
  }

  gizmo_mesh_bisect_update_from_op(ggd);

  /* Setup property callbacks */
  {
    {
      wmGizmoPropertyFnParams params{};
      params.value_get_fn = gizmo_bisect_prop_depth_get;
      params.value_set_fn = gizmo_bisect_prop_depth_set;
      params.range_get_fn = nullptr;
      params.user_data = nullptr;
      WM_gizmo_target_property_def_func(ggd->translate_z, "offset", &params);
    }

    {
      wmGizmoPropertyFnParams params{};
      params.value_get_fn = gizmo_bisect_prop_translate_get;
      params.value_set_fn = gizmo_bisect_prop_translate_set;
      params.range_get_fn = nullptr;
      params.user_data = nullptr;
      WM_gizmo_target_property_def_func(ggd->translate_c, "offset", &params);
    }

    {
      wmGizmoPropertyFnParams params{};
      params.value_get_fn = gizmo_bisect_prop_angle_get;
      params.value_set_fn = gizmo_bisect_prop_angle_set;
      params.range_get_fn = nullptr;
      params.user_data = nullptr;
      WM_gizmo_target_property_def_func(ggd->rotate_c, "offset", &params);
    }
  }
}

static void gizmo_mesh_bisect_draw_prepare(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  GizmoGroup *ggd = static_cast<GizmoGroup *>(gzgroup->customdata);
  if (ggd->data.op->next) {
    ggd->data.op = WM_operator_last_redo(ggd->data.context);
  }
  gizmo_mesh_bisect_update_from_op(ggd);
}

static void MESH_GGT_bisect(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Mesh Bisect";
  gzgt->idname = "MESH_GGT_bisect";

  gzgt->flag = WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DRAW_MODAL_EXCLUDE;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = gizmo_mesh_bisect_poll;
  gzgt->setup = gizmo_mesh_bisect_setup;
  gzgt->draw_prepare = gizmo_mesh_bisect_draw_prepare;
}

/** \} */

#endif /* USE_GIZMO */
