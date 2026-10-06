/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include "opentimelineio/serializableObjectWithMetadata.h"

namespace blender {
struct IDProperty;
struct PointerRNA;
}  // namespace blender

namespace blender::io::otio {
using namespace opentimelineio::OPENTIMELINEIO_VERSION_NS;

AnyDictionary rna_to_dictionary(PointerRNA &ptr);

void add_foreign_metadata(IDProperty *properties, SerializableObjectWithMetadata &object);

}  // namespace blender::io::otio
