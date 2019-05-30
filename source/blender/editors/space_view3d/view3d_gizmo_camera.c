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
 * \ingroup spview3d
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"

#include "DNA_object_types.h"
#include "DNA_camera_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "DEG_depsgraph.h"

#include "view3d_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Camera Gizmos
 * \{ */

struct CameraWidgetGroup {
  wmGizmo *dop_dist;
  wmGizmo *focal_len;
  wmGizmo *ortho_scale;
};

static bool WIDGETGROUP_camera_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_LENS) == 0 &&
      (v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_DOF_DIST) == 0) {
    return false;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = BASACT(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    Object *ob = base->object;
    if (ob->type == OB_CAMERA) {
      Camera *camera = ob->data;
      /* TODO: support overrides. */
      if (camera->id.lib == NULL) {
        return true;
      }
    }
  }
  return false;
}

static void WIDGETGROUP_camera_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  float dir[3];

  const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);

  struct CameraWidgetGroup *cagzgroup = MEM_callocN(sizeof(struct CameraWidgetGroup), __func__);
  gzgroup->customdata = cagzgroup;

  negate_v3_v3(dir, ob->obmat[2]);

  /* dof distance */
  {
    wmGizmo *gz;
    gz = cagzgroup->dop_dist = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CROSS);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);

    UI_GetThemeColor3fv(TH_GIZMO_A, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
  }

  /* focal length
   * - logic/calculations are similar to BKE_camera_view_frame_ex, better keep in sync */
  {
    wmGizmo *gz;
    gz = cagzgroup->focal_len = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
    gz->flag |= WM_GIZMO_DRAW_NO_SCALE;
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CONE);
    RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED);

    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

    gz = cagzgroup->ortho_scale = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
    gz->flag |= WM_GIZMO_DRAW_NO_SCALE;
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_CONE);
    RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_CONSTRAINED);

    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
  }
}

