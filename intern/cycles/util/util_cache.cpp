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

#include <stdio.h>

#include "util_cache.h"
#include "util_debug.h"
#include "util_foreach.h"
#include "util_map.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_types.h"

#include <boost/version.hpp>

#if (BOOST_VERSION < 104400)
#  define BOOST_FILESYSTEM_VERSION 2
#endif

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
	FILE *f = path_fopen(filename, "wb");

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
	FILE *f = path_fopen(filename, "rb");

	if(!f)
		return false;
	
	value.name = key.name;
	value.f = f;

	return true;
}

void Cache::clear_except(const string& name, const set<string>& except)
{
	path_cache_clear_except(name, except);
}

CCL_NAMESPACE_END

