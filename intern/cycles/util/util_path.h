/*
 * Copyright 2011, Blender Foundation.
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
 */

#ifndef __UTIL_PATH_H__
#define __UTIL_PATH_H__

/* Utility functions to get paths to files distributed with the program. For
 * the standalone apps, paths are relative to the executable, for dynamically
 * linked libraries, the path to the library may be set with path_init, which
 * then makes all paths relative to that. */

#include "util_string.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

void path_init(const string& path = "", const string& user_path = "");
string path_get(const string& sub = "");
string path_user_get(const string& sub = "");

string path_filename(const string& path);
string path_dirname(const string& path);
string path_join(const string& dir, const string& file);

string path_escape(const string& path);
bool path_exists(const string& path);
string path_files_md5_hash(const string& dir);

void path_create_directories(const string& path);
bool path_write_binary(const string& path, const vector<uint8_t>& binary);
bool path_read_binary(const string& path, vector<uint8_t>& binary);

string path_source_replace_includes(const string& source, const string& path);

CCL_NAMESPACE_END

#endif

