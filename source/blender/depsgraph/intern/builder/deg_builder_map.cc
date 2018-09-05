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

/** \file blender/depsgraph/intern/builder/deg_builder_map.cc
 *  \ingroup depsgraph
 */

#include "intern/builder/deg_builder_map.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_ID.h"

namespace DEG {

BuilderMap::BuilderMap()
{
	set = BLI_gset_ptr_new("deg builder gset");
}

BuilderMap::~BuilderMap()
{
	BLI_gset_free(set, NULL);
}

bool BuilderMap::checkIsBuilt(ID *id)
{
	return BLI_gset_haskey(set, id);
}

void BuilderMap::tagBuild(ID *id)
{
	BLI_gset_insert(set, id);
}

bool BuilderMap::checkIsBuiltAndTag(ID *id)
{
	void **key_p;
	if (!BLI_gset_ensure_p_ex(set, id, &key_p)) {
		*key_p = id;
		return false;
	}
	return true;
}

}  // namespace DEG
