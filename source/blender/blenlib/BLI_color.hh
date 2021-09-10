/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <iostream>

#include "BLI_math_color.h"

namespace blender {

/**
 * CPP based color structures.
 *
 * Strongly typed color storage structures with space and alpha association.
 * Will increase readability and visibility of typical mistakes when
 * working with colors.
 *
 * The storage structs can hold 4 channels (r, g, b and a).
 *
 * Usage:
 *
 * Convert a theme byte color to a linearrgb premultiplied.
 * \code{.cc}
 * ColorTheme4b theme_color;
 * ColorSceneLinear4f<eAlpha::Premultiplied> linearrgb_color =
 *     BLI_color_convert_to_scene_linear(theme_color).premultiply_alpha();
 * \endcode
 *
 * The API is structured to make most use of inlining. Most notable are space
 * conversions done via `BLI_color_convert_to*` functions.
 *
 * - Conversions between spaces (theme <=> scene linear) should always be done by
 *   invoking the `BLI_color_convert_to*` methods.
 * - Encoding colors (compressing to store colors inside a less precision storage)
 *   should be done by invoking the `encode` and `decode` methods.
 * - Changing alpha association should be done by invoking `premultiply_alpha` or
 *   `unpremultiply_alpha` methods.
 *
 * # Encoding.
 *
 * Color encoding is used to store colors with less precision as in using `uint8_t` in
 * stead of `float`. This encoding is supported for `eSpace::SceneLinear`.
 * To make this clear to the developer the `eSpace::SceneLinearByteEncoded`
 * space is added.
 *
 * # Precision
 *
 * Colors can be stored using `uint8_t` or `float` colors. The conversion
 * between the two precisions are available as methods. (`to_4b` and
 * `to_4f`).
 *
 * # Alpha conversion
 *
 * Alpha conversion is only supported in SceneLinear space.
 *
 * Extending this file:
 * - This file can be extended with `ColorHex/Hsl/Hsv` for different representations
 *   of rgb based colors. `ColorHsl4f<eSpace::SceneLinear, eAlpha::Premultiplied>`
 * - Add non RGB spaces/storages ColorXyz.
 */

/* Enumeration containing the different alpha modes. */
enum class eAlpha {
  /* Color and alpha are unassociated. */
  Straight,
  /* Color and alpha are associated. */
  Premultiplied,
};
std::ostream &operator<<(std::ostream &stream, const eAlpha &space);

/* Enumeration containing internal spaces. */
enum class eSpace {
  /* Blender theme color space (sRGB). */
  Theme,
  /* Blender internal scene linear color space (maps to SceneReference role in OCIO). */
  SceneLinear,
  /* Blender internal scene linear color space compressed to be stored in 4 uint8_t. */
  SceneLinearByteEncoded,
};
std::ostream &operator<<(std::ostream &stream, const eSpace &space);

/* Template class to store RGBA values with different precision, space and alpha association. */
template<typename ChannelStorageType, eSpace Space, eAlpha Alpha> class ColorRGBA {
 public:
  ChannelStorageType r, g, b, a;
  constexpr ColorRGBA() = default;

  constexpr ColorRGBA(const ChannelStorageType rgba[4])
      : r(rgba[0]), g(rgba[1]), b(rgba[2]), a(rgba[3])
  {
  }

  constexpr ColorRGBA(const ChannelStorageType r,
                      const ChannelStorageType g,
                      const ChannelStorageType b,
                      const ChannelStorageType a)
      : r(r), g(g), b(b), a(a)
  {
  }

  operator ChannelStorageType *()
  {
    return &r;
  }

  operator const ChannelStorageType *() const
  {
    return &r;
  }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const ColorRGBA<ChannelStorageType, Space, Alpha> &c)
  {

    stream << Space << Alpha << "(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return stream;
  }

  friend bool operator==(const ColorRGBA<ChannelStorageType, Space, Alpha> &a,
                         const ColorRGBA<ChannelStorageType, Space, Alpha> &b)
  {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
  }

