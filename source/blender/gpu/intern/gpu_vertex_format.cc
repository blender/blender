/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU vertex format
 */

#include "GPU_vertex_format.h"
#include "GPU_capabilities.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_format_private.h"

#include <cstddef>
#include <cstring>

#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#define PACK_DEBUG 0

#if PACK_DEBUG
#  include <stdio.h>
#endif

using namespace blender::gpu;
using namespace blender::gpu::shader;

void GPU_vertformat_clear(GPUVertFormat *format)
{
#if TRUST_NO_ONE
  memset(format, 0, sizeof(GPUVertFormat));
#else
  format->attr_len = 0;
  format->packed = false;
  format->name_offset = 0;
  format->name_len = 0;
  format->deinterleaved = false;

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

static uint comp_size(GPUVertCompType type)
{
#if TRUST_NO_ONE
  assert(type <= GPU_COMP_F32); /* other types have irregular sizes (not bytes) */
#endif
  const uint sizes[] = {1, 1, 2, 2, 4, 4, 4};
  return sizes[type];
}

static uint attr_size(const GPUVertAttr *a)
{
  if (a->comp_type == GPU_COMP_I10) {
    return 4; /* always packed as 10_10_10_2 */
  }
  return a->comp_len * comp_size(static_cast<GPUVertCompType>(a->comp_type));
}

static uint attr_align(const GPUVertAttr *a, uint minimum_stride)
{
  if (a->comp_type == GPU_COMP_I10) {
    return 4; /* always packed as 10_10_10_2 */
  }
  uint c = comp_size(static_cast<GPUVertCompType>(a->comp_type));
  if (a->comp_len == 3 && c <= 2) {
    return 4 * c; /* AMD HW can't fetch these well, so pad it out (other vendors too?) */
  }

  /* Most fetches are ok if components are naturally aligned.
   * However, in Metal,the minimum supported per-vertex stride is 4,
   * so we must query the GPU and pad out the size accordingly. */
  return max_ii(minimum_stride, c);
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
  /* `strncpy` does 110% of what we need; let's do exactly 100% */
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
      assert(ELEM(comp_len, 3, 4));

      /* Not strictly required, may relax later. */
      assert(fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT);

      break;
    default:
      /* integer types can be kept as int or converted/normalized to float */
      assert(fetch_mode != GPU_FETCH_FLOAT);
      /* only support float matrices (see Batch_update_program_bindings) */
      assert(!ELEM(comp_len, 8, 12, 16));
  }
#endif
  format->name_len++; /* Multi-name support. */

  const uint attr_id = format->attr_len++;
  GPUVertAttr *attr = &format->attrs[attr_id];

  attr->names[attr->name_len++] = copy_attr_name(format, name);
  attr->comp_type = comp_type;
  attr->comp_len = (comp_type == GPU_COMP_I10) ?
                       4 :
                       comp_len; /* system needs 10_10_10_2 to be 4 or BGRA */
  attr->size = attr_size(attr);
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
  format->name_len++; /* Multi-name support. */
  attr->names[attr->name_len++] = copy_attr_name(format, alias);
}

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
      char load_name[68 /* MAX_CUSTOMDATA_LAYER_NAME */];
      SNPRINTF(load_name, "%s%d", attr_name, j);
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

void GPU_vertformat_attr_rename(GPUVertFormat *format, int attr_id, const char *new_name)
{
  BLI_assert(attr_id > -1 && attr_id < format->attr_len);
  GPUVertAttr *attr = &format->attrs[attr_id];
  char *attr_name = (char *)GPU_vertformat_attr_name_get(format, attr, 0);
  BLI_assert(strlen(attr_name) == strlen(new_name));
  int i = 0;
  while (attr_name[i] != '\0') {
    attr_name[i] = new_name[i];
    i++;
  }
  attr->name_len = 1;
}

/* Encode 8 original bytes into 11 safe bytes. */
static void safe_bytes(char out[11], const char data[8])
{
  char safe_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  uint64_t in = *(uint64_t *)data;
  for (int i = 0; i < 11; i++) {
    out[i] = safe_chars[in % 62lu];
    in /= 62lu;
  }
}

