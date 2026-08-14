/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_anim_types.h"
#include "DNA_outliner_types.h"

#include "BLI_listbase.hh"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_anim_data.hh"
#include "tree_element_driver.hh"
#include "tree_element_nla.hh"

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

  if (anim_data_.action) {
    /* Animation data-block itself. */
    add_id_element({}, reinterpret_cast<ID *>(anim_data_.action));
  }

  expand_drivers();
  expand_NLA_tracks();
}

animrig::slot_handle_t TreeElementAnimData::get_slot_handle() const
{
  return this->anim_data_.slot_handle;
}

void TreeElementAnimData::expand_drivers() const
{
  if (anim_data_.drivers.is_empty()) {
    return;
  }
  add_element<TreeElementDriverBase>({}, anim_data_);
}

void TreeElementAnimData::expand_NLA_tracks() const
{
  if (anim_data_.nla_tracks.is_empty()) {
    return;
  }
  add_element<TreeElementNLA>({}, anim_data_);
}

}  // namespace blender::ed::outliner
