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

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include "internal/opensubdiv_converter_factory.h"

#include <cassert>
#include <cstdio>
#include <stack>
#include <vector>

#include <opensubdiv/far/topologyRefinerFactory.h>

#include "internal/opensubdiv_converter_internal.h"
#include "internal/opensubdiv_converter_orient.h"
#include "internal/opensubdiv_internal.h"
#include "opensubdiv_converter_capi.h"

struct TopologyRefinerData {
  const OpenSubdiv_Converter* converter;
};

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Far {

template <>
inline bool
TopologyRefinerFactory<TopologyRefinerData>::resizeComponentTopology(
    TopologyRefiner& refiner,
    const TopologyRefinerData& cb_data) {
  const OpenSubdiv_Converter* converter = cb_data.converter;
  /// Faces and face-vertices.
  const int num_faces = converter->getNumFaces(converter);
  setNumBaseFaces(refiner, num_faces);
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const int num_face_vertices =
        converter->getNumFaceVertices(converter, face_index);
    setNumBaseFaceVertices(refiner, face_index, num_face_vertices);
  }
  // Edges and edge-faces.
  const int num_edges = converter->getNumEdges(converter);
  setNumBaseEdges(refiner, num_edges);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const int num_edge_faces =
        converter->getNumEdgeFaces(converter, edge_index);
    setNumBaseEdgeFaces(refiner, edge_index, num_edge_faces);
  }
  // Vertices and vertex-faces and vertex-edges.
  const int num_vertices = converter->getNumVertices(converter);
  setNumBaseVertices(refiner, num_vertices);
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    const int num_vert_edges =
        converter->getNumVertexEdges(converter, vertex_index);
    const int num_vert_faces =
        converter->getNumVertexFaces(converter, vertex_index);
    setNumBaseVertexEdges(refiner, vertex_index, num_vert_edges);
    setNumBaseVertexFaces(refiner, vertex_index, num_vert_faces);
  }
  return true;
}

template <>
inline bool
TopologyRefinerFactory<TopologyRefinerData>::assignComponentTopology(
    TopologyRefiner& refiner,
    const TopologyRefinerData& cb_data) {
  using Far::IndexArray;
  const OpenSubdiv_Converter* converter = cb_data.converter;
  // Face relations.
  const int num_faces = converter->getNumFaces(converter);
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    IndexArray dst_face_verts = getBaseFaceVertices(refiner, face_index);
    converter->getFaceVertices(converter, face_index, &dst_face_verts[0]);
    IndexArray dst_face_edges = getBaseFaceEdges(refiner, face_index);
    converter->getFaceEdges(converter, face_index, &dst_face_edges[0]);
  }
  // Edge relations.
  const int num_edges = converter->getNumEdges(converter);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    // Edge-vertices.
    IndexArray dst_edge_vertices = getBaseEdgeVertices(refiner, edge_index);
    converter->getEdgeVertices(converter, edge_index, &dst_edge_vertices[0]);
    // Edge-faces.
    IndexArray dst_edge_faces = getBaseEdgeFaces(refiner, edge_index);
    converter->getEdgeFaces(converter, edge_index, &dst_edge_faces[0]);
  }
