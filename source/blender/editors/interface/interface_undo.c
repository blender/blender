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
 *
 * Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 *
 * Undo stack to use for UI widgets that manage their own editing state.
 */

#include <string.h>

#include "BLI_listbase.h"

#include "DNA_listBase.h"

#include "MEM_guardedalloc.h"

#include "interface_intern.h"

/* -------------------------------------------------------------------- */
/** \name Text Field Undo Stack
 * \{ */

typedef struct uiUndoStack_Text_State {
  struct uiUndoStack_Text_State *next, *prev;
  int cursor_index;
  char text[0];
} uiUndoStack_Text_State;

typedef struct uiUndoStack_Text {
  ListBase states;
  uiUndoStack_Text_State *current;
} uiUndoStack_Text;

static const char *ui_textedit_undo_impl(uiUndoStack_Text *stack, int *r_cursor_index)
{
  /* Don't undo if no data has been pushed yet. */
  if (stack->current == NULL) {
    return NULL;
  }

  /* Travel backwards in the stack and copy information to the caller. */
  if (stack->current->prev != NULL) {
    stack->current = stack->current->prev;

    *r_cursor_index = stack->current->cursor_index;
    return stack->current->text;
  }
  return NULL;
}

static const char *ui_textedit_redo_impl(uiUndoStack_Text *stack, int *r_cursor_index)
{
  /* Don't redo if no data has been pushed yet. */
  if (stack->current == NULL) {
    return NULL;
  }

  /* Only redo if new data has not been entered since the last undo. */
  if (stack->current->next) {
    stack->current = stack->current->next;

    *r_cursor_index = stack->current->cursor_index;
    return stack->current->text;
  }
  return NULL;
}

const char *ui_textedit_undo(uiUndoStack_Text *stack, int direction, int *r_cursor_index)
{
  BLI_assert(ELEM(direction, -1, 1));
  if (direction < 0) {
    return ui_textedit_undo_impl(stack, r_cursor_index);
  }
  else {
    return ui_textedit_redo_impl(stack, r_cursor_index);
  }
}

/**
 * Push the information in the arguments to a new state in the undo stack.
 *
 * \note Currently the total length of the undo stack is not limited.
 */
void ui_textedit_undo_push(uiUndoStack_Text *stack, const char *text, int cursor_index)
{
  /* Clear all redo actions from the current state. */
  if (stack->current != NULL) {
    while (stack->current->next) {
      uiUndoStack_Text_State *state = stack->current->next;
      BLI_remlink(&stack->states, state);
      MEM_freeN(state);
    }
  }

  /* Create the new state  */
  const int text_size = strlen(text) + 1;
  stack->current = MEM_mallocN(sizeof(uiUndoStack_Text_State) + text_size, __func__);
  stack->current->cursor_index = cursor_index;
  memcpy(stack->current->text, text, text_size);
  BLI_addtail(&stack->states, stack->current);
}
/**
 * Start the undo stack.
 *
 * \note The current state should be pushed immediately after calling this.
 */
uiUndoStack_Text *ui_textedit_undo_stack_create(void)
{
  uiUndoStack_Text *stack = MEM_mallocN(sizeof(uiUndoStack_Text), __func__);
  stack->current = NULL;
  BLI_listbase_clear(&stack->states);

  return stack;
}

void ui_textedit_undo_stack_destroy(uiUndoStack_Text *stack)
{
  BLI_freelistN(&stack->states);
  MEM_freeN(stack);
}

/** \} */
