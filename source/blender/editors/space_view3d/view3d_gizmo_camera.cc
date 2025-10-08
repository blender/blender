/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "ED_gizmo_library.hh"
#include "ED_screen.hh"

#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "WM_message.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"

#include "view3d_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Camera Gizmos
 * \{ */

struct CameraWidgetGroup {
  wmGizmo *dop_dist;
  wmGizmo *focal_len;
  wmGizmo *ortho_scale;
};

static bool WIDGETGROUP_camera_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_LENS) == 0 &&
      (v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_DOF_DIST) == 0)
  {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_active_base_get(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    Object *ob = base->object;
    if (ob->type == OB_CAMERA) {
      const Camera *camera = static_cast<Camera *>(ob->data);
      /* TODO: support overrides. */
      if (BKE_id_is_editable(CTX_data_main(C), &camera->id)) {
        return true;
      }
    }
  }
  return false;
}

static void WIDGETGROUP_camera_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  float dir[3];

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);

  CameraWidgetGroup *cagzgroup = MEM_callocN<CameraWidgetGroup>(__func__);
  gzgroup->customdata = cagzgroup;

  negate_v3_v3(dir, ob->object_to_world().ptr()[2]);

  /* dof distance */
  {
    wmGizmo *gz;
    gz = cagzgroup->dop_dist = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CROSS);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER | WM_GIZMO_DRAW_NO_SCALE, true);

    UI_GetThemeColor3fv(TH_GIZMO_A, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
  }

  /* focal length
   * - logic/calculations are similar to BKE_camera_view_frame_ex, better keep in sync */
  {
    wmGizmo *gz;
    gz = cagzgroup->focal_len = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
    gz->flag |= WM_GIZMO_DRAW_NO_SCALE;
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CONE);
    RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED);

    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

    gz = cagzgroup->ortho_scale = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr);
    gz->flag |= WM_GIZMO_DRAW_NO_SCALE;
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CONE);
    RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED);

    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
  }

  /* All gizmos must perform undo. */
  LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
    WM_gizmo_set_flag(gz, WM_GIZMO_NEEDS_UNDO, true);
  }
}

