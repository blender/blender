/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#pragma once

#include "BLI_assert.h"
#include "BLI_compiler_compat.h"
#include "BLI_math_geom.h"
#include "GPU_common.hh"

struct GPUShader;

#define GPU_VERT_ATTR_MAX_LEN 16
#define GPU_VERT_ATTR_MAX_NAMES 6
#define GPU_VERT_ATTR_NAMES_BUF_LEN 256
#define GPU_VERT_FORMAT_MAX_NAMES 63 /* More than enough, actual max is ~30. */
/* Computed as GPU_VERT_ATTR_NAMES_BUF_LEN / 30 (actual max format name). */
#define GPU_MAX_SAFE_ATTR_NAME 12

enum GPUVertCompType {
  GPU_COMP_I8 = 0,
  GPU_COMP_U8,
  GPU_COMP_I16,
  GPU_COMP_U16,
  GPU_COMP_I32,
  GPU_COMP_U32,

  GPU_COMP_F32,

  GPU_COMP_I10,
  /* Warning! adjust GPUVertAttr if changing. */

  GPU_COMP_MAX
};

enum GPUVertFetchMode {
  GPU_FETCH_FLOAT = 0,
  GPU_FETCH_INT,
  GPU_FETCH_INT_TO_FLOAT_UNIT, /* 127 (ubyte) -> 0.5 (and so on for other int types) */
  GPU_FETCH_INT_TO_FLOAT,      /* 127 (any int type) -> 127.0 */
  /* Warning! adjust GPUVertAttr if changing. */
};

struct GPUVertAttr {
  /* GPUVertFetchMode */
  uint fetch_mode : 2;
  /* GPUVertCompType */
  uint comp_type : 3;
  /* 1 to 4 or 8 or 12 or 16 */
  uint comp_len : 5;
  /* size in bytes, 1 to 64 */
  uint size : 7;
  /* from beginning of vertex, in bytes */
  uint offset : 11;
  /* up to GPU_VERT_ATTR_MAX_NAMES */
  uint name_len : 3;
  uchar names[GPU_VERT_ATTR_MAX_NAMES];
};

BLI_STATIC_ASSERT(GPU_VERT_ATTR_NAMES_BUF_LEN <= 256,
                  "We use uchar as index inside the name buffer "
                  "so GPU_VERT_ATTR_NAMES_BUF_LEN needs to be "
                  "smaller than GPUVertFormat->name_offset and "
                  "GPUVertAttr->names maximum value");

struct GPUVertFormat {
  /** 0 to 16 (GPU_VERT_ATTR_MAX_LEN). */
  uint attr_len : 5;
  /** Total count of active vertex attribute names. (max GPU_VERT_FORMAT_MAX_NAMES) */
  uint name_len : 6;
  /** Stride in bytes, 1 to 1024. */
  uint stride : 11;
  /** Has the format been packed. */
  uint packed : 1;
  /** Current offset in names[]. */
  uint name_offset : 8;
  /** Store each attribute in one contiguous buffer region. */
  uint deinterleaved : 1;

  GPUVertAttr attrs[GPU_VERT_ATTR_MAX_LEN];
  char names[GPU_VERT_ATTR_NAMES_BUF_LEN];
};

void GPU_vertformat_clear(GPUVertFormat *);
void GPU_vertformat_copy(GPUVertFormat *dest, const GPUVertFormat *src);
void GPU_vertformat_from_shader(GPUVertFormat *format, const GPUShader *shader);

uint GPU_vertformat_attr_add(
    GPUVertFormat *, const char *name, GPUVertCompType, uint comp_len, GPUVertFetchMode);
void GPU_vertformat_alias_add(GPUVertFormat *, const char *alias);

