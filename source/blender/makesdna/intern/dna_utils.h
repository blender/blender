/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include <utility>

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.hh"

namespace blender {

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
int DNA_member_array_num(StringRef str);

/** Find the start offset of the member id (the name) within the full member definition. */
uint DNA_member_id_offset_start(StringRef member_full);
/**
 * Return the stripped member identifier portion of #member_full. This references
 * the original string and does not make a copy.
 */
StringRef DNA_member_id_string_ref(StringRef member_full);
/**
 * Check if the member identifier given in \a member_id matches the full name given in \a
 * member_full. E.g. `var` matches full names like `var` or `*var[3]`, but not `variable`.
 *
 * \return true if it does, with the start offset of the match in \a r_member_full_offset.
 */
bool DNA_member_id_match(StringRef member_id, StringRef member_full, uint *r_member_full_offset);
/**
 * Rename a struct member to a different name.
 *
 * Replace the source member identifier (\a member_id_src) by the destination one
 * (\a member_id_dst), while preserving the potential prefixes and suffixes.
 *
 * \return a renamed DNA full member, allocated from \a mem_arena.
 */
StringRef DNA_member_id_rename(LinearAllocator<> &mem_arena,
                               StringRef member_id_src,
                               StringRef member_id_dst,
                               StringRef member_full_src,
                               uint member_full_src_offset_len);

/**
 * Rename maps built from `dna_rename_defs.h` and other versioning.
 *
 * - 'Static' is the original name of the data, the one that is still stored in blend-files
 *   DNA info (to avoid breaking forward compatibility).
 * - 'Alias' is the current name of the data, the one used in current DNA definition code.
 */
struct DnaRenameMaps {
  /* Type name mapping. */
  Map<StringRef, StringRef> types;
  /** Member name mapping (first in pair is always the static struct name). */
  Map<std::pair<StringRef, StringRef>, StringRef> members;
};

DnaRenameMaps DNA_rename_maps_alias_to_static();
DnaRenameMaps DNA_rename_maps_static_to_alias();

/**
 * DNA Compatibility Hack.
 */
StringRef DNA_struct_rename_legacy_hack_alias_from_static(StringRef name);
StringRef DNA_struct_rename_legacy_hack_static_from_alias(StringRef name);

}  // namespace blender
