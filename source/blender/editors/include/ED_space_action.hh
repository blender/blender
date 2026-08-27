/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_enum_flags.hh"
#include "BLI_vector.hh"

namespace blender {

struct bContext;

namespace ed::animfilter {
class DataChannel;
class AnimationChannel;
class ChannelArena;
class SummaryChannel;
}  // namespace ed::animfilter

/** #SpaceAction_Runtime.flag */
enum eSAction_Runtime_Flag : char {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
};
ENUM_OPERATORS(eSAction_Runtime_Flag);

struct SpaceAction_Runtime {
  eSAction_Runtime_Flag flag = {};
};

namespace ed::action {

/**
 * Return the buffered data or build up the tree if required.
 */
ed::animfilter::ChannelArena get_summary_tree(const bContext &C);

}  // namespace ed::action

}  // namespace blender
