/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase_wrapper.hh"

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_outliner_types.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_anim_data.hh"

namespace blender::ed::outliner {

TreeElementAnimData::TreeElementAnimData(TreeElement &legacy_te, AnimData &anim_data)
    : AbstractTreeElement(legacy_te), anim_data_(anim_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_ANIM_DATA);
  /* this element's info */
  legacy_te.name = IFACE_("Animation");
  legacy_te.directdata = &anim_data_;
}

void TreeElementAnimData::expand(SpaceOutliner & /*space_outliner*/) const
{
  if (!anim_data_.action) {
    return;
  }

  /* Animation data-block itself. */
  add_element(&legacy_te_.subtree,
              reinterpret_cast<ID *>(anim_data_.action),
              nullptr,
              &legacy_te_,
              TSE_SOME_ID,
              0);

  expand_drivers();
  expand_NLA_tracks();
}

void TreeElementAnimData::expand_drivers() const
{
  if (BLI_listbase_is_empty(&anim_data_.drivers)) {
    return;
  }
  add_element(&legacy_te_.subtree, nullptr, &anim_data_, &legacy_te_, TSE_DRIVER_BASE, 0);
}

void TreeElementAnimData::expand_NLA_tracks() const
{
  if (BLI_listbase_is_empty(&anim_data_.nla_tracks)) {
    return;
  }
  add_element(&legacy_te_.subtree, nullptr, &anim_data_, &legacy_te_, TSE_NLA, 0);
}

}  // namespace blender::ed::outliner
