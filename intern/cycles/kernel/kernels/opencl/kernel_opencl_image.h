/*
 * Copyright 2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WITH_NANOVDB
/* Data type to replace `double` used in the NanoVDB headers. Cycles don't need doubles, and is
 * safer and more portable to never use double datatype on GPU.
 * Use a special structure, so that the following is true:
 * - No unnoticed implicit cast or mathematical operations used on scalar 64bit type
 *   (which rules out trick like using `uint64_t` as a drop-in replacement for double).
 * - Padding rules are matching exactly `double`
 *   (which rules out array of `uint8_t`). */
typedef struct ccl_vdb_double_t {
  uint64_t i;
} ccl_vdb_double_t;

#  define double ccl_vdb_double_t
#  include "nanovdb/CNanoVDB.h"
#  undef double
#endif

/* For OpenCL we do manual lookup and interpolation. */

ccl_device_inline ccl_global TextureInfo *kernel_tex_info(KernelGlobals *kg, uint id)
{
  const uint tex_offset = id
#define KERNEL_TEX(type, name) +1
#include "kernel/kernel_textures.h"
      ;

  return &((ccl_global TextureInfo *)kg->buffers[0])[tex_offset];
}

#define tex_fetch(type, info, index) \
  ((ccl_global type *)(kg->buffers[info->cl_buffer] + info->data))[(index)]

ccl_device_inline int svm_image_texture_wrap_periodic(int x, int width)
{
  x %= width;
  if (x < 0)
    x += width;
  return x;
}

ccl_device_inline int svm_image_texture_wrap_clamp(int x, int width)
{
  return clamp(x, 0, width - 1);
}

ccl_device_inline float4 svm_image_texture_read(
    KernelGlobals *kg, const ccl_global TextureInfo *info, void *acc, int x, int y, int z)
{
  const int data_offset = x + info->width * y + info->width * info->height * z;
  const int texture_type = info->data_type;

  /* Float4 */
  if (texture_type == IMAGE_DATA_TYPE_FLOAT4) {
    return tex_fetch(float4, info, data_offset);
  }
  /* Byte4 */
  else if (texture_type == IMAGE_DATA_TYPE_BYTE4) {
    uchar4 r = tex_fetch(uchar4, info, data_offset);
    float f = 1.0f / 255.0f;
    return make_float4(r.x * f, r.y * f, r.z * f, r.w * f);
  }
  /* Ushort4 */
  else if (texture_type == IMAGE_DATA_TYPE_USHORT4) {
    ushort4 r = tex_fetch(ushort4, info, data_offset);
    float f = 1.0f / 65535.f;
    return make_float4(r.x * f, r.y * f, r.z * f, r.w * f);
  }
  /* Float */
  else if (texture_type == IMAGE_DATA_TYPE_FLOAT) {
    float f = tex_fetch(float, info, data_offset);
    return make_float4(f, f, f, 1.0f);
  }
  /* UShort */
  else if (texture_type == IMAGE_DATA_TYPE_USHORT) {
    ushort r = tex_fetch(ushort, info, data_offset);
    float f = r * (1.0f / 65535.0f);
    return make_float4(f, f, f, 1.0f);
  }
#ifdef WITH_NANOVDB
  /* NanoVDB Float */
  else if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT) {
    cnanovdb_coord coord;
    coord.mVec[0] = x;
    coord.mVec[1] = y;
    coord.mVec[2] = z;
    float f = cnanovdb_readaccessor_getValueF((cnanovdb_readaccessor *)acc, &coord);
    return make_float4(f, f, f, 1.0f);
  }
  /* NanoVDB Float3 */
  else if (texture_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
    cnanovdb_coord coord;
    coord.mVec[0] = x;
    coord.mVec[1] = y;
    coord.mVec[2] = z;
    cnanovdb_Vec3F f = cnanovdb_readaccessor_getValueF3((cnanovdb_readaccessor *)acc, &coord);
    return make_float4(f.mVec[0], f.mVec[1], f.mVec[2], 1.0f);
  }
#endif
#ifdef __KERNEL_CL_KHR_FP16__
  /* Half and Half4 are optional in OpenCL */
  else if (texture_type == IMAGE_DATA_TYPE_HALF) {
    float f = tex_fetch(half, info, data_offset);
    return make_float4(f, f, f, 1.0f);
  }
  else if (texture_type == IMAGE_DATA_TYPE_HALF4) {
    half4 r = tex_fetch(half4, info, data_offset);
    return make_float4(r.x, r.y, r.z, r.w);
  }
