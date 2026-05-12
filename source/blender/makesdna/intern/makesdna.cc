/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2002-2026 Blender Authors
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

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h" /* For `intptr_t` support. */
#include "BLI_system.h"    /* For #BLI_system_backtrace stub. */
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_genfile.h"
#include "DNA_sdna_types.h"
#include "dna_parse.h"
#include "dna_utils.h"

namespace blender {

/* Include files that will be used to generate makesdna output.
 * By default, they match the blender_includefiles, but could be overridden via a command line
 * argument for the purposes of regression testing. */
static Span<const char *> includefiles = dna::default_dna_header_filenames();

/* -------------------------------------------------------------------- */
/** \name Debugging
 * \{ */

/**
 * Variable to control debug output of makesdna.
 * debugSDNA:
 * - 0 = no output, except errors
 * - 1 = detail actions
 * - 2 = full trace, tell which names and types were found
 * - 4 = full trace, plus all gritty details
 */
extern int debugSDNA; /* Defined in `dna_parse.cc`. */

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
/** \name Type Table
 *
 * \{ */

/** SDNA builtin or struct type. */
struct TypeInfo {
  const std::string name;
  /* Native, 32 bit and 64 bit platform sizes. */
  short size_native;
  short size_32;
  short size_64;
  /* Alignment requirements for 32 and 64 bit platforms. */
  short align_32;
  short align_64;
  /* Struct or built-in type? */
  bool is_struct;
};

/** Table of unique SDNA types. */
struct TypeTable {
  /* Using a vector set with guaranteed insertion order, so that resulting SDNA
   * types are always in the same order. */
  class GetIDFn {
   public:
    StringRef operator()(const TypeInfo &info) const
    {
      return info.name;
    }
  };
  CustomIDVectorSet<TypeInfo, GetIDFn> types;

  void add_builtin(const StringRef name, const short size, const int expected_type_index)
  {
    BLI_assert(this->types.size() == expected_type_index);
    UNUSED_VARS_NDEBUG(expected_type_index);
    this->types.add_new({.name = name,
                         .size_native = size,
                         .size_32 = size,
                         .size_64 = size,
                         .align_32 = size,
                         .align_64 = size,
                         .is_struct = false});
  }

  /** Add a struct type if it doesn't exist yet. Sizes will be computed later. */
  void add_struct(StringRefNull name)
  {
    this->types.add({.name = name,
                     .size_native = 0,
                     .size_32 = 0,
                     .size_64 = 0,
                     .align_32 = 0,
                     .align_64 = 0,
                     .is_struct = true});
  }

  /** Look up the index of an existing type. */
  int lookup_index(StringRefNull name) const
  {
    return this->types.index_of_as(name);
  }

