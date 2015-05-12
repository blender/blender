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

#ifndef __DEPSGRAPH_UTIL_SET_H__
#define __DEPSGRAPH_UTIL_SET_H__

#include <set>

#include "depsgraph_util_hash.h"

using std::set;

#if defined(DEG_NO_UNORDERED_MAP)
#  include <set>
typedef std::set unordered_set;
#endif

#if defined(DEG_TR1_UNORDERED_MAP)
#  include <tr1/unordered_set>
using std::tr1::unordered_set;
#endif

#if defined(DEG_STD_UNORDERED_MAP)
#  include <unordered_set>
using std::unordered_set;
#endif

#if defined(DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#  include <unordered_set>
using std::tr1::unordered_set;
#endif

#if !defined(DEG_NO_UNORDERED_MAP) && !defined(DEG_TR1_UNORDERED_MAP) && \
    !defined(DEG_STD_UNORDERED_MAP) && !defined(DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)  // NOLINT
#  error One of: DEG_NO_UNORDERED_MAP, DEG_TR1_UNORDERED_MAP,\
 DEG_STD_UNORDERED_MAP, DEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE must be defined!  // NOLINT
#endif

#endif  /* __DEPSGRAPH_UTIL_SET_H__ */
