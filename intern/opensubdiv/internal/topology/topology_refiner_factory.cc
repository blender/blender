/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>

#include <opensubdiv/far/topologyRefinerFactory.h>

#include "internal/base/type_convert.h"

#include "opensubdiv_converter_capi.hh"
#include "opensubdiv_topology_refiner.hh"

struct TopologyRefinerData {
  const OpenSubdiv_Converter *converter;
};

// See #OffsetIndices::size().
static int faces_num_from_offsets(const std::span<const int> face_offsets)
{
  return face_offsets.empty() ? 0 : int(face_offsets.size()) - 1;
}

// See #OffsetIndices::total_size().
[[maybe_unused]] static int total_size_from_offsets(const std::span<const int> face_offsets)
{
  return face_offsets.empty() ? 0 : face_offsets.back() - face_offsets.front();
}

using TopologyRefinerFactoryType = OpenSubdiv::Far::TopologyRefinerFactory<TopologyRefinerData>;

namespace OpenSubdiv::OPENSUBDIV_VERSION::Far {

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::resizeComponentTopology(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  const OpenSubdiv_Converter *converter = cb_data.converter;

  // Vertices.
  setNumBaseVertices(refiner, converter->verts_num);

  // Faces and face-vertices.
  const std::span<const int> face_offsets = converter->face_offsets;
  const int num_faces = faces_num_from_offsets(face_offsets);
  setNumBaseFaces(refiner, num_faces);
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    setNumBaseFaceVertices(
        refiner, face_index, face_offsets[face_index + 1] - face_offsets[face_index]);
  }

  // If converter does not provide full topology, we are done.
  //
  // The rest is needed to define relations between faces-of-edge and
  // edges-of-vertex, which is not available for partially specified mesh.
  if (!converter->specifiesFullTopology(converter)) {
    return true;
  }

  // Edges and edge-faces.
  const int num_edges = converter->edges.size();
  setNumBaseEdges(refiner, num_edges);
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const int num_edge_faces = converter->getNumEdgeFaces(converter, edge_index);
    setNumBaseEdgeFaces(refiner, edge_index, num_edge_faces);
  }

  // Vertex-faces and vertex-edges.
  for (int vertex_index = 0; vertex_index < converter->verts_num; ++vertex_index) {
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

  const std::span<const int> face_offsets = converter->face_offsets;
  const std::span<const int> src_corner_verts = converter->corner_verts;
  const int num_faces = faces_num_from_offsets(face_offsets);

  // Vertices of face.
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const int start = face_offsets[face_index];
    const std::span<const int> face_verts = src_corner_verts.subspan(
        start, face_offsets[face_index + 1] - start);
    IndexArray dst_face_verts = getBaseFaceVertices(refiner, face_index);
    std::copy_n(face_verts.data(), face_verts.size(), dst_face_verts.begin());
  }

  // If converter does not provide full topology, we are done.
  //
  // The rest is needed to define relations between faces-of-edge and
  // edges-of-vertex, which is not available for partially specified mesh.
  if (!full_topology_specified) {
    return true;
  }

  // Vertex relations.
  std::vector<int> vertex_faces, vertex_edges;
  for (int vertex_index = 0; vertex_index < converter->verts_num; ++vertex_index) {
    // Vertex-faces.
    IndexArray dst_vertex_faces = getBaseVertexFaces(refiner, vertex_index);
    const int num_vertex_faces = converter->getNumVertexFaces(converter, vertex_index);
    vertex_faces.resize(num_vertex_faces);
    converter->getVertexFaces(converter, vertex_index, vertex_faces.data());

    // Vertex-edges.
    IndexArray dst_vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    const int num_vertex_edges = converter->getNumVertexEdges(converter, vertex_index);
    vertex_edges.resize(num_vertex_edges);
    converter->getVertexEdges(converter, vertex_index, vertex_edges.data());
    memcpy(&dst_vertex_edges[0], vertex_edges.data(), sizeof(int) * num_vertex_edges);
    memcpy(&dst_vertex_faces[0], vertex_faces.data(), sizeof(int) * num_vertex_faces);
  }

  // Edge relations.
  const std::span<const std::pair<int, int>> edges = converter->edges;
  for (int edge_index = 0; edge_index < int(edges.size()); ++edge_index) {
    // Vertices this edge connects.
    IndexArray dst_edge_vertices = getBaseEdgeVertices(refiner, edge_index);
    dst_edge_vertices[0] = edges[edge_index].first;
    dst_edge_vertices[1] = edges[edge_index].second;

    // Faces adjacent to this edge.
    IndexArray dst_edge_faces = getBaseEdgeFaces(refiner, edge_index);
    converter->getEdgeFaces(converter, edge_index, &dst_edge_faces[0]);
  }

  // Face relations.
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    IndexArray dst_face_edges = getBaseFaceEdges(refiner, face_index);
    converter->getFaceEdges(converter, face_index, &dst_face_edges[0]);
  }

  populateBaseLocalIndices(refiner);

  return true;
}

