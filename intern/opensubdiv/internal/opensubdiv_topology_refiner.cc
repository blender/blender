// Copyright 2018 Blender Foundation. All rights reserved.
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

#include "opensubdiv_topology_refiner_capi.h"

#include <vector>

#include "MEM_guardedalloc.h"
#include "internal/opensubdiv_converter_factory.h"
#include "internal/opensubdiv_converter_internal.h"
#include "internal/opensubdiv_edge_map.h"
#include "internal/opensubdiv_internal.h"
#include "internal/opensubdiv_topology_refiner_internal.h"
#include "internal/opensubdiv_util.h"

using opensubdiv_capi::vector;

namespace {

const OpenSubdiv::Far::TopologyRefiner *getOSDTopologyRefiner(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->internal->osd_topology_refiner;
}

const OpenSubdiv::Far::TopologyLevel *getOSDTopologyBaseLevel(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return &getOSDTopologyRefiner(topology_refiner)->GetLevel(0);
}

int getSubdivisionLevel(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->internal->settings.level;
}

bool getIsAdaptive(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->internal->settings.is_adaptive;
}

////////////////////////////////////////////////////////////////////////////////
// Query basic topology information from base level.

int getNumVertices(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumVertices();
}

int getNumEdges(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumEdges();
}

int getNumFaces(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumFaces();
}

////////////////////////////////////////////////////////////////////////////////
// PTex face geometry queries.

static void convertArrayToRaw(const OpenSubdiv::Far::ConstIndexArray &array, int *raw_array)
{
  for (int i = 0; i < array.size(); ++i) {
    raw_array[i] = array[i];
  }
}

int getNumFaceVertices(const OpenSubdiv_TopologyRefiner *topology_refiner, const int face_index)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetFaceVertices(face_index).size();
}

void getFaceVertices(const OpenSubdiv_TopologyRefiner *topology_refiner,
                     const int face_index,
                     int *face_vertices_indices)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level->GetFaceVertices(face_index);
  convertArrayToRaw(array, face_vertices_indices);
}

int getNumFaceEdges(const OpenSubdiv_TopologyRefiner *topology_refiner, const int face_index)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetFaceEdges(face_index).size();
}

void getFaceEdges(const OpenSubdiv_TopologyRefiner *topology_refiner,
                  const int face_index,
                  int *face_edges_indices)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level->GetFaceEdges(face_index);
  convertArrayToRaw(array, face_edges_indices);
}

void getEdgeVertices(const OpenSubdiv_TopologyRefiner *topology_refiner,
                     const int edge_index,
                     int edge_vertices_indices[2])
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level->GetEdgeVertices(edge_index);
  assert(array.size() == 2);
  edge_vertices_indices[0] = array[0];
  edge_vertices_indices[1] = array[1];
}

int getNumFacePtexFaces(const OpenSubdiv_TopologyRefiner *topology_refiner, const int face_index)
{
  const int num_face_vertices = topology_refiner->getNumFaceVertices(topology_refiner, face_index);
  if (num_face_vertices == 4) {
    return 1;
  }
  else {
    return num_face_vertices;
  }
}

int getNumPtexFaces(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    num_ptex_faces += topology_refiner->getNumFacePtexFaces(topology_refiner, face_index);
  }
  return num_ptex_faces;
}

void fillFacePtexIndexOffset(const OpenSubdiv_TopologyRefiner *topology_refiner,
                             int *face_ptex_index_offset)
{
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    face_ptex_index_offset[face_index] = num_ptex_faces;
    num_ptex_faces += topology_refiner->getNumFacePtexFaces(topology_refiner, face_index);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Face-varying data.

int getNumFVarChannels(const struct OpenSubdiv_TopologyRefiner *topology_refiner)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetNumFVarChannels();
}

OpenSubdiv_FVarLinearInterpolation getFVarLinearInterpolation(
    const struct OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return opensubdiv_capi::getCAPIFVarLinearInterpolationFromOSD(
      getOSDTopologyRefiner(topology_refiner)->GetFVarLinearInterpolation());
}

int getNumFVarValues(const struct OpenSubdiv_TopologyRefiner *topology_refiner, const int channel)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetNumFVarValues(channel);
}

