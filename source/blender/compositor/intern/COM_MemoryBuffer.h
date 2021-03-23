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

#include "COM_ExecutionGroup.h"
#include "COM_MemoryProxy.h"

#include "BLI_math.h"
#include "BLI_rect.h"

namespace blender::compositor {

/**
 * \brief state of a memory buffer
 * \ingroup Memory
 */
enum class MemoryBufferState {
  /** \brief memory has been allocated on creator device and CPU machine,
   * but kernel has not been executed */
  Default = 0,
  /** \brief chunk is consolidated from other chunks. special state.*/
  Temporary = 6,
};

enum class MemoryBufferExtend {
  Clip,
  Extend,
  Repeat,
};

class MemoryProxy;

/**
 * \brief a MemoryBuffer contains access to the data of a chunk
 */
class MemoryBuffer {
 private:
  /**
   * \brief proxy of the memory (same for all chunks in the same buffer)
   */
  MemoryProxy *m_memoryProxy;

  /**
   * \brief the type of buffer DataType::Value, DataType::Vector, DataType::Color
   */
  DataType m_datatype;

  /**
   * \brief region of this buffer inside relative to the MemoryProxy
   */
  rcti m_rect;

  /**
   * \brief state of the buffer
   */
  MemoryBufferState m_state;

  /**
   * \brief the actual float buffer/data
   */
  float *m_buffer;

  /**
   * \brief the number of channels of a single value in the buffer.
   * For value buffers this is 1, vector 3 and color 4
   */
  uint8_t m_num_channels;

 public:
  /**
   * \brief construct new temporarily MemoryBuffer for an area
   */
  MemoryBuffer(MemoryProxy *memoryProxy, const rcti &rect, MemoryBufferState state);

  /**
   * \brief construct new temporarily MemoryBuffer for an area
   */
  MemoryBuffer(DataType datatype, const rcti &rect);

  /**
   * Copy constructor
   */
  MemoryBuffer(const MemoryBuffer &src);

  /**
   * \brief destructor
   */
  ~MemoryBuffer();

  uint8_t get_num_channels()
  {
    return this->m_num_channels;
  }

  /**
   * \brief get the data of this MemoryBuffer
   * \note buffer should already be available in memory
   */
  float *getBuffer()
  {
    return this->m_buffer;
  }

