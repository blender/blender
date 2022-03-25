/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "interface_intern.h"

/* -------------------------------------------------------------------- */
/** \name Button Groups
 * \{ */

void ui_block_new_button_group(uiBlock *block, uiButtonGroupFlag flag)
{
  /* Don't create a new group if there is a "lock" on new groups. */
  if (!BLI_listbase_is_empty(&block->button_groups)) {
    uiButtonGroup *last_button_group = block->button_groups.last;
    if (last_button_group->flag & UI_BUTTON_GROUP_LOCK) {
      return;
    }
  }

  uiButtonGroup *new_group = MEM_mallocN(sizeof(uiButtonGroup), __func__);
  BLI_listbase_clear(&new_group->buttons);
  new_group->flag = flag;
  BLI_addtail(&block->button_groups, new_group);
}

void ui_button_group_add_but(uiBlock *block, uiBut *but)
{
  if (BLI_listbase_is_empty(&block->button_groups)) {
    ui_block_new_button_group(block, 0);
  }

  uiButtonGroup *current_button_group = block->button_groups.last;

  /* We can't use the button directly because adding it to
   * this list would mess with its `prev` and `next` pointers. */
  LinkData *button_link = BLI_genericNodeN(but);
  BLI_addtail(&current_button_group->buttons, button_link);
}

static void button_group_free(uiButtonGroup *button_group)
{
  BLI_freelistN(&button_group->buttons);
  MEM_freeN(button_group);
}

void ui_block_free_button_groups(uiBlock *block)
{
  LISTBASE_FOREACH_MUTABLE (uiButtonGroup *, button_group, &block->button_groups) {
    button_group_free(button_group);
  }
}

void ui_button_group_replace_but_ptr(uiBlock *block, const void *old_but_ptr, uiBut *new_but)
{
  LISTBASE_FOREACH (uiButtonGroup *, button_group, &block->button_groups) {
    LISTBASE_FOREACH (LinkData *, link, &button_group->buttons) {
      if (link->data == old_but_ptr) {
        link->data = new_but;
        return;
      }
    }
  }
}

/** \} */
