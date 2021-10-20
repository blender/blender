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
 */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.h"

#include "DNA_space_types.h"

#include "WM_api.h"

#include "UI_interface.h"

static bool ui_tree_view_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  const uiTreeViewItemHandle *hovered_tree_item = UI_block_tree_view_find_item_at(region,
                                                                                  event->xy);
  if (!hovered_tree_item) {
    return false;
  }

  return UI_tree_view_item_can_drop(hovered_tree_item, drag);
}

static char *ui_tree_view_drop_tooltip(bContext *C,
                                       wmDrag *drag,
                                       const wmEvent *event,
                                       wmDropBox *UNUSED(drop))
{
  const ARegion *region = CTX_wm_region(C);
  const uiTreeViewItemHandle *hovered_tree_item = UI_block_tree_view_find_item_at(region,
                                                                                  event->xy);
  if (!hovered_tree_item) {
    return nullptr;
  }

  return UI_tree_view_item_drop_tooltip(hovered_tree_item, C, drag, event);
}

void ED_dropboxes_ui()
{
  ListBase *lb = WM_dropboxmap_find("User Interface", SPACE_EMPTY, 0);

  WM_dropbox_add(lb,
                 "UI_OT_tree_view_drop",
                 ui_tree_view_drop_poll,
                 nullptr,
                 nullptr,
                 ui_tree_view_drop_tooltip);
}
