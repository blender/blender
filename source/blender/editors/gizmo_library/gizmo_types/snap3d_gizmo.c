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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edgizmolib
 *
 * \name Snap Gizmo
 *
 * 3D Gizmo
 *
 * \brief Snap gizmo which exposes the location, normal and index in the props.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "ED_gizmo_library.h"
#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"

/* own includes */
#include "../gizmo_library_intern.h"

typedef struct SnapGizmo3D {
  wmGizmo gizmo;
  V3DSnapCursorState *snap_state;
} SnapGizmo3D;

static void snap_gizmo_snap_elements_update(SnapGizmo3D *snap_gizmo)
{
  wmGizmoProperty *gz_prop_snap;
  gz_prop_snap = WM_gizmo_target_property_find(&snap_gizmo->gizmo, "snap_elements");

  if (gz_prop_snap->prop) {
    V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
    snap_state->snap_elem_force |= RNA_property_enum_get(&gz_prop_snap->ptr, gz_prop_snap->prop);
  }
}

/* -------------------------------------------------------------------- */
/** \name ED_gizmo_library specific API
 * \{ */

SnapObjectContext *ED_gizmotypes_snap_3d_context_ensure(Scene *scene, wmGizmo *UNUSED(gz))
{
  return ED_view3d_cursor_snap_context_ensure(scene);
}

void ED_gizmotypes_snap_3d_flag_set(struct wmGizmo *UNUSED(gz), int flag)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->flag |= flag;
}

void ED_gizmotypes_snap_3d_flag_clear(struct wmGizmo *UNUSED(gz), int flag)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->flag &= ~flag;
}

bool ED_gizmotypes_snap_3d_flag_test(struct wmGizmo *UNUSED(gz), int flag)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return (snap_state->flag & flag) != 0;
}

bool ED_gizmotypes_snap_3d_invert_snap_get(struct wmGizmo *UNUSED(gz))
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  return snap_data->is_snap_invert;
}

bool ED_gizmotypes_snap_3d_is_enabled(const wmGizmo *UNUSED(gz))
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  return snap_data->is_enabled;
}

void ED_gizmotypes_snap_3d_data_get(const struct bContext *C,
                                    wmGizmo *UNUSED(gz),
                                    float r_loc[3],
                                    float r_nor[3],
                                    int r_elem_index[3],
                                    int *r_snap_elem)
{
  V3DSnapCursorData *snap_data = NULL;
  if (C) {
    /* Snap values are updated too late at the cursor. Be sure to update ahead of time. */
    wmWindowManager *wm = CTX_wm_manager(C);
    const wmEvent *event = wm->winactive ? wm->winactive->eventstate : NULL;
    if (event) {
      ARegion *region = CTX_wm_region(C);
      int x = event->xy[0] - region->winrct.xmin;
      int y = event->xy[1] - region->winrct.ymin;
      snap_data = ED_view3d_cursor_snap_data_get(NULL, C, x, y);
    }
  }
  if (!snap_data) {
    snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  }

  if (r_loc) {
    copy_v3_v3(r_loc, snap_data->loc);
  }
  if (r_nor) {
    copy_v3_v3(r_nor, snap_data->nor);
  }
  if (r_elem_index) {
    copy_v3_v3_int(r_elem_index, snap_data->elem_index);
  }
  if (r_snap_elem) {
    *r_snap_elem = snap_data->snap_elem;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA callbacks
 * \{ */

static int gizmo_snap_rna_snap_elements_force_get_fn(struct PointerRNA *UNUSED(ptr),
                                                     struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return snap_state->snap_elem_force;
}

static void gizmo_snap_rna_snap_elements_force_set_fn(struct PointerRNA *UNUSED(ptr),
                                                      struct PropertyRNA *UNUSED(prop),
                                                      int value)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->snap_elem_force = (short)value;
}

static void gizmo_snap_rna_prevpoint_get_fn(struct PointerRNA *UNUSED(ptr),
                                            struct PropertyRNA *UNUSED(prop),
                                            float *values)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  if (snap_state->prevpoint) {
    copy_v3_v3(values, snap_state->prevpoint);
  }
}

static void gizmo_snap_rna_prevpoint_set_fn(struct PointerRNA *UNUSED(ptr),
                                            struct PropertyRNA *UNUSED(prop),
                                            const float *values)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  ED_view3d_cursor_snap_prevpoint_set(snap_state, values);
}

static void gizmo_snap_rna_location_get_fn(struct PointerRNA *UNUSED(ptr),
                                           struct PropertyRNA *UNUSED(prop),
                                           float *values)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  copy_v3_v3(values, snap_data->loc);
}

static void gizmo_snap_rna_location_set_fn(struct PointerRNA *UNUSED(ptr),
                                           struct PropertyRNA *UNUSED(prop),
                                           const float *values)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  copy_v3_v3(snap_data->loc, values);
}

static void gizmo_snap_rna_normal_get_fn(struct PointerRNA *UNUSED(ptr),
                                         struct PropertyRNA *UNUSED(prop),
                                         float *values)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  copy_v3_v3(values, snap_data->nor);
}

