/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_storage_buffer.hh"

#include "BLI_math_vector.hh"
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

static void test_storage_buffer_copy_from_vertex_buffer()
{
  GPUStorageBuf *ssbo = GPU_storagebuf_create_ex(
      SIZE_IN_BYTES, nullptr, GPU_USAGE_STATIC, __func__);
  EXPECT_NE(ssbo, nullptr);

  /* Create vertex buffer. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  VertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, 4);

  struct Vert {
    float2 pos;
    float4 color;
  };
  Vert data[4] = {
      {float2(-1.0, -1.0), float4(0.0, 0.0, 0.0, 1.0)},
      {float2(1.0, -1.0), float4(1.0, 0.0, 0.0, 1.0)},
      {float2(1.0, 1.0), float4(1.0, 1.0, 0.0, 1.0)},
      {float2(-1.0, 1.0), float4(0.0, 1.0, 0.0, 1.0)},
  };
  for (int i : IndexRange(4)) {
    GPU_vertbuf_vert_set(vbo, i, &data[i]);
  }
  float *expected_data = static_cast<float *>(static_cast<void *>(&data));

  Vector<float> read_data;
  read_data.resize(SIZE, 0);

  /* Copy vertex buffer to storage buffer. */
  {
    GPU_storagebuf_clear_to_zero(ssbo);
    GPU_storagebuf_copy_sub_from_vertbuf(ssbo, vbo, 0, 0, sizeof(data));

    /* Validate content of SSBO. */
    GPU_storagebuf_read(ssbo, read_data.data());
    EXPECT_EQ_ARRAY(expected_data, read_data.data(), 24);
    for (int i : IndexRange(24, SIZE - 24)) {
      EXPECT_EQ(0.0, read_data[i]);
    }
  }

  /* Copy vertex buffer to storage buffer with 16 bytes of offset. */
  {
    GPU_storagebuf_clear_to_zero(ssbo);
    GPU_storagebuf_copy_sub_from_vertbuf(ssbo, vbo, 16, 0, sizeof(data));

    /* Validate content of SSBO. */
    GPU_storagebuf_read(ssbo, read_data.data());
    for (int i : IndexRange(4)) {
      EXPECT_EQ(0.0, read_data[i]);
    }
    float *expected_data = static_cast<float *>(static_cast<void *>(&data));
    EXPECT_EQ_ARRAY(expected_data, &(read_data.data()[4]), 24);
    for (int i : IndexRange(28, SIZE - 28)) {
      EXPECT_EQ(0.0, read_data[i]);
    }
  }

  /* Partially Copy vertex buffer to storage buffer with 16 bytes of offset. */
  {
    GPU_storagebuf_clear_to_zero(ssbo);
    GPU_storagebuf_copy_sub_from_vertbuf(ssbo, vbo, 16, sizeof(Vert), sizeof(data) / 2);

    /* Validate content of SSBO. */
    GPU_storagebuf_read(ssbo, read_data.data());
    for (int i : IndexRange(4)) {
      EXPECT_EQ(0.0, read_data[i]);
    }
    float *expected_data = static_cast<float *>(static_cast<void *>(&data));
    EXPECT_EQ_ARRAY(&expected_data[6], &(read_data.data()[4]), 12);
    for (int i : IndexRange(16, SIZE - 16)) {
      EXPECT_EQ(0.0, read_data[i]);
    }
  }

  GPU_vertbuf_discard(vbo);
  GPU_storagebuf_free(ssbo);
}

GPU_TEST(storage_buffer_copy_from_vertex_buffer);

}  // namespace blender::gpu::tests
