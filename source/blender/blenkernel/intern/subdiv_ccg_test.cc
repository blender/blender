/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"

namespace blender::bke::tests {
TEST(subdiv_ccg_coord, to_index)
{
  CCGKey key;
  key.level = 2;
  key.elem_size = sizeof(float);

  key.has_normals = false;
  key.has_mask = false;

  key.normal_offset = -1;
  key.mask_offset = -1;

  key.grid_size = BKE_ccg_gridsize(key.level);   /* 3 */
  key.grid_area = key.grid_size * key.grid_size; /* 9 */
  key.grid_bytes = key.grid_area * key.elem_size;

  SubdivCCGCoord coord;
  coord.grid_index = 2;
  coord.x = 1;
  coord.y = 1;

  /* (grid_index * grid_area) + y * grid_size + x */
  /* (2 * 9) + (1 * 3) + 1 */
  EXPECT_EQ(coord.to_index(key), 22);
}

TEST(subdiv_ccg_coord, constructor)
{
  CCGKey key;
  key.level = 2;
  key.elem_size = sizeof(float);

  key.has_normals = false;
  key.has_mask = false;

  key.normal_offset = -1;
  key.mask_offset = -1;

  key.grid_size = BKE_ccg_gridsize(key.level);   /* 3 */
  key.grid_area = key.grid_size * key.grid_size; /* 9 */
  key.grid_bytes = key.grid_area * key.elem_size;

  SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, 22);
  coord.grid_index = 2;
  coord.x = 1;
  coord.y = 1;

  EXPECT_EQ(coord.grid_index, 2);
  EXPECT_EQ(coord.x, 1);
  EXPECT_EQ(coord.y, 1);
}
}  // namespace blender::bke::tests
