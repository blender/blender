/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "IO_stl.hh"

namespace blender::io::stl {

/* Main export function used from within Blender. */
void exporter_main(bContext *C, const STLExportParams &export_params);

}  // namespace blender::io::stl
