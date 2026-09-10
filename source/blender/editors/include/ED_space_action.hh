/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "RNA_path.hh"

#include "ED_anim_filter.hh"

namespace blender {

struct bContext;
struct ID;

/** #SpaceAction_Runtime.flag */
enum eSAction_Runtime_Flag : char {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
};
ENUM_OPERATORS(eSAction_Runtime_Flag);

struct SpaceAction_Runtime {
  eSAction_Runtime_Flag flag = {};

  /* TODO: the identifiers contain ID pointers, which should be reported to Blender for handling ID
   * remapping. */
  ed::animfilter::ChannelUIStateMap channel_states;

  /** A counter that is incremented every time the channel states above are used. */
  uint32_t last_used = 0;

  std::optional<ed::animfilter::ChannelArena> channel_arena;

  /**
   * Ensures there is an entry for this channel in the UI state map, returning that entry so it can
   * be updated. This also sets the entry's `last_used` field to #last_used.
   */
  ed::animfilter::SAction_AnimationChannelState &channel_state_for_update(
      const ed::animfilter::ChannelIdentifier &channel_key);
};

namespace ed::action {

/**
 * Returns all data channels for this editor. The summaries will not be built by this function.
 */
ed::animfilter::ChannelArena get_data_channels(const bContext &C);

/**
 * Return the buffered data or build up the tree if required.
 */
ed::animfilter::ChannelArena get_summary_tree(const bContext &C);

}  // namespace ed::action

}  // namespace blender