void GPU_vertformat_safe_attr_name(const char *attr_name, char *r_safe_name, uint /*max_len*/)
{
  char data[8] = {0};
  uint len = strlen(attr_name);

  if (len > 8) {
    /* Start with the first 4 chars of the name; */
    for (int i = 0; i < 4; i++) {
      data[i] = attr_name[i];
    }
    /* We use a hash to identify each data layer based on its name.
     * NOTE: This is still prone to hash collision but the risks are very low. */
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
static void show_pack(uint a_idx, uint size, uint pad)
{
  const char c = 'A' + a_idx;
  for (uint i = 0; i < pad; i++) {
    putchar('-');
  }
  for (uint i = 0; i < size; i++) {
    putchar(c);
  }
}
#endif

static void VertexFormat_pack_impl(GPUVertFormat *format, uint minimum_stride)
{
  GPUVertAttr *a0 = &format->attrs[0];
  a0->offset = 0;
  uint offset = a0->size;

#if PACK_DEBUG
  show_pack(0, a0->size, 0);
#endif

  for (uint a_idx = 1; a_idx < format->attr_len; a_idx++) {
    GPUVertAttr *a = &format->attrs[a_idx];
    uint mid_padding = padding(offset, attr_align(a, minimum_stride));
    offset += mid_padding;
    a->offset = offset;
    offset += a->size;

#if PACK_DEBUG
    show_pack(a_idx, a->size, mid_padding);
#endif
  }

  uint end_padding = padding(offset, attr_align(a0, minimum_stride));

#if PACK_DEBUG
  show_pack(0, 0, end_padding);
  putchar('\n');
#endif
  format->stride = offset + end_padding;
  format->packed = true;
}

void VertexFormat_pack(GPUVertFormat *format)
{
  /* Perform standard vertex packing, ensuring vertex format satisfies
   * minimum stride requirements for vertex assembly. */
  VertexFormat_pack_impl(format, GPU_minimum_per_vertex_stride());
}

void VertexFormat_texture_buffer_pack(GPUVertFormat *format)
{
  /* Validates packing for vertex formats used with texture buffers.
   * In these cases, there must only be a single vertex attribute.
   * This attribute should be tightly packed without padding, to ensure
   * it aligns with the backing texture data format, skipping
   * minimum per-vertex stride, which mandates 4-byte alignment in Metal.
   * This additional alignment padding caused smaller data types, e.g. U16,
   * to mis-align. */
  for (int i = 0; i < format->attr_len; i++) {
    /* The buffer texture setup uses the first attribute for type and size.
     * Make sure all attributes use the same size. */
    BLI_assert_msg(format->attrs[i].size == format->attrs[0].size,
                   "Texture buffer mode should only use a attributes with the same size.");
  }

  /* Pack vertex format without minimum stride, as this is not required by texture buffers. */
  VertexFormat_pack_impl(format, 1);
}

static uint component_size_get(const Type gpu_type)
{
  switch (gpu_type) {
    case Type::VEC2:
    case Type::IVEC2:
    case Type::UVEC2:
      return 2;
    case Type::VEC3:
    case Type::IVEC3:
    case Type::UVEC3:
      return 3;
    case Type::VEC4:
    case Type::IVEC4:
    case Type::UVEC4:
      return 4;
    case Type::MAT3:
      return 12;
    case Type::MAT4:
      return 16;
    default:
      return 1;
  }
}

static void recommended_fetch_mode_and_comp_type(Type gpu_type,
                                                 GPUVertCompType *r_comp_type,
                                                 GPUVertFetchMode *r_fetch_mode)
{
  switch (gpu_type) {
    case Type::FLOAT:
    case Type::VEC2:
    case Type::VEC3:
    case Type::VEC4:
    case Type::MAT3:
    case Type::MAT4:
      *r_comp_type = GPU_COMP_F32;
      *r_fetch_mode = GPU_FETCH_FLOAT;
      break;
    case Type::INT:
    case Type::IVEC2:
    case Type::IVEC3:
    case Type::IVEC4:
      *r_comp_type = GPU_COMP_I32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    case Type::UINT:
    case Type::UVEC2:
    case Type::UVEC3:
    case Type::UVEC4:
      *r_comp_type = GPU_COMP_U32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    default:
      BLI_assert(0);
  }
}

void GPU_vertformat_from_shader(GPUVertFormat *format, const GPUShader *gpushader)
{
  GPU_vertformat_clear(format);

  uint attr_len = GPU_shader_get_attribute_len(gpushader);
  int location_test = 0, attrs_added = 0;
  while (attrs_added < attr_len) {
    char name[256];
    Type gpu_type;
    if (!GPU_shader_get_attribute_info(gpushader, location_test++, name, (int *)&gpu_type)) {
      continue;
    }

    GPUVertCompType comp_type;
    GPUVertFetchMode fetch_mode;
    recommended_fetch_mode_and_comp_type(gpu_type, &comp_type, &fetch_mode);

    int comp_len = component_size_get(gpu_type);

    GPU_vertformat_attr_add(format, name, comp_type, comp_len, fetch_mode);
    attrs_added++;
  }
}
