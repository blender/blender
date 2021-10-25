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

/** \file BLI_fileops.h
 *  \ingroup bli
 *  \brief File and directory operations.
 * */

#ifndef __BLI_FILEOPS_H__
#define __BLI_FILEOPS_H__

#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* for size_t (needed on windows) */
#include <stddef.h>

#include <limits.h>  /* for PATH_MAX */

#include "BLI_compiler_attrs.h"

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif


/* Common */

int    BLI_exists(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int    BLI_copy(const char *path, const char *to) ATTR_NONNULL();
int    BLI_rename(const char *from, const char *to) ATTR_NONNULL();
int    BLI_delete(const char *path, bool dir, bool recursive) ATTR_NONNULL();
#if 0  /* Unused */
int    BLI_move(const char *path, const char *to) ATTR_NONNULL();
int    BLI_create_symlink(const char *path, const char *to) ATTR_NONNULL();
#endif

/* keep in sync with the definition of struct direntry in BLI_fileops_types.h */
#ifdef WIN32
#  if defined(_MSC_VER)
typedef struct _stat64 BLI_stat_t;
#  else
typedef struct _stat BLI_stat_t;
#  endif
#else
typedef struct stat BLI_stat_t;
#endif

int    BLI_stat(const char *path, BLI_stat_t *buffer) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
#ifdef WIN32
int    BLI_wstat(const wchar_t *path, BLI_stat_t *buffer);
#endif

/* Directories */

struct direntry;

bool   BLI_is_dir(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool   BLI_is_file(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool   BLI_dir_create_recursive(const char *dir) ATTR_NONNULL();
double BLI_dir_free_space(const char *dir) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char  *BLI_current_working_dir(char *dir, const size_t maxlen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* Filelist */

unsigned int BLI_filelist_dir_contents(const char *dir, struct direntry **r_filelist);
void BLI_filelist_entry_duplicate(struct direntry *dst, const struct direntry *src);
void BLI_filelist_duplicate(
        struct direntry **dest_filelist, struct direntry * const src_filelist, const unsigned int nrentries);
void BLI_filelist_entry_free(struct direntry *entry);
void BLI_filelist_free(struct direntry *filelist, const unsigned int nrentries);

void BLI_filelist_entry_size_to_string(const struct stat *st, const uint64_t sz, const bool compact, char r_size[]);
void BLI_filelist_entry_mode_to_string(
        const struct stat *st, const bool compact, char r_mode1[], char r_mode2[], char r_mode3[]);
void BLI_filelist_entry_owner_to_string(const struct stat *st, const bool compact, char r_owner[]);
void BLI_filelist_entry_datetime_to_string(
        const struct stat *st, const int64_t ts, const bool compact, char r_time[], char r_date[]);

/* Files */

FILE  *BLI_fopen(const char *filename, const char *mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void  *BLI_gzopen(const char *filename, const char *mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int    BLI_open(const char *filename, int oflag, int pmode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int    BLI_access(const char *filename, int mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool   BLI_file_is_writable(const char *file) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool   BLI_file_touch(const char *file) ATTR_NONNULL();

#if 0  /* UNUSED */
int    BLI_file_gzip(const char *from, const char *to) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
#endif
char  *BLI_file_ungzip_to_mem(const char *from_file, int *r_size) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

size_t BLI_file_descriptor_size(int file) ATTR_WARN_UNUSED_RESULT;
size_t BLI_file_size(const char *file) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* compare if one was last modified before the other */
bool   BLI_file_older(const char *file1, const char *file2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* read ascii file as lines, empty list if reading fails */
struct LinkNode *BLI_file_read_as_lines(const char *file) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void  *BLI_file_read_text_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size);
void  *BLI_file_read_binary_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size);
void   BLI_file_free_lines(struct LinkNode *lines);

/* this weirdo pops up in two places ... */
#if !defined(WIN32)
#  ifndef O_BINARY
#    define O_BINARY 0
#  endif
#else
void BLI_get_short_name(char short_name[256], const char *filename);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_FILEOPS_H__ */