const int *getFaceFVarValueIndices(const struct OpenSubdiv_TopologyRefiner *topology_refiner,
                                   const int face_index,
                                   const int channel)
{
  const OpenSubdiv::Far::TopologyLevel *base_level = getOSDTopologyBaseLevel(topology_refiner);
  return &base_level->GetFaceFVarValues(face_index, channel)[0];
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers.

void assignFunctionPointers(OpenSubdiv_TopologyRefiner *topology_refiner)
{
  topology_refiner->getSubdivisionLevel = getSubdivisionLevel;
  topology_refiner->getIsAdaptive = getIsAdaptive;
  // Basic topology information.
  topology_refiner->getNumVertices = getNumVertices;
  topology_refiner->getNumEdges = getNumEdges;
  topology_refiner->getNumFaces = getNumFaces;
  topology_refiner->getNumFaceVertices = getNumFaceVertices;
  topology_refiner->getFaceVertices = getFaceVertices;
  topology_refiner->getNumFaceEdges = getNumFaceEdges;
  topology_refiner->getFaceEdges = getFaceEdges;
  topology_refiner->getEdgeVertices = getEdgeVertices;
  // PTex face geometry.
  topology_refiner->getNumFacePtexFaces = getNumFacePtexFaces;
  topology_refiner->getNumPtexFaces = getNumPtexFaces;
  topology_refiner->fillFacePtexIndexOffset = fillFacePtexIndexOffset;
  // Face-varying data.
  topology_refiner->getNumFVarChannels = getNumFVarChannels;
  topology_refiner->getFVarLinearInterpolation = getFVarLinearInterpolation;
  topology_refiner->getNumFVarValues = getNumFVarValues;
  topology_refiner->getFaceFVarValueIndices = getFaceFVarValueIndices;
}

OpenSubdiv_TopologyRefiner *allocateTopologyRefiner()
{
  OpenSubdiv_TopologyRefiner *topology_refiner = OBJECT_GUARDED_NEW(OpenSubdiv_TopologyRefiner);
  topology_refiner->internal = OBJECT_GUARDED_NEW(OpenSubdiv_TopologyRefinerInternal);
  assignFunctionPointers(topology_refiner);
  return topology_refiner;
}

}  // namespace

OpenSubdiv_TopologyRefiner *openSubdiv_createTopologyRefinerFromConverter(
    OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings *settings)
{
  OpenSubdiv::Far::TopologyRefiner *osd_topology_refiner =
      opensubdiv_capi::createOSDTopologyRefinerFromConverter(converter);
  if (osd_topology_refiner == NULL) {
    // Happens on empty or bad topology.
    return NULL;
  }
  OpenSubdiv_TopologyRefiner *topology_refiner = allocateTopologyRefiner();
  topology_refiner->internal->osd_topology_refiner = osd_topology_refiner;
  // Store setting which we want to keep track of and which can not be stored
  // in OpenSubdiv's descriptor yet.
  topology_refiner->internal->settings = *settings;
  return topology_refiner;
}

void openSubdiv_deleteTopologyRefiner(OpenSubdiv_TopologyRefiner *topology_refiner)
{
  OBJECT_GUARDED_DELETE(topology_refiner->internal, OpenSubdiv_TopologyRefinerInternal);
  OBJECT_GUARDED_DELETE(topology_refiner, OpenSubdiv_TopologyRefiner);
}

////////////////////////////////////////////////////////////////////////////////
// Comparison with converter.

