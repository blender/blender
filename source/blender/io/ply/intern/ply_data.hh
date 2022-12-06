/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_math_vec_types.hh"
#include "BLI_vector.hh"
#include "DNA_meshdata_types.h"

namespace blender::io::ply {

enum PlyDataTypes { CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE };

//int typeSizes[8] = {1, 1, 2, 2, 4, 4, 4, 8};

struct PlyData {
  Vector<float3> vertices;
  Vector<float3> vertex_normals;
  // value between 0 and 1
  Vector<float3> vertex_colors;
  Vector<std::pair<int, int>> edges;
  Vector<float3> edge_colors;
  Vector<Vector<uint32_t>> faces;
};

enum PlyFormatType { ASCII, BINARY_LE, BINARY_BE };

struct PlyHeader {
  int vertex_count = 0, edge_count = 0, face_count = 0, header_size = 0;
  std::vector<std::pair<std::string, PlyDataTypes>> properties;
  PlyDataTypes vertex_index_count_type, vertex_index_type;
  PlyFormatType type;
};

}  // namespace blender::io::ply
