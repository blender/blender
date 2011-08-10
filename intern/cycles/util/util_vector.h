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

#ifndef __UTIL_VECTOR_H__
#define __UTIL_VECTOR_H__

/* Vector */

#include <string.h>
#include <vector>

CCL_NAMESPACE_BEGIN

using std::vector;

/* Array
 *
 * Simplified version of vector, serving two purposes:
 * - somewhat faster in that it does not clear memory on resize/alloc,
 *   this was actually showing up in profiles quite significantly
 * - if this is used, we are not tempted to use inefficient operations */

template<typename T>
class array
{
public:
	array()
	{
		data = NULL;
		datasize = 0;
	}

	array(size_t newsize)
	{
		if(newsize == 0) {
			data = NULL;
			datasize = 0;
		}
		else {
			data = new T[newsize];
			datasize = newsize;
		}
	}

	array(const array& from)
	{
		*this = from;
	}

	array& operator=(const array& from)
	{
		if(from.datasize == 0) {
			data = NULL;
			datasize = 0;
		}
		else {
			data = new T[from.datasize];
			memcpy(data, from.data, from.datasize*sizeof(T));
			datasize = from.datasize;
		}

		return *this;
	}

	array& operator=(const vector<T>& from)
	{
		datasize = from.size();
		data = NULL;

		if(datasize > 0) {
			data = new T[datasize];
			memcpy(data, &from[0], datasize*sizeof(T));
		}

		return *this;
	}

	~array()
	{
		delete [] data;
	}

	void resize(size_t newsize)
	{
		if(newsize == 0) {
			clear();
		}
		else {
			T *newdata = new T[newsize];
			memcpy(newdata, data, ((datasize < newsize)? datasize: newsize)*sizeof(T));
			delete [] data;

			data = newdata;
			datasize = newsize;
		}
	}

	void clear()
	{
		delete [] data;
		data = NULL;
		datasize = 0;
	}

	size_t size() const
	{
		return datasize;
	}

	T& operator[](size_t i) const
	{
		return data[i];
	}

protected:
	T *data;
	size_t datasize;
};

CCL_NAMESPACE_END

#endif /* __UTIL_VECTOR_H__ */

