/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_gtest_base.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "DNA_mesh_types.h"

namespace blender::bke::tests {

class MeshValidateTest : public BlenderGTestBase {};

TEST_F(MeshValidateTest, AllowMissingEdgesWithoutCornerEdgeMapping)
{
  const int2 edges[] = {{1, 2}, {2, 0}, {0, 1}};
  for (const int edges_num : {0, 3}) {
    SCOPED_TRACE(edges_num);
    Mesh *mesh = BKE_mesh_new_nomain(3, edges_num, 1, 3);
    mesh->vert_positions_for_write().copy_from(
        {float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0)});
    mesh->face_offsets_for_write().copy_from({0, 3});
    mesh->corner_verts_for_write().copy_from({0, 1, 2});
    mesh->edges_for_write().copy_from(Span<int2>(edges).take_front(edges_num));
    /* Set corner edges to invalid value. */
    mesh->corner_edges_for_write().fill(-1);

    EXPECT_TRUE(mesh_validate(*mesh, true, true));
    EXPECT_EQ(mesh->edges_num, 3);
    EXPECT_TRUE(mesh_is_valid(*mesh));
    BKE_id_free(nullptr, mesh);
  }
}

}  // namespace blender::bke::tests