namespace opensubdiv_capi {
namespace {

///////////////////////////////////////////////////////////
// Quick preliminary checks.

bool checkSchemeTypeMatches(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                            const OpenSubdiv_Converter *converter)
{
  const OpenSubdiv::Sdc::SchemeType converter_scheme_type = opensubdiv_capi::getSchemeTypeFromCAPI(
      converter->getSchemeType(converter));
  return (converter_scheme_type == topology_refiner->GetSchemeType());
}

bool checkOptionsMatches(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                         const OpenSubdiv_Converter *converter)
{
  typedef OpenSubdiv::Sdc::Options Options;
  const Options options = topology_refiner->GetSchemeOptions();
  const Options::FVarLinearInterpolation fvar_interpolation = options.GetFVarLinearInterpolation();
  const Options::FVarLinearInterpolation converter_fvar_interpolation =
      opensubdiv_capi::getFVarLinearInterpolationFromCAPI(
          converter->getFVarLinearInterpolation(converter));
  if (fvar_interpolation != converter_fvar_interpolation) {
    return false;
  }
  return true;
}

bool checkGeometryCountersMatches(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                                  const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  return ((converter->getNumVertices(converter) == base_level.GetNumVertices()) &&
          (converter->getNumEdges(converter) == base_level.GetNumEdges()) &&
          (converter->getNumFaces(converter) == base_level.GetNumFaces()));
}

bool checkPreliminaryMatches(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                             const OpenSubdiv_Converter *converter)
{
  return checkSchemeTypeMatches(topology_refiner, converter) &&
         checkOptionsMatches(topology_refiner, converter) &&
         checkGeometryCountersMatches(topology_refiner, converter);
}

///////////////////////////////////////////////////////////
// Geometry comparison.

// A thin wrapper around index like array which does cyclic access. This means,
// it basically does indices[requested_index % num_indices].
//
// NOTE: This array does not own the memory.
//
// TODO(sergey): Consider moving this to a more reusable place.
class CyclicArray {
 public:
  typedef int value_type;
  typedef int size_type;
  static constexpr size_type npos = -1;

  explicit CyclicArray(const std::vector<int> &data) : data_(data.data()), size_(data.size())
  {
  }

  explicit CyclicArray(const OpenSubdiv::Far::ConstIndexArray &data)
      : data_(&data[0]), size_(data.size())
  {
  }

  inline value_type operator[](int index) const
  {
    assert(index >= 0);
    // TODO(sergey): Check whether doing check for element index exceeding total
    // number of indices prior to modulo helps performance.
    return data_[index % size()];
  }

  inline size_type size() const
  {
    return size_;
  }

  // Find index of first occurrence of a given value.
  inline size_type find(const value_type value) const
  {
    const int num_indices = size();
    for (size_type i = 0; i < num_indices; ++i) {
      if (value == (*this)[i]) {
        return i;
      }
    }
    return npos;
  }

 protected:
  const value_type *data_;
  const size_type size_;
};

bool compareCyclicForward(const CyclicArray &array_a,
                          const int start_a,
                          const CyclicArray &array_b,
                          const int start_b)
{
  const int num_elements = array_a.size();
  for (int i = 0; i < num_elements; ++i) {
    if (array_a[start_a + i] != array_b[start_b + i]) {
      return false;
    }
  }
  return true;
}

bool compareCyclicBackward(const CyclicArray &array_a,
                           const int start_a,
                           const CyclicArray &array_b,
                           const int start_b)
{
  const int num_elements = array_a.size();
  // TODO(sergey): Some optimization might be possible with memcmp trickery.
  for (int i = 0; i < num_elements; ++i) {
    if (array_a[start_a + (num_elements - i - 1)] != array_b[start_b + (num_elements - i - 1)]) {
      return false;
    }
  }
  return true;
}

// Utility function dedicated for checking whether whether vertices indices
// used by two faces match.
// The tricky part here is that we can't trust 1:1 array match here, since it's
// possible that OpenSubdiv oriented edges of a face to make it compatible with
// an internal representation of non-manifold meshes.
//
// TODO(sergey): Check whether this is needed, ot whether OpenSubdiv is only
// creating edges in a proper orientation without modifying indices of face
// vertices.
bool checkVerticesOfFacesMatch(const CyclicArray &indices_a, const CyclicArray &indices_b)
{
  if (indices_a.size() != indices_a.size()) {
    return false;
  }
  // "Align" the arrays so we know first matched element.
  const int start_b = indices_b.find(indices_a[0]);
  if (start_b == indices_b.npos) {
    return false;
  }
  // Check match in both directions, for the case OpenSubdiv did orient face in
  // a way which made normals more consistent internally.
  if (compareCyclicForward(indices_a, 0, indices_b, start_b)) {
    return true;
  }
  if (compareCyclicBackward(indices_a, 0, indices_b, start_b)) {
    return true;
  }
  return false;
}

bool checkGeometryFacesMatch(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                             const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Consider using data structure which keeps handful of
  // elements on stack before doing heep allocation.
  vector<int> conv_face_vertices;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const ConstIndexArray &face_vertices = base_level.GetFaceVertices(face_index);
    const int num_face_vertices = face_vertices.size();
    if (num_face_vertices != converter->getNumFaceVertices(converter, face_index)) {
      return false;
    }
    conv_face_vertices.resize(num_face_vertices);
    converter->getFaceVertices(converter, face_index, &conv_face_vertices[0]);
    if (!checkVerticesOfFacesMatch(CyclicArray(conv_face_vertices), CyclicArray(face_vertices))) {
      return false;
    }
  }
  return true;
}

bool checkGeometryMatches(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                          const OpenSubdiv_Converter *converter)
{
  // NOTE: Since OpenSubdiv's topology refiner doesn't contain loose edges, we
  // are only checking for faces to be matched. Changes in edges we don't care
  // here too much (they'll be checked for creases changes later).
  return checkGeometryFacesMatch(topology_refiner, converter);
}

///////////////////////////////////////////////////////////
// Compare attributes which affects on topology

inline bool checkSingleEdgeSharpnessMatch(const OpenSubdiv::Far::TopologyLevel &base_level,
                                          int base_level_edge_index,
                                          const OpenSubdiv_Converter *converter,
                                          int converter_edge_index)
{
  // NOTE: Boundary and non-manifold edges are internally forced to an infinite
  // sharpness. So we can not reliably compare those.
  //
  // TODO(sergey): Watch for NON_MANIFOLD_SHARP option.
  if (base_level.IsEdgeBoundary(base_level_edge_index) ||
      base_level.IsEdgeNonManifold(base_level_edge_index)) {
    return true;
  }
  const float sharpness = base_level.GetEdgeSharpness(base_level_edge_index);
  const float converter_sharpness = converter->getEdgeSharpness(converter, converter_edge_index);
  if (sharpness != converter_sharpness) {
    return false;
  }
  return true;
}

inline bool checkSingleEdgeTagMatch(const OpenSubdiv::Far::TopologyLevel &base_level,
                                    int base_level_edge_index,
                                    const OpenSubdiv_Converter *converter,
                                    int converter_edge_index)
{
  return checkSingleEdgeSharpnessMatch(
      base_level, base_level_edge_index, converter, converter_edge_index);
}

// Compares edge tags between topology refiner and converter in a case when
// converter specifies a full topology.
// This is simplest loop, since we know that order of edges matches.
bool checkEdgeTagsMatchFullTopology(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                                    const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  const int num_edges = base_level.GetNumEdges();
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    if (!checkSingleEdgeTagMatch(base_level, edge_index, converter, edge_index)) {
      return false;
    }
  }
  return true;
}

