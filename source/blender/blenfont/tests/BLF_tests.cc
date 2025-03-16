/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLF_api.hh"
#include "BLI_path_utils.hh"
#include "testing/testing.h"

namespace blender::tests {

static std::string font_path(std::string font_name)
{
  char path[FILE_MAX];
  BLI_path_join(path,
                sizeof(path),
                blender::tests::flags_test_asset_dir().c_str(),
                "blenfont",
                font_name.c_str());
  return std::string(path);
}

static int open_font(std::string font_name)
{
  BLF_init();
  return BLF_load(font_path(font_name).c_str());
}

static void close_font(int id)
{
  BLF_unload_id(id);
  BLF_exit();
}

TEST(blf_load, load)
{
  const int id = open_font("Ahem.ttf");
  EXPECT_TRUE(id != -1);
  close_font(id);
}

TEST(blf_load, font_is_loaded_path)
{
  BLF_init();
  std::string path = font_path("Ahem.ttf");
  const int id = BLF_load(path.c_str());
  EXPECT_TRUE(BLF_is_loaded(path.c_str()));
  close_font(id);
}

TEST(blf_load, font_is_loaded_id)
{
  const int id = open_font("Ahem.ttf");
  EXPECT_TRUE(BLF_is_loaded_id(id));
  close_font(id);
}

TEST(blf_load, display_name_from_file)
{
  std::string path = font_path("Ahem.ttf");
  const char *name = BLF_display_name_from_file(path.c_str());
  EXPECT_TRUE(STREQ(name, "Ahem Regular"));
  /* BLF_display_name result must be freed. */
  MEM_freeN(name);
}

TEST(blf_load, display_name_from_id)
{
  const int id = open_font("Ahem.ttf");
  const char *name = BLF_display_name_from_id(id);
  EXPECT_TRUE(STREQ(name, "Ahem Regular"));
  /* BLF_display_name result must be freed. */
  MEM_freeN(name);
  close_font(id);
}

TEST(blf_load, has_glyph)
{
  const int id = open_font("Ahem.ttf");
  const bool has_glyph = BLF_has_glyph(id, 0x0058); /* 'X' */
  EXPECT_TRUE(has_glyph);
  close_font(id);
}

TEST(blf_metrics, get_vfont_metrics)
{
  const int id = open_font("Ahem.ttf");
  float ascend_ratio = 0.0f;
  float em_ratio = 0.0f;
  float scale = 0.0f;
  const bool has_metrics = BLF_get_vfont_metrics(id, &ascend_ratio, &em_ratio, &scale);
  EXPECT_TRUE(has_metrics);
  EXPECT_TRUE(ascend_ratio == 0.8f);
  EXPECT_TRUE(em_ratio == 1.0f);
  EXPECT_TRUE(scale == 0.001f);
  close_font(id);
}

TEST(blf_metrics, default_weight)
{
  const int id = open_font("Ahem.ttf");
  const int weight = BLF_default_weight(id);
  EXPECT_TRUE(weight == 400);
  close_font(id);
}

TEST(blf_metrics, has_variable_weight)
{
  const int id = open_font("Roboto.ttf");
  const bool has_variable_weight = BLF_has_variable_weight(id);
  EXPECT_TRUE(has_variable_weight);
  close_font(id);
}

TEST(blf_metrics, variable_weight)
{
  const int id = open_font("Roboto.ttf");
  const char sample[] = "MM";
  BLF_size(id, 100.0f);
  BLF_character_weight(id, 300);
  const float width_thin = BLF_width(id, sample, sizeof(sample));
  BLF_character_weight(id, 600);
  const float width_wide = BLF_width(id, sample, sizeof(sample));
  EXPECT_TRUE(width_wide > width_thin);
  close_font(id);
}

TEST(blf_dimensions, width_max)
{
  const int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const int width = BLF_width_max(id);
  EXPECT_TRUE(width == 100.0f);
  close_font(id);
}

TEST(blf_dimensions, height_max)
{
  const int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const int height = BLF_height_max(id);
  EXPECT_TRUE(height == 100.0f);
  close_font(id);
}

TEST(blf_dimensions, descender)
{
  const int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const int descender = BLF_descender(id);
  EXPECT_TRUE(descender == -20);
  close_font(id);
}

TEST(blf_dimensions, ascender)
{
  const int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const int ascender = BLF_ascender(id);
  EXPECT_TRUE(ascender == 80);
  close_font(id);
}

TEST(blf_dimensions, fixed_width)
{
  /* Ahem does not have all the characters needed for calculation. */
  const int id = open_font("Roboto.ttf");
  BLF_size(id, 100.0f);
  const float width = BLF_fixed_width(id);
  EXPECT_TRUE(width == 56.0f);
  close_font(id);
}

TEST(blf_dimensions, width_em)
{
  /* In this test font, 'X' is exactly one em wide. */
  const char sample[] = "XX";
  int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const float width = BLF_width(id, sample, sizeof(sample));
  EXPECT_TRUE(width == 200.0f);
  close_font(id);
}

TEST(blf_dimensions, height_em)
{
  /* In this test font, 'X' is exactly one em high. */
  const char sample[] = "X";
  int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const float width = BLF_height(id, sample, sizeof(sample));
  EXPECT_TRUE(width == 100.0f);
  close_font(id);
}

TEST(blf_dimensions, advance)
{
  /* In this test font, 'X' has advance of exactly one em. */
  const char sample[] = "X";
  int id = open_font("Ahem.ttf");
  BLF_size(id, 100.0f);
  const float width = BLF_glyph_advance(id, sample);
  EXPECT_NEAR(width, 100.0f, 0.000001f);
  close_font(id);
}

TEST(blf_wrapping_minimal, wrap_overflow_ascii)
{
  /* Do not break, even though over the wrap limit. */
  const char sample[] =
      "xxxxxxxxxxxxxxxxxxx!\"#$%&\'()*+,-./"
      "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
  /* Ahem does not contain all the characters included in above string. */
  int id = open_font("Roboto.ttf");
  BLF_size(id, 10.0f);
  const float width = BLF_width(id, sample, sizeof(sample));
  blender::Vector<blender::StringRef> wrapped = BLF_string_wrap(
      id, sample, int(float(width) * 0.05f));
  EXPECT_TRUE(wrapped.size() == 1 && (strlen(sample) == wrapped[0].size()));
  close_font(id);
}

TEST(blf_wrapping_minimal, wrap_space)
{
  /* Must break at the center spaces into two, one space trailing, one leading. */
  const char sample[] = "x xxxxxxxxxxxxxxxx  xxxxxxxxxxxxxxxxxxx ";
  int id = open_font("Ahem.ttf");
  BLF_size(id, 10.0f);
  const float width = BLF_width(id, sample, sizeof(sample));
  blender::Vector<blender::StringRef> wrapped = BLF_string_wrap(
      id, sample, int(float(width) * 0.7f));
  EXPECT_TRUE(wrapped.size() == 2 && wrapped[0].back() == ' ' && wrapped[1].substr(0, 1) != " ");
  close_font(id);
}

TEST(blf_wrapping_minimal, wrap_linefeed)
{
  /* Must break on every line feed except at end of string. */
  const char sample[] = "x\nxxxxxxxxxxxxxxxx\n\nxxxxxxxxxxxxxxxxxxx\n";
  int id = open_font("Ahem.ttf");
  BLF_size(id, 10.0f);
  const float width = BLF_width(id, sample, sizeof(sample));
  blender::Vector<blender::StringRef> wrapped = BLF_string_wrap(
      id, sample, int(float(width) * 0.7f));
  EXPECT_TRUE(wrapped.size() == 4 && wrapped[2].is_empty());
  close_font(id);
}

}  // namespace blender::tests
