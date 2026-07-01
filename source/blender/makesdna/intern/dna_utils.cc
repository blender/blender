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

#include "BLI_assert.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_sys_types.hh"
#include "BLI_utildefines.hh"

#include "dna_utils.h"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Struct Member Evaluation
 * \{ */

int DNA_member_array_num(const StringRef str)
{
  int result = 1;
  int current = 0;
  for (const char c : str) {
    switch (c) {
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
  return result;
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

uint DNA_member_id_offset_start(const StringRef member_full)
{
  uint elem_full_offset = 0;
  /* NOTE(@ideasman42): checking nil is needed for invalid names such as `*`,
   * these were written by older versions of Blender (v2.66).
   * In this case the "name" part will be an empty string.
   * The member cannot be used, this just prevents a crash. */
  while (elem_full_offset < member_full.size() && !is_identifier(member_full[elem_full_offset])) {
    elem_full_offset++;
  }
  return elem_full_offset;
}

static uint dna_member_id_length(const StringRef member_full_trimmed)
{
  uint elem_full_offset = 0;
  while (elem_full_offset < member_full_trimmed.size() &&
         is_identifier(member_full_trimmed[elem_full_offset]))
  {
    elem_full_offset++;
  }
  return elem_full_offset;
}

StringRef DNA_member_id_string_ref(const StringRef member_full)
{
  const uint id_start = DNA_member_id_offset_start(member_full);
  const StringRef member_id = member_full.drop_prefix(id_start);
  return member_id.substr(0, dna_member_id_length(member_id));
}

bool DNA_member_id_match(const StringRef member_id,
                         const StringRef member_full,
                         uint *r_member_full_offset)
{
  const uint elem_full_offset = DNA_member_id_offset_start(member_full);
  const StringRef elem_full_trim = member_full.drop_prefix(elem_full_offset);
  if (elem_full_trim.startswith(member_id)) {
    const int64_t next = member_id.size();
    if (next == elem_full_trim.size() || !is_identifier(elem_full_trim[next])) {
      *r_member_full_offset = elem_full_offset;
      return true;
    }
  }
  return false;
}

StringRef DNA_member_id_rename(LinearAllocator<> &mem_arena,
                               const StringRef member_id_src,
                               const StringRef member_id_dst,
                               const StringRef member_full_src,
                               const uint member_full_src_offset_len)
{
  BLI_assert(DNA_member_id_offset_start(member_full_src) == member_full_src_offset_len);

  const int64_t member_full_dst_len = (member_full_src.size() - member_id_src.size()) +
                                      member_id_dst.size();
  char *member_full_dst = mem_arena.allocate_array<char>(member_full_dst_len + 1).data();
  int64_t i = 0;
  if (member_full_src_offset_len != 0) {
    memcpy(member_full_dst, member_full_src.data(), member_full_src_offset_len);
    i = member_full_src_offset_len;
  }
  memcpy(&member_full_dst[i], member_id_dst.data(), member_id_dst.size());
  i += member_id_dst.size();
  const int64_t member_full_src_offset_end = member_full_src_offset_len + member_id_src.size();
  BLI_assert(dna_member_id_length(member_full_src.drop_prefix(member_full_src_offset_len)) ==
             (member_full_src_offset_end - member_full_src_offset_len));
  if (member_full_src_offset_end != member_full_src.size()) {
    const int64_t member_full_tail_len = member_full_src.size() - member_full_src_offset_end;
    memcpy(&member_full_dst[i],
           member_full_src.data() + member_full_src_offset_end,
           member_full_tail_len);
    i += member_full_tail_len;
  }
  member_full_dst[i] = '\0';
  BLI_assert(i == member_full_dst_len);
  UNUSED_VARS_NDEBUG(i);
  return member_full_dst;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rename Maps
 * \{ */

struct StructRename {
  StringRef old_name;
  StringRef new_name;
};
static const StructRename struct_renames[] = {
#define DNA_STRUCT_RENAME(old, new) {#old, #new},
#define DNA_STRUCT_RENAME_MEMBER(new_struct_name, old, new)
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
};

struct MemberRename {
  StringRef new_struct_name;
  StringRef old_name;
  StringRef new_name;
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
    const StringRef struct_static = data.types.lookup_default(r.new_struct_name,
                                                              r.new_struct_name);
    data.members.add_new({struct_static, r.new_name}, r.old_name);
  }
  return data;
}

DnaRenameMaps DNA_rename_maps_static_to_alias()
{
  DnaRenameMaps data;
  Map<StringRef, StringRef> struct_alias_to_static;
  for (const StructRename &r : struct_renames) {
    data.types.add_new(r.old_name, r.new_name);
    struct_alias_to_static.add_new(r.new_name, r.old_name);
  }
  for (const MemberRename &r : member_renames) {
    const StringRef struct_static = struct_alias_to_static.lookup_default(r.new_struct_name,
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

StringRef DNA_struct_rename_legacy_hack_static_from_alias(const StringRef name)
{
  /* 'bScreen' replaces the old IrisGL 'Screen' struct */
  if ("bScreen" == name) {
    return "Screen";
  }
  /* Groups renamed to collections in 2.8 */
  if ("Collection" == name) {
    return "Group";
  }
  if ("CollectionObject" == name) {
    return "GroupObject";
  }
  return name;
}

StringRef DNA_struct_rename_legacy_hack_alias_from_static(const StringRef name)
{
  /* 'bScreen' replaces the old IrisGL 'Screen' struct */
  if ("Screen" == name) {
    return "bScreen";
  }
  /* Groups renamed to collections in 2.8 */
  if ("Group" == name) {
    return "Collection";
  }
  if ("GroupObject" == name) {
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
