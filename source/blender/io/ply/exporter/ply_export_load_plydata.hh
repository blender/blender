/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

struct Depsgraph;
struct PLYExportParams;

namespace blender::io::ply {

struct PlyData;

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params);

}  // namespace blender::io::ply
