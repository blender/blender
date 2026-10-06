/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 *
 * Best effort conversion of DaVinci Resolve metadata.
 */

#pragma once

#include <optional>

#include "BLI_span.hh"

#include "DNA_scene_enums.h"
#include "DNA_sequence_types.h"

#include "opentimelineio/clip.h"
#include "opentimelineio/generatorReference.h"
#include "opentimelineio/track.h"
#include "opentimelineio/transition.h"

namespace blender {
struct Scene;
}  // namespace blender

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

std::optional<StripType> resolve_generator_type(const GeneratorReference &generator);
bool resolve_is_adjustment_clip(Clip &clip);
std::optional<StripType> resolve_transition_type(Transition &transition);
std::optional<eSeqImageFitMethod> resolve_fit_method(Item &item);
AnyDictionary resolve_strip_properties(const Scene &scene,
                                       const Strip &strip,
                                       SerializableObjectWithMetadata &object);
bool resolve_track_locked(Track &track);
void resolve_connect_strips(Span<std::pair<Strip *, SerializableObjectWithMetadata *>> strips);

}  // namespace blender::io::otio
