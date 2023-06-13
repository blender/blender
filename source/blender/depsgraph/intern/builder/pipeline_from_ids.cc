/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  bool need_pull_base_into_graph(const Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphNodeBuilder::need_pull_base_into_graph(base);
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

  bool need_pull_base_into_graph(const Base *base) override
  {
    if (!filter_.contains(&base->object->id)) {
      return false;
    }
    return DepsgraphRelationBuilder::need_pull_base_into_graph(base);
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
