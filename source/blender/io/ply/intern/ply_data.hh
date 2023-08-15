/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

namespace blender::io::ply {

enum PlyDataTypes { NONE, CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE, PLY_TYPE_COUNT };

struct PlyData {
  Vector<float3> vertices;
  Vector<float3> vertex_normals;
  Vector<float4> vertex_colors; /* Linear space, 0..1 range colors. */
  Vector<std::pair<int, int>> edges;
  Vector<uint32_t> face_vertices;
  Vector<uint32_t> face_sizes;
  Vector<float2> uv_coordinates;
  std::string error;
};

enum PlyFormatType { ASCII, BINARY_LE, BINARY_BE };

struct PlyProperty {
  std::string name;
  PlyDataTypes type = PlyDataTypes::NONE;
  PlyDataTypes count_type = PlyDataTypes::NONE; /* NONE means it's not a list property */
};

struct PlyElement {
  std::string name;
  int count = 0;
  Vector<PlyProperty> properties;
  int stride = 0;

  void calc_stride();
};

struct PlyHeader {
  Vector<PlyElement> elements;
  PlyFormatType type;
};

}  // namespace blender::io::ply
