/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase.hh"

#include "DNA_key_types.h"
#include "DNA_outliner_types.h"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_shapekey.hh"

namespace blender::ed::outliner {

TreeElementShapeKeyBase::TreeElementShapeKeyBase(TreeElement &legacy_te, Key &key)
    : AbstractTreeElement(legacy_te), key_(key)
{
  BLI_assert(legacy_te.store_elem->type == TSE_SHAPE_KEY_BASE);
  legacy_te.name = key_.id.name + 2;
}

void TreeElementShapeKeyBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  for (auto [index, keyblock] : key_.block.enumerate()) {
    add_element(&legacy_te_.subtree,
                &key_.id,
                const_cast<KeyBlock *>(&keyblock),
                &legacy_te_,
                TSE_SHAPE_KEY_BLOCK,
                index);
  }
}

TreeElementShapeKey::TreeElementShapeKey(TreeElement &legacy_te, KeyBlock &keyblock)
    : AbstractTreeElement(legacy_te), keyblock_(keyblock)
{
  BLI_assert(legacy_te.store_elem->type == TSE_SHAPE_KEY_BLOCK);
  legacy_te.name = keyblock_.name;
  legacy_te.directdata = &keyblock_;
}

}  // namespace blender::ed::outliner
