/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief DNA handling
 *
 * Lowest-level functions for decoding the parts of a saved .blend
 * file, including interpretation of its SDNA block and conversion of
 * contents of other parts according to the differences between that
 * SDNA and the SDNA of the current (running) version of Blender.
 */

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <fmt/format.h>

#include "MEM_guardedalloc.h" /* for MEM_freeN MEM_mallocN MEM_callocN */

#include "BLI_ghash.h"
#include "BLI_index_range.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_memarena.h"
#include "BLI_set.hh"
#include "BLI_utildefines.h"

#include "DNA_genfile.h"
#include "DNA_print.hh"
#include "DNA_sdna_pointers.hh"
#include "DNA_sdna_types.h" /* for SDNA ;-) */

/**
 * \section dna_genfile Overview
 *
 * - please NOTE: no builtin security to detect input of double structs
 * - if you want a struct not to be in DNA file: add two hash marks above it `(#<enter>#<enter>)`.
 *
 * Structure DNA data is added to each blender file and to each executable, this to detect
 * in .blend files new variables in structs, changed array sizes, etc. It is also used for
 * converting pointer size (32-64 bits) (and it used to be to handle endianness).
 * As an extra, Python uses a call to detect run-time the contents of a blender struct.
 *
 * Create a structDNA: only needed when one of the input include (.h) files change.
 * File Syntax:
 * \code{.unparsed}
 *     SDNA (4 bytes) (magic number)
 *     NAME (4 bytes)
 *     <nr> (4 bytes) amount of names `int`.
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TYPE (4 bytes)
 *     <nr> amount of types `int`.
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TLEN (4 bytes)
 *     <len> (short) the lengths of types
 *     <len>
 *     ...
 *     ...
 *     STRC (4 bytes)
 *     <nr> amount of structs `int`.
 *     <typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
 * \endcode
 *
 * **Remember to read/write integer and short aligned!**
 *
 * While writing a file, the names of a struct is indicated with a type number,
 * to be found with: `type = DNA_struct_find_with_alias(SDNA *, const char *)`
 * The value of `type` corresponds with the index within the structs array
 *
 * For the moment: the complete DNA file is included in a .blend file. For
 * the future we can think of smarter methods, like only included the used
 * structs. Only needed to keep a file short though...
 *
 * ALLOWED AND TESTED CHANGES IN STRUCTS:
 *  - Type change (a char to float will be divided by 255).
 *  - Location within a struct (everything can be randomly mixed up).
 *  - Struct within struct (within struct etc), this is recursive.
 *  - Adding new elements, will be default initialized zero.
 *  - Removing elements.
 *  - Change of array sizes.
 *  - Change of a pointer type: when the name doesn't change the contents is copied.
 *
 * NOT YET:
 *  - array (`vec[3]`) to float struct (`vec3f`).
 *
 * DONE:
 *  - Pointer conversion (32-64 bits).
 *
 * IMPORTANT:
 *  - Do not use #defines in structs for array lengths, this cannot be read by the dna functions.
 *  - Do not use `uint`, but unsigned int instead, `ushort` and `ulong` are allowed.
 *  - Only use a long in Blender if you want this to be the size of a pointer. so it is
 *    32 bits or 64 bits, dependent at the cpu architecture.
 *  - Chars are always unsigned
 *  - Alignment of variables has to be done in such a way, that any system does
 *    not create 'padding' (gaps) in structures. So make sure that:
 *    - short: 2 aligned.
 *    - int: 4 aligned.
 *    - float: 4 aligned.
 *    - double: 8 aligned.
 *    - long: 8 aligned.
 *    - int64: 8 aligned.
 *    - struct: 8 aligned.
 *  - the sdna functions have several error prints builtin,
 *    always check blender running from a console.
 */

/* NOTE: this is endianness-sensitive. */
/* Little Endian */
#define MAKE_ID(a, b, c, d) (int(d) << 24 | int(c) << 16 | (b) << 8 | (a))

/* ************************* DIV ********************** */

void DNA_sdna_free(SDNA *sdna)
{
  if (sdna->data_alloc) {
    MEM_freeN(sdna->data);
  }

  MEM_SAFE_FREE(sdna->members);
  MEM_SAFE_FREE(sdna->members_array_num);
  MEM_SAFE_FREE(sdna->types);
  MEM_SAFE_FREE(sdna->structs);
  MEM_SAFE_FREE(sdna->types_alignment);

#ifdef WITH_DNA_GHASH
  if (sdna->types_to_structs_map) {
    BLI_ghash_free(sdna->types_to_structs_map, nullptr, nullptr);
  }
#endif

  if (sdna->mem_arena) {
    BLI_memarena_free(sdna->mem_arena);
  }

  MEM_SAFE_FREE(sdna->alias.members);
  MEM_SAFE_FREE(sdna->alias.types);
#ifdef WITH_DNA_GHASH
  if (sdna->alias.types_to_structs_map) {
    BLI_ghash_free(sdna->alias.types_to_structs_map, nullptr, nullptr);
  }
#endif

  MEM_freeN(sdna);
}

int DNA_struct_size(const SDNA *sdna, int struct_index)
{
  return sdna->types_size[sdna->structs[struct_index]->type_index];
}

/**
 * Return true if the name indicates a pointer of some kind.
 */
static bool ispointer(const char *name)
{
  /* check if pointer or function pointer */
  return (name[0] == '*' || (name[0] == '(' && name[1] == '*'));
}

int DNA_struct_member_size(const SDNA *sdna, short type, short member_index)
{
  const char *cp = sdna->members[member_index];
  int len = 0;

  /* is it a pointer or function pointer? */
  if (ispointer(cp)) {
    /* has the name an extra length? (array) */
    len = sdna->pointer_size * sdna->members_array_num[member_index];
  }
  else if (sdna->types_size[type]) {
    /* has the name an extra length? (array) */
    len = int(sdna->types_size[type]) * sdna->members_array_num[member_index];
  }

  return len;
}

#if 0
static void printstruct(SDNA *sdna, short struct_index)
{
  /* is for debug */

  SDNA_Struct *struct_info = sdna->structs[struct_index];
  printf("struct %s\n", sdna->types[struct_info->type]);

  for (int b = 0; b < struct_info->members_len; b++) {
    SDNA_StructMember *struct_member = &struct_info->members[b];
    printf("   %s %s\n", sdna->types[struct_member->type], sdna->names[struct_member->name]);
  }
}
#endif

/**
 * Returns the index of the struct info for the struct with the specified name.
 */
static int dna_struct_find_index_ex_impl(
    /* From SDNA struct. */
    const char **types,
    const int /*types_num*/,
    SDNA_Struct **const structs,
    const int structs_num,
#ifdef WITH_DNA_GHASH
    GHash *structs_map,
#endif
    /* Regular args. */
    const char *str,
    uint *struct_index_last)
{
  if (*struct_index_last < structs_num) {
    const SDNA_Struct *struct_info = structs[*struct_index_last];
    if (STREQ(types[struct_info->type_index], str)) {
      return *struct_index_last;
    }
  }

#ifdef WITH_DNA_GHASH
  {
    void **struct_index_p = BLI_ghash_lookup_p(structs_map, str);
    if (struct_index_p) {
      const int struct_index = POINTER_AS_INT(*struct_index_p);
      *struct_index_last = struct_index;
      return struct_index;
    }
  }
#else
  {
    for (int struct_index = 0; struct_index < types_num; struct_index++) {
      const SDNA_Struct *struct_info = structs[struct_index];
      if (STREQ(types[struct_info->type], str)) {
        *struct_index_last = struct_index;
        return struct_index;
      }
    }
  }
#endif
  return -1;
}

int DNA_struct_find_index_without_alias_ex(const SDNA *sdna,
                                           const char *str,
                                           uint *struct_index_last)
{
#ifdef WITH_DNA_GHASH
  BLI_assert(sdna->types_to_structs_map != nullptr);
#endif
  return dna_struct_find_index_ex_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->types_num,
      sdna->structs,
      sdna->structs_num,
#ifdef WITH_DNA_GHASH
      sdna->types_to_structs_map,
#endif
      /* Regular args. */
      str,
      struct_index_last);
}

int DNA_struct_find_index_with_alias_ex(const SDNA *sdna, const char *str, uint *struct_index_last)
{
#ifdef WITH_DNA_GHASH
  BLI_assert(sdna->alias.types_to_structs_map != nullptr);
#endif
  return dna_struct_find_index_ex_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->types_num,
      sdna->structs,
      sdna->structs_num,
#ifdef WITH_DNA_GHASH
      sdna->alias.types_to_structs_map,
#endif
      /* Regular args. */
      str,
      struct_index_last);
}

int DNA_struct_find_index_without_alias(const SDNA *sdna, const char *str)
{
  uint index_last_dummy = UINT_MAX;
  return DNA_struct_find_index_without_alias_ex(sdna, str, &index_last_dummy);
}

int DNA_struct_find_with_alias(const SDNA *sdna, const char *str)
{
  uint index_last_dummy = UINT_MAX;
  return DNA_struct_find_index_with_alias_ex(sdna, str, &index_last_dummy);
}

bool DNA_struct_exists_with_alias(const SDNA *sdna, const char *str)
{
  return DNA_struct_find_with_alias(sdna, str) != -1;
}

/* ************************* END DIV ********************** */

/* ************************* READ DNA ********************** */

BLI_INLINE const char *pad_up_4(const char *ptr)
{
  return (const char *)((uintptr_t(ptr) + 3) & ~3);
}

/**
 * In sdna->data the data, now we convert that to something understandable
 */
