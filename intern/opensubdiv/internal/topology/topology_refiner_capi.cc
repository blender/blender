/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "opensubdiv_topology_refiner_capi.hh"

#include "MEM_guardedalloc.h"
#include "internal/base/type_convert.h"
#include "internal/topology/topology_refiner_impl.h"

namespace {

const OpenSubdiv::Far::TopologyRefiner *getOSDTopologyRefiner(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->impl->topology_refiner;
}

const OpenSubdiv::Far::TopologyLevel &getOSDTopologyBaseLevel(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyRefiner(topology_refiner)->GetLevel(0);
}

int getSubdivisionLevel(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->impl->settings.level;
}

bool getIsAdaptive(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->impl->settings.is_adaptive;
}

////////////////////////////////////////////////////////////////////////////////
// Query basic topology information from base level.

int getNumVertices(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner).GetNumVertices();
}

int getNumEdges(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner).GetNumEdges();
}

int getNumFaces(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyBaseLevel(topology_refiner).GetNumFaces();
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
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level.GetFaceVertices(face_index).size();
}

void getFaceVertices(const OpenSubdiv_TopologyRefiner *topology_refiner,
                     const int face_index,
                     int *face_vertices_indices)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetFaceVertices(face_index);
  convertArrayToRaw(array, face_vertices_indices);
}

int getNumFaceEdges(const OpenSubdiv_TopologyRefiner *topology_refiner, const int face_index)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level.GetFaceEdges(face_index).size();
}

void getFaceEdges(const OpenSubdiv_TopologyRefiner *topology_refiner,
                  const int face_index,
                  int *face_edges_indices)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetFaceEdges(face_index);
  convertArrayToRaw(array, face_edges_indices);
}

void getEdgeVertices(const OpenSubdiv_TopologyRefiner *topology_refiner,
                     const int edge_index,
                     int edge_vertices_indices[2])
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetEdgeVertices(edge_index);
  assert(array.size() == 2);
  edge_vertices_indices[0] = array[0];
  edge_vertices_indices[1] = array[1];
}

int getNumVertexEdges(const OpenSubdiv_TopologyRefiner *topology_refiner, const int vertex_index)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level.GetVertexEdges(vertex_index).size();
}

void getVertexEdges(const OpenSubdiv_TopologyRefiner *topology_refiner,
                    const int vertex_index,
                    int *vertex_edges_indices)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetVertexEdges(vertex_index);
  convertArrayToRaw(array, vertex_edges_indices);
}

int getNumFacePtexFaces(const OpenSubdiv_TopologyRefiner *topology_refiner, const int face_index)
{
  const int num_face_vertices = topology_refiner->getNumFaceVertices(topology_refiner, face_index);
  if (num_face_vertices == 4) {
    return 1;
  }
  return num_face_vertices;
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
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level.GetNumFVarChannels();
}

OpenSubdiv_FVarLinearInterpolation getFVarLinearInterpolation(
    const struct OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return blender::opensubdiv::getCAPIFVarLinearInterpolationFromOSD(
      getOSDTopologyRefiner(topology_refiner)->GetFVarLinearInterpolation());
}

int getNumFVarValues(const struct OpenSubdiv_TopologyRefiner *topology_refiner, const int channel)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return base_level.GetNumFVarValues(channel);
}

const int *getFaceFVarValueIndices(const struct OpenSubdiv_TopologyRefiner *topology_refiner,
                                   const int face_index,
                                   const int channel)
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner);
  return &base_level.GetFaceFVarValues(face_index, channel)[0];
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
  topology_refiner->getNumVertexEdges = getNumVertexEdges;
  topology_refiner->getVertexEdges = getVertexEdges;
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
  OpenSubdiv_TopologyRefiner *topology_refiner = MEM_new<OpenSubdiv_TopologyRefiner>(__func__);
  assignFunctionPointers(topology_refiner);
  return topology_refiner;
}

}  // namespace

OpenSubdiv_TopologyRefiner *openSubdiv_createTopologyRefinerFromConverter(
    OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings *settings)
{
  using blender::opensubdiv::TopologyRefinerImpl;

  TopologyRefinerImpl *topology_refiner_impl = TopologyRefinerImpl::createFromConverter(converter,
                                                                                        *settings);
  if (topology_refiner_impl == nullptr) {
    return nullptr;
  }

  OpenSubdiv_TopologyRefiner *topology_refiner = allocateTopologyRefiner();
  topology_refiner->impl = static_cast<OpenSubdiv_TopologyRefinerImpl *>(topology_refiner_impl);

  return topology_refiner;
}

void openSubdiv_deleteTopologyRefiner(OpenSubdiv_TopologyRefiner *topology_refiner)
{
  delete topology_refiner->impl;
  MEM_delete(topology_refiner);
}

bool openSubdiv_topologyRefinerCompareWithConverter(
    const OpenSubdiv_TopologyRefiner *topology_refiner, const OpenSubdiv_Converter *converter)
{
  return topology_refiner->impl->isEqualToConverter(converter);
}