static void gizmo_snap_rna_snap_elem_index_get_fn(struct PointerRNA *UNUSED(ptr),
                                                  struct PropertyRNA *UNUSED(prop),
                                                  int *values)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
  copy_v3_v3_int(values, snap_data->elem_index);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GIZMO_GT_snap_3d
 * \{ */

static void snap_gizmo_setup(wmGizmo *gz)
{
  gz->flag |= WM_GIZMO_NO_TOOLTIP;
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  snap_gizmo->snap_state = ED_view3d_cursor_snap_active();
  if (snap_gizmo->snap_state) {
    snap_gizmo->snap_state->draw_point = true;
    snap_gizmo->snap_state->draw_plane = false;
  }

  rgba_float_to_uchar(snap_gizmo->snap_state->color_point, gz->color);
}

static void snap_gizmo_draw(const bContext *UNUSED(C), wmGizmo *UNUSED(gz))
{
  /* All drawing is handled at the paint cursor. */
}

static int snap_gizmo_test_select(bContext *C, wmGizmo *gz, const int mval[2])
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  ARegion *region = CTX_wm_region(C);

  /* Make sure the cursor is only drawn in the gizmo region. */
  snap_gizmo->snap_state->region = region;

  /* Snap Elements can change while the gizmo is active. Need to be updated somewhere. */
  snap_gizmo_snap_elements_update(snap_gizmo);

  /* Snap values are updated too late at the cursor. Be sure to update ahead of time. */
  int x, y;
  {
    wmWindowManager *wm = CTX_wm_manager(C);
    const wmEvent *event = wm->winactive ? wm->winactive->eventstate : NULL;
    if (event) {
      x = event->xy[0] - region->winrct.xmin;
      y = event->xy[1] - region->winrct.ymin;
    }
    else {
      x = mval[0];
      y = mval[1];
    }
  }
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(snap_gizmo->snap_state, C, x, y);

  if (snap_data->snap_elem) {
    return 0;
  }
  return -1;
}

static int snap_gizmo_modal(bContext *UNUSED(C),
                            wmGizmo *UNUSED(gz),
                            const wmEvent *UNUSED(event),
                            eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
  return OPERATOR_RUNNING_MODAL;
}

static int snap_gizmo_invoke(bContext *UNUSED(C),
                             wmGizmo *UNUSED(gz),
                             const wmEvent *UNUSED(event))
{
  return OPERATOR_RUNNING_MODAL;
}

static void snap_gizmo_free(wmGizmo *gz)
{
  SnapGizmo3D *snap_gizmo = (SnapGizmo3D *)gz;
  if (snap_gizmo->snap_state) {
    ED_view3d_cursor_snap_deactive(snap_gizmo->snap_state);
  }
}

static void GIZMO_GT_snap_3d(wmGizmoType *gzt)
{
  /* identifiers */
  gzt->idname = "GIZMO_GT_snap_3d";

  /* api callbacks */
  gzt->setup = snap_gizmo_setup;
  gzt->draw = snap_gizmo_draw;
  gzt->test_select = snap_gizmo_test_select;
  gzt->modal = snap_gizmo_modal;
  gzt->invoke = snap_gizmo_invoke;
  gzt->free = snap_gizmo_free;

  gzt->struct_size = sizeof(SnapGizmo3D);

  const EnumPropertyItem *rna_enum_snap_element_items;
  {
    /* Get Snap Element Items enum. */
    bool free;
    PointerRNA toolsettings_ptr;
    RNA_pointer_create(NULL, &RNA_ToolSettings, NULL, &toolsettings_ptr);
    PropertyRNA *prop = RNA_struct_find_property(&toolsettings_ptr, "snap_elements");
    RNA_property_enum_items(
        NULL, &toolsettings_ptr, prop, &rna_enum_snap_element_items, NULL, &free);

    BLI_assert(free == false);
  }

  /* Setup. */
  PropertyRNA *prop;
  prop = RNA_def_enum_flag(gzt->srna,
                           "snap_elements_force",
                           rna_enum_snap_element_items,
                           SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE,
                           "Snap Elements",
                           "");
  RNA_def_property_enum_funcs_runtime(prop,
                                      gizmo_snap_rna_snap_elements_force_get_fn,
                                      gizmo_snap_rna_snap_elements_force_set_fn,
                                      NULL);

  prop = RNA_def_float_array(gzt->srna,
                             "prev_point",
                             3,
                             NULL,
                             FLT_MIN,
                             FLT_MAX,
                             "Previous Point",
                             "Point that defines the location of the perpendicular snap",
                             FLT_MIN,
                             FLT_MAX);
  RNA_def_property_float_array_funcs_runtime(
      prop, gizmo_snap_rna_prevpoint_get_fn, gizmo_snap_rna_prevpoint_set_fn, NULL);

  /* Returns. */
  prop = RNA_def_float_translation(gzt->srna,
                                   "location",
                                   3,
                                   NULL,
                                   FLT_MIN,
                                   FLT_MAX,
                                   "Location",
                                   "Snap Point Location",
                                   FLT_MIN,
                                   FLT_MAX);
  RNA_def_property_float_array_funcs_runtime(
      prop, gizmo_snap_rna_location_get_fn, gizmo_snap_rna_location_set_fn, NULL);

  prop = RNA_def_float_vector_xyz(gzt->srna,
                                  "normal",
                                  3,
                                  NULL,
                                  FLT_MIN,
                                  FLT_MAX,
                                  "Normal",
                                  "Snap Point Normal",
                                  FLT_MIN,
                                  FLT_MAX);
  RNA_def_property_float_array_funcs_runtime(prop, gizmo_snap_rna_normal_get_fn, NULL, NULL);

  prop = RNA_def_int_vector(gzt->srna,
                            "snap_elem_index",
                            3,
                            NULL,
                            INT_MIN,
                            INT_MAX,
                            "Snap Element",
                            "Array index of face, edge and vert snapped",
                            INT_MIN,
                            INT_MAX);
  RNA_def_property_int_array_funcs_runtime(
      prop, gizmo_snap_rna_snap_elem_index_get_fn, NULL, NULL);

  /* Read/Write. */
  WM_gizmotype_target_property_def(gzt, "snap_elements", PROP_ENUM, 1);
}

void ED_gizmotypes_snap_3d(void)
{
  WM_gizmotype_append(GIZMO_GT_snap_3d);
}

/** \} */
