/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * Set of tests to identify issues with glTextureView and glGetTexImage. Note; these tests
 * rely on device-to-host data conversion (f16 -> f32) for texture readback, which is only
 * currently supported on OpenGL. Hence, they are only enabled on OpenGL.
 */

#include "gpu_testing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "gpu_texture_private.hh"

namespace blender::gpu::tests {

/* Repeat the first `components` components of a float4 `n` times, into a vector. */
template<typename T> static Vector<T> repeat_data(VecBase<T, 4> data, size_t n, size_t components)
{
  Vector<T> out(n * components);
  for (uint i = 0; i < out.size(); ++i) {
    out[i] = data[i % components];
  }
  return out;
}

/* Create a base texture of the specified format and clear it to black. */
static gpu::Texture *create_base_texture(TextureFormat format,
                                         uint2 size,
                                         int mip_len = 1,
                                         int layer_len = 0,
                                         const float *data = nullptr)
{
  constexpr eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL | GPU_TEXTURE_USAGE_HOST_READ |
                                     GPU_TEXTURE_USAGE_FORMAT_VIEW;

  gpu::Texture *base = nullptr;
  if (layer_len == 0) {
    base = GPU_texture_create_2d("base", size.x, size.y, mip_len, format, usage, data);
  }
  else {
    base = GPU_texture_create_2d_array(
        "base", size.x, size.y, layer_len, mip_len, format, usage, data);
  }

  GPU_texture_mipmap_mode(base, false, false);
  GPU_memory_barrier(GPU_BARRIER_FRAMEBUFFER);

  if (!data) {
    /* Bind the texture as attachment to a temporary framebuffer, and clear to black. */
    gpu::FrameBuffer *fbo = nullptr;
    GPU_framebuffer_ensure_config(&fbo, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(base)});
    GPU_framebuffer_bind(fbo);
    GPU_framebuffer_clear(fbo, GPUFrameBufferBits::GPU_COLOR_BIT, {0.0, 0.0, 0.0, 0.0}, 0.0f, 0u);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_framebuffer_free(fbo);
  }

  return base;
}

/* Create a view texture of compatible aliasing format. */
static gpu::Texture *create_view_texture(TextureFormat format,
                                         gpu::Texture *base,
                                         int mip = 0,
                                         int layer = 0)
{
  gpu::Texture *view = GPU_texture_create_view(
      "view", base, format, mip, 1, layer, 1, false, false);
  GPU_texture_mipmap_mode(view, false, false);
  GPU_memory_barrier(GPU_BARRIER_FRAMEBUFFER);
  return view;
}

/* Read back a texture into a contiguous vector of T. */
template<typename T>
static Vector<T> read_texture(gpu::Texture *texture, eGPUDataFormat data_format, int mip_level = 0)
{
  void *src = GPU_texture_read(texture, data_format, mip_level);
  Vector<T> dst;
  dst.resize(GPU_texture_read_size_get(texture, data_format, mip_level) / sizeof(T));
  std::memcpy(static_cast<void *>(dst.data()), src, sizeof(T) * dst.size());

  MEM_delete_void(src);

  return dst;
}

/* Given a pair of TextureFormat values, create base and view textures and
 * attempt to perform a framebuffer color clear over the view texture. */