  friend bool operator!=(const ColorRGBA<ChannelStorageType, Space, Alpha> &a,
                         const ColorRGBA<ChannelStorageType, Space, Alpha> &b)
  {
    return !(a == b);
  }

  uint64_t hash() const
  {
    uint64_t x1 = *reinterpret_cast<const uint32_t *>(&r);
    uint64_t x2 = *reinterpret_cast<const uint32_t *>(&g);
    uint64_t x3 = *reinterpret_cast<const uint32_t *>(&b);
    uint64_t x4 = *reinterpret_cast<const uint32_t *>(&a);
    return (x1 * 1283591) ^ (x2 * 850177) ^ (x3 * 735391) ^ (x4 * 442319);
  }
};

/* Forward declarations of concrete color classes. */
template<eAlpha Alpha> class ColorSceneLinear4f;
template<eAlpha Alpha> class ColorSceneLinearByteEncoded4b;
template<typename ChannelStorageType> class ColorTheme4;

/* Forward declaration of precision conversion methods. */
BLI_INLINE ColorTheme4<float> BLI_color_convert_to_theme4f(const ColorTheme4<uint8_t> &srgb4b);
BLI_INLINE ColorTheme4<uint8_t> BLI_color_convert_to_theme4b(const ColorTheme4<float> &srgb4f);

template<eAlpha Alpha>
class ColorSceneLinear4f final : public ColorRGBA<float, eSpace::SceneLinear, Alpha> {
 public:
  constexpr ColorSceneLinear4f<Alpha>() : ColorRGBA<float, eSpace::SceneLinear, Alpha>()
  {
  }

  constexpr ColorSceneLinear4f<Alpha>(const float *rgba)
      : ColorRGBA<float, eSpace::SceneLinear, Alpha>(rgba)
  {
  }

  constexpr ColorSceneLinear4f<Alpha>(float r, float g, float b, float a)
      : ColorRGBA<float, eSpace::SceneLinear, Alpha>(r, g, b, a)
  {
  }

  /**
   * Convert to its byte encoded counter space.
   */
  ColorSceneLinearByteEncoded4b<Alpha> encode() const
  {
    ColorSceneLinearByteEncoded4b<Alpha> encoded;
    linearrgb_to_srgb_uchar4(encoded, *this);
    return encoded;
  }

  /**
   * Convert color and alpha association to premultiplied alpha.
   *
   * Does nothing when color has already a premultiplied alpha.
   */
  ColorSceneLinear4f<eAlpha::Premultiplied> premultiply_alpha() const
  {
    if constexpr (Alpha == eAlpha::Straight) {
      ColorSceneLinear4f<eAlpha::Premultiplied> premultiplied;
      straight_to_premul_v4_v4(premultiplied, *this);
      return premultiplied;
    }
    else {
      return *this;
    }
  }

  /**
   * Convert color and alpha association to straight alpha.
   *
   * Does nothing when color has straighten alpha.
   */
  ColorSceneLinear4f<eAlpha::Straight> unpremultiply_alpha() const
  {
    if constexpr (Alpha == eAlpha::Premultiplied) {
      ColorSceneLinear4f<eAlpha::Straight> straighten;
      premul_to_straight_v4_v4(straighten, *this);
      return straighten;
    }
    else {
      return *this;
    }
  }
};