  inline void wrap_pixel(int &x, int &y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
  {
    const int w = getWidth();
    const int h = getHeight();
    x = x - m_rect.xmin;
    y = y - m_rect.ymin;

    switch (extend_x) {
      case MemoryBufferExtend::Clip:
        break;
      case MemoryBufferExtend::Extend:
        if (x < 0) {
          x = 0;
        }
        if (x >= w) {
          x = w;
        }
        break;
      case MemoryBufferExtend::Repeat:
        x = (x >= 0.0f ? (x % w) : (x % w) + w);
        break;
    }

    switch (extend_y) {
      case MemoryBufferExtend::Clip:
        break;
      case MemoryBufferExtend::Extend:
        if (y < 0) {
          y = 0;
        }
        if (y >= h) {
          y = h;
        }
        break;
      case MemoryBufferExtend::Repeat:
        y = (y >= 0.0f ? (y % h) : (y % h) + h);
        break;
    }
  }

  inline void wrap_pixel(float &x,
                         float &y,
                         MemoryBufferExtend extend_x,
                         MemoryBufferExtend extend_y)
  {
    const float w = (float)getWidth();
    const float h = (float)getHeight();
    x = x - m_rect.xmin;
    y = y - m_rect.ymin;

    switch (extend_x) {
      case MemoryBufferExtend::Clip:
        break;
      case MemoryBufferExtend::Extend:
        if (x < 0) {
          x = 0.0f;
        }
        if (x >= w) {
          x = w;
        }
        break;
      case MemoryBufferExtend::Repeat:
        x = fmodf(x, w);
        break;
    }

    switch (extend_y) {
      case MemoryBufferExtend::Clip:
        break;
      case MemoryBufferExtend::Extend:
        if (y < 0) {
          y = 0.0f;
        }
        if (y >= h) {
          y = h;
        }
        break;
      case MemoryBufferExtend::Repeat:
        y = fmodf(y, h);
        break;
    }
  }

  inline void read(float *result,
                   int x,
                   int y,
                   MemoryBufferExtend extend_x = MemoryBufferExtend::Clip,
                   MemoryBufferExtend extend_y = MemoryBufferExtend::Clip)
  {
    bool clip_x = (extend_x == MemoryBufferExtend::Clip && (x < m_rect.xmin || x >= m_rect.xmax));
    bool clip_y = (extend_y == MemoryBufferExtend::Clip && (y < m_rect.ymin || y >= m_rect.ymax));
    if (clip_x || clip_y) {
      /* clip result outside rect is zero */
      memset(result, 0, this->m_num_channels * sizeof(float));
    }
    else {
      int u = x;
      int v = y;
      this->wrap_pixel(u, v, extend_x, extend_y);
      const int offset = (getWidth() * y + x) * this->m_num_channels;
      float *buffer = &this->m_buffer[offset];
      memcpy(result, buffer, sizeof(float) * this->m_num_channels);
    }
  }

  inline void readNoCheck(float *result,
                          int x,
                          int y,
                          MemoryBufferExtend extend_x = MemoryBufferExtend::Clip,
                          MemoryBufferExtend extend_y = MemoryBufferExtend::Clip)
  {
    int u = x;
    int v = y;

    this->wrap_pixel(u, v, extend_x, extend_y);
    const int offset = (getWidth() * v + u) * this->m_num_channels;

    BLI_assert(offset >= 0);
    BLI_assert(offset < this->buffer_len() * this->m_num_channels);
    BLI_assert(!(extend_x == MemoryBufferExtend::Clip && (u < m_rect.xmin || u >= m_rect.xmax)) &&
               !(extend_y == MemoryBufferExtend::Clip && (v < m_rect.ymin || v >= m_rect.ymax)));
    float *buffer = &this->m_buffer[offset];
    memcpy(result, buffer, sizeof(float) * this->m_num_channels);
  }

  void writePixel(int x, int y, const float color[4]);
  void addPixel(int x, int y, const float color[4]);
  inline void readBilinear(float *result,
                           float x,
                           float y,
                           MemoryBufferExtend extend_x = MemoryBufferExtend::Clip,
                           MemoryBufferExtend extend_y = MemoryBufferExtend::Clip)
  {
    float u = x;
    float v = y;
    this->wrap_pixel(u, v, extend_x, extend_y);
    if ((extend_x != MemoryBufferExtend::Repeat && (u < 0.0f || u >= getWidth())) ||
        (extend_y != MemoryBufferExtend::Repeat && (v < 0.0f || v >= getHeight()))) {
      copy_vn_fl(result, this->m_num_channels, 0.0f);
      return;
    }
    BLI_bilinear_interpolation_wrap_fl(this->m_buffer,
                                       result,
                                       getWidth(),
                                       getHeight(),
                                       this->m_num_channels,
                                       u,
                                       v,
                                       extend_x == MemoryBufferExtend::Repeat,
                                       extend_y == MemoryBufferExtend::Repeat);
  }

  void readEWA(float *result, const float uv[2], const float derivatives[2][2]);

  /**
   * \brief is this MemoryBuffer a temporarily buffer (based on an area, not on a chunk)
   */
  inline bool isTemporarily() const
  {
    return this->m_state == MemoryBufferState::Temporary;
  }

  /**
   * \brief add the content from otherBuffer to this MemoryBuffer
   * \param otherBuffer: source buffer
   *
   * \note take care when running this on a new buffer since it wont fill in
   *       uninitialized values in areas where the buffers don't overlap.
   */
  void fill_from(const MemoryBuffer &src);

  /**
   * \brief get the rect of this MemoryBuffer
   */
  const rcti &get_rect() const
  {
    return this->m_rect;
  }

  /**
   * \brief get the width of this MemoryBuffer
   */
  const int getWidth() const
  {
    return BLI_rcti_size_x(&m_rect);
  }

  /**
   * \brief get the height of this MemoryBuffer
   */
  const int getHeight() const
  {
    return BLI_rcti_size_y(&m_rect);
  }

  /**
   * \brief clear the buffer. Make all pixels black transparent.
   */
  void clear();

  float get_max_value() const;
  float get_max_value(const rcti &rect) const;

 private:
  const int buffer_len() const
  {
    return getWidth() * getHeight();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBuffer")
#endif
};

}  // namespace blender::compositor
