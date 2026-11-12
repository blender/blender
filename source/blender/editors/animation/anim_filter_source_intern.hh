#pragma once

#include <ostream>

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "ED_anim_filter.hh"

namespace blender {
struct bContext;
struct Object;
}  // namespace blender

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

/* ----------------------------------------------------------------- */

/* Context-like functions that do not use the context system.
 * \see anim_filter_fake_context.cc for more info.
 */

/** Return the selected objects from the context. */
Vector<Object *> fake_ctx_selected_objects(const bContext &C);
/** Return the visible objects from the context. */
Vector<Object *> fake_ctx_visible_objects(const bContext &C);

/* ----------------------------------------------------------------- */

}  // namespace blender::ed::animfilter
