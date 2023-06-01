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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h" /* for MEM_freeN MEM_mallocN MEM_callocN */

#include "BLI_endian_switch.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h" /* for SDNA ;-) */

/**
 * \section dna_genfile Overview
 *
 * - please NOTE: no builtin security to detect input of double structs
 * - if you want a struct not to be in DNA file: add two hash marks above it `(#<enter>#<enter>)`.
 *
 * Structure DNA data is added to each blender file and to each executable, this to detect
 * in .blend files new variables in structs, changed array sizes, etc. It's also used for
 * converting endian and pointer size (32-64 bits)
 * As an extra, Python uses a call to detect run-time the contents of a blender struct.
 *
 * Create a structDNA: only needed when one of the input include (.h) files change.
 * File Syntax:
 * \code{.unparsed}
 *     SDNA (4 bytes) (magic number)
 *     NAME (4 bytes)
 *     <nr> (4 bytes) amount of names (int)
 *     <string>
 *     <string>
 *     ...
 *     ...
 *     TYPE (4 bytes)
 *     <nr> amount of types (int)
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
 *     <nr> amount of structs (int)
 *     <typenr><nr_of_elems> <typenr><namenr> <typenr><namenr> ...
 * \endcode
 *
 * **Remember to read/write integer and short aligned!**
 *
 * While writing a file, the names of a struct is indicated with a type number,
 * to be found with: `type = DNA_struct_find_nr(SDNA *, const char *)`
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
 *  - Endian compatibility.
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

#ifdef __BIG_ENDIAN__
/* Big Endian */
#  define MAKE_ID(a, b, c, d) ((int)(a) << 24 | (int)(b) << 16 | (c) << 8 | (d))
#else
/* Little Endian */
#  define MAKE_ID(a, b, c, d) ((int)(d) << 24 | (int)(c) << 16 | (b) << 8 | (a))
#endif

/* ************************* DIV ********************** */

void DNA_sdna_free(SDNA *sdna)
{
  if (sdna->data_alloc) {
    MEM_freeN((void *)sdna->data);
  }

  MEM_freeN((void *)sdna->names);
  MEM_freeN((void *)sdna->names_array_len);
  MEM_freeN((void *)sdna->types);
  MEM_freeN(sdna->structs);

#ifdef WITH_DNA_GHASH
  if (sdna->structs_map) {
    BLI_ghash_free(sdna->structs_map, NULL, NULL);
  }
#endif

  if (sdna->mem_arena) {
    BLI_memarena_free(sdna->mem_arena);
  }

  MEM_SAFE_FREE(sdna->alias.names);
  MEM_SAFE_FREE(sdna->alias.types);
#ifdef WITH_DNA_GHASH
  if (sdna->alias.structs_map) {
    BLI_ghash_free(sdna->alias.structs_map, NULL, NULL);
  }
#endif

  MEM_freeN(sdna);
}

/**
 * Return true if the name indicates a pointer of some kind.
 */
static bool ispointer(const char *name)
{
  /* check if pointer or function pointer */
  return (name[0] == '*' || (name[0] == '(' && name[1] == '*'));
}

int DNA_elem_size_nr(const SDNA *sdna, short type, short name)
{
  const char *cp = sdna->names[name];
  int len = 0;

  /* is it a pointer or function pointer? */
  if (ispointer(cp)) {
    /* has the name an extra length? (array) */
    len = sdna->pointer_size * sdna->names_array_len[name];
  }
  else if (sdna->types_size[type]) {
    /* has the name an extra length? (array) */
    len = (int)sdna->types_size[type] * sdna->names_array_len[name];
  }

  return len;
}

#if 0
static void printstruct(SDNA *sdna, short strnr)
{
  /* is for debug */

  SDNA_Struct *struct_info = sdna->structs[strnr];
  printf("struct %s\n", sdna->types[struct_info->type]);

  for (int b = 0; b < struct_info->members_len; b++) {
    SDNA_StructMember *struct_member = &struct_info->members[b];
    printf("   %s %s\n",
           sdna->types[struct_member->type],
           sdna->names[struct_member->name]);
  }
}
#endif

/**
 * Returns the index of the struct info for the struct with the specified name.
 */
static int dna_struct_find_nr_ex_impl(
    /* From SDNA struct. */
    const char **types,
    const int UNUSED(types_len),
    SDNA_Struct **const structs,
    const int structs_len,
#ifdef WITH_DNA_GHASH
    GHash *structs_map,
#endif
    /* Regular args. */
    const char *str,
    uint *index_last)
{
  if (*index_last < structs_len) {
    const SDNA_Struct *struct_info = structs[*index_last];
    if (STREQ(types[struct_info->type], str)) {
      return *index_last;
    }
  }

#ifdef WITH_DNA_GHASH
  {
    void **index_p = BLI_ghash_lookup_p(structs_map, str);
    if (index_p) {
      const int index = POINTER_AS_INT(*index_p);
      *index_last = index;
      return index;
    }
  }
#else
  {
    for (int index = 0; index < structs_len; index++) {
      const SDNA_Struct *struct_info = structs[index];
      if (STREQ(types[struct_info->type], str)) {
        *index_last = index;
        return index;
      }
    }
  }
#endif
  return -1;
}

int DNA_struct_find_nr_ex(const SDNA *sdna, const char *str, uint *index_last)
{
  return dna_struct_find_nr_ex_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->types_len,
      sdna->structs,
      sdna->structs_len,
#ifdef WITH_DNA_GHASH
      sdna->structs_map,
#endif
      /* Regular args. */
      str,
      index_last);
}

