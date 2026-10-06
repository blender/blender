/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include "BLI_map.hh"

#include "DNA_sequence_types.h"

#include "opentimelineio/clip.h"
#include "opentimelineio/generatorReference.h"
#include "opentimelineio/timeline.h"
#include "opentimelineio/track.h"
#include "opentimelineio/transition.h"

namespace blender {
struct Scene;
}  // namespace blender

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

GeneratorReference *resolve_generator_reference(const Strip &strip,
                                                const TimeRange &available_range);
Map<const Strip *, int64_t> resolve_link_groups(const ListBaseT<Strip> &seqbase);
void resolve_add_clip_data(const Scene &scene,
                           const Strip &strip,
                           const Map<const Strip *, int64_t> &link_groups,
                           Clip &clip);
void resolve_add_transition_data(const Scene &scene,
                                 const Strip &transition,
                                 Transition &otio_transition);
void resolve_add_track_data(const SeqTimelineChannel *channel, Track &track);
void resolve_add_timeline_metadata(Timeline &timeline);

}  // namespace blender::io::otio
