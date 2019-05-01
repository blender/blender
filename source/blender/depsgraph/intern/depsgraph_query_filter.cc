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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of Graph Filtering API
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include <string.h>  // XXX: memcpy

#include "BLI_utildefines.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "BKE_action.h"  // XXX: BKE_pose_channel_from_name
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DEG_depsgraph_debug.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_type.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

/* *************************************************** */
/* Graph Filtering Internals */

namespace DEG {

/* UserData for deg_add_retained_id_cb */
struct RetainedIdUserData {
  DEG_FilterQuery *query;
  GSet *set;
};

/* Helper for DEG_foreach_ancestor_id()
 * Keep track of all ID's encountered in a set
 */
static void deg_add_retained_id_cb(ID *id, void *user_data)
{
  RetainedIdUserData *data = (RetainedIdUserData *)user_data;
  BLI_gset_add(data->set, (void *)id);
}

/* ------------------------------------------- */

/* Remove relations pointing to the given OperationNode */
/* TODO: Make this part of OperationNode? */
static void deg_unlink_opnode(Depsgraph *graph, OperationNode *op_node)
{
  vector<Relation *> all_links;

  /* Collect all inlinks to this operation */
  for (Relation *rel : op_node->inlinks) {
    all_links.push_back(rel);
  }
  /* Collect all outlinks from this operation */
  for (Relation *rel : op_node->outlinks) {
    all_links.push_back(rel);
  }

  /* Delete all collected relations */
  for (Relation *rel : all_links) {
    rel->unlink();
    OBJECT_GUARDED_DELETE(rel, Relation);
  }

  /* Remove from entry tags */
  if (BLI_gset_haskey(graph->entry_tags, op_node)) {
    BLI_gset_remove(graph->entry_tags, op_node, NULL);
  }
}

/* Remove every ID Node (and its associated subnodes, COW data) */
static void deg_filter_remove_unwanted_ids(Depsgraph *graph, GSet *retained_ids)
{
  /* 1) First pass over ID nodes + their operations
   * - Identify and tag ID's (via "custom_flags = 1") to be removed
   * - Remove all links to/from operations that will be removed. */
  for (IDNode *id_node : graph->id_nodes) {
    id_node->custom_flags = !BLI_gset_haskey(retained_ids, (void *)id_node->id_orig);
    if (id_node->custom_flags) {
      GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
        for (OperationNode *op_node : comp_node->operations) {
          deg_unlink_opnode(graph, op_node);
        }
      }
      GHASH_FOREACH_END();
    }
  }

  /* 2) Remove unwanted operations from graph->operations */
  for (Depsgraph::OperationNodes::iterator it_opnode = graph->operations.begin();
       it_opnode != graph->operations.end();) {
    OperationNode *op_node = *it_opnode;
    IDNode *id_node = op_node->owner->owner;
    if (id_node->custom_flags) {
      it_opnode = graph->operations.erase(it_opnode);
    }
    else {
      ++it_opnode;
    }
  }

  /* Free ID nodes that are no longer wanted
   *
   * This is loosely based on Depsgraph::clear_id_nodes().
   * However, we don't worry about the conditional freeing for physics
   * stuff, since it's rarely needed currently. */
  for (Depsgraph::IDDepsNodes::iterator it_id = graph->id_nodes.begin();
       it_id != graph->id_nodes.end();) {
    IDNode *id_node = *it_id;
    ID *id = id_node->id_orig;

    if (id_node->custom_flags) {
      /* Destroy node data, then remove from collections, and free */
      id_node->destroy();

      BLI_ghash_remove(graph->id_hash, id, NULL, NULL);
      it_id = graph->id_nodes.erase(it_id);

      OBJECT_GUARDED_DELETE(id_node, IDNode);
    }
    else {
      /* This node has not been marked for deletion. Increment iterator */
      ++it_id;
    }
  }
}

}  // namespace DEG

/* *************************************************** */
/* Graph Filtering API */

/* Obtain a new graph instance that only contains the subset of desired nodes
 * WARNING: Do NOT pass an already filtered depsgraph through this function again,
 *          as we are currently unable to accurately recreate it.
 */
Depsgraph *DEG_graph_filter(const Depsgraph *graph_src, Main *bmain, DEG_FilterQuery *query)
{
  const DEG::Depsgraph *deg_graph_src = reinterpret_cast<const DEG::Depsgraph *>(graph_src);
  if (deg_graph_src == NULL) {
    return NULL;
  }

  /* Construct a full new depsgraph based on the one we got */
  /* TODO: Improve the builders to not add any ID nodes we don't need later (e.g. ProxyBuilder?) */
  Depsgraph *graph_new = DEG_graph_new(
      deg_graph_src->scene, deg_graph_src->view_layer, deg_graph_src->mode);
  DEG_graph_build_from_view_layer(
      graph_new, bmain, deg_graph_src->scene, deg_graph_src->view_layer);

  /* Build a set of all the id's we want to keep */
  GSet *retained_ids = BLI_gset_ptr_new(__func__);
  DEG::RetainedIdUserData retained_id_data = {query, retained_ids};

  LISTBASE_FOREACH (DEG_FilterTarget *, target, &query->targets) {
    /* Target Itself */
    BLI_gset_add(retained_ids, (void *)target->id);

    /* Target's Ancestors (i.e. things it depends on) */
    DEG_foreach_ancestor_ID(graph_new, target->id, DEG::deg_add_retained_id_cb, &retained_id_data);
  }

  /* Remove everything we don't want to keep around anymore */
  DEG::Depsgraph *deg_graph_new = reinterpret_cast<DEG::Depsgraph *>(graph_new);
  if (BLI_gset_len(retained_ids) > 0) {
    DEG::deg_filter_remove_unwanted_ids(deg_graph_new, retained_ids);
  }
  // TODO: query->LOD filters

  /* Free temp data */
  BLI_gset_free(retained_ids, NULL);
  retained_ids = NULL;

  /* Print Stats */
  // XXX: Hide behind debug flags
  size_t s_outer, s_operations, s_relations;
  size_t s_ids = deg_graph_src->id_nodes.size();
  unsigned int s_idh = BLI_ghash_len(deg_graph_src->id_hash);

  size_t n_outer, n_operations, n_relations;
  size_t n_ids = deg_graph_new->id_nodes.size();
  unsigned int n_idh = BLI_ghash_len(deg_graph_new->id_hash);

  DEG_stats_simple(graph_src, &s_outer, &s_operations, &s_relations);
  DEG_stats_simple(graph_new, &n_outer, &n_operations, &n_relations);

  printf("%s: src = (ID's: %zu (%u), Out: %zu, Op: %zu, Rel: %zu)\n",
         __func__,
         s_ids,
         s_idh,
         s_outer,
         s_operations,
         s_relations);
  printf("%s: new = (ID's: %zu (%u), Out: %zu, Op: %zu, Rel: %zu)\n",
         __func__,
         n_ids,
         n_idh,
         n_outer,
         n_operations,
         n_relations);

  /* Return this new graph instance */
  return graph_new;
}

/* *************************************************** */
