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
 * \ingroup sptext
 */

#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BLI_array_utils.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_text.h"
#include "BKE_undo_system.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_text.h"
#include "ED_curve.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "text_intern.h"
#include "text_format.h"

/* TODO(campbell): undo_system: move text undo out of text block. */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct TextUndoStep {
  UndoStep step;
  UndoRefID_Text text_ref;
  TextUndoBuf data;
} TextUndoStep;

static bool text_undosys_poll(bContext *UNUSED(C))
{
  /* Only use when operators initialized. */
  UndoStack *ustack = ED_undo_stack_get();
  return (ustack->step_init && (ustack->step_init->type == BKE_UNDOSYS_TYPE_TEXT));
}

static void text_undosys_step_encode_init(struct bContext *C, UndoStep *us_p)
{
  TextUndoStep *us = (TextUndoStep *)us_p;
  BLI_assert(BLI_array_is_zeroed(&us->data, 1));

  UNUSED_VARS(C);
  /* XXX, use to set the undo type only. */

  us->data.buf = NULL;
  us->data.len = 0;
  us->data.pos = -1;
}

static bool text_undosys_step_encode(struct bContext *C,
                                     struct Main *UNUSED(bmain),
                                     UndoStep *us_p)
{
  TextUndoStep *us = (TextUndoStep *)us_p;

  Text *text = CTX_data_edit_text(C);

  /* No undo data was generated. Hint, use global undo here. */
  if ((us->data.pos == -1) || (us->data.buf == NULL)) {
    return false;
  }

  us_p->is_applied = true;

  us->text_ref.ptr = text;

  us->step.data_size = us->data.len;

  return true;
}

static void text_undosys_step_decode_undo_impl(Text *text, TextUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);
  TextUndoBuf data = us->data;
  while (data.pos > -1) {
    txt_do_undo(text, &data);
  }
  BLI_assert(data.pos == -1);
  us->step.is_applied = false;
}

static void text_undosys_step_decode_redo_impl(Text *text, TextUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  TextUndoBuf data = us->data;
  data.pos = -1;
  while (data.pos < us->data.pos) {
    txt_do_redo(text, &data);
  }
  BLI_assert(data.pos == us->data.pos);
  us->step.is_applied = true;
}

static void text_undosys_step_decode_undo(TextUndoStep *us)
{
  TextUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (TextUndoStep *)us_iter->step.next;
  }
  Text *text_prev = NULL;
  while (us_iter != us) {
    Text *text = us_iter->text_ref.ptr;
    text_undosys_step_decode_undo_impl(text, us_iter);
    if (text_prev != text) {
      text_update_edited(text);
      text_prev = text;
    }
    us_iter = (TextUndoStep *)us_iter->step.prev;
  }
}

static void text_undosys_step_decode_redo(TextUndoStep *us)
{
  TextUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = (TextUndoStep *)us_iter->step.prev;
  }
  Text *text_prev = NULL;
  while (us_iter && (us_iter->step.is_applied == false)) {
    Text *text = us_iter->text_ref.ptr;
    text_undosys_step_decode_redo_impl(text, us_iter);
    if (text_prev != text) {
      text_update_edited(text);
      text_prev = text;
    }
    if (us_iter == us) {
      break;
    }
    us_iter = (TextUndoStep *)us_iter->step.next;
  }
}

static void text_undosys_step_decode(
    struct bContext *C, struct Main *UNUSED(bmain), UndoStep *us_p, int dir, bool UNUSED(is_final))
{
  TextUndoStep *us = (TextUndoStep *)us_p;

  if (dir < 0) {
    text_undosys_step_decode_undo(us);
  }
  else {
    text_undosys_step_decode_redo(us);
  }

  Text *text = us->text_ref.ptr;
  SpaceText *st = CTX_wm_space_text(C);
  if (st) {
    /* Not essential, always show text being undo where possible. */
    st->text = text;
  }
  text_update_cursor_moved(C);
  text_drawcache_tag_update(st, 1);
  WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
}

static void text_undosys_step_free(UndoStep *us_p)
{
  TextUndoStep *us = (TextUndoStep *)us_p;
  MEM_SAFE_FREE(us->data.buf);
}

static void text_undosys_foreach_ID_ref(UndoStep *us_p,
                                        UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                        void *user_data)
{
  TextUndoStep *us = (TextUndoStep *)us_p;
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->text_ref));
}

/* Export for ED_undo_sys. */

void ED_text_undosys_type(UndoType *ut)
{
  ut->name = "Text";
  ut->poll = text_undosys_poll;
  ut->step_encode_init = text_undosys_step_encode_init;
  ut->step_encode = text_undosys_step_encode;
  ut->step_decode = text_undosys_step_decode;
  ut->step_free = text_undosys_step_free;

  ut->step_foreach_ID_ref = text_undosys_foreach_ID_ref;

  ut->use_context = false;

  ut->step_size = sizeof(TextUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/* Use operator system to finish the undo step. */
TextUndoBuf *ED_text_undo_push_init(bContext *C)
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us_p = BKE_undosys_step_push_init_with_type(ustack, C, NULL, BKE_UNDOSYS_TYPE_TEXT);
  TextUndoStep *us = (TextUndoStep *)us_p;
  return &us->data;
}

/** \} */