#endif
  /* Byte */
  else {
    uchar r = tex_fetch(uchar, info, data_offset);
    float f = r * (1.0f / 255.0f);
    return make_float4(f, f, f, 1.0f);
  }
}

ccl_device_inline float4
svm_image_texture_read_2d(KernelGlobals *kg, int id, void *acc, int x, int y)
{
  const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

#ifdef WITH_NANOVDB
  if (info->data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT &&
      info->data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
#endif
    /* Wrap */
    if (info->extension == EXTENSION_REPEAT) {
      x = svm_image_texture_wrap_periodic(x, info->width);
      y = svm_image_texture_wrap_periodic(y, info->height);
    }
    else {
      x = svm_image_texture_wrap_clamp(x, info->width);
      y = svm_image_texture_wrap_clamp(y, info->height);
    }
#ifdef WITH_NANOVDB
  }
#endif

  return svm_image_texture_read(kg, info, acc, x, y, 0);
}

ccl_device_inline float4
svm_image_texture_read_3d(KernelGlobals *kg, int id, void *acc, int x, int y, int z)
{
  const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

#ifdef WITH_NANOVDB
  if (info->data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT &&
      info->data_type != IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
#endif
    /* Wrap */
    if (info->extension == EXTENSION_REPEAT) {
      x = svm_image_texture_wrap_periodic(x, info->width);
      y = svm_image_texture_wrap_periodic(y, info->height);
      z = svm_image_texture_wrap_periodic(z, info->depth);
    }
    else {
      x = svm_image_texture_wrap_clamp(x, info->width);
      y = svm_image_texture_wrap_clamp(y, info->height);
      z = svm_image_texture_wrap_clamp(z, info->depth);
    }
#ifdef WITH_NANOVDB
  }
#endif

  return svm_image_texture_read(kg, info, acc, x, y, z);
}

ccl_device_inline float svm_image_texture_frac(float x, int *ix)
{
  int i = float_to_int(x) - ((x < 0.0f) ? 1 : 0);
  *ix = i;
  return x - (float)i;
}

#define SET_CUBIC_SPLINE_WEIGHTS(u, t) \
  { \
    u[0] = (((-1.0f / 6.0f) * t + 0.5f) * t - 0.5f) * t + (1.0f / 6.0f); \
    u[1] = ((0.5f * t - 1.0f) * t) * t + (2.0f / 3.0f); \
    u[2] = ((-0.5f * t + 0.5f) * t + 0.5f) * t + (1.0f / 6.0f); \
    u[3] = (1.0f / 6.0f) * t * t * t; \
  } \
  (void)0

