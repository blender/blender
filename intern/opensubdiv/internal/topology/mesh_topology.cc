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

  edges_.resize(num_edges_);
}

int MeshTopology::getNumEdges() const
{
  return num_edges_;
}

void MeshTopology::setEdgevertexIndices(int edge_index, int v1, int v2)
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  assert(v1 >= 0);
  assert(v1 < getNumVertices());

  assert(v2 >= 0);
  assert(v2 < getNumVertices());

  ensureNumEdgesAtLeast(edge_index + 1);

  EdgeTopology &edge = getEdge(edge_index);

  // Prevent attempts to override edges.
  // This is currently not supposed to happen.
  assert(!edge.isValid());

  edge.v1 = v1;
  edge.v2 = v2;
}

EdgeTopology &MeshTopology::getEdge(int edge_index)
{
  const MeshTopology *const_this = this;
  return const_cast<EdgeTopology &>(const_this->getEdge(edge_index));
}
const EdgeTopology &MeshTopology::getEdge(int edge_index) const
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  return edges_[edge_index];
}

void MeshTopology::setEdgeSharpness(int edge_index, float sharpness)
{
  assert(edge_index >= 0);
  assert(edge_index < getNumEdges());

  assert(getEdge(edge_index).isValid());

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
  if (getNumEdges() < num_edges) {
    setNumEdges(num_edges);
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

  faces_.resize(num_faces);
}

int MeshTopology::getNumFaces() const
{
  return num_faces_;
}

FaceTopology &MeshTopology::getFace(int face_index)
{
  const MeshTopology *const_this = this;
  return const_cast<FaceTopology &>(const_this->getFace(face_index));
}
const FaceTopology &MeshTopology::getFace(int face_index) const
{
  assert(face_index >= 0);
  assert(face_index < getNumFaces());

  return faces_[face_index];
}

void MeshTopology::setNumFaceVertices(int face_index, int num_face_vertices)
{
  FaceTopology &face = getFace(face_index);
  face.setNumVertices(num_face_vertices);
}

void MeshTopology::setFaceVertexIndices(int face_index, int *face_vertex_indices)
{
  FaceTopology &face = getFace(face_index);
  face.setVertexIndices(face_vertex_indices);
}

}  // namespace opensubdiv
}  // namespace blender
