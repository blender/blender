/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "interface_intern.hh"

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Button Groups
 * \{ */

void block_new_button_group(Block *block, ButtonGroupFlag flag)
{
  /* Don't create a new group if there is a "lock" on new groups. */
  if (!block->button_groups.is_empty()) {
    ButtonGroup &last_group = block->button_groups.last();
    if (last_group.flag & UI_BUTTON_GROUP_LOCK) {
      return;
    }
  }

  block->button_groups.append({});
  block->button_groups.last().flag = flag;
}

void button_group_add_but(Block *block, Button *but)
{
  if (block->button_groups.is_empty()) {
    block_new_button_group(block, ButtonGroupFlag(0));
  }

  ButtonGroup &current_group = block->button_groups.last();
  current_group.buttons.append(but);
}

void button_group_replace_but_ptr(Block *block, const Button *old_but_ptr, Button *new_but)
{
  for (ButtonGroup &group : block->button_groups) {
    std::replace_if(
        group.buttons.begin(),
        group.buttons.end(),
        [&](const Button *ptr) { return ptr == old_but_ptr; },
        new_but);
  }
}

/** \} */

}  // namespace blender::ui
