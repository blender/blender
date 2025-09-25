/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_paint_bvh.hh"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::bke::tests {
class PaintBVHTest : public testing::Test {
 public:
  Mesh *cube_mesh;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    cube_mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 10, 10, 10);
  }

  void TearDown() override
  {
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(PaintBVHTest, from_mesh)
{
  pbvh::Tree tree = pbvh::Tree::from_mesh(*cube_mesh);
  EXPECT_GT(tree.nodes<pbvh::MeshNode>().size(), 0)
      << "Paint BVH should have some non zero amount of nodes";
}
}  // namespace blender::bke::tests
