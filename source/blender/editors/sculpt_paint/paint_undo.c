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

/** \file blender/editors/sculpt_paint/paint_undo.c
 *  \ingroup edsculpt
 *  \brief Undo system for painting and sculpting.
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_userdef_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_global.h"

#include "ED_paint.h"

#include "paint_intern.h"

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char name[BKE_UNDO_STR_MAX];
	uintptr_t undosize;

	ListBase elems;

	UndoRestoreCb restore;
	UndoFreeCb free;
	UndoCleanupCb cleanup;
} UndoElem;

typedef struct UndoStack {
	int type;
	ListBase elems;
	UndoElem *current;
} UndoStack;

static UndoStack ImageUndoStack = {UNDO_PAINT_IMAGE, {NULL, NULL}, NULL};
static UndoStack MeshUndoStack = {UNDO_PAINT_MESH, {NULL, NULL}, NULL};

/* Generic */

static void undo_restore(bContext *C, UndoStack *UNUSED(stack), UndoElem *uel)
{
	if (uel && uel->restore)
		uel->restore(C, &uel->elems);
}

static void undo_elem_free(UndoStack *UNUSED(stack), UndoElem *uel)
{
	if (uel && uel->free) {
		uel->free(&uel->elems);
		BLI_freelistN(&uel->elems);
	}
}

static void undo_stack_push_begin(UndoStack *stack, const char *name, UndoRestoreCb restore, UndoFreeCb free, UndoCleanupCb cleanup)
{
	UndoElem *uel;
	int nr;
	
	/* Undo push is split up in begin and end, the reason is that as painting
	 * happens more tiles/nodes are added to the list, and at the very end we
	 * know how much memory the undo used to remove old undo elements */

	/* remove all undos after (also when stack->current==NULL) */
	while (stack->elems.last != stack->current) {
		uel = stack->elems.last;
		undo_elem_free(stack, uel);
		BLI_freelinkN(&stack->elems, uel);
	}
	
	/* make new */
	stack->current = uel = MEM_callocN(sizeof(UndoElem), "undo file");
	uel->restore = restore;
	uel->free = free;
	uel->cleanup = cleanup;
	BLI_addtail(&stack->elems, uel);

	/* name can be a dynamic string */
	BLI_strncpy(uel->name, name, sizeof(uel->name));
	
	/* limit amount to the maximum amount*/
	nr = 0;
	uel = stack->elems.last;
	while (uel) {
		nr++;
		if (nr == U.undosteps) break;
		uel = uel->prev;
	}
	if (uel) {
		while (stack->elems.first != uel) {
			UndoElem *first = stack->elems.first;
			undo_elem_free(stack, first);
			BLI_freelinkN(&stack->elems, first);
		}
	}
}

static void undo_stack_push_end(UndoStack *stack)
{
	UndoElem *uel;
	uintptr_t totmem, maxmem;
	int totundo = 0;

	/* first limit to undo steps */
	uel = stack->elems.last;

	while (uel) {
		totundo++;
		if (totundo > U.undosteps) break;
		uel = uel->prev;
	}

	if (uel) {
		UndoElem *first;

		/* in case the undo steps are zero, the current pointer will be invalid */
		if (uel == stack->current)
			stack->current = NULL;

		do {
			first = stack->elems.first;
			undo_elem_free(stack, first);
			BLI_freelinkN(&stack->elems, first);
		} while (first != uel);
	}

	if (U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem = 0;
		maxmem = ((uintptr_t)U.undomemory) * 1024 * 1024;

		uel = stack->elems.last;
		while (uel) {
			totmem += uel->undosize;
			if (totmem > maxmem) break;
			uel = uel->prev;
		}

		if (uel) {
			while (stack->elems.first != uel) {
				UndoElem *first = stack->elems.first;
				undo_elem_free(stack, first);
				BLI_freelinkN(&stack->elems, first);
			}
		}
	}
}

static void undo_stack_cleanup(UndoStack *stack, bContext *C)
{
	UndoElem *uel = stack->elems.first;
	bool stack_reset = false;

	while (uel) {
		if (uel->cleanup && uel->cleanup(C, &uel->elems)) {
			UndoElem *uel_tmp = uel->next;
			if (stack->current == uel) {
				stack->current = NULL;
				stack_reset = true;
			}
			undo_elem_free(stack, uel);
			BLI_freelinkN(&stack->elems, uel);
			uel = uel_tmp;
		}
		else
			uel = uel->next;
	}
	if (stack_reset) {
		stack->current = stack->elems.last;
	}

}

static int undo_stack_step(bContext *C, UndoStack *stack, int step, const char *name)
{
	UndoElem *undo;

	/* first cleanup any old undo steps that may belong to invalid data */
	undo_stack_cleanup(stack, C);

	if (step == 1) {
		if (stack->current == NULL) {
			/* pass */
		}
		else {
			if (!name || strcmp(stack->current->name, name) == 0) {
				if (G.debug & G_DEBUG_WM) {
					printf("%s: undo '%s'\n", __func__, stack->current->name);
				}
				undo_restore(C, stack, stack->current);
				stack->current = stack->current->prev;
				return 1;
			}
		}
	}
	else if (step == -1) {
		if ((stack->current != NULL && stack->current->next == NULL) || BLI_listbase_is_empty(&stack->elems)) {
			/* pass */
		}
		else {
			if (!name || strcmp(stack->current->name, name) == 0) {
				undo = (stack->current && stack->current->next) ? stack->current->next : stack->elems.first;
				undo_restore(C, stack, undo);
				stack->current = undo;
				if (G.debug & G_DEBUG_WM) {
					printf("%s: redo %s\n", __func__, undo->name);
				}
				return 1;
			}
		}
	}

	return 0;
}

