/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "pipeline_compositor.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.hh"

namespace blender::deg {

namespace {

class CompositorDepsgraphNodeBuilder : public DepsgraphNodeBuilder {
 public:
  using DepsgraphNodeBuilder::DepsgraphNodeBuilder;

  void build_idproperties(IDProperty * /*id_property*/) override {}
};

class CompositorDepsgraphRelationBuilder : public DepsgraphRelationBuilder {
 public:
  using DepsgraphRelationBuilder::DepsgraphRelationBuilder;

  void build_idproperties(IDProperty * /*id_property*/) override {}
};

}  // namespace

CompositorBuilderPipeline::CompositorBuilderPipeline(::Depsgraph *graph, bNodeTree *nodetree)
    : AbstractBuilderPipeline(graph), nodetree_(nodetree)
{
  deg_graph_->is_render_pipeline_depsgraph = true;
}

unique_ptr<DepsgraphNodeBuilder> CompositorBuilderPipeline::construct_node_builder()
{
  return std::make_unique<CompositorDepsgraphNodeBuilder>(bmain_, deg_graph_, &builder_cache_);
}

unique_ptr<DepsgraphRelationBuilder> CompositorBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<CompositorDepsgraphRelationBuilder>(bmain_, deg_graph_, &builder_cache_);
}

void CompositorBuilderPipeline::build_nodes(DepsgraphNodeBuilder &node_builder)
{
  node_builder.build_scene_render(scene_, view_layer_);
  node_builder.build_nodetree(nodetree_);
}

void CompositorBuilderPipeline::build_relations(DepsgraphRelationBuilder &relation_builder)
{
  relation_builder.build_scene_render(scene_, view_layer_);
  relation_builder.build_nodetree(nodetree_);
}

}  // namespace blender::deg
