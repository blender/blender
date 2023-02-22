/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

#include "pipeline_compositor.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.h"

namespace blender::deg {

CompositorBuilderPipeline::CompositorBuilderPipeline(::Depsgraph *graph, bNodeTree *nodetree)
    : AbstractBuilderPipeline(graph), nodetree_(nodetree)
{
  deg_graph_->is_render_pipeline_depsgraph = true;
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
