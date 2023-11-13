/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Utilities for stand-alone `makesdna.cc` and Blender to share.
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_assert.h"
#include "BLI_ghash.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_memarena.h"

#include "dna_utils.h"

/* -------------------------------------------------------------------- */
/** \name Struct Member Evaluation
 * \{ */

int DNA_elem_array_size(const char *str)
{
  int result = 1;
  int current = 0;
  while (true) {
    char c = *str++;
    switch (c) {
      case '\0':
        return result;
      case '[':
        current = 0;
        break;
      case ']':
        result *= current;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        current = current * 10 + (c - '0');
        break;
      default:
        break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct Member Manipulation
 * \{ */

static bool is_identifier(const char c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          (c == '_'));
}

uint DNA_elem_id_offset_start(const char *elem_full)
{
  uint elem_full_offset = 0;
  while (!is_identifier(elem_full[elem_full_offset])) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

uint DNA_elem_id_offset_end(const char *elem_full)
{
  uint elem_full_offset = 0;
  while (is_identifier(elem_full[elem_full_offset])) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

uint DNA_elem_id_strip_copy(char *elem_dst, const char *elem_src)
{
  const uint elem_src_offset = DNA_elem_id_offset_start(elem_src);
  const char *elem_src_trim = elem_src + elem_src_offset;
  const uint elem_src_trim_len = DNA_elem_id_offset_end(elem_src_trim);
  memcpy(elem_dst, elem_src_trim, elem_src_trim_len);
  elem_dst[elem_src_trim_len] = '\0';
  return elem_src_trim_len;
}

uint DNA_elem_id_strip(char *elem)
{
  const uint elem_offset = DNA_elem_id_offset_start(elem);
  const char *elem_trim = elem + elem_offset;
  const uint elem_trim_len = DNA_elem_id_offset_end(elem_trim);
  memmove(elem, elem_trim, elem_trim_len);
  elem[elem_trim_len] = '\0';
  return elem_trim_len;
}

bool DNA_elem_id_match(const char *elem_search,
                       const int elem_search_len,
                       const char *elem_full,
                       uint *r_elem_full_offset)
{
  BLI_assert(strlen(elem_search) == elem_search_len);
  const uint elem_full_offset = DNA_elem_id_offset_start(elem_full);
  const char *elem_full_trim = elem_full + elem_full_offset;
  if (strncmp(elem_search, elem_full_trim, elem_search_len) == 0) {
    const char c = elem_full_trim[elem_search_len];
    if (c == '\0' || !is_identifier(c)) {
      *r_elem_full_offset = elem_full_offset;
      return true;
    }
  }
  return false;
}

char *DNA_elem_id_rename(MemArena *mem_arena,
                         const char *elem_src,
                         const int elem_src_len,
                         const char *elem_dst,
                         const int elem_dst_len,
                         const char *elem_src_full,
                         const int elem_src_full_len,
                         const uint elem_src_full_offset_len)
{
  BLI_assert(strlen(elem_src) == elem_src_len);
  BLI_assert(strlen(elem_dst) == elem_dst_len);
  BLI_assert(strlen(elem_src_full) == elem_src_full_len);
  BLI_assert(DNA_elem_id_offset_start(elem_src_full) == elem_src_full_offset_len);
  UNUSED_VARS_NDEBUG(elem_src);

  const int elem_final_len = (elem_src_full_len - elem_src_len) + elem_dst_len;
  char *elem_dst_full = static_cast<char *>(BLI_memarena_alloc(mem_arena, elem_final_len + 1));
  uint i = 0;
  if (elem_src_full_offset_len != 0) {
    memcpy(elem_dst_full, elem_src_full, elem_src_full_offset_len);
    i = elem_src_full_offset_len;
  }
  memcpy(&elem_dst_full[i], elem_dst, elem_dst_len + 1);
  i += elem_dst_len;
  uint elem_src_full_offset_end = elem_src_full_offset_len + elem_src_len;
  if (elem_src_full[elem_src_full_offset_end] != '\0') {
    const int elem_full_tail_len = (elem_src_full_len - elem_src_full_offset_end);
    memcpy(&elem_dst_full[i], &elem_src_full[elem_src_full_offset_end], elem_full_tail_len + 1);
    i += elem_full_tail_len;
  }
  BLI_assert((strlen(elem_dst_full) == elem_final_len) && (i == elem_final_len));
  return elem_dst_full;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Versioning
 * \{ */

static uint strhash_pair_p(const void *ptr)
{
  const char *const *pair = static_cast<const char *const *>(ptr);
  return (BLI_ghashutil_strhash_p(pair[0]) ^ BLI_ghashutil_strhash_p(pair[1]));
}

static bool strhash_pair_cmp(const void *a, const void *b)
{
  const char *const *pair_a = static_cast<const char *const *>(a);
  const char *const *pair_b = static_cast<const char *const *>(b);
  return (STREQ(pair_a[0], pair_b[0]) && STREQ(pair_a[1], pair_b[1])) ? false : true;
}

void DNA_alias_maps(enum eDNA_RenameDir version_dir, GHash **r_struct_map, GHash **r_elem_map)
{
  GHash *struct_map_local = nullptr;
  if (r_struct_map) {
    const char *data[][2] = {
#define DNA_STRUCT_RENAME(old, new) {#old, #new},
#define DNA_STRUCT_RENAME_ELEM(struct_name, old, new)
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_ELEM
    };

    int elem_key, elem_val;
    if (version_dir == DNA_RENAME_ALIAS_FROM_STATIC) {
      elem_key = 0;
      elem_val = 1;
    }
    else {
      elem_key = 1;
      elem_val = 0;
    }
    GHash *struct_map = BLI_ghash_str_new_ex(__func__, ARRAY_SIZE(data));
    for (int i = 0; i < ARRAY_SIZE(data); i++) {
      BLI_ghash_insert(struct_map, (void *)data[i][elem_key], (void *)data[i][elem_val]);
    }

    if (version_dir == DNA_RENAME_STATIC_FROM_ALIAS) {
      const char *renames[][2] = {
          {"uint8_t", "uchar"},
          {"int16_t", "short"},
          {"uint16_t", "ushort"},
          {"int32_t", "int"},
          {"uint32_t", "int"},
      };
      for (int i = 0; i < ARRAY_SIZE(renames); i++) {
        BLI_ghash_insert(struct_map, (void *)renames[i][0], (void *)renames[i][1]);
      }
    }

    *r_struct_map = struct_map;

    /* We know the direction of this, for local use. */
    struct_map_local = BLI_ghash_str_new_ex(__func__, ARRAY_SIZE(data));
    for (int i = 0; i < ARRAY_SIZE(data); i++) {
      BLI_ghash_insert(struct_map_local, (void *)data[i][1], (void *)data[i][0]);
    }
  }

  if (r_elem_map != nullptr) {
    const char *data[][3] = {
#define DNA_STRUCT_RENAME(old, new)
#define DNA_STRUCT_RENAME_ELEM(struct_name, old, new) {#struct_name, #old, #new},
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_ELEM
    };

    int elem_key, elem_val;
    if (version_dir == DNA_RENAME_ALIAS_FROM_STATIC) {
      elem_key = 1;
      elem_val = 2;
    }
    else {
      elem_key = 2;
      elem_val = 1;
    }
    GHash *elem_map = BLI_ghash_new_ex(
        strhash_pair_p, strhash_pair_cmp, __func__, ARRAY_SIZE(data));
    for (int i = 0; i < ARRAY_SIZE(data); i++) {
      const char **str_pair = static_cast<const char **>(
          MEM_mallocN(sizeof(char *) * 2, __func__));
      str_pair[0] = static_cast<const char *>(
          BLI_ghash_lookup_default(struct_map_local, data[i][0], (void *)data[i][0]));
      str_pair[1] = data[i][elem_key];
      BLI_ghash_insert(elem_map, (void *)str_pair, (void *)data[i][elem_val]);
    }
    *r_elem_map = elem_map;
  }

  if (struct_map_local) {
    BLI_ghash_free(struct_map_local, nullptr, nullptr);
  }
}

#undef DNA_MAKESDNA

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct Name Legacy Hack
 * \{ */

const char *DNA_struct_rename_legacy_hack_static_from_alias(const char *name)
{
  /* Only keep this for compatibility: *NEVER ADD NEW STRINGS HERE*.
   *
   * The renaming here isn't complete, references to the old struct names
   * are still included in DNA, now fixing these struct names properly
   * breaks forward compatibility. Leave these as-is, but don't add to them!
   * See D4342#98780. */

  /* 'bScreen' replaces the old IrisGL 'Screen' struct */
  if (STREQ("bScreen", name)) {
    return "Screen";
  }
  /* Groups renamed to collections in 2.8 */
  if (STREQ("Collection", name)) {
    return "Group";
  }
  if (STREQ("CollectionObject", name)) {
    return "GroupObject";
  }
  return name;
}

const char *DNA_struct_rename_legacy_hack_alias_from_static(const char *name)
{
  /* 'bScreen' replaces the old IrisGL 'Screen' struct */
  if (STREQ("Screen", name)) {
    return "bScreen";
  }
  /* Groups renamed to collections in 2.8 */
  if (STREQ("Group", name)) {
    return "Collection";
  }
  if (STREQ("GroupObject", name)) {
    return "CollectionObject";
  }
  return name;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal helpers for C++
 * \{ */

extern "C" void _DNA_internal_memcpy(void *dst, const void *src, size_t size);
extern "C" void _DNA_internal_memcpy(void *dst, const void *src, const size_t size)
{
  memcpy(dst, src, size);
}

extern "C" void _DNA_internal_memzero(void *dst, size_t size);
extern "C" void _DNA_internal_memzero(void *dst, const size_t size)
{
  memset(dst, 0, size);
}

extern "C" void _DNA_internal_swap(void *a, void *b, size_t size);
extern "C" void _DNA_internal_swap(void *a, void *b, const size_t size)
{
  void *tmp = alloca(size);
  memcpy(tmp, a, size);
  memcpy(a, b, size);
  memcpy(b, tmp, size);
}

/** \} */
