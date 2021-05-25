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
