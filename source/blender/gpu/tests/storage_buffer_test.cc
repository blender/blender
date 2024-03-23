/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_storage_buffer.hh"

#include "BLI_vector.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

constexpr size_t SIZE = 128;
constexpr size_t SIZE_IN_BYTES = SIZE * sizeof(int);

static Vector<int32_t> test_data()
{
  Vector<int32_t> data;
  for (int i : IndexRange(SIZE)) {
    data.append(i);
  }
  return data;
}

static void test_storage_buffer_create_update_read()
{
  GPUStorageBuf *ssbo = GPU_storagebuf_create_ex(
      SIZE_IN_BYTES, nullptr, GPU_USAGE_STATIC, __func__);
  EXPECT_NE(ssbo, nullptr);

  /* Upload some dummy data. */
  const Vector<int32_t> data = test_data();
  GPU_storagebuf_update(ssbo, data.data());

  /* Read back data from SSBO. */
  Vector<int32_t> read_data;
  read_data.resize(SIZE, 0);
  GPU_storagebuf_read(ssbo, read_data.data());

  /* Check if data is the same. */
  for (int i : IndexRange(SIZE)) {
    EXPECT_EQ(data[i], read_data[i]);
  }

  GPU_storagebuf_free(ssbo);
}

GPU_TEST(storage_buffer_create_update_read);

static void test_storage_buffer_clear_zero()
{
  GPUStorageBuf *ssbo = GPU_storagebuf_create_ex(
      SIZE_IN_BYTES, nullptr, GPU_USAGE_STATIC, __func__);
  EXPECT_NE(ssbo, nullptr);

  /* Upload some dummy data. */
  const Vector<int32_t> data = test_data();
  GPU_storagebuf_update(ssbo, data.data());
  GPU_storagebuf_clear_to_zero(ssbo);

  /* Read back data from SSBO. */
  Vector<int32_t> read_data;
  read_data.resize(SIZE, 0);
  GPU_storagebuf_read(ssbo, read_data.data());

  /* Check if data is the same. */
  for (int i : IndexRange(SIZE)) {
    EXPECT_EQ(0, read_data[i]);
  }

  GPU_storagebuf_free(ssbo);
}
GPU_TEST(storage_buffer_clear_zero);

static void test_storage_buffer_clear()
{
  GPUStorageBuf *ssbo = GPU_storagebuf_create_ex(
      SIZE_IN_BYTES, nullptr, GPU_USAGE_STATIC, __func__);
  EXPECT_NE(ssbo, nullptr);

  GPU_storagebuf_clear(ssbo, 157255);

  /* Read back data from SSBO. */
  Vector<int32_t> read_data;
  read_data.resize(SIZE, 0);
  GPU_storagebuf_read(ssbo, read_data.data());

  /* Check if datatest_ is the same. */
  for (int i : IndexRange(SIZE)) {
    EXPECT_EQ(157255, read_data[i]);
  }

  GPU_storagebuf_free(ssbo);
}

GPU_TEST(storage_buffer_clear);

}  // namespace blender::gpu::tests
