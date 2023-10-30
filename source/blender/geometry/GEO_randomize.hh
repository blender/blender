/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct Mesh;
struct PointCloud;
namespace blender::bke {
class CurvesGeometry;
}

namespace blender::geometry {

bool use_debug_randomization();

void debug_randomize_vertex_order(Mesh *mesh);
void debug_randomize_edge_order(Mesh *mesh);
void debug_randomize_face_order(Mesh *mesh);
void debug_randomize_mesh_order(Mesh *mesh);
void debug_randomize_point_order(PointCloud *pointcloud);
void debug_randomize_curve_order(bke::CurvesGeometry *curves);

};  // namespace blender::geometry
