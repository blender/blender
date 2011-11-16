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

#ifndef __UTIL_MEMARENA_H__
#define __UTIL_MEMARENA_H__

#include <stdlib.h>

#include "util_list.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class MemArena {
public:
	MemArena(bool use_calloc = true, size_t buffer_size = (1<<14));
	~MemArena();

	void *alloc(size_t size);

protected:
	bool use_calloc;
	size_t buffer_size;

	list<uint8_t*> buffers;
	uint8_t *last_buffer;
	size_t last_left;
};

CCL_NAMESPACE_END

#endif /* __UTIL_MEMARENA_H__ */