  /** Look up an existing type by name. */
  TypeInfo &lookup(StringRefNull name)
  {
    /* Const cast is okay because only TypeInfo::name is used for #VectorSet hash and equality. */
    return const_cast<TypeInfo &>(this->types[this->lookup_index(name)]);
  }
  const TypeInfo &lookup(StringRefNull name) const
  {
    return this->types[this->lookup_index(name)];
  }
};

static TypeTable build_type_table(const Span<dna::ParsedStruct> parsed_structs)
{
  TypeTable table;

  /* Insert built-in types.
   *
   * \warning Order of function calls here must be aligned with #eSDNA_Type.
   * \warning uint is not allowed! use in structs an unsigned int.
   * \warning sizes must match #DNA_elem_type_size().
   */
  table.add_builtin("char", 1, SDNA_TYPE_CHAR);
  table.add_builtin("uchar", 1, SDNA_TYPE_UCHAR);
  table.add_builtin("short", 2, SDNA_TYPE_SHORT);
  table.add_builtin("ushort", 2, SDNA_TYPE_USHORT);
  table.add_builtin("int", 4, SDNA_TYPE_INT);

  /* NOTE: long isn't supported, these are place-holders to maintain alignment with #eSDNA_Type. */
  table.add_builtin("long", 4, 5 /* SDNA_TYPE_LONG */);
  table.add_builtin("ulong", 4, 6 /* SDNA_TYPE_ULONG */);

  table.add_builtin("float", 4, SDNA_TYPE_FLOAT);
  table.add_builtin("double", 8, SDNA_TYPE_DOUBLE);
  table.add_builtin("int64_t", 8, SDNA_TYPE_INT64);
  table.add_builtin("uint64_t", 8, SDNA_TYPE_UINT64);
  table.add_builtin("void", 0, SDNA_TYPE_VOID);
  table.add_builtin("int8_t", 1, SDNA_TYPE_INT8);

  /* Fake place-holder struct definition used to get an identifier for raw, untyped bytes buffers
   * in blend-files.
   *
   * It will be written into the blend-file's SDNA, but it must never be used in the source code.
   * Trying to declare `struct raw_data` in DNA headers will cause a build error.
   *
   * NOTE: While not critical, since all blend-files before introduction of this 'raw_data'
   * type/struct have been using the `0` value for raw data #BHead.SDNAnr, it's best to reserve
   * that first struct index to this raw data explicitly. */
  table.add_struct("raw_data"); /* SDNA_TYPE_RAW_DATA */
  BLI_STATIC_ASSERT(SDNA_RAW_DATA_STRUCT_INDEX == 0, "'raw data' SDNA struct index should be 0")

  /* Insert struct and member types. */
  for (const dna::ParsedStruct &parsed_struct : parsed_structs) {
    table.add_struct(parsed_struct.type_name);
    for (const dna::ParsedMember &parsed_member : parsed_struct.members) {
      table.add_struct(parsed_member.type_name);
    }
  }

  return table;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type Sizes and Alignment
 * \{ */

/** Native byte size contributed by a single member to the struct. */
static int member_size_native(const TypeInfo &member_type, const dna::ParsedMember &parsed_member)
{
  const char *cp = parsed_member.member_name.c_str();
  const int namelen = int(parsed_member.member_name.size());
  const int array_num = (cp[namelen - 1] == ']') ? DNA_member_array_num(cp) : 1;
  /* Pointer size. */
  if (cp[0] == '*' || cp[1] == '*') {
    return sizeof(void *) * array_num;
  }
  return member_type.size_native * array_num;
}

static bool check_member_alignment(const TypeInfo &struct_info,
                                   const TypeInfo &member_type,
                                   const int len,
                                   const int member_align_override,
                                   const StringRefNull name,
                                   const StringRefNull detail)
{
  bool result = true;
  const char *struct_name = struct_info.name.c_str();
  const short type_size = member_type.size_native;
  if (member_align_override > 0 && (len % member_align_override)) {
    fprintf(stderr,
            "Align %d error (%s) in struct: %s %s (add %d padding bytes)\n",
            member_align_override,
            detail.c_str(),
            struct_name,
            name.c_str(),
            member_align_override - (len % member_align_override));
    result = false;
  }
  if (!member_type.is_struct && type_size > 4 && (len % 8)) {
    fprintf(stderr,
            "Align 8 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail.c_str(),
            struct_name,
            name.c_str(),
            len % 8);
    result = false;
  }
  if (type_size > 3 && (len % 4)) {
    fprintf(stderr,
            "Align 4 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail.c_str(),
            struct_name,
            name.c_str(),
            len % 4);
    result = false;
  }
  if (type_size == 2 && (len % 2)) {
    fprintf(stderr,
            "Align 2 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail.c_str(),
            struct_name,
            name.c_str(),
            len % 2);
    result = false;
  }
  return result;
}

/** Determine how many bytes are needed for each struct. */
static int compute_type_size_and_alignment(TypeTable &table,
                                           Span<dna::ParsedStruct> parsed_structs)
{
  bool dna_error = false;

  /* Multiple iterations to handle nested structs. */
  int unknown = int(parsed_structs.size());
  while (unknown) {
    const int lastunknown = unknown;
    unknown = 0;

    for (const dna::ParsedStruct &parsed_struct : parsed_structs) {
      TypeInfo &struct_info = table.lookup(parsed_struct.type_name);

      /* When size is not known yet. */
      if (struct_info.size_native == 0) {
        int size_native = 0;
        int size_32 = 0;
        int size_64 = 0;
        /* Sizes of the largest field in a struct. */
        int max_align_32 = 0;
        int max_align_64 = 0;

        /* check all members in struct */
        for (const dna::ParsedMember &parsed_member : parsed_struct.members) {
          const TypeInfo &member_type = table.lookup(parsed_member.type_name);
          const char *cp = parsed_member.member_name.c_str();
          const int namelen = int(parsed_member.member_name.size());

          /* is it a pointer or function pointer? */
          if (cp[0] == '*' || cp[1] == '*') {
            /* has the name an extra length? (array) */
            const int array_num = (cp[namelen - 1] == ']') ? DNA_member_array_num(cp) : 1;

            if (array_num == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      struct_info.name.c_str(),
                      namelen + 1,
                      cp);
              dna_error = true;
            }

            /* 4-8 aligned/ */
            if (sizeof(void *) == 4) {
              if (size_native % 4) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 4): %s %s\n",
                        struct_info.name.c_str(),
                        cp);
                dna_error = true;
              }
            }
            else {
              if (size_native % 8) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 8): %s %s\n",
                        struct_info.name.c_str(),
                        cp);
                dna_error = true;
              }
            }

            if (size_64 % 8) {
              fprintf(stderr,
                      "Align pointer error in struct (size_64 8): %s %s\n",
                      struct_info.name.c_str(),
                      cp);
              dna_error = true;
            }

            size_native += member_size_native(member_type, parsed_member);
            size_32 += 4 * array_num;
            size_64 += 8 * array_num;
            max_align_32 = std::max(max_align_32, 4);
            max_align_64 = std::max(max_align_64, 8);
          }
          else if (cp[0] == '[') {
            /* parsing can cause names "var" and "[3]"
             * to be found for "float var [3]" */
            fprintf(stderr,
                    "Parse error in struct, invalid member name: %s %s\n",
                    struct_info.name.c_str(),
                    cp);
            dna_error = true;
          }
          else if (member_type.size_native) {
            /* has the name an extra length? (array) */
            const int array_num = (cp[namelen - 1] == ']') ? DNA_member_array_num(cp) : 1;

            if (array_num == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      struct_info.name.c_str(),
                      namelen + 1,
                      cp);
              dna_error = true;
            }

