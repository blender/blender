/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <cstdint>
#include <type_traits>

#include "BLI_struct_equality_utils.hh"
#include "BLI_utildefines.h"

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

/** Enumeration containing the different alpha modes. */
enum class eAlpha {
  /** Color and alpha are unassociated. */
  Straight,
  /** Color and alpha are associated. */
  Premultiplied,
};

/** Enumeration containing internal spaces. */
enum class eSpace {
  /** Blender theme color space (sRGB). */
  Theme,
  /** Blender internal scene linear color space (maps to scene_linear role in OCIO). */
  SceneLinear,
  /** Blender internal scene linear color space compressed to be stored in 4 uint8_t. */
  SceneLinearByteEncoded,
};

/** Template class to store RGBA values with different precision, space, and alpha association. */
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

  BLI_STRUCT_EQUALITY_OPERATORS_4(ColorRGBA, r, g, b, a)

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

template<eAlpha Alpha>
class ColorSceneLinear4f final : public ColorRGBA<float, eSpace::SceneLinear, Alpha> {
 public:
  constexpr ColorSceneLinear4f() = default;

  constexpr explicit ColorSceneLinear4f(float value)
      : ColorRGBA<float, eSpace::SceneLinear, Alpha>(value, value, value, value)
  {
  }

  template<typename U, BLI_ENABLE_IF((std::is_convertible_v<U, float>))>
  constexpr explicit ColorSceneLinear4f(U value) : ColorSceneLinear4f(float(value))
  {
  }

  constexpr ColorSceneLinear4f(const float *rgba)
      : ColorRGBA<float, eSpace::SceneLinear, Alpha>(rgba)
  {
  }

  constexpr ColorSceneLinear4f(float r, float g, float b, float a)
      : ColorRGBA<float, eSpace::SceneLinear, Alpha>(r, g, b, a)
  {
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
  constexpr ColorTheme4() = default;

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
};

using ColorTheme4b = ColorTheme4<uint8_t>;
using ColorTheme4f = ColorTheme4<float>;

/* Internal roles. For convenience to shorten the type names and hide complexity. */

using ColorGeometry4f = ColorSceneLinear4f<eAlpha::Premultiplied>;
using ColorGeometry4b = ColorSceneLinearByteEncoded4b<eAlpha::Premultiplied>;
using ColorPaint4f = ColorSceneLinear4f<eAlpha::Straight>;
using ColorPaint4b = ColorSceneLinearByteEncoded4b<eAlpha::Straight>;

}  // namespace blender