static bool init_structDNA(SDNA *sdna, const char **r_error_message)
{
  int member_index_gravity_fix = -1;

  int *data = (int *)sdna->data;

  /* Clear pointers in case of error. */
  sdna->types = nullptr;
  sdna->types_size = nullptr;
  sdna->types_alignment = nullptr;
  sdna->structs = nullptr;
#ifdef WITH_DNA_GHASH
  sdna->types_to_structs_map = nullptr;
#endif

  sdna->members = nullptr;
  sdna->members_array_num = nullptr;

  sdna->mem_arena = nullptr;

  /* Lazy initialize. */
  memset(&sdna->alias, 0, sizeof(sdna->alias));

  /* Struct DNA ('SDNA') */
  if (*data != MAKE_ID('S', 'D', 'N', 'A')) {
    *r_error_message = "SDNA error in SDNA file";
    return false;
  }

  const char *cp;

  data++;
  /* Names array ('NAME') */
  if (*data == MAKE_ID('N', 'A', 'M', 'E')) {
    data++;

    /* NOTE: this is endianness-sensitive. */
    sdna->members_num = *data;
    sdna->members_num_alloc = sdna->members_num;

    data++;
    sdna->members = MEM_calloc_arrayN<const char *>(sdna->members_num, "sdnanames");
  }
  if (!sdna->members) {
    *r_error_message = "NAME error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int member_index = 0; member_index < sdna->members_num; member_index++) {
    sdna->members[member_index] = cp;

    /* "float gravity [3]" was parsed wrong giving both "gravity" and
     * "[3]"  members. we rename "[3]", and later set the type of
     * "gravity" to "void" so the offsets work out correct */
    if (*cp == '[' && STREQ(cp, "[3]")) {
      if (member_index && STREQ(sdna->members[member_index - 1], "Cvi")) {
        sdna->members[member_index] = "gravity[3]";
        member_index_gravity_fix = member_index;
      }
    }
    while (*cp) {
      cp++;
    }
    cp++;
  }

  cp = pad_up_4(cp);

  /* Type names array ('TYPE') */
  data = (int *)cp;
  if (*data == MAKE_ID('T', 'Y', 'P', 'E')) {
    data++;

    /* NOTE: this is endianness-sensitive. */
    sdna->types_num = *data;

    data++;
    sdna->types = MEM_calloc_arrayN<const char *>(sdna->types_num, "sdnatypes");
  }
  if (!sdna->types) {
    *r_error_message = "TYPE error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int type_index = 0; type_index < sdna->types_num; type_index++) {
    /* WARNING! See: DNA_struct_rename_legacy_hack_static_from_alias docs. */
    sdna->types[type_index] = DNA_struct_rename_legacy_hack_static_from_alias(cp);
    while (*cp) {
      cp++;
    }
    cp++;
  }

  cp = pad_up_4(cp);

  /* Type lengths array ('TLEN') */
  data = (int *)cp;
  short *sp;
  if (*data == MAKE_ID('T', 'L', 'E', 'N')) {
    data++;
    /* NOTE: this is endianness-sensitive. */
    sp = (short *)data;
    sdna->types_size = sp;

    sp += sdna->types_num;
  }
  if (!sdna->types_size) {
    *r_error_message = "TLEN error in SDNA file";
    return false;
  }
  /* prevent BUS error */
  if (sdna->types_num & 1) {
    sp++;
  }

  /* Struct array ('STRC') */
  data = (int *)sp;
  if (*data == MAKE_ID('S', 'T', 'R', 'C')) {
    data++;

    /* NOTE: this is endianness-sensitive. */
    sdna->structs_num = *data;

    data++;
    sdna->structs = MEM_calloc_arrayN<SDNA_Struct *>(sdna->structs_num, "sdnastrcs");
  }
  if (!sdna->structs) {
    *r_error_message = "STRC error in SDNA file";
    return false;
  }

  /* Safety check, to ensure that there is no multiple usages of a same struct index. */
  blender::Set<short> struct_indices;
  sp = (short *)data;
  for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
    /* NOTE: this is endianness-sensitive. */
    SDNA_Struct *struct_info = (SDNA_Struct *)sp;
    sdna->structs[struct_index] = struct_info;

    if (!struct_indices.add(struct_info->type_index)) {
      *r_error_message = "Invalid duplicate struct type index in SDNA file";
      return false;
    }

    sp += 2 + (sizeof(SDNA_StructMember) / sizeof(short)) * struct_info->members_num;
  }

  {
    /* second part of gravity problem, setting "gravity" type to void */
    if (member_index_gravity_fix > -1) {
      for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
        sp = (short *)sdna->structs[struct_index];
        if (STREQ(sdna->types[sp[0]], "ClothSimSettings")) {
          sp[10] = SDNA_TYPE_VOID;
        }
      }
    }
  }

#ifdef WITH_DNA_GHASH
  {
    /* create a ghash lookup to speed up */
    sdna->types_to_structs_map = BLI_ghash_str_new_ex("init_structDNA gh", sdna->structs_num);

    for (intptr_t struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
      SDNA_Struct *struct_info = sdna->structs[struct_index];
      BLI_ghash_insert(sdna->types_to_structs_map,
                       (void *)sdna->types[struct_info->type_index],
                       POINTER_FROM_INT(struct_index));
    }
  }
#endif

  /* Calculate 'sdna->pointer_size'.
   *
   * NOTE: Cannot just do `sizeof(void *)` here, since the current DNA may come from a blend-file
   * saved on a different system, using a different pointer size. So instead, use half the size of
   * the #ListBase struct (only made of two pointers).
   */
  {
    const int struct_index = DNA_struct_find_index_without_alias(sdna, "ListBase");

    /* Should never happen, only with corrupt file for example. */
    if (UNLIKELY(struct_index == -1)) {
      *r_error_message = "ListBase struct error! Not found.";
      return false;
    }

    const SDNA_Struct *struct_info = sdna->structs[struct_index];
    sdna->pointer_size = sdna->types_size[struct_info->type_index] / 2;

    /* Should never fail, double-check that #ListBase struct is still what is should be
     * (a couple of pointers and nothing else). */
    if (UNLIKELY(struct_info->members_num != 2 || !ELEM(sdna->pointer_size, 4, 8))) {
      *r_error_message = "ListBase struct error: invalid computed pointer-size.";
      return false;
    }
  }

  /* Cache name size. */
  {
    short *members_array_num = MEM_malloc_arrayN<short>(size_t(sdna->members_num), __func__);
    for (int member_index = 0; member_index < sdna->members_num; member_index++) {
      members_array_num[member_index] = DNA_member_array_num(sdna->members[member_index]);
    }
    sdna->members_array_num = members_array_num;
  }

  sdna->types_alignment = MEM_malloc_arrayN<int>(size_t(sdna->types_num), __func__);
  for (int type_index = 0; type_index < sdna->types_num; type_index++) {
    sdna->types_alignment[type_index] = int(__STDCPP_DEFAULT_NEW_ALIGNMENT__);
  }
  {
    /* TODO: This should be generalized at some point. We should be able to specify `overaligned`
     * types directly in the DNA struct definitions. */
    uint dummy_index = 0;
    const int mat4x4f_struct_index = DNA_struct_find_index_without_alias_ex(
        sdna, "mat4x4f", &dummy_index);
    if (mat4x4f_struct_index > 0) {
      const SDNA_Struct *struct_info = sdna->structs[mat4x4f_struct_index];
      const int mat4x4f_type_index = struct_info->type_index;
      sdna->types_alignment[mat4x4f_type_index] = alignof(blender::float4x4);
    }
  }

  return true;
}

SDNA *DNA_sdna_from_data(const void *data,
                         const int data_len,
                         bool data_alloc,
                         const bool do_alias,
                         const char **r_error_message)
{
  SDNA *sdna = MEM_mallocN<SDNA>("sdna");
  const char *error_message = nullptr;

  sdna->data_size = data_len;
  if (data_alloc) {
    char *data_copy = MEM_malloc_arrayN<char>(size_t(data_len), "sdna_data");
    memcpy(data_copy, data, data_len);
    sdna->data = data_copy;
  }
  else {
    sdna->data = static_cast<const char *>(data);
  }
  sdna->data_alloc = data_alloc;

  if (init_structDNA(sdna, &error_message)) {
    if (do_alias) {
      DNA_sdna_alias_data_ensure_structs_map(sdna);
    }
    return sdna;
  }

  if (r_error_message == nullptr) {
    fprintf(stderr, "Error decoding blend file SDNA: %s\n", error_message);
  }
  else {
    *r_error_message = error_message;
  }
  DNA_sdna_free(sdna);
  return nullptr;
}

/**
 * Using a global is acceptable here,
 * the data is read-only and only changes between Blender versions.
 *
 * So it is safe to create once and reuse.
 */
static SDNA *g_sdna = nullptr;

void DNA_sdna_current_init()
{
  g_sdna = DNA_sdna_from_data(DNAstr, DNAlen, false, true, nullptr);
}

const SDNA *DNA_sdna_current_get()
{
  BLI_assert(g_sdna != nullptr);
  return g_sdna;
}

void DNA_sdna_current_free()
{
  DNA_sdna_free(g_sdna);
  g_sdna = nullptr;
}

/* ******************** END READ DNA ********************** */

/* ******************* HANDLE DNA ***************** */

/**
 * This function changes compare_flags[old_struct_index] from SDNA_CMP_UNKNOWN to something else.
 * It might call itself recursively.
 */
