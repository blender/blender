/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BKE_lib_id.h"

#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

namespace blender::io::obj {

/**
 * All vertex positions, normals, UVs, colors in the OBJ file.
 */
struct GlobalVertices {
  Vector<float3> vertices;
  Vector<float2> uv_vertices;
  Vector<float3> vert_normals;

  /**
   * Vertex colors might not be present in the file at all, or only
   * provided for some meshes. Store them in chunks as they are
   * spelled out in the file, e.g. if there are 10 vertices in sequence, all
   * with `xyzrgb` colors, they will be one block.
   */
  struct VertexColorsBlock {
    Vector<float3> colors;
    int start_vertex_index;
  };
  Vector<VertexColorsBlock> vertex_colors;
};

/**
 * A face's corner in an OBJ file. In Blender, it translates to a corner vertex.
 */
struct PolyCorner {
  /* These indices range from zero to total vertices in the OBJ file. */
  int vert_index;
  /* -1 is to indicate absence of UV vertices. Only < 0 condition should be checked since
   * it can be less than -1 too. */
  int uv_vert_index = -1;
  int vertex_normal_index = -1;
};

struct PolyElem {
  int vertex_group_index = -1;
  int material_index = -1;
  bool shaded_smooth = false;
  int start_index_ = 0;
  int corner_count_ = 0;
};

/**
 * Contains data for one single NURBS curve in the OBJ file.
 */
struct NurbsElement {
  /**
   * For curves, groups may be used to specify multiple splines in the same curve object.
   * It may also serve as the name of the curve if not specified explicitly.
   */
  std::string group_;
  int degree = 0;
  /**
   * Indices into the global list of vertex coordinates. Must be non-negative.
   */
  Vector<int> curv_indices;
  /* Values in the parm u/v line in a curve definition. */
  Vector<float> parm;
};

enum eGeometryType {
  GEOM_MESH = OB_MESH,
  GEOM_CURVE = OB_CURVES_LEGACY,
};

struct Geometry {
  eGeometryType geom_type_ = GEOM_MESH;
  std::string geometry_name_;
  Map<std::string, int> group_indices_;
  Vector<std::string> group_order_;
  Map<std::string, int> material_indices_;
  Vector<std::string> material_order_;

  int vertex_index_min_ = INT_MAX;
  int vertex_index_max_ = -1;
  /* Global vertex indices used by this geometry. */
  Set<int> vertices_;
  /* Mapping from global vertex index to geometry-local vertex index. */
  Map<int, int> global_to_local_vertices_;
  /* Loose edges in the file. */
  Vector<int2> edges_;

  Vector<PolyCorner> face_corners_;
  Vector<PolyElem> face_elements_;

  bool has_invalid_polys_ = false;
  bool has_vertex_groups_ = false;
  NurbsElement nurbs_element_;
  int total_loops_ = 0;

  int get_vertex_count() const
  {
    return int(vertices_.size());
  }
  void track_vertex_index(int index)
  {
    vertices_.add(index);
    math::min_inplace(vertex_index_min_, index);
    math::max_inplace(vertex_index_max_, index);
  }
  void track_all_vertices(int count)
  {
    vertices_.reserve(count);
    for (int i = 0; i < count; ++i) {
      vertices_.add(i);
    }
    vertex_index_min_ = 0;
    vertex_index_max_ = count - 1;
  }
};

}  // namespace blender::io::obj
