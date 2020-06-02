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

#include "GPU_vertex_format.h"
#include "gpu_vertex_format_private.h"
#include <stddef.h>
#include <string.h>

#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "GPU_shader.h"
#include "gpu_shader_private.h"

#define PACK_DEBUG 0

#if PACK_DEBUG
#  include <stdio.h>
#endif

void GPU_vertformat_clear(GPUVertFormat *format)
{
#if TRUST_NO_ONE
  memset(format, 0, sizeof(GPUVertFormat));
#else
  format->attr_len = 0;
  format->packed = false;
  format->name_offset = 0;
  format->name_len = 0;

  for (uint i = 0; i < GPU_VERT_ATTR_MAX_LEN; i++) {
    format->attrs[i].name_len = 0;
  }
#endif
}

void GPU_vertformat_copy(GPUVertFormat *dest, const GPUVertFormat *src)
{
  /* copy regular struct fields */
  memcpy(dest, src, sizeof(GPUVertFormat));
}

static GLenum convert_comp_type_to_gl(GPUVertCompType type)
{
  static const GLenum table[] = {
      [GPU_COMP_I8] = GL_BYTE,
      [GPU_COMP_U8] = GL_UNSIGNED_BYTE,
      [GPU_COMP_I16] = GL_SHORT,
      [GPU_COMP_U16] = GL_UNSIGNED_SHORT,
      [GPU_COMP_I32] = GL_INT,
      [GPU_COMP_U32] = GL_UNSIGNED_INT,

      [GPU_COMP_F32] = GL_FLOAT,

      [GPU_COMP_I10] = GL_INT_2_10_10_10_REV,
  };
  return table[type];
}

static uint comp_sz(GPUVertCompType type)
{
#if TRUST_NO_ONE
  assert(type <= GPU_COMP_F32); /* other types have irregular sizes (not bytes) */
#endif
  const GLubyte sizes[] = {1, 1, 2, 2, 4, 4, 4};
  return sizes[type];
}

static uint attr_sz(const GPUVertAttr *a)
{
  if (a->comp_type == GPU_COMP_I10) {
    return 4; /* always packed as 10_10_10_2 */
  }
  return a->comp_len * comp_sz(a->comp_type);
}

static uint attr_align(const GPUVertAttr *a)
{
  if (a->comp_type == GPU_COMP_I10) {
    return 4; /* always packed as 10_10_10_2 */
  }
  uint c = comp_sz(a->comp_type);
  if (a->comp_len == 3 && c <= 2) {
    return 4 * c; /* AMD HW can't fetch these well, so pad it out (other vendors too?) */
  }
  else {
    return c; /* most fetches are ok if components are naturally aligned */
  }
}

uint vertex_buffer_size(const GPUVertFormat *format, uint vertex_len)
{
#if TRUST_NO_ONE
  assert(format->packed && format->stride > 0);
#endif
  return format->stride * vertex_len;
}

static uchar copy_attr_name(GPUVertFormat *format, const char *name)
{
  /* strncpy does 110% of what we need; let's do exactly 100% */
  uchar name_offset = format->name_offset;
  char *name_copy = format->names + name_offset;
  uint available = GPU_VERT_ATTR_NAMES_BUF_LEN - name_offset;
  bool terminated = false;

  for (uint i = 0; i < available; i++) {
    const char c = name[i];
    name_copy[i] = c;
    if (c == '\0') {
      terminated = true;
      format->name_offset += (i + 1);
      break;
    }
  }
#if TRUST_NO_ONE
  assert(terminated);
  assert(format->name_offset <= GPU_VERT_ATTR_NAMES_BUF_LEN);
#else
  (void)terminated;
#endif
  return name_offset;
}

uint GPU_vertformat_attr_add(GPUVertFormat *format,
                             const char *name,
                             GPUVertCompType comp_type,
                             uint comp_len,
                             GPUVertFetchMode fetch_mode)
{
#if TRUST_NO_ONE
  assert(format->name_len < GPU_VERT_FORMAT_MAX_NAMES); /* there's room for more */
  assert(format->attr_len < GPU_VERT_ATTR_MAX_LEN);     /* there's room for more */
  assert(!format->packed);                              /* packed means frozen/locked */
  assert((comp_len >= 1 && comp_len <= 4) || comp_len == 8 || comp_len == 12 || comp_len == 16);

  switch (comp_type) {
    case GPU_COMP_F32:
      /* float type can only kept as float */
      assert(fetch_mode == GPU_FETCH_FLOAT);
      break;
    case GPU_COMP_I10:
      /* 10_10_10 format intended for normals (xyz) or colors (rgb)
       * extra component packed.w can be manually set to { -2, -1, 0, 1 } */
      assert(comp_len == 3 || comp_len == 4);

      /* Not strictly required, may relax later. */
      assert(fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT);

      break;
    default:
      /* integer types can be kept as int or converted/normalized to float */
      assert(fetch_mode != GPU_FETCH_FLOAT);
      /* only support float matrices (see Batch_update_program_bindings) */
      assert(comp_len != 8 && comp_len != 12 && comp_len != 16);
  }
#endif
  format->name_len++; /* multiname support */

  const uint attr_id = format->attr_len++;
  GPUVertAttr *attr = &format->attrs[attr_id];

  attr->names[attr->name_len++] = copy_attr_name(format, name);
  attr->comp_type = comp_type;
  attr->gl_comp_type = convert_comp_type_to_gl(comp_type);
  attr->comp_len = (comp_type == GPU_COMP_I10) ?
                       4 :
                       comp_len; /* system needs 10_10_10_2 to be 4 or BGRA */
  attr->sz = attr_sz(attr);
  attr->offset = 0; /* offsets & stride are calculated later (during pack) */
  attr->fetch_mode = fetch_mode;

  return attr_id;
}

