/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "RNA_path.hh"

namespace blender {

struct bContext;
struct ID;

namespace ed::animfilter {
class DataChannel;
class AnimationChannel;
class ChannelArena;
class SummaryChannel;

using ParsedSourcePath = blender::ParsedRNAPath<6>;

/**
 * Identifier for an AnimationChannel.
 *
 * Data is copied into the identifier, because these identifiers are designed to out-live the
 * channels they identify. This copying should be fast, as it just consists of a pointer an a bunch
 * of UString instances.
 */
class ChannelIdentifier {
 public:
  /** Owner ID of the channel source. */
  ID *owner_id;
  /** Path from the ID to the channel source data. */
  std::optional<ParsedSourcePath> source_rna_path;

  bool operator==(const ChannelIdentifier &other) const
  {
    if (other.owner_id != this->owner_id) {
      return false;
    }
    if (!this->source_rna_path.has_value()) {
      return !other.source_rna_path.has_value();
    }
    return this->source_rna_path->items == other.source_rna_path->items;
  }

  int64_t hash() const
  {
    Span<rna_path::Item> path_items = {};
    if (this->source_rna_path.has_value()) {
      path_items = this->source_rna_path->items;
    }
    return get_default_hash(this->owner_id, this->source_rna_path.has_value(), path_items);
  }
};

}  // namespace ed::animfilter

/** #SpaceAction_Runtime.flag */
enum eSAction_Runtime_Flag : char {
  /** Temporary flag to force channel selections to be synced with main */
  SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC = (1 << 0),
};
ENUM_OPERATORS(eSAction_Runtime_Flag);

struct SAction_AnimationChannelState {
  /** Whether the channel should be collapsed or not.
   *
   * Uses 'is_collapsed' (and not 'is_expanded') following AbstractTreeViewItem.
   */
  bool is_collapsed = false;

  /**
   * A 'timestamp' that's set from #SpaceAction_Runtime::last_used, used to detect the
   * least-recently-used ones for culling the remembered states.
   */
  uint32_t last_used = 0;
};

struct SpaceAction_Runtime {
  eSAction_Runtime_Flag flag = {};

  /* TODO: the identifiers contain ID pointers, which should be reported to Blender for handling ID
   * remapping. */
  Map<ed::animfilter::ChannelIdentifier, SAction_AnimationChannelState> channel_states;

  /** A counter that is incremented every time the channel states above are used. */
  uint32_t last_used = 0;
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
