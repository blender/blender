/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_interp.h"

static constexpr int image_width = 3;
static constexpr int image_height = 3;
static constexpr unsigned char image_char[image_height][image_width][4] = {
    {{255, 254, 217, 216}, {230, 230, 230, 230}, {240, 160, 90, 20}},
    {{0, 1, 2, 3}, {62, 72, 82, 92}, {126, 127, 128, 129}},
    {{1, 2, 3, 4}, {73, 108, 153, 251}, {128, 129, 130, 131}},
};

TEST(math_interp, BilinearCharExactSamples)
{
  unsigned char res[4];
  unsigned char exp1[4] = {73, 108, 153, 251};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 1.0f, 2.0f);
  EXPECT_EQ_ARRAY(exp1, res, 4);
  unsigned char exp2[4] = {240, 160, 90, 20};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 2.0f, 0.0f);
  EXPECT_EQ_ARRAY(exp2, res, 4);
}

TEST(math_interp, BilinearCharHalfwayUSamples)
{
  unsigned char res[4];
  unsigned char exp1[4] = {31, 37, 42, 48};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0.5f, 1.0f);
  EXPECT_EQ_ARRAY(exp1, res, 4);
  unsigned char exp2[4] = {243, 242, 224, 223};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0.5f, 0.0f);
  EXPECT_EQ_ARRAY(exp2, res, 4);
}

TEST(math_interp, BilinearCharHalfwayVSamples)
{
  unsigned char res[4];
  unsigned char exp1[4] = {1, 2, 3, 4};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0.0f, 1.5f);
  EXPECT_EQ_ARRAY(exp1, res, 4);
  unsigned char exp2[4] = {127, 128, 129, 130};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 2.0f, 1.5f);
  EXPECT_EQ_ARRAY(exp2, res, 4);
}

TEST(math_interp, BilinearCharSamples)
{
  unsigned char res[4];
  unsigned char exp1[4] = {136, 133, 132, 130};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 1.25f, 0.625f);
  EXPECT_EQ_ARRAY(exp1, res, 4);
  unsigned char exp2[4] = {219, 191, 167, 142};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 1.4f, 0.1f);
  EXPECT_EQ_ARRAY(exp2, res, 4);
}

TEST(math_interp, BilinearCharPartiallyOutsideImage)
{
  unsigned char res[4];
  unsigned char exp1[4] = {1, 1, 2, 2};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, -0.5f, 2.0f);
  EXPECT_EQ_ARRAY(exp1, res, 4);
  unsigned char exp2[4] = {9, 11, 15, 22};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 1.25f, 2.9f);
  EXPECT_EQ_ARRAY(exp2, res, 4);
  unsigned char exp3[4] = {173, 115, 65, 14};
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 2.2f, -0.1f);
  EXPECT_EQ_ARRAY(exp3, res, 4);
}

TEST(math_interp, BilinearCharFullyOutsideImage)
{
  unsigned char res[4];
  unsigned char exp[4] = {0, 0, 0, 0};
  /* Out of range on U */
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, -1.5f, 0);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, -1.1f, 0);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 3, 0);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 5, 0);
  EXPECT_EQ_ARRAY(exp, res, 4);

  /* Out of range on V */
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0, -3.2f);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0, -1.5f);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0, 3.1f);
  EXPECT_EQ_ARRAY(exp, res, 4);
  BLI_bilinear_interpolation_char(image_char[0][0], res, image_width, image_height, 0, 500.0f);
  EXPECT_EQ_ARRAY(exp, res, 4);
}