static void set_compare_flags_for_struct(const SDNA *oldsdna,
                                         const SDNA *newsdna,
                                         char *compare_flags,
                                         const int old_struct_index)
{
  if (compare_flags[old_struct_index] != SDNA_CMP_UNKNOWN) {
    /* This flag has been initialized already. */
    return;
  }

  SDNA_Struct *old_struct = oldsdna->structs[old_struct_index];
  const char *struct_name = oldsdna->types[old_struct->type_index];

  const int new_struct_index = DNA_struct_find_index_without_alias(newsdna, struct_name);
  if (new_struct_index == -1) {
    /* Didn't find a matching new struct, so it has been removed. */
    compare_flags[old_struct_index] = SDNA_CMP_REMOVED;
    return;
  }

  SDNA_Struct *new_struct = newsdna->structs[new_struct_index];
  if (old_struct->members_num != new_struct->members_num) {
    /* Structs with a different amount of members are not equal. */
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }
  if (oldsdna->types_size[old_struct->type_index] != newsdna->types_size[new_struct->type_index]) {
    /* Structs that don't have the same size are not equal. */
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }

  /* Compare each member individually. */
  for (int member_index = 0; member_index < old_struct->members_num; member_index++) {
    const SDNA_StructMember *old_member = &old_struct->members[member_index];
    const SDNA_StructMember *new_member = &new_struct->members[member_index];

    const char *old_type_name = oldsdna->types[old_member->type_index];
    const char *new_type_name = newsdna->types[new_member->type_index];
    if (!STREQ(old_type_name, new_type_name)) {
      /* If two members have a different type in the same place, the structs are not equal. */
      compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
      return;
    }

    const char *old_member_name = oldsdna->members[old_member->member_index];
    const char *new_member_name = newsdna->members[new_member->member_index];
    if (!STREQ(old_member_name, new_member_name)) {
      /* If two members have a different name in the same place, the structs are not equal. */
      compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
      return;
    }

    if (ispointer(old_member_name)) {
      if (oldsdna->pointer_size != newsdna->pointer_size) {
        /* When the struct contains a pointer, and the pointer sizes differ, the structs are not
         * equal. */
        compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
        return;
      }
    }
    else {
      const int old_member_struct_index = DNA_struct_find_index_without_alias(oldsdna,
                                                                              old_type_name);
      if (old_member_struct_index >= 0) {
        set_compare_flags_for_struct(oldsdna, newsdna, compare_flags, old_member_struct_index);
        if (compare_flags[old_member_struct_index] != SDNA_CMP_EQUAL) {
          /* If an embedded struct is not equal, the parent struct cannot be equal either. */
          compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
          return;
        }
      }
    }
  }

  compare_flags[old_struct_index] = SDNA_CMP_EQUAL;
}

const char *DNA_struct_get_compareflags(const SDNA *oldsdna, const SDNA *newsdna)
{
  if (oldsdna->structs_num == 0) {
    printf("error: file without SDNA\n");
    return nullptr;
  }

  char *compare_flags = MEM_malloc_arrayN<char>(size_t(oldsdna->structs_num), "compare flags");
  memset(compare_flags, SDNA_CMP_UNKNOWN, oldsdna->structs_num);

  /* Set correct flag for every struct. */
  for (int old_struct_index = 0; old_struct_index < oldsdna->structs_num; old_struct_index++) {
    set_compare_flags_for_struct(oldsdna, newsdna, compare_flags, old_struct_index);
    BLI_assert(compare_flags[old_struct_index] != SDNA_CMP_UNKNOWN);
  }

  /* First struct is the fake 'raw data' one (see the #SDNA_TYPE_RAW_DATA 'basic type' definition
   * and its usages). By definition, it is always 'equal'.
   *
   * NOTE: Bugs History (pre-4.3).
   *
   * It used to be `struct Link`, it was skipped in compare_flags (at index `0`). This was a bug,
   * and was dirty-patched by setting `compare_flags[0]` to `SDNA_CMP_EQUAL` unconditionally.
   *
   * Then the `0` struct became `struct DrawDataList`, which was never actually written in
   * blend-files.
   *
   * Write and read blend-file code also has had implicit assumptions that a `0` value in the
   * #BHead.SDNAnr (aka DNA struct index) meant 'raw data', and therefore was not representing any
   * real DNA struct. This assumption has been false for years. By luck, this bug seems to have
   * been fully harmless, for at least the following reasons:
   *   - Read code always ignored DNA struct info in BHead blocks with a `0` value.
   *   - `DrawDataList` data was never actually written in blend-files.
   *   - `struct Link` never needed DNA-versioning.
   *
   * NOTE: This may have been broken in BE/LE conversion cases, however this endianness handling
   * code have likely been dead/never used in practice for many years, and has been removed in
   * Blender 5.0.
   */
  BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")
  compare_flags[SDNA_RAW_DATA_STRUCT_INDEX] = SDNA_CMP_EQUAL;

/* This code can be enabled to see which structs have changed. */
#if 0
  for (int a = 0; a < oldsdna->structs_len; a++) {
    if (compare_flags[a] == SDNA_CMP_NOT_EQUAL) {
      SDNA_Struct *struct_info = oldsdna->structs[a];
      printf("changed: %s\n", oldsdna->types[struct_info->type]);
    }
  }
#endif

  return compare_flags;
}

/**
 * Converts a value of one primitive type to another.
 *
 * \note there is no optimization for the case where \a otype and \a ctype are the same:
 * assumption is that caller will handle this case.
 *
 * \param old_type: Type to convert from.
 * \param new_type: Type to convert to.
 * \param array_len: Number of elements to convert.
 * \param old_data: Buffer containing the old values.
 * \param new_data: Buffer the converted values will be written to.
 */
