/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#pragma once

namespace blender {

struct bContext;
struct PointCloud;
struct SPZImportParams;

namespace io::spz {

/* Main import function used from within Blender. */
void importer_main(const bContext *C, const SPZImportParams &import_params);

PointCloud *import_point_cloud(const SPZImportParams &import_params);

}  // namespace io::spz
}  // namespace blender
