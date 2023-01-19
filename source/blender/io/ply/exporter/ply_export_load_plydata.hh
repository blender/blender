/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BKE_mesh.h"
#include "BLI_math.h"

#include "DNA_layer_types.h"

#include "IO_ply.h"

#include "ply_data.hh"

namespace blender::io::ply {

Mesh *do_triangulation(const Mesh *mesh, bool force_triangulation);

struct UV_vertex_key {
  float2 UV;
  int vertex_index;

  UV_vertex_key(float2 UV, int vertex_index) : UV(UV), vertex_index(vertex_index)
  {
  }

  bool operator==(const UV_vertex_key &r) const
  {
    return (UV == r.UV && vertex_index == r.vertex_index);
  }
};

struct UV_vertex_hash {
  std::size_t operator()(const blender::io::ply::UV_vertex_key &key) const
  {
    return ((std::hash<float>()(key.UV.x) ^ (std::hash<float>()(key.UV.y) << 1)) >> 1) ^
           (std::hash<int>()(key.vertex_index) << 1);
  }
};

std::unordered_map<UV_vertex_key, int, UV_vertex_hash> generate_vertex_map(
    const Mesh *mesh, const MLoopUV *mloopuv, const PLYExportParams &export_params);

void load_plydata(PlyData &plyData, const bContext *C, const PLYExportParams &export_params);

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params);
}  // namespace blender::io::ply
