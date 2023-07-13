/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_windowmanager_types.h"

#include "BKE_context.h"

#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_gizmo.hh"
#include "transform_snap.hh"

#include "transform_mode.hh"

#define RESET_TRANSFORMATION
#define REMOVE_GIZMO

/* -------------------------------------------------------------------- */
/** \name Transform Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct SnapSouceCustomData {
  TransModeInfo *mode_info_prev;
  void *customdata_mode_prev;

  eSnapTargetOP target_operation_prev;
  eSnapMode snap_mode_confirm;

  struct {
    void (*apply)(TransInfo *t, MouseInput *mi, const double mval[2], float output[3]);
    void (*post)(TransInfo *t, float values[3]);
    bool use_virtual_mval;
  } mouse_prev;
};

static void snapsource_end(TransInfo *t)
{
  t->modifiers &= ~MOD_EDIT_SNAP_SOURCE;

  /* Restore. */
  SnapSouceCustomData *customdata = static_cast<SnapSouceCustomData *>(t->custom.mode.data);
  t->mode_info = customdata->mode_info_prev;
  t->custom.mode.data = customdata->customdata_mode_prev;

  t->tsnap.target_operation = customdata->target_operation_prev;

  t->mouse.apply = customdata->mouse_prev.apply;
  t->mouse.post = customdata->mouse_prev.post;
  t->mouse.use_virtual_mval = customdata->mouse_prev.use_virtual_mval;

  MEM_freeN(customdata);

  transform_gizmo_3d_model_from_constraint_and_mode_set(t);
  tranform_snap_source_restore_context(t);
}

static void snapsource_confirm(TransInfo *t)
{
  BLI_assert(t->modifiers & MOD_EDIT_SNAP_SOURCE);
  getSnapPoint(t, t->tsnap.snap_source);
  t->tsnap.snap_source_fn = nullptr;
  t->tsnap.status |= SNAP_SOURCE_FOUND;

  SnapSouceCustomData *customdata = static_cast<SnapSouceCustomData *>(t->custom.mode.data);
  t->tsnap.mode = customdata->snap_mode_confirm;

  int mval[2];
#ifndef RESET_TRANSFORMATION
  if (true) {
    if (t->transform_matrix) {
      float mat_inv[4][4];
      unit_m4(mat_inv);
      t->transform_matrix(t, mat_inv);
      invert_m4(mat_inv);
      mul_m4_v3(mat_inv, t->tsnap.snap_source);
    }
    else {
      float mat_inv[3][3];
      invert_m3_m3(mat_inv, t->mat);

      mul_m3_v3(mat_inv, t->tsnap.snap_source);
      sub_v3_v3(t->tsnap.snap_source, t->vec);
    }

    projectIntView(t, t->tsnap.snap_source, mval);
  }
  else
#endif
  {
    copy_v2_v2_int(mval, t->mval);
  }

  snapsource_end(t);
  transform_input_reset(t, mval);

  /* Remote individual snap projection since this mode does not use the new `snap_source`. */
  t->tsnap.mode &= ~(SCE_SNAP_INDIVIDUAL_PROJECT | SCE_SNAP_INDIVIDUAL_NEAREST);
}

static eRedrawFlag snapsource_handle_event_fn(TransInfo *t, const wmEvent *event)
{
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case TFM_MODAL_CONFIRM:
      case TFM_MODAL_EDIT_SNAP_SOURCE_ON:
      case TFM_MODAL_EDIT_SNAP_SOURCE_OFF:
        if (t->modifiers & MOD_EDIT_SNAP_SOURCE) {
          snapsource_confirm(t);

          BLI_assert(t->state != TRANS_CONFIRM);
        }
        else {
          t->modifiers |= MOD_EDIT_SNAP_SOURCE;
        }
        break;
      case TFM_MODAL_CANCEL:
        snapsource_end(t);
        t->state = TRANS_CANCEL;
        return TREDRAW_SOFT;
      default:
        break;
    }
  }
  else if (event->val == KM_RELEASE && t->state == TRANS_CONFIRM) {
    if (t->flag & T_RELEASE_CONFIRM && t->modifiers & MOD_EDIT_SNAP_SOURCE) {
      snapsource_confirm(t);
      t->flag &= ~T_RELEASE_CONFIRM;
      t->state = TRANS_RUNNING;
    }
  }
  return TREDRAW_NOTHING;
}

