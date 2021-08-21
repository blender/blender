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
 * \ingroup bli
 * \brief File and directory operations.
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

/* for size_t (needed on windows) */
#include <stddef.h>

#include <limits.h> /* for PATH_MAX */

#include "BLI_compiler_attrs.h"
#include "BLI_fileops_types.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

/* Common */

int BLI_exists(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_copy(const char *file, const char *to) ATTR_NONNULL();
int BLI_rename(const char *from, const char *to) ATTR_NONNULL();
int BLI_delete(const char *file, bool dir, bool recursive) ATTR_NONNULL();
int BLI_delete_soft(const char *file, const char **error_message) ATTR_NONNULL();
#if 0 /* Unused */
int BLI_move(const char *path, const char *to) ATTR_NONNULL();
int BLI_create_symlink(const char *path, const char *to) ATTR_NONNULL();
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

int BLI_fstat(int fd, BLI_stat_t *buffer) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_stat(const char *path, BLI_stat_t *buffer) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int64_t BLI_ftell(FILE *stream) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_fseek(FILE *stream, int64_t offset, int whence);
int64_t BLI_lseek(int fd, int64_t offset, int whence);

#ifdef WIN32
int BLI_wstat(const wchar_t *path, BLI_stat_t *buffer);
#endif

typedef enum eFileAttributes {
  FILE_ATTR_READONLY = 1 << 0,        /* Read-only or Immutable. */
  FILE_ATTR_HIDDEN = 1 << 1,          /* Hidden or invisible. */
  FILE_ATTR_SYSTEM = 1 << 2,          /* Used by the Operating System. */
  FILE_ATTR_ARCHIVE = 1 << 3,         /* Marked as archived. */
  FILE_ATTR_COMPRESSED = 1 << 4,      /* Compressed. */
  FILE_ATTR_ENCRYPTED = 1 << 5,       /* Encrypted. */
  FILE_ATTR_RESTRICTED = 1 << 6,      /* Protected by OS. */
  FILE_ATTR_TEMPORARY = 1 << 7,       /* Used for temporary storage. */
  FILE_ATTR_SPARSE_FILE = 1 << 8,     /* Sparse File. */
  FILE_ATTR_OFFLINE = 1 << 9,         /* Contents available after a short delay. */
  FILE_ATTR_ALIAS = 1 << 10,          /* Mac Alias or Windows LNK. File-based redirection. */
  FILE_ATTR_REPARSE_POINT = 1 << 11,  /* File has associated re-parse point. */
  FILE_ATTR_SYMLINK = 1 << 12,        /* Reference to another file. */
  FILE_ATTR_JUNCTION_POINT = 1 << 13, /* Folder Symbolic-link. */
  FILE_ATTR_MOUNT_POINT = 1 << 14,    /* Volume mounted as a folder. */
  FILE_ATTR_HARDLINK = 1 << 15,       /* Duplicated directory entry. */
} eFileAttributes;

#define FILE_ATTR_ANY_LINK \
  (FILE_ATTR_ALIAS | FILE_ATTR_REPARSE_POINT | FILE_ATTR_SYMLINK | FILE_ATTR_JUNCTION_POINT | \
   FILE_ATTR_MOUNT_POINT | FILE_ATTR_HARDLINK)

/* Directories */

struct direntry;

bool BLI_is_dir(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BLI_is_file(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BLI_dir_create_recursive(const char *dir) ATTR_NONNULL();
double BLI_dir_free_space(const char *dir) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_current_working_dir(char *dir, const size_t maxncpy) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
eFileAttributes BLI_file_attributes(const char *path);

/* Filelist */

unsigned int BLI_filelist_dir_contents(const char *dir, struct direntry **r_filelist);
void BLI_filelist_entry_duplicate(struct direntry *dst, const struct direntry *src);
void BLI_filelist_duplicate(struct direntry **dest_filelist,
                            struct direntry *const src_filelist,
                            const unsigned int nrentries);
void BLI_filelist_entry_free(struct direntry *entry);
void BLI_filelist_free(struct direntry *filelist, const unsigned int nrentries);

void BLI_filelist_entry_size_to_string(const struct stat *st,
                                       const uint64_t sz,
                                       const bool compact,
                                       char r_size[FILELIST_DIRENTRY_SIZE_LEN]);
void BLI_filelist_entry_mode_to_string(const struct stat *st,
                                       const bool compact,
                                       char r_mode1[FILELIST_DIRENTRY_MODE_LEN],
                                       char r_mode2[FILELIST_DIRENTRY_MODE_LEN],
                                       char r_mode3[FILELIST_DIRENTRY_MODE_LEN]);
void BLI_filelist_entry_owner_to_string(const struct stat *st,
                                        const bool compact,
                                        char r_owner[FILELIST_DIRENTRY_OWNER_LEN]);
void BLI_filelist_entry_datetime_to_string(const struct stat *st,
                                           const int64_t ts,
                                           const bool compact,
                                           char r_time[FILELIST_DIRENTRY_TIME_LEN],
                                           char r_date[FILELIST_DIRENTRY_DATE_LEN],
                                           bool *r_is_today,
                                           bool *r_is_yesterday);

/* Files */

FILE *BLI_fopen(const char *filename, const char *mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void *BLI_gzopen(const char *filename, const char *mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_open(const char *filename, int oflag, int pmode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_access(const char *filename, int mode) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

bool BLI_file_is_writable(const char *file) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BLI_file_touch(const char *file) ATTR_NONNULL();
bool BLI_file_alias_target(const char *filepath, char *r_targetpath) ATTR_WARN_UNUSED_RESULT;

bool BLI_file_magic_is_gzip(const char header[4]);

size_t BLI_file_zstd_from_mem_at_pos(void *buf,
                                     size_t len,
                                     FILE *file,
                                     size_t file_offset,
                                     int compression_level) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
size_t BLI_file_unzstd_to_mem_at_pos(void *buf, size_t len, FILE *file, size_t file_offset)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BLI_file_magic_is_zstd(const char header[4]);

size_t BLI_file_descriptor_size(int file) ATTR_WARN_UNUSED_RESULT;
size_t BLI_file_size(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* compare if one was last modified before the other */
bool BLI_file_older(const char *file1, const char *file2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* read ascii file as lines, empty list if reading fails */
struct LinkNode *BLI_file_read_as_lines(const char *file) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void *BLI_file_read_text_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size);
void *BLI_file_read_text_as_mem_with_newline_as_nil(const char *filepath,
                                                    bool trim_trailing_space,
                                                    size_t pad_bytes,
                                                    size_t *r_size);
void *BLI_file_read_binary_as_mem(const char *filepath, size_t pad_bytes, size_t *r_size);
void BLI_file_free_lines(struct LinkNode *lines);

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