ccl_device float4 kernel_tex_image_interp(KernelGlobals *kg, int id, float x, float y)
{
  const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

  if (info->extension == EXTENSION_CLIP) {
    if (x < 0.0f || y < 0.0f || x > 1.0f || y > 1.0f) {
      return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  if (info->interpolation == INTERPOLATION_CLOSEST) {
    /* Closest interpolation. */
    int ix, iy;
    svm_image_texture_frac(x * info->width, &ix);
    svm_image_texture_frac(y * info->height, &iy);

    return svm_image_texture_read_2d(kg, id, NULL, ix, iy);
  }
  else if (info->interpolation == INTERPOLATION_LINEAR) {
    /* Bilinear interpolation. */
    int ix, iy;
    float tx = svm_image_texture_frac(x * info->width - 0.5f, &ix);
    float ty = svm_image_texture_frac(y * info->height - 0.5f, &iy);

    float4 r;
    r = (1.0f - ty) * (1.0f - tx) * svm_image_texture_read_2d(kg, id, NULL, ix, iy);
    r += (1.0f - ty) * tx * svm_image_texture_read_2d(kg, id, NULL, ix + 1, iy);
    r += ty * (1.0f - tx) * svm_image_texture_read_2d(kg, id, NULL, ix, iy + 1);
    r += ty * tx * svm_image_texture_read_2d(kg, id, NULL, ix + 1, iy + 1);
    return r;
  }
  else {
    /* Bicubic interpolation. */
    int ix, iy;
    float tx = svm_image_texture_frac(x * info->width - 0.5f, &ix);
    float ty = svm_image_texture_frac(y * info->height - 0.5f, &iy);

    float u[4], v[4];
    SET_CUBIC_SPLINE_WEIGHTS(u, tx);
    SET_CUBIC_SPLINE_WEIGHTS(v, ty);

    float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        float weight = u[x] * v[y];
        r += weight * svm_image_texture_read_2d(kg, id, NULL, ix + x - 1, iy + y - 1);
      }
    }
    return r;
  }
}

ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals *kg, int id, float3 P, int interp)
{
  const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

  if (info->use_transform_3d) {
    Transform tfm = info->transform_3d;
    P = transform_point(&tfm, P);
  }

  float x = P.x;
  float y = P.y;
  float z = P.z;

  uint interpolation = (interp == INTERPOLATION_NONE) ? info->interpolation : interp;

#ifdef WITH_NANOVDB
  cnanovdb_readaccessor acc;
  if (info->data_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT ||
      info->data_type == IMAGE_DATA_TYPE_NANOVDB_FLOAT3) {
    ccl_global cnanovdb_griddata *grid =
        (ccl_global cnanovdb_griddata *)(kg->buffers[info->cl_buffer] + info->data);
    cnanovdb_readaccessor_init(&acc, cnanovdb_treedata_rootF(cnanovdb_griddata_tree(grid)));
  }
  else {
    if (info->extension == EXTENSION_CLIP) {
      if (x < 0.0f || y < 0.0f || z < 0.0f || x > 1.0f || y > 1.0f || z > 1.0f) {
        return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
      }
    }

    x *= info->width;
    y *= info->height;
    z *= info->depth;
  }
#  define NANOVDB_ACCESS_POINTER &acc
#else
#  define NANOVDB_ACCESS_POINTER NULL
#endif

  if (interpolation == INTERPOLATION_CLOSEST) {
    /* Closest interpolation. */
    int ix, iy, iz;
    svm_image_texture_frac(x, &ix);
    svm_image_texture_frac(y, &iy);
    svm_image_texture_frac(z, &iz);

    return svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix, iy, iz);
  }
  else if (interpolation == INTERPOLATION_LINEAR) {
    /* Trilinear interpolation. */
    int ix, iy, iz;
    float tx = svm_image_texture_frac(x - 0.5f, &ix);
    float ty = svm_image_texture_frac(y - 0.5f, &iy);
    float tz = svm_image_texture_frac(z - 0.5f, &iz);

    float4 r;
    r = (1.0f - tz) * (1.0f - ty) * (1.0f - tx) *
        svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix, iy, iz);
    r += (1.0f - tz) * (1.0f - ty) * tx *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix + 1, iy, iz);
    r += (1.0f - tz) * ty * (1.0f - tx) *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix, iy + 1, iz);
    r += (1.0f - tz) * ty * tx *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix + 1, iy + 1, iz);

    r += tz * (1.0f - ty) * (1.0f - tx) *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix, iy, iz + 1);
    r += tz * (1.0f - ty) * tx *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix + 1, iy, iz + 1);
    r += tz * ty * (1.0f - tx) *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix, iy + 1, iz + 1);
    r += tz * ty * tx *
         svm_image_texture_read_3d(kg, id, NANOVDB_ACCESS_POINTER, ix + 1, iy + 1, iz + 1);
    return r;
  }
  else {
    /* Tricubic interpolation. */
    int ix, iy, iz;
    float tx = svm_image_texture_frac(x - 0.5f, &ix);
    float ty = svm_image_texture_frac(y - 0.5f, &iy);
    float tz = svm_image_texture_frac(z - 0.5f, &iz);

    float u[4], v[4], w[4];
    SET_CUBIC_SPLINE_WEIGHTS(u, tx);
    SET_CUBIC_SPLINE_WEIGHTS(v, ty);
    SET_CUBIC_SPLINE_WEIGHTS(w, tz);

    float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int z = 0; z < 4; z++) {
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          float weight = u[x] * v[y] * w[z];
          r += weight * svm_image_texture_read_3d(
                            kg, id, NANOVDB_ACCESS_POINTER, ix + x - 1, iy + y - 1, iz + z - 1);
        }
      }
    }
    return r;
  }
#undef NANOVDB_ACCESS_POINTER
}

#undef SET_CUBIC_SPLINE_WEIGHTS
