/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "MEM_guardedalloc.h"

#include "interface_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Button Groups
 * \{ */

void ui_block_new_button_group(uiBlock *block, uiButtonGroupFlag flag)
{
  /* Don't create a new group if there is a "lock" on new groups. */
  if (!block->button_groups.is_empty()) {
    uiButtonGroup &last_group = block->button_groups.last();
    if (last_group.flag & UI_BUTTON_GROUP_LOCK) {
      return;
    }
  }

  block->button_groups.append({});
  block->button_groups.last().flag = flag;
}

void ui_button_group_add_but(uiBlock *block, uiBut *but)
{
  if (block->button_groups.is_empty()) {
    ui_block_new_button_group(block, uiButtonGroupFlag(0));
  }

  uiButtonGroup &current_group = block->button_groups.last();
  current_group.buttons.append(but);
}

void ui_button_group_replace_but_ptr(uiBlock *block, const uiBut *old_but_ptr, uiBut *new_but)
{
  for (uiButtonGroup &group : block->button_groups) {
    std::replace_if(
        group.buttons.begin(),
        group.buttons.end(),
        [&](const uiBut *ptr) { return ptr == old_but_ptr; },
        new_but);
  }
}

/** \} */
