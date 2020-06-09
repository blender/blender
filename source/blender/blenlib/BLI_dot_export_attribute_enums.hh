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

#ifndef __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_HH__
#define __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_HH__

#include "BLI_string_ref.hh"

namespace blender {
namespace DotExport {

enum class Attr_rankdir {
  LeftToRight,
  TopToBottom,
};

inline StringRef rankdir_to_string(Attr_rankdir value)
{
  switch (value) {
    case Attr_rankdir::LeftToRight:
      return "LR";
    case Attr_rankdir::TopToBottom:
      return "TB";
  }
  return "";
}

enum class Attr_shape {
  Rectangle,
  Ellipse,
  Circle,
  Point,
  Diamond,
  Square,
};

inline StringRef shape_to_string(Attr_shape value)
{
  switch (value) {
    case Attr_shape::Rectangle:
      return "rectangle";
    case Attr_shape::Ellipse:
      return "ellipse";
    case Attr_shape::Circle:
      return "circle";
    case Attr_shape::Point:
      return "point";
    case Attr_shape::Diamond:
      return "diamond";
    case Attr_shape::Square:
      return "square";
  }
  return "";
}

enum class Attr_arrowType {
  Normal,
  Inv,
  Dot,
  None,
  Empty,
  Box,
  Vee,
};

inline StringRef arrowType_to_string(Attr_arrowType value)
{
  switch (value) {
    case Attr_arrowType::Normal:
      return "normal";
    case Attr_arrowType::Inv:
      return "inv";
    case Attr_arrowType::Dot:
      return "dot";
    case Attr_arrowType::None:
      return "none";
    case Attr_arrowType::Empty:
      return "empty";
    case Attr_arrowType::Box:
      return "box";
    case Attr_arrowType::Vee:
      return "vee";
  }
  return "";
}

enum class Attr_dirType {
  Forward,
  Back,
  Both,
  None,
};

inline StringRef dirType_to_string(Attr_dirType value)
{
  switch (value) {
    case Attr_dirType::Forward:
      return "forward";
    case Attr_dirType::Back:
      return "back";
    case Attr_dirType::Both:
      return "both";
    case Attr_dirType::None:
      return "none";
  }
  return "";
}

}  // namespace DotExport
}  // namespace blender

#endif /* __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_HH__ */
