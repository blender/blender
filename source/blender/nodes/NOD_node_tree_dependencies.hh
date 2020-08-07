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
 */

#pragma once

#include "BLI_vector_set.hh"

#include "DNA_ID.h"
#include "DNA_object_types.h"

struct bNodeTree;

namespace blender::nodes {

class NodeTreeDependencies {
 private:
  VectorSet<Object *> transform_deps_;
  VectorSet<Object *> geometry_deps_;
  VectorSet<ID *> id_deps_;

 public:
  void add_transform_dependency(Object *object)
  {
    if (object == nullptr) {
      return;
    }
    transform_deps_.add(object);
    id_deps_.add(&object->id);
  }

  void add_geometry_dependency(Object *object)
  {
    if (object == nullptr) {
      return;
    }
    geometry_deps_.add(object);
    id_deps_.add(&object->id);
  }

  bool depends_on(ID *id) const
  {
    return id_deps_.contains(id);
  }

  Span<Object *> transform_dependencies()
  {
    return transform_deps_;
  }

  Span<Object *> geometry_dependencies()
  {
    return geometry_deps_;
  }

  Span<ID *> id_dependencies()
  {
    return id_deps_;
  }
};

NodeTreeDependencies find_node_tree_dependencies(bNodeTree &ntree);

}  // namespace blender::nodes
