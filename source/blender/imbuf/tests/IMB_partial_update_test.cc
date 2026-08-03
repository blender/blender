/* SPDX-FileCopyrightText: 2020-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_rect.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_partial_update.hh"

#include <cstdint>

namespace blender::imbuf::partial_update::tests {

/* Collect the changes to #buffer since #changeset_id, then advance #changeset_id to the
 * returned point, mirroring how a consumer persists its #changeset_id between collects. */
static Changes collect(ImBuf *buffer, int64_t &changeset_id)
{
  IMB_partial_update_flush(buffer);
  const int64_t new_changeset_id = IMB_partial_update_changeset_id_current();
  Changes changes = IMB_partial_update_collect(buffer, changeset_id);
  changeset_id = new_changeset_id;
  return changes;
}

class IMBPartialUpdateTest : public ::testing::Test {
 protected:
  ImBuf *image_buffer = nullptr;

  void SetUp() override
  {
    /* Only the dimensions matter for change tracking; no pixel data is needed. */
    image_buffer = IMB_allocImBuf(1024, 1024, ImBufFlags::Zero);
  }

  void TearDown() override
  {
    IMB_freeImBuf(image_buffer);
  }
};

TEST_F(IMBPartialUpdateTest, initial_full_update)
{
  /* Check that we get a single full update for a new image buffer. */
  const int64_t last_changeset_id = -1;
  IMB_partial_update_flush(image_buffer);
  const int64_t new_changeset_id = IMB_partial_update_changeset_id_current();
  EXPECT_EQ(IMB_partial_update_collect(image_buffer, last_changeset_id).kind, Changes::Kind::Full);
  EXPECT_EQ(IMB_partial_update_collect(image_buffer, new_changeset_id).kind, Changes::Kind::None);
}

TEST_F(IMBPartialUpdateTest, mark_full_update)
{
  int64_t changeset_id = -1;
  /* The first collect should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* The second collect should detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Mark full update */
  IMB_partial_update_mark_full(image_buffer);

  /* Validate need full update followed by no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);
}

TEST_F(IMBPartialUpdateTest, mark_update_global_order)
{
  /* Test full and partial update. */
  int64_t changeset_id = -1;
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  rcti region;
  BLI_rcti_init(&region, 10, 20, 40, 50);
  IMB_partial_update_mark_region(image_buffer, region);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Partial);

  /* Test second image buffer has higher changeset ID due to global order. */
  ImBuf *later = IMB_allocImBuf(1024, 1024, ImBufFlags::Zero);
  EXPECT_EQ(collect(later, changeset_id).kind, Changes::Kind::Full);
  IMB_freeImBuf(later);
}

TEST_F(IMBPartialUpdateTest, resize)
{
  int64_t changeset_id = -1;

  /* Add some history. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  rcti region;
  BLI_rcti_init(&region, 10, 20, 40, 50);
  IMB_partial_update_mark_region(image_buffer, region);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Partial);

  /* A consumer in sync at the old resolution. */
  int64_t old_changeset_id = changeset_id;

  /* Change resolution. and detect resize */
  IMB_scale(image_buffer, 512, 512, IMBScaleFilter::Nearest);
  EXPECT_EQ(collect(image_buffer, old_changeset_id).kind, Changes::Kind::Resized);
  EXPECT_EQ(collect(image_buffer, old_changeset_id).kind, Changes::Kind::None);

  /* Check resize is no longer detected. */
  IMB_partial_update_mark_full(image_buffer);
  EXPECT_EQ(collect(image_buffer, old_changeset_id).kind, Changes::Kind::Full);

  /* Ensure resize followed by full is resize. */
  IMB_scale(image_buffer, 256, 256, IMBScaleFilter::Nearest);
  IMB_partial_update_mark_full(image_buffer);
  EXPECT_EQ(collect(image_buffer, old_changeset_id).kind, Changes::Kind::Resized);
}

TEST_F(IMBPartialUpdateTest, mark_single_region)
{
  int64_t changeset_id = -1;
  /* First should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* Second should now detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Mark region. */
  rcti region;
  BLI_rcti_init(&region, 10, 20, 40, 50);
  IMB_partial_update_mark_region(image_buffer, region);

  /* Partial Update should be available. */
  const Changes changes = collect(image_buffer, changeset_id);
  EXPECT_EQ(changes.kind, Changes::Kind::Partial);

  /* Check regions. */
  const Vector<rcti> regions = changes.modified_regions();
  ASSERT_EQ(regions.size(), 1);
  EXPECT_TRUE(BLI_rcti_inside_rcti(&regions.first(), &region));

  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);
}