// TODO(sergey): Find a way to move this to an utility function.
#ifdef OPENSUBDIV_ORIENT_TOPOLOGY
  // Make face normals consistent.
  std::vector<bool> face_used(num_faces, false);
  std::stack<int> traverse_stack;
  int face_start = 0, num_traversed_faces = 0;
  // Traverse all islands.
  while (num_traversed_faces != num_faces) {
    // Find first face of any untraversed islands.
    while (face_used[face_start]) {
      ++face_start;
    }
    // Add first face to the stack.
    traverse_stack.push(face_start);
    face_used[face_start] = true;
    // Go over whole connected component.
    while (!traverse_stack.empty()) {
      int face = traverse_stack.top();
      traverse_stack.pop();
      IndexArray face_edges = getBaseFaceEdges(refiner, face);
      ConstIndexArray face_vertices = getBaseFaceVertices(refiner, face);
      for (int i = 0; i < face_edges.size(); ++i) {
        const int edge = face_edges[i];
        ConstIndexArray edge_faces = getBaseEdgeFaces(refiner, edge);
        if (edge_faces.size() != 2) {
          /* Can't make consistent normals for non-manifolds. */
          continue;
        }
        ConstIndexArray edge_vertices = getBaseEdgeVertices(refiner, edge);
        // Get winding of the reference face.
        const int vert0_of_face = face_vertices.FindIndex(edge_vertices[0]);
        const int vert1_of_face = face_vertices.FindIndex(edge_vertices[1]);
        const int delta_face =
            opensubdiv_capi::getLoopWinding(vert0_of_face, vert1_of_face);
        for (int edge_face = 0; edge_face < edge_faces.size(); ++edge_face) {
          const int other_face_index = edge_faces[edge_face];
          // Never re-traverse faces, only move forward.
          if (face_used[other_face_index]) {
            continue;
          }
          IndexArray other_face_vertics =
              getBaseFaceVertices(refiner, other_face_index);
          const int vert0_of_other_face =
              other_face_vertics.FindIndex(edge_vertices[0]);
          const int vert1_of_other_face =
              other_face_vertics.FindIndex(edge_vertices[1]);
          const int delta_other_face = opensubdiv_capi::getLoopWinding(
              vert0_of_other_face, vert1_of_other_face);
          if (delta_face * delta_other_face > 0) {
            IndexArray other_face_vertices =
                getBaseFaceVertices(refiner, other_face_index);
            IndexArray other_face_edges =
                getBaseFaceEdges(refiner, other_face_index);
            opensubdiv_capi::reverseFaceLoops(&other_face_vertices,
                                              &other_face_edges);
          }
          traverse_stack.push(other_face_index);
          face_used[other_face_index] = true;
        }
      }
      ++num_traversed_faces;
    }
  }
#endif  // OPENSUBDIV_ORIENT_TOPOLOGY
  // Vertex relations.
  const int num_vertices = converter->getNumVertices(converter);
  std::vector<int> vertex_faces, vertex_edges;
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    // Vertex-faces.
    IndexArray dst_vertex_faces = getBaseVertexFaces(refiner, vertex_index);
    const int num_vertex_faces =
        converter->getNumVertexFaces(converter, vertex_index);
    vertex_faces.resize(num_vertex_faces);
    converter->getVertexFaces(converter, vertex_index, &vertex_faces[0]);
    // Vertex-edges.
    IndexArray dst_vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    const int num_vertex_edges =
        converter->getNumVertexEdges(converter, vertex_index);
    vertex_edges.resize(num_vertex_edges);
    converter->getVertexEdges(converter, vertex_index, &vertex_edges[0]);
