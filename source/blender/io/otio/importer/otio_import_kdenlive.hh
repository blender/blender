/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include <optional>

#include "DNA_sequence_types.h"

#include "opentimelineio/generatorReference.h"

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

std::optional<StripType> kdenlive_generator_type(const GeneratorReference &generator);
AnyDictionary kdenlive_strip_properties(SerializableObjectWithMetadata &object);

}  // namespace blender::io::otio
