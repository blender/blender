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

#ifndef __UTIL_CACHE_H__
#define __UTIL_CACHE_H__

/* Disk Cache based on Hashing
 *
 * To be used to cache expensive computations. The hash key is created from an
 * arbitrary number of bytes, by hashing the bytes using MD5, which then gives
 * the file name containing the data. This data then is read from the file
 * again into the appropriate data structures.
 *
 * This way we do not need to accurately track changes, compare dates and
 * invalidate cache entries, at the cost of exta computation. If everything
 * is stored in a global cache, computations can perhaps even be shared between
 * different scenes where it may be hard to detect duplicate work.
 */

#include "util_set.h"
#include "util_string.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class CacheBuffer {
public:
	const void *data;
	size_t size;

	CacheBuffer(const void *data_, size_t size_)
	{ data = data_; size = size_; }
};

class CacheData {
public:
	vector<CacheBuffer> buffers;
	string name;
	string filename;
	bool have_filename;
	FILE *f;

	CacheData(const string& name = "");
	~CacheData();

	const string& get_filename();

	template<typename T> void add(const vector<T>& data)
	{
		CacheBuffer buffer(data.size()? &data[0]: NULL, data.size()*sizeof(T));
		buffers.push_back(buffer);
	}

	template<typename T> void add(const array<T>& data)
	{
		CacheBuffer buffer(data.size()? &data[0]: NULL, data.size()*sizeof(T));
		buffers.push_back(buffer);
	}

	void add(const void *data, size_t size)
	{
		if(size) {
			CacheBuffer buffer(data, size);
			buffers.push_back(buffer);
		}
	}

	void add(const int& data)
	{
		CacheBuffer buffer(&data, sizeof(int));
		buffers.push_back(buffer);
	}

	void add(const float& data)
	{
		CacheBuffer buffer(&data, sizeof(float));
		buffers.push_back(buffer);
	}

	void add(const size_t& data)
	{
		CacheBuffer buffer(&data, sizeof(size_t));
		buffers.push_back(buffer);
	}

	template<typename T> void read(array<T>& data)
	{
		size_t size;

		if(!fread(&size, sizeof(size), 1, f)) {
			fprintf(stderr, "Failed to read vector size from cache.\n");
			return;
		}

		if(!size)
			return;

		data.resize(size/sizeof(T));

		if(!fread(&data[0], size, 1, f)) {
			fprintf(stderr, "Failed to read vector data from cache (%lu).\n", (unsigned long)size);
			return;
		}
	}

	void read(int& data)
	{
		size_t size;

		if(!fread(&size, sizeof(size), 1, f))
			fprintf(stderr, "Failed to read int size from cache.\n");
		if(!fread(&data, sizeof(data), 1, f))
			fprintf(stderr, "Failed to read int from cache.\n");
	}

	void read(float& data)
	{
		size_t size;

		if(!fread(&size, sizeof(size), 1, f))
			fprintf(stderr, "Failed to read float size from cache.\n");
		if(!fread(&data, sizeof(data), 1, f))
			fprintf(stderr, "Failed to read float from cache.\n");
	}

	void read(size_t& data)
	{
		size_t size;

		if(!fread(&size, sizeof(size), 1, f))
			fprintf(stderr, "Failed to read size_t size from cache.\n");
		if(!fread(&data, sizeof(data), 1, f))
			fprintf(stderr, "Failed to read size_t from cache.\n");
	}
};

class Cache {
public:
	static Cache global;

	void insert(CacheData& key, CacheData& value);
	bool lookup(CacheData& key, CacheData& value);

	void clear_except(const string& name, const set<string>& except);

protected:
	string data_filename(CacheData& key);
};

CCL_NAMESPACE_END

#endif /* __UTIL_CACHE_H__ */

