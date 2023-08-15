/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_bitmap.h"
#include "testing/testing.h"

namespace blender::tests {

TEST(bitmap, empty_is_all_unset)
{
  BLI_BITMAP_DECLARE(bitmap, 10);
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(BLI_BITMAP_TEST_BOOL(bitmap, i));
  }
}

TEST(bitmap, find_first_unset_empty)
{
  BLI_BITMAP_DECLARE(bitmap, 10);
  EXPECT_EQ(0, BLI_bitmap_find_first_unset(bitmap, 10));
}

TEST(bitmap, find_first_unset_full)
{
  BLI_BITMAP_DECLARE(bitmap, 10);
  BLI_bitmap_flip_all(bitmap, 10);
  EXPECT_EQ(-1, BLI_bitmap_find_first_unset(bitmap, 10));
}

TEST(bitmap, find_first_unset_middle)
{
  BLI_BITMAP_DECLARE(bitmap, 100);
  BLI_bitmap_flip_all(bitmap, 100);
  /* Turn some bits off */
  BLI_BITMAP_DISABLE(bitmap, 53);
  BLI_BITMAP_DISABLE(bitmap, 81);
  BLI_BITMAP_DISABLE(bitmap, 85);
  BLI_BITMAP_DISABLE(bitmap, 86);

  /* Find lowest unset bit, and set it. */
  EXPECT_EQ(53, BLI_bitmap_find_first_unset(bitmap, 100));
  BLI_BITMAP_ENABLE(bitmap, 53);
  /* Now should find the next lowest bit. */
  EXPECT_EQ(81, BLI_bitmap_find_first_unset(bitmap, 100));
}

}  // namespace blender::tests
