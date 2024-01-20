/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENVDB

#  include "openvdb/openvdb.h"

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
  Volume *volume = static_cast<Volume *>(BKE_id_new(bmain, ID_VO, nullptr));
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
  Volume *volume_a = static_cast<Volume *>(BKE_id_new(bmain, ID_VO, nullptr));
  Volume *volume_b = static_cast<Volume *>(BKE_id_new(bmain, ID_VO, nullptr));
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

TEST_F(VolumeTest, lazy_load_grid)
{
  int load_counter = 0;
  auto load_grid = [&]() {
    load_counter++;
    return openvdb::FloatGrid::create(10.0f);
  };
  VolumeGrid<float> volume_grid{MEM_new<VolumeGridData>(__func__, load_grid)};
  EXPECT_EQ(load_counter, 0);
  EXPECT_FALSE(volume_grid->is_loaded());
  VolumeTreeAccessToken tree_token;
  EXPECT_EQ(volume_grid.grid(tree_token).background(), 10.0f);
  EXPECT_EQ(load_counter, 1);
  EXPECT_TRUE(volume_grid->is_loaded());
  EXPECT_TRUE(volume_grid->is_reloadable());
  EXPECT_EQ(volume_grid.grid(tree_token).background(), 10.0f);
  EXPECT_EQ(load_counter, 1);
  volume_grid->unload_tree_if_possible();
  EXPECT_TRUE(volume_grid->is_loaded());
  tree_token.reset();
  volume_grid->unload_tree_if_possible();
  EXPECT_FALSE(volume_grid->is_loaded());
  EXPECT_EQ(volume_grid.grid(tree_token).background(), 10.0f);
  EXPECT_TRUE(volume_grid->is_loaded());
  EXPECT_EQ(load_counter, 2);
  volume_grid.grid_for_write(tree_token).getAccessor().setValue({0, 0, 0}, 1.0f);
  EXPECT_EQ(volume_grid.grid(tree_token).getAccessor().getValue({0, 0, 0}), 1.0f);
  EXPECT_FALSE(volume_grid->is_reloadable());
}

TEST_F(VolumeTest, lazy_load_tree_only)
{
  bool load_run = false;
  auto load_grid = [&]() {
    load_run = true;
    return openvdb::FloatGrid::create(10.0f);
  };
  VolumeGrid<float> volume_grid{
      MEM_new<VolumeGridData>(__func__, load_grid, openvdb::FloatGrid::create(0.0f))};
  EXPECT_FALSE(volume_grid->is_loaded());
  EXPECT_EQ(volume_grid->name(), "");
  EXPECT_FALSE(load_run);
  volume_grid.get_for_write().set_name("Test");
  EXPECT_FALSE(load_run);
  EXPECT_EQ(volume_grid->name(), "Test");
  VolumeTreeAccessToken tree_token;
  volume_grid.grid_for_write(tree_token);
  EXPECT_TRUE(load_run);
  EXPECT_EQ(volume_grid->name(), "Test");
  EXPECT_EQ(volume_grid.grid(tree_token).background(), 10.0f);
}

}  // namespace blender::bke::tests

#endif /* WITH_OPENVDB */
