/*
 *
 * Undo system for painting and sculpting.
 * 
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
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_userdef_types.h"


#include "BKE_context.h"
#include "BKE_global.h"

#include "ED_sculpt.h"

#include "paint_intern.h"

#define MAXUNDONAME 64

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char name[MAXUNDONAME];
	uintptr_t undosize;

	ListBase elems;

	UndoRestoreCb restore;
	UndoFreeCb free;
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

static void undo_stack_push_begin(UndoStack *stack, const char *name, UndoRestoreCb restore, UndoFreeCb free)
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

static int undo_stack_step(bContext *C, UndoStack *stack, int step, const char *name)
{
	UndoElem *undo;

	if (step == 1) {
		if (stack->current == NULL) ;
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
		if ((stack->current != NULL && stack->current->next == NULL) || stack->elems.first == NULL) ;
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

void undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free)
{
	if (type == UNDO_PAINT_IMAGE)
		undo_stack_push_begin(&ImageUndoStack, name, restore, free);
	else if (type == UNDO_PAINT_MESH)
		undo_stack_push_begin(&MeshUndoStack, name, restore, free);
}

ListBase *undo_paint_push_get_list(int type)
{
	if (type == UNDO_PAINT_IMAGE) {
		if (ImageUndoStack.current)
			return &ImageUndoStack.current->elems;
	}
	else if (type == UNDO_PAINT_MESH) {
		if (MeshUndoStack.current)
			return &MeshUndoStack.current->elems;
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

void undo_paint_push_end(int type)
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

int ED_undo_paint_valid(int type, const char *name)
{
	UndoStack *stack;
	
	if (type == UNDO_PAINT_IMAGE)
		stack = &ImageUndoStack;
	else if (type == UNDO_PAINT_MESH)
		stack = &MeshUndoStack;
	else 
		return 0;
	
	if (stack->current == NULL) ;
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