template<eAlpha Alpha>
class ColorSceneLinearByteEncoded4b final
    : public ColorRGBA<uint8_t, eSpace::SceneLinearByteEncoded, Alpha> {
 public:
  constexpr ColorSceneLinearByteEncoded4b() = default;

  constexpr ColorSceneLinearByteEncoded4b(const uint8_t *rgba)
      : ColorRGBA<uint8_t, eSpace::SceneLinearByteEncoded, Alpha>(rgba)
  {
  }

  constexpr ColorSceneLinearByteEncoded4b(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
      : ColorRGBA<uint8_t, eSpace::SceneLinearByteEncoded, Alpha>(r, g, b, a)
  {
  }

  /**
   * Convert to back to float color.
   */
  ColorSceneLinear4f<Alpha> decode() const
  {
    ColorSceneLinear4f<Alpha> decoded;
    srgb_to_linearrgb_uchar4(decoded, *this);
    return decoded;
  }
};

/**
 * Theme color template class.
 *
 * Don't use directly, but use `ColorTheme4b/ColorTheme4b`.
 *
 * This has been implemented as a template to improve inlining. When implemented as concrete
 * classes (ColorTheme4b/f) the functions would be hidden in a compile unit what wouldn't be
 * inlined.
 */
template<typename ChannelStorageType>
class ColorTheme4 final : public ColorRGBA<ChannelStorageType, eSpace::Theme, eAlpha::Straight> {
 public:
  constexpr ColorTheme4() : ColorRGBA<ChannelStorageType, eSpace::Theme, eAlpha::Straight>(){};

  constexpr ColorTheme4(const ChannelStorageType *rgba)
      : ColorRGBA<ChannelStorageType, eSpace::Theme, eAlpha::Straight>(rgba)
  {
  }

  constexpr ColorTheme4(ChannelStorageType r,
                        ChannelStorageType g,
                        ChannelStorageType b,
                        ChannelStorageType a)
      : ColorRGBA<ChannelStorageType, eSpace::Theme, eAlpha::Straight>(r, g, b, a)
  {
  }

  /**
   * Change precision of color to float.
   */
  ColorTheme4<float> to_4f() const
  {
    if constexpr ((std::is_same_v<ChannelStorageType, uint8_t>)) {
      return BLI_color_convert_to_theme4f(*this);
    }
    else {
      return *this;
    }
  }

  /**
   * Change precision of color to uint8_t.
   */
  ColorTheme4<uint8_t> to_4b() const
  {
    if constexpr ((std::is_same_v<ChannelStorageType, float>)) {
      return BLI_color_convert_to_theme4b(*this);
    }
    else {
      return *this;
    }
  }
};

using ColorTheme4b = ColorTheme4<uint8_t>;
using ColorTheme4f = ColorTheme4<float>;

BLI_INLINE ColorTheme4b BLI_color_convert_to_theme4b(const ColorTheme4f &theme4f)
{
  ColorTheme4b theme4b;
  rgba_float_to_uchar(theme4b, theme4f);
  return theme4b;
}

BLI_INLINE ColorTheme4f BLI_color_convert_to_theme4f(const ColorTheme4b &theme4b)
{
  ColorTheme4f theme4f;
  rgba_uchar_to_float(theme4f, theme4b);
  return theme4f;
}

BLI_INLINE ColorSceneLinear4f<eAlpha::Straight> BLI_color_convert_to_scene_linear(
    const ColorTheme4f &theme4f)
{
  ColorSceneLinear4f<eAlpha::Straight> scene_linear;
  srgb_to_linearrgb_v4(scene_linear, theme4f);
  return scene_linear;
}

BLI_INLINE ColorSceneLinear4f<eAlpha::Straight> BLI_color_convert_to_scene_linear(
    const ColorTheme4b &theme4b)
{
  ColorSceneLinear4f<eAlpha::Straight> scene_linear;
  srgb_to_linearrgb_uchar4(scene_linear, theme4b);
  return scene_linear;
}

BLI_INLINE ColorTheme4f
BLI_color_convert_to_theme4f(const ColorSceneLinear4f<eAlpha::Straight> &scene_linear)
{
  ColorTheme4f theme4f;
  linearrgb_to_srgb_v4(theme4f, scene_linear);
  return theme4f;
}

BLI_INLINE ColorTheme4b
BLI_color_convert_to_theme4b(const ColorSceneLinear4f<eAlpha::Straight> &scene_linear)
{
  ColorTheme4b theme4b;
  linearrgb_to_srgb_uchar4(theme4b, scene_linear);
  return theme4b;
}

/* Internal roles. For convenience to shorten the type names and hide complexity. */
using ColorGeometry4f = ColorSceneLinear4f<eAlpha::Premultiplied>;
using ColorGeometry4b = ColorSceneLinearByteEncoded4b<eAlpha::Premultiplied>;

}  // namespace blender
