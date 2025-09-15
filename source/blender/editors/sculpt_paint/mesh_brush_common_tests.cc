/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "mesh_brush_common.hh"

#include "BLI_bit_span.hh"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::ed::sculpt_paint::tests {
class MeshTests : public testing::Test {
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
    cube_mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 2, 2, 2);
  }

  void TearDown() override
  {
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(MeshTests, calc_vert_neighbors_interior)
{
  const OffsetIndices faces = cube_mesh->faces();
  const Span<int> corner_verts = cube_mesh->corner_verts();
  const GroupedSpan<int> vert_to_face_map = cube_mesh->vert_to_face_map();

  Vector<int> verts(8);
  for (const int i : verts.index_range()) {
    verts[i] = i;
  }

  const BitVector boundary_verts(int64_t(cube_mesh->verts_num));
  const Vector<bool> hide_poly(cube_mesh->faces_num, false);

  Vector<int> offset_data;
  Vector<int> data;
  const GroupedSpan<int> result = calc_vert_neighbors_interior(
      faces, corner_verts, vert_to_face_map, boundary_verts, hide_poly, verts, offset_data, data);

  ASSERT_EQ(result.size(), 8);
  for (const int i : result.index_range()) {
    ASSERT_EQ(result[i].size(), 3);
  }
}
}  // namespace blender::ed::sculpt_paint::tests
