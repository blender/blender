/*
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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup depsgraph
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#pragma once

#include <functional>

/* TODO(sergey): Ideally we'll just use char* and statically allocated strings
 * to avoid any possible overhead caused by string (re)allocation/formatting. */
#include <string>
#include <vector>
#include <algorithm>

struct Depsgraph;

namespace DEG {

/* Commonly used types. */
using std::string;
using std::vector;

/* Commonly used functions. */
using std::max;
using std::to_string;

/* Function bindings. */
using std::function;
using namespace std::placeholders;
#define function_bind std::bind

/* Source of the dependency graph node update tag.
 *
 * NOTE: This is a bit mask, so accumulation of sources is possible.
 *
 * TODO(sergey): Find a better place for this. */
enum eUpdateSource {
	/* Update is caused by a time change. */
	DEG_UPDATE_SOURCE_TIME       = (1 << 0),
	/* Update caused by user directly or indirectly influencing the node. */
	DEG_UPDATE_SOURCE_USER_EDIT  = (1 << 1),
	/* Update is happening as a special response for the relations update. */
	DEG_UPDATE_SOURCE_RELATIONS  = (1 << 2),
	/* Update is happening due to visibility change. */
	DEG_UPDATE_SOURCE_VISIBILITY = (1 << 3),
};

}  // namespace DEG
