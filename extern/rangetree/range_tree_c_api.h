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

#ifndef RANGE_TREE_C_API_H
#define RANGE_TREE_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Simple C-accessible wrapper for RangeTree<unsigned> */

#ifndef RANGE_TREE_C_API_INTERNAL
typedef struct RangeTreeUInt RangeTreeUInt;
#endif

RangeTreeUInt *range_tree_uint_alloc(unsigned min, unsigned max);

RangeTreeUInt *range_tree_uint_copy(RangeTreeUInt *src);

void range_tree_uint_free(RangeTreeUInt *rt);

void range_tree_uint_take(RangeTreeUInt *rt, unsigned v);

unsigned range_tree_uint_take_any(RangeTreeUInt *rt);

void range_tree_uint_release(RangeTreeUInt *rt, unsigned v);

int range_tree_uint_has(const RangeTreeUInt *rt, unsigned v);

int range_tree_uint_has_range(const RangeTreeUInt *rt,
							  unsigned vmin,
							  unsigned vmax);

int range_tree_uint_empty(const RangeTreeUInt *rt);

unsigned range_tree_uint_size(const RangeTreeUInt *rt);

void range_tree_uint_print(const RangeTreeUInt *rt);

unsigned int range_tree_uint_allocation_lower_bound(const RangeTreeUInt *rt);

#ifdef __cplusplus
}
#endif

#endif /* __DUALCON_H__ */
