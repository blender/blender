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
 * Copyright 2021, Blender Foundation.
 */

#include "testing/testing.h"

#include "COM_BufferRange.h"

namespace blender::compositor::tests {

TEST(BufferRange, Constructor)
{
  const int size = 5;
  BufferRange<float> range(nullptr, 1, size, 4);
  EXPECT_EQ(range.size(), size);
}

static void fill_buffer_with_indexes(float *buf, int buf_len)
{
  for (int i = 0; i < buf_len; i++) {
    buf[i] = i;
  }
}

TEST(BufferRange, Subscript)
{
  const int start = 2;
  const int size = 4;
  const int num_channels = 3;
  const int buf_len = (start + size) * num_channels;
  float buf[buf_len];

  BufferRange<float> range(buf, start, size, num_channels);

  fill_buffer_with_indexes(buf, buf_len);
  int buf_index = start * num_channels;
  for (int i = 0; i < size; i++) {
    const float *elem = range[i];
    for (int ch = 0; ch < num_channels; ch++) {
      EXPECT_NEAR(elem[ch], buf_index, FLT_EPSILON);
      buf_index++;
    }
  }
  EXPECT_EQ(buf_index, buf_len);
}

TEST(BufferRange, SingleElemBufferIteration)
{
  const int start = 1;
  const int size = 3;
  const int num_channels = 4;
  float buf[num_channels];
  const int stride = 0;
  BufferRange<float> range(buf, start, size, stride);

  int elems_count = 0;
  for (float *elem : range) {
    EXPECT_EQ(elem, buf);
    elems_count++;
  }
  EXPECT_EQ(elems_count, 1);
}

TEST(BufferRange, FullBufferIteration)
{
  const int start = 2;
  const int size = 5;
  const int num_channels = 4;
  const int buf_len = (start + size) * num_channels;
  float buf[buf_len];
  BufferRange<float> range(buf, start, size, num_channels);

  fill_buffer_with_indexes(buf, buf_len);
  int buf_index = start * num_channels;
  for (float *elem : range) {
    for (int ch = 0; ch < num_channels; ch++) {
      EXPECT_NEAR(elem[ch], buf_index, FLT_EPSILON);
      buf_index++;
    }
  }
  EXPECT_EQ(buf_index, buf_len);
}

}  // namespace blender::compositor::tests
