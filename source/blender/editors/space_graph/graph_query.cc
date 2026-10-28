/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

#include "BKE_context.hh"

#include "ED_anim_api.hh"
#include "ED_space_graph.hh"

namespace blender::ed::graph {

ListBaseT<bAnimListElem> get_editable_fcurves(bAnimContext &ac)
{
  ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE |
                              ANIMFILTER_FCURVESONLY | ANIMFILTER_NODUPLIS);
  if (U.animation_flag & USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS) {
    filter |= ANIMFILTER_SEL;
  }

  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
  return anim_data;
}

static Span<animfilter::ChannelTagType> get_tag_order()
{
  using namespace ed::animfilter;

  static const Array<ChannelTagType> default_tag_order = {
      ChannelTagType::ACTION,
      ChannelTagType::ID,
      ChannelTagType::FCURVE_GROUP,
  };
  return default_tag_order;
}

animfilter::ChannelArena build_summary_tree(const bContext &C)
{
  using namespace ed::animfilter;

  SpaceGraph *space_graph = CTX_wm_space_graph(&C);
  if (!space_graph) {
    BLI_assert_unreachable();
    return {};
  }

  PRF_scope_with_name("ed::animfilter::get_summary_tree", ProfileCategory::Editor);

  ChannelArena arena;
  /* TODO pre-filter down to only FCurves. */
  arena.filters = get_filters(*space_graph->ads);
  collect_animation_channels(C, arena);
  group_channels(arena, get_tag_order(), space_graph->ads->flag & ADS_FLAG_MINIMAL_GROUPING);

  {
    PRF_scope_with_name("ed::animfilter::get_summary_tree::ui_state", ProfileCategory::Editor);

    /* Record this "use" of the channel runtime, as this is the moment where new channels are first
     * seen and old channels no longer exist. */
    space_graph->runtime->last_used++;

    /* Update the channels with the previously-stored UI state. */
    for (SummaryChannel *summary : arena.summary_channels) {
      summary->ui_state_map_set(&space_graph->runtime->channel_states);
    }
  }

  return arena;
}

/* The cached channel_arena is reset in #graph_draw_pre in space_graph.cc. */
animfilter::ChannelArena &get_summary_tree(const bContext &C)
{
  SpaceGraph *space_graph = CTX_wm_space_graph(&C);
  SpaceGraph_Runtime *runtime = space_graph->runtime;
  if (!runtime->channel_arena) {
    ed::animfilter::ChannelArena channels = build_summary_tree(C);
    runtime->channel_arena.emplace(std::move(channels));
  }

  return *runtime->channel_arena;
}

}  // namespace blender::ed::graph
