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

#include <optional>

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"

struct Object;
struct Collection;

namespace blender::ed::spreadsheet {

struct ObjectCellValue {
  const Object *object;
};

struct CollectionCellValue {
  const Collection *collection;
};

/**
 * This is a type that can hold the value of a cell in a spreadsheet. This type allows us to
 * decouple the drawing of individual cells from the code that generates the data to be displayed.
 */
class CellValue {
 public:
  /* The implementation just uses a bunch of `std::option` for now. Unfortunately, we cannot use
   * `std::variant` yet, due to missing compiler support. This type can really be optimized more,
   * but it does not really matter too much currently. */

  std::optional<int> value_int;
  std::optional<float> value_float;
  std::optional<bool> value_bool;
  std::optional<float2> value_float2;
  std::optional<float3> value_float3;
  std::optional<Color4f> value_color;
  std::optional<ObjectCellValue> value_object;
  std::optional<CollectionCellValue> value_collection;
};

}  // namespace blender::ed::spreadsheet
