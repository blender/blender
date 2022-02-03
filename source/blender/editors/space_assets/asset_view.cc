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

#include <iostream>

#include "DNA_asset_types.h"

#include "ED_asset.h"

#include "UI_interface.h"
#include "UI_interface.hh"

#include "asset_view.hh"

namespace ui = blender::ui;

namespace blender::ed::asset_browser {

AssetGridView::AssetGridView(const AssetLibraryReference &asset_library_ref, uiLayout &layout)
    : asset_library_ref_(asset_library_ref), layout(layout)
{
}

void AssetGridView::build()
{
  ED_assetlist_iterate(asset_library_ref_, [this](AssetHandle asset) {
    uiItemL(
        &layout, ED_asset_handle_get_name(&asset), ED_asset_handle_get_preview_icon_id(&asset));
    return true;
  });
}

bool AssetGridView::listen(const wmNotifier &notifier) const
{
  return ED_assetlist_listen(&asset_library_ref_, &notifier);
}

void asset_view_create_in_layout(const bContext &C,
                                 const AssetLibraryReference &asset_library_ref,
                                 uiLayout &layout)
{
  uiBlock *block = uiLayoutGetBlock(&layout);

  ED_assetlist_storage_fetch(&asset_library_ref, &C);
  ED_assetlist_ensure_previews_job(&asset_library_ref, &C);

  UI_block_layout_set_current(block, &layout);

  ui::AbstractGridView *grid_view = UI_block_add_view(
      *block, "asset grid view", std::make_unique<AssetGridView>(asset_library_ref, layout));

  grid_view->build();
}

}  // namespace blender::ed::asset_browser
