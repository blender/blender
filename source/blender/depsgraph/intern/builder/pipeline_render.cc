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

#include "pipeline_render.h"

#include "intern/builder/deg_builder_nodes.h"
#include "intern/builder/deg_builder_relations.h"
#include "intern/depsgraph.h"

namespace blender {
namespace deg {

RenderBuilderPipeline::RenderBuilderPipeline(::Depsgraph *graph,
                                             Main *bmain,
                                             Scene *scene,
                                             ViewLayer *view_layer)
    : AbstractBuilderPipeline(graph, bmain, scene, view_layer)
{
  deg_graph_->is_render_pipeline_depsgraph = true;
}

void RenderBuilderPipeline::build_nodes(DepsgraphNodeBuilder &node_builder)
{
  node_builder.build_scene_render(scene_, view_layer_);
}

void RenderBuilderPipeline::build_relations(DepsgraphRelationBuilder &relation_builder)
{
  relation_builder.build_scene_render(scene_, view_layer_);
}

}  // namespace deg
}  // namespace blender
