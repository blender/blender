/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.h"

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

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

  if (drag->drop_state.free_disabled_info) {
    MEM_SAFE_FREE(drag->drop_state.disabled_info);
  }

  drag->drop_state.free_disabled_info = false;
  return UI_tree_view_item_can_drop(hovered_tree_item, drag, &drag->drop_state.disabled_info);
}

static char *ui_tree_view_drop_tooltip(bContext *C,
                                       wmDrag *drag,
                                       const int xy[2],
                                       wmDropBox *UNUSED(drop))
{
  const ARegion *region = CTX_wm_region(C);
  const uiTreeViewItemHandle *hovered_tree_item = UI_block_tree_view_find_item_at(region, xy);
  if (!hovered_tree_item) {
    return nullptr;
  }

  return UI_tree_view_item_drop_tooltip(hovered_tree_item, drag);
}

/* ---------------------------------------------------------------------- */

static bool ui_drop_name_poll(struct bContext *C, wmDrag *drag, const wmEvent *UNUSED(event))
{
  return UI_but_active_drop_name(C) && (drag->type == WM_DRAG_ID);
}

static void ui_drop_name_copy(wmDrag *drag, wmDropBox *drop)
{
  const ID *id = WM_drag_get_local_ID(drag, 0);
  RNA_string_set(drop->ptr, "string", id->name + 2);
}

/* ---------------------------------------------------------------------- */

void ED_dropboxes_ui()
{
  ListBase *lb = WM_dropboxmap_find("User Interface", SPACE_EMPTY, 0);

  WM_dropbox_add(lb,
                 "UI_OT_tree_view_drop",
                 ui_tree_view_drop_poll,
                 nullptr,
                 nullptr,
                 ui_tree_view_drop_tooltip);
  WM_dropbox_add(lb,
                 "UI_OT_drop_name",
                 ui_drop_name_poll,
                 ui_drop_name_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
}
