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
 * Core routines for how the Depsgraph works.
 */

#include "intern/depsgraph.h" /* own include */

#include <algorithm>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_console.h"
#include "BLI_hash.h"
#include "BLI_ghash.h"

extern "C" {
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
}

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"

#include "intern/depsgraph_update.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

#include "intern/depsgraph_physics.h"

namespace DEG {

/* TODO(sergey): Find a better place for this. */
template<typename T> static void remove_from_vector(vector<T> *vector, const T &value)
{
  vector->erase(std::remove(vector->begin(), vector->end(), value), vector->end());
}

Depsgraph::Depsgraph(Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
    : time_source(NULL),
      need_update(true),
      scene(scene),
      view_layer(view_layer),
      mode(mode),
      ctime(BKE_scene_frame_get(scene)),
      scene_cow(NULL),
      is_active(false),
      debug_is_evaluating(false),
      is_render_pipeline_depsgraph(false)
{
  BLI_spin_init(&lock);
  id_hash = BLI_ghash_ptr_new("Depsgraph id hash");
  entry_tags = BLI_gset_ptr_new("Depsgraph entry_tags");
  debug_flags = G.debug;
  memset(id_type_updated, 0, sizeof(id_type_updated));
  memset(id_type_exist, 0, sizeof(id_type_exist));
  memset(physics_relations, 0, sizeof(physics_relations));
}

Depsgraph::~Depsgraph()
{
  clear_id_nodes();
  BLI_ghash_free(id_hash, NULL, NULL);
  BLI_gset_free(entry_tags, NULL);
  if (time_source != NULL) {
    OBJECT_GUARDED_DELETE(time_source, TimeSourceNode);
  }
  BLI_spin_end(&lock);
}

/* Node Management ---------------------------- */

TimeSourceNode *Depsgraph::add_time_source()
{
  if (time_source == NULL) {
    DepsNodeFactory *factory = type_get_factory(NodeType::TIMESOURCE);
    time_source = (TimeSourceNode *)factory->create_node(NULL, "", "Time Source");
  }
  return time_source;
}

TimeSourceNode *Depsgraph::find_time_source() const
{
  return time_source;
}

IDNode *Depsgraph::find_id_node(const ID *id) const
{
  return reinterpret_cast<IDNode *>(BLI_ghash_lookup(id_hash, id));
}

IDNode *Depsgraph::add_id_node(ID *id, ID *id_cow_hint)
{
  BLI_assert((id->tag & LIB_TAG_COPIED_ON_WRITE) == 0);
  IDNode *id_node = find_id_node(id);
  if (!id_node) {
    DepsNodeFactory *factory = type_get_factory(NodeType::ID_REF);
    id_node = (IDNode *)factory->create_node(id, "", id->name);
    id_node->init_copy_on_write(id_cow_hint);
    /* Register node in ID hash.
     *
     * NOTE: We address ID nodes by the original ID pointer they are
     * referencing to. */
    BLI_ghash_insert(id_hash, id, id_node);
    id_nodes.push_back(id_node);

    id_type_exist[BKE_idcode_to_index(GS(id->name))] = 1;
  }
  return id_node;
}

void Depsgraph::clear_id_nodes_conditional(const std::function<bool(ID_Type id_type)> &filter)
{
  for (IDNode *id_node : id_nodes) {
    if (id_node->id_cow == NULL) {
      /* This means builder "stole" ownership of the copy-on-written
       * datablock for her own dirty needs. */
      continue;
    }
    if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
      continue;
    }
    const ID_Type id_type = GS(id_node->id_cow->name);
    if (filter(id_type)) {
      id_node->destroy();
    }
  }
}

void Depsgraph::clear_id_nodes()
{
  /* Free memory used by ID nodes. */

  /* Stupid workaround to ensure we free IDs in a proper order. */
  clear_id_nodes_conditional([](ID_Type id_type) { return id_type == ID_SCE; });
  clear_id_nodes_conditional([](ID_Type id_type) { return id_type != ID_PA; });

  for (IDNode *id_node : id_nodes) {
    OBJECT_GUARDED_DELETE(id_node, IDNode);
  }
  /* Clear containers. */
  BLI_ghash_clear(id_hash, NULL, NULL);
  id_nodes.clear();
  /* Clear physics relation caches. */
  clear_physics_relations(this);
}

/* Add new relation between two nodes */
Relation *Depsgraph::add_new_relation(Node *from, Node *to, const char *description, int flags)
{
  Relation *rel = NULL;
  if (flags & RELATION_CHECK_BEFORE_ADD) {
    rel = check_nodes_connected(from, to, description);
  }
  if (rel != NULL) {
    rel->flag |= flags;
    return rel;
  }

#ifndef NDEBUG
  if (from->type == NodeType::OPERATION && to->type == NodeType::OPERATION) {
    OperationNode *operation_from = static_cast<OperationNode *>(from);
    OperationNode *operation_to = static_cast<OperationNode *>(to);
    BLI_assert(operation_to->owner->type != NodeType::COPY_ON_WRITE ||
               operation_from->owner->type == NodeType::COPY_ON_WRITE);
  }
#endif

  /* Create new relation, and add it to the graph. */
  rel = OBJECT_GUARDED_NEW(Relation, from, to, description);
  rel->flag |= flags;
  return rel;
}

Relation *Depsgraph::check_nodes_connected(const Node *from,
                                           const Node *to,
                                           const char *description)
{
  for (Relation *rel : from->outlinks) {
    BLI_assert(rel->from == from);
    if (rel->to != to) {
      continue;
    }
    if (description != NULL && !STREQ(rel->name, description)) {
      continue;
    }
    return rel;
  }
  return NULL;
}

/* ************************ */
/* Relationships Management */

Relation::Relation(Node *from, Node *to, const char *description)
    : from(from), to(to), name(description), flag(0)
{
  /* Hook it up to the nodes which use it.
   *
   * NOTE: We register relation in the nodes which this link connects to here
   * in constructor but we don't unregister it in the destructor.
   *
   * Reasoning:
   *
   * - Destructor is currently used on global graph destruction, so there's no
   *   real need in avoiding dangling pointers, all the memory is to be freed
   *   anyway.
   *
   * - Unregistering relation is not a cheap operation, so better to have it
   *   as an explicit call if we need this. */
  from->outlinks.push_back(this);
  to->inlinks.push_back(this);
}

Relation::~Relation()
{
  /* Sanity check. */
  BLI_assert(from != NULL && to != NULL);
}

void Relation::unlink()
{
  /* Sanity check. */
  BLI_assert(from != NULL && to != NULL);
  remove_from_vector(&from->outlinks, this);
  remove_from_vector(&to->inlinks, this);
}

/* Low level tagging -------------------------------------- */

/* Tag a specific node as needing updates. */
void Depsgraph::add_entry_tag(OperationNode *node)
{
  /* Sanity check. */
  if (node == NULL) {
    return;
  }
  /* Add to graph-level set of directly modified nodes to start searching
   * from.
   * NOTE: this is necessary since we have several thousand nodes to play
   * with. */
  BLI_gset_insert(entry_tags, node);
}

void Depsgraph::clear_all_nodes()
{
  clear_id_nodes();
  if (time_source != NULL) {
    OBJECT_GUARDED_DELETE(time_source, TimeSourceNode);
    time_source = NULL;
  }
}

ID *Depsgraph::get_cow_id(const ID *id_orig) const
{
  IDNode *id_node = find_id_node(id_orig);
  if (id_node == NULL) {
    /* This function is used from places where we expect ID to be either
     * already a copy-on-write version or have a corresponding copy-on-write
     * version.
     *
     * We try to enforce that in debug builds, for for release we play a bit
     * safer game here. */
    if ((id_orig->tag & LIB_TAG_COPIED_ON_WRITE) == 0) {
      /* TODO(sergey): This is nice sanity check to have, but it fails
       * in following situations:
       *
       * - Material has link to texture, which is not needed by new
       *   shading system and hence can be ignored at construction.
       * - Object or mesh has material at a slot which is not used (for
       *   example, object has material slot by materials are set to
       *   object data). */
      // BLI_assert(!"Request for non-existing copy-on-write ID");
    }
    return (ID *)id_orig;
  }
  return id_node->id_cow;
}

}  // namespace DEG

