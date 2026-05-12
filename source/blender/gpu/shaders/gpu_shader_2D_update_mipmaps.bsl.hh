/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#pragma once

#include "gpu_shader_utildefines_lib.glsl"

namespace builtin::mipmaps {

/* Conversion functions. */
template<typename DstType, typename SrcType>
void convert(DstType & /*dst_value*/, const SrcType /*src_value*/)
{
}

template<> void convert<float4, float>(float4 &dst_value, const float src_value)
{
  dst_value.x = src_value;
  dst_value.y = 0.0;
  dst_value.z = 0.0;
  dst_value.w = 0.0;
}
template<> void convert<float, float4>(float &dst_value, const float4 src_value)
{
  dst_value = src_value.x;
}
template<> void convert<float4, float4>(float4 &dst_value, const float4 src_value)
{
  dst_value = src_value;
}

/* Color transfer functions */
/* TODO: should be moved to a library */
float srgb_to_linearrgb(float c)
{
  if (c < 0.04045f) {
    return (c < 0.0f) ? 0.0f : c * (1.0f / 12.92f);
  }

  return pow((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308f) {
    return (c < 0.0f) ? 0.0f : c * 12.92f;
  }

  return 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

/**
 * General-case shader for generating 1 or 2 levels of the mip pyramid.
 * When generating 1 level, each workgroup handles up to 128 samples of the
 * output mip level. When generating 2 levels, each workgroup handles
 * a 8x8 tile of the last (2nd) output mip level, generating up to
 * 17x17 samples of the intermediate (1st) output mip level along the way.
 *
 * Dispatch with y, z = 1
 */
#define LOCAL_SIZE_X 128
#define TILE_SIZE 8
#define MAX_SHARED_SAMPLES (TILE_SIZE + TILE_SIZE + 1)
#define INPUT_LEVEL 0

/** Shared storage that can store intermediate results using without encoding. */
template<typename T> struct Shared {
  /**
   * When generating 2 levels, the results the first level are cached here; this is the input tile
   * needed to generate the 8x8 tile of the second level.
   */
  [[shared]] T intermediate_level[MAX_SHARED_SAMPLES][MAX_SHARED_SAMPLES];

  void store_sample(int2 dst_coord, T color)
  {
    intermediate_level[dst_coord.y][dst_coord.x] = color;
  }

  T load_sample(int2 src_coord)
  {
    return intermediate_level[src_coord.y][src_coord.x];
  }
};

/** Shared storage that can store intermediate results encoded as uint. */
struct SharedUnorm {
  /**
   * When generating 2 levels, the results the first level are cached here; this is the input tile
   * needed to generate the 8x8 tile of the second level.
   */
  [[shared]] uint intermediate_level[MAX_SHARED_SAMPLES][MAX_SHARED_SAMPLES];

  void store_sample(int2 dst_coord, float color)
  {
    uint encoded = uint(clamp(color, 0.0f, 1.0f) * UINT_MAX);
    intermediate_level[dst_coord.y][dst_coord.x] = encoded;
  }

  float load_sample(int2 src_coord)
  {
    uint encoded = intermediate_level[src_coord.y][src_coord.x];
    return float(encoded) / UINT_MAX;
  }
};

/** Shared storage that can store intermediate results in an SRGB encoded uint. */
struct SharedSRGB {
  /**
   * When generating 2 levels, the results the first level are cached here; this is the input tile
   * needed to generate the 8x8 tile of the second level.
   */
  [[shared]] uint intermediate_level[MAX_SHARED_SAMPLES][MAX_SHARED_SAMPLES];

  void store_sample(int2 dst_coord, float4 color)
  {
    float4 srgba;
    srgba.r = linearrgb_to_srgb(color.r);
    srgba.g = linearrgb_to_srgb(color.g);
    srgba.b = linearrgb_to_srgb(color.b);
    srgba.a = color.a;
    uint srgb_packed = packUnorm4x8(srgba);
    intermediate_level[dst_coord.y][dst_coord.x] = srgb_packed;
  }

  float4 load_sample(int2 src_coord)
  {
    uint srgb_packed = intermediate_level[src_coord.y][src_coord.x];
    float4 srgba = unpackUnorm4x8(srgb_packed);
    float4 linear_color;
    linear_color.r = srgb_to_linearrgb(srgba.r);
    linear_color.g = srgb_to_linearrgb(srgba.g);
    linear_color.b = srgb_to_linearrgb(srgba.b);
    linear_color.a = srgba.a;
    return linear_color;
  }
};

int2 kernel_size_from_input_size(int2 input_size)
{
  return int2(input_size.x == 1 ? 1 : (2 | (input_size.x & 1)),
              input_size.y == 1 ? 1 : (2 | (input_size.y & 1)));
}

/**
 * \brief Templated struct for bindings and performing the mipmap generation.
 *
 * The mipmap generation is based on the general algorithm of
 * https://github.com/nvpro-samples/vk_compute_mipmaps/tree/main/nvpro_pyramid
 * It can generate 2 mipmap levels per dispatch.
 *
 * \param format is the texture format of the mipmap images.
 *
 * \param SharedStorage is the storage class to store intermediate levels. Depending on the texture
 * format an optimal storage class can be selected.
 *
 * \param InnerType the type to use for computation. Depending on the number of samples that a
 * texture format has a more memory efficient type can be used.
 */
template<enum TextureWriteFormat format, typename SharedStorage, typename InnerType>
struct Resources {
  [[compilation_constant]] const bool is_srgb_texture;
  [[compilation_constant]] const bool is_layered;
  [[push_constant]] const int num_levels;
  [[image(0, read, format), condition(!is_layered)]] image2D mip_in;
  [[image(1, write, format), condition(!is_layered)]] image2D mip_out1;
  [[image(2, write, format), condition(!is_layered)]] image2D mip_out2;
  [[image(0, read, format), condition(is_layered)]] image2DArray mip_array_in;
  [[image(1, write, format), condition(is_layered)]] image2DArray mip_array_out1;
  [[image(2, write, format), condition(is_layered)]] image2DArray mip_array_out2;
  [[resource_table]] srt_t<SharedStorage> shared_storage;

  /** Store sample result into an output mip image. */
  void store_sample(int2 dst_coord, int dst_level, InnerType color)
  {
    float4 color_out;
    convert<float4, InnerType>(color_out, color);
    if (is_srgb_texture) [[static_branch]] {
      color_out.r = linearrgb_to_srgb(color_out.r);
      color_out.g = linearrgb_to_srgb(color_out.g);
      color_out.b = linearrgb_to_srgb(color_out.b);
    }
    if (is_layered == false) [[static_branch]] {
      if (dst_level == 1) {
        imageStore(mip_out1, dst_coord, color_out);
      }
      else if (dst_level == 2) {
        imageStore(mip_out2, dst_coord, color_out);
      }
    }
    if (is_layered) [[static_branch]] {
      if (dst_level == 1) {
        imageStore(mip_array_out1, int3(dst_coord, 0), color_out);
      }
      else if (dst_level == 2) {
        imageStore(mip_array_out2, int3(dst_coord, 0), color_out);
      }
    }
  }

  void store_shared_sample(int2 dst_coord, InnerType color)
  {
    SharedStorage &storage = shared_storage;
    storage.store_sample(dst_coord, color);
  }

  InnerType load_sample(int2 src_coord, bool load_from_shared)
  {
    InnerType color;
    if (load_from_shared) {
      SharedStorage &storage = shared_storage;
      color = storage.load_sample(src_coord);
    }
    else {
      float4 loaded_color;
      if (is_layered == false) [[static_branch]] {
        loaded_color = imageLoad(mip_in, src_coord);
      }
      if (is_layered) [[static_branch]] {
        loaded_color = imageLoad(mip_array_in, int3(src_coord, 0));
      }
      if (is_srgb_texture) [[static_branch]] {
        loaded_color.r = srgb_to_linearrgb(loaded_color.r);
        loaded_color.g = srgb_to_linearrgb(loaded_color.g);
        loaded_color.b = srgb_to_linearrgb(loaded_color.b);
      }
      convert<InnerType, float4>(color, loaded_color);
    }
    return color;
  }

  int2 level_size(int level)
  {
    int2 mip_in_size;
    if (is_layered == false) [[static_branch]] {
      mip_in_size = imageSize(mip_in);
    }
    if (is_layered) [[static_branch]] {
      mip_in_size = imageSize(mip_array_in).xy;
    }
    int2 mip_size = max((mip_in_size >> level), int2(1));
    return mip_size;
  }

  InnerType pyramid_reduce_3(
      float a0, InnerType v0, float a1, InnerType v1, float a2, InnerType v2)
  {
    return a0 * v0 + a1 * v1 + a2 * v2;
  }

  InnerType pyramid_reduce_2(InnerType v0, InnerType v1)
  {
    return 0.5 * (v0 + v1);
  }

  /**
   * Handle loading and reducing a rectangle of size kernel_size
   * with the given upper-left coordinate src_coord. Samples read from
   * mip level src_level if !loadFromShared_, sharedLevel_ otherwise.
   *
   * kernel_size must range from 1x1 to 3x3.
   *
   * Once computed, the sample is written to the given coordinate of the
   * specified destination mip level, and returned. The destination
   * image size is needed to compute the kernel weights.
   */
  template<bool load_from_shared>
  InnerType reduce_store_sample(int2 src_coord,
                                int /*src_level*/,
                                int2 kernel_size,
                                int2 dst_image_size,
                                int2 dst_coord,
                                int dst_level)
  {
    float num_dst_pixels = dst_image_size.y;
    float rcp = 1.0f / (2 * num_dst_pixels + 1);
    float w0 = rcp * (num_dst_pixels - dst_coord.y);
    float w1 = rcp * num_dst_pixels;
    float w2 = 1.0f - w0 - w1;

    InnerType v0, v1, v2, h0, h1, h2, out_pixel;

    /* Reduce vertically up to 3 times (depending on kernel horizontal size) */
    switch (kernel_size.x) {
      case 3:
        switch (kernel_size.y) {
          case 3:
            v2 = load_sample(src_coord + int2(2, 2), load_from_shared);
            ATTR_FALLTHROUGH;
          case 2:
            v1 = load_sample(src_coord + int2(2, 1), load_from_shared);
            ATTR_FALLTHROUGH;
          case 1:
            v0 = load_sample(src_coord + int2(2, 0), load_from_shared);
            break;
        }
        switch (kernel_size.y) {
          case 3:
            h2 = pyramid_reduce_3(w0, v0, w1, v1, w2, v2);
            break;
          case 2:
            h2 = pyramid_reduce_2(v0, v1);
            break;
          case 1:
            h2 = v0;
            break;
        }
        ATTR_FALLTHROUGH;
      case 2:
        switch (kernel_size.y) {
          case 3:
            v2 = load_sample(src_coord + int2(1, 2), load_from_shared);
            ATTR_FALLTHROUGH;
          case 2:
            v1 = load_sample(src_coord + int2(1, 1), load_from_shared);
            ATTR_FALLTHROUGH;
          case 1:
            v0 = load_sample(src_coord + int2(1, 0), load_from_shared);
            break;
        }
        switch (kernel_size.y) {
          case 3:
            h1 = pyramid_reduce_3(w0, v0, w1, v1, w2, v2);
            break;
          case 2:
            h1 = pyramid_reduce_2(v0, v1);
            break;
          case 1:
            h1 = v0;
            break;
        }
        ATTR_FALLTHROUGH;
      case 1:
        switch (kernel_size.y) {
          case 3:
            v2 = load_sample(src_coord + int2(0, 2), load_from_shared);
            ATTR_FALLTHROUGH;
          case 2:
            v1 = load_sample(src_coord + int2(0, 1), load_from_shared);
            ATTR_FALLTHROUGH;
          case 1:
            v0 = load_sample(src_coord + int2(0, 0), load_from_shared);
            break;
        }
        switch (kernel_size.y) {
          case 3:
            h0 = pyramid_reduce_3(w0, v0, w1, v1, w2, v2);
            break;
          case 2:
            h0 = pyramid_reduce_2(v0, v1);
            break;
          case 1:
            h0 = v0;
            break;
        }
    }

    /* Reduce up to 3 samples horizontally. */
    switch (kernel_size.x) {
      case 3:
        num_dst_pixels = dst_image_size.x;
        rcp = 1.0f / (2 * num_dst_pixels + 1);
        w0 = rcp * (num_dst_pixels - dst_coord.x);
        w1 = rcp * num_dst_pixels;
        w2 = 1.0f - w0 - w1;
        out_pixel = pyramid_reduce_3(w0, h0, w1, h1, w2, h2);
        break;
      case 2:
        out_pixel = pyramid_reduce_2(h0, h1);
        break;
      case 1:
        out_pixel = h0;
    }

    /* Write out sample. */
    store_sample(dst_coord, dst_level, out_pixel);
    return out_pixel;
  }

  /**
   * Compute and write out (to the 1st mip level generated) the samples
   * at coordinates
   *     init_dst_coord,
   *     init_dst_coord + step, ...
   *     init_dst_coord + (iterations-1) * step
   * and cache them at in the sharedLevel_ tile at coordinates
   *     init_shared_coord,
   *     init_shared_coord + step, ...
   *     init_shared_coord + (iterations-1) * step
   * If use_bounds_check is true, skip coordinates that are out of bounds.
   */
  void intermediate_level_loop(int2 init_dst_coord,
                               int2 init_shared_coord,
                               int2 step,
                               int iterations,
                               bool use_bounds_check)
  {
    int2 dst_coord = init_dst_coord;
    int2 shared_coord = init_shared_coord;
    int src_level = INPUT_LEVEL;
    int dst_level = src_level + 1;
    int2 src_image_size = level_size(src_level);
    int2 dst_image_size = level_size(dst_level);
    int2 kernel_size = kernel_size_from_input_size(src_image_size);

    for (int i_ = 0; i_ < iterations; ++i_) {
      int2 src_coord = dst_coord * 2;

      if (use_bounds_check) {
        if (uint(dst_coord.x) >= uint(dst_image_size.x)) {
          continue;
        }
        if (uint(dst_coord.y) >= uint(dst_image_size.y)) {
          continue;
        }
      }

      InnerType result = reduce_store_sample<false>(
          src_coord, src_level, kernel_size, dst_image_size, dst_coord, dst_level);

      /* `reduce_store_sample` handles writing to the actual output; manually
       * cache into shared memory here. */
      store_shared_sample(shared_coord, result);
      dst_coord += step;
      shared_coord += step;
    }
  }

  /**
   * Function for the workgroup that handles filling the intermediate level
   * (caching it in shared memory as well).
   *
   * We need somewhere from 16x16 to 17x17 samples, depending
   * on what the kernel size for the 2nd mip level generation will be.
   *
   * dst_tile_coord : upper left coordinate of the tile to generate.
   * use_bounds_check  : whether to skip samples that are out-of-bounds.
   */
  void fill_intermediate_tile(uint local_index, int2 dst_tile_coord, bool use_bounds_check)
  {
    int2 init_thread_offset;
    int2 step;
    int iterations;

    int2 dst_image_size = level_size(INPUT_LEVEL + 1);
    int2 future_kernel_size = kernel_size_from_input_size(dst_image_size);

    if (future_kernel_size.x == 3) {
      if (future_kernel_size.y == 3) {
        /* Fill in 2 17x7 steps and 1 17x3 step (9 idle threads) */
        init_thread_offset = int2(local_index % 17u, local_index / 17u);
        step = int2(0, 7);
        iterations = local_index >= 7 * 17 ? 0 : local_index < 3 * 17 ? 3 : 2;
      }
      else {
        /* Future 3x[2,1] kernel
         * Fill in 2 8x16 steps and 1 1x16 step */
        init_thread_offset = int2(local_index / 16u, local_index % 16u);
        step = int2(8, 0);
        iterations = local_index < 1 * 16 ? 3 : 2;
      }
    }
    else {
      if (future_kernel_size.y == 3) {
        /* Fill in 2 16x8 steps and 1 16x1 step */
        init_thread_offset = int2(local_index % 16u, local_index / 16u);
        step = int2(0, 8);
        iterations = local_index < 1 * 16 ? 3 : 2;
      }
      else {
        /* Fill in 2 16x8 steps */
        init_thread_offset = int2(local_index % 16u, local_index / 16u);
        step = int2(0, 8);
        iterations = 2;
      }
    }

    intermediate_level_loop(dst_tile_coord + init_thread_offset,
                            init_thread_offset,
                            step,
                            iterations,
                            use_bounds_check);
  }

  /**
   * Function for the workgroup that handles filling the last level tile
   * (2nd level after the original input level), using as input the
   * tile in shared memory.
   *
   * dst_tile_coord : upper left coordinate of the tile to generate.
   * use_bounds_check  : whether to skip samples that are out-of-bounds.
   */
  void fill_last_tile(uint local_index, int2 dst_tile_coord, bool use_bounds_check)
  {

    if (local_index < 8 * 8) {
      int2 thread_offset = int2(local_index % 8u, local_index / 8u);
      int src_level = INPUT_LEVEL + 1;
      int dst_level = INPUT_LEVEL + 2;
      int2 src_image_size = level_size(src_level);
      int2 dst_image_size = level_size(dst_level);

      int2 src_shared_coord = thread_offset * 2;
      int2 kernel_size = kernel_size_from_input_size(src_image_size);
      int2 dst_coord = thread_offset + dst_tile_coord;

      bool within_bounds = true;
      if (use_bounds_check) {
        within_bounds = (uint(dst_coord.x) < uint(dst_image_size.x)) &&
                        (uint(dst_coord.y) < uint(dst_image_size.y));
      }
      if (within_bounds) {
        reduce_store_sample<true>(
            src_shared_coord, 0, kernel_size, dst_image_size, dst_coord, dst_level);
      }
    }
  }
};

template<enum TextureWriteFormat format, typename SharedStorage, typename InnerType>
[[local_size(LOCAL_SIZE_X)]] [[compute]]
void update_mipmaps([[global_invocation_id]] const uint3 global_id,
                    [[work_group_id]] const uint3 group_id,
                    [[local_invocation_id]] const uint3 local_index,
                    [[resource_table]] Resources<format, SharedStorage, InnerType> &srt)
{
  if (srt.num_levels == 1u) {
    int2 kernel_size = kernel_size_from_input_size(srt.level_size(INPUT_LEVEL));
    int2 dst_image_size = srt.level_size(INPUT_LEVEL + 1);
    int2 dst_coord = int2(int(global_id.x) % dst_image_size.x,
                          int(global_id.x) / dst_image_size.x);
    int2 src_coord = dst_coord * 2;

    if (dst_coord.y < dst_image_size.y) {
      srt.template reduce_store_sample<false>(
          src_coord, INPUT_LEVEL, kernel_size, dst_image_size, dst_coord, INPUT_LEVEL + 1);
    }
  }
  else {
    /* Handling two levels.
     * Assign a 8x8 tile of mip level inputLevel_ + 2 to this workgroup. */
    int level2 = INPUT_LEVEL + 2;
    int2 level2_size = srt.level_size(level2);
    int2 tile_count;
    tile_count.x = int(uint(level2_size.x + 7) / 8u);
    tile_count.y = int(uint(level2_size.y + 7) / 8u);
    int2 tile_index = int2(group_id.x % uint(tile_count.x), group_id.x / uint(tile_count.x));

    /* Determine if bounds checking is needed; this is only the case
     * for tiles at the right or bottom fringe that might be cut off
     * by the image border. Note that later, I use if statements rather
     * than passing use_bounds_check directly to convince the compiler
     * to inline everything. */
    bool use_bounds_check = tile_index.x >= tile_count.x - 1 || tile_index.y >= tile_count.y - 1;

    if (use_bounds_check) {
      /* Compute the tile in level inputLevel_ + 1 that's needed to
       * compute the above 8x8 tile. */
      srt.fill_intermediate_tile(local_index.x, tile_index * 2 * int2(8, 8), true);
      barrier();

      /* Compute the inputLevel_ + 2 tile of size 8x8, loading
       * inputs from shared memory. */
      srt.fill_last_tile(local_index.x, tile_index * int2(8, 8), true);
    }
    else {
      /* Same but without bounds checking. */
      srt.fill_intermediate_tile(local_index.x, tile_index * 2 * int2(8, 8), false);
      barrier();
      srt.fill_last_tile(local_index.x, tile_index * int2(8, 8), false);
    }
  }
}

template struct Shared<float>;
template struct Shared<float4>;

template struct Resources<UNORM_8, SharedUnorm, float>;
template struct Resources<UNORM_8_8_8_8, SharedSRGB, float4>;
template struct Resources<SFLOAT_16, Shared<float>, float>;
template struct Resources<SFLOAT_16_16_16_16, Shared<float4>, float4>;
template struct Resources<SFLOAT_32, Shared<float>, float>;
template struct Resources<SFLOAT_32_32_32_32, Shared<float4>, float4>;

template float Resources<UNORM_8, SharedUnorm, float>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float Resources<UNORM_8, SharedUnorm, float>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<UNORM_8_8_8_8, SharedSRGB, float4>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<UNORM_8_8_8_8, SharedSRGB, float4>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float Resources<SFLOAT_16, Shared<float>, float>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float Resources<SFLOAT_16, Shared<float>, float>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<SFLOAT_16_16_16_16, Shared<float4>, float4>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<SFLOAT_16_16_16_16, Shared<float4>, float4>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float Resources<SFLOAT_32, Shared<float>, float>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float Resources<SFLOAT_32, Shared<float>, float>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<SFLOAT_32_32_32_32, Shared<float4>, float4>::reduce_store_sample<true>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);
template float4 Resources<SFLOAT_32_32_32_32, Shared<float4>, float4>::reduce_store_sample<false>(
    int2 src_coord,
    int src_level,
    int2 kernel_size,
    int2 dst_image_size,
    int2 dst_coord,
    int dst_level);

template void update_mipmaps<UNORM_8, SharedUnorm, float>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<UNORM_8, SharedUnorm, float> &srt);
template void update_mipmaps<UNORM_8_8_8_8, SharedSRGB, float4>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<UNORM_8_8_8_8, SharedSRGB, float4> &srt);
template void update_mipmaps<SFLOAT_16, Shared<float>, float>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<SFLOAT_16, Shared<float>, float> &srt);
template void update_mipmaps<SFLOAT_16_16_16_16, Shared<float4>, float4>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<SFLOAT_16_16_16_16, Shared<float4>, float4> &srt);
template void update_mipmaps<SFLOAT_32, Shared<float>, float>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<SFLOAT_32, Shared<float>, float> &srt);
template void update_mipmaps<SFLOAT_32_32_32_32, Shared<float4>, float4>(
    const uint3 global_id,
    const uint3 group_id,
    const uint3 local_index,
    Resources<SFLOAT_32_32_32_32, Shared<float4>, float4> &srt);

}  // namespace builtin::mipmaps

