/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "internal/topology/mesh_topology.h"
#include "testing/testing.h"

namespace blender::opensubdiv {

TEST(MeshTopology, TrivialVertexSharpness)
{
  MeshTopology mesh_topology;

  mesh_topology.setNumVertices(3);
  mesh_topology.finishResizeTopology();

  mesh_topology.setVertexSharpness(0, 0.1f);
  mesh_topology.setVertexSharpness(1, 0.2f);

  EXPECT_EQ(mesh_topology.getVertexSharpness(0), 0.1f);
  EXPECT_EQ(mesh_topology.getVertexSharpness(1), 0.2f);
  EXPECT_EQ(mesh_topology.getVertexSharpness(2), 0.0f);
}

TEST(MeshTopology, TrivialEdgeSharpness)
{
  MeshTopology mesh_topology;

  mesh_topology.setNumVertices(8);
  mesh_topology.setNumEdges(3);
  mesh_topology.finishResizeTopology();

  mesh_topology.setEdgeVertexIndices(0, 0, 1);
  mesh_topology.setEdgeVertexIndices(1, 1, 2);
  mesh_topology.setEdgeVertexIndices(2, 2, 3);

  mesh_topology.setEdgeSharpness(0, 0.1f);
  mesh_topology.setEdgeSharpness(2, 0.2f);

  EXPECT_EQ(mesh_topology.getEdgeSharpness(0), 0.1f);
  EXPECT_EQ(mesh_topology.getEdgeSharpness(1), 0.0f);
  EXPECT_EQ(mesh_topology.getEdgeSharpness(2), 0.2f);
}

TEST(MeshTopology, TrivialFaceTopology)
{
  MeshTopology mesh_topology;

  mesh_topology.setNumFaces(3);
  mesh_topology.setNumFaceVertices(0, 4);
  mesh_topology.setNumFaceVertices(1, 3);
  mesh_topology.setNumFaceVertices(2, 5);
  mesh_topology.finishResizeTopology();

  EXPECT_EQ(mesh_topology.getNumFaceVertices(0), 4);
  EXPECT_EQ(mesh_topology.getNumFaceVertices(1), 3);
  EXPECT_EQ(mesh_topology.getNumFaceVertices(2), 5);

  {
    const int vertex_indices[] = {0, 1, 2, 3};
    mesh_topology.setFaceVertexIndices(0, 4, vertex_indices);
  }

  {
    const int vertex_indices[] = {4, 5, 6};
    mesh_topology.setFaceVertexIndices(1, 3, vertex_indices);
  }

  {
    const int vertex_indices[] = {7, 8, 9, 10, 11};
    mesh_topology.setFaceVertexIndices(2, 5, vertex_indices);
  }

  EXPECT_TRUE(mesh_topology.isFaceVertexIndicesEqual(0, {{0, 1, 2, 3}}));
  EXPECT_FALSE(mesh_topology.isFaceVertexIndicesEqual(0, {{10, 1, 2, 3}}));
  EXPECT_FALSE(mesh_topology.isFaceVertexIndicesEqual(0, {{0, 1, 2}}));

  EXPECT_TRUE(mesh_topology.isFaceVertexIndicesEqual(1, {{4, 5, 6}}));
  EXPECT_TRUE(mesh_topology.isFaceVertexIndicesEqual(2, {{7, 8, 9, 10, 11}}));
}

}  // namespace blender::opensubdiv
