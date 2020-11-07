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

#include "pipeline_from_ids.h"

#include "DNA_layer_types.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.h"

namespace blender::deg {

namespace {

class DepsgraphFromIDsFilter {
 public:
  DepsgraphFromIDsFilter(Span<ID *> ids)
  {
    ids_.add_multiple(ids);
  }

  bool contains(ID *id)
  {
    return ids_.contains(id);
  }

 protected:
  Set<ID *> ids_;
};

class DepsgraphFromIDsNodeBuilder : public DepsgraphNodeBuilder {
 public:
  DepsgraphFromIDsNodeBuilder(Main *bmain,
                              Depsgraph *graph,
                              DepsgraphBuilderCache *cache,
                              Span<ID *> ids)
      : DepsgraphNodeBuilder(bmain, graph, cache), filter_(ids)
  {
  }

  virtual bool need_pull_base_into_graph(Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphNodeBuilder::need_pull_base_into_graph(base);
  }

  virtual void build_object_proxy_group(Object *object, bool is_visible) override
  {
    if (object->proxy_group == nullptr) {
      return;
    }
    if (!filter_.contains(&object->proxy_group->id)) {
      return;
    }
    DepsgraphNodeBuilder::build_object_proxy_group(object, is_visible);
  }

 protected:
  DepsgraphFromIDsFilter filter_;
};

class DepsgraphFromIDsRelationBuilder : public DepsgraphRelationBuilder {
 public:
  DepsgraphFromIDsRelationBuilder(Main *bmain,
                                  Depsgraph *graph,
                                  DepsgraphBuilderCache *cache,
                                  Span<ID *> ids)
      : DepsgraphRelationBuilder(bmain, graph, cache), filter_(ids)
  {
  }

  virtual bool need_pull_base_into_graph(Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphRelationBuilder::need_pull_base_into_graph(base);
  }

  virtual void build_object_proxy_group(Object *object) override
  {
    if (object->proxy_group == nullptr) {
      return;
    }
    if (!filter_.contains(&object->proxy_group->id)) {
      return;
    }
    DepsgraphRelationBuilder::build_object_proxy_group(object);
  }

 protected:
  DepsgraphFromIDsFilter filter_;
};

}  // namespace

FromIDsBuilderPipeline::FromIDsBuilderPipeline(::Depsgraph *graph, Span<ID *> ids)
    : AbstractBuilderPipeline(graph), ids_(ids)
{
}

unique_ptr<DepsgraphNodeBuilder> FromIDsBuilderPipeline::construct_node_builder()
{
  return std::make_unique<DepsgraphFromIDsNodeBuilder>(bmain_, deg_graph_, &builder_cache_, ids_);
}

unique_ptr<DepsgraphRelationBuilder> FromIDsBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<DepsgraphFromIDsRelationBuilder>(
      bmain_, deg_graph_, &builder_cache_, ids_);
}

void FromIDsBuilderPipeline::build_nodes(DepsgraphNodeBuilder &node_builder)
{
  node_builder.build_view_layer(scene_, view_layer_, DEG_ID_LINKED_DIRECTLY);
  for (ID *id : ids_) {
    node_builder.build_id(id);
  }
}

void FromIDsBuilderPipeline::build_relations(DepsgraphRelationBuilder &relation_builder)
{
  relation_builder.build_view_layer(scene_, view_layer_, DEG_ID_LINKED_DIRECTLY);
  for (ID *id : ids_) {
    relation_builder.build_id(id);
  }
}

}  // namespace blender::deg