void GPU_vertformat_alias_add(GPUVertFormat *format, const char *alias)
{
  GPUVertAttr *attr = &format->attrs[format->attr_len - 1];
#if TRUST_NO_ONE
  assert(format->name_len < GPU_VERT_FORMAT_MAX_NAMES); /* there's room for more */
  assert(attr->name_len < GPU_VERT_ATTR_MAX_NAMES);
#endif
  format->name_len++; /* multiname support */
  attr->names[attr->name_len++] = copy_attr_name(format, alias);
}

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
 * */
void GPU_vertformat_multiload_enable(GPUVertFormat *format, int load_count)
{
  /* Sanity check. Maximum can be upgraded if needed. */
  BLI_assert(load_count > 1 && load_count < 5);
  /* We need a packed format because of format->stride. */
  if (!format->packed) {
    VertexFormat_pack(format);
  }

  BLI_assert((format->name_len + 1) * load_count < GPU_VERT_FORMAT_MAX_NAMES);
  BLI_assert(format->attr_len * load_count <= GPU_VERT_ATTR_MAX_LEN);
  BLI_assert(format->name_offset * load_count < GPU_VERT_ATTR_NAMES_BUF_LEN);

  const GPUVertAttr *attr = format->attrs;
  int attr_len = format->attr_len;
  for (int i = 0; i < attr_len; i++, attr++) {
    const char *attr_name = GPU_vertformat_attr_name_get(format, attr, 0);
    for (int j = 1; j < load_count; j++) {
      char load_name[64];
      BLI_snprintf(load_name, sizeof(load_name), "%s%d", attr_name, j);
      GPUVertAttr *dst_attr = &format->attrs[format->attr_len++];
      *dst_attr = *attr;

      dst_attr->names[0] = copy_attr_name(format, load_name);
      dst_attr->name_len = 1;
      dst_attr->offset += format->stride * j;
    }
  }
}

int GPU_vertformat_attr_id_get(const GPUVertFormat *format, const char *name)
{
  for (int i = 0; i < format->attr_len; i++) {
    const GPUVertAttr *attr = &format->attrs[i];
    for (int j = 0; j < attr->name_len; j++) {
      const char *attr_name = GPU_vertformat_attr_name_get(format, attr, j);
      if (STREQ(name, attr_name)) {
        return i;
      }
    }
  }
  return -1;
}

/* Encode 8 original bytes into 11 safe bytes. */
static void safe_bytes(char out[11], const char data[8])
{
  char safe_chars[63] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

  uint64_t in = *(uint64_t *)data;
  for (int i = 0; i < 11; i++) {
    /* Encoding in base63 */
    out[i] = safe_chars[in % 63lu];
    in /= 63lu;
  }
}

/* Warning: Always add a prefix to the result of this function as
 * the generated string can start with a number and not be a valid attribute name. */
void GPU_vertformat_safe_attr_name(const char *attr_name, char *r_safe_name, uint UNUSED(max_len))
{
  char data[8] = {0};
  uint len = strlen(attr_name);

  if (len > 8) {
    /* Start with the first 4 chars of the name; */
    for (int i = 0; i < 4; i++) {
      data[i] = attr_name[i];
    }
    /* We use a hash to identify each data layer based on its name.
     * NOTE: This is still prone to hash collision but the risks are very low.*/
    /* Start hashing after the first 2 chars. */
    *(uint *)&data[4] = BLI_ghashutil_strhash_p_murmur(attr_name + 4);
  }
  else {
    /* Copy the whole name. Collision is barely possible
     * (hash would have to be equal to the last 4 bytes). */
    for (int i = 0; i < 8 && attr_name[i] != '\0'; i++) {
      data[i] = attr_name[i];
    }
  }
  /* Convert to safe bytes characters. */
  safe_bytes(r_safe_name, data);
  /* End the string */
  r_safe_name[11] = '\0';

  BLI_assert(GPU_MAX_SAFE_ATTR_NAME >= 12);
#if 0 /* For debugging */
  printf("%s > %lx > %s\n", attr_name, *(uint64_t *)data, r_safe_name);
#endif
}