static void WIDGETGROUP_camera_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  if (!gzgroup->customdata) {
    return;
  }

  CameraWidgetGroup *cagzgroup = static_cast<CameraWidgetGroup *>(gzgroup->customdata);
  View3D *v3d = CTX_wm_view3d(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Camera *ca = static_cast<Camera *>(ob->data);
  float dir[3];

  PointerRNA camera_ptr = RNA_pointer_create_discrete(&ca->id, &RNA_Camera, ca);

  const bool is_modal = WM_gizmo_group_is_modal(gzgroup);

  negate_v3_v3(dir, ob->object_to_world().ptr()[2]);

  if ((ca->flag & CAM_SHOWLIMITS) && (v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_DOF_DIST)) {
    WM_gizmo_set_matrix_location(cagzgroup->dop_dist, ob->object_to_world().location());
    WM_gizmo_set_matrix_rotation_from_yz_axis(
        cagzgroup->dop_dist, ob->object_to_world().ptr()[1], dir);
    WM_gizmo_set_scale(cagzgroup->dop_dist, ca->drawsize);
    WM_gizmo_set_flag(cagzgroup->dop_dist, WM_GIZMO_HIDDEN, false);

    /* Need to set property here for undo. TODO: would prefer to do this in _init. */
    PointerRNA camera_dof_ptr = RNA_pointer_create_discrete(
        &ca->id, &RNA_CameraDOFSettings, &ca->dof);
    WM_gizmo_target_property_def_rna(
        cagzgroup->dop_dist, "offset", &camera_dof_ptr, "focus_distance", -1);
  }
  else {
    WM_gizmo_set_flag(cagzgroup->dop_dist, WM_GIZMO_HIDDEN, true);
  }

  /* TODO: make focal length/ortho ob_scale_inv widget optional. */
  const float aspx = float(scene->r.xsch) * scene->r.xasp;
  const float aspy = float(scene->r.ysch) * scene->r.yasp;
  const bool is_ortho = (ca->type == CAM_ORTHO);
  const int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, aspx, aspy);
  /* Important to use camera value, not calculated fit since 'AUTO' uses width always. */
  const float sensor_size = BKE_camera_sensor_size(ca->sensor_fit, ca->sensor_x, ca->sensor_y);
  wmGizmo *widget = is_ortho ? cagzgroup->ortho_scale : cagzgroup->focal_len;
  float scale_matrix;
  if (true) {
    float offset[3];
    float aspect[2];

    WM_gizmo_set_flag(widget, WM_GIZMO_HIDDEN, false);
    WM_gizmo_set_flag(
        is_ortho ? cagzgroup->focal_len : cagzgroup->ortho_scale, WM_GIZMO_HIDDEN, true);

    /* account for lens shifting */
    offset[0] = ((ob->scale[0] > 0.0f) ? -2.0f : 2.0f) * ca->shiftx;
    offset[1] = 2.0f * ca->shifty;
    offset[2] = 0.0f;

    /* get aspect */
    aspect[0] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? 1.0f : aspx / aspy;
    aspect[1] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? aspy / aspx : 1.0f;

    unit_m4(widget->matrix_basis);
    WM_gizmo_set_matrix_location(widget, ob->object_to_world().location());
    WM_gizmo_set_matrix_rotation_from_yz_axis(widget, ob->object_to_world().ptr()[1], dir);

    if (is_ortho) {
      scale_matrix = ca->ortho_scale * 0.5f;
    }
    else {
      const float ob_scale_inv[3] = {
          1.0f / len_v3(ob->object_to_world().ptr()[0]),
          1.0f / len_v3(ob->object_to_world().ptr()[1]),
          1.0f / len_v3(ob->object_to_world().ptr()[2]),
      };
      const float ob_scale_uniform_inv = (ob_scale_inv[0] + ob_scale_inv[1] + ob_scale_inv[2]) /
                                         3.0f;
      scale_matrix = (ca->drawsize * 0.5f) / ob_scale_uniform_inv;
    }
    mul_v3_fl(widget->matrix_basis[0], scale_matrix);
    mul_v3_fl(widget->matrix_basis[1], scale_matrix);

    RNA_float_set_array(widget->ptr, "aspect", aspect);

    WM_gizmo_set_matrix_offset_location(widget, offset);
  }

  /* Define & update properties.
   *
   * Check modal to prevent feedback loop for orthographic cameras,
   * where the range is based on the scale, see: #141667. */
  if (!is_modal) {
    const char *propname = is_ortho ? "ortho_scale" : "lens";
    PropertyRNA *prop = RNA_struct_find_property(&camera_ptr, propname);
    const wmGizmoPropertyType *gz_prop_type = WM_gizmotype_target_property_find(widget->type,
                                                                                "offset");

    WM_gizmo_target_property_clear_rna_ptr(widget, gz_prop_type);

    float min, max, range;
    float step, precision;

    /* get property range */
    RNA_property_float_ui_range(&camera_ptr, prop, &min, &max, &step, &precision);
    range = max - min;

    ED_gizmo_arrow3d_set_range_fac(
        widget,
        is_ortho ?
            ((range / ca->ortho_scale) * ca->drawsize) :
            (scale_matrix * range /
             /* Half sensor, intentionally use sensor from camera and not calculated above. */
             (0.5f * sensor_size)));

    WM_gizmo_target_property_def_rna_ptr(widget, gz_prop_type, &camera_ptr, prop, -1);
  }

  /* This could be handled more elegantly (split into two gizmo groups). */
  if ((v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_LENS) == 0) {
    WM_gizmo_set_flag(cagzgroup->focal_len, WM_GIZMO_HIDDEN, true);
    WM_gizmo_set_flag(cagzgroup->ortho_scale, WM_GIZMO_HIDDEN, true);
  }
}