template<TextureFormat FormatA, TextureFormat FormatB>
static void texture_view_create_format_test()
{
  GPU_render_begin();

  /* Test operates on a 4x4 texture patch. */
  const uint2 texture_size = uint2(4);
  const uint pixel_count = texture_size.x * texture_size.y;

  /* Float comparator threshold; half-to-full conversion has significant precision loss. */
  constexpr float tolerance = 1e5f;

  gpu::Texture *base = create_base_texture(FormatA, texture_size);
  gpu::Texture *view = create_view_texture(FormatB, base);

  /* First check; the view texture should be all zeroes. */
  float4 zero(0.0f, 0.0f, 0.0f, 0.0f);
  if (ELEM(to_texture_data_format(FormatB), GPU_DATA_UINT, GPU_DATA_2_10_10_10_REV)) {
    uint4 uzero(zero);
    auto zero_expected = repeat_data(uzero, pixel_count, to_component_len(FormatB));
    auto zero_readback = read_texture<uint>(view, GPU_DATA_UINT);
    EXPECT_EQ(zero_expected, zero_readback);
  }
  else if (to_texture_data_format(FormatB) == GPU_DATA_INT) {
    int4 izero(zero);
    auto zero_expected = repeat_data(izero, pixel_count, to_component_len(FormatB));
    auto zero_readback = read_texture<int>(view, GPU_DATA_INT);
    EXPECT_EQ(zero_expected, zero_readback);
  }
  else if (ELEM(to_texture_data_format(FormatB), GPU_DATA_FLOAT, GPU_DATA_10_11_11_REV)) {
    auto zero_expected = repeat_data(zero, pixel_count, to_component_len(FormatB));
    auto zero_readback = read_texture<float>(view, GPU_DATA_FLOAT);
    EXPECT_NEAR_SPAN(zero_expected.as_span(), zero_readback.as_span(), tolerance);
  }
  else {
    BLI_assert_unreachable();
  }

  /* Create FBO with view as color attachment 0. */
  gpu::FrameBuffer *fbo = nullptr;
  GPU_framebuffer_ensure_config(&fbo, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(view)});
  GPU_framebuffer_bind(fbo);

  /* Clear FBO to specific color with a different value on each channel. */
  float4 colr = (ELEM(to_texture_data_format(FormatB), GPU_DATA_FLOAT, GPU_DATA_10_11_11_REV)) ?
                    float4(0.75f, 0.5f, 0.25f, 0.0f) :
                    float4(127.0f, 31.0f, 14.0f, 15.0f);
  GPU_framebuffer_clear(fbo, GPUFrameBufferBits::GPU_COLOR_BIT, double4(colr), 0.0f, 0u);
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  /* Second check; the view texture should read back this color. */
  if (ELEM(to_texture_data_format(FormatB), GPU_DATA_UINT, GPU_DATA_2_10_10_10_REV)) {
    uint4 ucolr(colr);
    auto colr_expected = repeat_data(ucolr, pixel_count, to_component_len(FormatB));
    auto colr_readback = read_texture<uint>(view, GPU_DATA_UINT);
    EXPECT_EQ(colr_expected, colr_readback);
  }
  else if (to_texture_data_format(FormatB) == GPU_DATA_INT) {
    int4 icolr(colr);
    auto colr_expected = repeat_data(icolr, pixel_count, to_component_len(FormatB));
    auto colr_readback = read_texture<int>(view, GPU_DATA_INT);
    EXPECT_EQ(colr_expected, colr_readback);
  }
  else if (ELEM(to_texture_data_format(FormatB), GPU_DATA_FLOAT, GPU_DATA_10_11_11_REV)) {
    auto colr_expected = repeat_data(colr, pixel_count, to_component_len(FormatB));
    auto colr_readback = read_texture<float>(view, GPU_DATA_FLOAT);
    EXPECT_NEAR_SPAN(colr_expected.as_span(), colr_readback.as_span(), tolerance);
  }
  else {
    BLI_assert_unreachable();
  }

  GPU_framebuffer_free(fbo);
  GPU_texture_free(view);
  GPU_texture_free(base);

  GPU_render_end();
}

static void test_texture_view_SFLOAT_32_32_32_32()
{
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32_32_32,
                                  TextureFormat::SFLOAT_32_32_32_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32_32_32,
                                  TextureFormat::UINT_32_32_32_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32_32_32,
                                  TextureFormat::SINT_32_32_32_32>();
}
GPU_OPENGL_TEST(texture_view_SFLOAT_32_32_32_32);

static void test_texture_view_SFLOAT_32_32()
{
  if (GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GTEST_SKIP() << "Broken on AMD.";
  }

  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::SFLOAT_32_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32,
                                  TextureFormat::SFLOAT_16_16_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::UINT_32_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::UINT_16_16_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::SINT_32_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::SINT_16_16_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::SNORM_16_16_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32_32, TextureFormat::UNORM_16_16_16_16>();
}
GPU_OPENGL_TEST(texture_view_SFLOAT_32_32);

static void test_texture_view_SFLOAT_32()
{
  if (GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GTEST_SKIP() << "Broken on AMD.";
  }

  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SFLOAT_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SFLOAT_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UINT_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UINT_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UINT_8_8_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SINT_32>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SINT_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SINT_8_8_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SNORM_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SNORM_8_8_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UNORM_16_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UNORM_8_8_8_8>();

  /* Note the special formats. */
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UFLOAT_11_11_10>();
  texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::SRGBA_8_8_8_8>();

  /* Skipped: readback is not handled as we store these in reverse order. */
  // texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UINT_10_10_10_2>();
  // texture_view_create_format_test<TextureFormat::SFLOAT_32, TextureFormat::UNORM_10_10_10_2>();
}
GPU_OPENGL_TEST(texture_view_SFLOAT_32);

