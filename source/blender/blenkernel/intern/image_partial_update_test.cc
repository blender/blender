/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. */
#include "testing/testing.h"

#include "CLG_log.h"

#include "BKE_appdir.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_image_partial_update.hh"
#include "BKE_main.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "DNA_image_types.h"

#include "MEM_guardedalloc.h"

namespace blender::bke::image::partial_update {

constexpr float black_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

class ImagePartialUpdateTest : public testing::Test {
 protected:
  Main *bmain;
  Main *prev_bmain;
  Image *image;
  ImageTile *image_tile;
  ImageUser image_user = {nullptr};
  ImBuf *image_buffer;
  PartialUpdateUser *partial_update_user;

 private:
  Image *create_test_image(int width, int height)
  {
    return BKE_image_add_generated(bmain,
                                   width,
                                   height,
                                   "Test Image",
                                   32,
                                   true,
                                   IMA_GENTYPE_BLANK,
                                   black_color,
                                   false,
                                   false,
                                   false);
  }

 protected:
  void SetUp() override
  {
    CLG_init();
    BKE_idtype_init();
    BKE_appdir_init();
    IMB_init();

    bmain = BKE_main_new();
    /* Required by usage of #ID_BLEND_PATH_FROM_GLOBAL in #add_ibuf_for_tile. */
    prev_bmain = G_MAIN;
    G_MAIN = bmain;
    /* Creating an image generates a memory-leak during tests. */
    image = create_test_image(1024, 1024);
    image_tile = BKE_image_get_tile(image, 0);
    image_buffer = BKE_image_acquire_ibuf(image, nullptr, nullptr);

    partial_update_user = BKE_image_partial_update_create(image);
  }

  void TearDown() override
  {
    BKE_image_release_ibuf(image, image_buffer, nullptr);
    BKE_image_partial_update_free(partial_update_user);

    /* Restore original main in G_MAIN. */
    G_MAIN = prev_bmain;
    BKE_main_free(bmain);

    IMB_moviecache_destruct();
    IMB_exit();
    BKE_appdir_exit();
    CLG_exit();
  }
};

TEST_F(ImagePartialUpdateTest, mark_full_update)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark full update */
  BKE_image_partial_update_mark_full_update(image);

  /* Validate need full update followed by no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
}

TEST_F(ImagePartialUpdateTest, mark_single_tile)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region;
  BLI_rcti_init(&region, 10, 20, 40, 50);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);

  /* Partial Update should be available. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  /* Check tiles. */
  PartialUpdateRegion changed_region;
  ePartialUpdateIterResult iter_result;
  iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
  EXPECT_EQ(iter_result, ePartialUpdateIterResult::ChangeAvailable);
  EXPECT_EQ(BLI_rcti_inside_rcti(&changed_region.region, &region), true);
  iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
  EXPECT_EQ(iter_result, ePartialUpdateIterResult::Finished);

  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
}

TEST_F(ImagePartialUpdateTest, mark_unconnected_tiles)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region_a;
  BLI_rcti_init(&region_a, 10, 20, 40, 50);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region_a);
  rcti region_b;
  BLI_rcti_init(&region_b, 710, 720, 740, 750);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region_b);

  /* Partial Update should be available. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  /* Check tiles. */
  PartialUpdateRegion changed_region;
  ePartialUpdateIterResult iter_result;
  iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
  EXPECT_EQ(iter_result, ePartialUpdateIterResult::ChangeAvailable);
  EXPECT_EQ(BLI_rcti_inside_rcti(&changed_region.region, &region_b), true);
  iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
  EXPECT_EQ(iter_result, ePartialUpdateIterResult::ChangeAvailable);
  EXPECT_EQ(BLI_rcti_inside_rcti(&changed_region.region, &region_a), true);
  iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
  EXPECT_EQ(iter_result, ePartialUpdateIterResult::Finished);

  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
}

TEST_F(ImagePartialUpdateTest, donot_mark_outside_image)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region;
  /* Axis. */
  BLI_rcti_init(&region, -100, 0, 50, 100);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, 1024, 1100, 50, 100);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, 50, 100, -100, 0);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, 50, 100, 1024, 1100);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Diagonals. */
  BLI_rcti_init(&region, -100, 0, -100, 0);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, -100, 0, 1024, 1100);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, 1024, 1100, -100, 0);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  BLI_rcti_init(&region, 1024, 1100, 1024, 1100);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
}