static void WIDGETGROUP_camera_message_subscribe(const bContext *C,
                                                 wmGizmoGroup *gzgroup,
                                                 wmMsgBus *mbus)
{
  ARegion *region = CTX_wm_region(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Camera *ca = static_cast<Camera *>(ob->data);

  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh{};
  msg_sub_value_gz_tag_refresh.owner = region;
  msg_sub_value_gz_tag_refresh.user_data = gzgroup->parent_gzmap;
  msg_sub_value_gz_tag_refresh.notify = WM_gizmo_do_msg_notify_tag_refresh;

  {
    const PropertyRNA *props[] = {
        &rna_CameraDOFSettings_focus_distance,
        &rna_Camera_display_size,
        &rna_Camera_ortho_scale,
        &rna_Camera_sensor_fit,
        &rna_Camera_sensor_width,
        &rna_Camera_sensor_height,
        &rna_Camera_shift_x,
        &rna_Camera_shift_y,
        &rna_Camera_type,
        &rna_Camera_lens,
    };

    PointerRNA idptr = RNA_id_pointer_create(&ca->id);

    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(mbus, &idptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
    }
  }

  /* Subscribe to render settings */
  {
    WM_msg_subscribe_rna_anon_prop(
        mbus, RenderSettings, resolution_x, &msg_sub_value_gz_tag_refresh);
    WM_msg_subscribe_rna_anon_prop(
        mbus, RenderSettings, resolution_y, &msg_sub_value_gz_tag_refresh);
    WM_msg_subscribe_rna_anon_prop(
        mbus, RenderSettings, pixel_aspect_x, &msg_sub_value_gz_tag_refresh);
    WM_msg_subscribe_rna_anon_prop(
        mbus, RenderSettings, pixel_aspect_y, &msg_sub_value_gz_tag_refresh);
  }
}

void VIEW3D_GGT_camera(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Camera Widgets";
  gzgt->idname = "VIEW3D_GGT_camera";

  gzgt->flag = (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_camera_poll;
  gzgt->setup = WIDGETGROUP_camera_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_camera_refresh;
  gzgt->message_subscribe = WIDGETGROUP_camera_message_subscribe;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CameraView Gizmos
 * \{ */

struct CameraViewWidgetGroup {
  Scene *scene;
  bool is_camera;

  wmGizmo *border;

  struct {
    rctf *edit_border;
    rctf view_border;
  } state;
};

/* scale callbacks */
static void gizmo_render_border_prop_matrix_get(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  CameraViewWidgetGroup *viewgroup = static_cast<CameraViewWidgetGroup *>(
      gz_prop->custom_func.user_data);
  const rctf *border = viewgroup->state.edit_border;

  unit_m4(matrix);
  matrix[0][0] = BLI_rctf_size_x(border);
  matrix[1][1] = BLI_rctf_size_y(border);
  matrix[3][0] = BLI_rctf_cent_x(border);
  matrix[3][1] = BLI_rctf_cent_y(border);
}

static void gizmo_render_border_prop_matrix_set(const wmGizmo * /*gz*/,
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  CameraViewWidgetGroup *viewgroup = static_cast<CameraViewWidgetGroup *>(
      gz_prop->custom_func.user_data);
  rctf *border = viewgroup->state.edit_border;
  BLI_assert(gz_prop->type->array_length == 16);

  rctf rect{};
  rect.xmin = 0;
  rect.ymin = 0;
  rect.xmax = 1;
  rect.ymax = 1;
  BLI_rctf_resize(border, len_v3(matrix[0]), len_v3(matrix[1]));
  BLI_rctf_recenter(border, matrix[3][0], matrix[3][1]);
  BLI_rctf_isect(&rect, border, border);

  if (viewgroup->is_camera) {
    DEG_id_tag_update(&viewgroup->scene->id, ID_RECALC_SYNC_TO_EVAL);
  }
}

static bool WIDGETGROUP_camera_view_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  Scene *scene = CTX_data_scene(C);

  /* This is just so the border isn't always in the way,
   * stealing mouse clicks from regular usage.
   * We could change the rules for when to show. */
  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    if (scene->camera != BKE_view_layer_active_object_get(view_layer)) {
      return false;
    }
  }

  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }

  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (rv3d->persp == RV3D_CAMOB) {
    if (scene->r.mode & R_BORDER) {
      /* TODO: support overrides. */
      if (BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
        return true;
      }
    }
  }
  else if (v3d->flag2 & V3D_RENDER_BORDER) {
    return true;
  }
  return false;
}

static void WIDGETGROUP_camera_view_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  CameraViewWidgetGroup *viewgroup = MEM_mallocN<CameraViewWidgetGroup>(__func__);

  viewgroup->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);

  RNA_enum_set(viewgroup->border->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE);
  /* Box style is more subtle in this case. */
  RNA_enum_set(viewgroup->border->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_BOX);

  WM_gizmo_set_scale(viewgroup->border, 10.0f / 0.15f);

  gzgroup->customdata = viewgroup;

  /* NOTE: #WM_GIZMO_NEEDS_UNDO is set on refresh and depends on modifying a camera object. */
}

