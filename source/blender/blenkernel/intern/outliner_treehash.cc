/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Tree hash for the outliner space.
 */

#include <cstdlib>
#include <cstring>

#include "BLI_mempool.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_outliner_types.h"

#include "BKE_outliner_treehash.hh"

#include "MEM_guardedalloc.h"

namespace blender::bke::outliner::treehash {

/* -------------------------------------------------------------------- */
/** \name #TseGroup
 * \{ */

class TseGroup {
 public:
  blender::Vector<TreeStoreElem *> elems;
  /* Index of last used #TreeStoreElem item, to speed up search for another one. */
  int lastused = 0;
  /* Counter used to reduce the amount of 'rests' of `lastused` index, otherwise search for unused
   * item is exponential and becomes critically slow when there are a lot of items in the group. */
  int lastused_reset_count = -1;

  void add_element(TreeStoreElem &elem);
  void remove_element(TreeStoreElem &elem);
};

/* Only allow reset of #TseGroup.lastused counter to 0 once every 1k search. */
#define TSEGROUP_LASTUSED_RESET_VALUE 10000

void TseGroup::add_element(TreeStoreElem &elem)
{
  const int64_t idx = elems.append_and_get_index(&elem);
  lastused = idx;
}

void TseGroup::remove_element(TreeStoreElem &elem)
{
  const int64_t idx = elems.first_index_of(&elem);
  elems.remove(idx);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #TreeStoreElemKey
 * \{ */

TreeStoreElemKey::TreeStoreElemKey(const TreeStoreElem &elem)
    : id(elem.id), type(elem.type), nr(elem.nr)
{
}

TreeStoreElemKey::TreeStoreElemKey(ID *id, short type, short nr) : id(id), type(type), nr(nr) {}

uint64_t TreeStoreElemKey::hash() const
{
  return get_default_hash_3(id, type, nr);
}

bool operator==(const TreeStoreElemKey &a, const TreeStoreElemKey &b)
{
  return (a.id == b.id) && (a.type == b.type) && (a.nr == b.nr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #TreeHash
 * \{ */

TreeHash::~TreeHash() = default;

std::unique_ptr<TreeHash> TreeHash::create_from_treestore(BLI_mempool &treestore)
{
  /* Can't use `make_unique()` here because of private constructor. */
  std::unique_ptr<TreeHash> tree_hash{new TreeHash()};
  tree_hash->fill_treehash(treestore);

  return tree_hash;
}

void TreeHash::fill_treehash(BLI_mempool &treestore)
{
  TreeStoreElem *tselem;
  BLI_mempool_iter iter;
  BLI_mempool_iternew(&treestore, &iter);

  while ((tselem = static_cast<TreeStoreElem *>(BLI_mempool_iterstep(&iter)))) {
    add_element(*tselem);
  }
}

void TreeHash::clear_used()
{
  for (auto &group : elem_groups_.values()) {
    group->lastused = 0;
    group->lastused_reset_count = 0;
  }
}

void TreeHash::rebuild_from_treestore(BLI_mempool &treestore)
{
  elem_groups_.clear();
  fill_treehash(treestore);
}

void TreeHash::add_element(TreeStoreElem &elem)
{
  std::unique_ptr<TseGroup> &group = elem_groups_.lookup_or_add_cb(
      TreeStoreElemKey(elem), []() { return std::make_unique<TseGroup>(); });
  group->add_element(elem);
}

void TreeHash::remove_element(TreeStoreElem &elem)
{
  TseGroup *group = lookup_group(elem);
  BLI_assert(group != nullptr);

  if (group->elems.size() <= 1) {
    /* One element -> remove group completely. */
    elem_groups_.remove(TreeStoreElemKey(elem));
  }
  else {
    group->remove_element(elem);
  }
}

TseGroup *TreeHash::lookup_group(const TreeStoreElemKey &key) const
{
  const auto *group = elem_groups_.lookup_ptr(key);
  if (group) {
    return group->get();
  }
  return nullptr;
}

TseGroup *TreeHash::lookup_group(const TreeStoreElem &key_elem) const
{
  return lookup_group(TreeStoreElemKey(key_elem));
}

TseGroup *TreeHash::lookup_group(const short type, const short nr, ID *id) const
{
  TreeStoreElemKey key(id, type, nr);
  if (type == TSE_SOME_ID) {
    key.nr = 0; /* we're picky! :) */
  }
  return lookup_group(key);
}

TreeStoreElem *TreeHash::lookup_unused(const short type, const short nr, ID *id) const
{
  TseGroup *group = lookup_group(type, nr, id);
  if (!group) {
    return nullptr;
  }

  /* Find unused element, with optimization to start from previously
   * found element assuming we do repeated lookups. */
  const int size = group->elems.size();
  int offset = group->lastused;

  for (int i = 0; i < size; i++, offset++) {
    /* Once at the end of the array of items, in most cases it just means that all items are
     * used, so only check the whole array once every TSEGROUP_LASTUSED_RESET_VALUE times. */
    if (offset >= size) {
      if (LIKELY(group->lastused_reset_count <= TSEGROUP_LASTUSED_RESET_VALUE)) {
        group->lastused_reset_count++;
        group->lastused = group->elems.size() - 1;
        break;
      }
      group->lastused_reset_count = 0;
      offset = 0;
    }

    if (!group->elems[offset]->used) {
      group->lastused = offset;
      return group->elems[offset];
    }
  }
  return nullptr;
}

TreeStoreElem *TreeHash::lookup_any(const short type, const short nr, ID *id) const
{
  const TseGroup *group = lookup_group(type, nr, id);
  return group ? group->elems[0] : nullptr;
}

/** \} */

}  // namespace blender::bke::outliner::treehash
