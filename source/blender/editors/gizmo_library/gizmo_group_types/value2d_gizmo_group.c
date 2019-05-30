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
 * \ingroup edgizmolib
 *
 * \name 2D Value Gizmo
 *
 * \brief Gizmo that edits a value for operator redo.
 */

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_undo.h"
#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* -------------------------------------------------------------------- */
/** \name Value Gizmo
 * \{ */

struct ValueOpRedoGroup {
  wmGizmo *gizmo;
  struct {
    const bContext *context; /* needed for redo. */
    wmOperator *op;
  } state;
};

static void gizmo_op_redo_exec(struct ValueOpRedoGroup *igzgroup)
{
  wmOperator *op = igzgroup->state.op;
  if (op == WM_operator_last_redo((bContext *)igzgroup->state.context)) {
    ED_undo_operator_repeat((bContext *)igzgroup->state.context, op);
  }
}

/* translate callbacks */
static void gizmo_value_operator_redo_value_get(const wmGizmo *gz,
                                                wmGizmoProperty *gz_prop,
                                                void *value_p)
{
  float *value = value_p;
  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  struct ValueOpRedoGroup *igzgroup = gz->parent_gzgroup->customdata;
  wmOperator *op = igzgroup->state.op;
  *value = RNA_property_float_get(op->ptr, op->type->prop);
}

static void gizmo_value_operator_redo_value_set(const wmGizmo *gz,
                                                wmGizmoProperty *gz_prop,
                                                const void *value_p)
{
  const float *value = value_p;
  BLI_assert(gz_prop->type->array_length == 1);
  UNUSED_VARS_NDEBUG(gz_prop);

  struct ValueOpRedoGroup *igzgroup = gz->parent_gzgroup->customdata;
  wmOperator *op = igzgroup->state.op;
  RNA_property_float_set(op->ptr, op->type->prop, *value);
  gizmo_op_redo_exec(igzgroup);
}

static void WIDGETGROUP_value_operator_redo_modal_from_setup(const bContext *C,
                                                             wmGizmoGroup *gzgroup)
{
  /* Start off dragging. */
  wmWindow *win = CTX_wm_window(C);
  wmGizmo *gz = gzgroup->gizmos.first;
  wmGizmoMap *gzmap = gzgroup->parent_gzmap;
  WM_gizmo_modal_set_from_setup(gzmap, (bContext *)C, gz, 0, win->eventstate);
}

static void WIDGETGROUP_value_operator_redo_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
  struct ValueOpRedoGroup *igzgroup = MEM_mallocN(sizeof(struct ValueOpRedoGroup), __func__);

  igzgroup->gizmo = WM_gizmo_new("GIZMO_GT_value_2d", gzgroup, NULL);
  wmGizmo *gz = igzgroup->gizmo;

  igzgroup->state.context = C;
  igzgroup->state.op = WM_operator_last_redo(C);

  gzgroup->customdata = igzgroup;

  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
  UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);

  WM_gizmo_target_property_def_func(gz,
                                    "offset",
                                    &(const struct wmGizmoPropertyFnParams){
                                        .value_get_fn = gizmo_value_operator_redo_value_get,
                                        .value_set_fn = gizmo_value_operator_redo_value_set,
                                        .range_get_fn = NULL,
                                        .user_data = igzgroup,
                                    });

  /* Become modal as soon as it's started. */
  WIDGETGROUP_value_operator_redo_modal_from_setup(C, gzgroup);
}

static void WIDGETGROUP_value_operator_redo_refresh(const bContext *UNUSED(C),
                                                    wmGizmoGroup *gzgroup)
{
  struct ValueOpRedoGroup *igzgroup = gzgroup->customdata;
  wmGizmo *gz = igzgroup->gizmo;
  wmOperator *op = WM_operator_last_redo((bContext *)igzgroup->state.context);
  wmGizmoMap *gzmap = gzgroup->parent_gzmap;

  /* FIXME */
  extern struct wmGizmo *wm_gizmomap_modal_get(struct wmGizmoMap * gzmap);
  if ((op != igzgroup->state.op) || (wm_gizmomap_modal_get(gzmap) != gz)) {
    WM_gizmo_group_type_unlink_delayed_ptr(gzgroup->type);
  }
}

static void WM_GGT_value_operator_redo(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Value Operator Redo";
  gzgt->idname = "WM_GGT_value_operator_redo";

  /* FIXME, allow multiple. */
  gzgt->flag = WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_TOOL_INIT;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->setup = WIDGETGROUP_value_operator_redo_setup;
  gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_drag;
  gzgt->refresh = WIDGETGROUP_value_operator_redo_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void ED_gizmogrouptypes_value_2d(void)
{
  WM_gizmogrouptype_append(WM_GGT_value_operator_redo);
}

/** \} */
