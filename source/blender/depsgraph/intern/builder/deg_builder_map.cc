/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_map.h"

#include "DNA_ID.h"

namespace blender::deg {

bool BuilderMap::check_is_built(ID *id, int tag) const
{
  return (this->get_ID_tag(id) & tag) == tag;
}

void BuilderMap::tag_built(ID *id, int tag)
{
  id_tags_.lookup_or_add(id, 0) |= tag;
}

bool BuilderMap::check_is_built_and_tag(ID *id, int tag)
{
  int &id_tag = id_tags_.lookup_or_add(id, 0);
  const bool result = (id_tag & tag) == tag;
  id_tag |= tag;
  return result;
}

int BuilderMap::get_ID_tag(ID *id) const
{
  return id_tags_.lookup_default(id, 0);
}

}  // namespace blender::deg
