/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_PATH_H__
#define __UTIL_PATH_H__

/* Utility functions to get paths to files distributed with the program. For
 * the standalone apps, paths are relative to the executable, for dynamically
 * linked libraries, the path to the library may be set with path_init, which
 * then makes all paths relative to that. */

#include <stdio.h>

#include "util/util_set.h"
#include "util/util_string.h"
#include "util/util_types.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* program paths */
void path_init(const string& path = "", const string& user_path = "");
string path_get(const string& sub = "");
string path_user_get(const string& sub = "");
string path_cache_get(const string& sub = "");

/* path string manipulation */
string path_filename(const string& path);
string path_dirname(const string& path);
string path_join(const string& dir, const string& file);
string path_escape(const string& path);
bool path_is_relative(const string& path);

/* file info */
size_t path_file_size(const string& path);
bool path_exists(const string& path);
bool path_is_directory(const string& path);
string path_files_md5_hash(const string& dir);
uint64_t path_modified_time(const string& path);

/* directory utility */
void path_create_directories(const string& path);

/* file read/write utilities */
FILE *path_fopen(const string& path, const string& mode);

bool path_write_binary(const string& path, const vector<uint8_t>& binary);
bool path_write_text(const string& path, string& text);
bool path_read_binary(const string& path, vector<uint8_t>& binary);
bool path_read_text(const string& path, string& text);

/* File manipulation. */
bool path_remove(const string& path);

/* source code utility */
string path_source_replace_includes(const string& source,
                                    const string& path,
                                    const string& source_filename="");

/* cache utility */
void path_cache_clear_except(const string& name, const set<string>& except);

CCL_NAMESPACE_END

#endif
