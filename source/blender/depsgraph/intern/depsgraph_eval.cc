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
 * Evaluation engine entrypoints for Depsgraph Engine.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "BKE_scene.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/eval/deg_eval.h"
#include "intern/eval/deg_eval_flush.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

#include "intern/depsgraph.h"

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(Depsgraph *graph)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  deg_graph->ctime = BKE_scene_frame_get(deg_graph->scene);
  /* Update time on primary timesource. */
  DEG::TimeSourceNode *tsrc = deg_graph->find_time_source();
  tsrc->cfra = deg_graph->ctime;
  /* Update time in scene. */
  if (deg_graph->scene_cow) {
    BKE_scene_frame_set(deg_graph->scene_cow, deg_graph->ctime);
  }
  DEG::deg_evaluate_on_refresh(deg_graph);
  deg_graph->need_update_time = false;
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(Main *bmain, Depsgraph *graph, float ctime)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  deg_graph->ctime = ctime;
  /* Update time on primary timesource. */
  DEG::TimeSourceNode *tsrc = deg_graph->find_time_source();
  tsrc->cfra = ctime;
  tsrc->tag_update(deg_graph, DEG::DEG_UPDATE_SOURCE_TIME);
  DEG::deg_graph_flush_updates(bmain, deg_graph);
  /* Update time in scene. */
  if (deg_graph->scene_cow) {
    BKE_scene_frame_set(deg_graph->scene_cow, deg_graph->ctime);
  }
  /* Perform recalculation updates. */
  DEG::deg_evaluate_on_refresh(deg_graph);
  deg_graph->need_update_time = false;
}

bool DEG_needs_eval(Depsgraph *graph)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
  return BLI_gset_len(deg_graph->entry_tags) != 0 || deg_graph->need_update_time;
}
