#pragma once

#include <ostream>

#include "ED_anim_filter.hh"

namespace blender::ed::animfilter {

void collection_channels_for_object(Object &obj,
                                    const CollectionFilters &collection_filters,
                                    ChannelArena &arena);

void debug_stream_hierarchy(std::ostream &stream,
                            const AnimationChannel &channel,
                            StringRef indent = {});

}  // namespace blender::ed::animfilter