int DNA_struct_alias_find_nr_ex(const SDNA *sdna, const char *str, uint *index_last)
{
#ifdef WITH_DNA_GHASH
  BLI_assert(sdna->alias.structs_map != NULL);
#endif
  return dna_struct_find_nr_ex_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->types_len,
      sdna->structs,
      sdna->structs_len,
#ifdef WITH_DNA_GHASH
      sdna->alias.structs_map,
#endif
      /* Regular args. */
      str,
      index_last);
}

int DNA_struct_find_nr(const SDNA *sdna, const char *str)
{
  uint index_last_dummy = UINT_MAX;
  return DNA_struct_find_nr_ex(sdna, str, &index_last_dummy);
}

int DNA_struct_alias_find_nr(const SDNA *sdna, const char *str)
{
  uint index_last_dummy = UINT_MAX;
  return DNA_struct_alias_find_nr_ex(sdna, str, &index_last_dummy);
}

/* ************************* END DIV ********************** */

/* ************************* READ DNA ********************** */

BLI_INLINE const char *pad_up_4(const char *ptr)
{
  return (const char *)((((uintptr_t)ptr) + 3) & ~3);
}

/**
 * In sdna->data the data, now we convert that to something understandable
 */
static bool init_structDNA(SDNA *sdna, bool do_endian_swap, const char **r_error_message)
{
  int gravity_fix = -1;

  int *data = (int *)sdna->data;

  /* Clear pointers in case of error. */
  sdna->names = NULL;
  sdna->types = NULL;
  sdna->structs = NULL;
#ifdef WITH_DNA_GHASH
  sdna->structs_map = NULL;
#endif
  sdna->mem_arena = NULL;

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

    sdna->names_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->names_len);
    }
    sdna->names_len_alloc = sdna->names_len;

    data++;
    sdna->names = MEM_callocN(sizeof(void *) * sdna->names_len, "sdnanames");
  }
  else {
    *r_error_message = "NAME error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int nr = 0; nr < sdna->names_len; nr++) {
    sdna->names[nr] = cp;

    /* "float gravity [3]" was parsed wrong giving both "gravity" and
     * "[3]"  members. we rename "[3]", and later set the type of
     * "gravity" to "void" so the offsets work out correct */
    if (*cp == '[' && STREQ(cp, "[3]")) {
      if (nr && STREQ(sdna->names[nr - 1], "Cvi")) {
        sdna->names[nr] = "gravity[3]";
        gravity_fix = nr;
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

    sdna->types_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->types_len);
    }

    data++;
    sdna->types = MEM_callocN(sizeof(void *) * sdna->types_len, "sdnatypes");
  }
  else {
    *r_error_message = "TYPE error in SDNA file";
    return false;
  }

  cp = (char *)data;
  for (int nr = 0; nr < sdna->types_len; nr++) {
    /* WARNING! See: DNA_struct_rename_legacy_hack_static_from_alias docs. */
    sdna->types[nr] = DNA_struct_rename_legacy_hack_static_from_alias(cp);
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
    sp = (short *)data;
    sdna->types_size = sp;

    if (do_endian_swap) {
      BLI_endian_switch_int16_array(sp, sdna->types_len);
    }

    sp += sdna->types_len;
  }
  else {
    *r_error_message = "TLEN error in SDNA file";
    return false;
  }
  /* prevent BUS error */
  if (sdna->types_len & 1) {
    sp++;
  }

  /* Struct array ('STRC') */
  data = (int *)sp;
  if (*data == MAKE_ID('S', 'T', 'R', 'C')) {
    data++;

    sdna->structs_len = *data;
    if (do_endian_swap) {
      BLI_endian_switch_int32(&sdna->structs_len);
    }

    data++;
    sdna->structs = MEM_callocN(sizeof(SDNA_Struct *) * sdna->structs_len, "sdnastrcs");
  }
  else {
    *r_error_message = "STRC error in SDNA file";
    return false;
  }

  sp = (short *)data;
  for (int nr = 0; nr < sdna->structs_len; nr++) {
    SDNA_Struct *struct_info = (SDNA_Struct *)sp;
    sdna->structs[nr] = struct_info;

    if (do_endian_swap) {
      BLI_endian_switch_int16(&struct_info->type);
      BLI_endian_switch_int16(&struct_info->members_len);

      for (short a = 0; a < struct_info->members_len; a++) {
        SDNA_StructMember *member = &struct_info->members[a];
        BLI_endian_switch_int16(&member->type);
        BLI_endian_switch_int16(&member->name);
      }
    }
    sp += 2 + (sizeof(SDNA_StructMember) / sizeof(short)) * struct_info->members_len;
  }

  {
    /* second part of gravity problem, setting "gravity" type to void */
    if (gravity_fix > -1) {
      for (int nr = 0; nr < sdna->structs_len; nr++) {
        sp = (short *)sdna->structs[nr];
        if (STREQ(sdna->types[sp[0]], "ClothSimSettings")) {
          sp[10] = SDNA_TYPE_VOID;
        }
      }
    }
  }

#ifdef WITH_DNA_GHASH
  {
    /* create a ghash lookup to speed up */
    sdna->structs_map = BLI_ghash_str_new_ex("init_structDNA gh", sdna->structs_len);

    for (intptr_t nr = 0; nr < sdna->structs_len; nr++) {
      SDNA_Struct *struct_info = sdna->structs[nr];
      BLI_ghash_insert(
          sdna->structs_map, (void *)sdna->types[struct_info->type], POINTER_FROM_INT(nr));
    }
  }
