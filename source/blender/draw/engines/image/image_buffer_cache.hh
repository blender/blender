/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_vector.hh"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::draw::image_engine {

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

/**
 * \brief Float buffer cache for image buffers.
 *
 * Image buffers might not have float buffers which are required for the image engine.
 * Image buffers are not allowed to have both a float buffer and a byte buffer as some
 * functionality doesn't know what to do.
 *
 * For this reason we store the float buffer in separate image buffers. The FloatBufferCache keep
 * track of the cached buffers and if they are still used.
 */
struct FloatBufferCache {
 private:
  Vector<FloatImageBuffer> cache_;

 public:
  ImBuf *cached_float_buffer(ImBuf *image_buffer)
  {
    /* Check if we can use the float buffer of the given image_buffer. */
    if (image_buffer->float_buffer.data != nullptr) {
      BLI_assert_msg(
          IMB_colormanagement_space_name_is_scene_linear(
              IMB_colormanagement_get_float_colorspace(image_buffer)),
          "Expected float buffer to be scene_linear - if there are code paths where this "
          "isn't the case we should convert those and add to the FloatBufferCache as well.");
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

    IMB_assign_float_buffer(new_imbuf, IMB_steal_float_buffer(image_buffer), IB_TAKE_OWNERSHIP);

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

}  // namespace blender::draw::image_engine
