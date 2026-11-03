/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.hh"
#include "BLI_span.hh"

#include "BKE_context.hh"

#include "DNA_action_types.h"

#include "ED_anim_filter.hh"
#include "ED_space_action.hh"

#include "PRF_profile.hh"

namespace blender::ed::action {

/* Get the data channels for the given dope sheet mode. */
static void collect_data_channels(const bContext &C,
                                  const eAnimEdit_Context dopesheet_mode,
                                  ed::animfilter::ChannelArena &arena)
{
  if (dopesheet_mode == SACTCONT_ACTION) {
    /* The Action editor has its own way to gather channels. */
    collect_action_editor_channels(C, arena);
  }
  else {
    collect_animation_channels(C, arena);
  }
}

/* Get just the data channels for the current space, skip all the grouping. */
ed::animfilter::ChannelArena get_data_channels(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  ChannelArena arena;
  arena.filters = get_filters(saction->ads);
  collect_data_channels(C, saction->mode, arena);
  return arena;
}

static Span<ed::animfilter::ChannelTagType> tag_order_for_dope_sheet_mode(
    const eAnimEdit_Context mode)
{
  using namespace ed::animfilter;

  static const Array<ChannelTagType> default_tag_order = {
      ChannelTagType::ACTION,
      ChannelTagType::ID,
      ChannelTagType::GREASEPENCIL_GROUP,
      ChannelTagType::FCURVE_GROUP,
  };

  switch (mode) {
    case SACTCONT_DOPESHEET:
    case SACTCONT_TIMELINE: {
      static const Array<ChannelTagType> tag_order = {
          ChannelTagType::ACTION,
          ChannelTagType::ID,
          ChannelTagType::GREASEPENCIL_GROUP,
          ChannelTagType::FCURVE_GROUP,
      };
      return tag_order;
    }

    case SACTCONT_ACTION: {
      static const Array<ChannelTagType> tag_order = {
          ChannelTagType::ACTION_SLOT,
          ChannelTagType::GREASEPENCIL_GROUP,
          ChannelTagType::FCURVE_GROUP,
      };
      return tag_order;
    }

    case SACTCONT_GPENCIL: {
      static const Array<ChannelTagType> tag_order = {
          ChannelTagType::ID,
          ChannelTagType::GREASEPENCIL_GROUP,
          ChannelTagType::ACTION,
          ChannelTagType::FCURVE_GROUP,
      };
      return tag_order;
    }

    case SACTCONT_SHAPEKEY:
    case SACTCONT_MASK:
    case SACTCONT_CACHEFILE:
      return default_tag_order;
  }

  BLI_assert_unreachable();
  return default_tag_order;
}

static std::optional<ed::animfilter::ChannelArena> build_summary_tree(
    const bContext &C, const ed::animfilter::ChannelArena *cached_arena)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  if (!saction) {
    BLI_assert_unreachable();
    return {};
  }

  PRF_scope_with_name("ed::animfilter::get_summary_tree", ProfileCategory::Editor);

  ChannelArena arena;
  arena.filters = get_filters(saction->ads);
  collect_data_channels(C, saction->mode, arena);

  arena.grouping_params.use_minimal_grouping = saction->ads.flag & ADS_FLAG_MINIMAL_GROUPING;
  arena.grouping_params.tag_order = tag_order_for_dope_sheet_mode(saction->mode);
  arena.compute_data_channel_hash();

  if (cached_arena && arena.is_same_as(*cached_arena)) {
    /* Skip grouping into Summary Channels and re-syncing of the UI state. */
    return {};
  }

  group_channels(arena);

  {
    PRF_scope_with_name("ed::animfilter::get_summary_tree::ui_state", ProfileCategory::Editor);

    /* Record this "use" of the channel runtime, as this is the moment where new channels are first
     * seen and old channels no longer exist. */
    saction->runtime->animfilter_runtime.last_used++;

    /* Update the channels with the previously-stored UI state. */
    for (SummaryChannel *summary : arena.summary_channels) {
      summary->ui_state_map_set(&saction->runtime->animfilter_runtime.channel_states);
    }
  }

  return arena;
}

/* Note: This cache is reset by #action_draw_pre in space_action.cc. */
ed::animfilter::ChannelArena &get_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceAction *saction = CTX_wm_space_action(&C);
  BLI_assert(saction->runtime);
  AnimfilterSpaceRuntime &runtime = saction->runtime->animfilter_runtime;

  std::optional<ChannelArena> new_arena;
  if (runtime.channel_arena.has_value()) {
    const ChannelArena *cached_arena = &runtime.channel_arena.value();
    new_arena = build_summary_tree(C, cached_arena);
  }
  else {
    new_arena = build_summary_tree(C, nullptr);
    BLI_assert_msg(new_arena.has_value(),
                   "when there is no cached ChannelArena, a new one should always be built.");
  }

  if (new_arena) {
    BLI_assert(new_arena->data_channel_hash != UniqueHash{});
    /* The old arena didn't exist or was outdated, and we got a new one. */
    runtime.channel_arena.emplace(std::move(*new_arena));
  }
  else {
    /* The cached channels are still valid, but that doesn't mean the key data is fresh. */
    runtime.channel_arena->mark_channels_dirty();
  }

  return *runtime.channel_arena;
}

}  // namespace blender::ed::action
