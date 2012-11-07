/*
 * Copyright 2012, Blender Foundation.
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

#ifndef __UTIL_STATS_H__
#define __UTIL_STATS_H__

#include "util_thread.h"

CCL_NAMESPACE_BEGIN

class Stats {
public:
	Stats() : lock(), mem_used(0), mem_peak(0) {}

	void mem_alloc(size_t size) {
		lock.lock();

		mem_used += size;
		if(mem_used > mem_peak)
			mem_peak = mem_used;

		lock.unlock();
	}

	void mem_free(size_t size) {
		lock.lock();
		mem_used -= size;
		lock.unlock();
	}

	spin_lock lock;
	size_t mem_used;
	size_t mem_peak;
};

CCL_NAMESPACE_END

#endif /* __UTIL_STATS_H__ */
