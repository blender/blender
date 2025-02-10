/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct MemArena;

/**
 * Naming convention in this header:
 *  - `member_full` refers to the full definition of a struct member, including its prefixes (like
 *    the pointer `*` ones) and suffixes (like the `[]` array ones).
 *  - `member_id` (for member identifier) refers to the bare name of the struct member (e.g. `var`
 *    is the member identifier of the `*var[n1][n2]` full member).
 */

/**
 * Parse the `[n1][n2]...` at the end of an array struct member.
 *
 * \return the total number of array elements `n1 * n2 ...`, or `1` if the member is not an array.
 */
int DNA_member_array_num(const char *str);

/** Find the start offset of the member id (the name) within the full member definition. */
uint DNA_member_id_offset_start(const char *member_full);
/**
 * Find the end offset of the member id (the name) within the trimmed full member definition.
 *
 * WARNING: Expects an input string which has already been trimmed from its non-identifier
 * prefixes. E.g. passing `*var[n1]` to this function will return `0`, while passing `var[n1]` will
 * return the expected `3` value.
 */
uint DNA_member_id_offset_end(const char *member_full_trimmed);
/**
 * Copy the member id part (the bare name) of the full source member into \a member_id_dst.
 *
 * \param member_id_dst: destination char buffer, must be at least the size of \a member_src_full.
 */
uint DNA_member_id_strip_copy(char *member_id_dst, const char *member_full_src);
/**
 * Same as #DNA_member_id_strip_copy, but modifies the given \a member string in place.
 */
uint DNA_member_id_strip(char *member);
/**
 * Check if the member identifier given in \a member_id matches the full name given in \a
 * member_full. E.g. `var` matches full names like `var` or `*var[3]`, but not `variable`.
 *
 * \return true if it does, with the start offset of the match in \a r_member_full_offset.
 */
bool DNA_member_id_match(const char *member_id,
                         int member_id_len,
                         const char *member_full,
                         uint *r_member_full_offset);
/**
 * Rename a struct member to a different name.
 *
 * Replace the source member identifier (\a member_id_src) by the destination one
 * (\a member_id_dst), while preserving the potential prefixes and suffixes.
 *
 * \return a renamed DNA full member, allocated from \a mem_arena.
 */
char *DNA_member_id_rename(struct MemArena *mem_arena,
                           const char *member_id_src,
                           int member_id_src_len,
                           const char *member_id_dst,
                           int member_id_dst_len,
                           const char *member_full_src,
                           int member_full_src_len,
                           uint member_full_src_offset_len);

/**
 * When requesting version info, support both directions.
 *
 * - 'Static' is the original name of the data, the one that is still stored in blend-files
 *   DNA info (to avoid breaking forward compatibility).
 * - 'Alias' is the current name of the data, the one used in current DNA definition code.
 */
enum eDNA_RenameDir {
  DNA_RENAME_STATIC_FROM_ALIAS = -1,
  DNA_RENAME_ALIAS_FROM_STATIC = 1,
};
void DNA_alias_maps(enum eDNA_RenameDir version_dir,
                    struct GHash **r_type_map,
                    struct GHash **r_member_map);

/**
 * DNA Compatibility Hack.
 */
const char *DNA_struct_rename_legacy_hack_alias_from_static(const char *name);
const char *DNA_struct_rename_legacy_hack_static_from_alias(const char *name);

#ifdef __cplusplus
}
#endif
