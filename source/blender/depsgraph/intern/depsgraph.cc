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

#include "BLI_console.h"
#include "BLI_hash.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"

#include "intern/depsgraph_physics.h"
#include "intern/depsgraph_registry.h"
#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_update.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

namespace DEG {

Depsgraph::Depsgraph(Main *bmain, Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
    : time_source(nullptr),
      need_update(true),
      need_update_time(false),
      bmain(bmain),
      scene(scene),
      view_layer(view_layer),
      mode(mode),
      ctime(BKE_scene_frame_get(scene)),
      scene_cow(nullptr),
      is_active(false),
      is_evaluating(false),
      is_render_pipeline_depsgraph(false)
{
  BLI_spin_init(&lock);
  memset(id_type_updated, 0, sizeof(id_type_updated));
  memset(id_type_exist, 0, sizeof(id_type_exist));
  memset(physics_relations, 0, sizeof(physics_relations));
}

Depsgraph::~Depsgraph()
{
  clear_id_nodes();
  if (time_source != nullptr) {
    OBJECT_GUARDED_DELETE(time_source, TimeSourceNode);
  }
  BLI_spin_end(&lock);
}

/* Node Management ---------------------------- */

TimeSourceNode *Depsgraph::add_time_source()
{
  if (time_source == nullptr) {
    DepsNodeFactory *factory = type_get_factory(NodeType::TIMESOURCE);
    time_source = (TimeSourceNode *)factory->create_node(nullptr, "", "Time Source");
  }
  return time_source;
}

TimeSourceNode *Depsgraph::find_time_source() const
{
  return time_source;
}

IDNode *Depsgraph::find_id_node(const ID *id) const
{
  return id_hash.lookup_default(id, nullptr);
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
    id_hash.add_new(id, id_node);
    id_nodes.append(id_node);

    id_type_exist[BKE_idtype_idcode_to_index(GS(id->name))] = 1;
  }
  return id_node;
}

void Depsgraph::clear_id_nodes_conditional(const std::function<bool(ID_Type id_type)> &filter)
{
  for (IDNode *id_node : id_nodes) {
    if (id_node->id_cow == nullptr) {
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
  id_hash.clear();
  id_nodes.clear();
  /* Clear physics relation caches. */
  clear_physics_relations(this);
}

/* Add new relation between two nodes */
Relation *Depsgraph::add_new_relation(Node *from, Node *to, const char *description, int flags)
{
  Relation *rel = nullptr;
  if (flags & RELATION_CHECK_BEFORE_ADD) {
    rel = check_nodes_connected(from, to, description);
  }
  if (rel != nullptr) {
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
    if (description != nullptr && !STREQ(rel->name, description)) {
      continue;
    }
    return rel;
  }
  return nullptr;
}

/* Low level tagging -------------------------------------- */

/* Tag a specific node as needing updates. */
void Depsgraph::add_entry_tag(OperationNode *node)
{
  /* Sanity check. */
  if (node == nullptr) {
    return;
  }
  /* Add to graph-level set of directly modified nodes to start searching
   * from.
   * NOTE: this is necessary since we have several thousand nodes to play
   * with. */
  entry_tags.add(node);
}

void Depsgraph::clear_all_nodes()
{
  clear_id_nodes();
  if (time_source != nullptr) {
    OBJECT_GUARDED_DELETE(time_source, TimeSourceNode);
    time_source = nullptr;
  }
}

ID *Depsgraph::get_cow_id(const ID *id_orig) const
{
  IDNode *id_node = find_id_node(id_orig);
  if (id_node == nullptr) {
    /* This function is used from places where we expect ID to be either
     * already a copy-on-write version or have a corresponding copy-on-write
     * version.
     *
     * We try to enforce that in debug builds, for release we play a bit
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
Depsgraph *DEG_graph_new(Main *bmain, Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
{
  DEG::Depsgraph *deg_depsgraph = OBJECT_GUARDED_NEW(
      DEG::Depsgraph, bmain, scene, view_layer, mode);
  DEG::register_graph(deg_depsgraph);
  return reinterpret_cast<Depsgraph *>(deg_depsgraph);
}

/* Replace the "owner" pointers (currently Main/Scene/ViewLayer) of this depsgraph.
 * Used during undo steps when we do want to re-use the old depsgraph data as much as possible. */
void DEG_graph_replace_owners(struct Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              ViewLayer *view_layer)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);

  const bool do_update_register = deg_graph->bmain != bmain;
  if (do_update_register && deg_graph->bmain != NULL) {
    DEG::unregister_graph(deg_graph);
  }

  deg_graph->bmain = bmain;
  deg_graph->scene = scene;
  deg_graph->view_layer = view_layer;

  if (do_update_register) {
    DEG::register_graph(deg_graph);
  }
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
  if (graph == nullptr) {
    return;
  }
  using DEG::Depsgraph;
  DEG::Depsgraph *deg_depsgraph = reinterpret_cast<DEG::Depsgraph *>(graph);
  DEG::unregister_graph(deg_depsgraph);
  OBJECT_GUARDED_DELETE(deg_depsgraph, Depsgraph);
}

bool DEG_is_evaluating(const struct Depsgraph *depsgraph)
{
  const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(depsgraph);
  return deg_graph->is_evaluating;
}

bool DEG_is_active(const struct Depsgraph *depsgraph)
{
  if (depsgraph == nullptr) {
    /* Happens for such cases as work object in what_does_obaction(),
     * and sine render pipeline parts. Shouldn't really be accepting
     * nullptr depsgraph, but is quite hard to get proper one in those
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