#endif

  /* Calculate 'sdna->pointer_size' */
  {
    const int nr = DNA_struct_find_nr(sdna, "ListBase");

    /* should never happen, only with corrupt file for example */
    if (UNLIKELY(nr == -1)) {
      *r_error_message = "ListBase struct error! Not found.";
      return false;
    }

    /* finally pointer_size: use struct #ListBase to test it, never change the size of it! */
    SDNA_Struct *struct_info = sdna->structs[nr];
    /* Weird; I have no memory of that... I think I used `sizeof(void *)` before... (ton). */

    sdna->pointer_size = sdna->types_size[struct_info->type] / 2;

    if (struct_info->members_len != 2 || !ELEM(sdna->pointer_size, 4, 8)) {
      *r_error_message = "ListBase struct error! Needs it to calculate pointer-size.";
      /* Well, at least `sizeof(ListBase)` is error proof! (ton). */
      return false;
    }
  }

  /* Cache name size. */
  {
    short *names_array_len = MEM_mallocN(sizeof(*names_array_len) * sdna->names_len, __func__);
    for (int i = 0; i < sdna->names_len; i++) {
      names_array_len[i] = DNA_elem_array_size(sdna->names[i]);
    }
    sdna->names_array_len = names_array_len;
  }

  return true;
}

SDNA *DNA_sdna_from_data(const void *data,
                         const int data_len,
                         bool do_endian_swap,
                         bool data_alloc,
                         const char **r_error_message)
{
  SDNA *sdna = MEM_mallocN(sizeof(*sdna), "sdna");
  const char *error_message = NULL;

  sdna->data_len = data_len;
  if (data_alloc) {
    char *data_copy = MEM_mallocN(data_len, "sdna_data");
    memcpy(data_copy, data, data_len);
    sdna->data = data_copy;
  }
  else {
    sdna->data = data;
  }
  sdna->data_alloc = data_alloc;

  if (init_structDNA(sdna, do_endian_swap, &error_message)) {
    return sdna;
  }

  if (r_error_message == NULL) {
    fprintf(stderr, "Error decoding blend file SDNA: %s\n", error_message);
  }
  else {
    *r_error_message = error_message;
  }
  DNA_sdna_free(sdna);
  return NULL;
}

/**
 * Using a global is acceptable here,
 * the data is read-only and only changes between Blender versions.
 *
 * So it is safe to create once and reuse.
 */
static SDNA *g_sdna = NULL;

void DNA_sdna_current_init(void)
{
  g_sdna = DNA_sdna_from_data(DNAstr, DNAlen, false, false, NULL);
}

const struct SDNA *DNA_sdna_current_get(void)
{
  BLI_assert(g_sdna != NULL);
  return g_sdna;
}

