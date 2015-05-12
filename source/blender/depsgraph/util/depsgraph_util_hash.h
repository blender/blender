/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 */

#ifndef __DEPSGRAPH_UTIL_HASH_H__
#define __DEPSGRAPH_UTIL_HASH_H__

#if defined(DEG_NO_UNORDERED_MAP)
#  define DEG_HASH_NAMESPACE_BEGIN
#  define DEG_HASH_NAMESPACE_END
#endif

#if defined(DEG_TR1_UNORDERED_MAP)
#  include <tr1/unordered_map>
#  define DEG_HASH_NAMESPACE_BEGIN namespace std { namespace tr1 {
#  define DEG_HASH_NAMESPACE_END } }
using std::tr1::hash;
#endif

#if defined(DEG_STD_UNORDERED_MAP)
#  include <unordered_map>
#  define DEG_HASH_NAMESPACE_BEGIN namespace std {
#  define DEG_HASH_NAMESPACE_END }
using std::hash;
#endif

#if defined(DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#  include <unordered_map>
#  define DEG_HASH_NAMESPACE_BEGIN namespace std { namespace tr1 {
#  define DEG_HASH_NAMESPACE_END } }
using std::tr1::hash;
#endif

#if !defined(DEG_NO_UNORDERED_MAP) && !defined(DEG_TR1_UNORDERED_MAP) && \
    !defined(DEG_STD_UNORDERED_MAP) && !defined(DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)  // NOLINT
#  error One of: DEG_NO_UNORDERED_MAP, DEG_TR1_UNORDERED_MAP,\
 DEG_STD_UNORDERED_MAP, DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE must be defined!  // NOLINT
#endif

/* XXX this might require 2 different variants for sizeof(size_t) (32 vs 64 bit) */
inline size_t hash_combine(size_t hash_a, size_t hash_b)
{
	return hash_a ^ (hash_b + 0x9e3779b9 + (hash_a << 6) + (hash_a >> 2));
}

#endif  /* __DEPSGRAPH_UTIL_HASH_H__ */