            /* struct alignment */
            if (member_type.is_struct) {
              if (sizeof(void *) == 8 && (size_native % 8)) {
                fprintf(stderr,
                        "Align struct error: %s::%s (starts at %d on the native platform; "
                        "%d %% %zu = %d bytes)\n",
                        struct_info.name.c_str(),
                        cp,
                        size_native,
                        size_native,
                        sizeof(void *),
                        size_native % 8);
                dna_error = true;
              }
            }

            /* Per-member C++ alignment override from the parser. */
            const int member_align = parsed_member.alignment;

            /* Check 2-4-8 aligned, plus any stricter C++ alignment. */
            if (!check_member_alignment(
                    struct_info, member_type, size_32, member_align, cp, "32 bit"))
            {
              dna_error = true;
            }
            if (!check_member_alignment(
                    struct_info, member_type, size_64, member_align, cp, "64 bit"))
            {
              dna_error = true;
            }

            size_native += member_size_native(member_type, parsed_member);
            size_32 += array_num * member_type.size_32;
            size_64 += array_num * member_type.size_64;
            max_align_32 = std::max<int>(max_align_32, member_type.align_32);
            max_align_64 = std::max<int>(max_align_64, member_type.align_64);
            max_align_32 = std::max<int>(max_align_32, member_align);
            max_align_64 = std::max<int>(max_align_64, member_align);
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
          /* Sanity check: struct sizes are written as #short to the SDNA blob. */
          BLI_assert(size_native <= SHRT_MAX);
          BLI_assert(size_32 <= SHRT_MAX);
          BLI_assert(size_64 <= SHRT_MAX);

          struct_info.size_native = short(size_native);
          struct_info.size_32 = short(size_32);
          struct_info.size_64 = short(size_64);
          struct_info.align_32 = short(max_align_32);
          struct_info.align_64 = short(max_align_64);

          /* Sanity check 1: alignment should never be 0. */
          BLI_assert(max_align_32);
          BLI_assert(max_align_64);

