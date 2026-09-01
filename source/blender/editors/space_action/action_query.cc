/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

namespace blender::ed::action {

static ed::animfilter::CollectionFilters get_filters(SpaceAction &saction)
{
  using namespace ed::animfilter;
  CollectionFilters filters;
  if (saction.ads.filterflag & ADS_FILTER_ONLYSEL) {
    filters.only_selected = true;
  }
  return filters;
}

ed::animfilter::ChannelArena get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  ChannelArena arena;

  collect_animation_channels(C, get_filters(*saction), arena);
  /* TODO: don't hard-code the hierarchy. */
  group_channels(arena,
                 {
                     ChannelTagType::ID,
                     ChannelTagType::ACTION,
                     ChannelTagType::ACTION_SLOT,
                     ChannelTagType::FCURVE_GROUP,
                     ChannelTagType::BONE,
                 },
                 false);

  return arena;
}

}  // namespace blender::ed::action
