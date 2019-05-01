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
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_string.h"

extern "C" {
#include "DNA_freestyle_types.h"
#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/depsgraph.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"
#include "intern/depsgraph_type.h"

namespace DEG {

void DepsgraphNodeBuilder::build_layer_collections(ListBase *lb)
{
  const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ? COLLECTION_RESTRICT_VIEW :
                                                                  COLLECTION_RESTRICT_RENDER;

  for (LayerCollection *lc = (LayerCollection *)lb->first; lc; lc = lc->next) {
    if (lc->collection->flag & restrict_flag) {
      continue;
    }
    if ((lc->flag & LAYER_COLLECTION_EXCLUDE) == 0) {
      build_collection(lc, lc->collection);
    }
    build_layer_collections(&lc->layer_collections);
  }
}

void DepsgraphNodeBuilder::build_view_layer(Scene *scene,
                                            ViewLayer *view_layer,
                                            eDepsNode_LinkedState_Type linked_state)
{
  /* NOTE: Pass view layer index of 0 since after scene CoW there is
   * only one view layer in there. */
  view_layer_index_ = 0;
  /* Scene ID block. */
  IDNode *id_node = add_id_node(&scene->id);
  id_node->linked_state = linked_state;
  /* Time source. */
  add_time_source();
  /* Setup currently building context. */
  scene_ = scene;
  view_layer_ = view_layer;
  /* Get pointer to a CoW version of scene ID. */
  Scene *scene_cow = get_cow_datablock(scene);
  /* Scene objects. */
  int select_id = 1;
  /* NOTE: Base is used for function bindings as-is, so need to pass CoW base,
   * but object is expected to be an original one. Hence we go into some
   * tricks here iterating over the view layer. */
  int base_index = 0;
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    /* object itself */
    if (need_pull_base_into_graph(base)) {
      /* NOTE: We consider object visible even if it's currently
       * restricted by the base/restriction flags. Otherwise its drivers
       * will never be evaluated.
       *
       * TODO(sergey): Need to go more granular on visibility checks. */
      build_object(base_index, base->object, linked_state, true);
      ++base_index;
    }
    base->object->select_id = select_id++;
  }
  build_layer_collections(&view_layer->layer_collections);
  if (scene->camera != NULL) {
    build_object(-1, scene->camera, DEG_ID_LINKED_INDIRECTLY, true);
  }
  /* Rigidbody. */
  if (scene->rigidbody_world != NULL) {
    build_rigidbody(scene);
  }
  /* Scene's animation and drivers. */
  if (scene->adt != NULL) {
    build_animdata(&scene->id);
  }
  /* World. */
  if (scene->world != NULL) {
    build_world(scene->world);
  }
  /* Compositor nodes */
  if (scene->nodetree != NULL) {
    build_compositor(scene);
  }
  /* Cache file. */
  LISTBASE_FOREACH (CacheFile *, cachefile, &bmain_->cachefiles) {
    build_cachefile(cachefile);
  }
  /* Masks. */
  LISTBASE_FOREACH (Mask *, mask, &bmain_->masks) {
    build_mask(mask);
  }
  /* Movie clips. */
  LISTBASE_FOREACH (MovieClip *, clip, &bmain_->movieclips) {
    build_movieclip(clip);
  }
  /* Material override. */
  if (view_layer->mat_override != NULL) {
    build_material(view_layer->mat_override);
  }
  /* Freestyle collections. */
  LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
    if (fls->group != NULL) {
      build_collection(NULL, fls->group);
    }
  }
  /* Sequencer. */
  if (linked_state == DEG_ID_LINKED_DIRECTLY) {
    build_scene_audio(scene);
    build_sequencer(scene);
  }
  /* Collections. */
  add_operation_node(
      &scene->id,
      NodeType::LAYER_COLLECTIONS,
      OperationCode::VIEW_LAYER_EVAL,
      function_bind(BKE_layer_eval_view_layer_indexed, _1, scene_cow, view_layer_index_));
  /* Parameters evaluation for scene relations mainly. */
  add_operation_node(&scene->id, NodeType::PARAMETERS, OperationCode::SCENE_EVAL);
  /* Build all set scenes. */
  if (scene->set != NULL) {
    ViewLayer *set_view_layer = BKE_view_layer_default_render(scene->set);
    build_view_layer(scene->set, set_view_layer, DEG_ID_LINKED_VIA_SET);
  }
}

}  // namespace DEG
