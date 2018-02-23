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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_map.h
 *  \ingroup depsgraph
 */

#pragma once

struct GSet;
struct ID;

namespace DEG {

class BuilderMap {
public:
	BuilderMap();
	~BuilderMap();

	/* Check whether given ID is already handled by builder (or if it's being
	 * handled).
	 */
	bool checkIsBuilt(ID *id);

	/* Tag given ID as handled/built. */
	void tagBuild(ID *id);

	/* Combination of previous two functions, returns truth if ID was already
	 * handled, or tags is handled otherwise and return false.
	 */
	bool checkIsBuiltAndTag(ID *id);

	template<typename T> bool checkIsBuilt(T *datablock) {
		return checkIsBuilt(&datablock->id);
	}
	template<typename T> void tagBuild(T *datablock) {
		tagBuild(&datablock->id);
	}
	template<typename T> bool checkIsBuiltAndTag(T *datablock) {
		return checkIsBuiltAndTag(&datablock->id);
	}

	GSet *set;
};

}  // namespace DEG
