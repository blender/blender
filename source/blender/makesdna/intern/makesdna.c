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
 *
 * \brief Struct muncher for making SDNA.
 *
 * \section aboutmakesdnac About makesdna tool
 * Originally by Ton, some mods by Frank, and some cleaning and
 * extension by Nzc.
 *
 * Makesdna creates a .c file with a long string of numbers that
 * encode the Blender file format. It is fast, because it is basically
 * a binary dump. There are some details to mind when reconstructing
 * the file (endianness and byte-alignment).
 *
 * This little program scans all structs that need to be serialized,
 * and determined the names and types of all members. It calculates
 * how much memory (on disk or in ram) is needed to store that struct,
 * and the offsets for reaching a particular one.
 *
 * There is a facility to get verbose output from sdna. Search for
 * \ref debugSDNA. This int can be set to 0 (no output) to some int. Higher
 * numbers give more output.
 */

#define DNA_DEPRECATED_ALLOW

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_sys_types.h" /* for intptr_t support */

#include "dna_utils.h"

#define SDNA_MAX_FILENAME_LENGTH 255

/* Included the path relative from /source/blender/ here, so we can move     */
/* headers around with more freedom.                                         */
static const char *includefiles[] = {
    /* if you add files here, please add them at the end
     * of makesdna.c (this file) as well */
    "DNA_listBase.h",
    "DNA_vec_types.h",
    "DNA_ID.h",
    "DNA_ipo_types.h",
    "DNA_key_types.h",
    "DNA_text_types.h",
    "DNA_packedFile_types.h",
    "DNA_gpu_types.h",
    "DNA_camera_types.h",
    "DNA_image_types.h",
    "DNA_texture_types.h",
    "DNA_light_types.h",
    "DNA_material_types.h",
    "DNA_vfont_types.h",
    "DNA_meta_types.h",
    "DNA_curve_types.h",
    "DNA_mesh_types.h",
    "DNA_meshdata_types.h",
    "DNA_modifier_types.h",
    "DNA_lattice_types.h",
    "DNA_object_types.h",
    "DNA_object_force_types.h",
    "DNA_object_fluidsim_types.h",
    "DNA_world_types.h",
    "DNA_scene_types.h",
    "DNA_view3d_types.h",
    "DNA_view2d_types.h",
    "DNA_space_types.h",
    "DNA_userdef_types.h",
    "DNA_screen_types.h",
    "DNA_sdna_types.h",
    "DNA_fileglobal_types.h",
    "DNA_sequence_types.h",
    "DNA_effect_types.h",
    "DNA_outliner_types.h",
    "DNA_sound_types.h",
    "DNA_collection_types.h",
    "DNA_armature_types.h",
    "DNA_action_types.h",
    "DNA_constraint_types.h",
    "DNA_nla_types.h",
    "DNA_node_types.h",
    "DNA_color_types.h",
    "DNA_brush_types.h",
    "DNA_customdata_types.h",
    "DNA_particle_types.h",
    "DNA_cloth_types.h",
    "DNA_gpencil_types.h",
    "DNA_gpencil_modifier_types.h",
    "DNA_shader_fx_types.h",
    "DNA_windowmanager_types.h",
    "DNA_anim_types.h",
    "DNA_boid_types.h",
    "DNA_smoke_types.h",
    "DNA_speaker_types.h",
    "DNA_movieclip_types.h",
    "DNA_tracking_types.h",
    "DNA_dynamicpaint_types.h",
    "DNA_mask_types.h",
    "DNA_rigidbody_types.h",
    "DNA_freestyle_types.h",
    "DNA_linestyle_types.h",
    "DNA_cachefile_types.h",
    "DNA_layer_types.h",
    "DNA_workspace_types.h",
    "DNA_lightprobe_types.h",

    /* see comment above before editing! */

    /* empty string to indicate end of includefiles */
    "",
};

/* -------------------------------------------------------------------- */
/** \name Variables
 * \{ */

static MemArena *mem_arena = NULL;

static int max_data_size = 500000, max_array_len = 50000;
static int names_len = 0;
static int types_len = 0;
static int structs_len = 0;
/** At address `names[a]` is string `a`. */
static char **names;
/** At address `types[a]` is string `a`. */
static char **types;
/** At `types_size[a]` is the size of type `a` on this systems bitness (32 or 64). */
static short *types_size_native;
/** Contains sizes as they are calculated on 32 bit systems. */
static short *types_size_32;
/** Contains sizes as they are calculated on 64 bit systems. */
static short *types_size_64;
/** At `sp = structs[a]` is the first address of a struct definition:
 * - `sp[0]` is type number.
 * - `sp[1]` is the length of the element array (next).
 * - `sp[2]` sp[3] is [(type_index, name_index), ..] (number of pairs is defined by `sp[1]`),
 */