static void WIDGETGROUP_camera_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  if (!gzgroup->customdata) {
    return;
  }

  struct CameraWidgetGroup *cagzgroup = gzgroup->customdata;
  View3D *v3d = CTX_wm_view3d(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Camera *ca = ob->data;
  PointerRNA camera_ptr;
  float dir[3];

  RNA_pointer_create(&ca->id, &RNA_Camera, ca, &camera_ptr);

  negate_v3_v3(dir, ob->obmat[2]);

  if ((ca->flag & CAM_SHOWLIMITS) && (v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_DOF_DIST)) {
    WM_gizmo_set_matrix_location(cagzgroup->dop_dist, ob->obmat[3]);
    WM_gizmo_set_matrix_rotation_from_yz_axis(cagzgroup->dop_dist, ob->obmat[1], dir);
    WM_gizmo_set_scale(cagzgroup->dop_dist, ca->drawsize);
    WM_gizmo_set_flag(cagzgroup->dop_dist, WM_GIZMO_HIDDEN, false);

    /* need to set property here for undo. TODO would prefer to do this in _init */
    WM_gizmo_target_property_def_rna(
        cagzgroup->dop_dist, "offset", &camera_ptr, "dof.focus_distance", -1);
  }
  else {
    WM_gizmo_set_flag(cagzgroup->dop_dist, WM_GIZMO_HIDDEN, true);
  }

  /* TODO - make focal length/ortho ob_scale_inv widget optional */
  const Scene *scene = CTX_data_scene(C);
  const float aspx = (float)scene->r.xsch * scene->r.xasp;
  const float aspy = (float)scene->r.ysch * scene->r.yasp;
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
    WM_gizmo_set_matrix_location(widget, ob->obmat[3]);
    WM_gizmo_set_matrix_rotation_from_yz_axis(widget, ob->obmat[1], dir);

    if (is_ortho) {
      scale_matrix = ca->ortho_scale * 0.5f;
    }
    else {
      const float ob_scale_inv[3] = {
          1.0f / len_v3(ob->obmat[0]),
          1.0f / len_v3(ob->obmat[1]),
          1.0f / len_v3(ob->obmat[2]),
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

  /* define & update properties */
  {
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

  /* This could be handled more elegently (split into two gizmo groups). */
  if ((v3d->gizmo_show_camera & V3D_GIZMO_SHOW_CAMERA_LENS) == 0) {
    WM_gizmo_set_flag(cagzgroup->focal_len, WM_GIZMO_HIDDEN, true);
    WM_gizmo_set_flag(cagzgroup->ortho_scale, WM_GIZMO_HIDDEN, true);
  }
}

static void WIDGETGROUP_camera_message_subscribe(const bContext *C,
                                                 wmGizmoGroup *gzgroup,
                                                 struct wmMsgBus *mbus)
{
  ARegion *ar = CTX_wm_region(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Camera *ca = ob->data;

  wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
      .owner = ar,
      .user_data = gzgroup->parent_gzmap,
      .notify = WM_gizmo_do_msg_notify_tag_refresh,
  };

  {
    extern PropertyRNA rna_CameraDOFSettings_focus_distance;
    extern PropertyRNA rna_Camera_display_size;
    extern PropertyRNA rna_Camera_ortho_scale;
    extern PropertyRNA rna_Camera_sensor_fit;
    extern PropertyRNA rna_Camera_sensor_width;
    extern PropertyRNA rna_Camera_sensor_height;
    extern PropertyRNA rna_Camera_shift_x;
    extern PropertyRNA rna_Camera_shift_y;
    extern PropertyRNA rna_Camera_type;
    extern PropertyRNA rna_Camera_lens;
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

    PointerRNA idptr;
    RNA_id_pointer_create(&ca->id, &idptr);

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
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_drag;
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
static void gizmo_render_border_prop_matrix_get(const wmGizmo *UNUSED(gz),
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float(*matrix)[4] = value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  struct CameraViewWidgetGroup *viewgroup = gz_prop->custom_func.user_data;
  const rctf *border = viewgroup->state.edit_border;

  unit_m4(matrix);
  matrix[0][0] = BLI_rctf_size_x(border);
  matrix[1][1] = BLI_rctf_size_y(border);
  matrix[3][0] = BLI_rctf_cent_x(border);
  matrix[3][1] = BLI_rctf_cent_y(border);
}

static void gizmo_render_border_prop_matrix_set(const wmGizmo *UNUSED(gz),
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float(*matrix)[4] = value_p;
  struct CameraViewWidgetGroup *viewgroup = gz_prop->custom_func.user_data;
  rctf *border = viewgroup->state.edit_border;
  BLI_assert(gz_prop->type->array_length == 16);

  BLI_rctf_resize(border, len_v3(matrix[0]), len_v3(matrix[1]));
  BLI_rctf_recenter(border, matrix[3][0], matrix[3][1]);
  BLI_rctf_isect(
      &(rctf){
          .xmin = 0,
          .ymin = 0,
          .xmax = 1,
          .ymax = 1,
      },
      border,
      border);

  if (viewgroup->is_camera) {
    DEG_id_tag_update(&viewgroup->scene->id, ID_RECALC_COPY_ON_WRITE);
  }
}

static bool WIDGETGROUP_camera_view_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
  Scene *scene = CTX_data_scene(C);

  /* This is just so the border isn't always in the way,
   * stealing mouse clicks from regular usage.
   * We could change the rules for when to show. */
  {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    if (scene->camera != OBACT(view_layer)) {
      return false;
    }
  }

  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }

  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ar->regiondata;
  if (rv3d->persp == RV3D_CAMOB) {
    if (scene->r.mode & R_BORDER) {
      /* TODO: support overrides. */
      if (scene->id.lib == NULL) {
        return true;
      }
    }
  }
  else if (v3d->flag2 & V3D_RENDER_BORDER) {
    return true;
  }
  return false;
}

static void WIDGETGROUP_camera_view_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  struct CameraViewWidgetGroup *viewgroup = MEM_mallocN(sizeof(struct CameraViewWidgetGroup),
                                                        __func__);

  viewgroup->border = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, NULL);

  RNA_enum_set(viewgroup->border->ptr,
               "transform",
               ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE);
  /* Box style is more subtle in this case. */
  RNA_enum_set(viewgroup->border->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_BOX);

  gzgroup->customdata = viewgroup;
}

static void WIDGETGROUP_camera_view_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  struct CameraViewWidgetGroup *viewgroup = gzgroup->customdata;

  ARegion *ar = CTX_wm_region(C);
  struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
  RegionView3D *rv3d = ar->regiondata;
  if (rv3d->persp == RV3D_CAMOB) {
    Scene *scene = CTX_data_scene(C);
    View3D *v3d = CTX_wm_view3d(C);
    ED_view3d_calc_camera_border(
        scene, depsgraph, ar, v3d, rv3d, &viewgroup->state.view_border, false);
  }
  else {
    viewgroup->state.view_border = (rctf){
        .xmin = 0,
        .ymin = 0,
        .xmax = ar->winx,
        .ymax = ar->winy,
    };
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
  struct CameraViewWidgetGroup *viewgroup = gzgroup->customdata;

  View3D *v3d = CTX_wm_view3d(C);
  ARegion *ar = CTX_wm_region(C);
  RegionView3D *rv3d = ar->regiondata;
  Scene *scene = CTX_data_scene(C);

  viewgroup->scene = scene;

  {
    wmGizmo *gz = viewgroup->border;
    WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

    RNA_enum_set(viewgroup->border->ptr,
                 "transform",
                 ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE | ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE);

    if (rv3d->persp == RV3D_CAMOB) {
      viewgroup->state.edit_border = &scene->r.border;
      viewgroup->is_camera = true;
    }
    else {
      viewgroup->state.edit_border = &v3d->render_border;
      viewgroup->is_camera = false;
    }

    WM_gizmo_target_property_def_func(gz,
                                      "matrix",
                                      &(const struct wmGizmoPropertyFnParams){
                                          .value_get_fn = gizmo_render_border_prop_matrix_get,
                                          .value_set_fn = gizmo_render_border_prop_matrix_set,
                                          .range_get_fn = NULL,
                                          .user_data = viewgroup,
                                      });
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
