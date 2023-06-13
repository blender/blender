/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"

struct ID;

namespace blender::deg {
struct DepsNodeFactory {
  virtual NodeType type() const = 0;
  virtual const char *type_name() const = 0;

  virtual int id_recalc_tag() const = 0;

  virtual Node *create_node(const ID *id, const char *subdata, const char *name) const = 0;
};

template<class ModeObjectType> struct DepsNodeFactoryImpl : public DepsNodeFactory {
  virtual NodeType type() const override;
  virtual const char *type_name() const override;

  virtual int id_recalc_tag() const override;

  virtual Node *create_node(const ID *id, const char *subdata, const char *name) const override;
};

/* Register typeinfo */
void register_node_typeinfo(DepsNodeFactory *factory);

/* Get typeinfo for specified type */
DepsNodeFactory *type_get_factory(NodeType type);

}  // namespace blender::deg

#include "intern/node/deg_node_factory_impl.h"
