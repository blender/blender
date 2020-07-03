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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node_factory.h"

struct ID;

namespace blender {
namespace deg {

template<class ModeObjectType> NodeType DepsNodeFactoryImpl<ModeObjectType>::type() const
{
  return ModeObjectType::typeinfo.type;
}

template<class ModeObjectType> const char *DepsNodeFactoryImpl<ModeObjectType>::type_name() const
{
  return ModeObjectType::typeinfo.type_name;
}

template<class ModeObjectType> int DepsNodeFactoryImpl<ModeObjectType>::id_recalc_tag() const
{
  return ModeObjectType::typeinfo.id_recalc_tag;
}

template<class ModeObjectType>
Node *DepsNodeFactoryImpl<ModeObjectType>::create_node(const ID *id,
                                                       const char *subdata,
                                                       const char *name) const
{
  Node *node = new ModeObjectType();
  /* Populate base node settings. */
  node->type = type();
  /* Set name if provided, or use default type name. */
  if (name[0] != '\0') {
    node->name = name;
  }
  else {
    node->name = type_name();
  }
  node->init(id, subdata);
  return node;
}

}  // namespace deg
}  // namespace blender
