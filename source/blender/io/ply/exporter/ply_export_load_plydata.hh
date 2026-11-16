/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_generic_virtual_array.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "ply_data.hh"

namespace blender {

struct Depsgraph;
struct PLYExportParams;
struct ReportList;

namespace io::ply {

void append_custom_attribute(StringRef name,
                             const GVArray &src,
                             Span<int> ply_to_vertex,
                             uint32_t vertex_offset,
                             StringRef mesh_name,
                             ReportList *reports,
                             Vector<PlyCustomAttribute> &r_attributes);

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params);

}  // namespace io::ply
}  // namespace blender
