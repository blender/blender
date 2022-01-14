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

namespace blender {
namespace opensubdiv {

MeshTopology::MeshTopology() : num_vertices_(0), num_edges_(0), num_faces_(0)
{
}

MeshTopology::~MeshTopology()
{
}

////////////////////////////////////////////////////////////////////////////////
// Vertices.

void MeshTopology::setNumVertices(int num_vertices)
{
  num_vertices_ = num_vertices;
}

int MeshTopology::getNumVertices() const
{
  return num_vertices_;
}

void MeshTopology::setVertexSharpness(int vertex_index, float sharpness)
{
  assert(vertex_index >= 0);
  assert(vertex_index < getNumVertices());

  ensureVertexTagsSize(vertex_index + 1);

  vertex_tags_[vertex_index].sharpness = sharpness;
}

float MeshTopology::getVertexSharpness(int vertex_index) const
{
  assert(vertex_index >= 0);
  assert(vertex_index < getNumVertices());

  if (vertex_index >= vertex_tags_.size()) {
    // Sharpness for the vertex was never provided.
    return 0.0f;
  }

  return vertex_tags_[vertex_index].sharpness;
}

void MeshTopology::ensureVertexTagsSize(int num_vertices)
{
  if (vertex_tags_.size() < num_vertices) {
    vertex_tags_.resize(num_vertices);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Edges.

void MeshTopology::setNumEdges(int num_edges)
{
  num_edges_ = num_edges;
}

int MeshTopology::getNumEdges() const
{
  return num_edges_;
}

void MeshTopology::setEdgeVertexIndices(int edge_index, int v1, int v2)
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  assert(v1 >= 0);
  assert(v1 < getNumVertices());

  assert(v2 >= 0);
  assert(v2 < getNumVertices());

  ensureNumEdgesAtLeast(edge_index + 1);

  Edge &edge = edges_[edge_index];
  edge.v1 = v1;
  edge.v2 = v2;
}

void MeshTopology::getEdgeVertexIndices(int edge_index, int *v1, int *v2) const
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  if (edge_index >= edges_.size()) {
    *v1 = -1;
    *v2 = -1;
    return;
  }

  const Edge &edge = edges_[edge_index];
  *v1 = edge.v1;
  *v2 = edge.v2;
}

bool MeshTopology::isEdgeEqual(int edge_index, int expected_v1, int expected_v2) const
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  if (edge_index >= edges_.size()) {
    return false;
  }

  const Edge &edge = edges_[edge_index];
  return edge.v1 == expected_v1 && edge.v2 == expected_v2;
}

void MeshTopology::setEdgeSharpness(int edge_index, float sharpness)
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  if (sharpness < 1e-6f) {
    return;
  }

  ensureEdgeTagsSize(edge_index + 1);

  edge_tags_[edge_index].sharpness = sharpness;
}

float MeshTopology::getEdgeSharpness(int edge_index) const
{
  assert(edge_index >= 0);

  if (edge_index >= edge_tags_.size()) {
    // NOTE: It's possible that full topology is not known and that there was
    // never sharpness assigned to any of the edges.
    return 0.0f;
  }

  return edge_tags_[edge_index].sharpness;
}

void MeshTopology::ensureNumEdgesAtLeast(int num_edges)
{
  if (edges_.size() < num_edges) {
    edges_.resize(num_edges);
  }
}

void MeshTopology::ensureEdgeTagsSize(int num_edges)
{
  if (edge_tags_.size() < num_edges) {
    edge_tags_.resize(num_edges);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Faces.

void MeshTopology::setNumFaces(int num_faces)
{
  num_faces_ = num_faces;

  // NOTE: Extra element to store fake face past the last real one to make it
  // possible to calculate number of vertices in the last face.
  faces_first_vertex_index_.resize(num_faces + 1, 0);
}

int MeshTopology::getNumFaces() const
{
  return num_faces_;
}

void MeshTopology::setNumFaceVertices(int face_index, int num_face_vertices)
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());

  faces_first_vertex_index_[face_index + 1] = faces_first_vertex_index_[face_index] +
                                              num_face_vertices;
}

int MeshTopology::getNumFaceVertices(int face_index) const
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());

  return faces_first_vertex_index_[face_index + 1] - faces_first_vertex_index_[face_index];
}

void MeshTopology::setFaceVertexIndices(int face_index,
                                        int num_face_vertex_indices,
                                        const int *face_vertex_indices)
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());
  assert(num_face_vertex_indices == getNumFaceVertices(face_index));

  int *face_vertex_indices_storage = getFaceVertexIndicesStorage(face_index);
  memcpy(face_vertex_indices_storage, face_vertex_indices, sizeof(int) * num_face_vertex_indices);
}

bool MeshTopology::isFaceVertexIndicesEqual(int face_index,
                                            int num_expected_face_vertex_indices,
                                            const int *expected_face_vertex_indices) const
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());

  if (getNumFaceVertices(face_index) != num_expected_face_vertex_indices) {
    return false;
  }

  const int *face_vertex_indices_storage = getFaceVertexIndicesStorage(face_index);
  return memcmp(face_vertex_indices_storage,
                expected_face_vertex_indices,
                sizeof(int) * num_expected_face_vertex_indices) == 0;
}

bool MeshTopology::isFaceVertexIndicesEqual(int face_index,
                                            const vector<int> &expected_face_vertex_indices) const
{
  return isFaceVertexIndicesEqual(
      face_index, expected_face_vertex_indices.size(), expected_face_vertex_indices.data());
}

int *MeshTopology::getFaceVertexIndicesStorage(int face_index)
{
  const MeshTopology *const_this = this;
  return const_cast<int *>(const_this->getFaceVertexIndicesStorage(face_index));
}
const int *MeshTopology::getFaceVertexIndicesStorage(int face_index) const
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());

  const int offset = faces_first_vertex_index_[face_index];
  return face_vertex_indices_.data() + offset;
}

////////////////////////////////////////////////////////////////////////////////
// Pipeline related.

void MeshTopology::finishResizeTopology()
{
  if (!faces_first_vertex_index_.empty()) {
    face_vertex_indices_.resize(faces_first_vertex_index_.back());
  }
}

}  // namespace opensubdiv
}  // namespace blender
