/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Undo stack to use for UI widgets that manage their own editing state.
 */

#include <cstring>

#include "BLI_listbase.h"

#include "DNA_listBase.h"

#include "MEM_guardedalloc.h"

#include "interface_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Text Field Undo Stack
 * \{ */

struct uiUndoStack_Text_State {
  uiUndoStack_Text_State *next, *prev;
  int cursor_index;
  char text[0];
};

struct uiUndoStack_Text {
  ListBase states;
  uiUndoStack_Text_State *current;
};

static const char *ui_textedit_undo_impl(uiUndoStack_Text *stack, int *r_cursor_index)
{
  /* Don't undo if no data has been pushed yet. */
  if (stack->current == nullptr) {
    return nullptr;
  }

  /* Travel backwards in the stack and copy information to the caller. */
  if (stack->current->prev != nullptr) {
    stack->current = stack->current->prev;

    *r_cursor_index = stack->current->cursor_index;
    return stack->current->text;
  }
  return nullptr;
}

static const char *ui_textedit_redo_impl(uiUndoStack_Text *stack, int *r_cursor_index)
{
  /* Don't redo if no data has been pushed yet. */
  if (stack->current == nullptr) {
    return nullptr;
  }

  /* Only redo if new data has not been entered since the last undo. */
  if (stack->current->next) {
    stack->current = stack->current->next;

    *r_cursor_index = stack->current->cursor_index;
    return stack->current->text;
  }
  return nullptr;
}

const char *ui_textedit_undo(uiUndoStack_Text *stack, int direction, int *r_cursor_index)
{
  BLI_assert(ELEM(direction, -1, 1));
  if (direction < 0) {
    return ui_textedit_undo_impl(stack, r_cursor_index);
  }
  return ui_textedit_redo_impl(stack, r_cursor_index);
}

void ui_textedit_undo_push(uiUndoStack_Text *stack, const char *text, int cursor_index)
{
  /* Clear all redo actions from the current state. */
  if (stack->current != nullptr) {
    while (stack->current->next) {
      uiUndoStack_Text_State *state = stack->current->next;
      BLI_remlink(&stack->states, state);
      MEM_freeN(state);
    }
  }

  /* Create the new state. */
  const int text_size = strlen(text) + 1;
  stack->current = static_cast<uiUndoStack_Text_State *>(
      MEM_mallocN(sizeof(uiUndoStack_Text_State) + text_size, __func__));
  stack->current->cursor_index = cursor_index;
  memcpy(stack->current->text, text, text_size);
  BLI_addtail(&stack->states, stack->current);
}

uiUndoStack_Text *ui_textedit_undo_stack_create()
{
  uiUndoStack_Text *stack = MEM_new<uiUndoStack_Text>(__func__);
  stack->current = nullptr;
  BLI_listbase_clear(&stack->states);

  return stack;
}

void ui_textedit_undo_stack_destroy(uiUndoStack_Text *stack)
{
  BLI_freelistN(&stack->states);
  MEM_freeN(stack);
}

/** \} */
