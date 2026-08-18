/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

#include <array>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.hh"
#include "BLI_span.hh"

#include "GPU_context.hh"
#include "GPU_shader.hh"
#include "GPU_shader_builtin.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

namespace blender::gpu::tests {

struct ChangedRegion {
  int layer;
  rcti rect;
};

static BitVector<> modified_chunks_bitmask(const int2 chunks_size,
                                           const Span<ChangedRegion> changed,
                                           const int layer)
{
  constexpr int chunk_size = GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE;
  BitVector<> chunks(chunks_size.x * chunks_size.y, false);
  for (const ChangedRegion &c : changed) {
    if (c.layer != layer) {
      continue;
    }
    const rcti &r = c.rect;
    for (int ty = r.ymin / chunk_size; ty <= (r.ymax - 1) / chunk_size; ty++) {
      for (int tx = r.xmin / chunk_size; tx <= (r.xmax - 1) / chunk_size; tx++) {
        chunks[int64_t(ty) * chunks_size.x + tx].set();
      }
    }
  }
  return chunks;
}

static float4 to_float4(const float4 &v)
{
  return v;
}
static float4 to_float4(const uchar4 &v)
{
  return float4(v.x, v.y, v.z, v.w) * (1.0f / 255.0f);
}

template<typename T>
static void test_mipmap_partial_matches_full(const TextureFormat format,
                                             const eGPUDataFormat data_format,
                                             const eGPUTextureUsage extra_usage,
                                             const GPUBuiltinShader mipmap_shader,
                                             const Span<T> layer_backgrounds,
                                             const T changed_value,
                                             const int size,
                                             const Span<ChangedRegion> changed,
                                             const float background_threshold)
{
  GPU_render_begin();

  EXPECT_NE(GPU_shader_get_builtin_shader(mipmap_shader), nullptr)
      << "mipmap compute shader failed to compile";

  /* Create texture. */
  int mip_len = 1;
  while ((size >> mip_len) > 0) {
    mip_len++;
  }
  const int layers = int(layer_backgrounds.size());
  const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                                 GPU_TEXTURE_USAGE_HOST_READ | extra_usage;
  gpu::Texture *texture =
      (layers == 1) ?
          GPU_texture_create_2d("test_mipmap", size, size, mip_len, format, usage, nullptr) :
          GPU_texture_create_2d_array(
              "test_mipmap", size, size, layers, mip_len, format, usage, nullptr);

  /* Fill mip 0 with background color. */
  const int64_t layer_len = size * size;
  Array<T> mip0(layer_len * layers);
  for (int layer = 0; layer < layers; layer++) {
    mip0.as_mutable_span().slice(layer * layer_len, layer_len).fill(layer_backgrounds[layer]);
  }

  /* Generate full mip chain. */
  GPU_texture_update(texture, data_format, mip0.data());
  GPU_texture_update_mipmap_chain(texture);
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  /* Verify full update preserves color. */
  for (int mip = 1; mip < mip_len; mip++) {
    const int mip_size = size >> mip;
    T *data = static_cast<T *>(GPU_texture_read(texture, data_format, mip));
    float max_diff = 0.0f;
    for (int layer = 0; layer < layers; layer++) {
      const float4 expected = to_float4(layer_backgrounds[layer]);
      for (int64_t i = 0; i < mip_size * mip_size; i++) {
        const float4 value = to_float4(data[layer * mip_size * mip_size + i]);
        max_diff = math::max(max_diff, math::reduce_max(math::abs(value - expected)));
      }
    }
    EXPECT_LT(max_diff, 0.02f) << "mip level " << mip
                               << " of a constant image deviates from the background";
    MEM_delete(data);
  }

  /* Upload the changed regions. */
  for (const ChangedRegion &c : changed) {
    const int w = BLI_rcti_size_x(&c.rect);
    const int h = BLI_rcti_size_y(&c.rect);
    Array<T> patch(w * h);
    patch.fill(changed_value);
    GPU_texture_update_sub(
        texture, data_format, patch.data(), c.rect.xmin, c.rect.ymin, c.layer, w, h, 1);
  }
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  /* Partial update of the changed regions. */
  const int chunks_len = (size + GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE - 1) /
                         GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE;
  const int2 chunks_size(chunks_len, chunks_len);
  for (int layer = 0; layer < layers; layer++) {
    const BitVector<> modified_chunks = modified_chunks_bitmask(chunks_size, changed, layer);
    GPU_texture_update_mipmap_chain_partial(texture, layer, modified_chunks);
  }
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  /* Read back mip levels. */
  Array<Array<T>> partial(mip_len);
  for (int mip = 1; mip < mip_len; mip++) {
    const int mip_size = size >> mip;
    T *data = static_cast<T *>(GPU_texture_read(texture, data_format, mip));
    partial[mip] = Array<T>(Span<T>(data, mip_size * mip_size * layers));
    MEM_delete(data);
  }

  /* Regenerate the whole chain and compare. */
  GPU_texture_update_mipmap_chain(texture);
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  for (int mip = 1; mip < mip_len; mip++) {
    const int mip_size = size >> mip;
    T *data = static_cast<T *>(GPU_texture_read(texture, data_format, mip));
    int mismatches = 0;
    for (int64_t i = 0; i < mip_size * mip_size * layers; i++) {
      if (partial[mip][i] != data[i]) {
        mismatches++;
      }
    }
    EXPECT_EQ(mismatches, 0) << "mip level " << mip << " differs between partial and full update";
    MEM_delete(data);
  }

  /* Check the 1x1 mip of each layer is close to the background color. */
  {
    T *data = static_cast<T *>(GPU_texture_read(texture, data_format, mip_len - 1));
    for (int layer = 0; layer < layers; layer++) {
      const float diff = math::reduce_max(
          math::abs(to_float4(data[layer]) - to_float4(layer_backgrounds[layer])));
      EXPECT_LT(diff, background_threshold)
          << "coarsest mip of layer " << layer << " is not close to the background";
    }
    MEM_delete(data);
  }

  GPU_texture_free(texture);
  GPU_render_end();
}

static const std::array<ChangedRegion, 3> test_changed_regions = {
    {{0, {37, 87, 41, 87}}, {0, {300, 344, 320, 359}}, {0, {256, 281, 256, 281}}}};

static void test_mipmap_partial_float_pow2()
{
  test_mipmap_partial_matches_full<float4>(TextureFormat::SFLOAT_32_32_32_32,
                                           GPU_DATA_FLOAT,
                                           eGPUTextureUsage(0),
                                           GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_32_32_32,
                                           {float4(0.1f, 0.4f, 0.7f, 1.0f)},
                                           float4(0.9f, 0.2f, 0.5f, 1.0f),
                                           2 * GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE,
                                           test_changed_regions,
                                           0.4f);
}
GPU_TEST(mipmap_partial_float_pow2)

static void test_mipmap_partial_npot()
{
  test_mipmap_partial_matches_full<float4>(TextureFormat::SFLOAT_32_32_32_32,
                                           GPU_DATA_FLOAT,
                                           eGPUTextureUsage(0),
                                           GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_32_32_32,
                                           {float4(0.1f, 0.4f, 0.7f, 1.0f)},
                                           float4(0.9f, 0.2f, 0.5f, 1.0f),
                                           511,
                                           test_changed_regions,
                                           0.4f);
}
GPU_TEST(mipmap_partial_npot)

static void test_mipmap_partial_srgb_pow2()
{
  test_mipmap_partial_matches_full<uchar4>(TextureFormat::SRGBA_8_8_8_8,
                                           GPU_DATA_UBYTE,
                                           GPU_TEXTURE_USAGE_FORMAT_VIEW,
                                           GPU_SHADER_2D_UPDATE_MIPMAPS_SRGBA_8_8_8_8,
                                           {uchar4(30, 110, 200, 255)},
                                           uchar4(230, 50, 130, 255),
                                           2 * GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE,
                                           test_changed_regions,
                                           0.4f);
}
GPU_TEST(mipmap_partial_srgb_pow2)

static void test_mipmap_partial_layered()
{
  test_mipmap_partial_matches_full<float4>(TextureFormat::SFLOAT_32_32_32_32,
                                           GPU_DATA_FLOAT,
                                           eGPUTextureUsage(0),
                                           GPU_SHADER_2D_UPDATE_MIPMAPS_SFLOAT_32_32_32_32_LAYERED,
                                           {float4(0.1f, 0.3f, 0.6f, 1.0f),
                                            float4(0.2f, 0.3f, 0.6f, 1.0f),
                                            float4(0.3f, 0.3f, 0.6f, 1.0f)},
                                           float4(0.9f, 0.2f, 0.5f, 1.0f),
                                           2 * GPU_TEXTURE_MIPMAP_UPDATE_CHUNK_SIZE,
                                           {{0, {37, 87, 41, 87}}, {2, {300, 344, 320, 359}}},
                                           0.05f);
}
GPU_TEST(mipmap_partial_layered)

}  // namespace blender::gpu::tests
