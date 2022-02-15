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
#include "BKE_context.h"

#include <iostream>

#include "DNA_asset_types.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "ED_asset.h"

#include "UI_interface.h"
#include "UI_interface.hh"

#include "WM_message.h"

#include "asset_view.hh"

namespace blender::ed::asset_browser {

AssetGridView::AssetGridView(const AssetLibraryReference &asset_library_ref,
                             const PointerRNA &active_asset_idx_owner_ptr,
                             PropertyRNA *active_asset_idx_prop,
                             wmMsgBus *msg_bus)
    : asset_library_ref_(asset_library_ref),
      active_asset_idx_owner_(active_asset_idx_owner_ptr),
      active_asset_idx_prop_(*active_asset_idx_prop),
      msg_bus_(*msg_bus)
{
}

void AssetGridView::build_items()
{
  int idx = 0;
  ED_assetlist_iterate(asset_library_ref_, [this, &idx](AssetHandle &asset) {
    AssetGridViewItem &item = add_item<AssetGridViewItem>(asset);

    item.set_is_active_fn([this, idx]() -> bool {
      return idx == RNA_property_int_get(&active_asset_idx_owner_, &active_asset_idx_prop_);
    });
    item.set_on_activate_fn([this, idx](ui::PreviewGridItem & /*item*/) {
      RNA_property_int_set(&active_asset_idx_owner_, &active_asset_idx_prop_, idx);
      WM_msg_publish_rna(&msg_bus_, &active_asset_idx_owner_, &active_asset_idx_prop_);
    });

    idx++;
    return true;
  });
}

bool AssetGridView::listen(const wmNotifier &notifier) const
{
  return ED_assetlist_listen(&asset_library_ref_, &notifier);
}

/* ---------------------------------------------------------------------- */

AssetGridViewItem::AssetGridViewItem(AssetHandle &asset)
    : ui::PreviewGridItem(ED_asset_handle_get_name(&asset),
                          ED_asset_handle_get_preview_icon_id(&asset)),
      asset_(asset),
      asset_identifier(ED_asset_handle_get_identifier(&asset))
{
}

bool AssetGridViewItem::matches(const ui::AbstractGridViewItem &other) const
{
  const AssetGridViewItem &other_item = dynamic_cast<const AssetGridViewItem &>(other);
  return StringRef(asset_identifier) == StringRef(other_item.asset_identifier);
}

AssetHandle &AssetGridViewItem::get_asset()
{
  return asset_;
}

/* ---------------------------------------------------------------------- */

void asset_view_create_in_layout(const bContext &C,
                                 const AssetLibraryReference &asset_library_ref,
                                 const AssetCatalogFilterSettings &catalog_filter_settings,
                                 const PointerRNA &active_asset_idx_owner_ptr,
                                 PropertyRNA *active_asset_idx_prop,
                                 const View2D &v2d,
                                 uiLayout &layout)
{
  uiBlock *block = uiLayoutGetBlock(&layout);
  UI_block_layout_set_current(block, &layout);

  ED_assetlist_storage_fetch(&asset_library_ref, &C);
  ED_assetlist_ensure_previews_job(&asset_library_ref, &C);
  ED_assetlist_catalog_filter_set(&asset_library_ref, &catalog_filter_settings);

  ui::AbstractGridView *grid_view = UI_block_add_view(
      *block,
      "asset grid view",
      std::make_unique<AssetGridView>(asset_library_ref,
                                      active_asset_idx_owner_ptr,
                                      active_asset_idx_prop,
                                      CTX_wm_message_bus(&C)));

  ui::GridViewBuilder builder(*block);
  builder.build_grid_view(*grid_view, v2d);
}

}  // namespace blender::ed::asset_browser
