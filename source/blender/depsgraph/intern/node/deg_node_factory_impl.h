/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node_factory.h"

struct ID;

namespace blender::deg {

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

}  // namespace blender::deg