// Compares tags of edges in the case when orientation of edges is left up to
// OpenSubdiv. In this case we do need to take care of mapping edges from the
// converter to current topology refiner, since the order is not guaranteed.
bool checkEdgeTagsMatchAutoOrient(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                                  const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  const int num_edges = base_level.GetNumEdges();
  // Create mapping for quick lookup of edge index from its vertices indices.
  //
  // TODO(sergey): Consider caching it in some sort of wrapper around topology
  // refiner.
  EdgeTagMap<int> edge_map;
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    ConstIndexArray edge_vertices = base_level.GetEdgeVertices(edge_index);
    edge_map.insert(edge_vertices[0], edge_vertices[1], edge_index);
  }
  // Compare all edges.
  for (int converter_edge_index = 0; converter_edge_index < num_edges; ++converter_edge_index) {
    // Get edge vertices indices, and lookup corresponding edge index in the
    // base topology level.
    int edge_vertices[2];
    converter->getEdgeVertices(converter, converter_edge_index, edge_vertices);
    const int base_level_edge_index = edge_map.at(edge_vertices[0], edge_vertices[1]);
    // Perform actual test.
    if (!checkSingleEdgeTagMatch(
            base_level, base_level_edge_index, converter, converter_edge_index)) {
      return false;
    }
  }
  return true;
}

bool checkEdgeTagsMatch(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                        const OpenSubdiv_Converter *converter)
{
  if (converter->specifiesFullTopology(converter)) {
    return checkEdgeTagsMatchFullTopology(topology_refiner, converter);
  }
  else {
    return checkEdgeTagsMatchAutoOrient(topology_refiner, converter);
  }
}