void DNA_sdna_current_free(void)
{
  DNA_sdna_free(g_sdna);
  g_sdna = NULL;
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
  const char *struct_name = oldsdna->types[old_struct->type];

  const int new_struct_index = DNA_struct_find_nr(newsdna, struct_name);
  if (new_struct_index == -1) {
    /* Didn't find a matching new struct, so it has been removed. */
    compare_flags[old_struct_index] = SDNA_CMP_REMOVED;
    return;
  }

  SDNA_Struct *new_struct = newsdna->structs[new_struct_index];
  if (old_struct->members_len != new_struct->members_len) {
    /* Structs with a different amount of members are not equal. */
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }
  if (oldsdna->types_size[old_struct->type] != newsdna->types_size[new_struct->type]) {
    /* Structs that don't have the same size are not equal. */
    compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
    return;
  }

  /* Compare each member individually. */
  for (int member_index = 0; member_index < old_struct->members_len; member_index++) {
    SDNA_StructMember *old_member = &old_struct->members[member_index];
    SDNA_StructMember *new_member = &new_struct->members[member_index];

    const char *old_type_name = oldsdna->types[old_member->type];
    const char *new_type_name = newsdna->types[new_member->type];
    if (!STREQ(old_type_name, new_type_name)) {
      /* If two members have a different type in the same place, the structs are not equal. */
      compare_flags[old_struct_index] = SDNA_CMP_NOT_EQUAL;
      return;
    }

    const char *old_member_name = oldsdna->names[old_member->name];
    const char *new_member_name = newsdna->names[new_member->name];
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
      const int old_member_struct_index = DNA_struct_find_nr(oldsdna, old_type_name);
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
  if (oldsdna->structs_len == 0) {
    printf("error: file without SDNA\n");
    return NULL;
  }

  char *compare_flags = MEM_mallocN(oldsdna->structs_len, "compare flags");
  memset(compare_flags, SDNA_CMP_UNKNOWN, oldsdna->structs_len);

  /* Set correct flag for every struct. */
  for (int a = 0; a < oldsdna->structs_len; a++) {
    set_compare_flags_for_struct(oldsdna, newsdna, compare_flags, a);
    BLI_assert(compare_flags[a] != SDNA_CMP_UNKNOWN);
  }

  /* First struct in `util.h` is struct Link, this is skipped in compare_flags (als # 0).
   * was a bug, and this way dirty patched! Solve this later. */
  compare_flags[0] = SDNA_CMP_EQUAL;

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
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_UCHAR: {
        const uchar value = *((uchar *)old_data);
        old_value_i = value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_SHORT: {
        const short value = *((short *)old_data);
        old_value_i = value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_USHORT: {
        const ushort value = *((ushort *)old_data);
        old_value_i = value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_INT: {
        const int value = *((int *)old_data);
        old_value_i = value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_FLOAT: {
        const float value = *((float *)old_data);
        /* `int64_t` range stored in a `uint64_t`. */
        old_value_i = (uint64_t)(int64_t)value;
        old_value_f = value;
        break;
      }
      case SDNA_TYPE_DOUBLE: {
        const double value = *((double *)old_data);
        /* `int64_t` range stored in a `uint64_t`. */
        old_value_i = (uint64_t)(int64_t)value;
        old_value_f = value;
        break;
      }
      case SDNA_TYPE_INT64: {
        const int64_t value = *((int64_t *)old_data);
        old_value_i = (uint64_t)value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_UINT64: {
        const uint64_t value = *((uint64_t *)old_data);
        old_value_i = value;
        old_value_f = (double)value;
        break;
      }
      case SDNA_TYPE_INT8: {
        const int8_t value = *((int8_t *)old_data);
        old_value_i = (uint64_t)value;
        old_value_f = (double)value;
      }
    }

    switch (new_type) {
      case SDNA_TYPE_CHAR:
        *new_data = (char)old_value_i;
        break;
      case SDNA_TYPE_UCHAR:
        *((uchar *)new_data) = (uchar)old_value_i;
        break;
      case SDNA_TYPE_SHORT:
        *((short *)new_data) = (short)old_value_i;
        break;
      case SDNA_TYPE_USHORT:
        *((ushort *)new_data) = (ushort)old_value_i;
        break;
      case SDNA_TYPE_INT:
        *((int *)new_data) = (int)old_value_i;
        break;
      case SDNA_TYPE_FLOAT:
        if (old_type < 2) {
          old_value_f /= 255.0;
        }
        *((float *)new_data) = old_value_f;
        break;
      case SDNA_TYPE_DOUBLE:
        if (old_type < 2) {
          old_value_f /= 255.0;
        }
        *((double *)new_data) = old_value_f;
        break;
      case SDNA_TYPE_INT64:
        *((int64_t *)new_data) = (int64_t)old_value_i;
        break;
      case SDNA_TYPE_UINT64:
        *((uint64_t *)new_data) = old_value_i;
        break;
      case SDNA_TYPE_INT8:
        *((int8_t *)new_data) = (int8_t)old_value_i;
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

  while (1) {
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
  for (int a = 0; a < old->members_len; a++) {
    const SDNA_StructMember *member = &old->members[a];
    const char *otype = types[member->type];
    const char *oname = names[member->name];

    if (elem_streq(name, oname)) { /* name equal */
      return STREQ(type, otype);   /* type equal */
    }
  }
  return false;
}

/**
 * \param sdna: Old SDNA.
 */
static bool elem_exists(const SDNA *sdna,
                        const char *type,
                        const char *name,
                        const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->types,
      sdna->names,
      /* Regular args. */
      type,
      name,
      old);
}

static bool elem_exists_alias(const SDNA *sdna,
                              const char *type,
                              const char *name,
                              const SDNA_Struct *old)
{
  return elem_exists_impl(
      /* Expand SDNA. */
      sdna->alias.types,
      sdna->alias.names,
      /* Regular args. */
      type,
      name,
      old);
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
 * \note Use #elem_exists if additional information provided by this function is not needed.
 *
 * \note We could have a version of this function that
 * returns the #SDNA_StructMember currently it's not needed.
 */
static int elem_offset(const SDNA *sdna,
                       const char *type,
                       const char *name,
                       const SDNA_Struct *old)
{
  /* without arraypart, so names can differ: return old namenr and type */

  /* in old is the old struct */
  int offset = 0;
  for (int a = 0; a < old->members_len; a++) {
    const SDNA_StructMember *member = &old->members[a];
    const char *otype = sdna->types[member->type];
    const char *oname = sdna->names[member->name];
    if (elem_streq(name, oname)) { /* name equal */
      if (STREQ(type, otype)) {    /* type equal */
        return offset;
      }
      break; /* Fail below. */
    }
    offset += DNA_elem_size_nr(sdna, member->type, member->name);
  }
  return -1;
}

/* Each struct member belongs to one of the categories below. */
typedef enum eStructMemberCategory {
  STRUCT_MEMBER_CATEGORY_STRUCT,
  STRUCT_MEMBER_CATEGORY_PRIMITIVE,
  STRUCT_MEMBER_CATEGORY_POINTER,
} eStructMemberCategory;

static eStructMemberCategory get_struct_member_category(const SDNA *sdna,
                                                        const SDNA_StructMember *member)
{
  const char *member_name = sdna->names[member->name];
  if (ispointer(member_name)) {
    return STRUCT_MEMBER_CATEGORY_POINTER;
  }
  const char *member_type_name = sdna->types[member->type];
  if (DNA_struct_find(sdna, member_type_name)) {
    return STRUCT_MEMBER_CATEGORY_STRUCT;
  }
  return STRUCT_MEMBER_CATEGORY_PRIMITIVE;
}

static int get_member_size_in_bytes(const SDNA *sdna, const SDNA_StructMember *member)
{
  const char *name = sdna->names[member->name];
  const int array_length = sdna->names_array_len[member->name];
  if (ispointer(name)) {
    return sdna->pointer_size * array_length;
  }
  const int type_size = sdna->types_size[member->type];
  return type_size * array_length;
}

void DNA_struct_switch_endian(const SDNA *sdna, int struct_nr, char *data)
{
  if (struct_nr == -1) {
    return;
  }

  const SDNA_Struct *struct_info = sdna->structs[struct_nr];

  int offset_in_bytes = 0;
  for (int member_index = 0; member_index < struct_info->members_len; member_index++) {
    const SDNA_StructMember *member = &struct_info->members[member_index];
    const eStructMemberCategory member_category = get_struct_member_category(sdna, member);
    char *member_data = data + offset_in_bytes;
    const char *member_type_name = sdna->types[member->type];
    const int member_array_length = sdna->names_array_len[member->name];

    switch (member_category) {
      case STRUCT_MEMBER_CATEGORY_STRUCT: {
        const int substruct_size = sdna->types_size[member->type];
        const int substruct_nr = DNA_struct_find_nr(sdna, member_type_name);
        BLI_assert(substruct_nr != -1);
        for (int a = 0; a < member_array_length; a++) {
          DNA_struct_switch_endian(sdna, substruct_nr, member_data + a * substruct_size);
        }
        break;
      }
      case STRUCT_MEMBER_CATEGORY_PRIMITIVE: {
        switch (member->type) {
          case SDNA_TYPE_SHORT:
          case SDNA_TYPE_USHORT: {
            BLI_endian_switch_int16_array((int16_t *)member_data, member_array_length);
            break;
          }
          case SDNA_TYPE_INT:
          case SDNA_TYPE_FLOAT: {
            /* NOTE: intentionally ignore `long/ulong`, because these could be 4 or 8 bytes.
             * Fortunately, we only use these types for runtime variables and only once for a
             * struct type that is no longer used. */
            BLI_endian_switch_int32_array((int32_t *)member_data, member_array_length);
            break;
          }
          case SDNA_TYPE_INT64:
          case SDNA_TYPE_UINT64:
          case SDNA_TYPE_DOUBLE: {
            BLI_endian_switch_int64_array((int64_t *)member_data, member_array_length);
            break;
          }
          default: {
            break;
          }
        }
        break;
      }
      case STRUCT_MEMBER_CATEGORY_POINTER: {
        /* See `readfile.c` (#bh4_from_bh8 swap endian argument),
         * this is only done when reducing the size of a pointer from 4 to 8. */
        if (sizeof(void *) < 8) {
          if (sdna->pointer_size == 8) {
            BLI_endian_switch_uint64_array((uint64_t *)member_data, member_array_length);
          }
        }
        break;
      }
    }
    offset_in_bytes += get_member_size_in_bytes(sdna, member);
  }
}

typedef enum eReconstructStepType {
  RECONSTRUCT_STEP_MEMCPY,
  RECONSTRUCT_STEP_CAST_PRIMITIVE,
  RECONSTRUCT_STEP_CAST_POINTER_TO_32,
  RECONSTRUCT_STEP_CAST_POINTER_TO_64,
  RECONSTRUCT_STEP_SUBSTRUCT,
  RECONSTRUCT_STEP_INIT_ZERO,
} eReconstructStepType;

typedef struct ReconstructStep {
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
      short old_struct_nr;
      short new_struct_nr;
    } substruct;
  } data;
} ReconstructStep;

typedef struct DNA_ReconstructInfo {
  const SDNA *oldsdna;
  const SDNA *newsdna;
  const char *compare_flags;

  int *step_counts;
  ReconstructStep **steps;
} DNA_ReconstructInfo;

static void reconstruct_structs(const DNA_ReconstructInfo *reconstruct_info,
                                const int blocks,
                                const int old_struct_nr,
                                const int new_struct_nr,
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
                               const int new_struct_nr,
                               const char *old_block,
                               char *new_block)
{
  const ReconstructStep *steps = reconstruct_info->steps[new_struct_nr];
  const int step_count = reconstruct_info->step_counts[new_struct_nr];

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
                            step->data.substruct.old_struct_nr,
                            step->data.substruct.new_struct_nr,
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
                                const int old_struct_nr,
                                const int new_struct_nr,
                                const char *old_blocks,
                                char *new_blocks)
{
  const SDNA_Struct *old_struct = reconstruct_info->oldsdna->structs[old_struct_nr];
  const SDNA_Struct *new_struct = reconstruct_info->newsdna->structs[new_struct_nr];

  const int old_block_size = reconstruct_info->oldsdna->types_size[old_struct->type];
  const int new_block_size = reconstruct_info->newsdna->types_size[new_struct->type];

  for (int a = 0; a < blocks; a++) {
    const char *old_block = old_blocks + a * old_block_size;
    char *new_block = new_blocks + a * new_block_size;
    reconstruct_struct(reconstruct_info, new_struct_nr, old_block, new_block);
  }
}

void *DNA_struct_reconstruct(const DNA_ReconstructInfo *reconstruct_info,
                             int old_struct_nr,
                             int blocks,
                             const void *old_blocks)
{
  const SDNA *oldsdna = reconstruct_info->oldsdna;
  const SDNA *newsdna = reconstruct_info->newsdna;

  const SDNA_Struct *old_struct = oldsdna->structs[old_struct_nr];
  const char *type_name = oldsdna->types[old_struct->type];
  const int new_struct_nr = DNA_struct_find_nr(newsdna, type_name);

  if (new_struct_nr == -1) {
    return NULL;
  }

  const SDNA_Struct *new_struct = newsdna->structs[new_struct_nr];
  const int new_block_size = newsdna->types_size[new_struct->type];

  char *new_blocks = MEM_callocN(blocks * new_block_size, "reconstruct");
  reconstruct_structs(
      reconstruct_info, blocks, old_struct_nr, new_struct_nr, old_blocks, new_blocks);
  return new_blocks;
}

/** Finds a member in the given struct with the given name. */
static const SDNA_StructMember *find_member_with_matching_name(const SDNA *sdna,
                                                               const SDNA_Struct *struct_info,
                                                               const char *name,
                                                               int *r_offset)
{
  int offset = 0;
  for (int a = 0; a < struct_info->members_len; a++) {
    const SDNA_StructMember *member = &struct_info->members[a];
    const char *member_name = sdna->names[member->name];
    if (elem_streq(name, member_name)) {
      *r_offset = offset;
      return member;
    }
    offset += get_member_size_in_bytes(sdna, member);
  }
  return NULL;
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
  const char *new_name = newsdna->names[new_member->name];
  const SDNA_StructMember *old_member = find_member_with_matching_name(
      oldsdna, old_struct, new_name, &old_member_offset);

  if (old_member == NULL) {
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

  const int new_array_length = newsdna->names_array_len[new_member->name];
  const int old_array_length = oldsdna->names_array_len[old_member->name];
  const int shared_array_length = MIN2(new_array_length, old_array_length);

  const char *new_type_name = newsdna->types[new_member->type];
  const char *old_type_name = oldsdna->types[old_member->type];

  switch (new_category) {
    case STRUCT_MEMBER_CATEGORY_STRUCT: {
      if (STREQ(new_type_name, old_type_name)) {
        const int old_struct_nr = DNA_struct_find_nr(oldsdna, old_type_name);
        BLI_assert(old_struct_nr != -1);
        enum eSDNA_StructCompare compare_flag = compare_flags[old_struct_nr];
        BLI_assert(compare_flag != SDNA_CMP_REMOVED);
        if (compare_flag == SDNA_CMP_EQUAL) {
          /* The old and new members are identical, just do a #memcpy. */
          r_step->type = RECONSTRUCT_STEP_MEMCPY;
          r_step->data.memcpy.new_offset = new_member_offset;
          r_step->data.memcpy.old_offset = old_member_offset;
          r_step->data.memcpy.size = newsdna->types_size[new_member->type] * shared_array_length;
        }
        else {
          const int new_struct_nr = DNA_struct_find_nr(newsdna, new_type_name);
          BLI_assert(new_struct_nr != -1);

          /* The old and new members are different, use recursion to reconstruct the
           * nested struct. */
          BLI_assert(compare_flag == SDNA_CMP_NOT_EQUAL);
          r_step->type = RECONSTRUCT_STEP_SUBSTRUCT;
          r_step->data.substruct.new_offset = new_member_offset;
          r_step->data.substruct.old_offset = old_member_offset;
          r_step->data.substruct.array_len = shared_array_length;
          r_step->data.substruct.new_struct_nr = new_struct_nr;
          r_step->data.substruct.old_struct_nr = old_struct_nr;
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
        r_step->data.memcpy.size = newsdna->types_size[new_member->type] * shared_array_length;
      }
      else {
        /* The old and new primitive types are different, cast from the old to new type. */
        r_step->type = RECONSTRUCT_STEP_CAST_PRIMITIVE;
        r_step->data.cast_primitive.array_len = shared_array_length;
        r_step->data.cast_primitive.new_offset = new_member_offset;
        r_step->data.cast_primitive.old_offset = old_member_offset;
        r_step->data.cast_primitive.new_type = new_member->type;
        r_step->data.cast_primitive.old_type = old_member->type;
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
static void print_reconstruct_step(ReconstructStep *step, const SDNA *oldsdna, const SDNA *newsdna)
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
          (int)step->data.cast_primitive.old_type,
          oldsdna->types[step->data.cast_primitive.old_type],
          (int)step->data.cast_primitive.new_type,
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
          step->data.substruct.new_struct_nr,
          newsdna->types[newsdna->structs[step->data.substruct.new_struct_nr]->type],
          newsdna->types_size[newsdna->structs[step->data.substruct.new_struct_nr]->type],
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
  ReconstructStep *steps = MEM_calloc_arrayN(
      new_struct->members_len, sizeof(ReconstructStep), __func__);

  int new_member_offset = 0;
  for (int new_member_index = 0; new_member_index < new_struct->members_len; new_member_index++) {
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
  DNA_ReconstructInfo *reconstruct_info = MEM_callocN(sizeof(DNA_ReconstructInfo), __func__);
  reconstruct_info->oldsdna = oldsdna;
  reconstruct_info->newsdna = newsdna;
  reconstruct_info->compare_flags = compare_flags;
  reconstruct_info->step_counts = MEM_malloc_arrayN(newsdna->structs_len, sizeof(int), __func__);
  reconstruct_info->steps = MEM_malloc_arrayN(
      newsdna->structs_len, sizeof(ReconstructStep *), __func__);

  /* Generate reconstruct steps for all structs. */
  for (int new_struct_nr = 0; new_struct_nr < newsdna->structs_len; new_struct_nr++) {
    const SDNA_Struct *new_struct = newsdna->structs[new_struct_nr];
    const char *new_struct_name = newsdna->types[new_struct->type];
    const int old_struct_nr = DNA_struct_find_nr(oldsdna, new_struct_name);
    if (old_struct_nr < 0) {
      reconstruct_info->steps[new_struct_nr] = NULL;
      reconstruct_info->step_counts[new_struct_nr] = 0;
      continue;
    }
    const SDNA_Struct *old_struct = oldsdna->structs[old_struct_nr];
    ReconstructStep *steps = create_reconstruct_steps_for_struct(
        oldsdna, newsdna, compare_flags, old_struct, new_struct);

    /* Comment the line below to skip the compression for debugging purposes. */
    const int steps_len = compress_reconstruct_steps(steps, new_struct->members_len);

    reconstruct_info->steps[new_struct_nr] = steps;
    reconstruct_info->step_counts[new_struct_nr] = steps_len;

/* This is useful when debugging the reconstruct steps. */
#if 0
    printf("%s: \n", new_struct_name);
    for (int a = 0; a < steps_len; a++) {
      printf("  ");
      print_reconstruct_step(&steps[a], oldsdna, newsdna);
      printf("\n");
    }
#endif
    UNUSED_VARS(print_reconstruct_step);
  }

  return reconstruct_info;
}

void DNA_reconstruct_info_free(DNA_ReconstructInfo *reconstruct_info)
{
  for (int a = 0; a < reconstruct_info->newsdna->structs_len; a++) {
    if (reconstruct_info->steps[a] != NULL) {
      MEM_freeN(reconstruct_info->steps[a]);
    }
  }
  MEM_freeN(reconstruct_info->steps);
  MEM_freeN(reconstruct_info->step_counts);
  MEM_freeN(reconstruct_info);
}

int DNA_elem_offset(SDNA *sdna, const char *stype, const char *vartype, const char *name)
{
  const int SDNAnr = DNA_struct_find_nr(sdna, stype);
  BLI_assert(SDNAnr != -1);
  const SDNA_Struct *const spo = sdna->structs[SDNAnr];
  return elem_offset(sdna, vartype, name, spo);
}

bool DNA_struct_find(const SDNA *sdna, const char *stype)
{
  return DNA_struct_find_nr(sdna, stype) != -1;
}

bool DNA_struct_elem_find(const SDNA *sdna,
                          const char *stype,
                          const char *vartype,
                          const char *name)
{
  const int SDNAnr = DNA_struct_find_nr(sdna, stype);

  if (SDNAnr != -1) {
    const SDNA_Struct *const spo = sdna->structs[SDNAnr];
    const bool found = elem_exists(sdna, vartype, name, spo);

    if (found) {
      return true;
    }
  }
  return false;
}

bool DNA_struct_alias_elem_find(const SDNA *sdna,
                                const char *stype,
                                const char *vartype,
                                const char *name)
{
  const int SDNAnr = DNA_struct_alias_find_nr(sdna, stype);

  if (SDNAnr != -1) {
    const SDNA_Struct *const spo = sdna->structs[SDNAnr];
    const bool found = elem_exists_alias(sdna, vartype, name, spo);

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
  }

  /* weak */
  return 8;
}

/* -------------------------------------------------------------------- */
/** \name Version Patch DNA
 * \{ */

static bool DNA_sdna_patch_struct_nr(SDNA *sdna,
                                     const int struct_name_old_nr,
                                     const char *struct_name_new)
{
  BLI_assert(DNA_struct_find_nr(DNA_sdna_current_get(), struct_name_new) != -1);
  const SDNA_Struct *struct_info = sdna->structs[struct_name_old_nr];
#ifdef WITH_DNA_GHASH
  BLI_ghash_remove(sdna->structs_map, (void *)sdna->types[struct_info->type], NULL, NULL);
  BLI_ghash_insert(
      sdna->structs_map, (void *)struct_name_new, POINTER_FROM_INT(struct_name_old_nr));
#endif
  sdna->types[struct_info->type] = struct_name_new;
  return true;
}
bool DNA_sdna_patch_struct(SDNA *sdna, const char *struct_name_old, const char *struct_name_new)
{
  const int struct_name_old_nr = DNA_struct_find_nr(sdna, struct_name_old);
  if (struct_name_old_nr != -1) {
    return DNA_sdna_patch_struct_nr(sdna, struct_name_old_nr, struct_name_new);
  }
  return false;
}

/* Make public if called often with same struct (avoid duplicate lookups). */
static bool DNA_sdna_patch_struct_member_nr(SDNA *sdna,
                                            const int struct_name_nr,
                                            const char *elem_old,
                                            const char *elem_new)
{
  /* These names aren't handled here (it's not used).
   * Ensure they are never used or we get out of sync arrays. */
  BLI_assert(sdna->alias.names == NULL);
  const int elem_old_len = strlen(elem_old);
  const int elem_new_len = strlen(elem_new);
  BLI_assert(elem_new != NULL);
  SDNA_Struct *sp = sdna->structs[struct_name_nr];
  for (int elem_index = sp->members_len; elem_index > 0; elem_index--) {
    SDNA_StructMember *member = &sp->members[elem_index];
    const char *elem_old_full = sdna->names[member->name];
    /* Start & end offsets in 'elem_old_full'. */
    uint elem_old_full_offset_start;
    if (DNA_elem_id_match(elem_old, elem_old_len, elem_old_full, &elem_old_full_offset_start)) {
      if (sdna->mem_arena == NULL) {
        sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }
      const char *elem_new_full = DNA_elem_id_rename(sdna->mem_arena,
                                                     elem_old,
                                                     elem_old_len,
                                                     elem_new,
                                                     elem_new_len,
                                                     elem_old_full,
                                                     strlen(elem_old_full),
                                                     elem_old_full_offset_start);

      if (sdna->names_len == sdna->names_len_alloc) {
        sdna->names_len_alloc += 64;
        sdna->names = MEM_recallocN((void *)sdna->names,
                                    sizeof(*sdna->names) * sdna->names_len_alloc);
        sdna->names_array_len = MEM_recallocN(
            (void *)sdna->names_array_len, sizeof(*sdna->names_array_len) * sdna->names_len_alloc);
      }
      const short name_nr_prev = member->name;
      member->name = sdna->names_len++;
      sdna->names[member->name] = elem_new_full;
      sdna->names_array_len[member->name] = sdna->names_array_len[name_nr_prev];

      return true;
    }
  }
  return false;
}
bool DNA_sdna_patch_struct_member(SDNA *sdna,
                                  const char *struct_name,
                                  const char *elem_old,
                                  const char *elem_new)
{
  const int struct_name_nr = DNA_struct_find_nr(sdna, struct_name);
  if (struct_name_nr != -1) {
    return DNA_sdna_patch_struct_member_nr(sdna, struct_name_nr, elem_old, elem_new);
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
  for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
    const SDNA_Struct *struct_old = sdna->structs[struct_nr];
    names_expand_len += struct_old->members_len;
  }
  const char **names_expand = MEM_mallocN(sizeof(*names_expand) * names_expand_len, __func__);
  short *names_array_len_expand = MEM_mallocN(sizeof(*names_array_len_expand) * names_expand_len,
                                              __func__);

  int names_expand_index = 0;
  for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
    /* We can't edit this memory 'sdna->structs' points to (read-only `datatoc` file). */
    const SDNA_Struct *struct_old = sdna->structs[struct_nr];

    const int array_size = sizeof(short) * 2 + sizeof(SDNA_StructMember) * struct_old->members_len;
    SDNA_Struct *struct_new = BLI_memarena_alloc(sdna->mem_arena, array_size);
    memcpy(struct_new, struct_old, array_size);
    sdna->structs[struct_nr] = struct_new;

    for (int i = 0; i < struct_old->members_len; i++) {
      const SDNA_StructMember *member_old = &struct_old->members[i];
      SDNA_StructMember *member_new = &struct_new->members[i];

      names_expand[names_expand_index] = sdna->names[member_old->name];
      names_array_len_expand[names_expand_index] = sdna->names_array_len[member_old->name];

      BLI_assert(names_expand_index < SHRT_MAX);
      member_new->name = names_expand_index;
      names_expand_index++;
    }
  }
  MEM_freeN((void *)sdna->names);
  sdna->names = names_expand;

  MEM_freeN((void *)sdna->names_array_len);
  sdna->names_array_len = names_array_len_expand;

  sdna->names_len = names_expand_len;
}

static const char *dna_sdna_alias_from_static_elem_full(SDNA *sdna,
                                                        GHash *elem_map_alias_from_static,
                                                        const char *struct_name_static,
                                                        const char *elem_static_full)
{
  const int elem_static_full_len = strlen(elem_static_full);
  char *elem_static = alloca(elem_static_full_len + 1);
  const int elem_static_len = DNA_elem_id_strip_copy(elem_static, elem_static_full);
  const char *str_pair[2] = {struct_name_static, elem_static};
  const char *elem_alias = BLI_ghash_lookup(elem_map_alias_from_static, str_pair);
  if (elem_alias) {
    return DNA_elem_id_rename(sdna->mem_arena,
                              elem_static,
                              elem_static_len,
                              elem_alias,
                              strlen(elem_alias),
                              elem_static_full,
                              elem_static_full_len,
                              DNA_elem_id_offset_start(elem_static_full));
  }
  return NULL;
}

void DNA_sdna_alias_data_ensure(SDNA *sdna)
{
  /* We may want this to be optional later. */
  const bool use_legacy_hack = true;

  if (sdna->mem_arena == NULL) {
    sdna->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  }

  GHash *struct_map_alias_from_static;
  GHash *elem_map_alias_from_static;

  DNA_alias_maps(
      DNA_RENAME_ALIAS_FROM_STATIC, &struct_map_alias_from_static, &elem_map_alias_from_static);

  if (sdna->alias.types == NULL) {
    sdna->alias.types = MEM_mallocN(sizeof(*sdna->alias.types) * sdna->types_len, __func__);
    for (int type_nr = 0; type_nr < sdna->types_len; type_nr++) {
      const char *struct_name_static = sdna->types[type_nr];

      if (use_legacy_hack) {
        struct_name_static = DNA_struct_rename_legacy_hack_alias_from_static(struct_name_static);
      }

      sdna->alias.types[type_nr] = BLI_ghash_lookup_default(
          struct_map_alias_from_static, struct_name_static, (void *)struct_name_static);
    }
  }

  if (sdna->alias.names == NULL) {
    sdna_expand_names(sdna);
    sdna->alias.names = MEM_mallocN(sizeof(*sdna->alias.names) * sdna->names_len, __func__);
    for (int struct_nr = 0; struct_nr < sdna->structs_len; struct_nr++) {
      const SDNA_Struct *struct_info = sdna->structs[struct_nr];
      const char *struct_name_static = sdna->types[struct_info->type];

      if (use_legacy_hack) {
        struct_name_static = DNA_struct_rename_legacy_hack_alias_from_static(struct_name_static);
      }

      for (int a = 0; a < struct_info->members_len; a++) {
        const SDNA_StructMember *member = &struct_info->members[a];
        const char *elem_alias_full = dna_sdna_alias_from_static_elem_full(
            sdna, elem_map_alias_from_static, struct_name_static, sdna->names[member->name]);
        if (elem_alias_full != NULL) {
          sdna->alias.names[member->name] = elem_alias_full;
        }
        else {
          sdna->alias.names[member->name] = sdna->names[member->name];
        }
      }
    }
  }
  BLI_ghash_free(struct_map_alias_from_static, NULL, NULL);
  BLI_ghash_free(elem_map_alias_from_static, MEM_freeN, NULL);
}

void DNA_sdna_alias_data_ensure_structs_map(SDNA *sdna)
{
  DNA_sdna_alias_data_ensure(sdna);
#ifdef WITH_DNA_GHASH
  /* create a ghash lookup to speed up */
  struct GHash *structs_map = BLI_ghash_str_new_ex(__func__, sdna->structs_len);
  for (intptr_t nr = 0; nr < sdna->structs_len; nr++) {
    const SDNA_Struct *struct_info = sdna->structs[nr];
    BLI_ghash_insert(
        structs_map, (void *)sdna->alias.types[struct_info->type], POINTER_FROM_INT(nr));
  }
  sdna->alias.structs_map = structs_map;
#else
  UNUSED_VARS(sdna);
#endif
}

/** \} */
