/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * A representation of the catalog paths as tree structure. Each component of the catalog tree is
 * represented by an #AssetCatalogTreeItem. The last path component of an item is used as its name,
 * which may also be shown to the user.
 * An item can not have multiple children with the same name. That means the name uniquely
 * identifies an item within its parent.
 *
 * There is no single root tree element, the #AssetCatalogTree instance itself represents the root.
 */

#pragma once

#include <map>
#include <optional>

#include "AS_asset_catalog.hh"

namespace blender::asset_system {

/**
 * Representation of a catalog path in the #AssetCatalogTree.
 */
class AssetCatalogTreeItem {
 public:
  /** Container for child items. Uses a #std::map to keep items ordered by their name (i.e. their
   * last catalog component). */
  using ChildMap = std::map<std::string, AssetCatalogTreeItem>;
  using ItemIterFn = FunctionRef<void(const AssetCatalogTreeItem &)>;

 private:
  /** Child tree items, ordered by their names. */
  ChildMap children_;
  /** The user visible name of this component. */
  CatalogPathComponent name_;
  CatalogID catalog_id_;
  /** Copy of #AssetCatalog::simple_name. */
  std::string simple_name_;
  /** Copy of #AssetCatalog::flags.has_unsaved_changes. */
  bool has_unsaved_changes_ = false;

  /** Pointer back to the parent item. Used to reconstruct the hierarchy from an item (e.g. to
   * build a path). */
  const AssetCatalogTreeItem *parent_ = nullptr;

  friend class AssetCatalogTree;

 public:
  AssetCatalogTreeItem(StringRef name,
                       CatalogID catalog_id,
                       StringRef simple_name,
                       const AssetCatalogTreeItem *parent = nullptr);

  CatalogID get_catalog_id() const;
  StringRefNull get_simple_name() const;
  StringRefNull get_name() const;
  bool has_unsaved_changes() const;
  /** Return the full catalog path, defined as the name of this catalog prefixed by the full
   * catalog path of its parent and a separator. */
  AssetCatalogPath catalog_path() const;
  int count_parents() const;
  bool has_children() const;

  /** Iterate over children calling \a callback for each of them, but do not recurse into their
   * children. */
  void foreach_child(ItemIterFn callback) const;
  void foreach_item(ItemIterFn callback) const;

 private:
  static void foreach_item_recursive(const ChildMap &children_, ItemIterFn callback);
};

class AssetCatalogTree {
  using ChildMap = AssetCatalogTreeItem::ChildMap;
  using ItemIterFn = AssetCatalogTreeItem::ItemIterFn;

  /** Child tree items, ordered by their names. */
  ChildMap root_items_;

 public:
  /**
   * Ensure an item representing \a catalog is in the tree, adding it if necessary.
   *
   * \param skip_prefix: If set and the catalog path starts with this prefix path, the prefix path
   *    will be stripped, and the catalog will be inserted into the tree as if it started after
   *    this prefix. For example if the path of \a catalog is "Lorem ipsum/dolor/sit", and \a
   *    skip_prefix is set to "Lorem ipsum/dolor", then the catalog will be inserted as if the path
   *    was "sit". Catalogs whose path do not start with the prefix will be unaffected.
   */
  void insert_item(const AssetCatalog &catalog,
                   std::optional<StringRef> skip_prefix = std::nullopt);

  void foreach_item(ItemIterFn callback) const;
  /** Iterate over root items calling \a callback for each of them, but do not recurse into their
   * children. */
  void foreach_root_item(ItemIterFn callback) const;

  bool is_empty() const;

  const AssetCatalogTreeItem *find_item(const AssetCatalogPath &path) const;
  const AssetCatalogTreeItem *find_root_item(const AssetCatalogPath &path) const;
};

}  // namespace blender::asset_system
