/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"

#include "sculpt_undo.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::ed::sculpt_paint::undo::tests {

class SculptUndoTest : public testing::Test {
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
    cube_mesh = geometry::create_cuboid_mesh(float3(1, 1, 1), 50, 50, 50);
  }

  void TearDown() override
  {
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(SculptUndoTest, CompressRoundTrip)
{
  Mesh *mesh = this->cube_mesh;

  Vector<std::byte> buffer;
  Vector<std::byte> compressed;

  {
    compression::filter_compress<float3>(mesh->vert_positions(), buffer, compressed);
    Vector<float3> decompressed;
    compression::filter_decompress<float3>(compressed, buffer, decompressed);
    EXPECT_EQ(mesh->vert_positions().size(), decompressed.size());
    EXPECT_EQ_SPAN(mesh->vert_positions(), decompressed.as_span());
  }

  {
    compression::filter_compress<int>(mesh->corner_verts(), buffer, compressed);
    Vector<int> decompressed;
    compression::filter_decompress<int>(compressed, buffer, decompressed);
    EXPECT_EQ(mesh->corner_verts().size(), decompressed.size());
    EXPECT_EQ_SPAN(mesh->corner_verts(), decompressed.as_span());
  }
}

}  // namespace blender::ed::sculpt_paint::undo::tests