/**
 * Makes vertex attribute from the next vertices to be accessible in the vertex shader.
 * For an attribute named "attr" you can access the next nth vertex using "attr{number}".
 * Use this function after specifying all the attributes in the format.
 *
 * NOTE: This does NOT work when using indexed rendering.
 * NOTE: Only works for first attribute name. (this limitation can be changed if needed)
 *
 * WARNING: this function creates a lot of aliases/attributes, make sure to keep the attribute
 * name short to avoid overflowing the name-buffer.
 */
void GPU_vertformat_multiload_enable(GPUVertFormat *format, int load_count);

/**
 * Make attribute layout non-interleaved.
 * Warning! This does not change data layout!
 * Use direct buffer access to fill the data.
 * This is for advanced usage.
 *
 * De-interleaved data means all attribute data for each attribute
 * is stored continuously like this:
 * 000011112222
 * instead of:
 * 012012012012
 *
 * \note This is per attribute de-interleaving, NOT per component.
 */
void GPU_vertformat_deinterleave(GPUVertFormat *format);

int GPU_vertformat_attr_id_get(const GPUVertFormat *, const char *name);

BLI_INLINE const char *GPU_vertformat_attr_name_get(const GPUVertFormat *format,
                                                    const GPUVertAttr *attr,
                                                    uint n_idx)
{
  return format->names + attr->names[n_idx];
}

/**
 * \warning Can only rename using a string with same character count.
 * \warning This removes all other aliases of this attribute.
 */
void GPU_vertformat_attr_rename(GPUVertFormat *format, int attr, const char *new_name);

/**
 * \warning Always add a prefix to the result of this function as
 * the generated string can start with a number and not be a valid attribute name.
 */
void GPU_vertformat_safe_attr_name(const char *attr_name, char *r_safe_name, uint max_len);

/* format conversion */

struct GPUPackedNormal {
  int x : 10;
  int y : 10;
  int z : 10;
  int w : 2; /* 0 by default, can manually set to { -2, -1, 0, 1 } */
};

struct GPUNormal {
  union {
    GPUPackedNormal low;
    short high[3];
  };
};

/* OpenGL ES packs in a different order as desktop GL but component conversion is the same.
 * Of the code here, only GPUPackedNormal needs to change. */

#define SIGNED_INT_10_MAX 511
#define SIGNED_INT_10_MIN -512

BLI_INLINE int clampi(int x, int min_allowed, int max_allowed)
{
#if TRUST_NO_ONE
  assert(min_allowed <= max_allowed);
#endif
  if (x < min_allowed) {
    return min_allowed;
  }
  else if (x > max_allowed) {
    return max_allowed;
  }
  else {
    return x;
  }
}

BLI_INLINE int gpu_convert_normalized_f32_to_i10(float x)
{
  int qx = x * 511.0f;
  return clampi(qx, SIGNED_INT_10_MIN, SIGNED_INT_10_MAX);
}

BLI_INLINE int gpu_convert_i16_to_i10(short x)
{
  /* 16-bit signed --> 10-bit signed */
  /* TODO: round? */
  return x >> 6;
}

BLI_INLINE GPUPackedNormal GPU_normal_convert_i10_v3(const float data[3])
{
  GPUPackedNormal n = {
      gpu_convert_normalized_f32_to_i10(data[0]),
      gpu_convert_normalized_f32_to_i10(data[1]),
      gpu_convert_normalized_f32_to_i10(data[2]),
  };
  return n;
}

BLI_INLINE GPUPackedNormal GPU_normal_convert_i10_s3(const short data[3])
{
  GPUPackedNormal n = {
      gpu_convert_i16_to_i10(data[0]),
      gpu_convert_i16_to_i10(data[1]),
      gpu_convert_i16_to_i10(data[2]),
  };
  return n;
}

BLI_INLINE void GPU_normal_convert_v3(GPUNormal *gpu_normal,
                                      const float data[3],
                                      const bool do_hq_normals)
{
  if (do_hq_normals) {
    normal_float_to_short_v3(gpu_normal->high, data);
  }
  else {
    gpu_normal->low = GPU_normal_convert_i10_v3(data);
  }
}
