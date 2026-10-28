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

  ed::animfilter::AnimfilterSpaceRuntime animfilter_runtime;
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

animfilter::ChannelArena &get_summary_tree(const bContext &C);

}  // namespace ed::graph
}  // namespace blender
