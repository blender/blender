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
  // value between 0 and 1
  Vector<float3> vertex_colors;
  Vector<MEdge> edges;
  Vector<Vector<int>> faces;
};

}  // namespace blender::io::ply
