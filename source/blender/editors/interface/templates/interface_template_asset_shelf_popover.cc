/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_resources.hh"
#include "interface_intern.hh"

#include "ED_asset_shelf.hh"

#include "WM_api.hh"

namespace blender::ui {

void template_asset_shelf_popover(uiLayout &layout,
                                  const bContext &C,
                                  const StringRefNull asset_shelf_id,
                                  const StringRefNull name,
                                  const BIFIconID icon)
{
  AssetShelfType *shelf_type = ed::asset::shelf::type_find_from_idname(asset_shelf_id);
  if (!shelf_type) {
    RNA_warning("Asset shelf type not found: %s", asset_shelf_id.c_str());
    return;
  }

  const ARegion *region = CTX_wm_region(&C);
  uiBlock *block = uiLayoutGetBlock(&layout);

  uiLayout *row = uiLayoutRow(&layout, true);
  const bool use_big_size = !RGN_TYPE_IS_HEADER_ANY(region->regiontype);
  const bool use_preview_icon = use_big_size;

  uiLayoutSetContextString(row, "asset_shelf_idname", asset_shelf_id);
  if (use_big_size) {
    uiLayoutSetScaleX(row, 6);
    uiLayoutSetScaleY(row, 6);
  }
  else {
    uiLayoutSetUnitsX(row, name.is_empty() ? 1.6f : 7);
  }

  uiItemPopoverPanel(row, &C, "ASSETSHELF_PT_popover_panel", name.c_str(), icon);
  uiBut *but = static_cast<uiBut *>(block->buttons.last);
  if (use_preview_icon) {
    ui_def_but_icon(but, icon, UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
  }
}

bool asset_shelf_popover_invoke(bContext &C, StringRef asset_shelf_idname, ReportList &reports)
{
  AssetShelfType *shelf_type = ed::asset::shelf::type_find_from_idname(asset_shelf_idname);
  if (ed::asset::shelf::type_poll_for_popup(C, shelf_type) == false) {
    return false;
  }

  PanelType *pt = WM_paneltype_find("ASSETSHELF_PT_popover_panel", true);
  if (pt == nullptr) {
    BKE_reportf(&reports, RPT_ERROR, "Asset shelf popover panel type not found");
    return false;
  }

  /* Skip panel poll check here. Should usually be done, but requires passing the asset shelf type
   * name via some context-store, but there's nothing to provide that here. Asset shelf type is
   * polled above, so it's okay. */

  std::string asset_shelf_id_str = asset_shelf_idname;
  ui_popover_panel_create(
      &C,
      nullptr,
      nullptr,
      [asset_shelf_id_str](bContext *C, uiLayout *layout, void *arg_pt) {
        uiLayoutSetContextString(layout, "asset_shelf_idname", asset_shelf_id_str);
        ui_item_paneltype_func(C, layout, arg_pt);
      },
      pt);

  return true;
}

}  // namespace blender::ui

using namespace blender;

std::optional<StringRefNull> UI_asset_shelf_idname_from_button_context(const uiBut *but)
{
  return UI_but_context_string_get(but, "asset_shelf_idname");
}
