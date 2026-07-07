/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "sculpt_boundary.hh"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"

#include "CLG_log.h"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"
#include "GEO_mesh_primitive_grid.hh"

#include "testing/testing.h"

namespace blender::ed::sculpt_paint::tests {
class MeshTests : public testing::Test {
 public:
  Mesh *mesh = nullptr;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void TearDown() override
  {
    if (mesh) {
      BKE_id_free(nullptr, mesh);
    }
  }
};

TEST_F(MeshTests, create_boundary_info__cube)
{
  mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 2, 2, 2);

  SculptBoundaryInfoCache boundary_info_cache = boundary::create_boundary_info(*mesh);

  ASSERT_EQ(boundary_info_cache.verts.size(), mesh->verts_num);
  int num_false = 0;
  for (int i = 0; i < mesh->verts_num; ++i) {
    if (!boundary_info_cache.verts[i].test()) {
      num_false++;
    }
  }
  ASSERT_EQ(num_false, mesh->verts_num);
  ASSERT_EQ(boundary_info_cache.edges.size(), 0);
}

TEST_F(MeshTests, create_boundary_info__grid)
{
  mesh = geometry::create_grid_mesh(3, 3, 1.0, 1.0, {});

  SculptBoundaryInfoCache boundary_info_cache = boundary::create_boundary_info(*mesh);

  ASSERT_EQ(boundary_info_cache.verts.size(), mesh->verts_num);
  int num_true = 0;
  for (int i = 0; i < mesh->verts_num; ++i) {
    if (boundary_info_cache.verts[i].test()) {
      num_true++;
    }
  }

  ASSERT_EQ(num_true, 8);
  ASSERT_EQ(boundary_info_cache.edges.size(), 8);
  ASSERT_NE(boundary_info_cache.edges.size(), mesh->edges().size());
}

TEST_F(MeshTests, create_boundary_info__1D_strip)
{
  mesh = geometry::create_grid_mesh(3, 2, 1.0, 1.0, {});

  SculptBoundaryInfoCache boundary_info_cache = boundary::create_boundary_info(*mesh);

  ASSERT_EQ(boundary_info_cache.verts.size(), mesh->verts_num);
  int num_true = 0;
  for (int i = 0; i < mesh->verts_num; ++i) {
    if (boundary_info_cache.verts[i].test()) {
      num_true++;
    }
  }
  ASSERT_EQ(num_true, mesh->verts_num);
  ASSERT_EQ(boundary_info_cache.edges.size(), 6);
  ASSERT_NE(boundary_info_cache.edges.size(), mesh->edges().size());
}

}  // namespace blender::ed::sculpt_paint::tests
