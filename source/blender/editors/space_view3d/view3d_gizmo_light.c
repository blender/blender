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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "DNA_object_types.h"
#include "DNA_light_types.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Spot Light Gizmos
 * \{ */

static bool WIDGETGROUP_light_spot_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_SIZE) == 0) {
    return false;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = BASACT(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      Light *la = ob->data;
      return (la->type == LA_SPOT);
    }
  }
  return false;
}

static void WIDGETGROUP_light_spot_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);

  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_arrow_3d", gzgroup, NULL);
  wmGizmo *gz = wwrapper->gizmo;
  RNA_enum_set(gz->ptr, "transform", ED_GIZMO_ARROW_XFORM_FLAG_INVERTED);

  gzgroup->customdata = wwrapper;

  ED_gizmo_arrow3d_set_range_fac(gz, 4.0f);

  UI_GetThemeColor3fv(TH_GIZMO_SECONDARY, gz->color);
}

static void WIDGETGROUP_light_spot_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = gzgroup->customdata;
  wmGizmo *gz = wwrapper->gizmo;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Light *la = ob->data;
  float dir[3];

  negate_v3_v3(dir, ob->obmat[2]);

  WM_gizmo_set_matrix_rotation_from_z_axis(gz, dir);
  WM_gizmo_set_matrix_location(gz, ob->obmat[3]);

  /* need to set property here for undo. TODO would prefer to do this in _init */
  PointerRNA lamp_ptr;
  const char *propname = "spot_size";
  RNA_pointer_create(&la->id, &RNA_Light, la, &lamp_ptr);
  WM_gizmo_target_property_def_rna(gz, "offset", &lamp_ptr, propname, -1);
}

void VIEW3D_GGT_light_spot(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Spot Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_spot";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_light_spot_poll;
  gzgt->setup = WIDGETGROUP_light_spot_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_drag;
  gzgt->refresh = WIDGETGROUP_light_spot_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Light Gizmos
 * \{ */

/* scale callbacks */
static void gizmo_area_light_prop_matrix_get(const wmGizmo *UNUSED(gz),
                                             wmGizmoProperty *gz_prop,
                                             void *value_p)
{
  BLI_assert(gz_prop->type->array_length == 16);
  float(*matrix)[4] = value_p;
  const Light *la = gz_prop->custom_func.user_data;

  matrix[0][0] = la->area_size;
  matrix[1][1] = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ? la->area_sizey :
                                                                       la->area_size;
}

static void gizmo_area_light_prop_matrix_set(const wmGizmo *UNUSED(gz),
                                             wmGizmoProperty *gz_prop,
                                             const void *value_p)
{
  const float(*matrix)[4] = value_p;
  BLI_assert(gz_prop->type->array_length == 16);
  Light *la = gz_prop->custom_func.user_data;

  if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
    la->area_size = len_v3(matrix[0]);
    la->area_sizey = len_v3(matrix[1]);
  }
  else {
    la->area_size = len_v3(matrix[0]);
  }

  DEG_id_tag_update(&la->id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_LAMP | ND_LIGHTING_DRAW, la);
}

static bool WIDGETGROUP_light_area_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_SIZE) == 0) {
    return false;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = BASACT(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      Light *la = ob->data;
      return (la->type == LA_AREA);
    }
  }
  return false;
}

static void WIDGETGROUP_light_area_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);
  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, NULL);
  wmGizmo *gz = wwrapper->gizmo;
  RNA_enum_set(gz->ptr, "transform", ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE);

  gzgroup->customdata = wwrapper;

  WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

static void WIDGETGROUP_light_area_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = gzgroup->customdata;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Light *la = ob->data;
  wmGizmo *gz = wwrapper->gizmo;

  copy_m4_m4(gz->matrix_basis, ob->obmat);

  int flag = ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE;
  if (ELEM(la->area_shape, LA_AREA_SQUARE, LA_AREA_DISK)) {
    flag |= ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM;
  }
  RNA_enum_set(gz->ptr, "transform", flag);

  /* need to set property here for undo. TODO would prefer to do this in _init */
  WM_gizmo_target_property_def_func(gz,
                                    "matrix",
                                    &(const struct wmGizmoPropertyFnParams){
                                        .value_get_fn = gizmo_area_light_prop_matrix_get,
                                        .value_set_fn = gizmo_area_light_prop_matrix_set,
                                        .range_get_fn = NULL,
                                        .user_data = la,
                                    });
}

void VIEW3D_GGT_light_area(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Area Light Widgets";
  gzgt->idname = "VIEW3D_GGT_light_area";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT | WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_DEPTH_3D);

  gzgt->poll = WIDGETGROUP_light_area_poll;
  gzgt->setup = WIDGETGROUP_light_area_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_drag;
  gzgt->refresh = WIDGETGROUP_light_area_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Target Gizmo
 * \{ */

static bool WIDGETGROUP_light_target_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)) {
    return false;
  }
  if ((v3d->gizmo_show_light & V3D_GIZMO_SHOW_LIGHT_LOOK_AT) == 0) {
    return false;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *base = BASACT(view_layer);
  if (base && BASE_SELECTABLE(v3d, base)) {
    Object *ob = base->object;
    if (ob->type == OB_LAMP) {
      Light *la = ob->data;
      return (ELEM(la->type, LA_SUN, LA_SPOT, LA_AREA));
    }
#if 0
    else if (ob->type == OB_CAMERA) {
      return true;
    }
#endif
  }
  return false;
}

static void WIDGETGROUP_light_target_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);
  wwrapper->gizmo = WM_gizmo_new("GIZMO_GT_move_3d", gzgroup, NULL);
  wmGizmo *gz = wwrapper->gizmo;

  gzgroup->customdata = wwrapper;

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  gz->scale_basis = 0.06f;

  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_transform_axis_target", true);

  RNA_enum_set(
      gz->ptr, "draw_options", ED_GIZMO_MOVE_DRAW_FLAG_FILL | ED_GIZMO_MOVE_DRAW_FLAG_ALIGN_VIEW);

  WM_gizmo_operator_set(gz, 0, ot, NULL);
}

static void WIDGETGROUP_light_target_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
  wmGizmoWrapper *wwrapper = gzgroup->customdata;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  wmGizmo *gz = wwrapper->gizmo;

  normalize_m4_m4(gz->matrix_basis, ob->obmat);
  unit_m4(gz->matrix_offset);

  if (ob->type == OB_LAMP) {
    Light *la = ob->data;
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
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_drag;
  gzgt->draw_prepare = WIDGETGROUP_light_target_draw_prepare;
}

/** \} */
