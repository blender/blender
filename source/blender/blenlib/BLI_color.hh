/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <ostream>

#include "BLI_color_types.hh"
#include "BLI_colorspace.hh"
#include "BLI_compiler_compat.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

namespace blender {

/*
 * Stream output.
 */

std::ostream &operator<<(std::ostream &stream, const eAlpha &space);
std::ostream &operator<<(std::ostream &stream, const eSpace &space);

template<typename ChannelStorageType, eSpace Space, eAlpha Alpha>
std::ostream &operator<<(std::ostream &stream,
                         const ColorRGBA<ChannelStorageType, Space, Alpha> &c);

namespace color {

/**
 * Change precision of color to uint8_t.
 */
BLI_INLINE ColorTheme4b to_byte(const ColorTheme4f &theme4f)
{
  ColorTheme4b theme4b;
  rgba_float_to_uchar(theme4b, theme4f);
  return theme4b;
}

BLI_INLINE ColorTheme4b to_byte(const ColorTheme4b &theme4b)
{
  return theme4b;
}

template<eAlpha Alpha>
BLI_INLINE ColorSceneLinearByteEncoded4b<Alpha> encode(const ColorSceneLinear4f<Alpha> &color)
{
  float4 value = static_cast<const float *>(color);
  if (!colorspace::scene_linear_is_rec709) {
    copy_v3_v3(value, colorspace::scene_linear_to_rec709 * value.xyz());
  }
  ColorSceneLinearByteEncoded4b<Alpha> encoded;
  linearrgb_to_srgb_uchar4(encoded, value);
  return encoded;
}

/**
 * Change precision of color to float.
 */
BLI_INLINE ColorTheme4f to_float(const ColorTheme4b &theme4b)
{
  ColorTheme4f theme4f;
  rgba_uchar_to_float(theme4f, theme4b);
  return theme4f;
}

BLI_INLINE ColorTheme4f to_float(const ColorTheme4f &theme4f)
{
  return theme4f;
}

template<eAlpha Alpha>
BLI_INLINE ColorSceneLinear4f<Alpha> decode(const ColorSceneLinearByteEncoded4b<Alpha> &color)
{
  ColorSceneLinear4f<Alpha> decoded;
  srgb_to_linearrgb_uchar4(decoded, color);
  if (!blender::colorspace::scene_linear_is_rec709) {
    copy_v3_v3(decoded, blender::colorspace::rec709_to_scene_linear * blender::float3(decoded));
  }
  return decoded;
}

/**
 * Convert color and alpha association to premultiplied alpha.
 *
 * Does nothing when color already has a premultiplied alpha.
 */
template<eAlpha Alpha>
ColorSceneLinear4f<eAlpha::Premultiplied> premultiply_alpha(const ColorSceneLinear4f<Alpha> &color)
{
  if constexpr (Alpha == eAlpha::Straight) {
    ColorSceneLinear4f<eAlpha::Premultiplied> premultiplied;
    straight_to_premul_v4_v4(premultiplied, color);
    return premultiplied;
  }
  else {
    return color;
  }
}

/**
 * Convert color and alpha association to straight alpha.
 *
 * Does nothing when color has straight alpha.
 */
template<eAlpha Alpha>
ColorSceneLinear4f<eAlpha::Straight> unpremultiply_alpha(const ColorSceneLinear4f<Alpha> &color)
{
  if constexpr (Alpha == eAlpha::Premultiplied) {
    ColorSceneLinear4f<eAlpha::Straight> straighten;
    premul_to_straight_v4_v4(straighten, color);
    return straighten;
  }
  else {
    return color;
  }
}

/**
 * Convert between theme and scene linear colors.
 */
BLI_INLINE ColorSceneLinear4f<eAlpha::Straight> to_scene_linear(const ColorTheme4f &theme4f)
{
  ColorSceneLinear4f<eAlpha::Straight> scene_linear;
  srgb_to_linearrgb_v4(scene_linear, theme4f);
  return scene_linear;
}

BLI_INLINE ColorSceneLinear4f<eAlpha::Straight> to_scene_linear(const ColorTheme4b &theme4b)
{
  ColorSceneLinear4f<eAlpha::Straight> scene_linear;
  srgb_to_linearrgb_uchar4(scene_linear, theme4b);
  return scene_linear;
}

BLI_INLINE ColorTheme4f to_theme4f(const ColorSceneLinear4f<eAlpha::Straight> &scene_linear)
{
  ColorTheme4f theme4f;
  linearrgb_to_srgb_v4(theme4f, scene_linear);
  return theme4f;
}

BLI_INLINE ColorTheme4b to_theme4b(const ColorSceneLinear4f<eAlpha::Straight> &scene_linear)
{
  ColorTheme4b theme4b;
  linearrgb_to_srgb_uchar4(theme4b, scene_linear);
  return theme4b;
}

}  // namespace color

}  // namespace blender
