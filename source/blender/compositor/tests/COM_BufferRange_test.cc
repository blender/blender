/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
