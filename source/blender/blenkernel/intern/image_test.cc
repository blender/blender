/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_image.h"

#include "MEM_guardedalloc.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(udim, image_ensure_tile_token)
{
  auto verify = [](const char *original, const char *expected) {
    char result[FILE_MAX];

    STRNCPY(result, original);
    BKE_image_ensure_tile_token_filename_only(result, sizeof(result));
    EXPECT_STREQ(result, expected);
  };

  /* Already present tokens. */
  verify("test.<UDIM>.png", "test.<UDIM>.png");
  verify("test.<UVTILE>.png", "test.<UVTILE>.png");

  /* UDIM pattern detection. */
  verify("test.1002.png", "test.<UDIM>.png");
  verify("test-1002-ao.png", "test-<UDIM>-ao.png");
  verify("test_1002_ao.png", "test_<UDIM>_ao.png");
  verify("test.1002.ver0023.png", "test.<UDIM>.ver0023.png");
  verify("test.ver0023.1002.png", "test.ver0023.<UDIM>.png");
  verify("test.1002.1.png", "test.<UDIM>.1.png");
  verify("test.1.1002.png", "test.1.<UDIM>.png");
  verify("test-2022-01-01.1002.png", "test-2022-01-01.<UDIM>.png");
  verify("1111_11.1002.png", "1111_11.<UDIM>.png");
  verify("2111_01.1002.png", "2111_01.<UDIM>.png");
  verify("2022_1002_100200.1002.png", "2022_1002_100200.<UDIM>.png");

  /* UVTILE pattern detection. */
  verify("uv-test.u2_v10.png", "uv-test.<UVTILE>.png");
  verify("uv-test-u2_v10-ao.png", "uv-test-<UVTILE>-ao.png");
  verify("uv-test_u2_v10_ao.png", "uv-test_<UVTILE>_ao.png");
  verify("uv-test.u10_v100.png", "uv-test.<UVTILE>.png");
  verify("u_v-test.u2_v10.png", "u_v-test.<UVTILE>.png");
  verify("u2_v10uv-test.png", "<UVTILE>uv-test.png");
  verify("u2_v10u_v-test.png", "<UVTILE>u_v-test.png");

  /* Patterns which should not be detected as UDIMs. */
  for (const char *incorrect : {"1002.png",
                                "1002test.png",
                                "test1002.png",
                                "test(1002).png",
                                "(1002)test.png",
                                "test-1080p.png",
                                "test-1920x1080.png",
                                "test.123.png",
                                "test.12345.png",
                                "test.uv.png",
                                "test.u1v.png",
                                "test.uv1.png",
                                "test.u_v.png",
                                "test.u1_v.png",
                                "test.u_v2.png",
                                "test.u2v3.png",
                                "test.u123_v1.png",
                                "test.u1_v12345.png"})
  {
    /* These should not result in modifications happening. */
    verify(incorrect, incorrect);
  }
}

TEST(udim, image_get_tile_strformat)
{
  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern;

  /* Parameter validation. */
  udim_pattern = BKE_image_get_tile_strformat(nullptr, &tile_format);
  EXPECT_EQ(udim_pattern, nullptr);

  udim_pattern = BKE_image_get_tile_strformat("", nullptr);
  EXPECT_EQ(udim_pattern, nullptr);

  /* Typical usage. */
  udim_pattern = BKE_image_get_tile_strformat("", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_NONE);
  EXPECT_EQ(udim_pattern, nullptr);

  udim_pattern = BKE_image_get_tile_strformat("test.<UNKNOWN>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_NONE);
  EXPECT_EQ(udim_pattern, nullptr);

  udim_pattern = BKE_image_get_tile_strformat("test.<UDIM>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UDIM);
  EXPECT_STREQ(udim_pattern, "test.%d.png");
  MEM_freeN(udim_pattern);

  udim_pattern = BKE_image_get_tile_strformat("test.<UVTILE>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UVTILE);
  EXPECT_STREQ(udim_pattern, "test.u%d_v%d.png");
  MEM_freeN(udim_pattern);
}

TEST(udim, image_get_tile_number_from_filepath)
{
  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern;
  int tile_number;

  udim_pattern = BKE_image_get_tile_strformat("test.<UDIM>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UDIM);
  EXPECT_NE(udim_pattern, nullptr);

  /* Parameter validation. */
  EXPECT_FALSE(
      BKE_image_get_tile_number_from_filepath(nullptr, udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.1004.png", nullptr, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.1004.png", udim_pattern, UDIM_TILE_FORMAT_NONE, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.1004.png", udim_pattern, tile_format, nullptr));

  /* UDIM tile format tests. */
  EXPECT_TRUE(BKE_image_get_tile_number_from_filepath(
      "test.1004.png", udim_pattern, tile_format, &tile_number));
  EXPECT_EQ(tile_number, 1004);

  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "has_no_number.png", udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.X.png", udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "wrong.1004.png", udim_pattern, tile_format, &tile_number));

  MEM_freeN(udim_pattern);

  /* UVTILE tile format tests. */
  udim_pattern = BKE_image_get_tile_strformat("test.<UVTILE>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UVTILE);
  EXPECT_NE(udim_pattern, nullptr);

  EXPECT_TRUE(BKE_image_get_tile_number_from_filepath(
      "test.u2_v2.png", udim_pattern, tile_format, &tile_number));
  EXPECT_EQ(tile_number, 1012);

  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "has_no_number.png", udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.u1_vX.png", udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "test.uX_v1.png", udim_pattern, tile_format, &tile_number));
  EXPECT_FALSE(BKE_image_get_tile_number_from_filepath(
      "wrong.u2_v2.png", udim_pattern, tile_format, &tile_number));

  MEM_freeN(udim_pattern);
}

TEST(udim, image_set_filepath_from_tile_number)
{
  eUDIM_TILE_FORMAT tile_format;
  char *udim_pattern;

  udim_pattern = BKE_image_get_tile_strformat("test.<UDIM>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UDIM);
  EXPECT_NE(udim_pattern, nullptr);

  char filepath[FILE_MAX];

  /* Parameter validation. */
  STRNCPY(filepath, "xxxx");

  BKE_image_set_filepath_from_tile_number(nullptr, udim_pattern, tile_format, 1028);
  BKE_image_set_filepath_from_tile_number(filepath, nullptr, tile_format, 1028);
  EXPECT_STREQ(filepath, "xxxx");
  BKE_image_set_filepath_from_tile_number(filepath, udim_pattern, UDIM_TILE_FORMAT_NONE, 1028);
  EXPECT_STREQ(filepath, "xxxx");

  /* UDIM tile format tests. */
  BKE_image_set_filepath_from_tile_number(filepath, udim_pattern, tile_format, 1028);
  EXPECT_STREQ(filepath, "test.1028.png");
  MEM_freeN(udim_pattern);

  /* UVTILE tile format tests. */
  udim_pattern = BKE_image_get_tile_strformat("test.<UVTILE>.png", &tile_format);
  EXPECT_EQ(tile_format, UDIM_TILE_FORMAT_UVTILE);
  EXPECT_NE(udim_pattern, nullptr);

  BKE_image_set_filepath_from_tile_number(filepath, udim_pattern, tile_format, 1028);
  EXPECT_STREQ(filepath, "test.u8_v3.png");
  MEM_freeN(udim_pattern);
}

}  // namespace blender::bke::tests
