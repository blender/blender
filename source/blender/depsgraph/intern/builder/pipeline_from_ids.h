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

#include "pipeline.h"

namespace blender {
namespace deg {

/* Optimized builders for dependency graph built from a given set of IDs.
 *
 * General notes:
 *
 * - We pull in all bases if their objects are in the set of IDs. This allows to have proper
 *   visibility and other flags assigned to the objects.
 *   All other bases (the ones which points to object which is outside of the set of IDs) are
 *   completely ignored.
 *
 * - Proxy groups pointing to objects which are outside of the IDs set are also ignored.
 *   This way we avoid high-poly character body pulled into the dependency graph when it's coming
 *   from a library into an animation file and the dependency graph constructed for a proxy rig. */

class FromIDsBuilderPipeline : public AbstractBuilderPipeline {
 public:
  FromIDsBuilderPipeline(
      ::Depsgraph *graph, Main *bmain, Scene *scene, ViewLayer *view_layer, ID **ids, int num_ids);

 protected:
  virtual unique_ptr<DepsgraphNodeBuilder> construct_node_builder() override;
  virtual unique_ptr<DepsgraphRelationBuilder> construct_relation_builder() override;

  virtual void build_nodes(DepsgraphNodeBuilder &node_builder) override;
  virtual void build_relations(DepsgraphRelationBuilder &relation_builder) override;

 private:
  ID **ids_;
  const int num_ids_;
};

}  // namespace deg
}  // namespace blender
