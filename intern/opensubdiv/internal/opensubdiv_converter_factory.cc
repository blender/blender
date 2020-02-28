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

#include <opensubdiv/far/topologyRefinerFactory.h>

#include "internal/opensubdiv_converter_internal.h"
#include "internal/opensubdiv_converter_orient.h"
#include "internal/opensubdiv_internal.h"
#include "internal/opensubdiv_util.h"
#include "opensubdiv_converter_capi.h"

using opensubdiv_capi::min;
using opensubdiv_capi::stack;
using opensubdiv_capi::vector;

struct TopologyRefinerData {
  const OpenSubdiv_Converter *converter;
};

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Far {

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::resizeComponentTopology(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  const OpenSubdiv_Converter *converter = cb_data.converter;
  // Faces and face-vertices.
  const int num_faces = converter->getNumFaces(converter);
  setNumBaseFaces(refiner, num_faces);
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const int num_face_vertices = converter->getNumFaceVertices(converter, face_index);
    setNumBaseFaceVertices(refiner, face_index, num_face_vertices);
  }
  // Vertices.
  const int num_vertices = converter->getNumVertices(converter);
  setNumBaseVertices(refiner, num_vertices);
  // If converter does not provide full topology, we are done.
  if (!converter->specifiesFullTopology(converter)) {
    return true;
  }
  // Edges and edge-faces.
  const int num_edges = converter->getNumEdges(converter);
  setNumBaseEdges(refiner, num_edges);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const int num_edge_faces = converter->getNumEdgeFaces(converter, edge_index);
    setNumBaseEdgeFaces(refiner, edge_index, num_edge_faces);
  }
  // Vertex-faces and vertex-edges.
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    const int num_vert_edges = converter->getNumVertexEdges(converter, vertex_index);
    const int num_vert_faces = converter->getNumVertexFaces(converter, vertex_index);
    setNumBaseVertexEdges(refiner, vertex_index, num_vert_edges);
    setNumBaseVertexFaces(refiner, vertex_index, num_vert_faces);
  }
  return true;
}

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::assignComponentTopology(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  using Far::IndexArray;
  const OpenSubdiv_Converter *converter = cb_data.converter;
  const bool full_topology_specified = converter->specifiesFullTopology(converter);
  // Face relations.
  const int num_faces = converter->getNumFaces(converter);
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    IndexArray dst_face_verts = getBaseFaceVertices(refiner, face_index);
    converter->getFaceVertices(converter, face_index, &dst_face_verts[0]);
    if (full_topology_specified) {
      IndexArray dst_face_edges = getBaseFaceEdges(refiner, face_index);
      converter->getFaceEdges(converter, face_index, &dst_face_edges[0]);
    }
  }
  // If converter does not provide full topology, we are done.
  if (!full_topology_specified) {
    return true;
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
  // Vertex relations.
  const int num_vertices = converter->getNumVertices(converter);
  vector<int> vertex_faces, vertex_edges;
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    // Vertex-faces.
    IndexArray dst_vertex_faces = getBaseVertexFaces(refiner, vertex_index);
    const int num_vertex_faces = converter->getNumVertexFaces(converter, vertex_index);
    vertex_faces.resize(num_vertex_faces);
    converter->getVertexFaces(converter, vertex_index, &vertex_faces[0]);
    // Vertex-edges.
    IndexArray dst_vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    const int num_vertex_edges = converter->getNumVertexEdges(converter, vertex_index);
    vertex_edges.resize(num_vertex_edges);
    converter->getVertexEdges(converter, vertex_index, &vertex_edges[0]);
    memcpy(&dst_vertex_edges[0], &vertex_edges[0], sizeof(int) * num_vertex_edges);
    memcpy(&dst_vertex_faces[0], &vertex_faces[0], sizeof(int) * num_vertex_faces);
  }
  populateBaseLocalIndices(refiner);
  return true;
}

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::assignComponentTags(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  using OpenSubdiv::Sdc::Crease;
  const OpenSubdiv_Converter *converter = cb_data.converter;
  const bool full_topology_specified = converter->specifiesFullTopology(converter);
  if (!full_topology_specified && converter->getNumEdges == NULL) {
    assert(converter->getEdgeSharpness == NULL);
    assert(converter->getVertexSharpness == NULL);
    assert(converter->isInfiniteSharpVertex == NULL);
    return true;
  }
  const int num_edges = converter->getNumEdges(converter);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const float sharpness = converter->getEdgeSharpness(converter, edge_index);
    if (sharpness < 1e-6f) {
      continue;
    }
    if (full_topology_specified) {
      setBaseEdgeSharpness(refiner, edge_index, sharpness);
    }
    else {
      int edge_vertices[2];
      converter->getEdgeVertices(converter, edge_index, edge_vertices);
      const int base_edge_index = findBaseEdge(refiner, edge_vertices[0], edge_vertices[1]);
      if (base_edge_index == OpenSubdiv::Far::INDEX_INVALID) {
        printf("OpenSubdiv Error: failed to find reconstructed edge\n");
        return false;
      }
      setBaseEdgeSharpness(refiner, base_edge_index, sharpness);
    }
  }
  // OpenSubdiv expects non-manifold vertices to be sharp but at the time it
  // handles correct cases when vertex is a corner of plane. Currently mark
  // vertices which are adjacent to a loose edge as sharp, but this decision
  // needs some more investigation.
  const int num_vertices = converter->getNumVertices(converter);
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    ConstIndexArray vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    if (converter->isInfiniteSharpVertex(converter, vertex_index)) {
      setBaseVertexSharpness(refiner, vertex_index, Crease::SHARPNESS_INFINITE);
      continue;
    }
    float sharpness = converter->getVertexSharpness(converter, vertex_index);
    if (vertex_edges.size() == 2) {
      const int edge0 = vertex_edges[0], edge1 = vertex_edges[1];
      const float sharpness0 = refiner._levels[0]->getEdgeSharpness(edge0);
      const float sharpness1 = refiner._levels[0]->getEdgeSharpness(edge1);
      // TODO(sergey): Find a better mixing between edge and vertex sharpness.
      sharpness += min(sharpness0, sharpness1);
      sharpness = min(sharpness, 10.0f);
    }
    setBaseVertexSharpness(refiner, vertex_index, sharpness);
  }
  return true;
}

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::assignFaceVaryingTopology(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  const OpenSubdiv_Converter *converter = cb_data.converter;
  if (converter->getNumUVLayers == NULL) {
    assert(converter->precalcUVLayer == NULL);
    assert(converter->getNumUVCoordinates == NULL);
    assert(converter->getFaceCornerUVIndex == NULL);
    assert(converter->finishUVLayer == NULL);
    return true;
  }
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
      Far::IndexArray dst_face_uvs = getBaseFaceFVarValues(refiner, face_index, channel);
      for (int corner = 0; corner < dst_face_uvs.size(); ++corner) {
        const int uv_index = converter->getFaceCornerUVIndex(converter, face_index, corner);
        dst_face_uvs[corner] = uv_index;
      }
    }
    converter->finishUVLayer(converter);
  }
  return true;
}

