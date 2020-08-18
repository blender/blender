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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "pipeline_view_layer.h"

namespace blender {
namespace deg {

/* Builds a dependency graph that contains all objects in the view layer.
 * This is contrary to the regular ViewLayerBuilderPipeline, which is limited to visible objects
 * (and their dependencies). */
class AllObjectsBuilderPipeline : public ViewLayerBuilderPipeline {
 public:
  AllObjectsBuilderPipeline(::Depsgraph *graph);

 protected:
  virtual unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  virtual unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;
};

}  // namespace deg
}  // namespace blender
