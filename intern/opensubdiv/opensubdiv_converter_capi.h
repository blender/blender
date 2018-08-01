// Copyright 2015 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_CONVERTER_CAPI_H_
#define OPENSUBDIV_CONVERTER_CAPI_H_

#include <stdint.h>  // for bool

#include "opensubdiv_capi_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenSubdiv_Converter {
  OpenSubdiv_SchemeType (*getSchemeType)(
      const struct OpenSubdiv_Converter* converter);

  OpenSubdiv_FVarLinearInterpolation (*getFVarLinearInterpolation)(
      const struct OpenSubdiv_Converter* converter);

  // Denotes whether this converter specifies full topology, which includes
  // vertices, edges, faces, vertices+edges of a face and edges/faces of a
  // vertex.
  // Otherwise this converter will only provide number of vertices and faces,
  // and vertices of faces. The rest of topology will be created by OpenSubdiv.
  //
  // NOTE: Even if converter does not provide full topology, it still needs
  // to provide number of edges and vertices-of-edge. Those are used to assign
  // topology tags.
  bool (*specifiesFullTopology)(const struct OpenSubdiv_Converter* converter);

  //////////////////////////////////////////////////////////////////////////////
  // Global geometry counters.

  // Number of faces/edges/vertices in the base mesh.
  int (*getNumFaces)(const struct OpenSubdiv_Converter* converter);
  int (*getNumEdges)(const struct OpenSubdiv_Converter* converter);
  int (*getNumVertices)(const struct OpenSubdiv_Converter* converter);

  //////////////////////////////////////////////////////////////////////////////
  // Face relationships.

  // Number of vertices the face consists of.
  int (*getNumFaceVertices)(const struct OpenSubdiv_Converter* converter,
                            const int face_index);
  // Array of vertex indices the face consists of.
  void (*getFaceVertices)(const struct OpenSubdiv_Converter* converter,
                          const int face_index,
                          int* face_vertices);
  // Array of edge indices the face consists of.
  // Aligned with the vertex indices array, edge i connects face vertex i
  // with face index i+1.
  void (*getFaceEdges)(const struct OpenSubdiv_Converter *converter,
                       const int face_index,
                       int *face_edges);

  //////////////////////////////////////////////////////////////////////////////
  // Edge relationships.

  // Vertices the edge consists of.
  void (*getEdgeVertices)(const struct OpenSubdiv_Converter* converter,
                          const int edge_index,
                          int edge_vertices[2]);
  // Number of faces which are sharing the given edge.
  int (*getNumEdgeFaces)(const struct OpenSubdiv_Converter* converter,
                         const int edge_index);
  // Array of face indices which are sharing the given edge.
  void (*getEdgeFaces)(const struct OpenSubdiv_Converter* converter,
                       const int edge,
                       int* edge_faces);
  // Edge sharpness (aka crease).
  float (*getEdgeSharpness)(const struct OpenSubdiv_Converter* converter,
                            const int edge_index);

  //////////////////////////////////////////////////////////////////////////////
  // Vertex relationships.

  // Number of edges which are adjacent to the given vertex.
  int (*getNumVertexEdges)(const struct OpenSubdiv_Converter* converter,
                           const int vertex_index);
  // Array fo edge indices which are adjacent to the given vertex.
  void (*getVertexEdges)(const struct OpenSubdiv_Converter* converter,
                         const int vertex_index,
                         int* vertex_edges);
  // Number of faces which are adjacent to the given vertex.
  int (*getNumVertexFaces)(const struct OpenSubdiv_Converter* converter,
                           const int vertex_index);
  // Array fo face indices which are adjacent to the given vertex.
  void (*getVertexFaces)(const struct OpenSubdiv_Converter* converter,
                         const int vertex_index,
                         int* vertex_faces);

  // Check whether vertex is to be marked as an infinite sharp.
  // This is a way to make sharp vertices which are adjacent to a loose edges.
  bool (*isInfiniteSharpVertex)(const struct OpenSubdiv_Converter* converter,
                                const int vertex_index);

  //////////////////////////////////////////////////////////////////////////////
  // Face-varying data.

  /////////////////////////////////////
  // UV coordinates.

  // Number of UV layers.
  int (*getNumUVLayers)(const struct OpenSubdiv_Converter* converter);

  // We need some corner connectivity information, which might not be trivial
  // to be gathered (might require multiple matching calculations per corver
  // query).
  // precalc() is called before any corner connectivity or UV coordinate is
  // queried from the given layer, allowing converter to calculate and cache
  // complex complex-to-calculate information.
  // finish() is called after converter is done porting UV layer to OpenSubdiv,
  // allowing to free cached data.
  void (*precalcUVLayer)(const struct OpenSubdiv_Converter* converter,
                         const int layer_index);
  void (*finishUVLayer)(const struct OpenSubdiv_Converter* converter);

  // Get number of UV coordinates in the current layer (layer which was
  // specified in precalcUVLayer().
  int (*getNumUVCoordinates)(const struct OpenSubdiv_Converter* converter);
  // For the given face index and its corner (known as loop in Blender)
  // get corrsponding UV coordinate index.
  int (*getFaceCornerUVIndex)(const struct OpenSubdiv_Converter* converter,
                              const int face_index,
                              const int corner_index);

  //////////////////////////////////////////////////////////////////////////////
  // User data associated with this converter.

  void (*freeUserData)(const struct OpenSubdiv_Converter* converter);
  void* user_data;
} OpenSubdiv_Converter;

#ifdef __cplusplus
}
#endif

#endif /* OPENSUBDIV_CONVERTER_CAPI_H_ */
