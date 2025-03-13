/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * \brief Struct parser for generating SDNA.
 *
 * \section aboutmakesdnac About makesdna tool
 *
 * `makesdna` creates a `.c` file with a long string of numbers that
 * encode the Blender file format. It is fast, because it is basically
 * a binary dump. There are some details to mind when reconstructing
 * the file (endianness and byte-alignment).
 *
 * This little program scans all structs that need to be serialized,
 * and determined the names and types of all members. It calculates
 * how much memory (on disk or in ram) is needed to store that struct,
 * and the offsets for reaching a particular one.
 *
 * There is a facility to get verbose output from `sdna`. Search for
 * \ref debugSDNA. This int can be set to 0 (no output) to some int.
 * Higher numbers give more output.
 */

#define DNA_DEPRECATED_ALLOW

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_sys_types.h" /* for intptr_t support */
#include "BLI_system.h"    /* for 'BLI_system_backtrace' stub. */
#include "BLI_utildefines.h"

#include "DNA_sdna_types.h"
#include "dna_utils.h"

#define SDNA_MAX_FILENAME_LENGTH 255

/* The include file below is automatically generated from the `SRC_DNA_INC`
 * variable in `source/blender/CMakeLists.txt`. */
static const char *includefiles[] = {
#include "dna_includes_as_strings.h"
    /* Empty string to indicate end of include files. */
    "",
};

/* -------------------------------------------------------------------- */
/** \name Variables
 * \{ */

static MemArena *mem_arena = nullptr;

static int max_data_size = 500000, max_array_len = 50000;
static int members_num = 0;
static int types_num = 0;
static int structs_num = 0;
/** At address `members[a]` is string `a`. */
static char **members;
/** At address `types[a]` is string `a`. */
static char **types;
/** At `types_size[a]` is the size of type `a` on this systems bitness (32 or 64). */
static short *types_size_native;
/** Contains align requirements for a struct on 32 bit systems. */
static short *types_align_32;
/** Contains align requirements for a struct on 64 bit systems. */
static short *types_align_64;
/** Contains sizes as they are calculated on 32 bit systems. */
static short *types_size_32;
/** Contains sizes as they are calculated on 64 bit systems. */
static short *types_size_64;
/**
 * At `sp = structs[a]` is the first address of a struct definition:
 * - `sp[0]` is type index.
 * - `sp[1]` is the length of the member array (next).
 * - `sp[2]` sp[3] is [(type_index, member_index), ..] (number of pairs is defined by `sp[1]`),
 */
static short **structs, *structdata;

/** Versioning data */
static struct {
  GHash *type_map_alias_from_static;
  GHash *type_map_static_from_alias;
  GHash *member_map_alias_from_static;
  GHash *member_map_static_from_alias;
} g_version_data = {nullptr};

/**
 * Variable to control debug output of makesdna.
 * debugSDNA:
 * - 0 = no output, except errors
 * - 1 = detail actions
 * - 2 = full trace, tell which names and types were found
 * - 4 = full trace, plus all gritty details
 */
static int debugSDNA = 0;
static int additional_slen_offset;

#define DEBUG_PRINTF(debug_level, ...) \
  { \
    if (debugSDNA > debug_level) { \
      printf(__VA_ARGS__); \
    } \
  } \
  ((void)0)

