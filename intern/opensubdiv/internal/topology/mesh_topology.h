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

#ifndef OPENSUBDIV_MESH_TOPOLOGY_H_
#define OPENSUBDIV_MESH_TOPOLOGY_H_

#include "internal/base/memory.h"
#include "internal/base/type.h"

struct OpenSubdiv_Converter;

namespace blender {
namespace opensubdiv {

class VertexTopologyTag {
 public:
  float sharpness = 0.0f;
};

class EdgeTopologyTag {
 public:
  float sharpness = 0.0f;
};

// Simplified representation of mesh topology.
// Only includes parts of actual mesh topology which is needed to perform
// comparison between Application side and OpenSubddiv side.
class MeshTopology {
 public:
  MeshTopology();
  MeshTopology(const MeshTopology &other) = default;
  MeshTopology(MeshTopology &&other) noexcept = default;
  ~MeshTopology();

  MeshTopology &operator=(const MeshTopology &other) = default;
  MeshTopology &operator=(MeshTopology &&other) = default;

  //////////////////////////////////////////////////////////////////////////////
  // Vertices.

  void setNumVertices(int num_vertices);
  int getNumVertices() const;

  void setVertexSharpness(int vertex_index, float sharpness);
  float getVertexSharpness(int vertex_index) const;

  //////////////////////////////////////////////////////////////////////////////
  // Edges.

  void setNumEdges(int num_edges);

  // NOTE: Unless full topology was specified will return number of edges based
  // on last edge index for which topology tag was specified.
  int getNumEdges() const;

  void setEdgeSharpness(int edge_index, float sharpness);
  float getEdgeSharpness(int edge_index) const;

 protected:
  // Unless full topology was specified the number of edges is not know ahead
  // of a time.
  void ensureNumEdgesAtLeast(int num_edges);

  // Geometry tags are stored sparsly.
  //
  // These functions ensures that the storage can be addressed by an index which
  // corresponds to the given size.
  void ensureVertexTagsSize(int num_vertices);
  void ensureEdgeTagsSize(int num_edges);

  int num_vertices_;
  vector<VertexTopologyTag> vertex_tags_;

  int num_edges_;
  vector<EdgeTopologyTag> edge_tags_;

  MEM_CXX_CLASS_ALLOC_FUNCS("MeshTopology");
};

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_MESH_TOPOLOGY_H_
