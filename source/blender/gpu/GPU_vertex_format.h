/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#ifndef __GPU_VERTEX_FORMAT_H__
#define __GPU_VERTEX_FORMAT_H__

#include "GPU_common.h"
#include "BLI_compiler_compat.h"
#include "BLI_assert.h"

#define GPU_VERT_ATTR_MAX_LEN 16
#define GPU_VERT_ATTR_MAX_NAMES 4
#define GPU_VERT_ATTR_NAME_AVERAGE_LEN 11
#define GPU_VERT_ATTR_NAMES_BUF_LEN ((GPU_VERT_ATTR_NAME_AVERAGE_LEN + 1) * GPU_VERT_ATTR_MAX_LEN)

typedef enum {
  GPU_COMP_I8,
  GPU_COMP_U8,
  GPU_COMP_I16,
  GPU_COMP_U16,
  GPU_COMP_I32,
  GPU_COMP_U32,

  GPU_COMP_F32,

  GPU_COMP_I10,
} GPUVertCompType;

typedef enum {
  GPU_FETCH_FLOAT,
  GPU_FETCH_INT,
  GPU_FETCH_INT_TO_FLOAT_UNIT, /* 127 (ubyte) -> 0.5 (and so on for other int types) */
  GPU_FETCH_INT_TO_FLOAT,      /* 127 (any int type) -> 127.0 */
} GPUVertFetchMode;

typedef struct GPUVertAttr {
  GPUVertFetchMode fetch_mode : 2;
  GPUVertCompType comp_type : 3;
  /* 1 to 4 or 8 or 12 or 16 */
  uint comp_len : 5;
  /* size in bytes, 1 to 64 */
  uint sz : 7;
  /* from beginning of vertex, in bytes */
  uint offset : 11;
  /* up to GPU_VERT_ATTR_MAX_NAMES */
  uint name_len : 3;
  uint gl_comp_type;
  /* -- 8 Bytes -- */
  uchar names[GPU_VERT_ATTR_MAX_NAMES];
} GPUVertAttr;

BLI_STATIC_ASSERT(GPU_VERT_ATTR_NAMES_BUF_LEN <= 256,
                  "We use uchar as index inside the name buffer "
                  "so GPU_VERT_ATTR_NAMES_BUF_LEN needs to be be "
                  "smaller than GPUVertFormat->name_offset and "
                  "GPUVertAttr->names maximum value");

typedef struct GPUVertFormat {
  /** 0 to 16 (GPU_VERT_ATTR_MAX_LEN). */
  uint attr_len : 5;
  /** Total count of active vertex attribute. */
  uint name_len : 5;
  /** Stride in bytes, 1 to 1024. */
  uint stride : 11;
  /** Has the format been packed. */
  uint packed : 1;
  /** Current offset in names[]. */
  uint name_offset : 8;

  GPUVertAttr attrs[GPU_VERT_ATTR_MAX_LEN];
  char names[GPU_VERT_ATTR_NAMES_BUF_LEN];
} GPUVertFormat;

struct GPUShaderInterface;

void GPU_vertformat_clear(GPUVertFormat *);
void GPU_vertformat_copy(GPUVertFormat *dest, const GPUVertFormat *src);
void GPU_vertformat_from_interface(GPUVertFormat *format,
                                   const struct GPUShaderInterface *shaderface);

uint GPU_vertformat_attr_add(
    GPUVertFormat *, const char *name, GPUVertCompType, uint comp_len, GPUVertFetchMode);
void GPU_vertformat_alias_add(GPUVertFormat *, const char *alias);

int GPU_vertformat_attr_id_get(const GPUVertFormat *, const char *name);

BLI_INLINE const char *GPU_vertformat_attr_name_get(const GPUVertFormat *format,
                                                    const GPUVertAttr *attr,
                                                    uint n_idx)
{
  return format->names + attr->names[n_idx];
}

/* format conversion */

typedef struct GPUPackedNormal {
  int x : 10;
  int y : 10;
  int z : 10;
  int w : 2; /* 0 by default, can manually set to { -2, -1, 0, 1 } */
} GPUPackedNormal;

GPUPackedNormal GPU_normal_convert_i10_v3(const float data[3]);
GPUPackedNormal GPU_normal_convert_i10_s3(const short data[3]);

#endif /* __GPU_VERTEX_FORMAT_H__ */
