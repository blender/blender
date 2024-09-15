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

int DNA_member_array_num(const char *str)
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

uint DNA_member_id_offset_start(const char *member_full)
{
  uint elem_full_offset = 0;
  /* NOTE(@ideasman42): checking nil is needed for invalid names such as `*`,
   * these were written by older versions of Blender (v2.66).
   * In this case the "name" part will be an empty string.
   * The member cannot be used, this just prevents a crash. */
  while (!is_identifier(member_full[elem_full_offset]) && member_full[elem_full_offset]) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

uint DNA_member_id_offset_end(const char *member_full_trimmed)
{
  uint elem_full_offset = 0;
  while (is_identifier(member_full_trimmed[elem_full_offset])) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

uint DNA_member_id_strip_copy(char *member_id_dst, const char *member_full_src)
{
  const uint member_src_offset = DNA_member_id_offset_start(member_full_src);
  const char *member_src_trimmed = member_full_src + member_src_offset;
  const uint member_src_trimmed_len = DNA_member_id_offset_end(member_src_trimmed);
  memcpy(member_id_dst, member_src_trimmed, member_src_trimmed_len);
  member_id_dst[member_src_trimmed_len] = '\0';
  return member_src_trimmed_len;
}

uint DNA_member_id_strip(char *member)
{
  const uint member_offset = DNA_member_id_offset_start(member);
  const char *member_trimmed = member + member_offset;
  const uint member_trimmed_len = DNA_member_id_offset_end(member_trimmed);
  memmove(member, member_trimmed, member_trimmed_len);
  member[member_trimmed_len] = '\0';
  return member_trimmed_len;
}

bool DNA_member_id_match(const char *member_id,
                         const int member_id_len,
                         const char *member_full,
                         uint *r_member_full_offset)
{
  BLI_assert(strlen(member_id) == member_id_len);
  const uint elem_full_offset = DNA_member_id_offset_start(member_full);
  const char *elem_full_trim = member_full + elem_full_offset;
  if (strncmp(member_id, elem_full_trim, member_id_len) == 0) {
    const char c = elem_full_trim[member_id_len];
    if (c == '\0' || !is_identifier(c)) {
      *r_member_full_offset = elem_full_offset;
      return true;
    }
  }
  return false;
}

char *DNA_member_id_rename(MemArena *mem_arena,
                           const char *member_id_src,
                           const int member_id_src_len,
                           const char *member_id_dst,
                           const int member_id_dst_len,
                           const char *member_full_src,
                           const int member_full_src_len,
                           const uint member_full_src_offset_len)
{
  BLI_assert(strlen(member_id_src) == member_id_src_len);
  BLI_assert(strlen(member_id_dst) == member_id_dst_len);
  BLI_assert(strlen(member_full_src) == member_full_src_len);
  BLI_assert(DNA_member_id_offset_start(member_full_src) == member_full_src_offset_len);
  UNUSED_VARS_NDEBUG(member_id_src);

  const int member_full_dst_len = (member_full_src_len - member_id_src_len) + member_id_dst_len;
  char *member_full_dst = static_cast<char *>(
      BLI_memarena_alloc(mem_arena, member_full_dst_len + 1));
  uint i = 0;
  if (member_full_src_offset_len != 0) {
    memcpy(member_full_dst, member_full_src, member_full_src_offset_len);
    i = member_full_src_offset_len;
  }
  memcpy(&member_full_dst[i], member_id_dst, member_id_dst_len + 1);
  i += member_id_dst_len;
  const uint member_full_src_offset_end = member_full_src_offset_len + member_id_src_len;
  BLI_assert(DNA_member_id_offset_end(member_full_src + member_full_src_offset_len) ==
             (member_full_src_offset_end - member_full_src_offset_len));
  if (member_full_src[member_full_src_offset_end] != '\0') {
    const int member_full_tail_len = (member_full_src_len - member_full_src_offset_end);
    memcpy(&member_full_dst[i],
           &member_full_src[member_full_src_offset_end],
           member_full_tail_len + 1);
    i += member_full_tail_len;
  }
  BLI_assert((strlen(member_full_dst) == member_full_dst_len) && (i == member_full_dst_len));
  UNUSED_VARS_NDEBUG(i);
  return member_full_dst;
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

void DNA_alias_maps(enum eDNA_RenameDir version_dir, GHash **r_type_map, GHash **r_member_map)
{
  GHash *type_map_local = nullptr;
  if (r_type_map) {
    const char *type_data[][2] = {
#define DNA_STRUCT_RENAME(old, new) {#old, #new},
#define DNA_STRUCT_RENAME_MEMBER(struct_name, old, new)
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
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
    GHash *type_map = BLI_ghash_str_new_ex(__func__, ARRAY_SIZE(type_data));
    for (int i = 0; i < ARRAY_SIZE(type_data); i++) {
      BLI_ghash_insert(type_map, (void *)type_data[i][elem_key], (void *)type_data[i][elem_val]);
    }

    if (version_dir == DNA_RENAME_STATIC_FROM_ALIAS) {
      const char *renames[][2] = {
          /* {old, new}, like in #DNA_STRUCT_RENAME */
          {"uchar", "uint8_t"},
          {"short", "int16_t"},
          {"ushort", "uint16_t"},
          {"int", "int32_t"},
          {"int", "uint32_t"},
      };
      for (int i = 0; i < ARRAY_SIZE(renames); i++) {
        BLI_ghash_insert(type_map, (void *)renames[i][elem_key], (void *)renames[i][elem_val]);
      }
    }

    *r_type_map = type_map;

    /* We know the direction of this, for local use. */
    type_map_local = BLI_ghash_str_new_ex(__func__, ARRAY_SIZE(type_data));
    for (int i = 0; i < ARRAY_SIZE(type_data); i++) {
      BLI_ghash_insert(type_map_local, (void *)type_data[i][1], (void *)type_data[i][0]);
    }
  }

  if (r_member_map != nullptr) {
    const char *member_data[][3] = {
#define DNA_STRUCT_RENAME(old, new)
#define DNA_STRUCT_RENAME_MEMBER(struct_name, old, new) {#struct_name, #old, #new},
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
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
    GHash *member_map = BLI_ghash_new_ex(
        strhash_pair_p, strhash_pair_cmp, __func__, ARRAY_SIZE(member_data));
    for (int i = 0; i < ARRAY_SIZE(member_data); i++) {
      const char **str_pair = static_cast<const char **>(
          MEM_mallocN(sizeof(char *) * 2, __func__));
      str_pair[0] = static_cast<const char *>(
          BLI_ghash_lookup_default(type_map_local, member_data[i][0], (void *)member_data[i][0]));
      str_pair[1] = member_data[i][elem_key];
      BLI_ghash_insert(member_map, (void *)str_pair, (void *)member_data[i][elem_val]);
    }
    *r_member_map = member_map;
  }

  if (type_map_local) {
    BLI_ghash_free(type_map_local, nullptr, nullptr);
  }
}

#undef DNA_MAKESDNA

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct Name Legacy Hack
 * \{ */

/* WARNING: Only keep this for compatibility: *NEVER ADD NEW STRINGS HERE*.
 *
 * The renaming here isn't complete, references to the old struct names
 * are still included in DNA, now fixing these struct names properly
 * breaks forward compatibility. Leave these as-is, but don't add to them!
 * See D4342#98780. */

const char *DNA_struct_rename_legacy_hack_static_from_alias(const char *name)
{
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
