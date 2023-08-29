/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "integrator/tile.h"
#include "util/math.h"

CCL_NAMESPACE_BEGIN

TEST(tile_calculate_best_size, Basic)
{
  /* Make sure CPU-like case is handled properly. */
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(1920, 1080), 1, 1, 1.0f), TileSize(1, 1, 1));
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(1920, 1080), 100, 1, 1.0f),
            TileSize(1, 1, 1));

  /* Enough path states to fit an entire image with all samples. */
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(1920, 1080), 1, 1920 * 1080, 1.0f),
            TileSize(1920, 1080, 1));
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(1920, 1080), 100, 1920 * 1080 * 100, 1.0f),
            TileSize(1920, 1080, 100));
}

TEST(tile_calculate_best_size, Extreme)
{
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(32, 32), 262144, 131072, 1.0f),
            TileSize(1, 1, 512));
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(32, 32), 1048576, 131072, 1.0f),
            TileSize(1, 1, 1024));
  EXPECT_EQ(tile_calculate_best_size(false, make_int2(32, 32), 10485760, 131072, 1.0f),
            TileSize(1, 1, 4096));

  EXPECT_EQ(tile_calculate_best_size(false, make_int2(32, 32), 8192 * 8192 * 2, 1024, 1.0f),
            TileSize(1, 1, 1024));
}

CCL_NAMESPACE_END
