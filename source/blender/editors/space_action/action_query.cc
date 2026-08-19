/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

namespace blender {

void SpaceAction_Runtime::clear_channel_cache()
{
  for (ed::animfilter::DataChannel *channel : channels) {
    MEM_delete(channel);
  }
}

namespace ed::action {

Span<ed::animfilter::DataChannel> current_data_channels() {
    return {};
}

static MutableSpan<ed::animfilter::DataChannel *> get_or_collect_data_channels(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  /* TODO add extra flag to track if collection ran. An empty list is one of the possible states.
   */
  if (saction->runtime->channels.is_empty()) {
    /* TODO collect pins here? */
    collect_animation_channels(C, {}, saction->runtime->channels);
  }

  return saction->runtime->channels;
}

ed::animfilter::SummaryTree get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  MutableSpan<DataChannel *> channels = get_or_collect_data_channels(C);
  /* TODO don't hardcode tag_order. */
  SummaryTree summaries = group_channels(
      channels, {ChannelTagType::ID, ChannelTagType::ACTION}, false);
  return summaries;
}

}  // namespace ed::action

}  // namespace blender
