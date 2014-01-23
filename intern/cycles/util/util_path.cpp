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
 * limitations under the License
 */

#include "util_debug.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_string.h"

#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
OIIO_NAMESPACE_USING

#include <stdio.h>

#include <boost/version.hpp>

#if (BOOST_VERSION < 104400)
#  define BOOST_FILESYSTEM_VERSION 2
#endif

#include <boost/filesystem.hpp> 
#include <boost/algorithm/string.hpp>

CCL_NAMESPACE_BEGIN

static string cached_path = "";
static string cached_user_path = "";

static boost::filesystem::path to_boost(const string& path)
{
#ifdef _MSC_VER
	std::wstring path_utf16 = Strutil::utf8_to_utf16(path.c_str());
	return boost::filesystem::path(path_utf16.c_str());
#else
	return boost::filesystem::path(path.c_str());
#endif
}

static string from_boost(const boost::filesystem::path& path)
{
#ifdef _MSC_VER
	return Strutil::utf16_to_utf8(path.wstring().c_str());
#else
	return path.string().c_str();
#endif
}

void path_init(const string& path, const string& user_path)
{
	cached_path = path;
	cached_user_path = user_path;

#ifdef _MSC_VER
	// fix for https://svn.boost.org/trac/boost/ticket/6320
	boost::filesystem::path::imbue( std::locale( "" ) );
#endif
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
#if (BOOST_FILESYSTEM_VERSION == 2)
	return to_boost(path).filename();
#else
	return from_boost(to_boost(path).filename());
#endif
}

string path_dirname(const string& path)
{
	return from_boost(to_boost(path).parent_path());
}

string path_join(const string& dir, const string& file)
{
	return from_boost((to_boost(dir) / to_boost(file)));
}

string path_escape(const string& path)
{
	string result = path;
	boost::replace_all(result, " ", "\\ ");
	return result;
}

bool path_exists(const string& path)
{
	return boost::filesystem::exists(to_boost(path));
}

static void path_files_md5_hash_recursive(MD5Hash& hash, const string& dir)
{
	boost::filesystem::path dirpath = to_boost(dir);

	if(boost::filesystem::exists(dirpath)) {
		boost::filesystem::directory_iterator it(dirpath), it_end;

		for(; it != it_end; it++) {
			if(boost::filesystem::is_directory(it->status())) {
				path_files_md5_hash_recursive(hash, from_boost(it->path()));
			}
			else {
				string filepath = from_boost(it->path());

				hash.append((const uint8_t*)filepath.c_str(), filepath.size());
				hash.append_file(filepath);
			}
		}
	}
}

string path_files_md5_hash(const string& dir)
{
	/* computes md5 hash of all files in the directory */
	MD5Hash hash;

	path_files_md5_hash_recursive(hash, dir);

	return hash.get_hex();
}

void path_create_directories(const string& path)
{
	boost::filesystem::create_directories(to_boost(path_dirname(path)));
}

bool path_write_binary(const string& path, const vector<uint8_t>& binary)
{
	path_create_directories(path);

	/* write binary file from memory */
	FILE *f = path_fopen(path, "wb");

	if(!f)
		return false;

	if(binary.size() > 0)
		fwrite(&binary[0], sizeof(uint8_t), binary.size(), f);

	fclose(f);

	return true;
}

bool path_write_text(const string& path, string& text)
{
	vector<uint8_t> binary(text.length(), 0);
	std::copy(text.begin(), text.end(), binary.begin());

	return path_write_binary(path, binary);
}

bool path_read_binary(const string& path, vector<uint8_t>& binary)
{
	binary.resize(boost::filesystem::file_size(to_boost(path)));

	/* read binary file into memory */
	FILE *f = path_fopen(path, "rb");

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

bool path_read_text(const string& path, string& text)
{
	vector<uint8_t> binary;

	if(!path_exists(path) || !path_read_binary(path, binary))
		return false;

	const char *str = (const char*)&binary[0];
	size_t size = binary.size();
	text = string(str, size);

	return true;
}

uint64_t path_modified_time(const string& path)
{
	if(boost::filesystem::exists(to_boost(path)))
		return (uint64_t)boost::filesystem::last_write_time(to_boost(path));
	
	return 0;
}

string path_source_replace_includes(const string& source_, const string& path)
{
	/* our own little c preprocessor that replaces #includes with the file
	 * contents, to work around issue of opencl drivers not supporting
	 * include paths with spaces in them */
	string source = source_;
	const string include = "#include \"";
	size_t n, pos = 0;

	while((n = source.find(include, pos)) != string::npos) {
		size_t n_start = n + include.size();
		size_t n_end = source.find("\"", n_start);
		string filename = source.substr(n_start, n_end - n_start);

		string text, filepath = path_join(path, filename);

		if(path_read_text(filepath, text)) {
			text = path_source_replace_includes(text, path_dirname(filepath));
			source.replace(n, n_end + 1 - n, "\n" + text + "\n");
		}
		else
			pos = n_end;
	}

	return source;
}

FILE *path_fopen(const string& path, const string& mode)
{
#ifdef _WIN32
	std::wstring path_utf16 = Strutil::utf8_to_utf16(path);
	std::wstring mode_utf16 = Strutil::utf8_to_utf16(mode);

	return _wfopen(path_utf16.c_str(), mode_utf16.c_str());
#else
	return fopen(path.c_str(), mode.c_str());
#endif
}

void path_cache_clear_except(const string& name, const set<string>& except)
{
	string dir = path_user_get("cache");

	if(boost::filesystem::exists(dir)) {
		boost::filesystem::directory_iterator it(dir), it_end;

		for(; it != it_end; it++) {
#if (BOOST_FILESYSTEM_VERSION == 2)
			string filename = from_boost(it->path().filename());
#else
			string filename = from_boost(it->path().filename().string());
#endif

			if(boost::starts_with(filename, name))
				if(except.find(filename) == except.end())
					boost::filesystem::remove(to_boost(filename));
		}
	}

}

CCL_NAMESPACE_END

