/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "BLI_listbase.h"
#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "DEG_depsgraph.hh"

#include "DNA_light_types.h"
#include "DNA_object_types.h"

#include "ED_gizmo_library.hh"

#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "view3d_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Spot Light Gizmos
 * \{ */
/* NOTE: scaling from `overlay_extra.cc`. */
#define CONE_SCALE 10.0f
#define INV_CONE_SCALE 0.1f

struct LightSpotWidgetGroup {
  wmGizmo *spot_angle;
  wmGizmo *spot_blend;
  wmGizmo *spot_radius;
};

static void gizmo_spot_blend_prop_matrix_get(const wmGizmo * /*gz*/,
                                             wmGizmoProperty *gz_prop,
                                             void *value_p)
{
  BLI_assert(gz_prop->type->array_length == 16);
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);

  const bContext *C = static_cast<const bContext *>(gz_prop->custom_func.user_data);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
  Light *la = static_cast<Light *>(BKE_view_layer_active_object_get(view_layer)->data);

  float a = cosf(la->spotsize * 0.5f);
  float b = la->spotblend;
  /* Cosine of the angle where spot attenuation == 1. */
  float c = (1.0f - a) * b + a;
  /* Tangent. */
  float t = sqrtf(1.0f - c * c) / c;

  matrix[0][0] = 2.0f * CONE_SCALE * t * a;
  matrix[1][1] = 2.0f * CONE_SCALE * t * a;
}

static void gizmo_spot_blend_prop_matrix_set(const wmGizmo * /*gz*/,
                                             wmGizmoProperty *gz_prop,
                                             const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);

  const bContext *C = static_cast<const bContext *>(gz_prop->custom_func.user_data);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Light *la = static_cast<Light *>(BKE_view_layer_active_object_get(view_layer)->data);

  float a = cosf(la->spotsize * 0.5f);
  float t = matrix[0][0] * 0.5f * INV_CONE_SCALE / a;
  float c = 1.0f / sqrt(t * t + 1.0f);

  float spot_blend = safe_divide(clamp_f(c - a, 0.0f, 1.0f - a), 1.0f - a);

  PointerRNA light_ptr = RNA_pointer_create_discrete(&la->id, &RNA_Light, la);
  PropertyRNA *spot_blend_prop = RNA_struct_find_property(&light_ptr, "spot_blend");
  RNA_property_float_set(&light_ptr, spot_blend_prop, spot_blend);

  RNA_property_update_main(CTX_data_main(C), scene, &light_ptr, spot_blend_prop);
}

/* Used by spot light and point light. */
static void gizmo_light_radius_prop_matrix_get(const wmGizmo * /*gz*/,
                                               wmGizmoProperty *gz_prop,
                                               void *value_p)
{
  BLI_assert(gz_prop->type->array_length == 16);
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);

  const bContext *C = static_cast<const bContext *>(gz_prop->custom_func.user_data);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
  const Light *la = static_cast<const Light *>(BKE_view_layer_active_object_get(view_layer)->data);

  const float diameter = 2.0f * la->radius;
  matrix[0][0] = diameter;
  matrix[1][1] = diameter;
}

static void gizmo_light_radius_prop_matrix_set(const wmGizmo * /*gz*/,
                                               wmGizmoProperty *gz_prop,
                                               const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);

  const bContext *C = static_cast<const bContext *>(gz_prop->custom_func.user_data);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Light *la = static_cast<Light *>(BKE_view_layer_active_object_get(view_layer)->data);

  const float radius = 0.5f * len_v3(matrix[0]);

  PointerRNA light_ptr = RNA_pointer_create_discrete(&la->id, &RNA_Light, la);
  PropertyRNA *radius_prop = RNA_struct_find_property(&light_ptr, "shadow_soft_size");
  RNA_property_float_set(&light_ptr, radius_prop, radius);

  RNA_property_update_main(CTX_data_main(C), scene, &light_ptr, radius_prop);
}

static bool WIDGETGROUP_light_spot_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_SIZE) == 0) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_active_base_get(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    const Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      const Light *la = static_cast<Light *>(ob->data);
      if (la->type == LA_SPOT) {
        if (BKE_id_is_editable(CTX_data_main(C), &la->id)) {
          return true;
        }
      }
    }
  }
  return false;
}

