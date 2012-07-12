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

#include <stdio.h>

#include "util_cache.h"
#include "util_debug.h"
#include "util_foreach.h"
#include "util_map.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_types.h"

#include <boost/filesystem.hpp> 
#include <boost/algorithm/string.hpp>

CCL_NAMESPACE_BEGIN

/* CacheData */

CacheData::CacheData(const string& name_)
{
	name = name_;
	f = NULL;
	have_filename = false;
}

CacheData::~CacheData()
{
	if(f)
		fclose(f);
}

const string& CacheData::get_filename()
{
	if(!have_filename) {
		MD5Hash hash;

		foreach(const CacheBuffer& buffer, buffers)
			if(buffer.size)
				hash.append((uint8_t*)buffer.data, buffer.size);
		
		filename = name + "_" + hash.get_hex();
		have_filename = true;
	}

	return filename;
}

/* Cache */

Cache Cache::global;

string Cache::data_filename(CacheData& key)
{
	return path_user_get(path_join("cache", key.get_filename()));
}

void Cache::insert(CacheData& key, CacheData& value)
{
	string filename = data_filename(key);
	path_create_directories(filename);
	FILE *f = fopen(filename.c_str(), "wb");

	if(!f) {
		fprintf(stderr, "Failed to open file %s for writing.\n", filename.c_str());
		return;
	}

	foreach(CacheBuffer& buffer, value.buffers) {
		if(!fwrite(&buffer.size, sizeof(buffer.size), 1, f))
			fprintf(stderr, "Failed to write to file %s.\n", filename.c_str());
		if(buffer.size)
			if(!fwrite(buffer.data, buffer.size, 1, f))
				fprintf(stderr, "Failed to write to file %s.\n", filename.c_str());
	}
	
	fclose(f);
}

bool Cache::lookup(CacheData& key, CacheData& value)
{
	string filename = data_filename(key);
	FILE *f = fopen(filename.c_str(), "rb");

	if(!f)
		return false;
	
	value.name = key.name;
	value.f = f;

	return true;
}

void Cache::clear_except(const string& name, const set<string>& except)
{
	string dir = path_user_get("cache");

	if(boost::filesystem::exists(dir)) {
		boost::filesystem::directory_iterator it(dir), it_end;

		for(; it != it_end; it++) {
			string filename = it->path().filename().string();

			if(boost::starts_with(filename, name))
				if(except.find(filename) == except.end())
					boost::filesystem::remove(it->path());
		}
	}
}

CCL_NAMESPACE_END

