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

ed::animfilter::ChannelArena get_data_channels(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  ChannelArena arena;
  collect_animation_channels(C, get_filters(*saction), arena);
  return arena;
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
                 saction->ads.flag & ADS_FLAG_MINIMAL_GROUPING);

  /* Record this "use" of the channel runtime, as this is the moment where new channels are first
   * seen and old channels no longer exist. */
  saction->runtime->last_used++;

  /* Update the channels with the previously-stored UI state. */
  for (SummaryChannel *summary : arena.summary_channels) {
    const ChannelIdentifier chan_ident = summary->identifier();

    SAction_AnimationChannelState *ui_state = saction->runtime->channel_states.lookup_ptr(
        chan_ident);
    if (ui_state) {
      ui_state->last_used = saction->runtime->last_used;
      summary->update_from_ui_state(*ui_state);
    }
  }

  return arena;
}

}  // namespace blender::ed::action
