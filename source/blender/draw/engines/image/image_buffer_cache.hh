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
 * Copyright 2022, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_vector.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

struct FloatImageBuffer {
  ImBuf *source_buffer = nullptr;
  ImBuf *float_buffer = nullptr;
  bool is_used = true;

  FloatImageBuffer(ImBuf *source_buffer, ImBuf *float_buffer)
      : source_buffer(source_buffer), float_buffer(float_buffer)
  {
  }

  FloatImageBuffer(FloatImageBuffer &&other) noexcept
  {
    source_buffer = other.source_buffer;
    float_buffer = other.float_buffer;
    is_used = other.is_used;
    other.source_buffer = nullptr;
    other.float_buffer = nullptr;
  }

  virtual ~FloatImageBuffer()
  {
    IMB_freeImBuf(float_buffer);
    float_buffer = nullptr;
    source_buffer = nullptr;
  }

  FloatImageBuffer &operator=(FloatImageBuffer &&other) noexcept
  {
    this->source_buffer = other.source_buffer;
    this->float_buffer = other.float_buffer;
    is_used = other.is_used;
    other.source_buffer = nullptr;
    other.float_buffer = nullptr;
    return *this;
  }
};

struct FloatBufferCache {
 private:
  blender::Vector<FloatImageBuffer> cache_;

 public:
  ImBuf *ensure_float_buffer(ImBuf *image_buffer)
  {
    /* Check if we can use the float buffer of the given image_buffer. */
    if (image_buffer->rect_float != nullptr) {
      return image_buffer;
    }

    /* Do we have a cached float buffer. */
    for (FloatImageBuffer &item : cache_) {
      if (item.source_buffer == image_buffer) {
        item.is_used = true;
        return item.float_buffer;
      }
    }

    /* Generate a new float buffer. */
    IMB_float_from_rect(image_buffer);
    ImBuf *new_imbuf = IMB_allocImBuf(image_buffer->x, image_buffer->y, image_buffer->planes, 0);
    new_imbuf->rect_float = image_buffer->rect_float;
    new_imbuf->flags |= IB_rectfloat;
    new_imbuf->mall |= IB_rectfloat;
    image_buffer->rect_float = nullptr;
    image_buffer->flags &= ~IB_rectfloat;
    image_buffer->mall &= ~IB_rectfloat;

    cache_.append(FloatImageBuffer(image_buffer, new_imbuf));
    return new_imbuf;
  }

  void reset_usage_flags()
  {
    for (FloatImageBuffer &buffer : cache_) {
      buffer.is_used = false;
    }
  }

  void mark_used(const ImBuf *image_buffer)
  {
    for (FloatImageBuffer &item : cache_) {
      if (item.source_buffer == image_buffer) {
        item.is_used = true;
        return;
      }
    }
  }

  void remove_unused_buffers()
  {
    for (int64_t i = cache_.size() - 1; i >= 0; i--) {
      if (!cache_[i].is_used) {
        cache_.remove_and_reorder(i);
      }
    }
  }

  void clear()
  {
    cache_.clear();
  }
};
