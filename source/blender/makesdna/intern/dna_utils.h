/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct MemArena;

/**
 * Parses the `[n1][n2]...` on the end of an array name
 * and returns the number of array elements `n1 * n2 ...`.
 */
int DNA_elem_array_size(const char *str);

uint DNA_elem_id_offset_start(const char *elem_full);
uint DNA_elem_id_offset_end(const char *elem_full);
/**
 * \a elem_dst must be at least the size of \a elem_src.
 */
uint DNA_elem_id_strip_copy(char *elem_dst, const char *elem_src);
uint DNA_elem_id_strip(char *elem);
/**
 * Check if 'var' matches '*var[3]' for eg,
 * return true if it does, with start/end offsets.
 */
bool DNA_elem_id_match(const char *elem_search,
                       int elem_search_len,
                       const char *elem_full,
                       uint *r_elem_full_offset);
/**
 * \return a renamed DNA name, allocated from \a mem_arena.
 */
char *DNA_elem_id_rename(struct MemArena *mem_arena,
                         const char *elem_src,
                         int elem_src_len,
                         const char *elem_dst,
                         int elem_dst_len,
                         const char *elem_src_full,
                         int elem_src_full_len,
                         uint elem_src_full_offset_len);

/**
 * When requesting version info, support both directions.
 */
enum eDNA_RenameDir {
  DNA_RENAME_STATIC_FROM_ALIAS = -1,
  DNA_RENAME_ALIAS_FROM_STATIC = 1,
};
void DNA_alias_maps(enum eDNA_RenameDir version_dir,
                    struct GHash **r_struct_map,
                    struct GHash **r_elem_map);

const char *DNA_struct_rename_legacy_hack_alias_from_static(const char *name);
/**
 * DNA Compatibility Hack.
 */
const char *DNA_struct_rename_legacy_hack_static_from_alias(const char *name);

#ifdef __cplusplus
}
#endif