// TODO(sergey): Find a way to move this to an utility function.
#ifdef OPENSUBDIV_ORIENT_TOPOLOGY
    // Order vertex edges and faces to be in a CCW order.
    std::fill(face_used.begin(), face_used.end(), false);
    // Number of edges and faces added to the ordered array.
    int edge_count_ordered = 0, face_count_ordered = 0;
    // Add loose edges straight into the edges array.
    bool has_fan_connections = false;
    for (int i = 0; i < num_vertex_edges; ++i) {
      IndexArray edge_faces = getBaseEdgeFaces(refiner, vertex_edges[i]);
      if (edge_faces.size() == 0) {
        dst_vertex_edges[edge_count_ordered++] = vertex_edges[i];
      } else if (edge_faces.size() > 2) {
        has_fan_connections = true;
      }
    }
    if (has_fan_connections) {
      // OpenSubdiv currently doesn't give us clues how to handle fan face
      // connections. and since handling such connections complicates the loop
      // below we simply don't do special orientation for them.
      memcpy(&dst_vertex_edges[0], &vertex_edges[0],
             sizeof(int) * num_vertex_edges);
      memcpy(&dst_vertex_faces[0], &vertex_faces[0],
             sizeof(int) * num_vertex_faces);
      continue;
    }
    // Perform at max numbder of vert-edges iteration and try to avoid
    // deadlock here for malformed mesh.
    for (int global_iter = 0; global_iter < num_vertex_edges; ++global_iter) {
      // Number of edges and faces which are still to be ordered.
      const int num_vertex_edges_remained =
          num_vertex_edges - edge_count_ordered;
      const int num_vertex_faces_remained =
          num_vertex_faces - face_count_ordered;
      if (num_vertex_edges_remained == 0 && num_vertex_faces_remained == 0) {
        // All done, nothing to do anymore.
        break;
      }
      // Face, edge and face-vertex index to start traversal from.
      int face_start = -1, edge_start = -1, face_vertex_start = -1;
      if (num_vertex_edges_remained == num_vertex_faces_remained) {
        // Vertex is either complete manifold or is connected to several
        // manifold islands (hourglass-like configuration), can pick up
        // random edge unused and start from it.
        //
        // TODO(sergey): Start from previous edge from which traversal began at
        // previous iteration.
        for (int i = 0; i < num_vertex_edges; ++i) {
          face_start = vertex_faces[i];
          if (!face_used[face_start]) {
            ConstIndexArray face_vertices =
                getBaseFaceVertices(refiner, face_start);
            ConstIndexArray face_edges = getBaseFaceEdges(refiner, face_start);
            face_vertex_start = face_vertices.FindIndex(vertex_index);
            edge_start = face_edges[face_vertex_start];
            break;
          }
        }
      } else {
        // Special handle of non-manifold vertex.
        for (int i = 0; i < num_vertex_edges; ++i) {
          edge_start = vertex_edges[i];
          IndexArray edge_faces = getBaseEdgeFaces(refiner, edge_start);
          if (edge_faces.size() == 1) {
            face_start = edge_faces[0];
            if (!face_used[face_start]) {
              ConstIndexArray face_vertices =
                  getBaseFaceVertices(refiner, face_start);
              ConstIndexArray face_edges =
                  getBaseFaceEdges(refiner, face_start);
              face_vertex_start = face_vertices.FindIndex(vertex_index);
              if (edge_start == face_edges[face_vertex_start]) {
                break;
              }
            }
          }
          // Reset indices for sanity check below.
          face_start = edge_start = face_vertex_start = -1;
        }
      }
      // Sanity check.
      assert(face_start != -1);
      assert(edge_start != -1);
      assert(face_vertex_start != -1);
      // Traverse faces starting from the current one. */
      int edge_first = edge_start;
      dst_vertex_faces[face_count_ordered++] = face_start;
      dst_vertex_edges[edge_count_ordered++] = edge_start;
      face_used[face_start] = true;
      while (edge_count_ordered < num_vertex_edges) {
        IndexArray face_vertices = getBaseFaceVertices(refiner, face_start);
        IndexArray face_edges = getBaseFaceEdges(refiner, face_start);
        int face_edge_start = face_vertex_start;
        int face_edge_next = (face_edge_start > 0) ? (face_edge_start - 1)
                                                   : (face_vertices.size() - 1);
        Index edge_next = face_edges[face_edge_next];
        if (edge_next == edge_first) {
          // Multiple manifolds found, stop for now and handle rest
          // in the next iteration.
          break;
        }
        dst_vertex_edges[edge_count_ordered++] = edge_next;
        if (face_count_ordered < num_vertex_faces) {
          IndexArray edge_faces = getBaseEdgeFaces(refiner, edge_next);
          assert(edge_faces.size() != 0);
          if (edge_faces.size() == 1) {
            assert(edge_faces[0] == face_start);
            break;
          } else if (edge_faces.size() != 2) {
            break;
          }
          assert(edge_faces.size() == 2);
          face_start = edge_faces[(edge_faces[0] == face_start) ? 1 : 0];
          face_vertex_start =
              getBaseFaceEdges(refiner, face_start).FindIndex(edge_next);
          dst_vertex_faces[face_count_ordered++] = face_start;
          face_used[face_start] = true;
        }
        edge_start = edge_next;
      }
    }
    // Verify ordering doesn't ruin connectivity information.
    assert(face_count_ordered == num_vertex_faces);
    assert(edge_count_ordered == num_vertex_edges);
    opensubdiv_capi::checkOrientedVertexConnectivity(
        num_vertex_edges, num_vertex_faces, &vertex_edges[0], &vertex_faces[0],
        &dst_vertex_edges[0], &dst_vertex_faces[0]);
    // For the release builds we're failing mesh construction so instead of
    // nasty bugs the unsupported mesh will simply disappear from the viewport.
    if (face_count_ordered != num_vertex_faces ||
        edge_count_ordered != num_vertex_edges) {
      return false;
    }
#else   // OPENSUBDIV_ORIENT_TOPOLOGY
    memcpy(&dst_vertex_edges[0], &vertex_edges[0],
           sizeof(int) * num_vertex_edges);
    memcpy(&dst_vertex_faces[0], &vertex_faces[0],
           sizeof(int) * num_vertex_faces);
#endif  // OPENSUBDIV_ORIENT_TOPOLOGY
  }
  populateBaseLocalIndices(refiner);
  return true;
}

