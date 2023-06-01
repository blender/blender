/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

namespace blender ::dot {

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

}  // namespace blender::dot
