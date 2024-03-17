/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "IO_stl.hh"

struct bContext;
struct Depsgraph;

namespace blender::io::stl {

void exporter_main(bContext *C, const STLExportParams &export_params);
void export_frame(Depsgraph *depsgraph,
                  float scene_unit_scale,
                  const STLExportParams &export_params);

}  // namespace blender::io::stl
