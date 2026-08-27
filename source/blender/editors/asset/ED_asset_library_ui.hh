/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#pragma once

#include "UI_tree_view.hh"

namespace blender {

struct AnyAssetLibraryDefinition {
  eAssetLibraryType type;
  bUserAssetLibrary *user_library;
};

struct AssetLibraryListItemCommon : public ui::AbstractTreeViewItem {
  AnyAssetLibraryDefinition library;
  int index_in_list = 0;

  AssetLibraryListItemCommon(const AnyAssetLibraryDefinition &library, const int index_in_list);

  bool supports_renaming() const override;
  bool rename(const bContext &C, StringRefNull new_name) override;
};

template<typename AssetLibraryListItemType> struct AssetLibraryList : public ui::AbstractTreeView {
  Vector<AnyAssetLibraryDefinition> libraries;

  AssetLibraryList(const Vector<AnyAssetLibraryDefinition> libraries) : libraries(libraries) {};

  void build_tree() override
  {
    this->is_flat_ = true;

    int i = 0;
    for (const AnyAssetLibraryDefinition &library : libraries) {
      add_tree_item<AssetLibraryListItemType>(library, i++);
    }
  }
};

template<typename AssetLibraryListItemType>
void draw_library_list(const bContext &C,
                       ui::Layout &layout,
                       Vector<AnyAssetLibraryDefinition> &libraries,
                       StringRef view_description)
{
  ui::Block *block = layout.block();

  ui::AbstractTreeView *tree_view = ui::block_add_view(
      *block,
      view_description,
      std::make_unique<AssetLibraryList<AssetLibraryListItemType>>(libraries));
  tree_view->set_default_rows(5);

  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout);
}

void draw_active_library_settings(const bContext *C,
                                  ui::Layout &layout,
                                  const AnyAssetLibraryDefinition &library);

}  // namespace blender
