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

#pragma once

#include "DNA_asset_types.h"

#include "UI_grid_view.hh"

struct AssetCatalogFilterSettings;
struct bContext;
struct uiLayout;
struct View2D;

namespace blender::ed::asset_browser {

class AssetGridView : public blender::ui::AbstractGridView {
  AssetLibraryReference asset_library_ref_;

 public:
  AssetGridView(const AssetLibraryReference &);

  void build_items() override;
  bool listen(const wmNotifier &) const override;
};

void asset_view_create_in_layout(const bContext &C,
                                 const AssetLibraryReference &asset_library_ref,
                                 const AssetCatalogFilterSettings &catalog_filter_settings,
                                 const View2D &v2d,
                                 uiLayout &layout);

}  // namespace blender::ed::asset_browser
