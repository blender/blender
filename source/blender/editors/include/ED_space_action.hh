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
class ChannelTree;
class SummaryChannel;
}  // namespace ed::animfilter

/** #SpaceAction_Runtime.flag */
enum eSAction_Runtime_Flag : char {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
};
ENUM_OPERATORS(eSAction_Runtime_Flag);

struct SpaceAction_Runtime {
  /* Array of currently buffered channels. The channels are heap allocated and owned by this
   * structure.
   * TODO can this be turned into a vector of data instead of pointers to data?
   */
  Vector<ed::animfilter::DataChannel *> data_channels;
  Vector<ed::animfilter::SummaryChannel *> summary_channels;
  eSAction_Runtime_Flag flag = {};

  ~SpaceAction_Runtime()
  {
    clear_summary_cache();
    clear_data_cache();
  }

  /* Frees the memory and clears the list. */
  void clear_data_cache();
  void clear_summary_cache();
};

namespace ed::action {

/**
 * Return the buffered data or build up the tree if required.
 */
ed::animfilter::ChannelTree get_summary_tree(const bContext &C);

}  // namespace ed::action

}  // namespace blender
