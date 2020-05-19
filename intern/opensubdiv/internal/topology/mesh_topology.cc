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

MeshTopology::MeshTopology()
{
}

MeshTopology::~MeshTopology()
{
}

void MeshTopology::setNumVertices(int num_vertices)
{
  vertices.resize(num_vertices);
}
int MeshTopology::getNumVertices() const
{
  return vertices.size();
}

void MeshTopology::setVertexSharpness(int vertex_index, float sharpness)
{
  assert(vertex_index >= 0);
  assert(vertex_index < vertices.size());

  vertices[vertex_index].sharpness = sharpness;
}

}  // namespace opensubdiv
}  // namespace blender