PipelineCompute gpu_shader_2D_update_mipmaps_unorm_8(
    builtin::mipmaps::update_mipmaps<UNORM_8, builtin::mipmaps::SharedUnorm, float>,
    builtin::mipmaps::Resources<UNORM_8, builtin::mipmaps::SharedUnorm, float>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_unorm_8_layered(
    builtin::mipmaps::update_mipmaps<UNORM_8, builtin::mipmaps::SharedUnorm, float>,
    builtin::mipmaps::Resources<UNORM_8, builtin::mipmaps::SharedUnorm, float>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_unorm_8_8_8_8(
    builtin::mipmaps::update_mipmaps<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>,
    builtin::mipmaps::Resources<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_unorm_8_8_8_8_layered(
    builtin::mipmaps::update_mipmaps<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>,
    builtin::mipmaps::Resources<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_16(
    builtin::mipmaps::update_mipmaps<SFLOAT_16, builtin::mipmaps::Shared<float>, float>,
    builtin::mipmaps::Resources<SFLOAT_16, builtin::mipmaps::Shared<float>, float>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_16_layered(
    builtin::mipmaps::update_mipmaps<SFLOAT_16, builtin::mipmaps::Shared<float>, float>,
    builtin::mipmaps::Resources<SFLOAT_16, builtin::mipmaps::Shared<float>, float>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_16_16_16_16(
    builtin::mipmaps::update_mipmaps<SFLOAT_16_16_16_16, builtin::mipmaps::Shared<float4>, float4>,
    builtin::mipmaps::Resources<SFLOAT_16_16_16_16, builtin::mipmaps::Shared<float4>, float4>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_16_16_16_16_layered(
    builtin::mipmaps::update_mipmaps<SFLOAT_16_16_16_16, builtin::mipmaps::Shared<float4>, float4>,
    builtin::mipmaps::Resources<SFLOAT_16_16_16_16, builtin::mipmaps::Shared<float4>, float4>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_32(
    builtin::mipmaps::update_mipmaps<SFLOAT_32, builtin::mipmaps::Shared<float>, float>,
    builtin::mipmaps::Resources<SFLOAT_32, builtin::mipmaps::Shared<float>, float>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_32_layered(
    builtin::mipmaps::update_mipmaps<SFLOAT_32, builtin::mipmaps::Shared<float>, float>,
    builtin::mipmaps::Resources<SFLOAT_32, builtin::mipmaps::Shared<float>, float>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_32_32_32_32(
    builtin::mipmaps::update_mipmaps<SFLOAT_32_32_32_32, builtin::mipmaps::Shared<float4>, float4>,
    builtin::mipmaps::Resources<SFLOAT_32_32_32_32, builtin::mipmaps::Shared<float4>, float4>{
        .is_srgb_texture = false, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_sfloat_32_32_32_32_layered(
    builtin::mipmaps::update_mipmaps<SFLOAT_32_32_32_32, builtin::mipmaps::Shared<float4>, float4>,
    builtin::mipmaps::Resources<SFLOAT_32_32_32_32, builtin::mipmaps::Shared<float4>, float4>{
        .is_srgb_texture = false, .is_layered = true});
PipelineCompute gpu_shader_2D_update_mipmaps_srgba_8_8_8_8(
    builtin::mipmaps::update_mipmaps<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>,
    builtin::mipmaps::Resources<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>{
        .is_srgb_texture = true, .is_layered = false});
PipelineCompute gpu_shader_2D_update_mipmaps_srgba_8_8_8_8_layered(
    builtin::mipmaps::update_mipmaps<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>,
    builtin::mipmaps::Resources<UNORM_8_8_8_8, builtin::mipmaps::SharedSRGB, float4>{
        .is_srgb_texture = true, .is_layered = true});
