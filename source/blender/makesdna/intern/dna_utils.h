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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct MemArena;

int DNA_elem_array_size(const char *str);

uint DNA_elem_id_offset_start(const char *elem_full);
uint DNA_elem_id_offset_end(const char *elem_full);
uint DNA_elem_id_strip_copy(char *elem_dst, const char *elem_src);
uint DNA_elem_id_strip(char *elem);
bool DNA_elem_id_match(const char *elem_search,
                       const int elem_search_len,
                       const char *elem_full,
                       uint *r_elem_full_offset);
char *DNA_elem_id_rename(struct MemArena *mem_arena,
                         const char *elem_src,
                         const int elem_src_len,
                         const char *elem_dst,
                         const int elem_dst_len,
                         const char *elem_src_full,
                         const int elem_src_full_len,
                         const uint elem_src_full_offset_len);

/* When requesting version info, support both directions. */
enum eDNA_RenameDir {
  DNA_RENAME_STATIC_FROM_ALIAS = -1,
  DNA_RENAME_ALIAS_FROM_STATIC = 1,
};
void DNA_alias_maps(enum eDNA_RenameDir version_dir,
                    struct GHash **r_struct_map,
                    struct GHash **r_elem_map);

const char *DNA_struct_rename_legacy_hack_alias_from_static(const char *name);
const char *DNA_struct_rename_legacy_hack_static_from_alias(const char *name);

#ifdef __cplusplus
}
#endif
