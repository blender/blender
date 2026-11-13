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

static std::optional<ed::animfilter::ChannelArena> build_summary_tree(
    const bContext &C, const ed::animfilter::ChannelArena *cached_arena)
{
  using namespace ed::animfilter;

  SpaceGraph *space_graph = CTX_wm_space_graph(&C);
  if (!space_graph) {
    BLI_assert_unreachable();
    return {};
  }

  PRF_scope_with_name("ed::graph::build_summary_tree", ProfileCategory::Editor);

  ChannelArena arena;
  arena.filters = get_filters(*space_graph->ads);
  switch (space_graph->mode) {
    case SIPO_MODE_ANIMATION:
      arena.filters.channel_type_allowlist = {DataChannelType::FCURVE};
      break;
    case SIPO_MODE_DRIVERS:
      arena.filters.channel_type_allowlist = {DataChannelType::DRIVER};
      break;
  }

  collect_animation_channels(C, arena);

  arena.grouping_params.use_minimal_grouping = space_graph->ads->flag & ADS_FLAG_MINIMAL_GROUPING;
  arena.grouping_params.tag_order = get_tag_order();
  arena.compute_datachannel_hash();

  if (cached_arena && arena.is_same_as(*cached_arena)) {
    /* Skip grouping into Summary Channels and re-syncing of the UI state. */
    return {};
  }

  group_channels(arena);

  {
    PRF_scope_with_name("ed::graph::build_summary_tree::ui_state", ProfileCategory::Editor);

    /* Record this "use" of the channel runtime, as this is the moment where new channels are first
     * seen and old channels no longer exist. */
    space_graph->runtime->animfilter_runtime.last_used++;

    /* Update the channels with the previously-stored UI state. */
    for (SummaryChannel *summary : arena.summary_channels) {
      summary->ui_state_map_set(&space_graph->runtime->animfilter_runtime.channel_states);
    }
  }

  return arena;
}

animfilter::ChannelArena &get_summary_tree(const bContext &C)
{
  SpaceGraph *sgraph = CTX_wm_space_graph(&C);
  ed::animfilter::AnimfilterSpaceRuntime &runtime = sgraph->runtime->animfilter_runtime;
  return runtime.get_summary_tree(C, build_summary_tree);
}

}  // namespace blender::ed::graph
