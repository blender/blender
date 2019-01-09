/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_text/text_undo.c
 *  \ingroup sptext
 */

#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BLI_listbase.h"
#include "BLI_array_utils.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_library.h"
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

static bool text_undosys_poll(bContext *C)
{
	Text *text = CTX_data_edit_text(C);
	if (text == NULL) {
		return false;
	}
	if (ID_IS_LINKED(text)) {
		return false;
	}
	return true;
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

static bool text_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	TextUndoStep *us = (TextUndoStep *)us_p;

	Text *text = CTX_data_edit_text(C);

	/* No undo data was generated. Hint, use global undo here. */
	if ((us->data.pos == -1) || (us->data.buf == NULL)) {
		return false;
	}

	us->text_ref.ptr = text;

	us->step.data_size = us->data.len;

	return true;
}

static void text_undosys_step_decode(struct bContext *C, UndoStep *us_p, int dir)
{
	TextUndoStep *us = (TextUndoStep *)us_p;
	Text *text = us->text_ref.ptr;

	if (dir < 0) {
		TextUndoBuf data = us->data;
		while (data.pos > -1) {
			txt_do_undo(text, &data);
		}
		BLI_assert(data.pos == -1);
	}
	else {
		TextUndoBuf data = us->data;
		data.pos = -1;
		while (data.pos < us->data.pos) {
			txt_do_redo(text, &data);
		}
		BLI_assert(data.pos == us->data.pos);
	}

	text_update_edited(text);
	text_update_cursor_moved(C);
	text_drawcache_tag_update(CTX_wm_space_text(C), 1);
	WM_event_add_notifier(C, NC_TEXT | NA_EDITED, text);
}

static void text_undosys_step_free(UndoStep *us_p)
{
	TextUndoStep *us = (TextUndoStep *)us_p;
	MEM_SAFE_FREE(us->data.buf);
}

static void text_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
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

	ut->mode = BKE_UNDOTYPE_MODE_ACCUMULATE;
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