          /* Sanity check 2: alignment should always be equal or smaller than the maximum
           * alignment we support. 8 bytes for built-in types (e.g. `int64_t`, `double`),
           * up to 16 bytes for C++ over-aligned types like `float4x4`. */
          BLI_assert(max_align_32 <= 16);
          BLI_assert(max_align_64 <= 16);

          if (size_32 % max_align_32) {
            /* There is an one odd case where only the 32 bit struct has alignment issues
             * and the 64 bit does not, that can only be fixed by adding a padding pointer
             * to the struct to resolve the problem. */
            if ((size_64 % max_align_64 == 0) && (size_32 % max_align_32 == 4)) {
              fprintf(stderr,
                      "Sizeerror in 32 bit struct: %s (add padding pointer)\n",
                      struct_info.name.c_str());
            }
            else {
              fprintf(stderr,
                      "Sizeerror in 32 bit struct: %s (add %d bytes)\n",
                      struct_info.name.c_str(),
                      max_align_32 - (size_32 % max_align_32));
            }
            dna_error = true;
          }

          if (size_64 % max_align_64) {
            fprintf(stderr,
                    "Sizeerror in 64 bit struct: %s (add %d bytes)\n",
                    struct_info.name.c_str(),
                    max_align_64 - (size_64 % max_align_64));
            dna_error = true;
          }