static void undo_stack_free(UndoStack *stack)
{
	UndoElem *uel;
	
	for (uel = stack->elems.first; uel; uel = uel->next)
		undo_elem_free(stack, uel);

	BLI_freelistN(&stack->elems);
	stack->current = NULL;
}

/* Exported Functions */

void ED_undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free, UndoCleanupCb cleanup)
{
	if (type == UNDO_PAINT_IMAGE)
		undo_stack_push_begin(&ImageUndoStack, name, restore, free, cleanup);
	else if (type == UNDO_PAINT_MESH)
		undo_stack_push_begin(&MeshUndoStack, name, restore, free, cleanup);
}

ListBase *undo_paint_push_get_list(int type)
{
	if (type == UNDO_PAINT_IMAGE) {
		if (ImageUndoStack.current) {
			return &ImageUndoStack.current->elems;
		}
	}
	else if (type == UNDO_PAINT_MESH) {
		if (MeshUndoStack.current) {
			return &MeshUndoStack.current->elems;
		}
	}
	
	return NULL;
}

void undo_paint_push_count_alloc(int type, int size)
{
	if (type == UNDO_PAINT_IMAGE)
		ImageUndoStack.current->undosize += size;
	else if (type == UNDO_PAINT_MESH)
		MeshUndoStack.current->undosize += size;
}

void ED_undo_paint_push_end(int type)
{
	if (type == UNDO_PAINT_IMAGE)
		undo_stack_push_end(&ImageUndoStack);
	else if (type == UNDO_PAINT_MESH)
		undo_stack_push_end(&MeshUndoStack);
}

int ED_undo_paint_step(bContext *C, int type, int step, const char *name)
{
	if (type == UNDO_PAINT_IMAGE)
		return undo_stack_step(C, &ImageUndoStack, step, name);
	else if (type == UNDO_PAINT_MESH)
		return undo_stack_step(C, &MeshUndoStack, step, name);
	
	return 0;
}

static void undo_step_num(bContext *C, UndoStack *stack, int step)
{
	UndoElem *uel;
	int a = 0;
	int curnum = BLI_findindex(&stack->elems, stack->current);

	for (uel = stack->elems.first; uel; uel = uel->next, a++) {
		if (a == step) break;
	}

	if (curnum > a) {
		while (a++ != curnum)
			undo_stack_step(C, stack, 1, NULL);
	}
	else if (curnum < a) {
		while (a-- != curnum)
			undo_stack_step(C, stack, -1, NULL);
	}
}

void ED_undo_paint_step_num(bContext *C, int type, int step)
{
	if (type == UNDO_PAINT_IMAGE)
		undo_step_num(C, &ImageUndoStack, step);
	else if (type == UNDO_PAINT_MESH)
		undo_step_num(C, &MeshUndoStack, step);
}

static char *undo_stack_get_name(UndoStack *stack, int nr, int *active)
{
	UndoElem *uel;

	if (active) *active = 0;

	uel = BLI_findlink(&stack->elems, nr);
	if (uel) {
		if (active && uel == stack->current)
			*active = 1;
		return uel->name;
	}

	return NULL;
}

const char *ED_undo_paint_get_name(bContext *C, int type, int nr, int *active)
{

	if (type == UNDO_PAINT_IMAGE) {
		undo_stack_cleanup(&ImageUndoStack, C);
		return undo_stack_get_name(&ImageUndoStack, nr, active);
	}
	else if (type == UNDO_PAINT_MESH) {
		undo_stack_cleanup(&MeshUndoStack, C);
		return undo_stack_get_name(&MeshUndoStack, nr, active);
	}
	return NULL;
}

bool ED_undo_paint_empty(int type)
{
	UndoStack *stack;

	if (type == UNDO_PAINT_IMAGE)
		stack = &ImageUndoStack;
	else if (type == UNDO_PAINT_MESH)
		stack = &MeshUndoStack;
	else
		return true;

	if (stack->current == NULL) {
		return true;
	}

	return false;
}

int ED_undo_paint_valid(int type, const char *name)
{
	UndoStack *stack;
	
	if (type == UNDO_PAINT_IMAGE)
		stack = &ImageUndoStack;
	else if (type == UNDO_PAINT_MESH)
		stack = &MeshUndoStack;
	else 
		return 0;
	
	if (stack->current == NULL) {
		/* pass */
	}
	else {
		if (name && strcmp(stack->current->name, name) == 0)
			return 1;
		else
			return stack->elems.first != stack->elems.last;
	}
	return 0;
}

void ED_undo_paint_free(void)
{
	undo_stack_free(&ImageUndoStack);
	undo_stack_free(&MeshUndoStack);
}
