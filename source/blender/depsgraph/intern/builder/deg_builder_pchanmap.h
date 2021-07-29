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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_pchanmap.h
 *  \ingroup depsgraph
 */

#pragma once

struct GHash;

namespace DEG {

struct RootPChanMap {
	/* ctor and dtor - Create and free the internal map respectively. */
	RootPChanMap();
	~RootPChanMap();

	/* Debug contents of map. */
	void print_debug();

	/* Add a mapping. */
	void add_bone(const char *bone, const char *root);

	/* Check if there's a common root bone between two bones. */
	bool has_common_root(const char *bone1, const char *bone2);

protected:
	/* The actual map:
	 * - Keys are "strings" (const char *) - not dynamically allocated.
	 * - Values are "sets" (const char *) - not dynamically allocated.
	 *
	 * We don't use the C++ maps here, as it's more convenient to use
	 * Blender's GHash and be able to compare by-value instead of by-ref.
	 */
	struct GHash *map_;
};

}  // namespace DEG