          if (size_native % 4 && !ELEM(size_native, 1, 2)) {
            fprintf(stderr,
                    "Sizeerror 4 in struct: %s (add %d bytes)\n",
                    struct_info.name.c_str(),
                    size_native % 4);
            dna_error = true;
          }
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
      for (const dna::ParsedStruct &ps : parsed_structs) {
        const TypeInfo &info = table.lookup(ps.type_name);
        if (info.size_native != 0) {
          fprintf(stderr, "  %s\n", info.name.c_str());
        }
      }
    }

    fprintf(stderr, "*** Unknown structs :\n");
    for (const dna::ParsedStruct &ps : parsed_structs) {
      const TypeInfo &info = table.lookup(ps.type_name);
      if (info.size_native == 0) {
        fprintf(stderr, "  %s\n", info.name.c_str());
      }
    }

    dna_error = true;
  }

  return dna_error;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DNA File Writing
 * \{ */

/** Construct the DNA.c file */
static void dna_write(FILE *file, const void *pntr, const int size)
{
  constexpr int MAX_DNA_LINE_LENGTH = 20;
  static int linelength = 0;
  const char *data = static_cast<const char *>(pntr);

  for (int i = 0; i < size; i++) {
    fprintf(file, "%d, ", data[i]);
    linelength++;
    if (linelength >= MAX_DNA_LINE_LENGTH) {
      fprintf(file, "\n");
      linelength = 0;
    }
  }
}

/** Serialize the SDNA tables to a file. */
static void write_sdna_blob(FILE *file,
                            const TypeTable &table,
                            const Span<dna::ParsedStruct> parsed_structs)
{
  /* `raw_data` plus parsed structs. */
  const int num_structs = 1 + int(parsed_structs.size());

  /* Deduplicated member names in source order. */
  VectorSet<StringRefNull> member_names;
  for (const dna::ParsedStruct &ps : parsed_structs) {
    for (const dna::ParsedMember &pm : ps.members) {
      member_names.add(pm.member_name);
    }
  }

  /* FOR DEBUG. */
  if (debugSDNA > 1) {
    printf("names_len %d types_len %d structs_len %d\n",
           int(member_names.size()),
           int(table.types.size()),
           num_structs);
    for (const StringRefNull name : member_names) {
      printf(" %s\n", name.c_str());
    }
    printf("\n");

    for (const TypeInfo &type : table.types) {
      printf(" %s %d\n", type.name.c_str(), type.size_native);
    }
    printf("\n");

    for (const dna::ParsedStruct &ps : parsed_structs) {
      const TypeInfo &info = table.lookup(ps.type_name);
      printf(" struct %s elems: %d size: %d\n",
             info.name.c_str(),
             int(ps.members.size()),
             info.size_native);
      for (const dna::ParsedMember &pm : ps.members) {
        const TypeInfo &member_type = table.lookup(pm.type_name);
        printf("   %s %s allign32:%d, allign64:%d\n",
               member_type.name.c_str(),
               pm.member_name.c_str(),
               member_type.align_32,
               member_type.align_64);
      }
    }
  }

  if (member_names.is_empty() || parsed_structs.is_empty()) {
    return;
  }

  const char nil_bytes[4] = {0};

  dna_write(file, "SDNA", 4);

  /* Write NAME: member names. */
  dna_write(file, "NAME", 4);
  int len = int(member_names.size());
  dna_write(file, &len, 4);
  len = 0;
  for (const StringRefNull member_name : member_names) {
    const int member_len = int(member_name.size()) + 1;
    dna_write(file, member_name.c_str(), member_len);
    len += member_len;
  }
  int len_align = (len + 3) & ~3;
  if (len != len_align) {
    dna_write(file, nil_bytes, len_align - len);
  }

  /* Write TYPES: built-in and struct table. */
  dna_write(file, "TYPE", 4);
  len = int(table.types.size());
  dna_write(file, &len, 4);
  len = 0;
  for (const TypeInfo &type : table.types) {
    const int type_len = int(type.name.size()) + 1;
    dna_write(file, type.name.c_str(), type_len);
    len += type_len;
  }
  len_align = (len + 3) & ~3;
  if (len != len_align) {
    dna_write(file, nil_bytes, len_align - len);
  }

  /* WRITE TLEN: size of each type. */
  dna_write(file, "TLEN", 4);
  for (const TypeInfo &type : table.types) {
    dna_write(file, &type.size_native, 2);
  }

  /* Pad to multiple of 4 bytes when types count is odd. */
  if (table.types.size() & 1) {
    dna_write(file, nil_bytes, 2);
  }

  /* WRITE STRUCTS */
  dna_write(file, "STRC", 4);
  dna_write(file, &num_structs, 4);

  BLI_assert_msg(table.types.size() < SHRT_MAX, "SDNA only supports up to SHRT_MAX types");

  /* Synthetic `raw_data` struct: type index, zero members. */
  const short raw_data_header[2] = {short(table.lookup_index("raw_data")), 0};
  dna_write(file, raw_data_header, 4);

  for (const dna::ParsedStruct &ps : parsed_structs) {
    const short header[2] = {short(table.lookup_index(ps.type_name)), short(ps.members.size())};
    dna_write(file, header, 4);
    for (const dna::ParsedMember &pm : ps.members) {
      const short pair[2] = {short(table.lookup_index(pm.type_name)),
                             short(member_names.index_of_as(pm.member_name))};
      dna_write(file, pair, 4);
    }
  }

  /* No padding needed as each entry is 4 bytes already. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auxiliary files
 * \{ */

/** Write the `dna_type_offsets.h` for `SDNA_TYPE_FROM_STRUCT(id)` macro. */
static void write_sdna_type_offsets(FILE *file, const Span<dna::ParsedStruct> parsed_structs)
{
  fprintf(file, "#pragma once\n");
  fprintf(file, "#define SDNA_TYPE_FROM_STRUCT(id) _SDNA_TYPE_##id\n");
  fprintf(file, "enum {\n");
  fprintf(file, "\t_SDNA_TYPE_raw_data = %d,\n", SDNA_RAW_DATA_STRUCT_INDEX);
  for (const int64_t i : parsed_structs.index_range()) {
    const int sdna_index = int(i) + 1;
    fprintf(
        file, "\t_SDNA_TYPE_%s = %d,\n", parsed_structs[i].alias_type_name.c_str(), sdna_index);
  }
  fprintf(file, "\tSDNA_TYPE_MAX = %d,\n", 1 + int(parsed_structs.size()));
  fprintf(file, "};\n\n");
}

/** Write the `dna_struct_ids.cc` file for `sdna_struct_id_get<T>()`. */
static void write_sdna_struct_ids(FILE *file, const Span<dna::ParsedStruct> parsed_structs)
{
  fprintf(file, "#include \"DNA_sdna_type_ids.hh\"\n\n");
  fprintf(file, "namespace blender {\n");
  fprintf(file, "namespace dna {\n\n");
  fprintf(file, "int sdna_struct_id_get_max() { return %d; }\n", int(parsed_structs.size()));
  fprintf(file, "\n}\n");

  for (const int64_t i : parsed_structs.index_range()) {
    const char *name = parsed_structs[i].alias_type_name.c_str();
    const int sdna_index = int(i) + 1;
    fprintf(file, "struct %s;\n", name);
    fprintf(
        file, "template<> int dna::sdna_struct_id_get<%s>() { return %d; }\n", name, sdna_index);
  }

  fprintf(file, "\n}\n");
}

/** Write the `dna_defaults.cc` for RNA to automatically set property defaults. */
static void write_rna_defaults(FILE *file,
                               const StringRefNull base_directory,
                               const Span<dna::ParsedStruct> parsed_structs)
{
  fprintf(file, "/* Default struct member values for RNA. */\n");
  fprintf(file, "#define DNA_DEPRECATED_ALLOW\n");
  fprintf(file, "#define DNA_NO_EXTERNAL_CONSTRUCTORS\n");
  fprintf(file, "#include \"DNA_sdna_type_ids.hh\"\n\n");

  for (const char *filename : includefiles) {
    fprintf(file, "#include \"%s%s\"\n", base_directory.c_str(), filename);
  }

  fprintf(file, "namespace blender {\n\n");

  /* Define an instance of each struct. */
  for (const dna::ParsedStruct &parsed_struct : parsed_structs) {
    const StringRefNull name = parsed_struct.type_name.c_str();
    std::string default_var;

    if (name == "bTheme") {
      /* Exception for bTheme which is auto-generated. */
      fprintf(file, "extern \"C\" const bTheme U_theme_default;\n");
      default_var = "U_theme_default";
    }
    else {
      fprintf(file, "static const %s DNA_DEFAULT_%s = {};\n", name.c_str(), name.c_str());
      default_var = "DNA_DEFAULT_" + name;
    }

    /* Table with pointer to each member. */
    fprintf(file, "static const void *const member_defaults_%s[] = {\n", name.c_str());
    for (const dna::ParsedMember &pm : parsed_struct.members) {
      const StringRef bare_id = DNA_member_id_string_ref(pm.member_name);
      fprintf(file, "  &%s.%.*s,\n", default_var.c_str(), int(bare_id.size()), bare_id.data());
    }
    fprintf(file, "};\n");
  }

  /* Table with all structs. */
  fprintf(file, "\nextern const void *const *const DNA_member_default_table[] = {\n");
  for (const dna::ParsedStruct &parsed_struct : parsed_structs) {
    fprintf(file, "  member_defaults_%s,\n", parsed_struct.type_name.c_str());
  }
  fprintf(file, "};\n\n");

  fprintf(file, "}  // namespace blender\n");
}

/** Write `dna_verify.cc` file to verify `sizeof` and `offsetof` match what we computed. */
static void write_sdna_verify(FILE *file,
                              const TypeTable &table,
                              const Span<dna::ParsedStruct> parsed_structs,
                              const StringRefNull base_directory)
{
  fprintf(file, "/* Verify struct sizes and member offsets are as expected by DNA. */\n");
  fprintf(file, "#include \"BLI_assert.h\"\n\n");
  /* Needed so we can find offsets of deprecated structs. */
  fprintf(file, "#define DNA_DEPRECATED_ALLOW\n");
  /* Workaround enum naming collision in static asserts
   * (ideally this included a unique name/id per file). */
  fprintf(file, "#define assert_line_ assert_line_DNA_\n");
  for (const char *filename : includefiles) {
    fprintf(file, "#include \"%s%s\"\n", base_directory.c_str(), filename);
  }
  fprintf(file, "#undef assert_line_\n");
  fprintf(file, "\n");
  fprintf(file, "using namespace blender;\n");
  fprintf(file, "\n");

  for (const dna::ParsedStruct &parsed_struct : parsed_structs) {
    int offset = 0;
    for (const dna::ParsedMember &parsed_member : parsed_struct.members) {
      const TypeInfo &member_type = table.lookup(parsed_member.type_name);
      const StringRef alias_id = DNA_member_id_string_ref(parsed_member.alias_member_name);
      fprintf(file,
              "BLI_STATIC_ASSERT(offsetof(struct %s, %.*s) == %d, \"DNA member offset "
              "verify\");\n",
              parsed_struct.alias_type_name.c_str(),
              int(alias_id.size()),
              alias_id.data(),
              offset);
      offset += member_size_native(member_type, parsed_member);
    }
    const TypeInfo &struct_info = table.lookup(parsed_struct.type_name);
    fprintf(file,
            "BLI_STATIC_ASSERT(sizeof(struct %s) == %d, \"DNA struct size verify\");\n\n",
            parsed_struct.alias_type_name.c_str(),
            struct_info.size_native);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Renaming
 *
 * Apply struct/member renames from `dna_rename_defs.h`.
 * \{ */

static bool apply_renames(Vector<dna::ParsedStruct> &parsed_structs)
{
  const DnaRenameMaps rename_maps = DNA_rename_maps_alias_to_static();

  for (dna::ParsedStruct &parsed_struct : parsed_structs) {
    /* Save the original C++ name and rewrite. */
    parsed_struct.alias_type_name = parsed_struct.type_name;
    parsed_struct.type_name = rename_maps.types.lookup_default_as(parsed_struct.type_name,
                                                                  parsed_struct.type_name);

    Set<StringRef> members_unique;
    members_unique.reserve(parsed_struct.members.size());

    for (dna::ParsedMember &parsed_member : parsed_struct.members) {
      /* Save the original C++ type and rewrite. */
      parsed_member.alias_member_name = parsed_member.member_name;
      parsed_member.type_name = rename_maps.types.lookup_default_as(parsed_member.type_name,
                                                                    parsed_member.type_name);

      /* Rewrite the member name. */
      std::string &member_name = parsed_member.member_name;
      StringRef stripped = DNA_member_id_string_ref(member_name);
      const StringRefNull *member_static = rename_maps.members.lookup_ptr(
          {parsed_struct.type_name, std::string(stripped)});
      if (member_static != nullptr) {
        const size_t prefix_len = stripped.data() - member_name.data();
        member_name.replace(prefix_len, stripped.size(), member_static->c_str());
        stripped = DNA_member_id_string_ref(member_name);
      }

      /* Sanity check to ensure all member names are still unique. */
      if (!members_unique.add(stripped)) {
        fprintf(stderr,
                "Error: duplicate name found '%s.%.*s', "
                "likely cause is 'dna_rename_defs.h'\n",
                parsed_struct.alias_type_name.c_str(),
                int(stripped.size()),
                stripped.data());
        return false;
      }
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make DNA
 *
 * Parse header files to generate SDNA blob and auxiliary files.
 * \{ */

static bool make_structDNA(const StringRefNull base_directory,
                           FILE *file,
                           FILE *file_offsets,
                           FILE *file_verify,
                           FILE *file_ids,
                           FILE *file_defaults)
{
  if (debugSDNA > 0) {
    fflush(stdout);
    printf("Running makesdna at debug level %d\n", debugSDNA);
  }

  /* Parse structs and enums from all DNA header files. */
  Vector<dna::ParsedStruct> parsed_structs;
  Vector<dna::ParsedEnum> parsed_enums;

  DEBUG_PRINTF(0, "\tStart of header scan:\n");
  if (!dna::parse_dna_headers(base_directory, parsed_structs, parsed_enums, includefiles)) {
    return false;
  }
  DEBUG_PRINTF(0, "\tFinished scanning headers.\n");

  /* Write default values for RNA before any substitution or renaming, as RNA binds
   * to the actual C++ data structures rather than SDNA. */
  write_rna_defaults(file_defaults, base_directory, parsed_structs);

  /* Substitute C++ types with C types known to SDNA. */
  if (!dna::substitute_cpp_types(parsed_structs, parsed_enums, false)) {
    return false;
  }

  /* Apply renames from `dna_rename_defs.h`. */
  if (!apply_renames(parsed_structs)) {
    return false;
  }

  /* Build type table. */
  TypeTable table = build_type_table(parsed_structs);

  /* Compute type sizes and check alignment. */
  if (compute_type_size_and_alignment(table, parsed_structs)) {
    return false;
  }

  /* Write SDNA blob. */
  DEBUG_PRINTF(0, "Writing file ... ");
  write_sdna_blob(file, table, parsed_structs);

  /* Write auxiliary files. */
  write_sdna_type_offsets(file_offsets, parsed_structs);
  write_sdna_struct_ids(file_ids, parsed_structs);
  write_sdna_verify(file_verify, table, parsed_structs, base_directory);

  DEBUG_PRINTF(0, "done.\n");

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Function
 * \{ */

static void make_bad_file(const StringRefNull filename, int line)
{
  FILE *fp = fopen(filename.c_str(), "w");
  fprintf(fp,
          "#error \"Error! can't make correct DNA.c file from %s:%d, check alignment.\"\n",
          __FILE__,
          line);
  fclose(fp);
}

#ifndef BASE_HEADER
#  define BASE_HEADER "../"
#endif

static void print_usage(const char *argv0)
{
  printf(
      "Usage: %s [--include-file <file>, ...] "
      "dna.cc dna_type_offsets.h dna_verify.cc dna_struct_ids.cc dna_defaults.cc "
      "[base directory]\n",
      argv0);
}

}  // namespace blender

int main(int argc, char **argv)
{
  using namespace blender;
  Vector<const char *> cli_include_files;

  /* There is a number of non-optional arguments that must be provided to the executable. */
  if (argc < 6) {
    print_usage(argv[0]);
    return 1;
  }

  /* Parse optional arguments. */
  int arg_index = 1; /* Skip the argv0. */
  while (arg_index < argc) {
    if (STREQ(argv[arg_index], "--include-file")) {
      ++arg_index;
      if (arg_index == argc) {
        printf("Missing argument for --include-file\n");
        print_usage(argv[0]);
        return 1;
      }
      cli_include_files.append(argv[arg_index]);
      ++arg_index;
      continue;
    }
    break;
  }

  if (!cli_include_files.is_empty()) {
    includefiles = Span<const char *>(cli_include_files.data(), cli_include_files.size());
  }

  /* Check the number of non-optional positional arguments. */
  const int num_arguments = argc - arg_index;
  if (!ELEM(num_arguments, 5, 6)) {
    print_usage(argv[0]);
    return 0;
  }

  int return_status = 0;

  FILE *file_dna = fopen(argv[arg_index], "w");
  FILE *file_dna_offsets = fopen(argv[arg_index + 1], "w");
  FILE *file_dna_verify = fopen(argv[arg_index + 2], "w");
  FILE *file_dna_ids = fopen(argv[arg_index + 3], "w");
  FILE *file_dna_defaults = fopen(argv[arg_index + 4], "w");
  if (!file_dna) {
    printf("Unable to open file: %s\n", argv[arg_index]);
    return_status = 1;
  }
  else if (!file_dna_offsets) {
    printf("Unable to open file: %s\n", argv[arg_index + 1]);
    return_status = 1;
  }
  else if (!file_dna_verify) {
    printf("Unable to open file: %s\n", argv[arg_index + 2]);
    return_status = 1;
  }
  else if (!file_dna_ids) {
    printf("Unable to open file: %s\n", argv[arg_index + 3]);
    return_status = 1;
  }
  else if (!file_dna_defaults) {
    printf("Unable to open file: %s\n", argv[arg_index + 4]);
    return_status = 1;
  }
  else {
    const char *base_directory;

    if (num_arguments == 6) {
      base_directory = argv[arg_index + 5];
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

    if (!make_structDNA(base_directory,
                        file_dna,
                        file_dna_offsets,
                        file_dna_verify,
                        file_dna_ids,
                        file_dna_defaults))
    {
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
  if (file_dna_ids) {
    fclose(file_dna_ids);
  }
  if (file_dna_defaults) {
    fclose(file_dna_defaults);
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
  using namespace blender;
#define DNA_STRUCT_RENAME(old, new) (void)sizeof(new);
#define DNA_STRUCT_RENAME_MEMBER(new_struct_name, old, new) (void)offsetof(new_struct_name, new);
#include "dna_rename_defs.h"

#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_MEMBER
}

/** \} */
