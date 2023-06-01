/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "COM_BufferArea.h"

namespace blender::compositor::tests {

static rcti create_rect(int width, int height)
{
  rcti rect;
  BLI_rcti_init(&rect, 0, width, 0, height);
  return rect;
}

static rcti create_rect(int width, int height, int offset)
{
  rcti rect;
  BLI_rcti_init(&rect, offset, offset + width, offset, offset + height);
  return rect;
}

TEST(BufferArea, BufferConstructor)
{
  const int width = 2;
  const int height = 3;
  BufferArea<float> area(nullptr, width, height, 4);
  EXPECT_EQ(area.width(), width);
  EXPECT_EQ(area.height(), height);
  rcti rect = create_rect(width, height);
  EXPECT_TRUE(BLI_rcti_compare(&area.get_rect(), &rect));
}

TEST(BufferArea, AreaConstructor)
{
  const int buf_width = 5;
  const int area_width = 1;
  const int area_height = 3;
  rcti area_rect = create_rect(area_width, area_height, 1);
  BufferArea<float> area(nullptr, buf_width, area_rect, 4);
  EXPECT_EQ(area.width(), area_width);
  EXPECT_EQ(area.height(), area_height);
  EXPECT_TRUE(BLI_rcti_compare(&area.get_rect(), &area_rect));
}

static void fill_buffer_with_indexes(float *buf, int buf_len)
{
  for (int i = 0; i < buf_len; i++) {
    buf[i] = i;
  }
}

static void test_single_elem_iteration(float *buffer, BufferArea<float> area)
{
  int elems_count = 0;
  for (float *elem : area) {
    EXPECT_EQ(elem, buffer);
    elems_count++;
  }
  EXPECT_EQ(elems_count, 1);
}

static void test_full_buffer_iteration(
    float *buf, int buf_width, int buf_len, int num_channels, BufferArea<float> area)
{
  fill_buffer_with_indexes(buf, buf_len);
  rcti rect = area.get_rect();
  int x = rect.xmin;
  int y = rect.ymin;
  for (float *elem : area) {
    for (int ch = 0; ch < num_channels; ch++) {
      const int buf_index = y * buf_width * num_channels + x * num_channels + ch;
      EXPECT_NEAR(elem[ch], buf_index, FLT_EPSILON);
    }
    x++;
    if (x == rect.xmax) {
      y++;
      x = rect.xmin;
    }
  }
  EXPECT_EQ(x, rect.xmin);
  EXPECT_EQ(y, rect.ymax);
}

TEST(BufferArea, SingleElemBufferIteration)
{
  const int buf_width = 4;
  const int buf_height = 5;
  const int area_width = 2;
  const int area_height = 3;
  const int num_channels = 4;
  const int stride = 0;
  float buf[num_channels];
  {
    BufferArea area(buf, buf_width, buf_height, stride);
    test_single_elem_iteration(buf, area);
  }
  {
    rcti area_rect = create_rect(area_width, area_height, 1);
    BufferArea area(buf, buf_width, area_rect, stride);
    test_single_elem_iteration(buf, area);
  }
}

TEST(BufferArea, FullBufferIteration)
{
  const int buf_width = 4;
  const int area_width = 2;
  const int area_height = 3;
  const int buf_height = (area_height + 1);
  const int num_channels = 4;
  const int buf_len = buf_height * buf_width * num_channels;
  float buf[buf_len];
  {
    BufferArea area(buf, buf_width, buf_height, num_channels);
    test_full_buffer_iteration(buf, buf_width, buf_len, num_channels, area);
  }
  {
    rcti area_rect = create_rect(area_width, area_height, 1);
    BufferArea area(buf, buf_width, area_rect, num_channels);
    test_full_buffer_iteration(buf, buf_width, buf_len, num_channels, area);
  }
}

}  // namespace blender::compositor::tests