static short **structs, *structdata;

/** Versioning data */
static struct {
  GHash *struct_map_alias_from_static;
  GHash *struct_map_static_from_alias;
  GHash *elem_map_alias_from_static;
  GHash *elem_map_static_from_alias;
} g_version_data = {NULL};

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
 * Ensure type \c str to is in the #types array.
 * \param str: Struct name without any qualifiers.
 * \param len: The struct size in bytes.
 * \return Index in the #types array.
 */
static int add_type(const char *str, int size);

/**
 * Ensure \c str is int the #names array.
 * \param str: Struct member name which may include pointer prefix & array size.
 * \return Index in the #names array.
 */
static int add_name(const char *str);

/**
 * Search whether this structure type was already found, and if not,
 * add it.
 */
static short *add_struct(int namecode);

/**
 * Remove comments from this buffer. Assumes that the buffer refers to
 * ascii-code text.
 */
static int preprocess_include(char *maindata, const int maindata_len);

/**
 * Scan this file for serializable types.
 */
static int convert_include(const char *filename);

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
void print_struct_sizes(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implementation
 *
 * Make DNA string (write to file).
 * \{ */

static const char *version_struct_static_from_alias(const char *str)
{
  const char *str_test = BLI_ghash_lookup(g_version_data.struct_map_static_from_alias, str);
  if (str_test != NULL) {
    return str_test;
  }
  return str;
}

static const char *version_struct_alias_from_static(const char *str)
{
  const char *str_test = BLI_ghash_lookup(g_version_data.struct_map_alias_from_static, str);
  if (str_test != NULL) {
    return str_test;
  }
  return str;
}

static const char *version_elem_static_from_alias(const int strct, const char *elem_alias_full)
{
  const uint elem_alias_full_len = strlen(elem_alias_full);
  char *elem_alias = alloca(elem_alias_full_len + 1);
  const int elem_alias_len = DNA_elem_id_strip_copy(elem_alias, elem_alias_full);
  const char *str_pair[2] = {types[strct], elem_alias};
  const char *elem_static = BLI_ghash_lookup(g_version_data.elem_map_static_from_alias, str_pair);
  if (elem_static != NULL) {
    return DNA_elem_id_rename(mem_arena,
                              elem_alias,
                              elem_alias_len,
                              elem_static,
                              strlen(elem_static),
                              elem_alias_full,
                              elem_alias_full_len,
                              DNA_elem_id_offset_start(elem_alias_full));
  }
  return elem_alias_full;
}

/**
 * Enforce '_pad123' naming convention, disallow 'pad123' or 'pad_123',
 * special exception for [a-z] after since there is a 'pad_rot_angle' preference.
 */
static bool is_name_legal(const char *name)
{
  const int name_size = strlen(name) + 1;
  char *name_strip = alloca(name_size);
  DNA_elem_id_strip_copy(name_strip, name);

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

static int add_type(const char *str, int size)
{
  char *cp;

  /* first do validity check */
  if (str[0] == 0) {
    return -1;
  }
  else if (strchr(str, '*')) {
    /* note: this is valid C syntax but we can't parse, complain!
     * 'struct SomeStruct* somevar;' <-- correct but we cant handle right now. */
    return -1;
  }

  str = version_struct_static_from_alias(str);

  /* search through type array */
  for (int index = 0; index < types_len; index++) {
    if (strcmp(str, types[index]) == 0) {
      if (size) {
        types_size_native[index] = size;
        types_size_32[index] = size;
        types_size_64[index] = size;
      }
      return index;
    }
  }

  /* append new type */
  const int str_size = strlen(str) + 1;
  cp = BLI_memarena_alloc(mem_arena, str_size);
  memcpy(cp, str, str_size);
  types[types_len] = cp;
  types_size_native[types_len] = size;
  types_size_32[types_len] = size;
  types_size_64[types_len] = size;

  if (types_len >= max_array_len) {
    printf("too many types\n");
    return types_len - 1;
  }
  types_len++;

  return types_len - 1;
}

/**
 *
 * Because of the weird way of tokenizing, we have to 'cast' function
 * pointers to ... (*f)(), whatever the original signature. In fact,
 * we add name and type at the same time... There are two special
 * cases, unfortunately. These are explicitly checked.
 *
 * */
static int add_name(const char *str)
{
  int nr, i, j, k;
  char *cp;
  char buf[255]; /* stupid limit, change it :) */
  const char *name;

  additional_slen_offset = 0;

  if (str[0] == 0 /*  || (str[1] == 0) */) {
    return -1;
  }

  if (str[0] == '(' && str[1] == '*') {
    /* we handle function pointer and special array cases here, e.g.
     * void (*function)(...) and float (*array)[..]. the array case
     * name is still converted to (array *)() though because it is that
     * way in old dna too, and works correct with elementsize() */
    int isfuncptr = (strchr(str + 1, '(')) != NULL;

    DEBUG_PRINTF(3, "\t\t\t\t*** Function pointer or multidim array pointer found\n");
    /* functionpointer: transform the type (sometimes) */
    i = 0;

    while (str[i] != ')') {
      buf[i] = str[i];
      i++;
    }

    /* Another number we need is the extra slen offset. This extra
     * offset is the overshoot after a space. If there is no
     * space, no overshoot should be calculated. */
    j = i; /* j at first closing brace */

    DEBUG_PRINTF(3, "first brace after offset %d\n", i);

    j++; /* j beyond closing brace ? */
    while ((str[j] != 0) && (str[j] != ')')) {
      DEBUG_PRINTF(3, "seen %c (%d)\n", str[j], str[j]);
      j++;
    }
    DEBUG_PRINTF(3,
                 "seen %c (%d)\n"
                 "special after offset%d\n",
                 str[j],
                 str[j],
                 j);

    if (!isfuncptr) {
      /* multidimensional array pointer case */
      if (str[j] == 0) {
        DEBUG_PRINTF(3, "offsetting for multidim array pointer\n");
      }
      else {
        printf("Error during tokening multidim array pointer\n");
      }
    }
    else if (str[j] == 0) {
      DEBUG_PRINTF(3, "offsetting for space\n");
      /* get additional offset */
      k = 0;
      while (str[j] != ')') {
        j++;
        k++;
      }
      DEBUG_PRINTF(3, "extra offset %d\n", k);
      additional_slen_offset = k;
    }
    else if (str[j] == ')') {
      DEBUG_PRINTF(3, "offsetting for brace\n");
      /* don't get extra offset */
    }
    else {
      printf("Error during tokening function pointer argument list\n");
    }

    /*
     * Put )(void) at the end? Maybe )(). Should check this with
     * old sdna. Actually, sometimes )(), sometimes )(void...)
     * Alas.. such is the nature of brain-damage :(
     *
     * Sorted it out: always do )(), except for headdraw and
     * windraw, part of ScrArea. This is important, because some
     * linkers will treat different fp's differently when called
     * !!! This has to do with interference in byte-alignment and
     * the way args are pushed on the stack.
     *
     * */
    buf[i] = 0;
    DEBUG_PRINTF(3, "Name before chomping: %s\n", buf);
    if ((strncmp(buf, "(*headdraw", 10) == 0) || (strncmp(buf, "(*windraw", 9) == 0)) {
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
    /* now precede with buf*/
    DEBUG_PRINTF(3, "\t\t\t\t\tProposing fp name %s\n", buf);
    name = buf;
  }
  else {
    /* normal field: old code */
    name = str;
  }

  /* search name array */
  for (nr = 0; nr < names_len; nr++) {
    if (strcmp(name, names[nr]) == 0) {
      return nr;
    }
  }

  /* Sanity check the name. */
  if (!is_name_legal(name)) {
    return -1;
  }

  /* Append new name. */
  const int name_size = strlen(name) + 1;
  cp = BLI_memarena_alloc(mem_arena, name_size);
  memcpy(cp, name, name_size);
  names[names_len] = cp;

  if (names_len >= max_array_len) {
    printf("too many names\n");
    return names_len - 1;
  }
  names_len++;

  return names_len - 1;
}

static short *add_struct(int namecode)
{
  int len;
  short *sp;

  if (structs_len == 0) {
    structs[0] = structdata;
  }
  else {
    sp = structs[structs_len - 1];
    len = sp[1];
    structs[structs_len] = sp + 2 * len + 2;
  }

  sp = structs[structs_len];
  sp[0] = namecode;

  if (structs_len >= max_array_len) {
    printf("too many structs\n");
    return sp;
  }
  structs_len++;

  return sp;
}

static int preprocess_include(char *maindata, const int maindata_len)
{
  int a, newlen, comment = 0;
  char *cp, *temp, *md;

  /* note: len + 1, last character is a dummy to prevent
   * comparisons using uninitialized memory */
  temp = MEM_mallocN(maindata_len + 1, "preprocess_include");
  temp[maindata_len] = ' ';

  memcpy(temp, maindata, maindata_len);

  /* remove all c++ comments */
  /* replace all enters/tabs/etc with spaces */
  cp = temp;
  a = maindata_len;
  comment = 0;
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

  /* data from temp copy to maindata, remove comments and double spaces */
  cp = temp;
  md = maindata;
  newlen = 0;
  comment = 0;
  a = maindata_len;
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
    else if (strncmp("DNA_DEPRECATED", cp, 14) == 0) {
      /* single values are skipped already, so decrement 1 less */
      a -= 13;
      cp += 13;
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

static void *read_file_data(const char *filename, int *r_len)
{
#ifdef WIN32
  FILE *fp = fopen(filename, "rb");
#else
  FILE *fp = fopen(filename, "r");
#endif
  void *data;

  if (!fp) {
    *r_len = -1;
    return NULL;
  }

  fseek(fp, 0L, SEEK_END);
  *r_len = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  if (*r_len == -1) {
    fclose(fp);
    return NULL;
  }

  data = MEM_mallocN(*r_len, "read_file_data");
  if (!data) {
    *r_len = -1;
    fclose(fp);
    return NULL;
  }

  if (fread(data, *r_len, 1, fp) != 1) {
    *r_len = -1;
    MEM_freeN(data);
    fclose(fp);
    return NULL;
  }

  fclose(fp);
  return data;
}

static int convert_include(const char *filename)
{
  /* read include file, skip structs with a '#' before it.
   * store all data in temporal arrays.
   */
  int maindata_len, count, slen, type, name, strct;
  short *structpoin, *sp;
  char *maindata, *mainend, *md, *md1;
  bool skip_struct;

  md = maindata = read_file_data(filename, &maindata_len);
  if (maindata_len == -1) {
    fprintf(stderr, "Can't read file %s\n", filename);
    return 1;
  }

  maindata_len = preprocess_include(maindata, maindata_len);
  mainend = maindata + maindata_len - 1;

  /* we look for '{' and then back to 'struct' */
  count = 0;
  skip_struct = false;
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
        md1 = md - 2;
        while (*md1 != 32) {
          /* to beginning of word */
          md1--;
        }
        md1++;

        /* we've got a struct name when... */
        if (strncmp(md1 - 7, "struct", 6) == 0) {

          strct = add_type(md1, 0);
          if (strct == -1) {
            fprintf(stderr, "File '%s' contains struct we cant parse \"%s\"\n", filename, md1);
            return 1;
          }

          structpoin = add_struct(strct);
          sp = structpoin + 2;

          DEBUG_PRINTF(1, "\t|\t|-- detected struct %s\n", types[strct]);

          /* first lets make it all nice strings */
          md1 = md + 1;
          while (*md1 != '}') {
            if (md1 > mainend) {
              break;
            }

            if (*md1 == ',' || *md1 == ' ') {
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
              if (strncmp(md1, "struct", 6) == 0) {
                md1 += 7;
              }
              if (strncmp(md1, "unsigned", 8) == 0) {
                md1 += 9;
              }
              if (strncmp(md1, "const", 5) == 0) {
                md1 += 6;
              }

              /* we've got a type! */
              type = add_type(md1, 0);
              if (type == -1) {
                fprintf(
                    stderr, "File '%s' contains struct we can't parse \"%s\"\n", filename, md1);
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
                  slen = (int)strlen(md1);
                  if (md1[slen - 1] == ';') {
                    md1[slen - 1] = 0;

                    name = add_name(version_elem_static_from_alias(strct, md1));
                    if (name == -1) {
                      fprintf(stderr,
                              "File '%s' contains struct with name that can't be added \"%s\"\n",
                              filename,
                              md1);
                      return 1;
                    }
                    slen += additional_slen_offset;
                    sp[0] = type;
                    sp[1] = name;

                    if (names[name] != NULL) {
                      DEBUG_PRINTF(1, "%s |", names[name]);
                    }

                    structpoin[1]++;
                    sp += 2;

                    md1 += slen;
                    break;
                  }

                  name = add_name(version_elem_static_from_alias(strct, md1));
                  if (name == -1) {
                    fprintf(stderr,
                            "File '%s' contains struct with name that can't be added \"%s\"\n",
                            filename,
                            md1);
                    return 1;
                  }
                  slen += additional_slen_offset;

                  sp[0] = type;
                  sp[1] = name;
                  if (names[name] != NULL) {
                    DEBUG_PRINTF(1, "%s ||", names[name]);
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

static bool check_field_alignment(
    int firststruct, int structtype, int type, int len, const char *name, const char *detail)
{
  bool result = true;
  if (type < firststruct && types_size_native[type] > 4 && (len % 8)) {
    fprintf(stderr,
            "Align 8 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[structtype],
            name,
            len % 8);
    result = false;
  }
  if (types_size_native[type] > 3 && (len % 4)) {
    fprintf(stderr,
            "Align 4 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[structtype],
            name,
            len % 4);
    result = false;
  }
  if (types_size_native[type] == 2 && (len % 2)) {
    fprintf(stderr,
            "Align 2 error (%s) in struct: %s %s (add %d padding bytes)\n",
            detail,
            types[structtype],
            name,
            len % 2);
    result = false;
  }
  return result;
}

static int calculate_struct_sizes(int firststruct, FILE *file_verify, const char *base_directory)
{
  int unknown = structs_len, lastunknown;
  bool dna_error = false;

  /* Write test to verify sizes are accurate. */
  fprintf(file_verify, "/* Verify struct sizes and member offsets are as expected by DNA. */\n");
  fprintf(file_verify, "#include \"BLI_assert.h\"\n\n");
  fprintf(file_verify, "#define DNA_DEPRECATED\n");
  for (int i = 0; *(includefiles[i]) != '\0'; i++) {
    fprintf(file_verify, "#include \"%s%s\"\n", base_directory, includefiles[i]);
  }
  fprintf(file_verify, "\n");

  /* Multiple iterations to handle nested structs. */
  while (unknown) {
    lastunknown = unknown;
    unknown = 0;

    /* check all structs... */
    for (int a = 0; a < structs_len; a++) {
      const short *structpoin = structs[a];
      const int structtype = structpoin[0];
      const char *structname = version_struct_alias_from_static(types[structtype]);

      /* when length is not known... */
      if (types_size_native[structtype] == 0) {

        const short *sp = structpoin + 2;
        int size_native = 0;
        int size_32 = 0;
        int size_64 = 0;
        bool has_pointer = false;

        /* check all elements in struct */
        for (int b = 0; b < structpoin[1]; b++, sp += 2) {
          int type = sp[0];
          const char *cp = names[sp[1]];
          int namelen = (int)strlen(cp);

          /* Write size verification to file. */
          {
            char *name_static = alloca(namelen + 1);
            DNA_elem_id_strip_copy(name_static, cp);
            const char *str_pair[2] = {types[structtype], name_static};
            const char *name_alias = BLI_ghash_lookup(g_version_data.elem_map_alias_from_static,
                                                      str_pair);
            fprintf(file_verify,
                    "BLI_STATIC_ASSERT(offsetof(struct %s, %s) == %d, \"DNA member offset "
                    "verify\");\n",
                    structname,
                    name_alias ? name_alias : name_static,
                    size_native);
          }

          /* is it a pointer or function pointer? */
          if (cp[0] == '*' || cp[1] == '*') {
            has_pointer = 1;
            /* has the name an extra length? (array) */
            int mul = 1;
            if (cp[namelen - 1] == ']') {
              mul = DNA_elem_array_size(cp);
            }

            if (mul == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      types[structtype],
                      namelen + 1,
                      cp);
              dna_error = 1;
            }

            /* 4-8 aligned/ */
            if (sizeof(void *) == 4) {
              if (size_native % 4) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 4): %s %s\n",
                        types[structtype],
                        cp);
                dna_error = 1;
              }
            }
            else {
              if (size_native % 8) {
                fprintf(stderr,
                        "Align pointer error in struct (size_native 8): %s %s\n",
                        types[structtype],
                        cp);
                dna_error = 1;
              }
            }

            if (size_64 % 8) {
              fprintf(stderr,
                      "Align pointer error in struct (size_64 8): %s %s\n",
                      types[structtype],
                      cp);
              dna_error = 1;
            }

            size_native += sizeof(void *) * mul;
            size_32 += 4 * mul;
            size_64 += 8 * mul;
          }
          else if (cp[0] == '[') {
            /* parsing can cause names "var" and "[3]"
             * to be found for "float var [3]" */
            fprintf(stderr,
                    "Parse error in struct, invalid member name: %s %s\n",
                    types[structtype],
                    cp);
            dna_error = 1;
          }
          else if (types_size_native[type]) {
            /* has the name an extra length? (array) */
            int mul = 1;
            if (cp[namelen - 1] == ']') {
              mul = DNA_elem_array_size(cp);
            }

            if (mul == 0) {
              fprintf(stderr,
                      "Zero array size found or could not parse %s: '%.*s'\n",
                      types[structtype],
                      namelen + 1,
                      cp);
              dna_error = 1;
            }

            /* struct alignment */
            if (type >= firststruct) {
              if (sizeof(void *) == 8 && (size_native % 8)) {
                fprintf(stderr, "Align struct error: %s %s\n", types[structtype], cp);
                dna_error = 1;
              }
            }

            /* Check 2-4-8 aligned. */
            if (!check_field_alignment(firststruct, structtype, type, size_32, cp, "32 bit")) {
              dna_error = 1;
            }
            if (!check_field_alignment(firststruct, structtype, type, size_64, cp, "64 bit")) {
              dna_error = 1;
            }

            size_native += mul * types_size_native[type];
            size_32 += mul * types_size_32[type];
            size_64 += mul * types_size_64[type];
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
          types_size_native[structtype] = size_native;
          types_size_32[structtype] = size_32;
          types_size_64[structtype] = size_64;
          /* two ways to detect if a struct contains a pointer:
           * has_pointer is set or size_native  doesn't match any of 32/64bit lengths*/
          if (has_pointer || size_64 != size_native || size_32 != size_native) {
            if (size_64 % 8) {
              fprintf(stderr,
                      "Sizeerror 8 in struct: %s (add %d bytes)\n",
                      types[structtype],
                      size_64 % 8);
              dna_error = 1;
            }
          }

          if (size_native % 4) {
            fprintf(stderr,
                    "Sizeerror 4 in struct: %s (add %d bytes)\n",
                    types[structtype],
                    size_native % 4);
            dna_error = 1;
          }

          /* Write size verification to file. */
          fprintf(file_verify,
                  "BLI_STATIC_ASSERT(sizeof(struct %s) == %d, \"DNA struct size verify\");\n\n",
                  structname,
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

      for (int a = 0; a < structs_len; a++) {
        const short *structpoin = structs[a];
        const int structtype = structpoin[0];

        /* length unknown */
        if (types_size_native[structtype] != 0) {
          fprintf(stderr, "  %s\n", types[structtype]);
        }
      }
    }

    fprintf(stderr, "*** Unknown structs :\n");

    for (int a = 0; a < structs_len; a++) {
      const short *structpoin = structs[a];
      const int structtype = structpoin[0];

      /* length unknown yet */
      if (types_size_native[structtype] == 0) {
        fprintf(stderr, "  %s\n", types[structtype]);
      }
    }

    dna_error = 1;
  }

  return dna_error;
}

#define MAX_DNA_LINE_LENGTH 20

static void dna_write(FILE *file, const void *pntr, const int size)
{
  static int linelength = 0;
  int i;
  const char *data;

  data = (const char *)pntr;

  for (i = 0; i < size; i++) {
    fprintf(file, "%d, ", data[i]);
    linelength++;
    if (linelength >= MAX_DNA_LINE_LENGTH) {
      fprintf(file, "\n");
      linelength = 0;
    }
  }
}

void print_struct_sizes(void)
{
  int a, unknown = structs_len, structtype;
  /*int lastunknown;*/ /*UNUSED*/
  const short *structpoin;
  printf("\n\n*** All detected structs:\n");

  while (unknown) {
    /*lastunknown = unknown;*/ /*UNUSED*/
    unknown = 0;

    /* check all structs... */
    for (a = 0; a < structs_len; a++) {
      structpoin = structs[a];
      structtype = structpoin[0];
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
  int i;
  const short *sp;
  /* str contains filenames. Since we now include paths, I stretched       */
  /* it a bit. Hope this is enough :) -nzc-                                */
  char str[SDNA_MAX_FILENAME_LENGTH];
  int firststruct;

  if (debugSDNA > 0) {
    fflush(stdout);
    printf("Running makesdna at debug level %d\n", debugSDNA);
  }

  mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  /* the longest known struct is 50k, so we assume 100k is sufficient! */
  structdata = MEM_callocN(max_data_size, "structdata");

  /* a maximum of 5000 variables, must be sufficient? */
  names = MEM_callocN(sizeof(char *) * max_array_len, "names");
  types = MEM_callocN(sizeof(char *) * max_array_len, "types");
  types_size_native = MEM_callocN(sizeof(short) * max_array_len, "types_size_native");
  types_size_32 = MEM_callocN(sizeof(short) * max_array_len, "types_size_32");
  types_size_64 = MEM_callocN(sizeof(short) * max_array_len, "types_size_64");
  structs = MEM_callocN(sizeof(short *) * max_array_len, "structs");

  /* Build versioning data */
  DNA_alias_maps(DNA_RENAME_ALIAS_FROM_STATIC,
                 &g_version_data.struct_map_alias_from_static,
                 &g_version_data.elem_map_alias_from_static);
  DNA_alias_maps(DNA_RENAME_STATIC_FROM_ALIAS,
                 &g_version_data.struct_map_static_from_alias,
                 &g_version_data.elem_map_static_from_alias);

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

  /* note, long isn't supported,
   * these are place-holders to maintain alignment with eSDNA_Type*/
  add_type("long", 4);  /* SDNA_TYPE_LONG */
  add_type("ulong", 4); /* SDNA_TYPE_ULONG */

  add_type("float", 4);    /* SDNA_TYPE_FLOAT */
  add_type("double", 8);   /* SDNA_TYPE_DOUBLE */
  add_type("int64_t", 8);  /* SDNA_TYPE_INT64 */
  add_type("uint64_t", 8); /* SDNA_TYPE_UINT64 */
  add_type("void", 0);     /* SDNA_TYPE_VOID */

  /* the defines above shouldn't be output in the padding file... */
  firststruct = types_len;

  /* add all include files defined in the global array                     */
  /* Since the internal file+path name buffer has limited length, I do a   */
  /* little test first...                                                  */
  /* Mind the breaking condition here!                                     */
  DEBUG_PRINTF(0, "\tStart of header scan:\n");
  for (i = 0; *(includefiles[i]) != '\0'; i++) {
    sprintf(str, "%s%s", base_directory, includefiles[i]);
    DEBUG_PRINTF(0, "\t|-- Converting %s\n", str);
    if (convert_include(str)) {
      return 1;
    }
  }
  DEBUG_PRINTF(0, "\tFinished scanning %d headers.\n", i);

  if (calculate_struct_sizes(firststruct, file_verify, base_directory)) {
    /* error */
    return 1;
  }

  /* FOR DEBUG */
  if (debugSDNA > 1) {
    int a, b;
    /* short *elem; */
    short num_types;

    printf("names_len %d types_len %d structs_len %d\n", names_len, types_len, structs_len);
    for (a = 0; a < names_len; a++) {
      printf(" %s\n", names[a]);
    }
    printf("\n");

    sp = types_size_native;
    for (a = 0; a < types_len; a++, sp++) {
      printf(" %s %d\n", types[a], *sp);
    }
    printf("\n");

    for (a = 0; a < structs_len; a++) {
      sp = structs[a];
      printf(" struct %s elems: %d size: %d\n", types[sp[0]], sp[1], types_size_native[sp[0]]);
      num_types = sp[1];
      sp += 2;
      /* ? num_types was elem? */
      for (b = 0; b < num_types; b++, sp += 2) {
        printf("   %s %s\n", types[sp[0]], names[sp[1]]);
      }
    }
  }

  /* file writing */

  DEBUG_PRINTF(0, "Writing file ... ");

  if (names_len == 0 || structs_len == 0) {
    /* pass */
  }
  else {
    const char nil_bytes[4] = {0};
    int len, len_align;

    dna_write(file, "SDNA", 4);

    /* write names */
    dna_write(file, "NAME", 4);
    len = names_len;
    dna_write(file, &len, 4);
    /* write array */
    len = 0;
    for (int nr = 0; nr < names_len; nr++) {
      int name_size = strlen(names[nr]) + 1;
      dna_write(file, names[nr], name_size);
      len += name_size;
    }
    len_align = (len + 3) & ~3;
    if (len != len_align) {
      dna_write(file, nil_bytes, len_align - len);
    }

    /* write TYPES */
    dna_write(file, "TYPE", 4);
    len = types_len;
    dna_write(file, &len, 4);
    /* write array */
    len = 0;
    for (int nr = 0; nr < types_len; nr++) {
      int type_size = strlen(types[nr]) + 1;
      dna_write(file, types[nr], type_size);
      len += type_size;
    }
    len_align = (len + 3) & ~3;
    if (len != len_align) {
      dna_write(file, nil_bytes, len_align - len);
    }

    /* WRITE TYPELENGTHS */
    dna_write(file, "TLEN", 4);

    len = 2 * types_len;
    if (types_len & 1) {
      len += 2;
    }
    dna_write(file, types_size_native, len);

    /* WRITE STRUCTS */
    dna_write(file, "STRC", 4);
    len = structs_len;
    dna_write(file, &len, 4);

    /* calc datablock size */
    sp = structs[structs_len - 1];
    sp += 2 + 2 * (sp[1]);
    len = (intptr_t)((char *)sp - (char *)structs[0]);
    len = (len + 3) & ~3;

    dna_write(file, structs[0], len);
  }

  /* write a simple enum with all structs offsets,
   * should only be accessed via SDNA_TYPE_FROM_STRUCT macro */
  {
    fprintf(file_offsets, "#define SDNA_TYPE_FROM_STRUCT(id) _SDNA_TYPE_##id\n");
    fprintf(file_offsets, "enum {\n");
    for (i = 0; i < structs_len; i++) {
      const short *structpoin = structs[i];
      const int structtype = structpoin[0];
      fprintf(file_offsets,
              "\t_SDNA_TYPE_%s = %d,\n",
              version_struct_alias_from_static(types[structtype]),
              i);
    }
    fprintf(file_offsets, "\tSDNA_TYPE_MAX = %d,\n", structs_len);
    fprintf(file_offsets, "};\n\n");
  }

  /* Check versioning errors which could cause duplicate names,
   * do last because names are stripped. */
  {
    GSet *names_unique = BLI_gset_str_new_ex(__func__, 512);
    for (int struct_nr = 0; struct_nr < structs_len; struct_nr++) {
      sp = structs[struct_nr];
      const char *struct_name = types[sp[0]];
      const int len = sp[1];
      sp += 2;
      for (int a = 0; a < len; a++, sp += 2) {
        char *name = names[sp[1]];
        DNA_elem_id_strip(name);
        if (!BLI_gset_add(names_unique, name)) {
          fprintf(stderr,
                  "Error: duplicate name found '%s.%s', "
                  "likely cause is 'dna_rename_defs.h'\n",
                  struct_name,
                  name);
          return 1;
        }
      }
      BLI_gset_clear(names_unique, NULL);
    }
    BLI_gset_free(names_unique, NULL);
  }

  MEM_freeN(structdata);
  MEM_freeN(names);
  MEM_freeN(types);
  MEM_freeN(types_size_native);
  MEM_freeN(types_size_32);
  MEM_freeN(types_size_64);
  MEM_freeN(structs);

  BLI_memarena_free(mem_arena);

  BLI_ghash_free(g_version_data.struct_map_alias_from_static, NULL, NULL);
  BLI_ghash_free(g_version_data.struct_map_static_from_alias, NULL, NULL);
  BLI_ghash_free(g_version_data.elem_map_static_from_alias, MEM_freeN, NULL);
  BLI_ghash_free(g_version_data.elem_map_alias_from_static, MEM_freeN, NULL);

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

  if (argc != 4 && argc != 5) {
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

      fprintf(file_dna, "extern const unsigned char DNAstr[];\n");
      fprintf(file_dna, "const unsigned char DNAstr[] = {\n");
      if (make_structDNA(base_directory, file_dna, file_dna_offsets, file_dna_verify)) {
        /* error */
        fclose(file_dna);
        file_dna = NULL;
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

/* even though DNA supports, 'long' shouldn't be used since it can be either 32 or 64bit,
 * use int or int64_t instead.
 * Only valid use would be as a runtime variable if an API expected a long,
 * but so far we dont have this happening. */
#ifdef __GNUC__
#  pragma GCC poison long
#endif

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_text_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_texture_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_vfont_types.h"
#include "DNA_meta_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_view2d_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_sequence_types.h"
#include "DNA_effect_types.h"
#include "DNA_outliner_types.h"
#include "DNA_sound_types.h"
#include "DNA_collection_types.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_nla_types.h"
#include "DNA_node_types.h"
#include "DNA_color_types.h"
#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_particle_types.h"
#include "DNA_cloth_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_anim_types.h"
#include "DNA_boid_types.h"
#include "DNA_smoke_types.h"
#include "DNA_speaker_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_mask_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_layer_types.h"
#include "DNA_workspace_types.h"
#include "DNA_lightprobe_types.h"

/* end of list */

/** \} */

/* -------------------------------------------------------------------- */
/** \name DNA Renaming Sanity Check
 *
 * Without this it's possible to reference struct members that don't exist,
 * breaking backward & forward compatibility.
 *
 * \{ */

static void UNUSED_FUNCTION(dna_rename_defs_ensure)(void)
{
#define DNA_STRUCT_RENAME(old, new) (void)sizeof(new);
#define DNA_STRUCT_RENAME_ELEM(struct_name, old, new) (void)offsetof(struct_name, new);
#include "dna_rename_defs.h"
#undef DNA_STRUCT_RENAME
#undef DNA_STRUCT_RENAME_ELEM
}

/** \} */
