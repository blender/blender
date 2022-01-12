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

#include "MEM_guardedalloc.h"

#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"

struct ID;

namespace blender {
namespace deg {

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

}  // namespace deg
}  // namespace blender

#include "intern/node/deg_node_factory_impl.h"
