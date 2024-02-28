/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "DNA_vec_types.h"

namespace blender::compositor {

using Size2f = float2;

enum class eDimension { X, Y };

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

constexpr int COM_data_type_bytes_len(DataType data_type)
{
  return COM_data_type_num_channels(data_type) * sizeof(float);
}

constexpr int COM_DATA_TYPE_VALUE_CHANNELS = COM_data_type_num_channels(DataType::Value);
constexpr int COM_DATA_TYPE_VECTOR_CHANNELS = COM_data_type_num_channels(DataType::Vector);
constexpr int COM_DATA_TYPE_COLOR_CHANNELS = COM_data_type_num_channels(DataType::Color);

constexpr float COM_COLOR_TRANSPARENT[4] = {0.0f, 0.0f, 0.0f, 0.0f};
constexpr float COM_VECTOR_ZERO[3] = {0.0f, 0.0f, 0.0f};
constexpr float COM_COLOR_BLACK[4] = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr float COM_VALUE_ZERO[1] = {0.0f};
constexpr float COM_VALUE_ONE[1] = {1.0f};

/**
 * Utility to get data type for given number of channels.
 */
constexpr DataType COM_num_channels_data_type(const int num_channels)
{
  switch (num_channels) {
    case 1:
      return DataType::Value;
    case 3:
      return DataType::Vector;
    case 4:
    default:
      return DataType::Color;
  }
}

constexpr float COM_PREVIEW_SIZE = 140.f;
constexpr float COM_RULE_OF_THIRDS_DIVIDER = 100.0f;
constexpr float COM_BLUR_BOKEH_PIXELS = 512;

constexpr rcti COM_AREA_NONE = {0, 0, 0, 0};
constexpr rcti COM_CONSTANT_INPUT_AREA_OF_INTEREST = COM_AREA_NONE;

}  // namespace blender::compositor
