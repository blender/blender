/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"

#include "BKE_paint.hh"
#include "BKE_undo_system.hh"

#include "ED_paint.hh"
#include "ED_undo.hh"

#include "WM_api.hh"

#include "paint_intern.hh"

#ifndef NDEBUG
#  include "BLI_array_utils.h" /* #BLI_array_is_zeroed */
#endif

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

struct UndoCurve {
  PaintCurvePoint *points; /* points of curve */
  int tot_points;
  int add_index;
};

static void undocurve_from_paintcurve(UndoCurve *uc, const PaintCurve *pc)
{
  BLI_assert(BLI_array_is_zeroed(uc, 1));
  uc->points = static_cast<PaintCurvePoint *>(MEM_dupallocN(pc->points));
  uc->tot_points = pc->tot_points;
  uc->add_index = pc->add_index;
}

static void undocurve_to_paintcurve(const UndoCurve *uc, PaintCurve *pc)
{
  MEM_SAFE_FREE(pc->points);
  pc->points = static_cast<PaintCurvePoint *>(MEM_dupallocN(uc->points));
  pc->tot_points = uc->tot_points;
  pc->add_index = uc->add_index;
}

static void undocurve_free_data(UndoCurve *uc)
{
  MEM_SAFE_FREE(uc->points);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

struct PaintCurveUndoStep {
  UndoStep step;

  UndoRefID_PaintCurve pc_ref;

  UndoCurve data;
};

static bool paintcurve_undosys_poll(bContext *C)
{
  if (C == nullptr || !paint_curve_poll(C)) {
    return false;
  }
  Paint *p = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(p);
  return (brush && brush->paint_curve);
}

static void paintcurve_undosys_step_encode_init(bContext *C, UndoStep *us_p)
{
  /* XXX, use to set the undo type only. */
  UNUSED_VARS(C, us_p);
}

static bool paintcurve_undosys_step_encode(bContext *C, Main * /*bmain*/, UndoStep *us_p)
{
  /* FIXME Double check this, it should not be needed here at all? undo system is supposed to
   * ensure that. */
  if (!paint_curve_poll(C)) {
    return false;
  }

  Paint *p = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(p);
  PaintCurve *pc = p ? (brush ? brush->paint_curve : nullptr) : nullptr;
  if (pc == nullptr) {
    return false;
  }

  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  BLI_assert(us->step.data_size == 0);

  us->pc_ref.ptr = pc;
  undocurve_from_paintcurve(&us->data, pc);

  return true;
}

static void paintcurve_undosys_step_decode(bContext * /*C*/,
                                           Main * /*bmain*/,
                                           UndoStep *us_p,
                                           const eUndoStepDir /*dir*/,
                                           bool /*is_final*/)
{
  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  undocurve_to_paintcurve(&us->data, us->pc_ref.ptr);
}

static void paintcurve_undosys_step_free(UndoStep *us_p)
{
  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  undocurve_free_data(&us->data);
}

static void paintcurve_undosys_foreach_ID_ref(UndoStep *us_p,
                                              UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                              void *user_data)
{
  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->pc_ref));
}

void ED_paintcurve_undosys_type(UndoType *ut)
{
  ut->name = "Paint Curve";
  ut->poll = paintcurve_undosys_poll;
  ut->step_encode_init = paintcurve_undosys_step_encode_init;
  ut->step_encode = paintcurve_undosys_step_encode;
  ut->step_decode = paintcurve_undosys_step_decode;
  ut->step_free = paintcurve_undosys_step_free;

  ut->step_foreach_ID_ref = paintcurve_undosys_foreach_ID_ref;

  ut->flags = 0;

  ut->step_size = sizeof(PaintCurveUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void ED_paintcurve_undo_push_begin(const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = nullptr; /* special case, we never read from this. */
  BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_PAINTCURVE);
}

void ED_paintcurve_undo_push_end(bContext *C)
{
  UndoStack *ustack = ED_undo_stack_get();
  BKE_undosys_step_push(ustack, C, nullptr);
  BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
  WM_file_tag_modified();
}

/** \} */
