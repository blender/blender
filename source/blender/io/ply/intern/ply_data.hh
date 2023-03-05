/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

namespace blender::io::ply {

enum PlyDataTypes { CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE };

struct PlyData {
  Vector<float3> vertices;
  Vector<float3> vertex_normals;
  /* Value between 0 and 1. */
  Vector<float4> vertex_colors;
  Vector<std::pair<int, int>> edges;
  Vector<float3> edge_colors;
  Vector<Array<uint32_t>> faces;
  Vector<float2> UV_coordinates;
};

enum PlyFormatType { ASCII, BINARY_LE, BINARY_BE };

struct PlyHeader {
  int vertex_count = 0;
  int edge_count = 0;
  int face_count = 0;
  int header_size = 0;
  /* List of elements in ply file with their count. */
  Vector<std::pair<std::string, int>> elements;
  /* List of properties (Name, type) per element. */
  Vector<Vector<std::pair<std::string, PlyDataTypes>>> properties;
  PlyFormatType type;
};

}  // namespace blender::io::ply
