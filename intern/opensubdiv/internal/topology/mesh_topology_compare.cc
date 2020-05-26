// Copyright 2020 Blender Foundation. All rights reserved.
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

#include "internal/topology/mesh_topology.h"

#include <cassert>
#include <cstring>
#include <opensubdiv/sdc/crease.h>

#include "internal/base/type.h"

#include "opensubdiv_converter_capi.h"

namespace blender {
namespace opensubdiv {

namespace {

////////////////////////////////////////////////////////////////////////////////
// Quick preliminary checks.

int getEffectiveNumEdges(const OpenSubdiv_Converter *converter)
{
  if (converter->getNumEdges == nullptr) {
    return 0;
  }

  return converter->getNumEdges(converter);
}

bool isEqualGeometryCounters(const MeshTopology &mesh_topology,
                             const OpenSubdiv_Converter *converter)
{
  if (converter->getNumVertices(converter) != mesh_topology.getNumVertices()) {
    return false;
  }
  if (converter->getNumFaces(converter) != mesh_topology.getNumFaces()) {
    return false;
  }
  if (getEffectiveNumEdges(converter) != mesh_topology.getNumEdges()) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Geometry.

// Edges.

bool isEqualGeometryEdge(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  const int num_requested_edges = getEffectiveNumEdges(converter);
  if (num_requested_edges != mesh_topology.getNumEdges()) {
    return false;
  }

  // NOTE: Ignoring the sharpness we don't really care of the content of the
  // edges, they should be in the consistent state with faces and face-vertices.
  // If that's not the case the mesh is invalid and comparison can not happen
  // reliably.
  //
  // For sharpness it is important to know that edges are connecting same pair
  // of vertices. But since sharpness is stored sparesly the connectivity will
  // be checked when comparing edge sharpness.

  return true;
}

// Faces.

bool isEqualGeometryFace(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  const int num_requested_faces = converter->getNumFaces(converter);
  if (num_requested_faces != mesh_topology.getNumFaces()) {
    return false;
  }

  vector<int> vertices_of_face;
  for (int face_index = 0; face_index < num_requested_faces; ++face_index) {
    int num_face_vertices = converter->getNumFaceVertices(converter, face_index);
    if (mesh_topology.getNumFaceVertices(face_index) != num_face_vertices) {
      return false;
    }

    vertices_of_face.resize(num_face_vertices);
    converter->getFaceVertices(converter, face_index, vertices_of_face.data());

    if (!mesh_topology.isFaceVertexIndicesEqual(face_index, vertices_of_face)) {
      return false;
    }
  }

  return true;
}

// Geometry comparison entry point.

bool isEqualGeometry(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  if (!isEqualGeometryEdge(mesh_topology, converter)) {
    return false;
  }
  if (!isEqualGeometryFace(mesh_topology, converter)) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Geometry tags.

// Vertices.

// TODO(sergey): Make this function usable by factory as well.
float getEffectiveVertexSharpness(const OpenSubdiv_Converter *converter, const int vertex_index)
{
  if (converter->isInfiniteSharpVertex != nullptr &&
      converter->isInfiniteSharpVertex(converter, vertex_index)) {
    return OpenSubdiv::Sdc::Crease::SHARPNESS_INFINITE;
  }

  if (converter->getVertexSharpness != nullptr) {
    return converter->getVertexSharpness(converter, vertex_index);
  }

  return 0.0f;
}

bool isEqualVertexTags(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  const int num_vertices = mesh_topology.getNumVertices();
  for (int vertex_index = 0; vertex_index < num_vertices; ++vertex_index) {
    const float current_sharpness = mesh_topology.getVertexSharpness(vertex_index);
    const float requested_sharpness = getEffectiveVertexSharpness(converter, vertex_index);

    if (current_sharpness != requested_sharpness) {
      return false;
    }
  }

  return true;
}

// Edges.

// TODO(sergey): Make this function usable by factory as well.
float getEffectiveEdgeSharpness(const OpenSubdiv_Converter *converter, const int edge_index)
{
  if (converter->getEdgeSharpness != nullptr) {
    return converter->getEdgeSharpness(converter, edge_index);
  }

  return 0.0f;
}

bool isEqualEdgeTags(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  const int num_edges = mesh_topology.getNumEdges();
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const float current_sharpness = mesh_topology.getEdgeSharpness(edge_index);
    const float requested_sharpness = getEffectiveEdgeSharpness(converter, edge_index);

    if (current_sharpness != requested_sharpness) {
      return false;
    }

    if (current_sharpness < 1e-6f) {
      continue;
    }

    int requested_edge_vertices[2];
    converter->getEdgeVertices(converter, edge_index, requested_edge_vertices);
    if (!mesh_topology.isEdgeEqual(
            edge_index, requested_edge_vertices[0], requested_edge_vertices[1])) {
      return false;
    }
  }

  return true;
}

// Tags comparison entry point.

bool isEqualTags(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  if (!isEqualVertexTags(mesh_topology, converter)) {
    return false;
  }
  if (!isEqualEdgeTags(mesh_topology, converter)) {
    return false;
  }

  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Entry point.

bool MeshTopology::isEqualToConverter(const OpenSubdiv_Converter *converter) const
{
  // Preliminary checks.
  if (!isEqualGeometryCounters(*this, converter)) {
    return false;
  }

  // Geometry.
  if (!isEqualGeometry(*this, converter)) {
    return false;
  }

  // Tags.
  if (!isEqualTags(*this, converter)) {
    return false;
  }

  return true;
}

}  // namespace opensubdiv
}  // namespace blender
