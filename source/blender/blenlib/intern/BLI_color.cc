/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_color.hh"

#include <ostream>

namespace blender {

std::ostream &operator<<(std::ostream &stream, const eAlpha &space)
{
  switch (space) {
    case eAlpha::Straight: {
      stream << "Straight";
      break;
    }
    case eAlpha::Premultiplied: {
      stream << "Premultiplied";
      break;
    }
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const eSpace &space)
{
  switch (space) {
    case eSpace::Theme: {
      stream << "Theme";
      break;
    }
    case eSpace::SceneLinear: {
      stream << "SceneLinear";
      break;
    }
    case eSpace::SceneLinearByteEncoded: {
      stream << "SceneLinearByteEncoded";
      break;
    }
  }
  return stream;
}

template<typename ChannelStorageType, eSpace Space, eAlpha Alpha>
std::ostream &operator<<(std::ostream &stream,
                         const ColorRGBA<ChannelStorageType, Space, Alpha> &c)
{
  stream << Space << Alpha << "(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
  return stream;
}

template std::ostream &operator<<(
    std::ostream &stream, const ColorRGBA<float, eSpace::SceneLinear, eAlpha::Premultiplied> &c);
template std::ostream &operator<<(
    std::ostream &stream, const ColorRGBA<float, eSpace::SceneLinear, eAlpha::Straight> &c);
template std::ostream &operator<<(std::ostream &stream,
                                  const ColorRGBA<float, eSpace::Theme, eAlpha::Straight> &c);
template std::ostream &operator<<(
    std::ostream &stream,
    const ColorRGBA<uint8_t, eSpace::SceneLinearByteEncoded, eAlpha::Premultiplied> &c);
template std::ostream &operator<<(
    std::ostream &stream,
    const ColorRGBA<uint8_t, eSpace::SceneLinearByteEncoded, eAlpha::Straight> &c);
template std::ostream &operator<<(std::ostream &stream,
                                  const ColorRGBA<uint8_t, eSpace::Theme, eAlpha::Straight> &c);

}  // namespace blender
