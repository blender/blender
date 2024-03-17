/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "pipeline_from_collection.h"

#include "BKE_collection.hh"

#include "DNA_layer_types.h"

#include "DEG_depsgraph.hh"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.hh"

namespace blender::deg {

namespace {

class DepsgraphFromCollectionIDsFilter {
 public:
  DepsgraphFromCollectionIDsFilter(const Set<ID *> &ids) : ids_(ids) {}

  bool contains(ID *id)
  {
    return ids_.contains(id);
  }

 protected:
  const Set<ID *> &ids_;
};

class DepsgraphFromCollectionIDsNodeBuilder : public DepsgraphNodeBuilder {
 public:
  DepsgraphFromCollectionIDsNodeBuilder(Main *bmain,
                                        Depsgraph *graph,
                                        DepsgraphBuilderCache *cache,
                                        const Set<ID *> &ids)
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
  DepsgraphFromCollectionIDsFilter filter_;
};

class DepsgraphFromCollectionIDsRelationBuilder : public DepsgraphRelationBuilder {
 public:
  DepsgraphFromCollectionIDsRelationBuilder(Main *bmain,
                                            Depsgraph *graph,
                                            DepsgraphBuilderCache *cache,
                                            const Set<ID *> &ids)
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
  DepsgraphFromCollectionIDsFilter filter_;
};

}  // namespace

FromCollectionBuilderPipeline::FromCollectionBuilderPipeline(::Depsgraph *graph,
                                                             Collection *collection)
    : AbstractBuilderPipeline(graph)
{
  Base *base = BKE_collection_or_layer_objects(scene_, view_layer_, collection);
  const int base_flag = (deg_graph_->mode == DAG_EVAL_RENDER) ? BASE_ENABLED_RENDER :
                                                                BASE_ENABLED_VIEWPORT;
  for (; base; base = base->next) {
    if (base->flag & base_flag) {
      ids_.add(&base->object->id);
    }
  }
}

unique_ptr<DepsgraphNodeBuilder> FromCollectionBuilderPipeline::construct_node_builder()
{
  return std::make_unique<DepsgraphFromCollectionIDsNodeBuilder>(
      bmain_, deg_graph_, &builder_cache_, ids_);
}

unique_ptr<DepsgraphRelationBuilder> FromCollectionBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<DepsgraphFromCollectionIDsRelationBuilder>(
      bmain_, deg_graph_, &builder_cache_, ids_);
}

void FromCollectionBuilderPipeline::build_nodes(DepsgraphNodeBuilder &node_builder)
{
  node_builder.build_view_layer(scene_, view_layer_, DEG_ID_LINKED_DIRECTLY);
  for (ID *id : ids_) {
    node_builder.build_id(id, true);
  }
}

void FromCollectionBuilderPipeline::build_relations(DepsgraphRelationBuilder &relation_builder)
{
  relation_builder.build_view_layer(scene_, view_layer_, DEG_ID_LINKED_DIRECTLY);
  for (ID *id : ids_) {
    relation_builder.build_id(id);
  }
}

}  // namespace blender::deg