static void WIDGETGROUP_light_spot_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  LightSpotWidgetGroup *ls_gzgroup = MEM_mallocN<LightSpotWidgetGroup>(__func__);

  gzgroup->customdata = ls_gzgroup;

  /* Spot angle gizmo. */
  {
    ls_gzgroup->spot_angle = WM_gizmo_new("GIZMO_GT_arrow_3d", gzgroup, nullptr);
    wmGizmo *gz = ls_gzgroup->spot_angle;
    RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_INVERTED);
    ED_gizmo_arrow3d_set_range_fac(gz, 4.0f);
    UI_GetThemeColor3fv(TH_GIZMO_SECONDARY, gz->color);
  }

  /* Spot blend gizmo. */
  {
    ls_gzgroup->spot_blend = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);
    wmGizmo *gz = ls_gzgroup->spot_blend;
    RNA_enum_set(gz->ptr,
                 "transform",
                 ED_GIZMO_CAGE_XFORM_FLAG_SCALE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_CIRCLE);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

    wmGizmoPropertyFnParams params{};
    params.value_get_fn = gizmo_spot_blend_prop_matrix_get;
    params.value_set_fn = gizmo_spot_blend_prop_matrix_set;
    params.range_get_fn = nullptr;
    params.user_data = (void *)C;
    WM_gizmo_target_property_def_func(gz, "matrix", &params);
  }

  /* Spot radius gizmo. */
  {
    ls_gzgroup->spot_radius = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);
    wmGizmo *gz = ls_gzgroup->spot_radius;
    RNA_enum_set(gz->ptr,
                 "transform",
                 ED_GIZMO_CAGE_XFORM_FLAG_SCALE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM);
    RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_CIRCLE);
    WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);
    UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
    UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

    wmGizmoPropertyFnParams params{};
    params.value_get_fn = gizmo_light_radius_prop_matrix_get;
    params.value_set_fn = gizmo_light_radius_prop_matrix_set;
    params.range_get_fn = nullptr;
    params.user_data = (void *)C;
    WM_gizmo_target_property_def_func(gz, "matrix", &params);
  }

  /* All gizmos must perform undo. */
  LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
    WM_gizmo_set_flag(gz, WM_GIZMO_NEEDS_UNDO, true);
  }
}

static void WIDGETGROUP_light_spot_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  LightSpotWidgetGroup *ls_gzgroup = static_cast<LightSpotWidgetGroup *>(gzgroup->customdata);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Light *la = static_cast<Light *>(ob->data);

  /* Spot angle gizmo. */
  {
    PointerRNA lamp_ptr = RNA_pointer_create_discrete(&la->id, &RNA_Light, la);

    wmGizmo *gz = ls_gzgroup->spot_angle;
    float dir[3];
    negate_v3_v3(dir, ob->object_to_world().ptr()[2]);
    WM_gizmo_set_matrix_rotation_from_z_axis(gz, dir);
    WM_gizmo_set_matrix_location(gz, ob->object_to_world().location());

    const char *propname = "spot_size";
    WM_gizmo_target_property_def_rna(gz, "offset", &lamp_ptr, propname, -1);
  }

  /* Spot blend gizmo. */
  {
    wmGizmo *gz = ls_gzgroup->spot_blend;

    copy_m4_m4(gz->matrix_basis, ob->object_to_world().ptr());

    /* Move center to the cone base plane. */
    float dir[3];
    negate_v3_v3(dir, ob->object_to_world().ptr()[2]);
    mul_v3_fl(dir, CONE_SCALE * cosf(0.5f * la->spotsize));
    add_v3_v3(gz->matrix_basis[3], dir);
  }
}

static void WIDGETGROUP_light_spot_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  LightSpotWidgetGroup *ls_gzgroup = static_cast<LightSpotWidgetGroup *>(gzgroup->customdata);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* Spot radius gizmo. */
  wmGizmo *gz = ls_gzgroup->spot_radius;

  /* Draw circle in the screen space. */
  RegionView3D *rv3d = static_cast<RegionView3D *>(CTX_wm_region(C)->regiondata);
  WM_gizmo_set_matrix_rotation_from_z_axis(gz, rv3d->viewinv[2]);

  WM_gizmo_set_matrix_location(gz, ob->object_to_world().location());
}

