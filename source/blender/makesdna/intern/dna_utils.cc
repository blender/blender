/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Utilities for stand-alone `makesdna.cc` and Blender to share.
 */

#include <cstring>

#include "DNA_defs.h"

#include "BLI_alloca.h"
#include "BLI_assert.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_memarena.h"

#include "dna_utils.h"

namespace blender {

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

static uint dna_member_id_length(const char *member_full_trimmed)
{
  uint elem_full_offset = 0;
  while (is_identifier(member_full_trimmed[elem_full_offset])) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

StringRef DNA_member_id_string_ref(const StringRefNull member_full)
{
  const uint id_start = DNA_member_id_offset_start(member_full.c_str());
  const uint id_len = dna_member_id_length(member_full.c_str() + id_start);
  return StringRef(member_full.c_str() + id_start, id_len);
}

uint DNA_member_id_strip_copy(char *member_id_dst, const char *member_full_src)
{
  const StringRef stripped = DNA_member_id_string_ref(member_full_src);
  memcpy(member_id_dst, stripped.data(), stripped.size());
  member_id_dst[stripped.size()] = '\0';
  return stripped.size();
}

uint DNA_member_id_strip(char *member)
{
  const StringRef stripped = DNA_member_id_string_ref(member);
  memmove(member, stripped.data(), stripped.size());
  member[stripped.size()] = '\0';
  return stripped.size();
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
  BLI_assert(dna_member_id_length(member_full_src + member_full_src_offset_len) ==
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
/** \name Rename Maps
 * \{ */

struct StructRename {
  const char *old_name;
  const char *new_name;
};
static const StructRename struct_renames[] = {
#define DNA_STRUCT_RENAME(old, new) {#old, #new},
#define DNA_STRUCT_RENAME_MEMBER(new_struct_name, old, new)
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
};

struct MemberRename {
  const char *new_struct_name;
  const char *old_name;
  const char *new_name;
};
static const MemberRename member_renames[] = {
#define DNA_STRUCT_RENAME(old, new)
#define DNA_STRUCT_RENAME_MEMBER(new_struct_name, old, new) {#new_struct_name, #old, #new},
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
};

DnaRenameMaps DNA_rename_maps_alias_to_static()
{
  DnaRenameMaps data;
  for (const StructRename &r : struct_renames) {
    data.types.add_new(r.new_name, r.old_name);
  }
  for (const MemberRename &r : member_renames) {
    const StringRefNull struct_static = data.types.lookup_default(r.new_struct_name,
                                                                  r.new_struct_name);
    data.members.add_new({struct_static, r.new_name}, r.old_name);
  }
  return data;
}

DnaRenameMaps DNA_rename_maps_static_to_alias()
{
  DnaRenameMaps data;
  Map<StringRefNull, StringRefNull> struct_alias_to_static;
  for (const StructRename &r : struct_renames) {
    data.types.add_new(r.old_name, r.new_name);
    struct_alias_to_static.add_new(r.new_name, r.old_name);
  }
  for (const MemberRename &r : member_renames) {
    const StringRefNull struct_static = struct_alias_to_static.lookup_default(r.new_struct_name,
                                                                              r.new_struct_name);
    data.members.add_new({struct_static, r.old_name}, r.new_name);
  }
  return data;
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

void _DNA_internal_memcpy(void *dst, const void *src, const size_t size)
{
  memcpy(dst, src, size);
}

void _DNA_internal_memzero(void *dst, const size_t size)
{
  memset(dst, 0, size);
}

void _DNA_internal_swap(void *a, void *b, const size_t size)
{
  void *tmp = alloca(size);
  memcpy(tmp, a, size);
  memcpy(a, b, size);
  memcpy(b, tmp, size);
}

/** \} */

}  // namespace blender