bool checkvertexSharpnessMatch(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                               const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  using OpenSubdiv::Sdc::Crease;
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  // Create mapping for quick lookup of edge index from its vertices indices.
  //
  // TODO(sergey): Consider caching it in some sort of wrapper around topology
  // refiner.
  const int num_edges = base_level.GetNumEdges();
  EdgeTagMap<int> edge_map;
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    int edge_vertices[2];
    converter->getEdgeVertices(converter, edge_index, edge_vertices);
    edge_map.insert(edge_vertices[0], edge_vertices[1], edge_index);
  }
  const int num_vertices = base_level.GetNumVertices();
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    const float current_sharpness = base_level.GetVertexSharpness(vertex_index);
    if (converter->isInfiniteSharpVertex(converter, vertex_index)) {
      if (current_sharpness != Crease::SHARPNESS_INFINITE) {
        return false;
      }
    }
    else {
      ConstIndexArray vertex_edges = base_level.GetVertexEdges(vertex_index);
      float sharpness = converter->getVertexSharpness(converter, vertex_index);
      if (vertex_edges.size() == 2) {
        const int edge0 = vertex_edges[0], edge1 = vertex_edges[1];
        // Construct keys for lookup.
        ConstIndexArray edge0_vertices = base_level.GetEdgeVertices(edge0);
        ConstIndexArray edge1_vertices = base_level.GetEdgeVertices(edge1);
        EdgeKey edge0_key(edge0_vertices[0], edge0_vertices[1]);
        EdgeKey edge1_key(edge1_vertices[0], edge1_vertices[1]);
        // Lookup edge indices in the converter.
        const int edge0_converter_index = edge_map[edge0_key];
        const int edge1_converter_index = edge_map[edge1_key];
        // Lookup sharpness.
        const float sharpness0 = converter->getEdgeSharpness(converter, edge0_converter_index);
        const float sharpness1 = converter->getEdgeSharpness(converter, edge1_converter_index);
        // TODO(sergey): Find a better mixing between edge and vertex sharpness.
        sharpness += min(sharpness0, sharpness1);
        sharpness = min(sharpness, 10.0f);
      }
      if (sharpness != current_sharpness) {
        return false;
      }
    }
  }
  return true;
}

bool checkSingleUVLayerMatch(const OpenSubdiv::Far::TopologyLevel &base_level,
                             const OpenSubdiv_Converter *converter,
                             const int layer_index)
{
  converter->precalcUVLayer(converter, layer_index);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Need to check whether converter changed the winding of
  // face to match OpenSubdiv's expectations.
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    OpenSubdiv::Far::ConstIndexArray base_level_face_uvs = base_level.GetFaceFVarValues(
        face_index, layer_index);
    for (int corner = 0; corner < base_level_face_uvs.size(); ++corner) {
      const int uv_index = converter->getFaceCornerUVIndex(converter, face_index, corner);
      if (base_level_face_uvs[corner] != uv_index) {
        converter->finishUVLayer(converter);
        return false;
      }
    }
  }
  converter->finishUVLayer(converter);
  return true;
}

bool checkUVLayersMatch(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                        const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyLevel;
  const int num_layers = converter->getNumUVLayers(converter);
  const TopologyLevel &base_level = topology_refiner->GetLevel(0);
  // Number of UV layers should match.
  if (base_level.GetNumFVarChannels() != num_layers) {
    return false;
  }
  for (int layer_index = 0; layer_index < num_layers; ++layer_index) {
    if (!checkSingleUVLayerMatch(base_level, converter, layer_index)) {
      return false;
    }
  }
  return true;
}

bool checkTopologyAttributesMatch(const OpenSubdiv::Far::TopologyRefiner *topology_refiner,
                                  const OpenSubdiv_Converter *converter)
{
  return checkEdgeTagsMatch(topology_refiner, converter) &&
         checkvertexSharpnessMatch(topology_refiner, converter) &&
         checkUVLayersMatch(topology_refiner, converter);
}

}  // namespace
}  // namespace opensubdiv_capi

bool openSubdiv_topologyRefinerCompareWithConverter(
    const OpenSubdiv_TopologyRefiner *topology_refiner, const OpenSubdiv_Converter *converter)
{
  const OpenSubdiv::Far::TopologyRefiner *refiner = getOSDTopologyRefiner(topology_refiner);
  return (opensubdiv_capi::checkPreliminaryMatches(refiner, converter) &&
          opensubdiv_capi::checkGeometryMatches(refiner, converter) &&
          opensubdiv_capi::checkTopologyAttributesMatch(refiner, converter));
}