template <>
inline bool TopologyRefinerFactory<TopologyRefinerData>::assignComponentTags(
    TopologyRefiner& refiner,
    const TopologyRefinerData& cb_data) {
  using OpenSubdiv::Sdc::Crease;
  const OpenSubdiv_Converter* converter = cb_data.converter;
  const int num_edges = converter->getNumEdges(converter);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const float sharpness =
        opensubdiv_capi::getCompatibleEdgeSharpness(converter, edge_index);
    setBaseEdgeSharpness(refiner, edge_index, sharpness);
  }
  // OpenSubdiv expects non-manifold vertices to be sharp but at the time it
  // handles correct cases when vertex is a corner of plane. Currently mark
  // vertices which are adjacent to a loose edge as sharp, but this decision
  // needs some more investigation.
  const int num_vertices = converter->getNumVertices(converter);
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    ConstIndexArray vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    if (converter->isInfiniteSharpVertex(converter, vertex_index)) {
      setBaseVertexSharpness(
          refiner, vertex_index, Crease::SHARPNESS_INFINITE);
    } else if (vertex_edges.size() == 2) {
      const int edge0 = vertex_edges[0], edge1 = vertex_edges[1];
      const float sharpness0 = converter->getEdgeSharpness(converter, edge0);
      const float sharpness1 = converter->getEdgeSharpness(converter, edge1);
      const float sharpness = std::min(sharpness0, sharpness1);
      setBaseVertexSharpness(refiner, vertex_index, sharpness);
    }
  }
  return true;
}

template <>
inline bool
TopologyRefinerFactory<TopologyRefinerData>::assignFaceVaryingTopology(
    TopologyRefiner& refiner,
    const TopologyRefinerData& cb_data) {
  const OpenSubdiv_Converter* converter = cb_data.converter;
  const int num_layers = converter->getNumUVLayers(converter);
  if (num_layers <= 0) {
    // No UV maps, we can skip any face-varying data.
    return true;
  }
  const int num_faces = getNumBaseFaces(refiner);
  for (int layer_index = 0; layer_index < num_layers; ++layer_index) {
    converter->precalcUVLayer(converter, layer_index);
    const int num_uvs = converter->getNumUVCoordinates(converter);
    // Fill in per-corner index of the UV.
    const int channel = createBaseFVarChannel(refiner, num_uvs);
    // TODO(sergey): Need to check whether converter changed the winding of
    // face to match OpenSubdiv's expectations.
    for (int face_index = 0; face_index < num_faces; ++face_index) {
      Far::IndexArray dst_face_uvs =
          getBaseFaceFVarValues(refiner, face_index, channel);
      for (int corner = 0; corner < dst_face_uvs.size(); ++corner) {
        const int uv_index =
           converter->getFaceCornerUVIndex(converter, face_index, corner);
        dst_face_uvs[corner] = uv_index;
      }
    }
    converter->finishUVLayer(converter);
  }
  return true;
}

template <>
inline void TopologyRefinerFactory<TopologyRefinerData>::reportInvalidTopology(
    TopologyError /*errCode*/, const char* msg,
    const TopologyRefinerData& /*mesh*/) {
  printf("OpenSubdiv Error: %s\n", msg);
}

} /* namespace Far */
} /* namespace OPENSUBDIV_VERSION */
} /* namespace OpenSubdiv */

namespace opensubdiv_capi {

OpenSubdiv::Far::TopologyRefiner* createOSDTopologyRefinerFromConverter(
    OpenSubdiv_Converter* converter) {
  using OpenSubdiv::Sdc::Options;
  using OpenSubdiv::Far::TopologyRefinerFactory;
  const OpenSubdiv::Sdc::SchemeType scheme_type =
      getSchemeTypeFromCAPI(converter->getSchemeType(converter));
  const Options::FVarLinearInterpolation linear_interpolation =
      getFVarLinearInterpolationFromCAPI(
          converter->getFVarLinearInterpolation(converter));
  Options options;
  options.SetVtxBoundaryInterpolation(Options::VTX_BOUNDARY_EDGE_ONLY);
  options.SetCreasingMethod(Options::CREASE_UNIFORM);
  options.SetFVarLinearInterpolation(linear_interpolation);

  TopologyRefinerFactory<TopologyRefinerData>::Options topology_options(
      scheme_type, options);
#ifdef OPENSUBDIV_VALIDATE_TOPOLOGY
  topology_options.validateFullTopology = true;
#endif
  TopologyRefinerData cb_data;
  cb_data.converter = converter;
  return TopologyRefinerFactory<TopologyRefinerData>::Create(
      cb_data, topology_options);
}

}  // namespace opensubdiv_capi