/* stub for BLI_abort() */
#ifndef NDEBUG
void BLI_system_backtrace(FILE *fp)
{
  (void)fp;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Function Declarations
 * \{ */

/**
 * Ensure that type \a type_name is in the #types array.
 * \param type_name: Struct name without any qualifiers.
 * \param size: The struct size in bytes.
 * \return Index in the #types array.
 */
static int add_type(const char *type_name, int size);

/**
 * Ensure that \a member_name is in the #members array.
 * \param member_name: Full struct member name (may include pointer prefix & array size).
 * \return Index in the #members array.
 */
static int add_member(const char *member_name);

/**
 * Add a new structure definition, of type matching the given \a type_index.
 *
 * NOTE: there is no lookup performed here, a new struct definition is always added.
 */
static short *add_struct(int type_index);

/**
 * Remove comments from this buffer. Assumes that the buffer refers to
 * ascii-code text.
 */
static int preprocess_include(char *maindata, const int maindata_len);

/**
 * Scan this file for serializable types.
 */
static int convert_include(const char *filepath);

/**
 * Determine how many bytes are needed for each struct.
 */
static int calculate_struct_sizes(int firststruct, FILE *file_verify, const char *base_directory);

/**
 * Construct the DNA.c file
 */
static void dna_write(FILE *file, const void *pntr, const int size);

/**
 * Report all structures found so far, and print their lengths.
 */
void print_struct_sizes();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementation
 *
 * Make DNA string (write to file).
 * \{ */

static bool match_identifier_with_len(const char *str,
                                      const char *identifier,
                                      const size_t identifier_len)
{
  if (strncmp(str, identifier, identifier_len) == 0) {
    /* Check `str` isn't a prefix to a longer identifier. */
    if (isdigit(str[identifier_len]) || isalpha(str[identifier_len]) ||
        (str[identifier_len] == '_'))
    {
      return false;
    }
    return true;
  }
  return false;
}

static bool match_identifier(const char *str, const char *identifier)
{
  const size_t identifier_len = strlen(identifier);
  return match_identifier_with_len(str, identifier, identifier_len);
}

static bool match_identifier_and_advance(char **str_ptr, const char *identifier)
{
  const size_t identifier_len = strlen(identifier);
  if (match_identifier_with_len(*str_ptr, identifier, identifier_len)) {
    (*str_ptr) += identifier_len;
    return true;
  }
  return false;
}

static const char *version_struct_static_from_alias(const char *type_alias)
{
  const char *type_static = static_cast<const char *>(
      BLI_ghash_lookup(g_version_data.type_map_static_from_alias, type_alias));
  if (type_static != nullptr) {
    return type_static;
  }
  return type_alias;
}

static const char *version_struct_alias_from_static(const char *type_static)
{
  const char *type_alias = static_cast<const char *>(
      BLI_ghash_lookup(g_version_data.type_map_alias_from_static, type_static));
  if (type_alias != nullptr) {
    return type_alias;
  }
  return type_static;
}

static const char *version_member_static_from_alias(const int type_index,
                                                    const char *member_alias_full)
{
  const uint member_alias_full_len = strlen(member_alias_full);
  char *member_alias = static_cast<char *>(alloca(member_alias_full_len + 1));
  const int member_alias_len = DNA_member_id_strip_copy(member_alias, member_alias_full);
  const char *str_pair[2] = {types[type_index], member_alias};
  const char *member_static = static_cast<const char *>(
      BLI_ghash_lookup(g_version_data.member_map_static_from_alias, str_pair));
  if (member_static != nullptr) {
    return DNA_member_id_rename(mem_arena,
                                member_alias,
                                member_alias_len,
                                member_static,
                                strlen(member_static),
                                member_alias_full,
                                member_alias_full_len,
                                DNA_member_id_offset_start(member_alias_full));
  }
  return member_alias_full;
}

/**
 * Enforce '_pad123' naming convention, disallow 'pad123' or 'pad_123',
 * special exception for [a-z] after since there is a 'pad_rot_angle' preference.
 */
static bool is_name_legal(const char *name)
{
  const int name_size = strlen(name) + 1;
  char *name_strip = static_cast<char *>(alloca(name_size));
  DNA_member_id_strip_copy(name_strip, name);

  const char prefix[] = {'p', 'a', 'd'};

  if (name[0] == '_') {
    if (strncmp(&name_strip[1], prefix, sizeof(prefix)) != 0) {
      fprintf(
          stderr, "Error: only '_pad' variables can start with an underscore, found '%s'\n", name);
      return false;
    }
  }
  else if (strncmp(name_strip, prefix, sizeof(prefix)) == 0) {
    int i = sizeof(prefix);
    if (name_strip[i] >= 'a' && name_strip[i] <= 'z') {
      /* may be part of a word, allow that. */
      return true;
    }
    bool has_only_digit_or_none = true;
    for (; name_strip[i]; i++) {
      const char c = name_strip[i];
      if (!((c >= '0' && c <= '9') || c == '_')) {
        has_only_digit_or_none = false;
        break;
      }
    }
    if (has_only_digit_or_none) {
      /* found 'pad' or 'pad123'. */
      fprintf(
          stderr, "Error: padding variables must be formatted '_pad[number]', found '%s'\n", name);
      return false;
    }
  }
  return true;
}

static int add_type(const char *type_name, int size)
{
  /* first do validity check */
  if (type_name[0] == 0) {
    return -1;
  }
  if (strchr(type_name, '*')) {
    /* NOTE: this is valid C syntax but we can't parse, complain!
     * `struct SomeStruct* some_var;` <-- correct but we can't handle right now. */
    return -1;
  }

  type_name = version_struct_static_from_alias(type_name);

  /* search through type array */
  for (int type_index = 0; type_index < types_num; type_index++) {
    if (STREQ(type_name, types[type_index])) {
      if (size) {
        types_size_native[type_index] = size;
        types_size_32[type_index] = size;
        types_size_64[type_index] = size;
        types_align_32[type_index] = size;
        types_align_64[type_index] = size;
      }
      return type_index;
    }
  }

  /* append new type */
  const int type_name_len = strlen(type_name) + 1;
  char *cp = static_cast<char *>(BLI_memarena_alloc(mem_arena, type_name_len));
  memcpy(cp, type_name, type_name_len);
  types[types_num] = cp;
  types_size_native[types_num] = size;
  types_size_32[types_num] = size;
  types_size_64[types_num] = size;
  types_align_32[types_num] = size;
  types_align_64[types_num] = size;
  if (types_num >= max_array_len) {
    printf("too many types\n");
    return types_num - 1;
  }
  types_num++;

  return types_num - 1;
}

/**
 * Because of the weird way of tokenizing, we have to 'cast' function
 * pointers to ... (*f)(), whatever the original signature. In fact,
 * we add name and type at the same time... There are two special
 * cases, unfortunately. These are explicitly checked.
 */
static int add_member(const char *member_name)
{
  char buf[255]; /* stupid limit, change it :) */
  const char *name;

  additional_slen_offset = 0;

  if (member_name[0] == 0 /*  || (member_name[1] == 0) */) {
    return -1;
  }

  if (member_name[0] == '(' && member_name[1] == '*') {
    /* We handle function pointer and special array cases here, e.g.
     * `void (*function)(...)` and `float (*array)[..]`. the array case
     * name is still converted to (array *)() though because it is that
     * way in old DNA too, and works correct with #DNA_struct_member_size. */
    int isfuncptr = (strchr(member_name + 1, '(')) != nullptr;

    DEBUG_PRINTF(3, "\t\t\t\t*** Function pointer or multidim array pointer found\n");
    /* function-pointer: transform the type (sometimes). */
    int i = 0;

    while (member_name[i] != ')') {
      buf[i] = member_name[i];
      i++;
    }

    /* Another number we need is the extra slen offset. This extra
     * offset is the overshoot after a space. If there is no
     * space, no overshoot should be calculated. */
    int j = i; /* j at first closing brace */

    DEBUG_PRINTF(3, "first brace after offset %d\n", i);

    j++; /* j beyond closing brace ? */
    while ((member_name[j] != 0) && (member_name[j] != ')')) {
      DEBUG_PRINTF(3, "seen %c (%d)\n", member_name[j], member_name[j]);
      j++;
    }
    DEBUG_PRINTF(3,
                 "seen %c (%d)\n"
                 "special after offset%d\n",
                 member_name[j],
                 member_name[j],
                 j);

    if (!isfuncptr) {
      /* multidimensional array pointer case */
      if (member_name[j] == 0) {
        DEBUG_PRINTF(3, "offsetting for multi-dimensional array pointer\n");
      }
      else {
        printf("Error during tokenizing multi-dimensional array pointer\n");
      }
    }
    else if (member_name[j] == 0) {
      DEBUG_PRINTF(3, "offsetting for space\n");
      /* get additional offset */
      int k = 0;
      while (member_name[j] != ')') {
        j++;
        k++;
      }
      DEBUG_PRINTF(3, "extra offset %d\n", k);
      additional_slen_offset = k;
    }
    else if (member_name[j] == ')') {
      DEBUG_PRINTF(3, "offsetting for brace\n");
      /* don't get extra offset */
    }
    else {
      printf("Error during tokening function pointer argument list\n");
    }

    /*
     * Put `)(void)` at the end? Maybe `)()`. Should check this with
     * old `sdna`. Actually, sometimes `)()`, sometimes `)(void...)`
     * Alas.. such is the nature of brain-damage :(
     *
     * Sorted it out: always do )(), except for `headdraw` and
     * `windraw`, part of #ScrArea. This is important, because some
     * linkers will treat different fp's differently when called
     * !!! This has to do with interference in byte-alignment and
     * the way arguments are pushed on the stack.
     */
    buf[i] = 0;
    DEBUG_PRINTF(3, "Name before chomping: %s\n", buf);
    if ((strncmp(buf, "(*headdraw", 10) == 0) || strncmp(buf, "(*windraw", 9) == 0) {
      buf[i] = ')';
      buf[i + 1] = '(';
      buf[i + 2] = 'v';
      buf[i + 3] = 'o';
      buf[i + 4] = 'i';
      buf[i + 5] = 'd';
      buf[i + 6] = ')';
      buf[i + 7] = 0;
    }
    else {
      buf[i] = ')';
      buf[i + 1] = '(';
      buf[i + 2] = ')';
      buf[i + 3] = 0;
    }
    /* Now proceed with buf. */
    DEBUG_PRINTF(3, "\t\t\t\t\tProposing fp name %s\n", buf);
    name = buf;
  }
  else {
    /* normal field: old code */
    name = member_name;
  }

  /* search name array */
  for (int member_index = 0; member_index < members_num; member_index++) {
    if (STREQ(name, members[member_index])) {
      return member_index;
    }
  }

  /* Sanity check the name. */
  if (!is_name_legal(name)) {
    return -1;
  }

  /* Append new name. */
  const int name_len = strlen(name) + 1;
  char *cp = static_cast<char *>(BLI_memarena_alloc(mem_arena, name_len));
  memcpy(cp, name, name_len);
  members[members_num] = cp;

  if (members_num >= max_array_len) {
    printf("too many names\n");
    return members_num - 1;
  }
  members_num++;

  return members_num - 1;
}

static short *add_struct(int type_index)
{
  if (structs_num == 0) {
    structs[0] = structdata;
  }
  else {
    short *sp = structs[structs_num - 1];
    const int len = sp[1];
    structs[structs_num] = sp + 2 * len + 2;
  }

  short *sp = structs[structs_num];
  sp[0] = type_index;

  if (structs_num >= max_array_len) {
    printf("too many structs\n");
    return sp;
  }
  structs_num++;

  return sp;
}

/* Copied from `BLI_str_startswith` string.c
 * to avoid complicating the compilation process of makesdna. */
static bool str_startswith(const char *__restrict str, const char *__restrict start)
{
  for (; *str && *start; str++, start++) {
    if (*str != *start) {
      return false;
    }
  }

  return (*start == '\0');
}

/**
 * Check if `str` is a preprocessor string that starts with `start`.
 * The `start` doesn't need the `#` prefix.
 * `ifdef VALUE` will match `#ifdef VALUE` as well as `#  ifdef VALUE`.
 */
static bool match_preproc_prefix(const char *__restrict str, const char *__restrict start)
{
  if (*str != '#') {
    return false;
  }
  str++;
  while (*str == ' ') {
    str++;
  }
  return str_startswith(str, start);
}

/**
 * \return The point in `str` that starts with `start` or nullptr when not found.
 */
static char *match_preproc_strstr(char *__restrict str, const char *__restrict start)
{
  while ((str = strchr(str, '#'))) {
    str++;
    while (*str == ' ') {
      str++;
    }
    if (str_startswith(str, start)) {
      return str;
    }
  }
  return nullptr;
}

static int preprocess_include(char *maindata, const int maindata_len)
{
  /* NOTE: len + 1, last character is a dummy to prevent
   * comparisons using uninitialized memory */
  char *temp = static_cast<char *>(MEM_mallocN(maindata_len + 1, "preprocess_include"));
  temp[maindata_len] = ' ';

  memcpy(temp, maindata, maindata_len);

  /* remove all c++ comments */
  /* replace all enters/tabs/etc with spaces */
  char *cp = temp;
  int a = maindata_len;
  int comment = 0;
  while (a--) {
    if (cp[0] == '/' && cp[1] == '/') {
      comment = 1;
    }
    else if (*cp == '\n') {
      comment = 0;
    }
    if (comment || *cp < 32 || *cp > 128) {
      *cp = 32;
    }
    cp++;
  }

  /* No need for leading '#' character. */
  const char *cpp_block_start = "ifdef __cplusplus";
  const char *cpp_block_end = "endif";

  /* data from temp copy to maindata, remove comments and double spaces */
  cp = temp;
  char *md = maindata;
  int newlen = 0;
  comment = 0;
  a = maindata_len;
  bool skip_until_closing_brace = false;
  while (a--) {

    if (cp[0] == '/' && cp[1] == '*') {
      comment = 1;
      cp[0] = cp[1] = 32;
    }
    if (cp[0] == '*' && cp[1] == '/') {
      comment = 0;
      cp[0] = cp[1] = 32;
    }

    /* do not copy when: */
    if (comment) {
      /* pass */
    }
    else if (cp[0] == ' ' && cp[1] == ' ') {
      /* pass */
    }
    else if (cp[-1] == '*' && cp[0] == ' ') {
      /* pointers with a space */
    } /* skip special keywords */
    else if (match_identifier(cp, "DNA_DEPRECATED")) {
      /* single values are skipped already, so decrement 1 less */
      a -= 13;
      cp += 13;
    }
    else if (match_identifier(cp, "DNA_DEFINE_CXX_METHODS")) {
      /* single values are skipped already, so decrement 1 less */
      a -= 21;
      cp += 21;
      skip_until_closing_brace = true;
    }
    else if (skip_until_closing_brace) {
      if (cp[0] == ')') {
        skip_until_closing_brace = false;
      }
    }
    else if (match_preproc_prefix(cp, cpp_block_start)) {
      char *end_ptr = match_preproc_strstr(cp, cpp_block_end);

      if (end_ptr == nullptr) {
        fprintf(stderr, "Error: '%s' block must end with '%s'\n", cpp_block_start, cpp_block_end);
      }
      else {
        const int skip_offset = end_ptr - cp + strlen(cpp_block_end);
        a -= skip_offset;
        cp += skip_offset;
      }
    }
    else {
      md[0] = cp[0];
      md++;
      newlen++;
    }
    cp++;
  }

  MEM_freeN(temp);
  return newlen;
}

static void *read_file_data(const char *filepath, int *r_len)
{
#ifdef WIN32
  FILE *fp = fopen(filepath, "rb");
#else
  FILE *fp = fopen(filepath, "r");
#endif
  void *data;

  if (!fp) {
    *r_len = -1;
    return nullptr;
  }

  fseek(fp, 0L, SEEK_END);
  *r_len = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  if (*r_len == -1) {
    fclose(fp);
    return nullptr;
  }

  data = MEM_mallocN(*r_len, "read_file_data");
  if (!data) {
    *r_len = -1;
    fclose(fp);
    return nullptr;
  }

  if (fread(data, *r_len, 1, fp) != 1) {
    *r_len = -1;
    MEM_freeN(data);
    fclose(fp);
    return nullptr;
  }

  fclose(fp);
  return data;
}

static int convert_include(const char *filepath)
{
  /* read include file, skip structs with a '#' before it.
   * store all data in temporal arrays.
   */

  int maindata_len;
  char *maindata = static_cast<char *>(read_file_data(filepath, &maindata_len));
  char *md = maindata;
  if (maindata_len == -1) {
    fprintf(stderr, "Can't read file %s\n", filepath);
    return 1;
  }

  maindata_len = preprocess_include(maindata, maindata_len);
  char *mainend = maindata + maindata_len - 1;

  /* we look for '{' and then back to 'struct' */
  int count = 0;
  bool skip_struct = false;
  while (count < maindata_len) {

    /* code for skipping a struct: two hashes on 2 lines. (preprocess added a space) */
    if (md[0] == '#' && md[1] == ' ' && md[2] == '#') {
      skip_struct = true;
    }

    if (md[0] == '{') {
      md[0] = 0;
      if (skip_struct) {
        skip_struct = false;
      }
      else {
        if (md[-1] == ' ') {
          md[-1] = 0;
        }
        char *md1 = md - 2;
        while (*md1 != 32) {
          /* to beginning of word */
          md1--;
        }
        md1++;

        /* we've got a struct name when... */
        if (match_identifier(md1 - 7, "struct")) {

          const int struct_type_index = add_type(md1, 0);
          if (struct_type_index == -1) {
            fprintf(stderr, "File '%s' contains struct we can't parse \"%s\"\n", filepath, md1);
            return 1;
          }

          short *structpoin = add_struct(struct_type_index);
          short *sp = structpoin + 2;

          DEBUG_PRINTF(1, "\t|\t|-- detected struct %s\n", types[struct_type_index]);

          /* first lets make it all nice strings */
          md1 = md + 1;
          while (*md1 != '}') {
            if (md1 > mainend) {
              break;
            }

            if (ELEM(*md1, ',', ' ')) {
              *md1 = 0;
            }
            md1++;
          }

          /* read types and names until first character that is not '}' */
          md1 = md + 1;
          while (*md1 != '}') {
            if (md1 > mainend) {
              break;
            }

            /* skip when it says 'struct' or 'unsigned' or 'const' */
            if (*md1) {
              const char *md1_prev = md1;
              while (match_identifier_and_advance(&md1, "struct") ||
                     match_identifier_and_advance(&md1, "unsigned") ||
                     match_identifier_and_advance(&md1, "const"))
              {
                if (UNLIKELY(!ELEM(*md1, '\0', ' '))) {
                  /* This will happen with: `unsigned(*value)[3]` which isn't supported. */
                  fprintf(stderr,
                          "File '%s' contains non white space character "
                          "\"%c\" after identifier \"%s\"\n",
                          filepath,
                          *md1,
                          md1_prev);
                  return 1;
                }
                /* Skip ' ' or '\0'. */
                md1++;
              }

              /* we've got a type! */
              if (STR_ELEM(md1, "long", "ulong")) {
                /* Forbid using long/ulong because those can be either 32 or 64 bit. */
                fprintf(stderr,
                        "File '%s' contains use of \"%s\" in DNA struct which is not allowed\n",
                        filepath,
                        md1);
                return -1;
              }
              const int member_type_index = add_type(md1, 0);
              if (member_type_index == -1) {
                fprintf(
                    stderr, "File '%s' contains struct we can't parse \"%s\"\n", filepath, md1);
                return 1;
              }

              DEBUG_PRINTF(1, "\t|\t|\tfound type %s (", md1);

              md1 += strlen(md1);

              /* read until ';' */
              while (*md1 != ';') {
                if (md1 > mainend) {
                  break;
                }

                if (*md1) {
                  /* We've got a name. slen needs
                   * correction for function
                   * pointers! */
                  int slen = int(strlen(md1));
                  if (md1[slen - 1] == ';') {
                    md1[slen - 1] = 0;

                    const int name = add_member(
                        version_member_static_from_alias(struct_type_index, md1));
                    if (name == -1) {
                      fprintf(stderr,
                              "File '%s' contains struct with name that can't be added \"%s\"\n",
                              filepath,
                              md1);
                      return 1;
                    }
                    slen += additional_slen_offset;
                    sp[0] = member_type_index;
                    sp[1] = name;

                    if (members[name] != nullptr) {
                      DEBUG_PRINTF(1, "%s |", members[name]);
                    }

                    structpoin[1]++;
                    sp += 2;

                    md1 += slen;
                    break;
                  }

                  const int name = add_member(
                      version_member_static_from_alias(struct_type_index, md1));
                  if (name == -1) {
                    fprintf(stderr,
                            "File '%s' contains struct with name that can't be added \"%s\"\n",
                            filepath,
                            md1);
                    return 1;
                  }
                  slen += additional_slen_offset;

                  sp[0] = member_type_index;
                  sp[1] = name;
                  if (members[name] != nullptr) {
                    DEBUG_PRINTF(1, "%s ||", members[name]);
                  }

                  structpoin[1]++;
                  sp += 2;

                  md1 += slen;
                }
                md1++;
              }

              DEBUG_PRINTF(1, ")\n");
            }
            md1++;
          }
        }
      }
    }
    count++;
    md++;
  }

  MEM_freeN(maindata);

  return 0;
}

static bool check_field_alignment(int firststruct,
                                  int struct_type_index,
                                  int type,
                                  int len,
                                  const char *name,
                                  const char *detail)
{
  bool result = true;
  if (type < firststruct && types_size_native[type] > 4 && (len % 8)) {
    fprintf(stderr,
            "Align 8 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[struct_type_index],
            name,
            len % 8);
    result = false;
  }
  if (types_size_native[type] > 3 && (len % 4)) {
    fprintf(stderr,
            "Align 4 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[struct_type_index],
            name,
            len % 4);
    result = false;
  }
  if (types_size_native[type] == 2 && (len % 2)) {
    fprintf(stderr,
            "Align 2 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[struct_type_index],
            name,
            len % 2);
    result = false;
  }
  return result;
}

static int calculate_struct_sizes(int firststruct, FILE *file_verify, const char *base_directory)
{
  bool dna_error = false;

  /* Write test to verify sizes are accurate. */
  fprintf(file_verify, "/* Verify struct sizes and member offsets are as expected by DNA. */\n");
  fprintf(file_verify, "#include \"BLI_assert.h\"\n\n");
  /* Needed so we can find offsets of deprecated structs. */
  fprintf(file_verify, "#define DNA_DEPRECATED_ALLOW\n");
  /* Workaround enum naming collision in static asserts
   * (ideally this included a unique name/id per file). */
  fprintf(file_verify, "#define assert_line_ assert_line_DNA_\n");
  for (int i = 0; *(includefiles[i]) != '\0'; i++) {
    fprintf(file_verify, "#include \"%s%s\"\n", base_directory, includefiles[i]);
  }
  fprintf(file_verify, "#undef assert_line_\n");
  fprintf(file_verify, "\n");

  /* Multiple iterations to handle nested structs. */
  /* 'raw data' SDNA_RAW_DATA_STRUCT_INDEX fake struct should be ignored here. */
  int unknown = structs_num - 1;
  while (unknown) {
    const int lastunknown = unknown;
    unknown = 0;

    /* check all structs... */
    BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")
    for (int a = SDNA_RAW_DATA_STRUCT_INDEX + 1; a < structs_num; a++) {
      const short *structpoin = structs[a];
      const int struct_type_index = structpoin[0];
      const char *struct_type_name = version_struct_alias_from_static(types[struct_type_index]);

      /* when length is not known... */
      if (types_size_native[struct_type_index] == 0) {

        const short *sp = structpoin + 2;
        int size_native = 0;
        int size_32 = 0;
        int size_64 = 0;
        /* Sizes of the largest field in a struct. */
        int max_align_32 = 0;
        int max_align_64 = 0;

        /* check all members in struct */
        for (int b = 0; b < structpoin[1]; b++, sp += 2) {
          int type = sp[0];
          const char *cp = members[sp[1]];
          int namelen = int(strlen(cp));

          /* Write size verification to file. */
          {
            /* Normally 'alloca' would be used here, however we can't in a loop.
             * Use an over-sized buffer instead. */
            char name_static[1024];
            BLI_assert(sizeof(name_static) > namelen);

            DNA_member_id_strip_copy(name_static, cp);
            const char *str_pair[2] = {types[struct_type_index], name_static};
            const char *name_alias = static_cast<const char *>(
                BLI_ghash_lookup(g_version_data.member_map_alias_from_static, str_pair));
            fprintf(file_verify,
                    "BLI_STATIC_ASSERT(offsetof(struct %s, %s) == %d, \"DNA member offset "
                    "verify\");\n",
                    struct_type_name,
                    name_alias ? name_alias : name_static,
                    size_native);
          }

          /* is it a pointer or function pointer? */
          if (cp[0] == '*' || cp[1] == '*') {
            /* has the name an extra length? (array) */
            int mul = 1;
            if (cp[namelen - 1] == ']') {
              mul = DNA_member_array_num(cp);
            }

            if (mul == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      types[struct_type_index],
                      namelen + 1,
                      cp);
              dna_error = true;
            }

            /* 4-8 aligned/ */
            if (sizeof(void *) == 4) {
              if (size_native % 4) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 4): %s %s\n",
                        types[struct_type_index],
                        cp);
                dna_error = true;
              }
            }
            else {
              if (size_native % 8) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 8): %s %s\n",
                        types[struct_type_index],
                        cp);
                dna_error = true;
              }
            }

            if (size_64 % 8) {
              fprintf(stderr,
                      "Align pointer error in struct (size_64 8): %s %s\n",
                      types[struct_type_index],
                      cp);
              dna_error = true;
            }

            size_native += sizeof(void *) * mul;
            size_32 += 4 * mul;
            size_64 += 8 * mul;
            max_align_32 = std::max(max_align_32, 4);
            max_align_64 = std::max(max_align_64, 8);
          }
          else if (cp[0] == '[') {
            /* parsing can cause names "var" and "[3]"
             * to be found for "float var [3]" */
            fprintf(stderr,
                    "Parse error in struct, invalid member name: %s %s\n",
                    types[struct_type_index],
                    cp);
            dna_error = true;
          }
          else if (types_size_native[type]) {
            /* has the name an extra length? (array) */
            int mul = 1;
            if (cp[namelen - 1] == ']') {
              mul = DNA_member_array_num(cp);
            }

            if (mul == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      types[struct_type_index],
                      namelen + 1,
                      cp);
              dna_error = true;
            }

            /* struct alignment */
            if (type >= firststruct) {
              if (sizeof(void *) == 8 && (size_native % 8)) {
                fprintf(stderr,
                        "Align struct error: %s::%s (starts at %d on the native platform; "
                        "%d %% %zu = %d bytes)\n",
                        types[struct_type_index],
                        cp,
                        size_native,
                        size_native,
                        sizeof(void *),
                        size_native % 8);
                dna_error = true;
              }
            }

            /* Check 2-4-8 aligned. */
            if (!check_field_alignment(
                    firststruct, struct_type_index, type, size_32, cp, "32 bit"))
            {
              dna_error = true;
            }
            if (!check_field_alignment(
                    firststruct, struct_type_index, type, size_64, cp, "64 bit"))
            {
              dna_error = true;
            }

            size_native += mul * types_size_native[type];
            size_32 += mul * types_size_32[type];
            size_64 += mul * types_size_64[type];
            max_align_32 = std::max<int>(max_align_32, types_align_32[type]);
            max_align_64 = std::max<int>(max_align_64, types_align_64[type]);
          }
          else {
            size_native = 0;
            size_32 = 0;
            size_64 = 0;
            break;
          }
        }

        if (size_native == 0) {
          unknown++;
        }
        else {
          types_size_native[struct_type_index] = size_native;
          types_size_32[struct_type_index] = size_32;
          types_size_64[struct_type_index] = size_64;
          types_align_32[struct_type_index] = max_align_32;
          types_align_64[struct_type_index] = max_align_64;

          /* Sanity check 1: alignment should never be 0. */
          BLI_assert(max_align_32);
          BLI_assert(max_align_64);

          /* Sanity check 2: alignment should always be equal or smaller than the maximum
           * size of a build in type which is 8 bytes (i.e. `int64_t` or double). */
          BLI_assert(max_align_32 <= 8);
          BLI_assert(max_align_64 <= 8);

          if (size_32 % max_align_32) {
            /* There is an one odd case where only the 32 bit struct has alignment issues
             * and the 64 bit does not, that can only be fixed by adding a padding pointer
             * to the struct to resolve the problem. */
            if ((size_64 % max_align_64 == 0) && (size_32 % max_align_32 == 4)) {
              fprintf(stderr,
                      "Sizeerror in 32 bit struct: %s (add padding pointer)\n",
                      types[struct_type_index]);
            }
            else {
              fprintf(stderr,
                      "Sizeerror in 32 bit struct: %s (add %d bytes)\n",
                      types[struct_type_index],
                      max_align_32 - (size_32 % max_align_32));
            }
            dna_error = true;
          }

          if (size_64 % max_align_64) {
            fprintf(stderr,
                    "Sizeerror in 64 bit struct: %s (add %d bytes)\n",
                    types[struct_type_index],
                    max_align_64 - (size_64 % max_align_64));
            dna_error = true;
          }

          if (size_native % 4 && !ELEM(size_native, 1, 2)) {
            fprintf(stderr,
                    "Sizeerror 4 in struct: %s (add %d bytes)\n",
                    types[struct_type_index],
                    size_native % 4);
            dna_error = true;
          }

          /* Write size verification to file. */
          fprintf(file_verify,
                  "BLI_STATIC_ASSERT(sizeof(struct %s) == %d, \"DNA struct size verify\");\n\n",
                  struct_type_name,
                  size_native);
        }
      }
    }

    if (unknown == lastunknown) {
      break;
    }
  }

  if (unknown) {
    fprintf(stderr, "ERROR: still %d structs unknown\n", unknown);

    if (debugSDNA) {
      fprintf(stderr, "*** Known structs :\n");

      for (int a = 0; a < structs_num; a++) {
        const short *structpoin = structs[a];
        const int structtype = structpoin[0];

        /* length unknown */
        if (types_size_native[structtype] != 0) {
          fprintf(stderr, "  %s\n", types[structtype]);
        }
      }
    }

    fprintf(stderr, "*** Unknown structs :\n");

    for (int a = 0; a < structs_num; a++) {
      const short *structpoin = structs[a];
      const int structtype = structpoin[0];

      /* length unknown yet */
      if (types_size_native[structtype] == 0) {
        fprintf(stderr, "  %s\n", types[structtype]);
      }
    }

    dna_error = true;
  }

  return dna_error;
}