TEST_F(IMBPartialUpdateTest, mark_unconnected_regions)
{
  int64_t changeset_id = -1;
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);

  /* Two regions in different chunks. */
  static_assert(CHUNK_SIZE == 256);
  rcti region_a;
  BLI_rcti_init(&region_a, 10, 20, 40, 50);
  IMB_partial_update_mark_region(image_buffer, region_a);
  rcti region_b;
  BLI_rcti_init(&region_b, 600, 610, 700, 710);
  IMB_partial_update_mark_region(image_buffer, region_b);

  const Changes changes = collect(image_buffer, changeset_id);
  EXPECT_EQ(changes.kind, Changes::Kind::Partial);
  const Vector<rcti> regions = changes.modified_regions();
  EXPECT_EQ(regions.size(), 2);

  bool found_a = false;
  bool found_b = false;
  for (const rcti &changed : regions) {
    found_a |= BLI_rcti_inside_rcti(&changed, &region_a);
    found_b |= BLI_rcti_inside_rcti(&changed, &region_b);
  }
  EXPECT_TRUE(found_a);
  EXPECT_TRUE(found_b);
}

TEST_F(IMBPartialUpdateTest, donot_mark_outside_image)
{
  int64_t changeset_id = -1;
  /* First tile should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* Second invoke should now detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Axis. */
  IMB_partial_update_mark_region(image_buffer, {-100, 0, 50, 100});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {1024, 1100, 50, 100});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {50, 100, -100, 0});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {50, 100, 1024, 1100});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Diagonals. */
  IMB_partial_update_mark_region(image_buffer, {-100, 0, -100, 0});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {-100, 0, 1024, 1100});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {1024, 1100, -100, 0});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  IMB_partial_update_mark_region(image_buffer, {1024, 1100, 1024, 1100});
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);
}

TEST_F(IMBPartialUpdateTest, mark_inside_image)
{
  int64_t changeset_id = -1;
  /* First should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* Second should now detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Regions that touch the image edges are still inside, and detected as partial changes. */
  rcti region;
  BLI_rcti_init(&region, 0, 1, 0, 1);
  IMB_partial_update_mark_region(image_buffer, region);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Partial);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  BLI_rcti_init(&region, 1023, 1024, 0, 1);
  IMB_partial_update_mark_region(image_buffer, region);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Partial);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  BLI_rcti_init(&region, 1023, 1024, 1023, 1024);
  IMB_partial_update_mark_region(image_buffer, region);
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Partial);
}

TEST_F(IMBPartialUpdateTest, sequential_mark_region)
{
  int64_t changeset_id = -1;
  /* First should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* Second should now detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  {
    /* Mark region. */
    rcti region;
    BLI_rcti_init(&region, 10, 20, 40, 50);
    IMB_partial_update_mark_region(image_buffer, region);

    /* Partial Update should be available. */
    const Changes changes = collect(image_buffer, changeset_id);
    EXPECT_EQ(changes.kind, Changes::Kind::Partial);

    /* Check regions. */
    const Vector<rcti> regions = changes.modified_regions();
    ASSERT_EQ(regions.size(), 1);
    EXPECT_TRUE(BLI_rcti_inside_rcti(&regions.first(), &region));

    EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);
  }

  {
    /* Mark different region. */
    rcti region;
    BLI_rcti_init(&region, 710, 720, 740, 750);
    IMB_partial_update_mark_region(image_buffer, region);

    /* Partial Update should be available. */
    const Changes changes = collect(image_buffer, changeset_id);
    EXPECT_EQ(changes.kind, Changes::Kind::Partial);

    /* Check regions. */
    const Vector<rcti> regions = changes.modified_regions();
    ASSERT_EQ(regions.size(), 1);
    EXPECT_TRUE(BLI_rcti_inside_rcti(&regions.first(), &region));

    EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);
  }
}

TEST_F(IMBPartialUpdateTest, mark_multiple_chunks)
{
  int64_t changeset_id = -1;
  /* First should always return a full update. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::Full);
  /* Second should now detect no changes. */
  EXPECT_EQ(collect(image_buffer, changeset_id).kind, Changes::Kind::None);

  /* Mark a region spanning multiple chunks. */
  static_assert(CHUNK_SIZE == 256);
  rcti region;
  BLI_rcti_init(&region, 300, 700, 300, 700);
  IMB_partial_update_mark_region(image_buffer, region);

  /* Partial Update should be available. */
  const Changes changes = collect(image_buffer, changeset_id);
  EXPECT_EQ(changes.kind, Changes::Kind::Partial);

  /* Contiguous modified chunks in a row are merged. */
  const Vector<rcti> regions = changes.modified_regions();
  ASSERT_EQ(regions.size(), 1);
  EXPECT_TRUE(BLI_rcti_inside_rcti(&regions.first(), &region));
}

}  // namespace blender::imbuf::partial_update::tests
