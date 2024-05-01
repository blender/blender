/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"

#include "UI_interface_c.hh"
#include "UI_resources.hh"
#include "interface_intern.hh"

#include "ED_asset_shelf.hh"

namespace blender::ui {

static uiBlock *asset_shelf_block_fn(bContext *C, ARegion *region, void *arg_shelf_type)
{
  AssetShelfType *shelf_type = reinterpret_cast<AssetShelfType *>(arg_shelf_type);
  return ed::asset::shelf::popup_block_create(C, region, shelf_type);
}

void template_asset_shelf_popover(uiLayout *layout,
                                  const bContext *C,
                                  const StringRefNull asset_shelf_id,
                                  const StringRef name,
                                  const BIFIconID icon)
{
  const ScrArea *area = CTX_wm_area(C);
  AssetShelfType *shelf_type = ed::asset::shelf::type_find_from_idname(*area->type,
                                                                       asset_shelf_id);
  if (!shelf_type) {
    RNA_warning("Asset shelf type not found: %s", asset_shelf_id.c_str());
    return;
  }

  const ARegion *region = CTX_wm_region(C);
  const bool use_big_size = !RGN_TYPE_IS_HEADER_ANY(region->regiontype);
  const short width = [&]() -> short {
    if (use_big_size) {
      return UI_UNIT_X * 6;
    }
    return UI_UNIT_X * (name.is_empty() ? 7 : 1.6f);
  }();
  const short height = UI_UNIT_Y * (use_big_size ? 6 : 1);

  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = uiDefBlockBut(
      block, asset_shelf_block_fn, shelf_type, name, 0, 0, width, height, "Select an asset");
  ui_def_but_icon(but, icon, UI_HAS_ICON);
  UI_but_drawflag_enable(but, UI_BUT_ICON_LEFT);

  if (ed::asset::shelf::type_poll(*C, *area->type, shelf_type) == false) {
    UI_but_flag_enable(but, UI_BUT_DISABLED);
  }
}

}  // namespace blender::ui
