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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/util/deg_util_foreach.h
 *  \ingroup depsgraph
 */

#pragma once

#if (__cplusplus > 199711L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
#  define foreach(x, y) for(x : y)
#elif defined(HAVE_BOOST_FUNCTION_BINDINGS)
#  include <boost/foreach.hpp>
#  define foreach BOOST_FOREACH
#else
#pragma message("No available foreach() implementation. Using stub instead, disabling new depsgraph")

#ifndef WITH_LEGACY_DEPSGRAPH
#  error "Unable to build new depsgraph and legacy one is disabled."
#endif

#define DISABLE_NEW_DEPSGRAPH

#  define foreach(x, y) for (x; false; (void)y)
#endif
