/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include "DNA_listBase.h"

#include "ED_anim_filter.hh"

namespace blender {

struct bAnimContext;
struct bAnimListElem;

struct SpaceGraph_Runtime {
  /** #eGraphEdit_Runtime_Flag */
  char flag = 0;
  char _pad[7] = {};
  /** Sampled snapshots of F-Curves used as in-session guides */
  ListBaseT<FCurve> ghost_curves = {nullptr, nullptr};

  /* TODO deduplicate with SpaceAction_Runtime. */
  ed::animfilter::ChannelUIStateMap channel_states;
  /** A counter that is incremented every time the channel states above are used. */
  int32_t last_used = 0;
  /* Runtime cache for channels in the channel box. */
  std::optional<ed::animfilter::ChannelArena> channel_arena;
  /**
   * Ensures there is an entry for this channel in the UI state map, returning that entry so it can
   * be updated. This also sets the entry's `last_used` field to #last_used.
   */
  ed::animfilter::AnimationChannelUIState &channel_state_for_update(
      const ed::animfilter::ChannelIdentifier &channel_key);
};

namespace ed::graph {

/**
 * Return all bAnimListElem for which the keyframes are visible in the
 * GUI. This excludes FCurves that are drawn as curves but whose keyframes are NOT shown.
 * All entries of the list will be of type ANIMTYPE_FCURVE.
 *
 * The listbase will have to be freed by the caller with ANIM_animdata_freelist;
 */
ListBaseT<bAnimListElem> get_editable_fcurves(bAnimContext &ac);

animfilter::ChannelArena get_summary_tree(const bContext &C);

}  // namespace ed::graph
}  // namespace blender
