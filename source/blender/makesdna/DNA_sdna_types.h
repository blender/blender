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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct MemArena;

#
#
typedef struct SDNA_StructMember {
  /** This struct must not change, it's only a convenience view for raw data stored in SDNA. */

  /** An index into SDNA->types. */
  short type;
  /** An index into SDNA->names. */
  short name;
} SDNA_StructMember;

#
#
typedef struct SDNA_Struct {
  /** This struct must not change, it's only a convenience view for raw data stored in SDNA. */

  /** An index into SDNA->types. */
  short type;
  /** The amount of members in this struct. */
  short members_len;
  /** "Flexible array member" that contains information about all members of this struct. */
  SDNA_StructMember members[];
} SDNA_Struct;

#
#
typedef struct SDNA {
  /** Full copy of 'encoded' data (when data_alloc is set, otherwise borrowed). */
  const char *data;
  /** Length of data. */
  int data_len;
  bool data_alloc;

  /** Total number of struct members. */
  int names_len, names_len_alloc;
  /** Struct member names. */
  const char **names;
  /** Result of #DNA_elem_array_size (aligned with #names). */
  short *names_array_len;

  /** Size of a pointer in bytes. */
  int pointer_size;

  /** Type names. */
  const char **types;
  /** Number of basic types + struct types. */
  int types_len;

  /** Type lengths. */
  short *types_size;

  /** Information about structs and their members. */
  SDNA_Struct **structs;
  /** Number of struct types. */
  int structs_len;

  /** #GHash for faster lookups, requires WITH_DNA_GHASH to be used for now. */
  struct GHash *structs_map;

  /** Temporary memory currently only used for version patching DNA. */
  struct MemArena *mem_arena;
  /** Runtime versions of data stored in DNA, lazy initialized,
   * only different when renaming is done. */
  struct {
    /** Aligned with #SDNA.names, same pointers when unchanged. */
    const char **names;
    /** Aligned with #SDNA.types, same pointers when unchanged. */
    const char **types;
    /** A version of #SDNA.structs_map that uses #SDNA.alias.types for its keys. */
    struct GHash *structs_map;
  } alias;
} SDNA;

#
#
typedef struct BHead {
  int code, len;
  const void *old;
  int SDNAnr, nr;
} BHead;
#
#
typedef struct BHead4 {
  int code, len;
  uint old;
  int SDNAnr, nr;
} BHead4;
#
#
typedef struct BHead8 {
  int code, len;
  uint64_t old;
  int SDNAnr, nr;
} BHead8;

#ifdef __cplusplus
}
#endif
