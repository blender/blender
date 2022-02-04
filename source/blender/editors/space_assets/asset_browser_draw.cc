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
 * \ingroup spassets
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "asset_browser_intern.hh"
#include "asset_view.hh"

namespace blender::ed::asset_browser {

}  // namespace blender::ed::asset_browser

using namespace blender::ed::asset_browser;

void asset_browser_main_region_draw(const bContext *C, ARegion *region)
{
  const SpaceAssets *asset_space = CTX_wm_space_assets(C);
  View2D *v2d = &region->v2d;

  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  const uiStyle *style = UI_style_get_dpi();
  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, style->panelspace, 0, region->winx, 1, 0, style);

  asset_view_create_in_layout(*C, asset_space->asset_library_ref, *layout);

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
