/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <cstdio>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_collection_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_layer_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_type.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

void DepsgraphNodeBuilder::build_layer_collections(ListBase *lb)
{
  const int visibility_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ? COLLECTION_HIDE_VIEWPORT :
                                                                    COLLECTION_HIDE_RENDER;

  for (LayerCollection *lc = (LayerCollection *)lb->first; lc; lc = lc->next) {
    if (lc->collection->flag & visibility_flag) {
      continue;
    }
    if ((lc->flag & LAYER_COLLECTION_EXCLUDE) == 0) {
      build_collection(lc, lc->collection);
    }
    build_layer_collections(&lc->layer_collections);
  }
}

void DepsgraphNodeBuilder::build_freestyle_lineset(FreestyleLineSet *fls)
{
  if (fls->group != nullptr) {
    build_collection(nullptr, fls->group);
  }
  if (fls->linestyle != nullptr) {
    build_freestyle_linestyle(fls->linestyle);
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

  add_operation_node(&scene->id, NodeType::HIERARCHY, OperationCode::HIERARCHY);

  /* Time source. */
  add_time_source();
  /* Setup currently building context. */
  scene_ = scene;
  view_layer_ = view_layer;
  /* Get pointer to a CoW version of scene ID. */
  Scene *scene_cow = get_cow_datablock(scene);
  /* Scene objects. */
  /* NOTE: Base is used for function bindings as-is, so need to pass CoW base,
   * but object is expected to be an original one. Hence we go into some
   * tricks here iterating over the view layer. */
  int base_index = 0;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    /* object itself */
    if (!need_pull_base_into_graph(base)) {
      continue;
    }

    /* NOTE: We consider object visible even if it's currently
     * restricted by the base/restriction flags. Otherwise its drivers
     * will never be evaluated.
     *
     * TODO(sergey): Need to go more granular on visibility checks. */
    build_object(base_index, base->object, linked_state, true);
    base_index++;

    if (!graph_->has_animated_visibility) {
      graph_->has_animated_visibility |= is_object_visibility_animated(base->object);
    }
  }
  build_layer_collections(&view_layer->layer_collections);
  if (scene->camera != nullptr) {
    build_object(-1, scene->camera, DEG_ID_LINKED_INDIRECTLY, true);
  }
  /* Rigidbody. */
  if (scene->rigidbody_world != nullptr) {
    build_rigidbody(scene);
  }
  /* Scene's animation and drivers. */
  if (scene->adt != nullptr) {
    build_animdata(&scene->id);
  }
  /* World. */
  if (scene->world != nullptr) {
    build_world(scene->world);
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
  if (view_layer->mat_override != nullptr) {
    build_material(view_layer->mat_override);
  }
  /* Freestyle linesets. */
  LISTBASE_FOREACH (FreestyleLineSet *, fls, &view_layer->freestyle_config.linesets) {
    build_freestyle_lineset(fls);
  }
  /* Sequencer. */
  if (linked_state == DEG_ID_LINKED_DIRECTLY) {
    build_scene_audio(scene);
    build_scene_sequencer(scene);
  }
  /* Collections. */
  add_operation_node(&scene->id,
                     NodeType::LAYER_COLLECTIONS,
                     OperationCode::VIEW_LAYER_EVAL,
                     [view_layer_index = view_layer_index_, scene_cow](::Depsgraph *depsgraph) {
                       BKE_layer_eval_view_layer_indexed(depsgraph, scene_cow, view_layer_index);
                     });
  /* Parameters evaluation for scene relations mainly. */
  build_scene_compositor(scene);
  build_scene_parameters(scene);
  /* Build all set scenes. */
  if (scene->set != nullptr) {
    ViewLayer *set_view_layer = BKE_view_layer_default_render(scene->set);
    build_view_layer(scene->set, set_view_layer, DEG_ID_LINKED_VIA_SET);
  }
}

}  // namespace blender::deg