void VIEW3D_GGT_light_spot(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Spot Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_spot";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_light_spot_poll;
  gzgt->setup = WIDGETGROUP_light_spot_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_light_spot_refresh;
  gzgt->draw_prepare = WIDGETGROUP_light_spot_draw_prepare;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Light Gizmo
 * \{ */

static bool WIDGETGROUP_light_point_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  const View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_SIZE) == 0) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *base = BKE_view_layer_active_base_get(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    const Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      const Light *la = static_cast<const Light *>(ob->data);
      if (la->type == LA_LOCAL) {
        if (BKE_id_is_editable(CTX_data_main(C), &la->id)) {
          return true;
        }
      }
    }
  }
  return false;
}

static void WIDGETGROUP_light_point_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN<wmGizmoWrapper>(__func__);
  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);
  /* Point radius gizmo. */
  wmGizmo *gz = wwrapper->gizmo;
  gzgroup->customdata = wwrapper;

  RNA_enum_set(gz->ptr,
               "transform",
               ED_GIZMO_CAGE_XFORM_FLAG_SCALE | ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM);
  RNA_enum_set(gz->ptr, "draw_style", ED_GIZMO_CAGE2D_STYLE_CIRCLE);
  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_light_radius_prop_matrix_get;
  params.value_set_fn = gizmo_light_radius_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = (void *)C;
  WM_gizmo_target_property_def_func(gz, "matrix", &params);

  /* All gizmos must perform undo. */
  LISTBASE_FOREACH (wmGizmo *, gz_iter, &gzgroup->gizmos) {
    WM_gizmo_set_flag(gz_iter, WM_GIZMO_NEEDS_UNDO, true);
  }
}

static void WIDGETGROUP_light_point_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = static_cast<wmGizmoWrapper *>(gzgroup->customdata);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
  const Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* Point radius gizmo. */
  wmGizmo *gz = wwrapper->gizmo;

  /* Draw circle in the screen space. */
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(CTX_wm_region(C)->regiondata);
  WM_gizmo_set_matrix_rotation_from_z_axis(gz, rv3d->viewinv[2]);

  WM_gizmo_set_matrix_location(gz, ob->object_to_world().location());
}

void VIEW3D_GGT_light_point(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Point Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_point";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_light_point_poll;
  gzgt->setup = WIDGETGROUP_light_point_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = WIDGETGROUP_light_point_draw_prepare;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Light Gizmos
 * \{ */

/* scale callbacks */
static void gizmo_area_light_prop_matrix_get(const wmGizmo * /*gz*/,
                                             wmGizmoProperty *gz_prop,
                                             void *value_p)
{
  BLI_assert(gz_prop->type->array_length == 16);
  float (*matrix)[4] = static_cast<float (*)[4]>(value_p);
  const Light *la = static_cast<const Light *>(gz_prop->custom_func.user_data);

  matrix[0][0] = la->area_size;
  matrix[1][1] = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ? la->area_sizey :
                                                                       la->area_size;
}

static void gizmo_area_light_prop_matrix_set(const wmGizmo * /*gz*/,
                                             wmGizmoProperty *gz_prop,
                                             const void *value_p)
{
  const float (*matrix)[4] = static_cast<const float (*)[4]>(value_p);
  BLI_assert(gz_prop->type->array_length == 16);
  Light *la = static_cast<Light *>(gz_prop->custom_func.user_data);

  if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
    la->area_size = len_v3(matrix[0]);
    la->area_sizey = len_v3(matrix[1]);
  }
  else {
    la->area_size = len_v3(matrix[0]);
  }

  DEG_id_tag_update(&la->id, ID_RECALC_PARAMETERS);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING_DRAW, la);
}

static bool WIDGETGROUP_light_area_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_SIZE) == 0) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_active_base_get(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    const Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      const Light *la = static_cast<Light *>(ob->data);
      if (la->type == LA_AREA) {
        if (BKE_id_is_editable(CTX_data_main(C), &la->id)) {
          return true;
        }
      }
    }
  }
  return false;
}

