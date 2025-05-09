/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstddef>

#include "BLI_assert.h"

#if defined(WITH_OPENCOLORIO)
#  include "intern/opencolorio.hh"
#endif

namespace blender::ocio {

enum class BitDepth {
  BIT_DEPTH_UNKNOWN,
  BIT_DEPTH_F32,
};

class PackedImage {
#if defined(WITH_OPENCOLORIO)
  OCIO_NAMESPACE::PackedImageDesc image_desc_;

  static OCIO_NAMESPACE::BitDepth convert_bit_depth(const BitDepth bit_depth)
  {
    switch (bit_depth) {
      case BitDepth::BIT_DEPTH_UNKNOWN:
        return OCIO_NAMESPACE::BIT_DEPTH_UNKNOWN;
      case BitDepth::BIT_DEPTH_F32:
        return OCIO_NAMESPACE::BIT_DEPTH_F32;
    }
    BLI_assert_unreachable();
    return OCIO_NAMESPACE::BIT_DEPTH_UNKNOWN;
  }

  static BitDepth convert_bit_depth(const OCIO_NAMESPACE::BitDepth bit_depth)
  {
    switch (bit_depth) {
      case OCIO_NAMESPACE::BIT_DEPTH_UNKNOWN:
        return BitDepth::BIT_DEPTH_UNKNOWN;
      case OCIO_NAMESPACE::BIT_DEPTH_F32:
        return BitDepth::BIT_DEPTH_F32;
      default:
        /* Other bit depths are currently not supported. */
        return BitDepth::BIT_DEPTH_UNKNOWN;
    }
  }

#else
  void *data_ = nullptr;
  size_t width_ = 0;
  size_t height_ = 0;
  size_t num_channels_ = 0;
  BitDepth bit_depth_ = BitDepth::BIT_DEPTH_UNKNOWN;
  size_t chan_stride_in_bytes_ = 0;
  size_t x_stride_in_bytes_ = 0;
  size_t y_stride_in_bytes_ = 0;
#endif

 public:
  PackedImage(void *data,
              const size_t width,
              const size_t height,
              const size_t num_channels,
              const BitDepth bit_depth,
              const size_t chan_stride_in_bytes,
              const size_t x_stride_in_bytes,
              const size_t y_stride_in_bytes)
#if defined(WITH_OPENCOLORIO)
      : image_desc_(data,
                    width,
                    height,
                    num_channels,
                    convert_bit_depth(bit_depth),
                    chan_stride_in_bytes,
                    x_stride_in_bytes,
                    y_stride_in_bytes)
#else
      : data_(data),
        width_(width),
        height_(height),
        num_channels_(num_channels),
        bit_depth_(bit_depth),
        chan_stride_in_bytes_(chan_stride_in_bytes),
        x_stride_in_bytes_(x_stride_in_bytes),
        y_stride_in_bytes_(y_stride_in_bytes)
#endif
  {
  }

#if defined(WITH_OPENCOLORIO)
  size_t get_width() const
  {
    return image_desc_.getWidth();
  }
  size_t get_height() const
  {
    return image_desc_.getHeight();
  }

  size_t get_num_channels() const
  {
    return image_desc_.getNumChannels();
  }

  void *get_data() const
  {
    return image_desc_.getData();
  }

  BitDepth get_bit_depth() const
  {
    return convert_bit_depth(image_desc_.getBitDepth());
  }

  size_t get_chan_stride_in_bytes() const
  {
    return image_desc_.getChanStrideBytes();
  }
  size_t get_x_stride_in_bytes() const
  {
    return image_desc_.getXStrideBytes();
  }
  size_t get_y_stride_in_bytes() const
  {
    return image_desc_.getYStrideBytes();
  }

  operator const OCIO_NAMESPACE::PackedImageDesc &() const
  {
    return image_desc_;
  }
#else
  size_t get_width() const
  {
    return width_;
  }
  size_t get_height() const
  {
    return height_;
  }

  size_t get_num_channels() const
  {
    return num_channels_;
  }

  void *get_data() const
  {
    return data_;
  }

  BitDepth get_bit_depth() const
  {
    return bit_depth_;
  }

  size_t get_chan_stride_in_bytes() const
  {
    return chan_stride_in_bytes_;
  }
  size_t get_x_stride_in_bytes() const
  {
    return x_stride_in_bytes_;
  }
  size_t get_y_stride_in_bytes() const
  {
    return y_stride_in_bytes_;
  }
#endif
};

}  // namespace blender::ocio
