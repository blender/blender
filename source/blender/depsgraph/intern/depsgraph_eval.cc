/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DEG_depsgraph_writeback_sync.hh"

#include "intern/eval/deg_eval.h"
#include "intern/eval/deg_eval_flush.h"

#include "intern/node/deg_node.hh"
#include "intern/node/deg_node_operation.hh"
#include "intern/node/deg_node_time.hh"

#include "intern/depsgraph.hh"
#include "intern/depsgraph_tag.hh"

namespace deg = blender::deg;

static void deg_flush_updates_and_refresh(deg::Depsgraph *deg_graph,
                                          const DepsgraphEvaluateSyncWriteback sync_writeback)
{
  /* Update the time on the cow scene. */
  if (deg_graph->scene_cow) {
    BKE_scene_frame_set(deg_graph->scene_cow, deg_graph->frame);
  }

  deg::graph_tag_ids_for_visible_update(deg_graph);
  deg::deg_graph_flush_updates(deg_graph);
  deg::deg_evaluate_on_refresh(deg_graph);

  if (sync_writeback == DEG_EVALUATE_SYNC_WRITEBACK_YES) {
    if (deg_graph->is_active) {
      for (std::function<void()> &fn : deg_graph->sync_writeback_callbacks) {
        fn();
      }
    }
  }
  deg_graph->sync_writeback_callbacks.clear();
}

void DEG_evaluate_on_refresh(Depsgraph *graph, const DepsgraphEvaluateSyncWriteback sync_writeback)
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
  else if (scene->id.recalc & ID_RECALC_FRAME_CHANGE) {
    /* Comparing depsgraph & scene frame fails in the case of undo,
     * since the undo state is stored before updates from the frame change have been applied.
     * In this case reading back the undo state will behave as if no updates on frame change
     * is needed as the #Depsgraph.ctime & frame will match the values in the input scene.
     * Use #ID_RECALC_FRAME_CHANGE to detect that recalculation is necessary. see: #66913. */
    deg_graph->tag_time_source();
  }

  deg_flush_updates_and_refresh(deg_graph, sync_writeback);
}

void DEG_evaluate_on_framechange(Depsgraph *graph,
                                 float frame,
                                 const DepsgraphEvaluateSyncWriteback sync_writeback)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(graph);
  const Scene *scene = DEG_get_input_scene(graph);

  deg_graph->tag_time_source();
  deg_graph->frame = frame;
  deg_graph->ctime = BKE_scene_frame_to_ctime(scene, frame);
  deg_flush_updates_and_refresh(deg_graph, sync_writeback);
}
