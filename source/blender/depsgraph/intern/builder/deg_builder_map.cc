/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_map.h"

#include "DNA_ID.h"

namespace blender::deg {

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

}  // namespace blender::deg