static void test_texture_view_SFLOAT_16()
{
  if (GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GTEST_SKIP() << "Broken on AMD.";
  }

  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::SFLOAT_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::UINT_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::UINT_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::SINT_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::SINT_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::SNORM_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::UNORM_8_8>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::UNORM_16>();
  texture_view_create_format_test<TextureFormat::SFLOAT_16, TextureFormat::UNORM_8_8>();
}
GPU_OPENGL_TEST(texture_view_SFLOAT_16);

static void test_texture_view_UINT_8()
{
  texture_view_create_format_test<TextureFormat::UINT_8, TextureFormat::UINT_8>();
  texture_view_create_format_test<TextureFormat::UINT_8, TextureFormat::SINT_8>();
  texture_view_create_format_test<TextureFormat::UINT_8, TextureFormat::SNORM_8>();
  texture_view_create_format_test<TextureFormat::UINT_8, TextureFormat::UNORM_8>();
}
GPU_OPENGL_TEST(texture_view_UINT_8);

static void test_texture_view_mip_layer_test()
{
  if (GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL) ||
      GPU_type_matches_ex(GPU_DEVICE_INTEL, GPU_OS_UNIX, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL))
  {
    GTEST_SKIP() << "Broken on AMD and Intel-Linux.";
  }

  GPU_render_begin();

  /* Test operates on a 4x4 texture patch. */
  const uint2 texture_size = uint2(4);
  const TextureFormat format = TextureFormat::UINT_8_8;

  auto mip_size = [&](int mip) { return int3(texture_size.x >> mip, texture_size.y >> mip, 1); };

  auto layer_mip_data = [&](int layer, int mip) {
    int3 size = mip_size(mip);
    return repeat_data(uint4(layer, layer == 0 ? 0 : mip, 0, 0), size.x * size.y, 2);
  };

  gpu::Texture *base = create_base_texture(format, texture_size, 3, 3);

  /* Clear everything. Layer 0 will be kept like this. */
  base->clear(double4(0, 0, 0, 0));

  for (int mip : IndexRange(3)) {
    /* Upload to layer 1 through base texture */
    if (GPU_backend_get_type() == GPU_BACKEND_METAL && mip != 0) {
      /* The Metal API doesn't support this.
       * > Updating texture layers other than mip=0 when data is mismatched is not possible in
       * > METAL on macOS using texture->write.
       * Use a clear instead. :/ */
      gpu::Texture *layer_1_view = create_view_texture(format, base, mip, 1);
      layer_1_view->clear(double4(1, mip, 0, 0));
      GPU_texture_free(layer_1_view);
    }
    else {
      base->update_sub(mip,
                       int3(0, 0, 1),
                       mip_size(mip),
                       eGPUDataFormat::GPU_DATA_UINT,
                       layer_mip_data(1, mip).data());
    }

    /* Clear layer 2 using a layer and mip view. */
    gpu::Texture *layer_2_view = create_view_texture(format, base, mip, 2);
    layer_2_view->clear(double4(2, mip, 0, 0));
    GPU_texture_free(layer_2_view);
  }

  gpu::Texture *copy = create_base_texture(format, texture_size, 3, 4);

  for (int layer : IndexRange(3)) {
    for (int mip : IndexRange(3)) {
      auto expected_data = layer_mip_data(layer, mip);

      gpu::Texture *base_view = create_view_texture(format, base, mip, layer);
      auto base_readback = read_texture<uint>(base_view, GPU_DATA_UINT, 0);

      EXPECT_EQ(expected_data, base_readback);

      gpu::Texture *copy_view = create_view_texture(format, copy, mip, 3 - layer);
      GPU_texture_copy(copy_view, base_view);
      auto copy_readback = read_texture<uint>(copy_view, GPU_DATA_UINT, 0);

      EXPECT_EQ(expected_data, copy_readback);

      GPU_texture_free(base_view);
      GPU_texture_free(copy_view);
    }
  }

  GPU_texture_free(base);
  GPU_texture_free(copy);

  GPU_render_end();
}
GPU_TEST(texture_view_mip_layer_test);

}  // namespace blender::gpu::tests
