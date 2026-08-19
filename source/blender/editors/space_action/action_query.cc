/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

namespace blender {

namespace ed::action {

Span<ed::animfilter::DataChannel> current_data_channels() {
    return {};
}

/* TODO only return pointer to root, keep summaries buffered. */
ed::animfilter::SummaryTree get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;
  Vector<DataChannel> channels;
  /* TODO collect pins here. */
  collect_animation_channels(C, {}, channels);
  /* TODO don't hardcode tag_order. */
  SummaryTree summaries = group_channels(
      channels, {ChannelTagType::ID, ChannelTagType::ACTION}, false);
  return summaries;
}

}  // namespace ed::action

}  // namespace blender
