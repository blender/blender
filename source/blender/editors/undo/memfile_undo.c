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

/** \file blender/editors/undo/memfile_undo.c
 *  \ingroup edundo
 *
 * Wrapper between 'ED_undo.h' and 'BKE_undo_system.h' API's.
 */

#include "BLI_utildefines.h"
#include "BLI_sys_types.h"

#include "DNA_object_enums.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_undo_system.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_undo.h"
#include "ED_render.h"


#include "../blenloader/BLO_undofile.h"

#include "undo_intern.h"

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct MemFileUndoStep {
	UndoStep step;
	MemFileUndoData *data;
} MemFileUndoStep;

static bool memfile_undosys_poll(bContext *UNUSED(C))
{
	/* other poll functions must run first, this is a catch-all. */

	if ((U.uiflag & USER_GLOBALUNDO) == 0) {
		return false;
	}
	return true;
}

static bool memfile_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	MemFileUndoStep *us = (MemFileUndoStep *)us_p;

	/* Important we only use 'main' from the context (see: BKE_undosys_stack_init_from_main). */
	struct Main *bmain = CTX_data_main(C);
	UndoStack *ustack = ED_undo_stack_get();

	/* can be NULL, use when set. */
	MemFileUndoStep *us_prev = (MemFileUndoStep *)BKE_undosys_step_find_by_type(ustack, BKE_UNDOSYS_TYPE_MEMFILE);
	us->data = BKE_memfile_undo_encode(bmain, us_prev ? us_prev->data : NULL);
	us->step.data_size = us->data->undo_size;

	return true;
}

static void memfile_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	/* Loading the content will correctly switch into compatible non-object modes. */
	ED_object_mode_exit(C);

	MemFileUndoStep *us = (MemFileUndoStep *)us_p;
	BKE_memfile_undo_decode(us->data, C);

	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, CTX_data_scene(C));
}

static void memfile_undosys_step_free(UndoStep *us_p)
{
	/* To avoid unnecessary slow down, free backwards (so we don't need to merge when clearing all). */
	MemFileUndoStep *us = (MemFileUndoStep *)us_p;
	if (us_p->next != NULL) {
		UndoStep *us_next_p = BKE_undosys_step_same_type_next(us_p);
		if (us_next_p != NULL) {
			MemFileUndoStep *us_next = (MemFileUndoStep *)us_next_p;
			BLO_memfile_merge(&us->data->memfile, &us_next->data->memfile);
		}
	}

	BKE_memfile_undo_free(us->data);
}

/* Export for ED_undo_sys. */
void ED_memfile_undosys_type(UndoType *ut)
{
	ut->name = "Global Undo";
	ut->poll = memfile_undosys_poll;
	ut->step_encode = memfile_undosys_step_encode;
	ut->step_decode = memfile_undosys_step_decode;
	ut->step_free = memfile_undosys_step_free;

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(MemFileUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Ideally we wouldn't need to export global undo internals, there are some cases where it's needed though.
 */
static struct MemFile *ed_undosys_step_get_memfile(UndoStep *us_p)
{
	MemFileUndoStep *us = (MemFileUndoStep *)us_p;
	return &us->data->memfile;
}

struct MemFile *ED_undosys_stack_memfile_get_active(UndoStack *ustack)
{
	UndoStep *us = BKE_undosys_stack_active_with_type(ustack, BKE_UNDOSYS_TYPE_MEMFILE);
	if (us) {
		return ed_undosys_step_get_memfile(us);
	}
	return NULL;
}


/** \} */