template<>
inline void TopologyRefinerFactory<TopologyRefinerData>::reportInvalidTopology(
    TopologyError /*errCode*/, const char *msg, const TopologyRefinerData & /*mesh*/)
{
  printf("OpenSubdiv Error: %s\n", msg);
}

} /* namespace Far */
} /* namespace OPENSUBDIV_VERSION */
} /* namespace OpenSubdiv */

namespace opensubdiv_capi {

namespace {

OpenSubdiv::Sdc::Options::VtxBoundaryInterpolation getVtxBoundaryInterpolationFromCAPI(
    OpenSubdiv_VtxBoundaryInterpolation boundary_interpolation)
{
  using OpenSubdiv::Sdc::Options;
  switch (boundary_interpolation) {
    case OSD_VTX_BOUNDARY_NONE:
      return Options::VTX_BOUNDARY_NONE;
    case OSD_VTX_BOUNDARY_EDGE_ONLY:
      return Options::VTX_BOUNDARY_EDGE_ONLY;
    case OSD_VTX_BOUNDARY_EDGE_AND_CORNER:
      return Options::VTX_BOUNDARY_EDGE_AND_CORNER;
  }
  assert(!"Unknown veretx boundary interpolation.");
  return Options::VTX_BOUNDARY_EDGE_ONLY;
}

}  // namespace

OpenSubdiv::Far::TopologyRefiner *createOSDTopologyRefinerFromConverter(
    OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyRefinerFactory;
  using OpenSubdiv::Sdc::Options;
  const OpenSubdiv::Sdc::SchemeType scheme_type = getSchemeTypeFromCAPI(
      converter->getSchemeType(converter));
  const Options::FVarLinearInterpolation linear_interpolation = getFVarLinearInterpolationFromCAPI(
      converter->getFVarLinearInterpolation(converter));
  Options options;
  options.SetVtxBoundaryInterpolation(
      getVtxBoundaryInterpolationFromCAPI(converter->getVtxBoundaryInterpolation(converter)));
  options.SetCreasingMethod(Options::CREASE_UNIFORM);
  options.SetFVarLinearInterpolation(linear_interpolation);

  TopologyRefinerFactory<TopologyRefinerData>::Options topology_options(scheme_type, options);
#ifdef OPENSUBDIV_VALIDATE_TOPOLOGY
  topology_options.validateFullTopology = true;
#endif
  TopologyRefinerData cb_data;
  cb_data.converter = converter;
  return TopologyRefinerFactory<TopologyRefinerData>::Create(cb_data, topology_options);
}

}  // namespace opensubdiv_capi