#define MAX_DNA_LINE_LENGTH 20

static void dna_write(FILE *file, const void *pntr, const int size)
{
  static int linelength = 0;
  const char *data = (const char *)pntr;

  for (int i = 0; i < size; i++) {
    fprintf(file, "%d, ", data[i]);
    linelength++;
    if (linelength >= MAX_DNA_LINE_LENGTH) {
      fprintf(file, "\n");
      linelength = 0;
    }
  }
}

void print_struct_sizes()
{
  int unknown = structs_num;
  printf("\n\n*** All detected structs:\n");

  while (unknown) {
    unknown = 0;

    /* check all structs... */
    for (int a = 0; a < structs_num; a++) {
      const short *structpoin = structs[a];
      const int structtype = structpoin[0];
      printf("\t%s\t:%d\n", types[structtype], types_size_native[structtype]);
    }
  }

  printf("*** End of list\n");
}

static int make_structDNA(const char *base_directory,
                          FILE *file,
                          FILE *file_offsets,
                          FILE *file_verify)
{
  if (debugSDNA > 0) {
    fflush(stdout);
    printf("Running makesdna at debug level %d\n", debugSDNA);
  }

  mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  /* the longest known struct is 50k, so we assume 100k is sufficient! */
  structdata = static_cast<short *>(MEM_callocN(max_data_size, "structdata"));

  /* a maximum of 5000 variables, must be sufficient? */
  members = static_cast<char **>(MEM_callocN(sizeof(char *) * max_array_len, "names"));
  types = static_cast<char **>(MEM_callocN(sizeof(char *) * max_array_len, "types"));
  types_size_native = static_cast<short *>(
      MEM_callocN(sizeof(short) * max_array_len, "types_size_native"));
  types_size_32 = static_cast<short *>(
      MEM_callocN(sizeof(short) * max_array_len, "types_size_32"));
  types_size_64 = static_cast<short *>(
      MEM_callocN(sizeof(short) * max_array_len, "types_size_64"));
  types_align_32 = static_cast<short *>(
      MEM_callocN(sizeof(short) * max_array_len, "types_size_32"));
  types_align_64 = static_cast<short *>(
      MEM_callocN(sizeof(short) * max_array_len, "types_size_64"));

  structs = static_cast<short **>(MEM_callocN(sizeof(short *) * max_array_len, "structs"));

  /* Build versioning data */
  DNA_alias_maps(DNA_RENAME_ALIAS_FROM_STATIC,
                 &g_version_data.type_map_alias_from_static,
                 &g_version_data.member_map_alias_from_static);
  DNA_alias_maps(DNA_RENAME_STATIC_FROM_ALIAS,
                 &g_version_data.type_map_static_from_alias,
                 &g_version_data.member_map_static_from_alias);

  /**
   * Insertion of all known types.
   *
   * \warning Order of function calls here must be aligned with #eSDNA_Type.
   * \warning uint is not allowed! use in structs an unsigned int.
   * \warning sizes must match #DNA_elem_type_size().
   */
  add_type("char", 1);   /* SDNA_TYPE_CHAR */
  add_type("uchar", 1);  /* SDNA_TYPE_UCHAR */
  add_type("short", 2);  /* SDNA_TYPE_SHORT */
  add_type("ushort", 2); /* SDNA_TYPE_USHORT */
  add_type("int", 4);    /* SDNA_TYPE_INT */

  /* NOTE: long isn't supported,
   * these are place-holders to maintain alignment with #eSDNA_Type. */
  add_type("long", 4);  /* SDNA_TYPE_LONG */
  add_type("ulong", 4); /* SDNA_TYPE_ULONG */

  add_type("float", 4);    /* SDNA_TYPE_FLOAT */
  add_type("double", 8);   /* SDNA_TYPE_DOUBLE */
  add_type("int64_t", 8);  /* SDNA_TYPE_INT64 */
  add_type("uint64_t", 8); /* SDNA_TYPE_UINT64 */
  add_type("void", 0);     /* SDNA_TYPE_VOID */
  add_type("int8_t", 1);   /* SDNA_TYPE_INT8 */

  /* Fake place-holder struct definition used to get an identifier for raw, untyped bytes buffers
   * in blend-files.
   *
   * It will be written into the blend-files SDNA, but it must never be used in the source code.
   * Trying to declare `struct raw_data` in DNA headers will cause a build error.
   *
   * NOTE: While not critical, since all blend-files before introduction of this 'raw_data'
   * type/struct have been using the `0` value for raw data #BHead.SDNAnr, it's best to reserve
   * that first struct index to this raw data explicitly. */
  const int raw_data_type_index = add_type("raw_data", 0); /* SDNA_TYPE_RAW_DATA */
  short *raw_data_struct_info = add_struct(raw_data_type_index);
  /* There are no members in this struct. */
  raw_data_struct_info[1] = 0;
  BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")
  BLI_assert(raw_data_struct_info == structs[SDNA_RAW_DATA_STRUCT_INDEX]);

  /* the defines above shouldn't be output in the padding file... */
  const int firststruct = types_num;

  /* Add all include files defined in the global array.
   * Since the internal file+path name buffer has limited length,
   * I do a little test first...
   * Mind the breaking condition here! */
  DEBUG_PRINTF(0, "\tStart of header scan:\n");
  int header_count = 0;
  for (int i = 0; *(includefiles[i]) != '\0'; i++) {
    header_count++;

    /* NOTE(nzc): `str` contains filenames.
     * Since we now include paths, I stretched it a bit. Hope this is enough :). */
    char str[SDNA_MAX_FILENAME_LENGTH];
    SNPRINTF(str, "%s%s", base_directory, includefiles[i]);
    DEBUG_PRINTF(0, "\t|-- Converting %s\n", str);
    if (convert_include(str)) {
      return 1;
    }
  }
  DEBUG_PRINTF(0, "\tFinished scanning %d headers.\n", header_count);

  if (calculate_struct_sizes(firststruct, file_verify, base_directory)) {
    /* error */
    return 1;
  }

  /* FOR DEBUG */
  if (debugSDNA > 1) {
    int a, b;
    // short *elem;
    short struct_members_num;

    printf("names_len %d types_len %d structs_len %d\n", members_num, types_num, structs_num);
    for (a = 0; a < members_num; a++) {
      printf(" %s\n", members[a]);
    }
    printf("\n");

    const short *sp = types_size_native;
    for (a = 0; a < types_num; a++, sp++) {
      printf(" %s %d\n", types[a], *sp);
    }
    printf("\n");

    for (a = 0; a < structs_num; a++) {
      sp = structs[a];
      printf(" struct %s elems: %d size: %d\n", types[sp[0]], sp[1], types_size_native[sp[0]]);
      struct_members_num = sp[1];
      sp += 2;
      for (b = 0; b < struct_members_num; b++, sp += 2) {
        printf("   %s %s allign32:%d, allign64:%d\n",
               types[sp[0]],
               members[sp[1]],
               types_align_32[sp[0]],
               types_align_64[sp[0]]);
      }
    }
  }

  /* file writing */

  DEBUG_PRINTF(0, "Writing file ... ");

  if (members_num == 0 || structs_num == 0) {
    /* pass */
  }
  else {
    const char nil_bytes[4] = {0};

    dna_write(file, "SDNA", 4);

    /* write names */
    dna_write(file, "NAME", 4);
    int len = members_num;
    dna_write(file, &len, 4);
    /* write array */
    len = 0;
    for (int member_index = 0; member_index < members_num; member_index++) {
      int member_len = strlen(members[member_index]) + 1;
      dna_write(file, members[member_index], member_len);
      len += member_len;
    }
    int len_align = (len + 3) & ~3;
    if (len != len_align) {
      dna_write(file, nil_bytes, len_align - len);
    }

    /* write TYPES */
    dna_write(file, "TYPE", 4);
    len = types_num;
    dna_write(file, &len, 4);
    /* write array */
    len = 0;
    for (int type_index = 0; type_index < types_num; type_index++) {
      int type_len = strlen(types[type_index]) + 1;
      dna_write(file, types[type_index], type_len);
      len += type_len;
    }
    len_align = (len + 3) & ~3;
    if (len != len_align) {
      dna_write(file, nil_bytes, len_align - len);
    }

    /* WRITE TYPELENGTHS */
    dna_write(file, "TLEN", 4);

    len = 2 * types_num;
    if (types_num & 1) {
      len += 2;
    }
    dna_write(file, types_size_native, len);

    /* WRITE STRUCTS */
    dna_write(file, "STRC", 4);
    len = structs_num;
    dna_write(file, &len, 4);

    /* calc datablock size */
    const short *sp = structs[structs_num - 1];
    sp += 2 + 2 * (sp[1]);
    len = intptr_t((char *)sp - (char *)structs[0]);
    len = (len + 3) & ~3;

    dna_write(file, structs[0], len);
  }

  /* write a simple enum with all structs offsets,
   * should only be accessed via SDNA_TYPE_FROM_STRUCT macro */
  {
    fprintf(file_offsets, "#pragma once\n");
    fprintf(file_offsets, "#define SDNA_TYPE_FROM_STRUCT(id) _SDNA_TYPE_##id\n");
    fprintf(file_offsets, "enum {\n");
    for (int i = 0; i < structs_num; i++) {
      const short *structpoin = structs[i];
      const int struct_type_index = structpoin[0];
      fprintf(file_offsets,
              "\t_SDNA_TYPE_%s = %d,\n",
              version_struct_alias_from_static(types[struct_type_index]),
              i);
    }
    fprintf(file_offsets, "\tSDNA_TYPE_MAX = %d,\n", structs_num);
    fprintf(file_offsets, "};\n\n");
  }

  /* Check versioning errors which could cause duplicate names,
   * do last because names are stripped. */
  {
    GSet *members_unique = BLI_gset_str_new_ex(__func__, 512);
    for (int struct_index = 0; struct_index < structs_num; struct_index++) {
      const short *sp = structs[struct_index];
      const char *type = types[sp[0]];
      const int len = sp[1];
      sp += 2;
      for (int a = 0; a < len; a++, sp += 2) {
        char *member = members[sp[1]];
        DNA_member_id_strip(member);
        if (!BLI_gset_add(members_unique, member)) {
          fprintf(stderr,
                  "Error: duplicate name found '%s.%s', "
                  "likely cause is 'dna_rename_defs.h'\n",
                  type,
                  member);
          return 1;
        }
      }
      BLI_gset_clear(members_unique, nullptr);
    }
    BLI_gset_free(members_unique, nullptr);
  }

  MEM_freeN(structdata);
  MEM_freeN(members);
  MEM_freeN(types);
  MEM_freeN(types_size_native);
  MEM_freeN(types_size_32);
  MEM_freeN(types_size_64);
  MEM_freeN(types_align_32);
  MEM_freeN(types_align_64);
  MEM_freeN(structs);

  BLI_memarena_free(mem_arena);

  BLI_ghash_free(g_version_data.type_map_alias_from_static, nullptr, nullptr);
  BLI_ghash_free(g_version_data.type_map_static_from_alias, nullptr, nullptr);
  BLI_ghash_free(g_version_data.member_map_static_from_alias, MEM_freeN, nullptr);
  BLI_ghash_free(g_version_data.member_map_alias_from_static, MEM_freeN, nullptr);

  DEBUG_PRINTF(0, "done.\n");

  return 0;
}

