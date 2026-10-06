/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#include "IO_otio.hh"
#include "otio_export.hh"

namespace blender {

void OTIO_export(bContext *C, const OTIOExportParams &params)
{
  io::otio::exporter_main(C, params);
}

}  // namespace blender
