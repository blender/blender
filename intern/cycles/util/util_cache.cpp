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
#include "util_foreach.h"
#include "util_md5.h"
#include "util_path.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* CacheData */

CacheData::CacheData(const string& name_)
{
	name = name_;
	f = NULL;
}

CacheData::~CacheData()
{
	if(f)
		fclose(f);
}

/* Cache */

Cache Cache::global;

string Cache::data_filename(const CacheData& key)
{
	MD5Hash hash;

	foreach(const CacheBuffer& buffer, key.buffers)
		hash.append((uint8_t*)buffer.data, buffer.size);
	
	string fname = key.name + "_" + hash.get_hex();
	return path_get("cache/" + fname);
}

void Cache::insert(const CacheData& key, const CacheData& value)
{
	string filename = data_filename(key);
	FILE *f = fopen(filename.c_str(), "wb");

	if(!f) {
		fprintf(stderr, "Failed to open file %s for writing.\n", filename.c_str());
		return;
	}

	foreach(const CacheBuffer& buffer, value.buffers) {
		if(!fwrite(&buffer.size, sizeof(buffer.size), 1, f))
			fprintf(stderr, "Failed to write to file %s.\n", filename.c_str());
		if(!fwrite(buffer.data, buffer.size, 1, f))
			fprintf(stderr, "Failed to write to file %s.\n", filename.c_str());
	}
	
	fclose(f);
}

bool Cache::lookup(const CacheData& key, CacheData& value)
{
	string filename = data_filename(key);
	FILE *f = fopen(filename.c_str(), "rb");

	if(!f)
		return false;
	
	value.name = key.name;
	value.f = f;

	return true;
}

CCL_NAMESPACE_END

