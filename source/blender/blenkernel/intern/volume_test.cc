/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENVDB

#  include "testing/testing.h"

#  include "DNA_volume_types.h"

#  include "BKE_idtype.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_main.hh"
#  include "BKE_volume.hh"
#  include "BKE_volume_grid.hh"

namespace blender::bke::tests {

class VolumeTest : public ::testing::Test {
 public:
  Main *bmain;

  static void SetUpTestSuite()
  {
    BKE_idtype_init();
  }

  static void TearDownTestSuite() {}

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(VolumeTest, add_grid_with_name_and_find)
{
  Volume *volume = BKE_id_new<Volume>(bmain, nullptr);
  GVolumeGrid grid{VOLUME_GRID_FLOAT};
  grid.get_for_write().set_name("My Grid");
  const VolumeGridData *grid_data = grid.release();
  BKE_volume_grid_add(volume, *grid_data);
  EXPECT_EQ(grid_data, BKE_volume_grid_find(volume, "My Grid"));
  EXPECT_TRUE(grid_data->is_mutable());
  BKE_id_free(bmain, volume);
}

TEST_F(VolumeTest, add_grid_in_two_volumes)
{
  Volume *volume_a = BKE_id_new<Volume>(bmain, nullptr);
  Volume *volume_b = BKE_id_new<Volume>(bmain, nullptr);
  GVolumeGrid grid{VOLUME_GRID_FLOAT};
  grid.get_for_write().set_name("My Grid");
  const VolumeGridData *grid_data = grid.release();
  BKE_volume_grid_add(volume_a, *grid_data);
  EXPECT_TRUE(grid_data->is_mutable());
  grid_data->add_user();
  BKE_volume_grid_add(volume_b, *grid_data);
  EXPECT_FALSE(grid_data->is_mutable());

  VolumeGridData *grid_from_a = BKE_volume_grid_get_for_write(volume_a, 0);
  const VolumeGridData *grid_from_b = BKE_volume_grid_get(volume_b, 0);
  EXPECT_NE(grid_data, grid_from_a);
  EXPECT_TRUE(grid_from_a->is_mutable());
  EXPECT_TRUE(grid_from_b->is_mutable());

  BKE_id_free(bmain, volume_a);
  BKE_id_free(bmain, volume_b);
}

}  // namespace blender::bke::tests

#endif /* WITH_OPENVDB */
