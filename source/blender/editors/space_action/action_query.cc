/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

namespace blender {

namespace ed::action {

Span<ed::animfilter::DataChannel> current_data_channels() {
    return {};
}

static MutableSpan<ed::animfilter::DataChannel> get_or_collect_data_channels(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }
  if (saction->runtime->channels) {
    // return MutableSpan<DataChannel>(saction->runtime->channels, saction->runtime->channels_len);
  }
  /* TODO only return pointer to root, keep summaries buffered. */
  Vector<DataChannel> channels;
  /* TODO collect pins here. */
  collect_animation_channels(C, {}, channels);
}

ed::animfilter::SummaryTree get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  MutableSpan<DataChannel> channels = get_or_collect_data_channels(C);
  /* TODO don't hardcode tag_order. */
  SummaryTree summaries = group_channels(
      channels, {ChannelTagType::ID, ChannelTagType::ACTION}, false);
  return summaries;
}

}  // namespace ed::action

}  // namespace blender