TEST_F(ImagePartialUpdateTest, mark_inside_image)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region;
  BLI_rcti_init(&region, 0, 1, 0, 1);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
  BLI_rcti_init(&region, 1023, 1024, 0, 1);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
  BLI_rcti_init(&region, 1023, 1024, 1023, 1024);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
  BLI_rcti_init(&region, 1023, 1024, 0, 1);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);
}

TEST_F(ImagePartialUpdateTest, sequential_mark_region)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  {
    /* Mark region. */
    rcti region;
    BLI_rcti_init(&region, 10, 20, 40, 50);
    BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);

    /* Partial Update should be available. */
    result = BKE_image_partial_update_collect_changes(image, partial_update_user);
    EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

    /* Check tiles. */
    PartialUpdateRegion changed_region;
    ePartialUpdateIterResult iter_result;
    iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
    EXPECT_EQ(iter_result, ePartialUpdateIterResult::ChangeAvailable);
    EXPECT_EQ(BLI_rcti_inside_rcti(&changed_region.region, &region), true);
    iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
    EXPECT_EQ(iter_result, ePartialUpdateIterResult::Finished);

    result = BKE_image_partial_update_collect_changes(image, partial_update_user);
    EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
  }

  {
    /* Mark different region. */
    rcti region;
    BLI_rcti_init(&region, 710, 720, 740, 750);
    BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);

    /* Partial Update should be available. */
    result = BKE_image_partial_update_collect_changes(image, partial_update_user);
    EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

    /* Check tiles. */
    PartialUpdateRegion changed_region;
    ePartialUpdateIterResult iter_result;
    iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
    EXPECT_EQ(iter_result, ePartialUpdateIterResult::ChangeAvailable);
    EXPECT_EQ(BLI_rcti_inside_rcti(&changed_region.region, &region), true);
    iter_result = BKE_image_partial_update_get_next_change(partial_update_user, &changed_region);
    EXPECT_EQ(iter_result, ePartialUpdateIterResult::Finished);

    result = BKE_image_partial_update_collect_changes(image, partial_update_user);
    EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);
  }
}

TEST_F(ImagePartialUpdateTest, mark_multiple_chunks)
{
  ePartialUpdateCollectResult result;
  /* First tile should always return a full update. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region;
  BLI_rcti_init(&region, 300, 700, 300, 700);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);

  /* Partial Update should be available. */
  result = BKE_image_partial_update_collect_changes(image, partial_update_user);
  EXPECT_EQ(result, ePartialUpdateCollectResult::PartialChangesDetected);

  /* Check tiles. */
  PartialUpdateRegion changed_region;
  int num_chunks_found = 0;
  while (BKE_image_partial_update_get_next_change(partial_update_user, &changed_region) ==
         ePartialUpdateIterResult::ChangeAvailable) {
    BLI_rcti_isect(&changed_region.region, &region, nullptr);
    num_chunks_found++;
  }
  EXPECT_EQ(num_chunks_found, 4);
}

TEST_F(ImagePartialUpdateTest, iterator)
{
  PartialUpdateChecker<NoTileData> checker(image, &image_user, partial_update_user);
  /* First tile should always return a full update. */
  PartialUpdateChecker<NoTileData>::CollectResult changes = checker.collect_changes();
  EXPECT_EQ(changes.get_result_code(), ePartialUpdateCollectResult::FullUpdateNeeded);
  /* Second invoke should now detect no changes. */
  changes = checker.collect_changes();
  EXPECT_EQ(changes.get_result_code(), ePartialUpdateCollectResult::NoChangesDetected);

  /* Mark region. */
  rcti region;
  BLI_rcti_init(&region, 300, 700, 300, 700);
  BKE_image_partial_update_mark_region(image, image_tile, image_buffer, &region);

  /* Partial Update should be available. */
  changes = checker.collect_changes();
  EXPECT_EQ(changes.get_result_code(), ePartialUpdateCollectResult::PartialChangesDetected);

  /* Check tiles. */
  int num_tiles_found = 0;
  while (changes.get_next_change() == ePartialUpdateIterResult::ChangeAvailable) {
    BLI_rcti_isect(&changes.changed_region.region, &region, nullptr);
    num_tiles_found++;
  }
  EXPECT_EQ(num_tiles_found, 4);
}

}  // namespace blender::bke::image::partial_update
