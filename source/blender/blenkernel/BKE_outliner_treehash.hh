/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * Hash table of tree-store elements (#TreeStoreElem) for fast lookups via a (id, type, index)
 * tuple as key.
 *
 * The Outliner may have to perform many lookups for rebuilding complex trees, so this should be
 * treated as performance sensitive.
 */

#include <memory>

#include "BLI_map.hh"

struct BLI_mempool;
struct ID;
struct TreeStoreElem;

namespace blender::bke::outliner::treehash {

/* -------------------------------------------------------------------- */

class TreeStoreElemKey {
 public:
  ID *id = nullptr;
  short type = 0;
  short nr = 0;

  explicit TreeStoreElemKey(const TreeStoreElem &elem);
  TreeStoreElemKey(ID *id, short type, short nr);

  uint64_t hash() const;
  friend bool operator==(const TreeStoreElemKey &a, const TreeStoreElemKey &b);
};

/* -------------------------------------------------------------------- */

class TreeHash {
  Map<TreeStoreElemKey, std::unique_ptr<class TseGroup>> elem_groups_;

 public:
  ~TreeHash();

  /** Create and fill hash-table with treestore elements */
  static std::unique_ptr<TreeHash> create_from_treestore(BLI_mempool &treestore);

  /** Full rebuild for already allocated hash-table. */
  void rebuild_from_treestore(BLI_mempool &treestore);

  /** Clear element usage flags. */
  void clear_used();

  /** Add hash-table element. */
  void add_element(TreeStoreElem &elem);
  /** Remove hash-table element. */
  void remove_element(TreeStoreElem &elem);

  /** Find first unused element with specific type, nr and id. */
  TreeStoreElem *lookup_unused(short type, short nr, ID *id) const;

  /** Find user or unused element with specific type, nr and id. */
  TreeStoreElem *lookup_any(short type, short nr, ID *id) const;

 private:
  TreeHash() = default;

  TseGroup *lookup_group(const TreeStoreElemKey &key) const;
  TseGroup *lookup_group(const TreeStoreElem &elem) const;
  TseGroup *lookup_group(short type, short nr, ID *id) const;
  void fill_treehash(BLI_mempool &treestore);
};

}  // namespace blender::bke::outliner::treehash
