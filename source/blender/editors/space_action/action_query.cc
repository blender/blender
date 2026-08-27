/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

namespace blender {

void SpaceAction_Runtime::clear_summary_cache()
{
  for (ed::animfilter::SummaryChannel *channel : summary_channels) {
    MEM_delete(channel);
  }
  summary_channels.clear();
}

void SpaceAction_Runtime::clear_data_cache()
{
  for (ed::animfilter::DataChannel *channel : data_channels) {
    MEM_delete(channel);
  }
  data_channels.clear();
}

namespace ed::action {

static ed::animfilter::CollectionFilters get_filters(SpaceAction &saction)
{
  using namespace ed::animfilter;
  CollectionFilters filters;
  if (saction.ads.filterflag & ADS_FILTER_ONLYSEL) {
    filters.only_selected = true;
  }
  return filters;
}

static MutableSpan<ed::animfilter::DataChannel *> get_or_create_data_channels(const bContext &C,
                                                                              SpaceAction &saction)
{
  using namespace ed::animfilter;

  /* TODO: don't cache at all. But that means changing the function signature, which is a bit too
   * big of a change right now. */
  saction.runtime->clear_data_cache();

  /* TODO collect pins here? */
  collect_animation_channels(C, get_filters(saction), saction.runtime->data_channels);

  return saction.runtime->data_channels;
}

/**
 * Construct the summary channels, and return them all.
 *
 * The first in the span is the root channel. Other summary channels are there too, even though
 * they are also available in `root.children` or any of their children.
 */
static MutableSpan<ed::animfilter::SummaryChannel *> get_or_create_summary_channels(
    MutableSpan<ed::animfilter::DataChannel *> channels, SpaceAction &saction)
{
  using namespace ed::animfilter;

  /* TODO: don't cache at all. But that means changing the function signature, which is a bit too
   * big of a change right now. */
  saction.runtime->clear_summary_cache();

  /* TODO don't hardcode tag_order. */
  saction.runtime->summary_channels = group_channels(
      channels, {ChannelTagType::ID, ChannelTagType::ACTION}, false);

  return saction.runtime->summary_channels;
}

ed::animfilter::ChannelTree get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  MutableSpan<DataChannel *> channels = get_or_create_data_channels(C, *saction);
  MutableSpan<SummaryChannel *> summaries = get_or_create_summary_channels(channels, *saction);

  return {summaries[0], int(channels.size() + summaries.size())};
}

}  // namespace ed::action

}  // namespace blender
