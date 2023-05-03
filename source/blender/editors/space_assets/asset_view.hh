/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#pragma once

#include "DNA_asset_types.h"

#include "UI_grid_view.hh"

struct AssetCatalogFilterSettings;
struct bContext;
struct PointerRNA;
struct PropertyRNA;
struct uiLayout;
struct View2D;
struct wmMsgBus;

namespace blender::ed::asset_browser {

class AssetGridView : public blender::ui::AbstractGridView {
  AssetLibraryReference asset_library_ref_;

  /** Reference to bind the active asset of the editor to the view. */
  PointerRNA active_asset_idx_owner_;
  PropertyRNA &active_asset_idx_prop_;
  wmMsgBus &msg_bus_;

 public:
  AssetGridView(const AssetLibraryReference &,
                const PointerRNA &active_asset_idx_owner_ptr,
                PropertyRNA *active_asset_idx_prop,
                wmMsgBus *msg_bus);

  void build_items() override;
  bool listen(const wmNotifier &) const override;
};

class AssetGridViewItem : public ui::PreviewGridItem {
  /* Can't store this here, since the wrapped FileDirEntry will be freed while progressively
   * loading items. */
  // AssetHandle &asset_;
  std::string asset_identifier_;

 public:
  AssetGridViewItem(AssetHandle &);
};

void asset_view_create_in_layout(const bContext &C,
                                 const AssetLibraryReference &asset_library_ref,
                                 const AssetCatalogFilterSettings &catalog_filter_settings,
                                 const PointerRNA &active_asset_idx_owner_ptr,
                                 PropertyRNA *active_asset_idx_prop,
                                 const View2D &v2d,
                                 uiLayout &layout);

}  // namespace blender::ed::asset_browser
