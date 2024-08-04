/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 */

#pragma once

struct MemArena;

#
#
typedef struct SDNA_StructMember {
  /** This struct must not change, it's only a convenience view for raw data stored in SDNA. */

  /** An index into SDNA->types. */
  short type_index;
  /** An index into SDNA->members. */
  short member_index;
} SDNA_StructMember;

#
#
typedef struct SDNA_Struct {
  /** This struct must not change, it's only a convenience view for raw data stored in SDNA. */

  /** An index into SDNA->types. */
  short type_index;
  /** The amount of members in this struct. */
  short members_num;
  /** "Flexible array member" that contains information about all members of this struct. */
  SDNA_StructMember members[];
} SDNA_Struct;

#
#
typedef struct SDNA {
  /** The 'encoded' data (full copy when #data_alloc is set, otherwise borrowed memory). */
  const char *data;
  /** Length of #data, in bytes. */
  int data_size;
  bool data_alloc;

  /** Size of a pointer in bytes. */
  int pointer_size;

  /* ***** Start of SDNA types. ***** */
  /**
   * This covers all known types (basic and structs ones) from SDNA.
   *
   * NOTE: This data is not in sync with the SDNA #structs info below. Among other things:
   *   - Basic types (int, float, etc.) have _no_ matching struct definitions currently.
   *   - Types can be discovered and added before their struct definition, when they are used for
   *     members of another struct which gets parsed first.
   */
  /** Number of types. */
  int types_num;
  /** Type names. */
  const char **types;
  /** Type lengths. */
  short *types_size;
  /**
   * Alignment used when allocating pointers to this type. The actual minimum alignment of the
   * type may be lower in some cases. For example, the pointer alignment of a single char is at
   * least 8 bytes, but the alignment of the type itself is 1.
   */
  int *types_alignment;
  /* ***** End of SDNA types. ***** */

  /* ***** Start of SDNA structs. ***** */
  /**
   * This covers all known structs from SDNA (pointers to #SDNA_Struct data).
   *
   * NOTE: See comment above about SDNA types above for differences between structs and types
   * definitions.
   */
  /** Number of struct definitions. */
  int structs_num;
  /** Information about structs and their members. */
  SDNA_Struct **structs;
  /* ***** End of SDNA structs. ***** */

  /* ***** Start of SDNA struct members. ***** */
  /** Total number of struct members. */
  int members_num;
  /**
   * Contains the number of allocated items in both #members and #members_array_num arrays below.
   *
   * Typically same as #members_len, unless after versioning DNA info (these arrays are
   * reallocated by chunks, see #DNA_sdna_patch_struct_member).
   */
  int members_num_alloc;
  /** Struct member names. */
  const char **members;
  /**
   * Aligned with #members. The total number of items in the array defined by the matching member,
   * if any, otherwise 1.
   *
   * Result of #DNA_member_array_num.
   */
  short *members_array_num;
  /* ***** End of SDNA struct members. ***** */

  /**
   * Mapping between type names (from #types array above) and struct indices (into #structs array
   * above).
   *
   * Requires WITH_DNA_GHASH to be used for now.
   */
  struct GHash *types_to_structs_map;

  /**
   * Runtime versions of data stored in DNA, lazy initialized, only different when renaming is
   * done.
   *
   * Contains mapping from original (static) types/members names to their current (alias)
   * DNA-defined versions (i.e. results from calling #DNA_alias_maps with
   * #DNA_RENAME_ALIAS_FROM_STATIC).
   */
  struct {
    /** Aligned with #SDNA.types, same pointers when unchanged. */
    const char **types;
    /** Aligned with #SDNA.members, same pointers when unchanged. */
    const char **members;
    /** A version of #SDNA.types_to_structs_map that uses #SDNA.alias.types for its keys. */
    struct GHash *types_to_structs_map;
  } alias;

  /** Temporary memory currently only used for version patching DNA. */
  struct MemArena *mem_arena;
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
