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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_map.h"

#include "DNA_ID.h"

namespace DEG {

BuilderMap::BuilderMap()
{
}

BuilderMap::~BuilderMap()
{
}

bool BuilderMap::checkIsBuilt(ID *id, int tag) const
{
  return (getIDTag(id) & tag) == tag;
}

void BuilderMap::tagBuild(ID *id, int tag)
{
  id_tags_.lookup_or_add(id, 0) |= tag;
}

bool BuilderMap::checkIsBuiltAndTag(ID *id, int tag)
{
  int &id_tag = id_tags_.lookup_or_add(id, 0);
  const bool result = (id_tag & tag) == tag;
  id_tag |= tag;
  return result;
}

int BuilderMap::getIDTag(ID *id) const
{
  return id_tags_.lookup_default(id, 0);
}

}  // namespace DEG
