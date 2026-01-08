/* SPDX-FileCopyrightText: 2020 Blender Authors
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

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Text Field Undo Stack
 * \{ */

struct UndoStack_Text_State {
  UndoStack_Text_State *next, *prev;
  int cursor_index;
  char text[0];
};

struct UndoStack_Text {
  ListBaseT<UndoStack_Text_State> states;
  UndoStack_Text_State *current;
};

static const char *ui_textedit_undo_impl(UndoStack_Text *stack, int *r_cursor_index)
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

static const char *ui_textedit_redo_impl(UndoStack_Text *stack, int *r_cursor_index)
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

const char *textedit_undo(UndoStack_Text *stack, int direction, int *r_cursor_index)
{
  BLI_assert(ELEM(direction, -1, 1));
  if (direction < 0) {
    return ui_textedit_undo_impl(stack, r_cursor_index);
  }
  return ui_textedit_redo_impl(stack, r_cursor_index);
}

void textedit_undo_push(UndoStack_Text *stack, const char *text, int cursor_index)
{
  /* Clear all redo actions from the current state. */
  if (stack->current != nullptr) {
    while (stack->current->next) {
      UndoStack_Text_State *state = stack->current->next;
      BLI_remlink(&stack->states, state);
      MEM_delete(state);
    }
  }

  /* Create the new state. */
  const int text_size = strlen(text) + 1;
  stack->current = static_cast<UndoStack_Text_State *>(
      MEM_new_uninitialized(sizeof(UndoStack_Text_State) + text_size, __func__));
  stack->current->cursor_index = cursor_index;
  memcpy(stack->current->text, text, text_size);
  BLI_addtail(&stack->states, stack->current);
}

UndoStack_Text *textedit_undo_stack_create()
{
  UndoStack_Text *stack = MEM_new_zeroed<UndoStack_Text>(__func__);
  stack->current = nullptr;
  BLI_listbase_clear(&stack->states);

  return stack;
}

void textedit_undo_stack_destroy(UndoStack_Text *stack)
{
  BLI_freelistN(&stack->states);
  MEM_delete(stack);
}

/** \} */

}  // namespace blender::ui
