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

#include "BLI_array_store.h"
#include "BLI_array_utils.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_text.h"
#include "BKE_undo_system.h"
#include "BKE_main.h"

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

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

#define ARRAY_CHUNK_SIZE 128

typedef struct TextUndoStep {
  UndoStep step;
  UndoRefID_Text text_ref;
  struct {
    BArrayState *state;
    int buf_len;
  } data;

  struct {
    int line, line_select;
    int column, column_select;
  } cursor;

} TextUndoStep;

static struct {
  BArrayStore *buffer_store;
  int users;
} g_text_buffers = {NULL};

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

  UNUSED_VARS(C, us);
  /* XXX, use to set the undo type only. */
}

static bool text_undosys_step_encode(struct bContext *C,
                                     struct Main *UNUSED(bmain),
                                     UndoStep *us_p)
{
  TextUndoStep *us = (TextUndoStep *)us_p;

  Text *text = CTX_data_edit_text(C);

  int buf_len = 0;

  uchar *buf = (uchar *)txt_to_buf_for_undo(text, &buf_len);
  if (g_text_buffers.buffer_store == NULL) {
    g_text_buffers.buffer_store = BLI_array_store_create(1, ARRAY_CHUNK_SIZE);
  }
  g_text_buffers.users += 1;
  const size_t total_size_prev = BLI_array_store_calc_size_compacted_get(
      g_text_buffers.buffer_store);

  us->data.state = BLI_array_store_state_add(g_text_buffers.buffer_store, buf, buf_len, NULL);
  MEM_freeN(buf);

  us->cursor.line = txt_get_span(text->lines.first, text->curl);
  us->cursor.column = text->curc;

  if (txt_has_sel(text)) {
    us->cursor.line_select = (text->curl == text->sell) ?
                                 us->cursor.line :
                                 txt_get_span(text->lines.first, text->sell);
    us->cursor.column_select = text->selc;
  }
  else {
    us->cursor.line_select = us->cursor.line;
    us->cursor.column_select = us->cursor.column;
  }

  us_p->is_applied = true;

  us->text_ref.ptr = text;

  us->step.data_size = BLI_array_store_calc_size_compacted_get(g_text_buffers.buffer_store) -
                       total_size_prev;

  return true;
}

static void text_undosys_step_decode(struct bContext *C,
                                     struct Main *UNUSED(bmain),
                                     UndoStep *us_p,
                                     int UNUSED(dir),
                                     bool UNUSED(is_final))
{
  TextUndoStep *us = (TextUndoStep *)us_p;
  Text *text = us->text_ref.ptr;
  size_t buf_len;

  {
    const uchar *buf = BLI_array_store_state_data_get_alloc(us->data.state, &buf_len);
    txt_from_buf_for_undo(text, (const char *)buf, buf_len);
    MEM_freeN((void *)buf);
  }

  const bool has_select = ((us->cursor.line != us->cursor.line_select) ||
                           (us->cursor.column != us->cursor.column_select));
  if (has_select) {
    txt_move_to(text, us->cursor.line_select, us->cursor.column_select, false);
  }
  txt_move_to(text, us->cursor.line, us->cursor.column, has_select);

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

  BLI_array_store_state_remove(g_text_buffers.buffer_store, us->data.state);

  g_text_buffers.users -= 1;
  if (g_text_buffers.users == 0) {
    BLI_array_store_destroy(g_text_buffers.buffer_store);
    g_text_buffers.buffer_store = NULL;
  }
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
UndoStep *ED_text_undo_push_init(bContext *C)
{
  UndoStack *ustack = ED_undo_stack_get();
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = bmain->wm.first;
  if (wm->op_undo_depth <= 1) {
    UndoStep *us_p = BKE_undosys_step_push_init_with_type(ustack, C, NULL, BKE_UNDOSYS_TYPE_TEXT);
    return us_p;
  }
  return NULL;
}

/** \} */
