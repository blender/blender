/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <utility>

#include "BLI_assert.hh"

#include "BKE_context.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

void AnimfilterSpaceRuntime::copy_ui_state_from(const AnimfilterSpaceRuntime &other)
{
  this->channel_states = other.channel_states;
  this->last_used = other.last_used;
  this->active_channel_ident = other.active_channel_ident;
  /* The ChannelArena should not be shared between spaces. */
  this->channel_arena.reset();
}

AnimationChannelUIState &AnimfilterSpaceRuntime::channel_state_for_update(
    const ChannelIdentifier &channel_key)
{
  AnimationChannelUIState &channel_state = this->channel_states.lookup_or_add_default(channel_key);
  channel_state.last_used = this->last_used;
  return channel_state;
}

ChannelArena &AnimfilterSpaceRuntime::get_summary_tree(const bContext &C,
                                                       const build_tree_fn build_summary_tree_fn)
{
  std::optional<ChannelArena> new_arena;
  if (this->channel_arena.has_value()) {
    /* There is already a ChannelArena from a previous run. Reuse or replace it. */
    ChannelArena *cached_arena = &this->channel_arena.value();

    /* Write the active channel back to the space runtime data. This should actually be done
     * whenever the active channel identifier changes, but doing it here is good enough given that
     * there will very likely be redraws anyway between setting the active channel and saving the
     * blend file. */
    this->active_channel_ident = cached_arena->active_channel_identifier();

    /* Ask the build function for a new one. If it returns nullptr, it means the cached one is
     * still good. */
    new_arena = build_summary_tree_fn(C, cached_arena);

    if (new_arena) {
      /* Replace. Migrate data from the old ChannelArena to the new ChannelArena. */
      new_arena->active_channel_identifier_set(cached_arena->active_channel_identifier());
    }
    else {
      /* Reuse. Cached channels are still valid, but that doesn't mean their data is fresh. */
      cached_arena->mark_channels_dirty();
    }
  }
  else {
    /* Construct a brand new ChannelArena for this space. It should get the active channel
     * identifier from the space data, as it this may be the first time one is created after
     * loading this file. */
    new_arena = build_summary_tree_fn(C, nullptr);
    BLI_assert_msg(new_arena.has_value(),
                   "when there is no cached ChannelArena, a new one should always be built.");
    new_arena->active_channel_identifier_set(this->active_channel_ident);
  }

  if (new_arena) {
    BLI_assert(new_arena->datachannel_hash != UniqueHash{});
    this->channel_arena.emplace(std::move(*new_arena));
  }

  return *this->channel_arena;
}

}  // namespace blender::ed::animfilter