/** \} */

/* end make DNA. */

/* -------------------------------------------------------------------- */
/** \name Main Function
 * \{ */

static void make_bad_file(const char *file, int line)
{
  FILE *fp = fopen(file, "w");
  fprintf(fp,
          "#error \"Error! can't make correct DNA.c file from %s:%d, check alignment.\"\n",
          __FILE__,
          line);
  fclose(fp);
}

#ifndef BASE_HEADER
#  define BASE_HEADER "../"
#endif

int main(int argc, char **argv)
{
  int return_status = 0;

  if (!ELEM(argc, 4, 5)) {
    printf("Usage: %s dna.c dna_struct_offsets.h [base directory]\n", argv[0]);
    return_status = 1;
  }
  else {
    FILE *file_dna = fopen(argv[1], "w");
    FILE *file_dna_offsets = fopen(argv[2], "w");
    FILE *file_dna_verify = fopen(argv[3], "w");
    if (!file_dna) {
      printf("Unable to open file: %s\n", argv[1]);
      return_status = 1;
    }
    else if (!file_dna_offsets) {
      printf("Unable to open file: %s\n", argv[2]);
      return_status = 1;
    }
    else if (!file_dna_verify) {
      printf("Unable to open file: %s\n", argv[3]);
      return_status = 1;
    }
    else {
      const char *base_directory;

      if (argc == 5) {
        base_directory = argv[4];
      }
      else {
        base_directory = BASE_HEADER;
      }

      /* NOTE: #init_structDNA() in dna_genfile.cc expects `sdna->data` is 4-bytes aligned.
       * `DNAstr[]` buffer written by `makesdna` is used for this data, so make `DNAstr` forcefully
       * 4-bytes aligned. */
#ifdef __GNUC__
#  define FORCE_ALIGN_4 " __attribute__((aligned(4))) "
#else
#  define FORCE_ALIGN_4 " "
#endif
      fprintf(file_dna, "extern const unsigned char DNAstr[];\n");
      fprintf(file_dna, "const unsigned char" FORCE_ALIGN_4 "DNAstr[] = {\n");
#undef FORCE_ALIGN_4

      if (make_structDNA(base_directory, file_dna, file_dna_offsets, file_dna_verify)) {
        /* error */
        fclose(file_dna);
        file_dna = nullptr;
        make_bad_file(argv[1], __LINE__);
        return_status = 1;
      }
      else {
        fprintf(file_dna, "};\n");
        fprintf(file_dna, "extern const int DNAlen;\n");
        fprintf(file_dna, "const int DNAlen = sizeof(DNAstr);\n");
      }
    }

    if (file_dna) {
      fclose(file_dna);
    }
    if (file_dna_offsets) {
      fclose(file_dna_offsets);
    }
    if (file_dna_verify) {
      fclose(file_dna_verify);
    }
  }

  return return_status;
}

/* handy but fails on struct bounds which makesdna doesn't care about
 * with quite the same strictness as GCC does */
#if 0
/* include files for automatic dependencies */

/* extra safety check that we are aligned,
 * warnings here are easier to fix the makesdna's */
#  ifdef __GNUC__
#    pragma GCC diagnostic error "-Wpadded"
#  endif

#endif /* if 0 */

/* The include file below is automatically generated from the `SRC_DNA_INC`
 * variable in 'source/blender/CMakeLists.txt'. */
#include "dna_includes_all.h"

/* end of list */

/** \} */

/* -------------------------------------------------------------------- */
/** \name DNA Renaming Sanity Check
 *
 * Without this it's possible to reference struct members that don't exist,
 * breaking backward & forward compatibility.
 *
 * \{ */

static void UNUSED_FUNCTION(dna_rename_defs_ensure)()
{
#define DNA_STRUCT_RENAME(old, new) (void)sizeof(new);
#define DNA_STRUCT_RENAME_MEMBER(struct_name, old, new) (void)offsetof(struct_name, new);
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
}

/** \} */
