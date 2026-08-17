/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_set.hh"

#include "DNA_ID.h"

#include "RNA_access.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"

#include "ED_anim_filter.hh"


namespace blender::ed::animfilter {

static void collect_animation_channels_for_id(ID &animated_id, Set<DataChannel> &r_channels)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  if (!adt) {
    return;
  }
}

void collect_action_channels(Set<DataChannel> &r_channels)
{
  /* TODO */
}

void collect_animation_channels(bContext &C,
                                const CollectionFilters &collection_filters,
                                Set<DataChannel> &r_channels)
{
  Vector<PointerRNA> objects;
  if (collection_filters.only_selected) {
    CTX_data_selected_objects(&C, &objects);
  }
  else {
    /* TODO review if this is the correct list. */
    CTX_data_visible_objects(&C, &objects);
  }

  for (PointerRNA &obj_ptr : objects) {
    collect_animation_channels_for_id(*obj_ptr.owner_id, r_channels);
  }
}

}  // namespace blender::ed::animfilter
