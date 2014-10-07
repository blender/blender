/* This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "range_tree.hh"

/* Give RangeTreeUInt a real type rather than the opaque struct type
   defined for external use. */
#define RANGE_TREE_C_API_INTERNAL
typedef RangeTree<unsigned> RangeTreeUInt;

#include "range_tree_c_api.h"

RangeTreeUInt *range_tree_uint_alloc(unsigned min, unsigned max)
{
	return new RangeTreeUInt(min, max);
}

RangeTreeUInt *range_tree_uint_copy(RangeTreeUInt *src)
{
	return new RangeTreeUInt(*src);
}

void range_tree_uint_free(RangeTreeUInt *rt)
{
	delete rt;
}

void range_tree_uint_take(RangeTreeUInt *rt, unsigned v)
{
	rt->take(v);
}

bool range_tree_uint_retake(RangeTreeUInt *rt, unsigned v)
{
	return rt->retake(v);
}

unsigned range_tree_uint_take_any(RangeTreeUInt *rt)
{
	return rt->take_any();
}

void range_tree_uint_release(RangeTreeUInt *rt, unsigned v)
{
	rt->release(v);
}

bool range_tree_uint_has(const RangeTreeUInt *rt, unsigned v)
{
	return rt->has(v);
}

bool range_tree_uint_has_range(
        const RangeTreeUInt *rt,
        unsigned vmin,
        unsigned vmax)
{
	return rt->has_range(vmin, vmax);
}

bool range_tree_uint_empty(const RangeTreeUInt *rt)
{
	return rt->empty();
}

unsigned range_tree_uint_size(const RangeTreeUInt *rt)
{
	return rt->size();
}

void range_tree_uint_print(const RangeTreeUInt *rt)
{
	rt->print();
}

unsigned int range_tree_uint_allocation_lower_bound(const RangeTreeUInt *rt)
{
	return rt->allocation_lower_bound();
}
