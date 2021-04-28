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
 *
 * Copyright 2011, Blender Foundation.
 */

#pragma once

namespace blender::compositor {

/**
 * \brief possible data types for sockets
 * \ingroup Model
 */
enum class DataType {
  /** \brief Value data type */
  Value = 0,
  /** \brief Vector data type */
  Vector = 1,
  /** \brief Color data type */
  Color = 2,
};

/**
 * Utility to get the number of channels of the given data type.
 */
constexpr int COM_data_type_num_channels(const DataType datatype)
{
  switch (datatype) {
    case DataType::Value:
      return 1;
    case DataType::Vector:
      return 3;
    case DataType::Color:
    default:
      return 4;
  }
}

constexpr int COM_DATA_TYPE_VALUE_CHANNELS = COM_data_type_num_channels(DataType::Value);
constexpr int COM_DATA_TYPE_COLOR_CHANNELS = COM_data_type_num_channels(DataType::Color);

// configurable items

// chunk size determination

// chunk order
/**
 * \brief The order of chunks to be scheduled
 * \ingroup Execution
 */
enum class ChunkOrdering {
  /** \brief order from a distance to centerX/centerY */
  CenterOut = 0,
  /** \brief order randomly */
  Random = 1,
  /** \brief no ordering */
  TopDown = 2,
  /** \brief experimental ordering with 9 hot-spots. */
  RuleOfThirds = 3,

  Default = ChunkOrdering::CenterOut,
};

constexpr float COM_PREVIEW_SIZE = 140.f;
constexpr float COM_RULE_OF_THIRDS_DIVIDER = 100.0f;
constexpr float COM_BLUR_BOKEH_PIXELS = 512;

}  // namespace blender::compositor
