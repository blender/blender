/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

namespace blender {

struct Depsgraph;
struct PLYExportParams;

namespace io::ply {

struct PlyData;

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params);

}  // namespace io::ply
}  // namespace blender
