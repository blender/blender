/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include "IO_otio.hh"

#include "opentimelineio/serializableObject.h"

namespace blender {

struct bContext;
struct Scene;

namespace io::otio {

Scene *importer_main(bContext *C, const OTIOImportParams &params);
opentimelineio::OPENTIMELINEIO_VERSION_NS::SerializableObject *read_otio_file_with_workarounds(
    const char *filepath,
    opentimelineio::OPENTIMELINEIO_VERSION_NS::ErrorStatus *error_status = nullptr);

}  // namespace io::otio
}  // namespace blender