static void snapsource_transform_fn(TransInfo *t, const int[2] /*mval*/)
{
  BLI_assert(t->modifiers & MOD_EDIT_SNAP_SOURCE);

  t->tsnap.snap_target_fn(t, nullptr);
  if (t->tsnap.status & SNAP_MULTI_POINTS) {
    getSnapPoint(t, t->tsnap.snap_source);
  }
  t->redraw |= TREDRAW_SOFT;
}

void transform_mode_snap_source_init(TransInfo *t, wmOperator * /*op*/)
{
  if (t->mode_info == &TransMode_snapsource) {
    /* Already running. */
    return;
  }

  if (ELEM(t->mode, TFM_INIT, TFM_DUMMY)) {
    /* Fallback */
    transform_mode_init(t, nullptr, TFM_TRANSLATION);
  }

  SnapSouceCustomData *customdata = static_cast<SnapSouceCustomData *>(
      MEM_callocN(sizeof(*customdata), __func__));
  customdata->mode_info_prev = t->mode_info;

  customdata->target_operation_prev = t->tsnap.target_operation;

  customdata->mouse_prev.apply = t->mouse.apply;
  customdata->mouse_prev.post = t->mouse.post;
  customdata->mouse_prev.use_virtual_mval = t->mouse.use_virtual_mval;

  customdata->customdata_mode_prev = t->custom.mode.data;
  t->custom.mode.data = customdata;

  if (!(t->modifiers & MOD_SNAP) || !transformModeUseSnap(t)) {
    t->modifiers |= (MOD_SNAP | MOD_SNAP_FORCED);
  }

  t->mode_info = &TransMode_snapsource;
  t->flag |= T_DRAW_SNAP_SOURCE;
  t->tsnap.target_operation = SCE_SNAP_TARGET_ALL;
  t->tsnap.status &= ~SNAP_SOURCE_FOUND;

  customdata->snap_mode_confirm = t->tsnap.mode;
  t->tsnap.mode &= ~(SCE_SNAP_TO_EDGE_PERPENDICULAR | SCE_SNAP_INDIVIDUAL_PROJECT |
                     SCE_SNAP_INDIVIDUAL_NEAREST);

  if ((t->tsnap.mode & ~(SCE_SNAP_TO_INCREMENT | SCE_SNAP_TO_GRID)) == 0) {
    /* Initialize snap modes for geometry. */
    t->tsnap.mode &= ~(SCE_SNAP_TO_INCREMENT | SCE_SNAP_TO_GRID);
    t->tsnap.mode |= SCE_SNAP_TO_GEOM & ~SCE_SNAP_TO_EDGE_PERPENDICULAR;

    if (!(customdata->snap_mode_confirm & SCE_SNAP_TO_EDGE_PERPENDICULAR)) {
      customdata->snap_mode_confirm = t->tsnap.mode;
    }
  }

  if (t->data_type == &TransConvertType_Mesh) {
    ED_transform_snap_object_context_set_editmesh_callbacks(
        t->tsnap.object_context, nullptr, nullptr, nullptr, nullptr);
  }

#ifdef RESET_TRANSFORMATION
  /* Temporarily disable snapping.
   * We don't want #SCE_SNAP_PROJECT to affect `recalcData` for example. */
  t->tsnap.flag &= ~SCE_SNAP;

  restoreTransObjects(t);

  /* Restore snapping status. */
  transform_snap_flag_from_modifiers_set(t);

  /* Reset initial values to restore gizmo position. */
  applyMouseInput(t, &t->mouse, t->mouse.imval, t->values_final);
#endif

#ifdef REMOVE_GIZMO
  wmGizmo *gz = WM_gizmomap_get_modal(t->region->gizmo_map);
  if (gz) {
    const wmEvent *event = CTX_wm_window(t->context)->eventstate;
#  ifdef RESET_TRANSFORMATION
    wmGizmoFnModal modal_fn = gz->custom_modal ? gz->custom_modal : gz->type->modal;
    modal_fn(t->context, gz, event, eWM_GizmoFlagTweak(0));
#  endif

    WM_gizmo_modal_set_while_modal(t->region->gizmo_map, t->context, nullptr, event);
  }
#endif

  t->mouse.apply = nullptr;
  t->mouse.post = nullptr;
  t->mouse.use_virtual_mval = false;
}

/** \} */

TransModeInfo TransMode_snapsource = {
    /*flags*/ 0,
    /*init_fn*/ transform_mode_snap_source_init,
    /*transform_fn*/ snapsource_transform_fn,
    /*transform_matrix_fn*/ nullptr,
    /*handle_event_fn*/ snapsource_handle_event_fn,
    /*snap_distance_fn*/ nullptr,
    /*snap_apply_fn*/ nullptr,
    /*draw_fn*/ nullptr,
};
