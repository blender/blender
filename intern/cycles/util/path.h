/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_PATH_H__
#define __UTIL_PATH_H__

/* Utility functions to get paths to files distributed with the program. For
 * the standalone apps, paths are relative to the executable, for dynamically
 * linked libraries, the path to the library may be set with path_init, which
 * then makes all paths relative to that. */

#include <stdio.h>

#include "util/set.h"
#include "util/string.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* program paths */
void path_init(const string &path = "", const string &user_path = "");
string path_get(const string &sub = "");
string path_user_get(const string &sub = "");
string path_cache_get(const string &sub = "");

/* path string manipulation */
string path_filename(const string &path);
string path_dirname(const string &path);
string path_join(const string &dir, const string &file);
string path_escape(const string &path);
bool path_is_relative(const string &path);

/* file info */
size_t path_file_size(const string &path);
bool path_exists(const string &path);
bool path_is_directory(const string &path);
string path_files_md5_hash(const string &dir);
uint64_t path_modified_time(const string &path);

/* directory utility */
void path_create_directories(const string &path);

/* file read/write utilities */
FILE *path_fopen(const string &path, const string &mode);

bool path_write_binary(const string &path, const vector<uint8_t> &binary);
bool path_write_text(const string &path, string &text);
bool path_read_binary(const string &path, vector<uint8_t> &binary);
bool path_read_text(const string &path, string &text);

/* File manipulation. */
bool path_remove(const string &path);

/* source code utility */
string path_source_replace_includes(const string &source, const string &path);

/* Simple least-recently-used cache for kernels.
 *
 * Kernels of same type are cached in the same directory.
 * Whenever a kernel is used, its last modified time is updated.
 * When a new kernel is added to the cache, clear old entries of the same type (i.e. in the same
 * directory). */
bool path_cache_kernel_exists_and_mark_used(const string &path);
void path_cache_kernel_mark_added_and_clear_old(const string &path,
                                                const size_t max_old_kernel_of_same_type = 5);

CCL_NAMESPACE_END

#endif
