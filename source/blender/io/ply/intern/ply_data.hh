/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_math_vec_types.hh"
#include "BLI_vector.hh"
#include "DNA_meshdata_types.h"

namespace blender::io::ply {

enum PlyDataTypes { CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE };

struct PlyData {
  Vector<float3> vertices;
  Vector<float3> vertex_normals;
  // Value between 0 and 1.
  Vector<float4> vertex_colors;
  Vector<std::pair<int, int>> edges;
  Vector<float3> edge_colors;
  Vector<Vector<uint32_t>> faces;
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
  PlyDataTypes vertex_index_count_type;
  PlyDataTypes vertex_index_type;
  PlyFormatType type;
};

}  // namespace blender::io::ply
