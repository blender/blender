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
#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void BLI_setenv(const char *env, const char *val) ATTR_NONNULL(1);
void BLI_setenv_if_new(const char *env, const char *val) ATTR_NONNULL(1);
const char *BLI_getenv(const char *env) ATTR_NONNULL(1);

void BLI_make_file_string(const char *relabase, char *string, const char *dir, const char *file);
bool BLI_make_existing_file(const char *name);
void BLI_split_dirfile(
    const char *string, char *dir, char *file, const size_t dirlen, const size_t filelen);
void BLI_split_dir_part(const char *string, char *dir, const size_t dirlen);
void BLI_split_file_part(const char *string, char *file, const size_t filelen);
const char *BLI_path_extension(const char *filepath) ATTR_NONNULL();

void BLI_path_append(char *__restrict dst, const size_t maxlen, const char *__restrict file)
    ATTR_NONNULL();
void BLI_join_dirfile(char *__restrict dst,
                      const size_t maxlen,
                      const char *__restrict dir,
                      const char *__restrict file) ATTR_NONNULL();
size_t BLI_path_join(char *__restrict dst, const size_t dst_len, const char *path_first, ...)
    ATTR_NONNULL(1, 3) ATTR_SENTINEL(0);
const char *BLI_path_basename(const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_name_at_index(const char *__restrict path,
                            const int index,
                            int *__restrict r_offset,
                            int *__restrict r_len) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

/** Return true only if #containee_path is contained in #container_path. */
bool BLI_path_contains(const char *container_path,
                       const char *containee_path) ATTR_WARN_UNUSED_RESULT;

const char *BLI_path_slash_rfind(const char *string) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
int BLI_path_slash_ensure(char *string) ATTR_NONNULL();
void BLI_path_slash_rstrip(char *string) ATTR_NONNULL();
const char *BLI_path_slash_find(const char *string) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void BLI_path_slash_native(char *path) ATTR_NONNULL();

#ifdef _WIN32
bool BLI_path_program_extensions_add_win32(char *name, const size_t maxlen);
#endif
bool BLI_path_program_search(char *fullname, const size_t maxlen, const char *name);

bool BLI_path_extension_check(const char *str, const char *ext)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_extension_check_n(const char *str, ...) ATTR_NONNULL(1) ATTR_SENTINEL(0);
bool BLI_path_extension_check_array(const char *str, const char **ext_array)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_extension_check_glob(const char *str, const char *ext_fnmatch)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_extension_glob_validate(char *ext_fnmatch) ATTR_NONNULL();
bool BLI_path_extension_replace(char *path, size_t maxlen, const char *ext) ATTR_NONNULL();
bool BLI_path_extension_ensure(char *path, size_t maxlen, const char *ext) ATTR_NONNULL();
bool BLI_path_filename_ensure(char *filepath, size_t maxlen, const char *filename) ATTR_NONNULL();
int BLI_path_sequence_decode(const char *string,
                             char *head,
                             char *tail,
                             unsigned short *r_num_len);
void BLI_path_sequence_encode(
    char *string, const char *head, const char *tail, unsigned short numlen, int pic);

void BLI_path_normalize(const char *relabase, char *path) ATTR_NONNULL(2);
/* Same as above but adds a trailing slash. */
void BLI_path_normalize_dir(const char *relabase, char *dir) ATTR_NONNULL(2);

bool BLI_filename_make_safe(char *fname) ATTR_NONNULL(1);
bool BLI_path_make_safe(char *path) ATTR_NONNULL(1);

/* Go back one directory. */
bool BLI_path_parent_dir(char *path) ATTR_NONNULL();
/* Go back until the directory is found. */
bool BLI_path_parent_dir_until_exists(char *path) ATTR_NONNULL();

bool BLI_path_abs(char *path, const char *basepath) ATTR_NONNULL();
bool BLI_path_frame(char *path, int frame, int digits) ATTR_NONNULL();
bool BLI_path_frame_range(char *path, int sta, int end, int digits) ATTR_NONNULL();
bool BLI_path_frame_get(char *path, int *r_frame, int *numdigits) ATTR_NONNULL();
void BLI_path_frame_strip(char *path, char *ext) ATTR_NONNULL();
bool BLI_path_frame_check_chars(const char *path) ATTR_NONNULL();
bool BLI_path_abs_from_cwd(char *path, const size_t maxlen) ATTR_NONNULL();
void BLI_path_rel(char *file, const char *relfile) ATTR_NONNULL();

bool BLI_path_is_rel(const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_is_unc(const char *path);

void BLI_path_to_display_name(char *display_name, int maxlen, const char *name) ATTR_NONNULL();

#if defined(WIN32)
void BLI_path_normalize_unc_16(wchar_t *path_16);
void BLI_path_normalize_unc(char *path_16, int maxlen);
#endif

bool BLI_path_suffix(char *string, size_t maxlen, const char *suffix, const char *sep)
    ATTR_NONNULL();

/* path string comparisons: case-insensitive for Windows, case-sensitive otherwise */
#if defined(WIN32)
#  define BLI_path_cmp BLI_strcasecmp
#  define BLI_path_ncmp BLI_strncasecmp
#else
#  define BLI_path_cmp strcmp
#  define BLI_path_ncmp strncmp
#endif

/* these values need to be hardcoded in structs, dna does not recognize defines */
/* also defined in DNA_space_types.h */
#ifndef FILE_MAXDIR
#  define FILE_MAXDIR 768
#  define FILE_MAXFILE 256
#  define FILE_MAX 1024
#endif

#ifdef WIN32
#  define SEP '\\'
#  define ALTSEP '/'
#  define SEP_STR "\\"
#  define ALTSEP_STR "/"
#else
#  define SEP '/'
#  define ALTSEP '\\'
#  define SEP_STR "/"
#  define ALTSEP_STR "\\"
#endif

/* Parent and current dir helpers. */
#define FILENAME_PARENT ".."
#define FILENAME_CURRENT "."

/* Avoid calling strcmp on one or two chars! */
#define FILENAME_IS_PARENT(_n) (((_n)[0] == '.') && ((_n)[1] == '.') && ((_n)[2] == '\0'))
#define FILENAME_IS_CURRENT(_n) (((_n)[0] == '.') && ((_n)[1] == '\0'))
#define FILENAME_IS_CURRPAR(_n) \
  (((_n)[0] == '.') && (((_n)[1] == '\0') || (((_n)[1] == '.') && ((_n)[2] == '\0'))))

#ifdef __cplusplus
}
#endif
