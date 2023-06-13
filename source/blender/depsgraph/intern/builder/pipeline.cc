/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "pipeline.h"

#include "PIL_time.h"

#include "BKE_global.h"

#include "DNA_scene_types.h"

#include "deg_builder_cycle.h"
#include "deg_builder_nodes.h"
#include "deg_builder_relations.h"
#include "deg_builder_transitive.h"

namespace blender::deg {

AbstractBuilderPipeline::AbstractBuilderPipeline(::Depsgraph *graph)
    : deg_graph_(reinterpret_cast<Depsgraph *>(graph)),
      bmain_(deg_graph_->bmain),
      scene_(deg_graph_->scene),
      view_layer_(deg_graph_->view_layer)
{
}

void AbstractBuilderPipeline::build()
{
  double start_time = 0.0;
  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    start_time = PIL_check_seconds_timer();
  }

  build_step_sanity_check();
  build_step_nodes();
  build_step_relations();
  build_step_finalize();

  if (G.debug & (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_TIME)) {
    printf("Depsgraph built in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

void AbstractBuilderPipeline::build_step_sanity_check()
{
  BLI_assert(BLI_findindex(&scene_->view_layers, view_layer_) != -1);
  BLI_assert(deg_graph_->scene == scene_);
  BLI_assert(deg_graph_->view_layer == view_layer_);
}

void AbstractBuilderPipeline::build_step_nodes()
{
  /* Generate all the nodes in the graph first */
  unique_ptr<DepsgraphNodeBuilder> node_builder = construct_node_builder();
  node_builder->begin_build();
  build_nodes(*node_builder);
  node_builder->end_build();
}

void AbstractBuilderPipeline::build_step_relations()
{
  /* Hook up relationships between operations - to determine evaluation order. */
  unique_ptr<DepsgraphRelationBuilder> relation_builder = construct_relation_builder();
  relation_builder->begin_build();
  build_relations(*relation_builder);
  relation_builder->build_copy_on_write_relations();
  relation_builder->build_driver_relations();
}

void AbstractBuilderPipeline::build_step_finalize()
{
  /* Detect and solve cycles. */
  deg_graph_detect_cycles(deg_graph_);
  /* Simplify the graph by removing redundant relations (to optimize
   * traversal later). */
  /* TODO: it would be useful to have an option to disable this in cases where
   *       it is causing trouble. */
  if (G.debug_value == 799) {
    deg_graph_transitive_reduction(deg_graph_);
  }
  /* Store pointers to commonly used evaluated datablocks. */
  deg_graph_->scene_cow = (Scene *)deg_graph_->get_cow_id(&deg_graph_->scene->id);
  /* Flush visibility layer and re-schedule nodes for update. */
  deg_graph_build_finalize(bmain_, deg_graph_);
  DEG_graph_tag_on_visible_update(reinterpret_cast<::Depsgraph *>(deg_graph_), false);
#if 0
  if (!DEG_debug_consistency_check(deg_graph_)) {
    printf("Consistency validation failed, ABORTING!\n");
    abort();
  }
#endif
  /* Relations are up to date. */
  deg_graph_->need_update_relations = false;
}

unique_ptr<DepsgraphNodeBuilder> AbstractBuilderPipeline::construct_node_builder()
{
  return std::make_unique<DepsgraphNodeBuilder>(bmain_, deg_graph_, &builder_cache_);
}

unique_ptr<DepsgraphRelationBuilder> AbstractBuilderPipeline::construct_relation_builder()
{
  return std::make_unique<DepsgraphRelationBuilder>(bmain_, deg_graph_, &builder_cache_);
}

}  // namespace blender::deg
