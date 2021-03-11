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

#include "tree_element_nla.hh"

namespace blender::ed::outliner {

TreeElementNLA::TreeElementNLA(TreeElement &legacy_te, AnimData &anim_data)
    : AbstractTreeElement(legacy_te), anim_data_(anim_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_NLA);
  legacy_te.name = IFACE_("NLA Tracks");
  legacy_te.directdata = &anim_data;
}

void TreeElementNLA::expand(SpaceOutliner &space_outliner) const
{
  int a = 0;
  for (NlaTrack *nlt : ListBaseWrapper<NlaTrack>(anim_data_.nla_tracks)) {
    outliner_add_element(&space_outliner, &legacy_te_.subtree, nlt, &legacy_te_, TSE_NLA_TRACK, a);
    a++;
  }
}

/* -------------------------------------------------------------------- */

TreeElementNLATrack::TreeElementNLATrack(TreeElement &legacy_te, NlaTrack &track)
    : AbstractTreeElement(legacy_te), track_(track)
{
  BLI_assert(legacy_te.store_elem->type == TSE_NLA_TRACK);
  legacy_te.name = track.name;
}

void TreeElementNLATrack::expand(SpaceOutliner &space_outliner) const
{
  int a = 0;
  for (NlaStrip *strip : ListBaseWrapper<NlaStrip>(track_.strips)) {
    outliner_add_element(
        &space_outliner, &legacy_te_.subtree, strip->act, &legacy_te_, TSE_NLA_ACTION, a);
    a++;
  }
}

/* -------------------------------------------------------------------- */

TreeElementNLAAction::TreeElementNLAAction(TreeElement &legacy_te, const bAction &action)
    : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_NLA_ACTION);
  legacy_te.name = action.id.name + 2;
}

}  // namespace blender::ed::outliner
