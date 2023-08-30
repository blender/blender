/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */
#include "BKE_context.h"

#include "DNA_asset_types.h"

#include "RNA_access.hh"
#include "RNA_types.hh"

#include "ED_asset.hh"

#include "UI_interface.hh"

#include "WM_message.hh"

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
    item.set_on_activate_fn([this, idx](bContext & /*C*/, ui::PreviewGridItem & /*item*/) {
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
    : ui::PreviewGridItem(ED_asset_handle_get_identifier(&asset),
                          ED_asset_handle_get_name(&asset)),
      /* Get a copy so the identifier is always available (the file data wrapped by the handle may
       * be freed). */
      asset_identifier_(identifier_)
{
  preview_icon_id = ED_assetlist_asset_preview_or_type_icon_id_request(&asset);

  /* Update reference so we don't point into the possibly freed file data. */
  identifier_ = asset_identifier_;

  enable_selectable();
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
  ED_assetlist_catalog_filter_set(&asset_library_ref, &catalog_filter_settings);

  ui::AbstractGridView *grid_view = UI_block_add_view(
      *block,
      "asset grid view",
      std::make_unique<AssetGridView>(asset_library_ref,
                                      active_asset_idx_owner_ptr,
                                      active_asset_idx_prop,
                                      CTX_wm_message_bus(&C)));

  ui::GridViewBuilder builder(*block);
  builder.build_grid_view(*grid_view, v2d, layout);
}

}  // namespace blender::ed::asset_browser
