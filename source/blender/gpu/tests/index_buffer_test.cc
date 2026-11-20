/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_index_buffer.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static void test_index_buffer_copy_sub()
{
  const uint src_len = 8;
  const uint dst_len = 6;

  /* Create source index buffer with known data. */
  const uint32_t src_data[src_len] = {0, 1, 2, 3, 4, 5, 6, 7};
  IndexBufPtr src_ibo = IndexBufPtr(
      GPU_indexbuf_build_from_memory(GPU_PRIM_POINTS, src_data, src_len, 0, src_len - 1, false));
  GPU_indexbuf_use(src_ibo.get());

  /* Create destination index buffer. */
  const uint32_t dst_data[dst_len] = {100, 101, 102, 103, 104, 105};
  IndexBufPtr dst_ibo = IndexBufPtr(
      GPU_indexbuf_build_from_memory(GPU_PRIM_POINTS, dst_data, dst_len, 100, 105, false));
  GPU_indexbuf_use(dst_ibo.get());

  /* Copy indices [2, 6) from the source buffer to [1, 5) of the destination buffer. */
  dst_ibo->copy_sub(*src_ibo, 2, 1, 4);

  /* Read back and validate. */
  uint32_t read_data[dst_len];
  GPU_indexbuf_read(dst_ibo.get(), read_data);
  ASSERT_FALSE(dst_ibo->is_32bit());
  MutableSpan<uint16_t> read_16 = MutableSpan<uint32_t>(read_data, dst_len).cast<uint16_t>();
  EXPECT_EQ(read_16[0], dst_data[0]);
  for (int i : IndexRange(4)) {
    EXPECT_EQ(read_16[i + 1], src_data[i + 2]);
  }
  EXPECT_EQ(read_16[5], dst_data[5]);
}

GPU_TEST(index_buffer_copy_sub)

}  // namespace blender::gpu::tests