static void cast_primitive_type(const eSDNA_Type old_type,
                                const eSDNA_Type new_type,
                                const int array_len,
                                const char *old_data,
                                char *new_data)
{
  /* define lengths */
  const int oldlen = DNA_elem_type_size(old_type);
  const int curlen = DNA_elem_type_size(new_type);

  double old_value_f = 0.0;
  /* Intentionally overflow signed values into an unsigned type.
   * Casting back to a signed value preserves the sign (when the new value is signed). */
  uint64_t old_value_i = 0;

  for (int a = 0; a < array_len; a++) {
    switch (old_type) {
      case SDNA_TYPE_CHAR: {
        const char value = *old_data;
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_UCHAR: {
        const uchar value = *reinterpret_cast<const uchar *>(old_data);
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_SHORT: {
        const short value = *reinterpret_cast<const short *>(old_data);
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_USHORT: {
        const ushort value = *reinterpret_cast<const ushort *>(old_data);
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_INT: {
        const int value = *reinterpret_cast<const int *>(old_data);
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_FLOAT: {
        const float value = *reinterpret_cast<const float *>(old_data);
        /* `int64_t` range stored in a `uint64_t`. */
        old_value_i = uint64_t(int64_t(value));
        old_value_f = value;
        break;
      }
      case SDNA_TYPE_DOUBLE: {
        const double value = *reinterpret_cast<const double *>(old_data);
        /* `int64_t` range stored in a `uint64_t`. */
        old_value_i = uint64_t(int64_t(value));
        old_value_f = value;
        break;
      }
      case SDNA_TYPE_INT64: {
        const int64_t value = *reinterpret_cast<const int64_t *>(old_data);
        old_value_i = uint64_t(value);
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_UINT64: {
        const uint64_t value = *reinterpret_cast<const uint64_t *>(old_data);
        old_value_i = value;
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_INT8: {
        const int8_t value = *reinterpret_cast<const int8_t *>(old_data);
        old_value_i = uint64_t(value);
        old_value_f = double(value);
        break;
      }
      case SDNA_TYPE_RAW_DATA:
        BLI_assert_msg(false, "Conversion from SDNA_TYPE_RAW_DATA is not supported");
        break;
    }

    switch (new_type) {
      case SDNA_TYPE_CHAR:
        *new_data = char(old_value_i);
        break;
      case SDNA_TYPE_UCHAR:
        *reinterpret_cast<uchar *>(new_data) = uchar(old_value_i);
        break;
      case SDNA_TYPE_SHORT:
        *reinterpret_cast<short *>(new_data) = short(old_value_i);
        break;
      case SDNA_TYPE_USHORT:
        *reinterpret_cast<ushort *>(new_data) = ushort(old_value_i);
        break;
      case SDNA_TYPE_INT:
        *reinterpret_cast<int *>(new_data) = int(old_value_i);
        break;
      case SDNA_TYPE_FLOAT:
        if (old_type < 2) {
          old_value_f /= 255.0;
        }
        *reinterpret_cast<float *>(new_data) = old_value_f;
        break;
      case SDNA_TYPE_DOUBLE:
        if (old_type < 2) {
          old_value_f /= 255.0;
        }
        *reinterpret_cast<double *>(new_data) = old_value_f;
        break;
      case SDNA_TYPE_INT64:
        *reinterpret_cast<int64_t *>(new_data) = int64_t(old_value_i);
        break;
      case SDNA_TYPE_UINT64:
        *reinterpret_cast<uint64_t *>(new_data) = old_value_i;
        break;
      case SDNA_TYPE_INT8:
        *reinterpret_cast<int8_t *>(new_data) = int8_t(old_value_i);
        break;
      case SDNA_TYPE_RAW_DATA:
        BLI_assert_msg(false, "Conversion to SDNA_TYPE_RAW_DATA is not supported");
        break;
    }

    old_data += oldlen;
    new_data += curlen;
  }
}

static void cast_pointer_32_to_64(const int array_len,
                                  const uint32_t *old_data,
                                  uint64_t *new_data)
{
  for (int a = 0; a < array_len; a++) {
    new_data[a] = old_data[a];
  }
}

static void cast_pointer_64_to_32(const int array_len,
                                  const uint64_t *old_data,
                                  uint32_t *new_data)
{
  /* WARNING: 32-bit Blender trying to load file saved by 64-bit Blender,
   * pointers may lose uniqueness on truncation! (Hopefully this won't
   * happen unless/until we ever get to multi-gigabyte .blend files...) */
  for (int a = 0; a < array_len; a++) {
    new_data[a] = old_data[a] >> 3;
  }
}

/**
 * Equality test on name and `oname` excluding any array-size suffix.
 */
static bool elem_streq(const char *name, const char *oname)
{
  int a = 0;

  while (true) {
    if (name[a] != oname[a]) {
      return false;
    }
    if (name[a] == '[' || oname[a] == '[') {
      break;
    }
    if (name[a] == 0 || oname[a] == 0) {
      break;
    }
    a++;
  }
  return true;
}

/**
 * Returns whether the specified field exists according to the struct format
 * pointed to by old.
 *
 * \param type: Current field type name.
 * \param name: Current field name.
 * \param old: Pointer to struct information in `sdna`.
 * \return true when existing, false otherwise..
 */
static bool elem_exists_impl(
    /* Expand SDNA. */
    const char **types,
    const char **names,
    /* Regular args. */
    const char *type,
    const char *name,
    const SDNA_Struct *old)
{
  /* in old is the old struct */
  for (int a = 0; a < old->members_num; a++) {
    const SDNA_StructMember *member = &old->members[a];
    const char *otype = types[member->type_index];
    const char *oname = names[member->member_index];

    if (elem_streq(name, oname)) { /* name equal */
      return STREQ(type, otype);   /* type equal */
    }
  }
  return false;
}

/**
 * \param sdna: Old SDNA.
 */
static bool elem_exists_without_alias(const SDNA *sdna,
                                      const char *type,
                                      const char *name,
                                      const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->members,
      /* Regular args. */
      type,
      name,
      old);
}

static bool elem_exists_with_alias(const SDNA *sdna,
                                   const char *type,
                                   const char *name,
                                   const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->alias.members,
      /* Regular args. */
      type,
      name,
      old);
}

static int elem_offset_impl(const SDNA *sdna,
                            const char **types,
                            const char **names,
                            const char *type,
                            const char *name,
                            const SDNA_Struct *old)
{
  /* Without array-part, so names can differ: return old `namenr` and type. */

  /* in old is the old struct */
  int offset = 0;
  for (int a = 0; a < old->members_num; a++) {
    const SDNA_StructMember *member = &old->members[a];
    const char *otype = types[member->type_index];
    const char *oname = names[member->member_index];
    if (elem_streq(name, oname)) { /* name equal */
      if (STREQ(type, otype)) {    /* type equal */
        return offset;
      }
      break; /* Fail below. */
    }
    offset += DNA_struct_member_size(sdna, member->type_index, member->member_index);
  }
  return -1;
}

/**
 * Return the offset in bytes or -1 on failure to find the struct member with its expected type.
 *
 * \param sdna: Old #SDNA.
 * \param type: Current field type name.
 * \param name: Current field name.
 * \param old: Pointer to struct information in #SDNA.
 * \return The offset or -1 on failure.
 *
 * \note Use #elem_exists_without_alias if additional information provided by this function
 * is not needed.
 *
 * \note We could have a version of this function that
 * returns the #SDNA_StructMember currently it's not needed.
 */
static int elem_offset_without_alias(const SDNA *sdna,
                                     const char *type,
                                     const char *name,
                                     const SDNA_Struct *old)
{
  return elem_offset_impl(sdna, sdna->types, sdna->members, type, name, old);
}

/**
 * A version of #elem_exists_without_alias that uses aliases.
 */
static int elem_offset_with_alias(const SDNA *sdna,
                                  const char *type,
                                  const char *name,
                                  const SDNA_Struct *old)
{
  return elem_offset_impl(sdna, sdna->alias.types, sdna->alias.members, type, name, old);
}

/* Each struct member belongs to one of the categories below. */
enum eStructMemberCategory {
  STRUCT_MEMBER_CATEGORY_STRUCT,
  STRUCT_MEMBER_CATEGORY_PRIMITIVE,
  STRUCT_MEMBER_CATEGORY_POINTER,
};

static eStructMemberCategory get_struct_member_category(const SDNA *sdna,
                                                        const SDNA_StructMember *member)
{
  const char *member_name = sdna->members[member->member_index];
  if (ispointer(member_name)) {
    return STRUCT_MEMBER_CATEGORY_POINTER;
  }
  const char *member_type_name = sdna->types[member->type_index];
  if (DNA_struct_exists_without_alias(sdna, member_type_name)) {
    return STRUCT_MEMBER_CATEGORY_STRUCT;
  }
  return STRUCT_MEMBER_CATEGORY_PRIMITIVE;
}

static int get_member_size_in_bytes(const SDNA *sdna, const SDNA_StructMember *member)
{
  const char *name = sdna->members[member->member_index];
  const int array_length = sdna->members_array_num[member->member_index];
  if (ispointer(name)) {
    return sdna->pointer_size * array_length;
  }
  const int type_size = sdna->types_size[member->type_index];
  return type_size * array_length;
}

enum eReconstructStepType {
  RECONSTRUCT_STEP_MEMCPY,
  RECONSTRUCT_STEP_CAST_PRIMITIVE,
  RECONSTRUCT_STEP_CAST_POINTER_TO_32,
  RECONSTRUCT_STEP_CAST_POINTER_TO_64,
  RECONSTRUCT_STEP_SUBSTRUCT,
  RECONSTRUCT_STEP_INIT_ZERO,
};

struct ReconstructStep {
  eReconstructStepType type;
  union {
    struct {
      int old_offset;
      int new_offset;
      int size;
    } memcpy;
    struct {
      int old_offset;
      int new_offset;
      int array_len;
      eSDNA_Type old_type;
      eSDNA_Type new_type;
    } cast_primitive;
    struct {
      int old_offset;
      int new_offset;
      int array_len;
    } cast_pointer;
    struct {
      int old_offset;
      int new_offset;
      int array_len;
      short old_struct_index;
      short new_struct_index;
    } substruct;
  } data;
};

struct DNA_ReconstructInfo {
  const SDNA *oldsdna;
  const SDNA *newsdna;
  const char *compare_flags;

  int *step_counts;
  ReconstructStep **steps;
};

static void reconstruct_structs(const DNA_ReconstructInfo *reconstruct_info,
                                const int blocks,
                                const int old_struct_index,
                                const int new_struct_index,
                                const char *old_blocks,
                                char *new_blocks);

/**
 * Converts the contents of an entire struct from oldsdna to newsdna format.
 *
 * \param reconstruct_info: Preprocessed reconstruct information generated by
 * #DNA_reconstruct_info_create.
 * \param new_struct_nr: Index in `newsdna->structs` of the struct that is being reconstructed.
 * \param old_block: Memory buffer containing the old struct.
 * \param new_block: Where to put converted struct contents.
 */
static void reconstruct_struct(const DNA_ReconstructInfo *reconstruct_info,
                               const int new_struct_index,
                               const char *old_block,
                               char *new_block)
{
  const ReconstructStep *steps = reconstruct_info->steps[new_struct_index];
  const int step_count = reconstruct_info->step_counts[new_struct_index];

  /* Execute all preprocessed steps. */
  for (int a = 0; a < step_count; a++) {
    const ReconstructStep *step = &steps[a];
    switch (step->type) {
      case RECONSTRUCT_STEP_MEMCPY:
        memcpy(new_block + step->data.memcpy.new_offset,
               old_block + step->data.memcpy.old_offset,
               step->data.memcpy.size);
        break;
      case RECONSTRUCT_STEP_CAST_PRIMITIVE:
        cast_primitive_type(step->data.cast_primitive.old_type,
                            step->data.cast_primitive.new_type,
                            step->data.cast_primitive.array_len,
                            old_block + step->data.cast_primitive.old_offset,
                            new_block + step->data.cast_primitive.new_offset);
        break;
      case RECONSTRUCT_STEP_CAST_POINTER_TO_32:
        cast_pointer_64_to_32(step->data.cast_pointer.array_len,
                              (const uint64_t *)(old_block + step->data.cast_pointer.old_offset),
                              (uint32_t *)(new_block + step->data.cast_pointer.new_offset));
        break;
      case RECONSTRUCT_STEP_CAST_POINTER_TO_64:
        cast_pointer_32_to_64(step->data.cast_pointer.array_len,
                              (const uint32_t *)(old_block + step->data.cast_pointer.old_offset),
                              (uint64_t *)(new_block + step->data.cast_pointer.new_offset));
        break;
      case RECONSTRUCT_STEP_SUBSTRUCT:
        reconstruct_structs(reconstruct_info,
                            step->data.substruct.array_len,
                            step->data.substruct.old_struct_index,
                            step->data.substruct.new_struct_index,
                            old_block + step->data.substruct.old_offset,
                            new_block + step->data.substruct.new_offset);
        break;
      case RECONSTRUCT_STEP_INIT_ZERO:
        /* Do nothing, because the memory block are zeroed (from #MEM_callocN).
         *
         * Note that the struct could be initialized with the default struct,
         * however this complicates versioning, especially with flags, see: D4500. */
        break;
    }
  }
}

/** Reconstructs an array of structs. */
static void reconstruct_structs(const DNA_ReconstructInfo *reconstruct_info,
                                const int blocks,
                                const int old_struct_index,
                                const int new_struct_index,
                                const char *old_blocks,
                                char *new_blocks)
{
  const SDNA_Struct *old_struct = reconstruct_info->oldsdna->structs[old_struct_index];
  const SDNA_Struct *new_struct = reconstruct_info->newsdna->structs[new_struct_index];

  const int old_block_size = reconstruct_info->oldsdna->types_size[old_struct->type_index];
  const int new_block_size = reconstruct_info->newsdna->types_size[new_struct->type_index];

  for (int a = 0; a < blocks; a++) {
    const char *old_block = old_blocks + a * old_block_size;
    char *new_block = new_blocks + a * new_block_size;
    reconstruct_struct(reconstruct_info, new_struct_index, old_block, new_block);
  }
}

void *DNA_struct_reconstruct(const DNA_ReconstructInfo *reconstruct_info,
                             int old_struct_index,
                             int blocks,
                             const void *old_blocks,
                             const char *alloc_name)
{
  const SDNA *oldsdna = reconstruct_info->oldsdna;
  const SDNA *newsdna = reconstruct_info->newsdna;

  const SDNA_Struct *old_struct = oldsdna->structs[old_struct_index];
  const char *type_name = oldsdna->types[old_struct->type_index];
  const int new_struct_index = DNA_struct_find_index_without_alias(newsdna, type_name);

  if (new_struct_index == -1) {
    return nullptr;
  }

  const SDNA_Struct *new_struct = newsdna->structs[new_struct_index];
  const int new_block_size = newsdna->types_size[new_struct->type_index];

  const int alignment = DNA_struct_alignment(newsdna, new_struct_index);
  char *new_blocks = static_cast<char *>(
      MEM_calloc_arrayN_aligned(new_block_size, blocks, alignment, alloc_name));
  reconstruct_structs(reconstruct_info,
                      blocks,
                      old_struct_index,
                      new_struct_index,
                      static_cast<const char *>(old_blocks),
                      new_blocks);
  return new_blocks;
}

/** Finds a member in the given struct with the given name. */
static const SDNA_StructMember *find_member_with_matching_name(const SDNA *sdna,
                                                               const SDNA_Struct *struct_info,
                                                               const char *name,
                                                               int *r_offset)
{
  int offset = 0;
  for (int a = 0; a < struct_info->members_num; a++) {
    const SDNA_StructMember *member = &struct_info->members[a];
    const char *member_name = sdna->members[member->member_index];
    if (elem_streq(name, member_name)) {
      *r_offset = offset;
      return member;
    }
    offset += get_member_size_in_bytes(sdna, member);
  }
  return nullptr;
}

/** Initializes a single reconstruct step for a member in the new struct. */
static void init_reconstruct_step_for_member(const SDNA *oldsdna,
                                             const SDNA *newsdna,
                                             const char *compare_flags,
                                             const SDNA_Struct *old_struct,
                                             const SDNA_StructMember *new_member,
                                             const int new_member_offset,
                                             ReconstructStep *r_step)
{

  /* Find the matching old member. */
  int old_member_offset;
  const char *new_name = newsdna->members[new_member->member_index];
  const SDNA_StructMember *old_member = find_member_with_matching_name(
      oldsdna, old_struct, new_name, &old_member_offset);

  if (old_member == nullptr) {
    /* No matching member has been found in the old struct. */
    r_step->type = RECONSTRUCT_STEP_INIT_ZERO;
    return;
  }

  /* Determine the member category of the old an new members. */
  const eStructMemberCategory new_category = get_struct_member_category(newsdna, new_member);
  const eStructMemberCategory old_category = get_struct_member_category(oldsdna, old_member);

  if (new_category != old_category) {
    /* Can only reconstruct the new member based on the old member, when the belong to the same
     * category. */
    r_step->type = RECONSTRUCT_STEP_INIT_ZERO;
    return;
  }

  const int new_array_length = newsdna->members_array_num[new_member->member_index];
  const int old_array_length = oldsdna->members_array_num[old_member->member_index];
  const int shared_array_length = std::min(new_array_length, old_array_length);

  const char *new_type_name = newsdna->types[new_member->type_index];
  const char *old_type_name = oldsdna->types[old_member->type_index];

  switch (new_category) {
    case STRUCT_MEMBER_CATEGORY_STRUCT: {
      if (STREQ(new_type_name, old_type_name)) {
        const int old_struct_index = DNA_struct_find_index_without_alias(oldsdna, old_type_name);
        BLI_assert(old_struct_index != -1);
        enum eSDNA_StructCompare compare_flag = eSDNA_StructCompare(
            compare_flags[old_struct_index]);
        BLI_assert(compare_flag != SDNA_CMP_REMOVED);
        if (compare_flag == SDNA_CMP_EQUAL) {
          /* The old and new members are identical, just do a #memcpy. */
          r_step->type = RECONSTRUCT_STEP_MEMCPY;
          r_step->data.memcpy.new_offset = new_member_offset;
          r_step->data.memcpy.old_offset = old_member_offset;
          r_step->data.memcpy.size = newsdna->types_size[new_member->type_index] *
                                     shared_array_length;
        }
        else {
          const int new_struct_index = DNA_struct_find_index_without_alias(newsdna, new_type_name);
          BLI_assert(new_struct_index != -1);

          /* The old and new members are different, use recursion to reconstruct the
           * nested struct. */
          BLI_assert(compare_flag == SDNA_CMP_NOT_EQUAL);
          r_step->type = RECONSTRUCT_STEP_SUBSTRUCT;
          r_step->data.substruct.new_offset = new_member_offset;
          r_step->data.substruct.old_offset = old_member_offset;
          r_step->data.substruct.array_len = shared_array_length;
          r_step->data.substruct.new_struct_index = new_struct_index;
          r_step->data.substruct.old_struct_index = old_struct_index;
        }
      }
      else {
        /* Cannot match structs that have different names. */
        r_step->type = RECONSTRUCT_STEP_INIT_ZERO;
      }
      break;
    }
    case STRUCT_MEMBER_CATEGORY_PRIMITIVE: {
      if (STREQ(new_type_name, old_type_name)) {
        /* Primitives with the same name cannot be different, so just do a #memcpy. */
        r_step->type = RECONSTRUCT_STEP_MEMCPY;
        r_step->data.memcpy.new_offset = new_member_offset;
        r_step->data.memcpy.old_offset = old_member_offset;
        r_step->data.memcpy.size = newsdna->types_size[new_member->type_index] *
                                   shared_array_length;
      }
      else {
        /* The old and new primitive types are different, cast from the old to new type. */
        r_step->type = RECONSTRUCT_STEP_CAST_PRIMITIVE;
        r_step->data.cast_primitive.array_len = shared_array_length;
        r_step->data.cast_primitive.new_offset = new_member_offset;
        r_step->data.cast_primitive.old_offset = old_member_offset;
        r_step->data.cast_primitive.new_type = eSDNA_Type(new_member->type_index);
        r_step->data.cast_primitive.old_type = eSDNA_Type(old_member->type_index);
      }
      break;
    }
    case STRUCT_MEMBER_CATEGORY_POINTER: {
      if (newsdna->pointer_size == oldsdna->pointer_size) {
        /* The pointer size is the same, so just do a #memcpy. */
        r_step->type = RECONSTRUCT_STEP_MEMCPY;
        r_step->data.memcpy.new_offset = new_member_offset;
        r_step->data.memcpy.old_offset = old_member_offset;
        r_step->data.memcpy.size = newsdna->pointer_size * shared_array_length;
      }
      else if (newsdna->pointer_size == 8 && oldsdna->pointer_size == 4) {
        /* Need to convert from 32 bit to 64 bit pointers. */
        r_step->type = RECONSTRUCT_STEP_CAST_POINTER_TO_64;
        r_step->data.cast_pointer.new_offset = new_member_offset;
        r_step->data.cast_pointer.old_offset = old_member_offset;
        r_step->data.cast_pointer.array_len = shared_array_length;
      }
      else if (newsdna->pointer_size == 4 && oldsdna->pointer_size == 8) {
        /* Need to convert from 64 bit to 32 bit pointers. */
        r_step->type = RECONSTRUCT_STEP_CAST_POINTER_TO_32;
        r_step->data.cast_pointer.new_offset = new_member_offset;
        r_step->data.cast_pointer.old_offset = old_member_offset;
        r_step->data.cast_pointer.array_len = shared_array_length;
      }
      else {
        BLI_assert_msg(0, "invalid pointer size");
        r_step->type = RECONSTRUCT_STEP_INIT_ZERO;
      }
      break;
    }
  }
}

/** Useful function when debugging the reconstruct steps. */
[[maybe_unused]] static void print_reconstruct_step(const ReconstructStep *step,
                                                    const SDNA *oldsdna,
                                                    const SDNA *newsdna)
{
  switch (step->type) {
    case RECONSTRUCT_STEP_INIT_ZERO: {
      printf("initialize zero");
      break;
    }
    case RECONSTRUCT_STEP_MEMCPY: {
      printf("memcpy, size: %d, old offset: %d, new offset: %d",
             step->data.memcpy.size,
             step->data.memcpy.old_offset,
             step->data.memcpy.new_offset);
      break;
    }
    case RECONSTRUCT_STEP_CAST_PRIMITIVE: {
      printf(
          "cast element, old type: %d ('%s'), new type: %d ('%s'), old offset: %d, new offset: "
          "%d, length: %d",
          int(step->data.cast_primitive.old_type),
          oldsdna->types[step->data.cast_primitive.old_type],
          int(step->data.cast_primitive.new_type),
          newsdna->types[step->data.cast_primitive.new_type],
          step->data.cast_primitive.old_offset,
          step->data.cast_primitive.new_offset,
          step->data.cast_primitive.array_len);
      break;
    }
    case RECONSTRUCT_STEP_CAST_POINTER_TO_32: {
      printf("pointer to 32, old offset: %d, new offset: %d, length: %d",
             step->data.cast_pointer.old_offset,
             step->data.cast_pointer.new_offset,
             step->data.cast_pointer.array_len);
      break;
    }
    case RECONSTRUCT_STEP_CAST_POINTER_TO_64: {
      printf("pointer to 64, old offset: %d, new offset: %d, length: %d",
             step->data.cast_pointer.old_offset,
             step->data.cast_pointer.new_offset,
             step->data.cast_pointer.array_len);
      break;
    }
    case RECONSTRUCT_STEP_SUBSTRUCT: {
      printf(
          "substruct, old offset: %d, new offset: %d, new struct: %d ('%s', size per struct: %d), "
          "length: %d",
          step->data.substruct.old_offset,
          step->data.substruct.new_offset,
          step->data.substruct.new_struct_index,
          newsdna->types[newsdna->structs[step->data.substruct.new_struct_index]->type_index],
          newsdna->types_size[newsdna->structs[step->data.substruct.new_struct_index]->type_index],
          step->data.substruct.array_len);
      break;
    }
  }
}

/**
 * Generate an array of reconstruct steps for the given #new_struct. There will be one
 * reconstruct step for every member.
 */
static ReconstructStep *create_reconstruct_steps_for_struct(const SDNA *oldsdna,
                                                            const SDNA *newsdna,
                                                            const char *compare_flags,
                                                            const SDNA_Struct *old_struct,
                                                            const SDNA_Struct *new_struct)
{
  ReconstructStep *steps = MEM_calloc_arrayN<ReconstructStep>(new_struct->members_num, __func__);

  int new_member_offset = 0;
  for (int new_member_index = 0; new_member_index < new_struct->members_num; new_member_index++) {
    const SDNA_StructMember *new_member = &new_struct->members[new_member_index];
    init_reconstruct_step_for_member(oldsdna,
                                     newsdna,
                                     compare_flags,
                                     old_struct,
                                     new_member,
                                     new_member_offset,
                                     &steps[new_member_index]);
    new_member_offset += get_member_size_in_bytes(newsdna, new_member);
  }

  return steps;
}

/** Compresses an array of reconstruct steps in-place and returns the new step count. */
static int compress_reconstruct_steps(ReconstructStep *steps, const int old_step_count)
{
  int new_step_count = 0;
  for (int a = 0; a < old_step_count; a++) {
    ReconstructStep *step = &steps[a];
    switch (step->type) {
      case RECONSTRUCT_STEP_INIT_ZERO:
        /* These steps are simply removed. */
        break;
      case RECONSTRUCT_STEP_MEMCPY:
        if (new_step_count > 0) {
          /* Try to merge this memcpy step with the previous one. */
          ReconstructStep *prev_step = &steps[new_step_count - 1];
          if (prev_step->type == RECONSTRUCT_STEP_MEMCPY) {
            /* Check if there are no bytes between the blocks to copy. */
            if (prev_step->data.memcpy.old_offset + prev_step->data.memcpy.size ==
                    step->data.memcpy.old_offset &&
                prev_step->data.memcpy.new_offset + prev_step->data.memcpy.size ==
                    step->data.memcpy.new_offset)
            {
              prev_step->data.memcpy.size += step->data.memcpy.size;
              break;
            }
          }
        }
        steps[new_step_count] = *step;
        new_step_count++;
        break;
      case RECONSTRUCT_STEP_CAST_PRIMITIVE:
      case RECONSTRUCT_STEP_CAST_POINTER_TO_32:
      case RECONSTRUCT_STEP_CAST_POINTER_TO_64:
      case RECONSTRUCT_STEP_SUBSTRUCT:
        /* These steps are not changed at all for now. It should be possible to merge consecutive
         * steps of the same type, but it is not really worth it. */
        steps[new_step_count] = *step;
        new_step_count++;
        break;
    }
  }
  return new_step_count;
}

DNA_ReconstructInfo *DNA_reconstruct_info_create(const SDNA *oldsdna,
                                                 const SDNA *newsdna,
                                                 const char *compare_flags)
{
  DNA_ReconstructInfo *reconstruct_info = MEM_callocN<DNA_ReconstructInfo>(__func__);
  reconstruct_info->oldsdna = oldsdna;
  reconstruct_info->newsdna = newsdna;
  reconstruct_info->compare_flags = compare_flags;
  reconstruct_info->step_counts = MEM_malloc_arrayN<int>(size_t(newsdna->structs_num), __func__);
  reconstruct_info->steps = MEM_malloc_arrayN<ReconstructStep *>(size_t(newsdna->structs_num),
                                                                 __func__);

  /* Generate reconstruct steps for all structs. */
  for (int new_struct_index = 0; new_struct_index < newsdna->structs_num; new_struct_index++) {
    const SDNA_Struct *new_struct = newsdna->structs[new_struct_index];
    const char *new_struct_name = newsdna->types[new_struct->type_index];
    const int old_struct_index = DNA_struct_find_index_without_alias(oldsdna, new_struct_name);
    if (old_struct_index < 0) {
      reconstruct_info->steps[new_struct_index] = nullptr;
      reconstruct_info->step_counts[new_struct_index] = 0;
      continue;
    }
    const SDNA_Struct *old_struct = oldsdna->structs[old_struct_index];
    ReconstructStep *steps = create_reconstruct_steps_for_struct(
        oldsdna, newsdna, compare_flags, old_struct, new_struct);

    /* Comment the line below to skip the compression for debugging purposes. */
    const int steps_len = compress_reconstruct_steps(steps, new_struct->members_num);

    reconstruct_info->steps[new_struct_index] = steps;
    reconstruct_info->step_counts[new_struct_index] = steps_len;

/* This is useful when debugging the reconstruct steps. */
#if 0
    printf("%s: \n", new_struct_name);
    for (int a = 0; a < steps_len; a++) {
      printf("  ");
      print_reconstruct_step(&steps[a], oldsdna, newsdna);
      printf("\n");
    }
#endif
  }

  return reconstruct_info;
}

void DNA_reconstruct_info_free(DNA_ReconstructInfo *reconstruct_info)
{
  for (int new_struct_index = 0; new_struct_index < reconstruct_info->newsdna->structs_num;
       new_struct_index++)
  {
    if (reconstruct_info->steps[new_struct_index] != nullptr) {
      MEM_freeN(reconstruct_info->steps[new_struct_index]);
    }
  }
  MEM_freeN(reconstruct_info->steps);
  MEM_freeN(reconstruct_info->step_counts);
  MEM_freeN(reconstruct_info);
}

int DNA_struct_member_offset_by_name_without_alias(const SDNA *sdna,
                                                   const char *stype,
                                                   const char *vartype,
                                                   const char *name)
{
  const int struct_index = DNA_struct_find_index_without_alias(sdna, stype);
  BLI_assert(struct_index != -1);
  const SDNA_Struct *const struct_info = sdna->structs[struct_index];
  return elem_offset_without_alias(sdna, vartype, name, struct_info);
}

int DNA_struct_member_offset_by_name_with_alias(const SDNA *sdna,
                                                const char *stype,
                                                const char *vartype,
                                                const char *name)
{
  const int struct_index = DNA_struct_find_with_alias(sdna, stype);
  BLI_assert(struct_index != -1);
  const SDNA_Struct *const struct_info = sdna->structs[struct_index];
  return elem_offset_with_alias(sdna, vartype, name, struct_info);
}

bool DNA_struct_exists_without_alias(const SDNA *sdna, const char *stype)
{
  return DNA_struct_find_index_without_alias(sdna, stype) != -1;
}

bool DNA_struct_member_exists_without_alias(const SDNA *sdna,
                                            const char *stype,
                                            const char *vartype,
                                            const char *name)
{
  const int struct_index = DNA_struct_find_index_without_alias(sdna, stype);

  if (struct_index != -1) {
    const SDNA_Struct *const struct_info = sdna->structs[struct_index];
    const bool found = elem_exists_without_alias(sdna, vartype, name, struct_info);

    if (found) {
      return true;
    }
  }
  return false;
}

bool DNA_struct_member_exists_with_alias(const SDNA *sdna,
                                         const char *stype,
                                         const char *vartype,
                                         const char *name)
{
  const int SDNAnr = DNA_struct_find_with_alias(sdna, stype);

  if (SDNAnr != -1) {
    const SDNA_Struct *const spo = sdna->structs[SDNAnr];
    const bool found = elem_exists_with_alias(sdna, vartype, name, spo);

    if (found) {
      return true;
    }
  }
  return false;
}

int DNA_elem_type_size(const eSDNA_Type elem_nr)
{
  /* should contain all enum types */
  switch (elem_nr) {
    case SDNA_TYPE_CHAR:
    case SDNA_TYPE_UCHAR:
    case SDNA_TYPE_INT8:
      return 1;
    case SDNA_TYPE_SHORT:
    case SDNA_TYPE_USHORT:
      return 2;
    case SDNA_TYPE_INT:
    case SDNA_TYPE_FLOAT:
      return 4;
    case SDNA_TYPE_DOUBLE:
    case SDNA_TYPE_INT64:
    case SDNA_TYPE_UINT64:
      return 8;
    case SDNA_TYPE_RAW_DATA:
      BLI_assert_msg(false, "Operations on the size of SDNA_TYPE_RAW_DATA is not supported");
      return 0;
  }

  /* weak */
  return 8;
}

int DNA_struct_alignment(const SDNA *sdna, const int struct_index)
{
  return sdna->types_alignment[struct_index];
}

const char *DNA_struct_identifier(SDNA *sdna, const int struct_index)
{
  DNA_sdna_alias_data_ensure(sdna);
  const SDNA_Struct *struct_info = sdna->structs[struct_index];
  return sdna->alias.types[struct_info->type_index];
}

/* -------------------------------------------------------------------- */
/** \name Version Patch DNA
 * \{ */

static bool DNA_sdna_patch_struct(SDNA *sdna, const int struct_index, const char *new_type_name)
{
  BLI_assert(DNA_struct_find_index_without_alias(DNA_sdna_current_get(), new_type_name) != -1);
  const SDNA_Struct *struct_info = sdna->structs[struct_index];
#ifdef WITH_DNA_GHASH
  BLI_ghash_remove(
      sdna->types_to_structs_map, (void *)sdna->types[struct_info->type_index], nullptr, nullptr);
  BLI_ghash_insert(
      sdna->types_to_structs_map, (void *)new_type_name, POINTER_FROM_INT(struct_index));
#endif
  sdna->types[struct_info->type_index] = new_type_name;
  return true;
}
bool DNA_sdna_patch_struct_by_name(SDNA *sdna,
                                   const char *old_type_name,
                                   const char *new_type_name)
{
  const int struct_index = DNA_struct_find_index_without_alias(sdna, old_type_name);
  if (struct_index != -1) {
    return DNA_sdna_patch_struct(sdna, struct_index, new_type_name);
  }
  return false;
}

/* Make public if called often with same struct (avoid duplicate lookups). */
static bool DNA_sdna_patch_struct_member(SDNA *sdna,
                                         const int struct_index,
                                         const char *old_member_name,
                                         const char *new_member_name)
{
  /* These names aren't handled here (it's not used).
   * Ensure they are never used or we get out of sync arrays. */
  BLI_assert(sdna->alias.members == nullptr);
  const int old_member_name_len = strlen(old_member_name);
  const int new_member_name_len = strlen(new_member_name);
  BLI_assert(new_member_name != nullptr);
  SDNA_Struct *struct_info = sdna->structs[struct_index];
  for (int struct_member_index = struct_info->members_num; struct_member_index > 0;
       struct_member_index--)
  {
    SDNA_StructMember *member_info = &struct_info->members[struct_member_index];
    const char *old_member_name_full = sdna->members[member_info->member_index];
    /* Start & end offsets in #old_member_full. */
    uint old_member_name_full_offset_start;
    if (DNA_member_id_match(old_member_name,
                            old_member_name_len,
                            old_member_name_full,
                            &old_member_name_full_offset_start))
    {
      if (sdna->mem_arena == nullptr) {
        sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }
      const char *new_member_name_full = DNA_member_id_rename(sdna->mem_arena,
                                                              old_member_name,
                                                              old_member_name_len,
                                                              new_member_name,
                                                              new_member_name_len,
                                                              old_member_name_full,
                                                              strlen(old_member_name_full),
                                                              old_member_name_full_offset_start);

      if (sdna->members_num == sdna->members_num_alloc) {
        sdna->members_num_alloc += 64;
        sdna->members = static_cast<const char **>(MEM_recallocN(
            (void *)sdna->members, sizeof(*sdna->members) * sdna->members_num_alloc));
        sdna->members_array_num = static_cast<short int *>(
            MEM_recallocN((void *)sdna->members_array_num,
                          sizeof(*sdna->members_array_num) * sdna->members_num_alloc));
      }
      const short old_member_index = member_info->member_index;
      member_info->member_index = sdna->members_num++;
      sdna->members[member_info->member_index] = new_member_name_full;
      sdna->members_array_num[member_info->member_index] =
          sdna->members_array_num[old_member_index];

      return true;
    }
  }
  return false;
}
bool DNA_sdna_patch_struct_member_by_name(SDNA *sdna,
                                          const char *type_name,
                                          const char *old_member_name,
                                          const char *new_member_name)
{
  const int struct_index = DNA_struct_find_index_without_alias(sdna, type_name);
  if (struct_index != -1) {
    return DNA_sdna_patch_struct_member(sdna, struct_index, old_member_name, new_member_name);
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Versioning (Forward Compatible)
 *
 * Versioning that allows new names.
 * \{ */

/**
 * Names are shared between structs which causes problems renaming.
 * Make sure every struct member gets its own name so renaming only ever impacts a single struct.
 *
 * The resulting SDNA is never written to disk.
 */
static void sdna_expand_names(SDNA *sdna)
{
  int names_expand_len = 0;
  for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
    const SDNA_Struct *struct_old = sdna->structs[struct_index];
    names_expand_len += struct_old->members_num;
  }
  const char **names_expand = MEM_malloc_arrayN<const char *>(size_t(names_expand_len), __func__);
  short *names_array_len_expand = MEM_malloc_arrayN<short>(size_t(names_expand_len), __func__);

  int names_expand_index = 0;
  for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
    /* We can't edit this memory 'sdna->structs' points to (read-only `datatoc` file). */
    const SDNA_Struct *struct_old = sdna->structs[struct_index];

    const int array_size = sizeof(short) * 2 + sizeof(SDNA_StructMember) * struct_old->members_num;
    SDNA_Struct *struct_new = static_cast<SDNA_Struct *>(
        BLI_memarena_alloc(sdna->mem_arena, array_size));
    memcpy(struct_new, struct_old, array_size);
    sdna->structs[struct_index] = struct_new;

    for (int i = 0; i < struct_old->members_num; i++) {
      const SDNA_StructMember *member_old = &struct_old->members[i];
      SDNA_StructMember *member_new = &struct_new->members[i];

      names_expand[names_expand_index] = sdna->members[member_old->member_index];
      names_array_len_expand[names_expand_index] =
          sdna->members_array_num[member_old->member_index];

      BLI_assert(names_expand_index < SHRT_MAX);
      member_new->member_index = names_expand_index;
      names_expand_index++;
    }
  }
  MEM_freeN(sdna->members);
  sdna->members = names_expand;

  MEM_freeN(sdna->members_array_num);
  sdna->members_array_num = names_array_len_expand;

  sdna->members_num = names_expand_len;
}

static const char *dna_sdna_alias_from_static_elem_full(SDNA *sdna,
                                                        GHash *elem_map_alias_from_static,
                                                        const char *struct_name_static,
                                                        const char *elem_static_full)
{
  const int elem_static_full_len = strlen(elem_static_full);
  char *elem_static = static_cast<char *>(alloca(elem_static_full_len + 1));
  const int elem_static_len = DNA_member_id_strip_copy(elem_static, elem_static_full);
  const char *str_pair[2] = {struct_name_static, elem_static};
  const char *elem_alias = static_cast<const char *>(
      BLI_ghash_lookup(elem_map_alias_from_static, str_pair));
  if (elem_alias) {
    return DNA_member_id_rename(sdna->mem_arena,
                                elem_static,
                                elem_static_len,
                                elem_alias,
                                strlen(elem_alias),
                                elem_static_full,
                                elem_static_full_len,
                                DNA_member_id_offset_start(elem_static_full));
  }
  return nullptr;
}

void DNA_sdna_alias_data_ensure(SDNA *sdna)
{
  if (sdna->alias.members && sdna->alias.types) {
    return;
  }

  /* We may want this to be optional later. */
  const bool use_legacy_hack = true;

  if (sdna->mem_arena == nullptr) {
    sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  }

  GHash *type_map_alias_from_static;
  GHash *member_map_alias_from_static;

  DNA_alias_maps(
      DNA_RENAME_ALIAS_FROM_STATIC, &type_map_alias_from_static, &member_map_alias_from_static);

  if (sdna->alias.types == nullptr) {
    sdna->alias.types = MEM_malloc_arrayN<const char *>(size_t(sdna->types_num), __func__);
    for (int type_index = 0; type_index < sdna->types_num; type_index++) {
      const char *type_name_static = sdna->types[type_index];

      if (use_legacy_hack) {
        type_name_static = DNA_struct_rename_legacy_hack_alias_from_static(type_name_static);
      }

      sdna->alias.types[type_index] = static_cast<const char *>(BLI_ghash_lookup_default(
          type_map_alias_from_static, type_name_static, (void *)type_name_static));
    }
  }

  if (sdna->alias.members == nullptr) {
    sdna_expand_names(sdna);
    sdna->alias.members = MEM_malloc_arrayN<const char *>(size_t(sdna->members_num), __func__);
    for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
      const SDNA_Struct *struct_info = sdna->structs[struct_index];
      const char *struct_name_static = sdna->types[struct_info->type_index];

      if (use_legacy_hack) {
        struct_name_static = DNA_struct_rename_legacy_hack_alias_from_static(struct_name_static);
      }

      for (int a = 0; a < struct_info->members_num; a++) {
        const SDNA_StructMember *member = &struct_info->members[a];
        const char *member_alias_full = dna_sdna_alias_from_static_elem_full(
            sdna,
            member_map_alias_from_static,
            struct_name_static,
            sdna->members[member->member_index]);
        if (member_alias_full != nullptr) {
          sdna->alias.members[member->member_index] = member_alias_full;
        }
        else {
          sdna->alias.members[member->member_index] = sdna->members[member->member_index];
        }
      }
    }
  }
  BLI_ghash_free(type_map_alias_from_static, nullptr, nullptr);
  BLI_ghash_free(member_map_alias_from_static, MEM_freeN, nullptr);
}

void DNA_sdna_alias_data_ensure_structs_map(SDNA *sdna)
{
  if (sdna->alias.types_to_structs_map) {
    return;
  }

  DNA_sdna_alias_data_ensure(sdna);
#ifdef WITH_DNA_GHASH
  /* create a ghash lookup to speed up */
  GHash *type_to_struct_index_map = BLI_ghash_str_new_ex(__func__, sdna->structs_num);
  for (intptr_t struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
    const SDNA_Struct *struct_info = sdna->structs[struct_index];
    BLI_ghash_insert(type_to_struct_index_map,
                     (void *)sdna->alias.types[struct_info->type_index],
                     POINTER_FROM_INT(struct_index));
  }
  sdna->alias.types_to_structs_map = type_to_struct_index_map;
#else
  UNUSED_VARS(sdna);
#endif
}

namespace blender::dna::pointers {

PointersInDNA::PointersInDNA(const SDNA &sdna) : sdna_(sdna)
{
  structs_.resize(sdna.structs_num);
  for (const int struct_i : IndexRange(sdna.structs_num)) {
    const SDNA_Struct &sdna_struct = *sdna.structs[struct_i];
    StructInfo &struct_info = structs_[struct_i];

    struct_info.size_in_bytes = 0;
    for (const int member_i : IndexRange(sdna_struct.members_num)) {
      struct_info.size_in_bytes += get_member_size_in_bytes(&sdna_,
                                                            &sdna_struct.members[member_i]);
    }

    this->gather_pointer_members_recursive(sdna_struct, 0, structs_[struct_i]);
  }
}

void PointersInDNA::gather_pointer_members_recursive(const SDNA_Struct &sdna_struct,
                                                     int initial_offset,
                                                     StructInfo &r_struct_info) const
{
  int offset = initial_offset;
  for (const int member_i : IndexRange(sdna_struct.members_num)) {
    const SDNA_StructMember &member = sdna_struct.members[member_i];
    const char *member_type_name = sdna_.types[member.type_index];
    const eStructMemberCategory member_category = get_struct_member_category(&sdna_, &member);
    const int array_elem_num = sdna_.members_array_num[member.member_index];

    if (member_category == STRUCT_MEMBER_CATEGORY_POINTER) {
      for (int elem_i = 0; elem_i < array_elem_num; elem_i++) {
        const char *member_name = sdna_.members[member.member_index];
        r_struct_info.pointers.append(
            {offset + elem_i * sdna_.pointer_size, member_type_name, member_name});
      }
    }
    else if (member_category == STRUCT_MEMBER_CATEGORY_STRUCT) {
      const int substruct_i = DNA_struct_find_index_without_alias(&sdna_, member_type_name);
      const SDNA_Struct &sub_sdna_struct = *sdna_.structs[substruct_i];
      int substruct_size = sdna_.types_size[member.type_index];
      for (int elem_i = 0; elem_i < array_elem_num; elem_i++) {
        this->gather_pointer_members_recursive(
            sub_sdna_struct, offset + elem_i * substruct_size, r_struct_info);
      }
    }
    offset += get_member_size_in_bytes(&sdna_, &member);
  }
}

}  // namespace blender::dna::pointers

/** \} */

/* -------------------------------------------------------------------- */
/** \name Print DNA structs
 *
 * \{ */

namespace blender::dna {

static void print_struct_array_recursive(const SDNA &sdna,
                                         const SDNA_Struct &sdna_struct,
                                         const void *initial_data,
                                         const int64_t element_num,
                                         const int indent,
                                         fmt::appender &dst);
static void print_single_struct_recursive(const SDNA &sdna,
                                          const SDNA_Struct &sdna_struct,
                                          const void *initial_data,
                                          const int indent,
                                          fmt::appender &dst);

/**
 * Uses a heuristic to detect if a char array should be printed as string.
 */
static bool char_array_startswith_simple_name(const char *data, const int array_len)
{
  const int string_length = strnlen(data, array_len);
  if (string_length == array_len) {
    return false;
  }
  for (const int i : IndexRange(string_length)) {
    const uchar c = data[i];
    /* This is only a very simple check and does not cover more complex cases with multi-byte UTF8
     * characters. It's only a heuristic anyway, making a wrong decision here just means that the
     * data will be printed differently. */
    if (!std::isprint(c)) {
      return false;
    }
  }
  return true;
}

static void print_struct_array_recursive(const SDNA &sdna,
                                         const SDNA_Struct &sdna_struct,
                                         const void *data,
                                         const int64_t element_num,
                                         const int indent,
                                         fmt::appender &dst)
{
  if (element_num == 1) {
    print_single_struct_recursive(sdna, sdna_struct, data, indent, dst);
    return;
  }

  const char *struct_name = sdna.types[sdna_struct.type_index];
  const int64_t struct_size = sdna.types_size[sdna_struct.type_index];
  for (const int64_t i : IndexRange(element_num)) {
    const void *element_data = POINTER_OFFSET(data, i * struct_size);
    fmt::format_to(dst, "{:{}}{}: <{}>\n", "", indent, i, struct_name);
    print_single_struct_recursive(sdna, sdna_struct, element_data, indent + 2, dst);
  }
}

static void print_single_struct_recursive(const SDNA &sdna,
                                          const SDNA_Struct &sdna_struct,
                                          const void *initial_data,
                                          const int indent,
                                          fmt::appender &dst)
{
  using namespace blender;
  const void *data = initial_data;

  for (const int member_i : IndexRange(sdna_struct.members_num)) {
    const SDNA_StructMember &member = sdna_struct.members[member_i];
    const char *member_type_name = sdna.types[member.type_index];
    const char *member_name = sdna.members[member.member_index];
    const eStructMemberCategory member_category = get_struct_member_category(&sdna, &member);
    const int member_array_len = sdna.members_array_num[member.member_index];

    fmt::format_to(dst, "{:{}}{} {}:", "", indent, member_type_name, member_name);

    if (member_category == STRUCT_MEMBER_CATEGORY_PRIMITIVE &&
        member.type_index == SDNA_TYPE_CHAR && member_array_len > 1)
    {
      const char *str_data = static_cast<const char *>(data);
      fmt::format_to(dst, " ");
      if (char_array_startswith_simple_name(str_data, member_array_len)) {
        fmt::format_to(dst, "'{}'", str_data);
      }
      else {
        for (const int i : IndexRange(member_array_len)) {
          fmt::format_to(dst, "{} ", int(str_data[i]));
        }
      }
      fmt::format_to(dst, "\n");
    }
    else {
      switch (member_category) {
        case STRUCT_MEMBER_CATEGORY_STRUCT: {
          fmt::format_to(dst, "\n");
          const int substruct_i = DNA_struct_find_index_without_alias(&sdna, member_type_name);
          const SDNA_Struct &sub_sdna_struct = *sdna.structs[substruct_i];
          print_struct_array_recursive(
              sdna, sub_sdna_struct, data, member_array_len, indent + 2, dst);
          break;
        }
        case STRUCT_MEMBER_CATEGORY_PRIMITIVE: {
          fmt::format_to(dst, " ");
          const int type_size = sdna.types_size[member.type_index];
          const eSDNA_Type type = eSDNA_Type(member.type_index);
          for ([[maybe_unused]] const int elem_i : IndexRange(member_array_len)) {
            const void *current_data = POINTER_OFFSET(data, elem_i * type_size);
            switch (type) {
              case SDNA_TYPE_CHAR: {
                const char value = *reinterpret_cast<const char *>(current_data);
                fmt::format_to(dst, "{}", int(value));
                break;
              }
              case SDNA_TYPE_UCHAR: {
                const uchar value = *reinterpret_cast<const uchar *>(current_data);
                fmt::format_to(dst, "{}", int(value));
                break;
              }
              case SDNA_TYPE_INT8: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const int8_t *>(current_data));
                break;
              }
              case SDNA_TYPE_SHORT: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const short *>(current_data));
                break;
              }
              case SDNA_TYPE_USHORT: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const ushort *>(current_data));
                break;
              }
              case SDNA_TYPE_INT: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const int *>(current_data));
                break;
              }
              case SDNA_TYPE_FLOAT: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const float *>(current_data));
                break;
              }
              case SDNA_TYPE_DOUBLE: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const double *>(current_data));
                break;
              }
              case SDNA_TYPE_INT64: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const int64_t *>(current_data));
                break;
              }
              case SDNA_TYPE_UINT64: {
                fmt::format_to(dst, "{}", *reinterpret_cast<const uint64_t *>(current_data));
                break;
              }
              case SDNA_TYPE_RAW_DATA: {
                BLI_assert_unreachable();
                break;
              }
            }
            fmt::format_to(dst, " ");
          }
          fmt::format_to(dst, "\n");
          break;
        }
        case STRUCT_MEMBER_CATEGORY_POINTER: {
          for ([[maybe_unused]] const int elem_i : IndexRange(member_array_len)) {
            const void *current_data = POINTER_OFFSET(data, sdna.pointer_size * elem_i);
            fmt::format_to(dst, " {}", *reinterpret_cast<const void *const *>(current_data));
          }
          fmt::format_to(dst, "\n");
          break;
        }
      }
    }
    const int member_size = get_member_size_in_bytes(&sdna, &member);
    data = POINTER_OFFSET(data, member_size);
  }
}

void print_structs_at_address(const SDNA &sdna,
                              const int struct_id,
                              const void *initial_data,
                              const void *address,
                              const int64_t element_num,
                              std::ostream &stream)
{
  const SDNA_Struct &sdna_struct = *sdna.structs[struct_id];

  fmt::memory_buffer buf;
  fmt::appender dst{buf};

  const char *struct_name = sdna.types[sdna_struct.type_index];
  fmt::format_to(dst, "<{}> {}x at {}\n", struct_name, element_num, address);

  print_struct_array_recursive(sdna, sdna_struct, initial_data, element_num, 2, dst);
  stream << fmt::to_string(buf);
}

void print_struct_by_id(const int struct_id, const void *data)
{
  const SDNA &sdna = *DNA_sdna_current_get();
  print_structs_at_address(sdna, struct_id, data, data, 1, std::cout);
}

}  // namespace blender::dna

/** \} */
