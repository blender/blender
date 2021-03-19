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
 * \ingroup spoutliner
 */

#include "BLI_listbase_wrapper.hh"

#include "DNA_anim_types.h"
#include "DNA_listBase.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.h"

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

void TreeElementAnimData::expand(SpaceOutliner &space_outliner) const
{
  /* Animation data-block itself. */
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, anim_data_.action, &legacy_te_, TSE_SOME_ID, 0);

  expand_drivers(space_outliner);
  expand_NLA_tracks(space_outliner);
}

void TreeElementAnimData::expand_drivers(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&anim_data_.drivers)) {
    return;
  }
  outliner_add_element(
      &space_outliner, &legacy_te_.subtree, &anim_data_, &legacy_te_, TSE_DRIVER_BASE, 0);
}

void TreeElementAnimData::expand_NLA_tracks(SpaceOutliner &space_outliner) const
{
  if (BLI_listbase_is_empty(&anim_data_.nla_tracks)) {
    return;
  }
  outliner_add_element(&space_outliner, &legacy_te_.subtree, &anim_data_, &legacy_te_, TSE_NLA, 0);
}

}  // namespace blender::ed::outliner
