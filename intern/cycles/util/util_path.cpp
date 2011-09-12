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

#include "util_debug.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_string.h"

#include <OpenImageIO/sysutil.h>
OIIO_NAMESPACE_USING

#include <stdio.h>

#define BOOST_FILESYSTEM_VERSION 2

#include <boost/filesystem.hpp> 
#include <boost/algorithm/string.hpp>

CCL_NAMESPACE_BEGIN

static string cached_path = "";
static string cached_user_path = "";

void path_init(const string& path, const string& user_path)
{
	cached_path = path;
	cached_user_path = user_path;
}

string path_get(const string& sub)
{
	if(cached_path == "")
		cached_path = path_dirname(Sysutil::this_program_path());

	return path_join(cached_path, sub);
}

string path_user_get(const string& sub)
{
	if(cached_user_path == "")
		cached_user_path = path_dirname(Sysutil::this_program_path());

	return path_join(cached_user_path, sub);
}

string path_filename(const string& path)
{
	return boost::filesystem::path(path).filename();
}

string path_dirname(const string& path)
{
	return boost::filesystem::path(path).parent_path().string();
}

string path_join(const string& dir, const string& file)
{
	return (boost::filesystem::path(dir) / boost::filesystem::path(file)).string();
}

string path_escape(const string& path)
{
	string result = path;
	boost::replace_all(result, " ", "\\ ");
	return result;
}

bool path_exists(const string& path)
{
	return boost::filesystem::exists(path);
}

string path_files_md5_hash(const string& dir)
{
	/* computes md5 hash of all files in the directory */
	MD5Hash hash;

	if(boost::filesystem::exists(dir)) {
		boost::filesystem::directory_iterator it(dir), it_end;

		for(; it != it_end; it++) {
			if(boost::filesystem::is_directory(it->status())) {
				path_files_md5_hash(it->path().string());
			}
			else {
				string filepath = it->path().string();

				hash.append((const uint8_t*)filepath.c_str(), filepath.size());
				hash.append_file(filepath);
			}
		}
	}

	return hash.get_hex();
}

void path_create_directories(const string& path)
{
	boost::filesystem::create_directories(path_dirname(path));
}

bool path_write_binary(const string& path, const vector<uint8_t>& binary)
{
	path_create_directories(path);

	/* write binary file from memory */
	FILE *f = fopen(path.c_str(), "wb");

	if(!f)
		return false;

	if(binary.size() > 0)
		fwrite(&binary[0], sizeof(uint8_t), binary.size(), f);

	fclose(f);

	return true;
}

bool path_read_binary(const string& path, vector<uint8_t>& binary)
{
	binary.resize(boost::filesystem::file_size(path));

	/* read binary file into memory */
	FILE *f = fopen(path.c_str(), "rb");

	if(!f)
		return false;

	if(binary.size() == 0) {
		fclose(f);
		return false;
	}

	if(fread(&binary[0], sizeof(uint8_t), binary.size(), f) != binary.size()) {
		fclose(f);
		return false;
	}

	fclose(f);

	return true;
}

CCL_NAMESPACE_END

