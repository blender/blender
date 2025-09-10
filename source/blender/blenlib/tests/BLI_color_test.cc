/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_color.hh"

namespace blender::tests {

/* -------------------------------------------------------------------- */
/** \name Conversions
 * \{ */

TEST(color, ThemeByteToFloat)
{
  ColorTheme4b theme_byte(192, 128, 64, 128);
  ColorTheme4f theme_float = color::to_float(theme_byte);
  EXPECT_NEAR(0.75f, theme_float.r, 0.01f);
  EXPECT_NEAR(0.5f, theme_float.g, 0.01f);
  EXPECT_NEAR(0.25f, theme_float.b, 0.01f);
  EXPECT_NEAR(0.5f, theme_float.a, 0.01f);
}

TEST(color, SrgbStraightFloatToByte)
{
  ColorTheme4f theme_float(0.75f, 0.5f, 0.25f, 0.5f);
  ColorTheme4b theme_byte = color::to_byte(theme_float);
  EXPECT_EQ(191, theme_byte.r);
  EXPECT_EQ(128, theme_byte.g);
  EXPECT_EQ(64, theme_byte.b);
  EXPECT_EQ(128, theme_byte.a);
}

TEST(color, SrgbStraightToSceneLinearPremultiplied)
{
  BLI_init_srgb_conversion();

  ColorTheme4b theme(192, 128, 64, 128);
  ColorSceneLinear4f<eAlpha::Premultiplied> linear = color::premultiply_alpha(
      color::to_scene_linear(theme));
  EXPECT_NEAR(0.26f, linear.r, 0.01f);
  EXPECT_NEAR(0.11f, linear.g, 0.01f);
  EXPECT_NEAR(0.02f, linear.b, 0.01f);
  EXPECT_NEAR(0.5f, linear.a, 0.01f);
}

TEST(color, SceneLinearStraightToPremultiplied)
{
  ColorSceneLinear4f<eAlpha::Straight> straight(0.75f, 0.5f, 0.25f, 0.5f);
  ColorSceneLinear4f<eAlpha::Premultiplied> premultiplied = color::premultiply_alpha(straight);
  EXPECT_NEAR(0.37f, premultiplied.r, 0.01f);
  EXPECT_NEAR(0.25f, premultiplied.g, 0.01f);
  EXPECT_NEAR(0.12f, premultiplied.b, 0.01f);
  EXPECT_NEAR(0.5f, premultiplied.a, 0.01f);
}

TEST(color, SceneLinearPremultipliedToStraight)
{
  ColorSceneLinear4f<eAlpha::Premultiplied> premultiplied(0.75f, 0.5f, 0.25f, 0.5f);
  ColorSceneLinear4f<eAlpha::Straight> straight = color::unpremultiply_alpha(premultiplied);
  EXPECT_NEAR(1.5f, straight.r, 0.01f);
  EXPECT_NEAR(1.0f, straight.g, 0.01f);
  EXPECT_NEAR(0.5f, straight.b, 0.01f);
  EXPECT_NEAR(0.5f, straight.a, 0.01f);
}

TEST(color, SceneLinearStraightSrgbFloat)
{
  BLI_init_srgb_conversion();
  ColorSceneLinear4f<eAlpha::Straight> linear(0.75f, 0.5f, 0.25f, 0.5f);
  ColorTheme4f theme = color::to_theme4f(linear);
  EXPECT_NEAR(0.88f, theme.r, 0.01);
  EXPECT_NEAR(0.73f, theme.g, 0.01);
  EXPECT_NEAR(0.53f, theme.b, 0.01);
  EXPECT_NEAR(0.5f, theme.a, 0.01);
}

TEST(color, SceneLinearPremultipliedToSrgbFloat)
{
  BLI_init_srgb_conversion();
  ColorSceneLinear4f<eAlpha::Premultiplied> linear(0.75f, 0.5f, 0.25f, 0.5f);
  ColorTheme4f theme = color::to_theme4f(color::unpremultiply_alpha(linear));

  EXPECT_NEAR(1.19f, theme.r, 0.01);
  EXPECT_NEAR(1.0f, theme.g, 0.01);
  EXPECT_NEAR(0.74f, theme.b, 0.01);
  EXPECT_NEAR(0.5f, theme.a, 0.01);
}

TEST(color, SceneLinearStraightSrgbByte)
{
  BLI_init_srgb_conversion();
  ColorSceneLinear4f<eAlpha::Straight> linear(0.75f, 0.5f, 0.25f, 0.5f);
  ColorTheme4b theme = color::to_theme4b(linear);
  EXPECT_EQ(225, theme.r);
  EXPECT_EQ(188, theme.g);
  EXPECT_EQ(137, theme.b);
  EXPECT_EQ(128, theme.a);
}

TEST(color, SceneLinearPremultipliedToSrgbByte)
{
  BLI_init_srgb_conversion();
  ColorSceneLinear4f<eAlpha::Premultiplied> linear(0.75f, 0.5f, 0.25f, 0.5f);
  ColorTheme4b theme = color::to_theme4b(color::unpremultiply_alpha(linear));
  EXPECT_EQ(255, theme.r);
  EXPECT_EQ(255, theme.g);
  EXPECT_EQ(188, theme.b);
  EXPECT_EQ(128, theme.a);
}

TEST(color, SceneLinearByteEncoding)
{
  ColorSceneLinear4f<eAlpha::Premultiplied> linear(0.75f, 0.5f, 0.25f, 0.5f);
  ColorSceneLinearByteEncoded4b<eAlpha::Premultiplied> encoded = color::encode(linear);
  EXPECT_EQ(225, encoded.r);
  EXPECT_EQ(188, encoded.g);
  EXPECT_EQ(137, encoded.b);
  EXPECT_EQ(128, encoded.a);
}

TEST(color, SceneLinearByteDecoding)
{
  ColorSceneLinearByteEncoded4b<eAlpha::Premultiplied> encoded(225, 188, 137, 128);
  ColorSceneLinear4f<eAlpha::Premultiplied> decoded = color::decode(encoded);
  EXPECT_NEAR(0.75f, decoded.r, 0.01f);
  EXPECT_NEAR(0.5f, decoded.g, 0.01f);
  EXPECT_NEAR(0.25f, decoded.b, 0.01f);
  EXPECT_NEAR(0.5f, decoded.a, 0.01f);
}

/** \} */

}  // namespace blender::tests
