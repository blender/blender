/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "mesh_brush_common.hh"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"

#include "BLI_array_utils.hh"
#include "BLI_map.hh"

#include "CLG_log.h"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"
#include "GEO_mesh_primitive_grid.hh"

#include "sculpt_boundary.hh"

#include "testing/testing.h"

namespace blender::ed::sculpt_paint::tests {
class MeshTests : public testing::Test {
 public:
  Mesh *mesh;

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

TEST_F(MeshTests, calc_vert_neighbors_interior__cube)
{
  mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 2, 2, 2);

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh->vert_to_face_map();
  const Vector<bool> hide_poly(mesh->faces_num, false);

  Array<int> verts(8);
  blender::array_utils::fill_index_range(verts.as_mutable_span(), 0);

  SculptBoundaryInfoCache boundary_info_cache = boundary::create_boundary_info(*mesh);

  Vector<int> offset_data;
  Vector<int> data;
  const GroupedSpan<int> result = calc_vert_neighbors_interior(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               boundary_info_cache.verts,
                                                               boundary_info_cache.edges,
                                                               hide_poly,
                                                               verts,
                                                               offset_data,
                                                               data);

  /* Each of the cube's 8 vertices should have 3 neighbor elements */
  Map<int, int> expected_counts = {{3, 8}};

  ASSERT_EQ(result.size(), 8);
  Map<int, int> calculated_counts;
  for (const int i : result.index_range()) {
    calculated_counts.add_or_modify(
        result[i].size(), [](int *value) { *value = 1; }, [](int *value) { (*value)++; });
  }
  for (const int key : expected_counts.keys()) {
    ASSERT_EQ(calculated_counts.lookup_default(key, -1), expected_counts.lookup_default(key, -2));
  }
}

TEST_F(MeshTests, calc_vert_neighbors_interior__1D_strip)
{
  mesh = geometry::create_grid_mesh(3, 2, 1.0, 1.0, {});

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh->vert_to_face_map();
  const Vector<bool> hide_poly(mesh->faces_num, false);

  Array<int> verts(6);
  blender::array_utils::fill_index_range(verts.as_mutable_span(), 0);

  SculptBoundaryInfoCache boundary_info_cache = boundary::create_boundary_info(*mesh);

  Vector<int> offset_data;
  Vector<int> data;
  const GroupedSpan<int> result = calc_vert_neighbors_interior(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               boundary_info_cache.verts,
                                                               boundary_info_cache.edges,
                                                               hide_poly,
                                                               verts,
                                                               offset_data,
                                                               data);
  /* The 4 corner elements should each have no neighbor. The remaining 2 elements should have 2
   * neighbors */
  Map<int, int> expected_counts = {{0, 4}, {2, 2}};

  ASSERT_EQ(result.size(), 6);
  Map<int, int> calculated_counts;
  for (const int i : result.index_range()) {
    calculated_counts.add_or_modify(
        result[i].size(), [](int *value) { *value = 1; }, [](int *value) { (*value)++; });
  }
  for (const int key : expected_counts.keys()) {
    ASSERT_EQ(calculated_counts.lookup_default(key, -1), expected_counts.lookup_default(key, -2));
  }
}
}  // namespace blender::ed::sculpt_paint::tests
