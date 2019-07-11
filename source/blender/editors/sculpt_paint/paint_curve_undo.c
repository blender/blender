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
 * \ingroup edsculpt
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_space_types.h"

#include "BLI_array_utils.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_undo_system.h"

#include "ED_paint.h"
#include "ED_undo.h"

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoCurve {
  PaintCurvePoint *points; /* points of curve */
  int tot_points;
  int add_index;
} UndoCurve;

static void undocurve_from_paintcurve(UndoCurve *uc, const PaintCurve *pc)
{
  BLI_assert(BLI_array_is_zeroed(uc, 1));
  uc->points = MEM_dupallocN(pc->points);
  uc->tot_points = pc->tot_points;
  uc->add_index = pc->add_index;
}

static void undocurve_to_paintcurve(const UndoCurve *uc, PaintCurve *pc)
{
  MEM_SAFE_FREE(pc->points);
  pc->points = MEM_dupallocN(uc->points);
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

typedef struct PaintCurveUndoStep {
  UndoStep step;
  PaintCurve *pc;
  UndoCurve data;
} PaintCurveUndoStep;

static bool paintcurve_undosys_poll(bContext *C)
{
  if (C == NULL || !paint_curve_poll(C)) {
    return false;
  }
  Paint *p = BKE_paint_get_active_from_context(C);
  return (p->brush && p->brush->paint_curve);
}

static void paintcurve_undosys_step_encode_init(struct bContext *C, UndoStep *us_p)
{
  /* XXX, use to set the undo type only. */
  UNUSED_VARS(C, us_p);
}

static bool paintcurve_undosys_step_encode(struct bContext *C,
                                           struct Main *UNUSED(bmain),
                                           UndoStep *us_p)
{
  if (C == NULL || !paint_curve_poll(C)) {
    return false;
  }
  Paint *p = BKE_paint_get_active_from_context(C);
  PaintCurve *pc = p ? (p->brush ? p->brush->paint_curve : NULL) : NULL;
  if (pc == NULL) {
    return false;
  }

  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  BLI_assert(us->step.data_size == 0);

  us->pc = pc;
  undocurve_from_paintcurve(&us->data, pc);

  return true;
}

static void paintcurve_undosys_step_decode(struct bContext *UNUSED(C),
                                           struct Main *UNUSED(bmain),
                                           UndoStep *us_p,
                                           int UNUSED(dir),
                                           bool UNUSED(is_final))
{
  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  undocurve_to_paintcurve(&us->data, us->pc);
}

static void paintcurve_undosys_step_free(UndoStep *us_p)
{
  PaintCurveUndoStep *us = (PaintCurveUndoStep *)us_p;
  undocurve_free_data(&us->data);
}

/* Export for ED_undo_sys. */
void ED_paintcurve_undosys_type(UndoType *ut)
{
  ut->name = "Paint Curve";
  /* don't poll for now */
  ut->poll = paintcurve_undosys_poll;
  ut->step_encode_init = paintcurve_undosys_step_encode_init;
  ut->step_encode = paintcurve_undosys_step_encode;
  ut->step_decode = paintcurve_undosys_step_decode;
  ut->step_free = paintcurve_undosys_step_free;

  ut->use_context = false;

  ut->step_size = sizeof(PaintCurveUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void ED_paintcurve_undo_push_begin(const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = NULL; /* special case, we never read from this. */
  BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_PAINTCURVE);
}

void ED_paintcurve_undo_push_end(void)
{
  UndoStack *ustack = ED_undo_stack_get();
  BKE_undosys_step_push(ustack, NULL, NULL);
}

/** \} */
