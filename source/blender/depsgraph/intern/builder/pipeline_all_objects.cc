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

#include "pipeline_all_objects.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.h"

#include "DNA_layer_types.h"

namespace blender::deg {

namespace {

class AllObjectsNodeBuilder : public DepsgraphNodeBuilder {
 public:
  AllObjectsNodeBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache)
      : DepsgraphNodeBuilder(bmain, graph, cache)
  {
  }

  bool need_pull_base_into_graph(Base * /*base*/) override
  {
    return true;
  }
};

class AllObjectsRelationBuilder : public DepsgraphRelationBuilder {
 public:
  AllObjectsRelationBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache)
      : DepsgraphRelationBuilder(bmain, graph, cache)
  {
  }

  bool need_pull_base_into_graph(Base * /*base*/) override
  {
    return true;
  }
};

}  // namespace

AllObjectsBuilderPipeline::AllObjectsBuilderPipeline(::Depsgraph *graph)
    : ViewLayerBuilderPipeline(graph)
{
}

unique_ptr<DepsgraphNodeBuilder> AllObjectsBuilderPipeline::construct_node_builder()
{
  return std::make_unique<AllObjectsNodeBuilder>(bmain_, deg_graph_, &builder_cache_);
}

unique_ptr<DepsgraphRelationBuilder> AllObjectsBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<AllObjectsRelationBuilder>(bmain_, deg_graph_, &builder_cache_);
}

}  // namespace blender::deg