template<>
inline bool TopologyRefinerFactory<TopologyRefinerData>::assignComponentTags(
    TopologyRefiner &refiner, const TopologyRefinerData &cb_data)
{
  using OpenSubdiv::Sdc::Crease;

  /* Not static assert because Crease::SHARPNESS_INFINITE is not constexpr. */
  assert(OPENSUBDIV_SHARPNESS_INFINITE == Crease::SHARPNESS_INFINITE);

  const OpenSubdiv_Converter *converter = cb_data.converter;

  const bool full_topology_specified = converter->specifiesFullTopology(converter);
  const std::span<const float> edge_sharpness = converter->edge_sharpness;
  for (int edge_index = 0; edge_index < int(edge_sharpness.size()); ++edge_index) {
    const float sharpness = edge_sharpness[edge_index];
    if (sharpness < 1e-6f) {
      continue;
    }

    if (full_topology_specified) {
      setBaseEdgeSharpness(refiner, edge_index, sharpness);
    }
    else {
      // TODO(sergey): Should be a faster way to find reconstructed edge to
      // specify sharpness for (assuming, findBaseEdge has linear complexity).
      const std::pair<int, int> edge_vertices = converter->edges[edge_index];
      const int base_edge_index = findBaseEdge(refiner, edge_vertices.first, edge_vertices.second);
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
  const std::span<const float> vert_sharpness = converter->vert_sharpness;
  for (int vertex_index = 0; vertex_index < converter->verts_num; ++vertex_index) {
    float sharpness = vert_sharpness.empty() ? 0.0f : vert_sharpness[vertex_index];

    // If its vertex where 2 non-manifold edges meet adjust vertex sharpness to
    // the edges.
    // This way having a plane with all 4 edges set to be sharp produces sharp
    // corners in the subdivided result.
    const ConstIndexArray vertex_edges = getBaseVertexEdges(refiner, vertex_index);
    if (!Crease::IsInfinite(sharpness) && vertex_edges.size() == 2) {
      const int edge0 = vertex_edges[0], edge1 = vertex_edges[1];
      const float sharpness0 = refiner._levels[0]->getEdgeSharpness(edge0);
      const float sharpness1 = refiner._levels[0]->getEdgeSharpness(edge1);
      // TODO(sergey): Find a better mixing between edge and vertex sharpness.
      sharpness += std::min(sharpness0, sharpness1);
      sharpness = std::min(sharpness, Crease::SHARPNESS_INFINITE);
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
  if (converter->getNumUVLayers == nullptr) {
    assert(converter->precalcUVLayer == nullptr);
    assert(converter->getNumUVCoordinates == nullptr);
    assert(converter->getFaceCornerUVIndex == nullptr);
    assert(converter->finishUVLayer == nullptr);
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

}  // namespace OpenSubdiv::OPENSUBDIV_VERSION::Far

namespace blender::opensubdiv {

static OpenSubdiv::Sdc::Options getSDCOptions(OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Sdc::Options;

  const Options::FVarLinearInterpolation linear_interpolation = getFVarLinearInterpolationFromCAPI(
      converter->fvar_linear_interpolation);

  Options options;
  options.SetVtxBoundaryInterpolation(
      getVtxBoundaryInterpolationFromCAPI(converter->vtx_boundary_interpolation));
  options.SetCreasingMethod(Options::CREASE_UNIFORM);
  options.SetFVarLinearInterpolation(linear_interpolation);

  return options;
}

static TopologyRefinerFactoryType::Options getTopologyRefinerOptions(
    OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Sdc::SchemeType;

  OpenSubdiv::Sdc::Options sdc_options = getSDCOptions(converter);

  const SchemeType scheme_type = getSchemeTypeFromCAPI(converter->scheme_type);
  TopologyRefinerFactoryType::Options topology_options(scheme_type, sdc_options);

  // NOTE: When debugging topology conversion related functionality it is helpful to set this
  // to truth. In all other cases leave it at false. so debugging of other areas is not affected
  // by performance penalty happening in this module.
  topology_options.validateFullTopology = false;

  return topology_options;
}

// Copy the topology of the base mesh out of the converter, so that a later converter can be
// compared against it.
static void storeBaseMeshTopology(const OpenSubdiv_Converter *converter,
                                  TopologyRefinerImpl &refiner_impl)
{
  refiner_impl.base_verts_num = converter->verts_num;
  refiner_impl.base_face_offsets.assign(converter->face_offsets.begin(),
                                        converter->face_offsets.end());
  assert(converter->corner_verts.size() == total_size_from_offsets(converter->face_offsets));
  refiner_impl.base_corner_verts.assign(converter->corner_verts.begin(),
                                        converter->corner_verts.end());
  assert(converter->edge_sharpness.empty() ||
         converter->edge_sharpness.size() == converter->edges.size());
  refiner_impl.base_edge_sharpness.assign(converter->edge_sharpness.begin(),
                                          converter->edge_sharpness.end());
  assert(converter->vert_sharpness.empty() ||
         int(converter->vert_sharpness.size()) == refiner_impl.base_verts_num);
  refiner_impl.base_vert_sharpness.assign(converter->vert_sharpness.begin(),
                                          converter->vert_sharpness.end());

  // Only up to the last sharp one is stored, see #TopologyRefinerImpl::base_edges_sparse.
  const std::span<const float> sharpness = converter->edge_sharpness;
  int stored_edges_num = 0;
  for (int edge_index = int(sharpness.size()) - 1; edge_index >= 0; edge_index--) {
    if (sharpness[edge_index] >= 1e-6f) {
      stored_edges_num = edge_index + 1;
      break;
    }
  }
  const std::span<const std::pair<int, int>> edges = converter->edges.first(stored_edges_num);
  refiner_impl.base_edges_sparse.assign(edges.begin(), edges.end());
}

TopologyRefinerImpl *TopologyRefinerImpl::createFromConverter(
    OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings &settings)
{
  using OpenSubdiv::Far::TopologyRefiner;

  auto topology_refiner_impl = std::make_unique<TopologyRefinerImpl>();
  topology_refiner_impl->settings = settings;
  storeBaseMeshTopology(converter, *topology_refiner_impl);

  TopologyRefinerData cb_data;
  cb_data.converter = converter;

  // Create OpenSubdiv descriptor for the topology refiner.
  TopologyRefinerFactoryType::Options topology_refiner_options = getTopologyRefinerOptions(
      converter);
  TopologyRefiner *topology_refiner = TopologyRefinerFactoryType::Create(cb_data,
                                                                         topology_refiner_options);
  if (topology_refiner == nullptr) {
    return nullptr;
  }
  topology_refiner_impl->topology_refiner = topology_refiner;

  return topology_refiner_impl.release();
}

}  // namespace blender::opensubdiv