/* Make attribute layout non-interleaved.
 * Warning! This does not change data layout!
 * Use direct buffer access to fill the data.
 * This is for advanced usage.
 *
 * De-interleaved data means all attribute data for each attribute
 * is stored continuously like this:
 * 000011112222
 * instead of :
 * 012012012012
 *
 * Note this is per attribute de-interleaving, NOT per component.
 *  */
void GPU_vertformat_deinterleave(GPUVertFormat *format)
{
  /* Ideally we should change the stride and offset here. This would allow
   * us to use GPU_vertbuf_attr_set / GPU_vertbuf_attr_fill. But since
   * we use only 11 bits for attr->offset this limits the size of the
   * buffer considerably. So instead we do the conversion when creating
   * bindings in create_bindings(). */
  format->deinterleaved = true;
}

uint padding(uint offset, uint alignment)
{
  const uint mod = offset % alignment;
  return (mod == 0) ? 0 : (alignment - mod);
}

#if PACK_DEBUG
static void show_pack(uint a_idx, uint sz, uint pad)
{
  const char c = 'A' + a_idx;
  for (uint i = 0; i < pad; i++) {
    putchar('-');
  }
  for (uint i = 0; i < sz; i++) {
    putchar(c);
  }
}
#endif

void VertexFormat_pack(GPUVertFormat *format)
{
  /* For now, attributes are packed in the order they were added,
   * making sure each attribute is naturally aligned (add padding where necessary)
   * Later we can implement more efficient packing w/ reordering
   * (keep attribute ID order, adjust their offsets to reorder in buffer). */

  /* TODO: realloc just enough to hold the final combo string. And just enough to
   * hold used attributes, not all 16. */

  GPUVertAttr *a0 = &format->attrs[0];
  a0->offset = 0;
  uint offset = a0->sz;

#if PACK_DEBUG
  show_pack(0, a0->sz, 0);
#endif

  for (uint a_idx = 1; a_idx < format->attr_len; a_idx++) {
    GPUVertAttr *a = &format->attrs[a_idx];
    uint mid_padding = padding(offset, attr_align(a));
    offset += mid_padding;
    a->offset = offset;
    offset += a->sz;

#if PACK_DEBUG
    show_pack(a_idx, a->sz, mid_padding);
#endif
  }

  uint end_padding = padding(offset, attr_align(a0));

#if PACK_DEBUG
  show_pack(0, 0, end_padding);
  putchar('\n');
#endif
  format->stride = offset + end_padding;
  format->packed = true;
}

static uint calc_component_size(const GLenum gl_type)
{
  switch (gl_type) {
    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
      return 2;
    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
      return 3;
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT_VEC4:
      return 4;
    case GL_FLOAT_MAT3:
      return 9;
    case GL_FLOAT_MAT4:
      return 16;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
      return 6;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
      return 8;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
      return 12;
    default:
      return 1;
  }
}

static void get_fetch_mode_and_comp_type(int gl_type,
                                         GPUVertCompType *r_comp_type,
                                         GPUVertFetchMode *r_fetch_mode)
{
  switch (gl_type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT4x3:
      *r_comp_type = GPU_COMP_F32;
      *r_fetch_mode = GPU_FETCH_FLOAT;
      break;
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      *r_comp_type = GPU_COMP_I32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_VEC2:
    case GL_UNSIGNED_INT_VEC3:
    case GL_UNSIGNED_INT_VEC4:
      *r_comp_type = GPU_COMP_U32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    default:
      BLI_assert(0);
  }
}

void GPU_vertformat_from_shader(GPUVertFormat *format, const GPUShader *shader)
{
  GPU_vertformat_clear(format);
  GPUVertAttr *attr = &format->attrs[0];

  GLint attr_len;
  glGetProgramiv(shader->program, GL_ACTIVE_ATTRIBUTES, &attr_len);

  for (int i = 0; i < attr_len; i++) {
    char name[256];
    GLenum gl_type;
    GLint size;
    glGetActiveAttrib(shader->program, i, sizeof(name), NULL, &size, &gl_type, name);

    /* Ignore OpenGL names like `gl_BaseInstanceARB`, `gl_InstanceID` and `gl_VertexID`. */
    if (glGetAttribLocation(shader->program, name) == -1) {
      continue;
    }

    format->name_len++; /* multiname support */
    format->attr_len++;

    GPUVertCompType comp_type;
    GPUVertFetchMode fetch_mode;
    get_fetch_mode_and_comp_type(gl_type, &comp_type, &fetch_mode);

    attr->names[attr->name_len++] = copy_attr_name(format, name);
    attr->offset = 0; /* offsets & stride are calculated later (during pack) */
    attr->comp_len = calc_component_size(gl_type) * size;
    attr->sz = attr->comp_len * 4;
    attr->fetch_mode = fetch_mode;
    attr->comp_type = comp_type;
    attr->gl_comp_type = convert_comp_type_to_gl(comp_type);
    attr += 1;
  }
}
