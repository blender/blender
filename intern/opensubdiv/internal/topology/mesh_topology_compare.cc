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
#include <opensubdiv/sdc/crease.h>

#include "opensubdiv_converter_capi.h"

namespace blender {
namespace opensubdiv {

namespace {

////////////////////////////////////////////////////////////////////////////////
// Geometry.

// Edges.

int getEffectiveNumEdges(const OpenSubdiv_Converter *converter)
{
  if (converter->getNumEdges == nullptr) {
    return 0;
  }

  return converter->getNumEdges(converter);
}

bool isEqualEdgeGeometry(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  const int num_requested_edges = getEffectiveNumEdges(converter);
  if (num_requested_edges != mesh_topology.getNumEdges()) {
    return false;
  }

  for (int edge_index = 0; edge_index < num_requested_edges; ++edge_index) {
    int requested_edge_vertices[2];
    converter->getEdgeVertices(converter, edge_index, requested_edge_vertices);

    const EdgeTopology &current_edge = mesh_topology.getEdge(edge_index);
    if (current_edge.v1 != requested_edge_vertices[0] ||
        current_edge.v2 != requested_edge_vertices[1]) {
      return false;
    }
  }

  return true;
}

// Geometry comparison entry point.

bool isEqualGeometry(const MeshTopology &mesh_topology, const OpenSubdiv_Converter *converter)
{
  if (!isEqualEdgeGeometry(mesh_topology, converter)) {
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
