/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"

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

}  // namespace blender
