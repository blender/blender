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

/** \file blender/editors/util/memfile_undo.c
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

/** Store workspace modes in undo steps, this may be removed if find a better way to handle. */
#define USE_WORKSPACE_OBJECT_MODE_HACK

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
#include "MEM_guardedalloc.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BKE_main.h"
#include "DNA_workspace_types.h"
#endif


/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
typedef struct WorkSpaceData {
	struct WorkSpaceData *next, *prev;
	char name[MAX_ID_NAME - 2];
	eObjectMode object_mode, object_mode_restore;
	/* TODO, view_layer? */
} WorkSpaceData;
#endif /* USE_WORKSPACE_OBJECT_MODE_HACK */

typedef struct MemFileUndoStep {
	UndoStep step;
	MemFileUndoData *data;

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
	ListBase workspace_data;
#endif
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

	/* can be NULL, use when set. */
	MemFileUndoStep *us_prev = (MemFileUndoStep *)BKE_undosys_step_same_type_prev(us_p);
	us->data = BKE_memfile_undo_encode(bmain, us_prev ? us_prev->data : NULL);
	us->step.data_size = us->data->undo_size;

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
	{
		for (WorkSpace *workspace = bmain->workspaces.first; workspace != NULL; workspace = workspace->id.next) {
			WorkSpaceData *wsd = MEM_mallocN(sizeof(*wsd), __func__);
			BLI_strncpy(wsd->name, workspace->id.name + 2, sizeof(wsd->name));
			wsd->object_mode = workspace->object_mode;
			wsd->object_mode_restore = workspace->object_mode_restore;
			BLI_addtail(&us->workspace_data, wsd);
		}
	}
#endif

	return true;
}

static void memfile_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	/* Loading the content will correctly switch into compatible non-object modes. */
	ED_object_mode_set(C, OB_MODE_OBJECT);

	/* This is needed so undoing/redoing doesn't crash with threaded previews going */
	ED_viewport_render_kill_jobs(CTX_wm_manager(C), CTX_data_main(C), true);
	MemFileUndoStep *us = (MemFileUndoStep *)us_p;
	BKE_memfile_undo_decode(us->data, C);

	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, CTX_data_scene(C));

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
	{
		struct Main *bmain = CTX_data_main(C);
		for (WorkSpaceData *wsd = us->workspace_data.first; wsd != NULL; wsd = wsd->next) {
			WorkSpace *workspace = BLI_findstring(&bmain->workspaces, wsd->name, offsetof(ID, name) + 2);
			if (workspace) {
				workspace->object_mode = wsd->object_mode;
				workspace->object_mode_restore = wsd->object_mode_restore;
			}
		}
	}
#endif
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

#ifdef USE_WORKSPACE_OBJECT_MODE_HACK
	BLI_freelistN(&us->workspace_data);
#endif
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