static void WIDGETGROUP_camera_view_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  CameraViewWidgetGroup *viewgroup = static_cast<CameraViewWidgetGroup *>(gzgroup->customdata);

  ARegion *region = CTX_wm_region(C);
  /* Drawing code should happen with fully evaluated graph. */
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (rv3d->persp == RV3D_CAMOB) {
    Scene *scene = CTX_data_scene(C);
    View3D *v3d = CTX_wm_view3d(C);
    ED_view3d_calc_camera_border(
        scene, depsgraph, region, v3d, rv3d, false, &viewgroup->state.view_border);
  }
  else {
    rctf rect{};
    rect.xmin = 0;
    rect.ymin = 0;
    rect.xmax = region->winx;
    rect.ymax = region->winy;
    viewgroup->state.view_border = rect;
  }

  wmGizmo *gz = viewgroup->border;
  unit_m4(gz->matrix_space);
  mul_v3_fl(gz->matrix_space[0], BLI_rctf_size_x(&viewgroup->state.view_border));
  mul_v3_fl(gz->matrix_space[1], BLI_rctf_size_y(&viewgroup->state.view_border));
  gz->matrix_space[3][0] = viewgroup->state.view_border.xmin;
  gz->matrix_space[3][1] = viewgroup->state.view_border.ymin;
}

static void WIDGETGROUP_camera_view_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  CameraViewWidgetGroup *viewgroup = static_cast<CameraViewWidgetGroup *>(gzgroup->customdata);

  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  Scene *scene = CTX_data_scene(C);

  viewgroup->scene = scene;

  {
    wmGizmo *gz = viewgroup->border;
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

    RNA_enum_set(viewgroup->border->ptr,
                 "transform",
                 ED_GIZMO_CAGE_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE);

    if (rv3d->persp == RV3D_CAMOB) {
      viewgroup->state.edit_border = &scene->r.border;
      viewgroup->is_camera = true;
    }
    else {
      viewgroup->state.edit_border = &v3d->render_border;
      viewgroup->is_camera = false;
    }

    wmGizmoPropertyFnParams params{};
    params.value_get_fn = gizmo_render_border_prop_matrix_get;
    params.value_set_fn = gizmo_render_border_prop_matrix_set;
    params.range_get_fn = nullptr;
    params.user_data = viewgroup;
    WM_gizmo_target_property_def_func(gz, "matrix", &params);

    WM_gizmo_set_flag(gz, WM_GIZMO_NEEDS_UNDO, viewgroup->is_camera);
  }
}

void VIEW3D_GGT_camera_view(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Camera View Widgets";
  gzgt->idname = "VIEW3D_GGT_camera_view";

  gzgt->flag = (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_SCALE);

  gzgt->poll = WIDGETGROUP_camera_view_poll;
  gzgt->setup = WIDGETGROUP_camera_view_setup;
  gzgt->draw_prepare = WIDGETGROUP_camera_view_draw_prepare;
  gzgt->refresh = WIDGETGROUP_camera_view_refresh;
}

/** \} */