/* **************** */
/* Public Graph API */

/* Initialize a new Depsgraph */
Depsgraph *DEG_graph_new(Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
{
  DEG::Depsgraph *deg_depsgraph = OBJECT_GUARDED_NEW(DEG::Depsgraph, scene, view_layer, mode);
  return reinterpret_cast<Depsgraph *>(deg_depsgraph);
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
  using DEG::Depsgraph;
  DEG::Depsgraph *deg_depsgraph = reinterpret_cast<DEG::Depsgraph *>(graph);
  OBJECT_GUARDED_DELETE(deg_depsgraph, Depsgraph);
}

bool DEG_is_active(const struct Depsgraph *depsgraph)
{
  if (depsgraph == NULL) {
    /* Happens for such cases as work object in what_does_obaction(),
     * and sine render pipeline parts. Shouldn't really be accepting
     * NULL depsgraph, but is quite hard to get proper one in those
     * cases. */
    return false;
  }
  const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(depsgraph);
  return deg_graph->is_active;
}

void DEG_make_active(struct Depsgraph *depsgraph)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  deg_graph->is_active = true;
  /* TODO(sergey): Copy data from evaluated state to original. */
}

void DEG_make_inactive(struct Depsgraph *depsgraph)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  deg_graph->is_active = false;
}
