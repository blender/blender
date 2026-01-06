/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

namespace blender {

struct bContext;
struct PLYExportParams;

namespace io::ply {

/* Main export function used from within Blender. */
void exporter_main(bContext *C, const PLYExportParams &export_params);

}  // namespace io::ply
}  // namespace blender
