/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BLI_PATH_UTIL_H__
#define __BLI_PATH_UTIL_H__

/** \file BLI_path_util.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct ListBase;
struct direntry;

#ifdef WIN32
#define SEP '\\'
#define ALTSEP '/'
#else
#define SEP '/'
#define ALTSEP '\\'
#endif

void BLI_setenv(const char *env, const char *val) ATTR_NONNULL(1);
void BLI_setenv_if_new(const char *env, const char *val) ATTR_NONNULL(1);

void BLI_make_file_string(const char *relabase, char *string,  const char *dir, const char *file);
void BLI_make_exist(char *dir);
void BLI_make_existing_file(const char *name);
void BLI_split_dirfile(const char *string, char *dir, char *file, const size_t dirlen, const size_t filelen);
void BLI_split_dir_part(const char *string, char *dir, const size_t dirlen);
void BLI_split_file_part(const char *string, char *file, const size_t filelen);
void BLI_path_append(char *__restrict dst, const size_t maxlen,
                     const char *__restrict file) ATTR_NONNULL();
void BLI_join_dirfile(char *__restrict string, const size_t maxlen,
                      const char *__restrict dir, const char *__restrict file) ATTR_NONNULL();
const char *BLI_path_basename(const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

#if 0
typedef enum bli_rebase_state {
	BLI_REBASE_NO_SRCDIR = 0,
	BLI_REBASE_OK        = 1,
	BLI_REBASE_IDENTITY  = 2
} bli_rebase_state;

int BLI_rebase_path(char *abs, size_t abs_len, char *rel, size_t rel_len, const char *base_dir, const char *src_dir, const char *dest_dir);
#endif

const char *BLI_last_slash(const char *string) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
int         BLI_add_slash(char *string) ATTR_NONNULL();
void        BLI_del_slash(char *string) ATTR_NONNULL();
const char *BLI_first_slash(const char *string) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
void        BLI_path_native_slash(char *path) ATTR_NONNULL();

void BLI_getlastdir(const char *dir, char *last, const size_t maxlen);
bool BLI_testextensie(const char *str, const char *ext) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_testextensie_n(const char *str, ...) ATTR_NONNULL(1) ATTR_SENTINEL(0);
bool BLI_testextensie_array(const char *str, const char **ext_array) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_testextensie_glob(const char *str, const char *ext_fnmatch) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_replace_extension(char *path, size_t maxlen, const char *ext) ATTR_NONNULL();
bool BLI_ensure_extension(char *path, size_t maxlen, const char *ext) ATTR_NONNULL();
bool BLI_ensure_filename(char *filepath, size_t maxlen, const char *filename) ATTR_NONNULL();
void BLI_uniquename(struct ListBase *list, void *vlink, const char *defname, char delim, int name_offs, int len);
bool BLI_uniquename_cb(bool (*unique_check)(void *arg, const char *name),
                       void *arg, const char *defname, char delim, char *name, int name_len);
void BLI_newname(char *name, int add);
int BLI_stringdec(const char *string, char *head, char *start, unsigned short *numlen);
void BLI_stringenc(char *string, const char *head, const char *tail, unsigned short numlen, int pic);
int BLI_split_name_num(char *left, int *nr, const char *name, const char delim);

/**
 * dir can be any input, like from buttons, and this function
 * converts it to a regular full path.
 * Also removes garbage from directory paths, like /../ or double slashes etc 
 */

/* removes trailing slash */
void BLI_cleanup_file(const char *relabase, char *path) ATTR_NONNULL(2);
/* same as above but adds a trailing slash */
void BLI_cleanup_dir(const char *relabase, char *dir) ATTR_NONNULL(2);
/* doesn't touch trailing slash */
void BLI_cleanup_path(const char *relabase, char *path) ATTR_NONNULL(2);

/* go back one directory */
bool BLI_parent_dir(char *path) ATTR_NONNULL();

/**
 * Blender's path code replacement function.
 * Bases \a path strings leading with "//" by the
 * directory \a basepath, and replaces instances of
 * '#' with the \a framenum. Results are written
 * back into \a path.
 * 
 * \a path The path to convert
 * \a basepath The directory to base relative paths with.
 * \a framenum The framenumber to replace the frame code with.
 * \retval Returns true if the path was relative (started with "//").
 */
bool BLI_path_abs(char *path, const char *basepath)  ATTR_NONNULL();
bool BLI_path_frame(char *path, int frame, int digits) ATTR_NONNULL();
bool BLI_path_frame_range(char *path, int sta, int end, int digits) ATTR_NONNULL();
bool BLI_path_frame_check_chars(const char *path) ATTR_NONNULL();
bool BLI_path_cwd(char *path) ATTR_NONNULL();
void BLI_path_rel(char *file, const char *relfile) ATTR_NONNULL();

bool BLI_path_is_rel(const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
bool BLI_path_is_unc(const char *path);

#if defined(WIN32)
void BLI_cleanup_unc_16(wchar_t *path_16);
void BLI_cleanup_unc(char *path_16, int maxlen);
#endif

bool BLI_path_suffix(char *string, size_t maxlen, const char *suffix, const char *sep) ATTR_NONNULL();

/* path string comparisons: case-insensitive for Windows, case-sensitive otherwise */
#if defined(WIN32)
#  define BLI_path_cmp BLI_strcasecmp
#  define BLI_path_ncmp BLI_strncasecmp
#else
#  define BLI_path_cmp strcmp
#  define BLI_path_ncmp strncmp
#endif

void BLI_char_switch(char *string, char from, char to) ATTR_NONNULL();

#ifdef WITH_ICONV
void BLI_string_to_utf8(char *original, char *utf_8, const char *code);
#endif

/* these values need to be hardcoded in structs, dna does not recognize defines */
/* also defined in DNA_space_types.h */
#ifndef FILE_MAXDIR
#  define FILE_MAXDIR         768
#  define FILE_MAXFILE        256
#  define FILE_MAX            1024
#endif

#ifdef __cplusplus
}
#endif

#endif

