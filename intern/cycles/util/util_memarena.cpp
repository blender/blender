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

#include "util_foreach.h"
#include "util_math.h"
#include "util_memarena.h"

CCL_NAMESPACE_BEGIN

MemArena::MemArena(bool use_calloc_, size_t buffer_size_)
{
	use_calloc = use_calloc_;
	buffer_size = buffer_size_;

	last_left = 0;
	last_buffer = NULL;
}

MemArena::~MemArena()
{
	foreach(uint8_t *buffer, buffers)
		delete [] buffer;
}

void *MemArena::alloc(size_t size)
{
	if(size > last_left) {
		last_left = (size > buffer_size)? size: buffer_size;
		last_buffer = new uint8_t[last_left];

		if(use_calloc)
			memset(last_buffer, 0, last_left);

		buffers.push_back(last_buffer);
	}

	uint8_t *mem = last_buffer;

	last_buffer += size;
	last_left -= size;

	return (void*)mem;
}

CCL_NAMESPACE_END

