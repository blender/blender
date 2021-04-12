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

#include "deg_builder_cache.h"

#include "intern/depsgraph_type.h"

struct Depsgraph;
struct Main;
struct Scene;
struct ViewLayer;

namespace blender {
namespace deg {

struct Depsgraph;
class DepsgraphNodeBuilder;
class DepsgraphRelationBuilder;

/* Base class for Depsgraph Builder pipelines.
 *
 * Basically it runs through the following steps:
 * - sanity check
 * - build nodes
 * - build relations
 * - finalize
 */
class AbstractBuilderPipeline {
 public:
  AbstractBuilderPipeline(::Depsgraph *graph);
  virtual ~AbstractBuilderPipeline() = default;

  void build();

 protected:
  Depsgraph *deg_graph_;
  Main *bmain_;
  Scene *scene_;
  ViewLayer *view_layer_;
  DepsgraphBuilderCache builder_cache_;

  virtual unique_ptr<DepsgraphNodeBuilder> construct_node_builder();
  virtual unique_ptr<DepsgraphRelationBuilder> construct_relation_builder();

  virtual void build_step_sanity_check();
  void build_step_nodes();
  void build_step_relations();
  void build_step_finalize();

  virtual void build_nodes(DepsgraphNodeBuilder &node_builder) = 0;
  virtual void build_relations(DepsgraphRelationBuilder &relation_builder) = 0;
};

}  // namespace deg
}  // namespace blender
