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

void collection_channels_for_object(Object &obj, ChannelSourcingContext &ctx);

/**
 * Callback to be registered to setting buttons using `button_func_set`.
 *
 * \param setting_key has to be the enum value for `ChannelSettingKey` cast to a `void *`.
 */
void setting_button_pushed(bContext *C, void *setting_key, void *arg2);

void debug_stream_hierarchy(std::ostream &stream,
                            const AnimationChannel &channel,
                            StringRef indent = {});

}  // namespace blender::ed::animfilter
