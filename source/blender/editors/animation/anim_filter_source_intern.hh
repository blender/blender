#pragma once

#include <ostream>

#include "BLI_set.hh"

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

/* Information that needs to be passed around various channel-sourcing functions. */
struct ChannelSourcingContext {
  ChannelArena &arena;
  Set<ID *> visited_ids = {};
};

void collection_channels_for_object(Object &obj,
                                    const CollectionFilters &collection_filters,
                                    ChannelSourcingContext &ctx);

void debug_stream_hierarchy(std::ostream &stream,
                            const AnimationChannel &channel,
                            StringRef indent = {});

}  // namespace blender::ed::animfilter
