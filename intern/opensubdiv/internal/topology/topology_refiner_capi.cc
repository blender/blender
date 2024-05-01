/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "opensubdiv_topology_refiner_capi.hh"

#include "MEM_guardedalloc.h"
#include "internal/base/type_convert.h"
#include "internal/topology/topology_refiner_impl.h"

static const OpenSubdiv::Far::TopologyRefiner *getOSDTopologyRefiner(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return topology_refiner->impl->topology_refiner;
}

static const OpenSubdiv::Far::TopologyLevel &getOSDTopologyBaseLevel(
    const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  return getOSDTopologyRefiner(topology_refiner)->GetLevel(0);
}

int OpenSubdiv_TopologyRefiner::getSubdivisionLevel() const
{
  return this->impl->settings.level;
}

bool OpenSubdiv_TopologyRefiner::getIsAdaptive() const
{
  return this->impl->settings.is_adaptive;
}

////////////////////////////////////////////////////////////////////////////////
// Query basic topology information from base level.

int OpenSubdiv_TopologyRefiner::getNumVertices() const
{
  return getOSDTopologyBaseLevel(this).GetNumVertices();
}

int OpenSubdiv_TopologyRefiner::getNumEdges() const
{
  return getOSDTopologyBaseLevel(this).GetNumEdges();
}

int OpenSubdiv_TopologyRefiner::getNumFaces() const
{
  return getOSDTopologyBaseLevel(this).GetNumFaces();
}

////////////////////////////////////////////////////////////////////////////////
// PTex face geometry queries.

static void convertArrayToRaw(const OpenSubdiv::Far::ConstIndexArray &array, int *raw_array)
{
  for (int i = 0; i < array.size(); ++i) {
    raw_array[i] = array[i];
  }
}

int OpenSubdiv_TopologyRefiner::getNumFaceVertices(const int face_index) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return base_level.GetFaceVertices(face_index).size();
}

void OpenSubdiv_TopologyRefiner::getFaceVertices(const int face_index,
                                                 int *face_vertices_indices) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetFaceVertices(face_index);
  convertArrayToRaw(array, face_vertices_indices);
}

int OpenSubdiv_TopologyRefiner::getNumFaceEdges(const int face_index) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return base_level.GetFaceEdges(face_index).size();
}

void OpenSubdiv_TopologyRefiner::getFaceEdges(const int face_index, int *face_edges_indices) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetFaceEdges(face_index);
  convertArrayToRaw(array, face_edges_indices);
}

void OpenSubdiv_TopologyRefiner::getEdgeVertices(const int edge_index,
                                                 int edge_vertices_indices[2]) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetEdgeVertices(edge_index);
  assert(array.size() == 2);
  edge_vertices_indices[0] = array[0];
  edge_vertices_indices[1] = array[1];
}

int OpenSubdiv_TopologyRefiner::getNumVertexEdges(const int vertex_index) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return base_level.GetVertexEdges(vertex_index).size();
}

void OpenSubdiv_TopologyRefiner::getVertexEdges(const int vertex_index,
                                                int *vertex_edges_indices) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  OpenSubdiv::Far::ConstIndexArray array = base_level.GetVertexEdges(vertex_index);
  convertArrayToRaw(array, vertex_edges_indices);
}

int OpenSubdiv_TopologyRefiner::getNumFacePtexFaces(const int face_index) const
{
  const int num_face_vertices = this->getNumFaceVertices(face_index);
  if (num_face_vertices == 4) {
    return 1;
  }
  return num_face_vertices;
}

int OpenSubdiv_TopologyRefiner::getNumPtexFaces() const
{
  const int num_faces = this->getNumFaces();
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    num_ptex_faces += this->getNumFacePtexFaces(face_index);
  }
  return num_ptex_faces;
}

void OpenSubdiv_TopologyRefiner::fillFacePtexIndexOffset(int *face_ptex_index_offset) const
{
  const int num_faces = this->getNumFaces();
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    face_ptex_index_offset[face_index] = num_ptex_faces;
    num_ptex_faces += this->getNumFacePtexFaces(face_index);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Face-varying data.

int OpenSubdiv_TopologyRefiner::getNumFVarChannels() const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return base_level.GetNumFVarChannels();
}

OpenSubdiv_FVarLinearInterpolation OpenSubdiv_TopologyRefiner::getFVarLinearInterpolation() const
{
  return blender::opensubdiv::getCAPIFVarLinearInterpolationFromOSD(
      getOSDTopologyRefiner(this)->GetFVarLinearInterpolation());
}

int OpenSubdiv_TopologyRefiner::getNumFVarValues(const int channel) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return base_level.GetNumFVarValues(channel);
}

const int *OpenSubdiv_TopologyRefiner::getFaceFVarValueIndices(const int face_index,
                                                               const int channel) const
{
  const OpenSubdiv::Far::TopologyLevel &base_level = getOSDTopologyBaseLevel(this);
  return &base_level.GetFaceFVarValues(face_index, channel)[0];
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers.

OpenSubdiv_TopologyRefiner *openSubdiv_createTopologyRefinerFromConverter(
    OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings *settings)
{
  using blender::opensubdiv::TopologyRefinerImpl;

  TopologyRefinerImpl *topology_refiner_impl = TopologyRefinerImpl::createFromConverter(converter,
                                                                                        *settings);
  if (topology_refiner_impl == nullptr) {
    return nullptr;
  }

  OpenSubdiv_TopologyRefiner *topology_refiner = MEM_new<OpenSubdiv_TopologyRefiner>(__func__);
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
