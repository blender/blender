/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "asset_browser_intern.hh"
#include "asset_view.hh"

namespace blender::ed::asset_browser {

}  // namespace blender::ed::asset_browser

using namespace blender::ed::asset_browser;

void asset_browser_main_region_draw(const bContext *C, ARegion *region)
{
  SpaceAssets *asset_space = CTX_wm_space_assets(C);
  bScreen *screen = CTX_wm_screen(C);
  View2D *v2d = &region->v2d;

  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  const uiStyle *style = UI_style_get_dpi();
  const float padding = style->panelouter;
  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  uiLayout *layout = UI_block_layout(
      block,
      UI_LAYOUT_VERTICAL,
      UI_LAYOUT_PANEL,
      padding,
      -padding,
      /* 3x (instead of 2x) padding to add extra space for the scrollbar on the right. */
      region->winx - 3 * padding,
      1,
      0,
      style);

  PointerRNA asset_space_ptr = RNA_pointer_create(
      &screen->id, &RNA_SpaceAssetBrowser, asset_space);
  PropertyRNA *active_asset_idx_prop = RNA_struct_find_property(&asset_space_ptr,
                                                                "active_asset_idx");

  asset_view_create_in_layout(*C,
                              asset_space->asset_library_ref,
                              asset_space->catalog_filter,
                              asset_space_ptr,
                              active_asset_idx_prop,
                              *v2d,
                              *layout);

  /* Update main region View2d dimensions. */
  int layout_width, layout_height;
  UI_block_layout_resolve(block, &layout_width, &layout_height);
  UI_view2d_totRect_set(v2d, layout_width, layout_height);

  UI_block_end(C, block);
  UI_block_draw(C, block);

  /* reset view matrix */
  UI_view2d_view_restore(C);
  UI_view2d_scrollers_draw(v2d, nullptr);
}
