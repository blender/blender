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
  eSAction_Runtime_Flag flag = eSAction_Runtime_Flag(0);

  ed::animfilter::AnimfilterSpaceRuntime animfilter_runtime;
};

namespace ed::action {

/**
 * Returns all data channels for this editor. The summaries will not be built by this function.
 */
ed::animfilter::ChannelArena get_data_channels(const bContext &C);

/**
 * Return the buffered data or build up the tree if required.
 *
 * The SpaceAction owns the returned ChannelArena.
 */
ed::animfilter::ChannelArena &get_summary_tree(const bContext &C);

}  // namespace ed::action

}  // namespace blender
