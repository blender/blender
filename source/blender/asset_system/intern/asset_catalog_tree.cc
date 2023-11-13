/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "AS_asset_catalog_tree.hh"

namespace blender::asset_system {

AssetCatalogTreeItem::AssetCatalogTreeItem(StringRef name,
                                           CatalogID catalog_id,
                                           StringRef simple_name,
                                           const AssetCatalogTreeItem *parent)
    : name_(name), catalog_id_(catalog_id), simple_name_(simple_name), parent_(parent)
{
}

CatalogID AssetCatalogTreeItem::get_catalog_id() const
{
  return catalog_id_;
}

StringRefNull AssetCatalogTreeItem::get_name() const
{
  return name_;
}

StringRefNull AssetCatalogTreeItem::get_simple_name() const
{
  return simple_name_;
}
bool AssetCatalogTreeItem::has_unsaved_changes() const
{
  return has_unsaved_changes_;
}

AssetCatalogPath AssetCatalogTreeItem::catalog_path() const
{
  AssetCatalogPath current_path = name_;
  for (const AssetCatalogTreeItem *parent = parent_; parent; parent = parent->parent_) {
    current_path = AssetCatalogPath(parent->name_) / current_path;
  }
  return current_path;
}

int AssetCatalogTreeItem::count_parents() const
{
  int i = 0;
  for (const AssetCatalogTreeItem *parent = parent_; parent; parent = parent->parent_) {
    i++;
  }
  return i;
}

bool AssetCatalogTreeItem::has_children() const
{
  return !children_.empty();
}

void AssetCatalogTreeItem::foreach_item_recursive(AssetCatalogTreeItem::ChildMap &children,
                                                  const ItemIterFn callback)
{
  for (auto &[key, item] : children) {
    callback(item);
    foreach_item_recursive(item.children_, callback);
  }
}

void AssetCatalogTreeItem::foreach_child(const ItemIterFn callback)
{
  for (auto &[key, item] : children_) {
    callback(item);
  }
}

/* ---------------------------------------------------------------------- */

void AssetCatalogTree::insert_item(const AssetCatalog &catalog)
{
  const AssetCatalogTreeItem *parent = nullptr;
  /* The children for the currently iterated component, where the following component should be
   * added to (if not there yet). */
  AssetCatalogTreeItem::ChildMap *current_item_children = &root_items_;

  BLI_assert_msg(!ELEM(catalog.path.str()[0], '/', '\\'),
                 "Malformed catalog path; should not start with a separator");

  const CatalogID nil_id{};

  catalog.path.iterate_components([&](StringRef component_name, const bool is_last_component) {
    /* Insert new tree element - if no matching one is there yet! */
    auto [key_and_item, was_inserted] = current_item_children->emplace(
        component_name,
        AssetCatalogTreeItem(component_name,
                             is_last_component ? catalog.catalog_id : nil_id,
                             is_last_component ? catalog.simple_name : "",
                             parent));
    AssetCatalogTreeItem &item = key_and_item->second;

    /* If full path of this catalog already exists as parent path of a previously read catalog,
     * we can ensure this tree item's UUID is set here. */
    if (is_last_component) {
      if (BLI_uuid_is_nil(item.catalog_id_) || catalog.flags.is_first_loaded) {
        item.catalog_id_ = catalog.catalog_id;
      }
      item.has_unsaved_changes_ = catalog.flags.has_unsaved_changes;
    }

    /* Walk further into the path (no matter if a new item was created or not). */
    parent = &item;
    current_item_children = &item.children_;
  });
}

void AssetCatalogTree::foreach_item(AssetCatalogTreeItem::ItemIterFn callback)
{
  AssetCatalogTreeItem::foreach_item_recursive(root_items_, callback);
}

void AssetCatalogTree::foreach_root_item(const ItemIterFn callback)
{
  for (auto &[key, item] : root_items_) {
    callback(item);
  }
}

bool AssetCatalogTree::is_empty() const
{
  return root_items_.empty();
}

AssetCatalogTreeItem *AssetCatalogTree::find_item(const AssetCatalogPath &path)
{
  AssetCatalogTreeItem *result = nullptr;
  this->foreach_item([&](AssetCatalogTreeItem &item) {
    if (result) {
      /* There is no way to stop iteration. */
      return;
    }
    if (item.catalog_path() == path) {
      result = &item;
    }
  });
  return result;
}

AssetCatalogTreeItem *AssetCatalogTree::find_root_item(const AssetCatalogPath &path)
{
  AssetCatalogTreeItem *result = nullptr;
  this->foreach_root_item([&](AssetCatalogTreeItem &item) {
    if (result) {
      /* There is no way to stop iteration. */
      return;
    }
    if (item.catalog_path() == path) {
      result = &item;
    }
  });
  return result;
}

}  // namespace blender::asset_system
