/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <span>
#include <utility>

#include "opensubdiv_capi_type.hh"

/**
 * Sharpness value which makes an edge or a vertex infinitely sharp. Mirrors
 * `OpenSubdiv::Sdc::Crease::SHARPNESS_INFINITE`, which can not be included here.
 */
constexpr float OPENSUBDIV_SHARPNESS_INFINITE = 10.0f;

struct OpenSubdiv_Converter {
  int verts_num;
  /**
   * The topology of the mesh to be subdivided. See #Mesh::edges(), #Mesh::face_offsets(), and
   * #Mesh::corner_verts() documentation for the details. Other topology information is currently
   * encoded with callbacks rather than arrays directly.
   */
  std::span<const std::pair<int, int>> edges;
  std::span<const int> face_offsets;
  std::span<const int> corner_verts;

  /**
   * Sharpness (aka crease) of every edge and every vertex, in the
   * `[0, #OPENSUBDIV_SHARPNESS_INFINITE]` range. Vertex sharpness includes the infinite sharpness
   * used to make vertices adjacent to a loose edge sharp.
   *
   * Either aligned with #edges and with the vertices respectively, or empty.
   */
  std::span<const float> edge_sharpness;
  std::span<const float> vert_sharpness;

  OpenSubdiv_SchemeType scheme_type;

  OpenSubdiv_VtxBoundaryInterpolation vtx_boundary_interpolation;
  OpenSubdiv_FVarLinearInterpolation fvar_linear_interpolation;

  // Denotes whether this converter specifies full topology, which includes
  // vertices, edges, faces, vertices+edges of a face and edges/faces of a
  // vertex.
  // Otherwise this converter will only provide number of vertices and faces,
  // and vertices of faces. The rest of topology will be created by OpenSubdiv.
  //
  // NOTE: Even if converter does not provide full topology, it still needs
  // to provide number of edges and vertices-of-edge. Those are used to assign
  // topology tags.
  bool (*specifiesFullTopology)(const OpenSubdiv_Converter *converter);

  //////////////////////////////////////////////////////////////////////////////
  // Face relationships.

  // Array of edge indices the face consists of.
  // Aligned with the vertex indices array, edge i connects face vertex i
  // with face index i+1.
  void (*getFaceEdges)(const OpenSubdiv_Converter *converter,
                       const int face_index,
                       int *face_edges);

  //////////////////////////////////////////////////////////////////////////////
  // Edge relationships.

  // Number of faces which are sharing the given edge.
  int (*getNumEdgeFaces)(const OpenSubdiv_Converter *converter, const int edge_index);
  // Array of face indices which are sharing the given edge.
  void (*getEdgeFaces)(const OpenSubdiv_Converter *converter, const int edge, int *edge_faces);

  //////////////////////////////////////////////////////////////////////////////
  // Vertex relationships.

  // Number of edges which are adjacent to the given vertex.
  int (*getNumVertexEdges)(const OpenSubdiv_Converter *converter, const int vertex_index);
  // Array for edge indices which are adjacent to the given vertex.
  void (*getVertexEdges)(const OpenSubdiv_Converter *converter,
                         const int vertex_index,
                         int *vertex_edges);
  // Number of faces which are adjacent to the given vertex.
  int (*getNumVertexFaces)(const OpenSubdiv_Converter *converter, const int vertex_index);
  // Array for face indices which are adjacent to the given vertex.
  void (*getVertexFaces)(const OpenSubdiv_Converter *converter,
                         const int vertex_index,
                         int *vertex_faces);

  /////////////////////////////////////
  // UV coordinates.

  // Number of UV layers.
  int (*getNumUVLayers)(const OpenSubdiv_Converter *converter);

  // We need some corner connectivity information, which might not be trivial
  // to be gathered (might require multiple matching calculations per corver
  // query).
  // precalc() is called before any corner connectivity or UV coordinate is
  // queried from the given layer, allowing converter to calculate and cache
  // complex complex-to-calculate information.
  // finish() is called after converter is done porting UV layer to OpenSubdiv,
  // allowing to free cached data.
  void (*precalcUVLayer)(const OpenSubdiv_Converter *converter, const int layer_index);
  void (*finishUVLayer)(const OpenSubdiv_Converter *converter);

  // Get number of UV coordinates in the current layer (layer which was
  // specified in precalcUVLayer().
  int (*getNumUVCoordinates)(const OpenSubdiv_Converter *converter);
  // For the given face index and its corner (known as loop in Blender)
  // get corresponding UV coordinate index.
  int (*getFaceCornerUVIndex)(const OpenSubdiv_Converter *converter,
                              const int face_index,
                              const int corner_index);

  //////////////////////////////////////////////////////////////////////////////
  // User data associated with this converter.

  void (*freeUserData)(const OpenSubdiv_Converter *converter);
  void *user_data;
};
