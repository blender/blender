/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "pipeline_render.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.hh"

namespace blender::deg {

namespace {

class RenderDepsgraphNodeBuilder : public DepsgraphNodeBuilder {
 public:
  using DepsgraphNodeBuilder::DepsgraphNodeBuilder;

  void build_idproperties(IDProperty * /*id_property*/) override {}
};

class RenderDepsgraphRelationBuilder : public DepsgraphRelationBuilder {
 public:
  using DepsgraphRelationBuilder::DepsgraphRelationBuilder;

  void build_idproperties(IDProperty * /*id_property*/) override {}
};

}  // namespace

RenderBuilderPipeline::RenderBuilderPipeline(::Depsgraph *graph) : AbstractBuilderPipeline(graph)
{
  deg_graph_->is_render_pipeline_depsgraph = true;
}

unique_ptr<DepsgraphNodeBuilder> RenderBuilderPipeline::construct_node_builder()
{
  return std::make_unique<RenderDepsgraphNodeBuilder>(bmain_, deg_graph_, &builder_cache_);
}

unique_ptr<DepsgraphRelationBuilder> RenderBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<RenderDepsgraphRelationBuilder>(bmain_, deg_graph_, &builder_cache_);
}

void RenderBuilderPipeline::build_nodes(DepsgraphNodeBuilder &node_builder)
{
  node_builder.build_scene_render(scene_, view_layer_);
}

void RenderBuilderPipeline::build_relations(DepsgraphRelationBuilder &relation_builder)
{
  relation_builder.build_scene_render(scene_, view_layer_);
}

}  // namespace blender::deg