static void WIDGETGROUP_light_area_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN<wmGizmoWrapper>(__func__);
  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, nullptr);
  wmGizmo *gz = wwrapper->gizmo;
  RNA_enum_set(gz->ptr, "transform", ED_GIZMO_CAGE_XFORM_FLAG_SCALE);

  gzgroup->customdata = wwrapper;

  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  /* All gizmos must perform undo. */
  LISTBASE_FOREACH (wmGizmo *, gz_iter, &gzgroup->gizmos) {
    WM_gizmo_set_flag(gz_iter, WM_GIZMO_NEEDS_UNDO, true);
  }
}

static void WIDGETGROUP_light_area_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = static_cast<wmGizmoWrapper *>(gzgroup->customdata);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Light *la = static_cast<Light *>(ob->data);
  wmGizmo *gz = wwrapper->gizmo;

  copy_m4_m4(gz->matrix_basis, ob->object_to_world().ptr());

  int flag = ED_GIZMO_CAGE_XFORM_FLAG_SCALE;
  if (ELEM(la->area_shape, LA_AREA_SQUARE, LA_AREA_DISK)) {
    flag |= ED_GIZMO_CAGE_XFORM_FLAG_SCALE_UNIFORM;
  }
  RNA_enum_set(gz->ptr, "transform", flag);

  /* need to set property here for undo. TODO: would prefer to do this in _init. */
  wmGizmoPropertyFnParams params{};
  params.value_get_fn = gizmo_area_light_prop_matrix_get;
  params.value_set_fn = gizmo_area_light_prop_matrix_set;
  params.range_get_fn = nullptr;
  params.user_data = la;
  WM_gizmo_target_property_def_func(gz, "matrix", &params);
}

void VIEW3D_GGT_light_area(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Area Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_area";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_light_area_poll;
  gzgt->setup = WIDGETGROUP_light_area_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->refresh = WIDGETGROUP_light_area_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Target Gizmo
 * \{ */

static bool WIDGETGROUP_light_target_poll(const bContext *C, wmGizmoGroupType * /*gzgt*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_LOOK_AT) == 0) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_active_base_get(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    const Object *ob = base->object;
    if (BKE_id_is_editable(CTX_data_main(C), &ob->id)) {
      if (ob->type == OB_LAMP) {
        /* No need to check the light is editable, only the object is transformed. */
        const Light *la = static_cast<Light *>(ob->data);
        if (ELEM(la->type, LA_SUN, LA_SPOT, LA_AREA)) {
          return true;
        }
      }
#if 0
      else if (ob->type == OB_CAMERA) {
        return true;
      }
#endif
    }
  }
  return false;
}

static void WIDGETGROUP_light_target_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN<wmGizmoWrapper>(__func__);
  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_move_3d", gzgroup, nullptr);
  wmGizmo *gz = wwrapper->gizmo;

  gzgroup->customdata = wwrapper;

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  gz->scale_basis = 0.06f;

  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_transform_axis_target", true);

  RNA_enum_set(
      gz->ptr, "draw_options", ED_GIZMO_MOVE_DRAW_FLAG_FILL | ED_GIZMO_MOVE_DRAW_FLAG_ALIGN_VIEW);

  WM_gizmo_operator_set(gz, 0, ot, nullptr);

  /* The operator handles undo, no need to set #WM_GIZMO_NEEDS_UNDO. */
}

static void WIDGETGROUP_light_target_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = static_cast<wmGizmoWrapper *>(gzgroup->customdata);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  wmGizmo *gz = wwrapper->gizmo;

  normalize_m4_m4(gz->matrix_basis, ob->object_to_world().ptr());
  unit_m4(gz->matrix_offset);

  if (ob->type == OB_LAMP) {
    Light *la = static_cast<Light *>(ob->data);
    if (la->type == LA_SPOT) {
      /* Draw just past the light size angle gizmo. */
      madd_v3_v3fl(gz->matrix_basis[3], gz->matrix_basis[2], -la->spotsize);
    }
  }
  gz->matrix_offset[3][2] -= 23.0;
  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_OFFSET_SCALE, true);
}

void VIEW3D_GGT_light_target(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Target Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_target";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D);

  gzgt->poll = WIDGETGROUP_light_target_poll;
  gzgt->setup = WIDGETGROUP_light_target_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_maybe_drag;
  gzgt->draw_prepare = WIDGETGROUP_light_target_draw_prepare;
}

/** \} */
