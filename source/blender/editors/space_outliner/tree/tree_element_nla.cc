/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase_wrapper.hh"

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"

#include "tree_element_nla.hh"

namespace blender::ed::outliner {

TreeElementNLA::TreeElementNLA(TreeElement &legacy_te, AnimData &anim_data)
    : AbstractTreeElement(legacy_te), anim_data_(anim_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_NLA);
  legacy_te.name = IFACE_("NLA Tracks");
  legacy_te.directdata = &anim_data;
}

void TreeElementNLA::expand(SpaceOutliner & /*space_outliner*/) const
{
  int a = 0;
  for (NlaTrack *nlt : ListBaseWrapper<NlaTrack>(anim_data_.nla_tracks)) {
    add_element(&legacy_te_.subtree, nullptr, nlt, &legacy_te_, TSE_NLA_TRACK, a);
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

void TreeElementNLATrack::expand(SpaceOutliner & /*space_outliner*/) const
{
  int a = 0;
  for (NlaStrip *strip : ListBaseWrapper<NlaStrip>(track_.strips)) {
    add_element(&legacy_te_.subtree,
                reinterpret_cast<ID *>(strip->act),
                nullptr,
                &legacy_te_,
                TSE_NLA_ACTION,
                a);
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
