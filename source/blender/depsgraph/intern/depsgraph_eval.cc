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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Evaluation engine entry-points for Depsgraph Engine.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_scene.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/eval/deg_eval.h"
#include "intern/eval/deg_eval_flush.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_tag.h"

namespace deg = blender::deg;

static void deg_flush_updates_and_refresh(deg::Depsgraph *deg_graph)
{
  /* Update the time on the cow scene. */
  if (deg_graph->scene_cow) {
    BKE_scene_frame_set(deg_graph->scene_cow, deg_graph->frame);
  }

  deg::graph_tag_ids_for_visible_update(deg_graph);
  deg::deg_graph_flush_updates(deg_graph);
  deg::deg_evaluate_on_refresh(deg_graph);
}

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(Depsgraph *graph)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(graph);
  const Scene *scene = DEG_get_input_scene(graph);
  const float frame = BKE_scene_frame_get(scene);
  const float ctime = BKE_scene_ctime_get(scene);

  if (deg_graph->frame != frame || ctime != deg_graph->ctime) {
    deg_graph->tag_time_source();
    deg_graph->frame = frame;
    deg_graph->ctime = ctime;
  }

  deg_flush_updates_and_refresh(deg_graph);
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(Depsgraph *graph, float frame)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(graph);
  const Scene *scene = DEG_get_input_scene(graph);

  deg_graph->tag_time_source();
  deg_graph->frame = frame;
  deg_graph->ctime = BKE_scene_frame_to_ctime(scene, frame);
  deg_flush_updates_and_refresh(deg_graph);
}
