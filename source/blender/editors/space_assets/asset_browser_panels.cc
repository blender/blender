/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#include <cstring>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_screen.h"

#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "ED_asset.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "WM_api.hh"

#include "asset_browser_intern.hh"

static void assets_panel_asset_catalog_buttons_draw(const bContext *C, Panel *panel)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceAssets *assets_space = CTX_wm_space_assets(C);

  uiLayout *col = uiLayoutColumn(panel->layout, false);
  uiLayout *row = uiLayoutRow(col, true);

  PointerRNA assets_space_ptr = RNA_pointer_create(
      &screen->id, &RNA_SpaceAssetBrowser, assets_space);

  uiItemR(row, &assets_space_ptr, "asset_library_reference", UI_ITEM_NONE, "", ICON_NONE);
  if (assets_space->asset_library_ref.type == ASSET_LIBRARY_LOCAL) {
    bContext *mutable_ctx = CTX_copy(C);
    if (WM_operator_name_poll(mutable_ctx, "asset.bundle_install")) {
      uiItemS(col);
      uiItemMenuEnumO(col,
                      mutable_ctx,
                      "asset.bundle_install",
                      "asset_library_reference",
                      "Copy Bundle to Asset Library...",
                      ICON_IMPORT);
    }
    CTX_free(mutable_ctx);
  }
  else {
    uiItemO(row, "", ICON_FILE_REFRESH, "ASSET_OT_library_refresh");
  }

  uiItemS(col);

  AssetLibrary *library = ED_assetlist_library_get(&assets_space->asset_library_ref);
  PropertyRNA *catalog_filter_prop = RNA_struct_find_property(&assets_space_ptr, "catalog_filter");

  asset_view_create_catalog_tree_view_in_layout(
      library, col, &assets_space_ptr, catalog_filter_prop, CTX_wm_message_bus(C));
}

void asset_browser_navigation_region_panels_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_cnew<PanelType>("asset browser catalog buttons");
  strcpy(pt->idname, "FILE_PT_asset_catalog_buttons");
  strcpy(pt->label, N_("Asset Catalogs"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->flag = PANEL_TYPE_NO_HEADER;
  pt->draw = assets_panel_asset_catalog_buttons_draw;
  BLI_addtail(&art->paneltypes, pt);
}
