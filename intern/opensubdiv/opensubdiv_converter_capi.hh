/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>  // for bool

#include "BLI_offset_indices.hh"

#include "opensubdiv_capi_type.hh"

struct OpenSubdiv_Converter {
  /**
   * The face topology of the base mesh to be subdivided. See #Mesh::faces() documentation for the
   * details.
   *
   * Other topology information is currently encoded with callbacks rather than arrays directly.
   */
  blender::OffsetIndices<int> faces;

  OpenSubdiv_SchemeType (*getSchemeType)(const OpenSubdiv_Converter *converter);

  OpenSubdiv_VtxBoundaryInterpolation (*getVtxBoundaryInterpolation)(
      const OpenSubdiv_Converter *converter);
  OpenSubdiv_FVarLinearInterpolation (*getFVarLinearInterpolation)(
      const OpenSubdiv_Converter *converter);

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
  // Global geometry counters.

  // Number of faces/edges/vertices in the base mesh.
  int (*getNumEdges)(const OpenSubdiv_Converter *converter);
  int (*getNumVertices)(const OpenSubdiv_Converter *converter);

  //////////////////////////////////////////////////////////////////////////////
  // Face relationships.

  // Array of vertex indices the face consists of.
  void (*getFaceVertices)(const OpenSubdiv_Converter *converter,
                          const int face_index,
                          int *face_vertices);
  // Array of edge indices the face consists of.
  // Aligned with the vertex indices array, edge i connects face vertex i
  // with face index i+1.
  void (*getFaceEdges)(const OpenSubdiv_Converter *converter,
                       const int face_index,
                       int *face_edges);

  //////////////////////////////////////////////////////////////////////////////
  // Edge relationships.

  // Vertices the edge consists of.
  void (*getEdgeVertices)(const OpenSubdiv_Converter *converter,
                          const int edge_index,
                          int edge_vertices[2]);
  // Number of faces which are sharing the given edge.
  int (*getNumEdgeFaces)(const OpenSubdiv_Converter *converter, const int edge_index);
  // Array of face indices which are sharing the given edge.
  void (*getEdgeFaces)(const OpenSubdiv_Converter *converter, const int edge, int *edge_faces);
  // Edge sharpness (aka crease).
  float (*getEdgeSharpness)(const OpenSubdiv_Converter *converter, const int edge_index);

  //////////////////////////////////////////////////////////////////////////////
  // Vertex relationships.

  // Number of edges which are adjacent to the given vertex.
  int (*getNumVertexEdges)(const OpenSubdiv_Converter *converter, const int vertex_index);
  // Array fo edge indices which are adjacent to the given vertex.
  void (*getVertexEdges)(const OpenSubdiv_Converter *converter,
                         const int vertex_index,
                         int *vertex_edges);
  // Number of faces which are adjacent to the given vertex.
  int (*getNumVertexFaces)(const OpenSubdiv_Converter *converter, const int vertex_index);
  // Array fo face indices which are adjacent to the given vertex.
  void (*getVertexFaces)(const OpenSubdiv_Converter *converter,
                         const int vertex_index,
                         int *vertex_faces);

  // Check whether vertex is to be marked as an infinite sharp.
  // This is a way to make sharp vertices which are adjacent to a loose edges.
  bool (*isInfiniteSharpVertex)(const OpenSubdiv_Converter *converter, const int vertex_index);

  // If vertex is not infinitely sharp, this is its actual sharpness.
  float (*getVertexSharpness)(const OpenSubdiv_Converter *converter, const int vertex_index);

  //////////////////////////////////////////////////////////////////////////////
  // Face-varying data.

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
