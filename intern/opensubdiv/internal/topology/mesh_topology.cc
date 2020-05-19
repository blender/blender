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

MeshTopology::MeshTopology() : num_vertices_(0), num_edges_(0)
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

void MeshTopology::setEdgeSharpness(int edge_index, float sharpness)
{
  assert(edge_index >= 0);

  ensureNumEdgesAtLeast(edge_index + 1);

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

}  // namespace opensubdiv
}  // namespace blender
