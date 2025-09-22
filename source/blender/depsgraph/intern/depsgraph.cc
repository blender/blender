/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/depsgraph.hh" /* own include */

#include <cstring>
#include <type_traits>

#include "BLI_utildefines.h"

#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_debug.hh"

#include "intern/depsgraph_physics.hh"
#include "intern/depsgraph_registry.hh"
#include "intern/depsgraph_relation.hh"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/node/deg_node.hh"
#include "intern/node/deg_node_component.hh"
#include "intern/node/deg_node_factory.hh"
#include "intern/node/deg_node_id.hh"
#include "intern/node/deg_node_operation.hh"
#include "intern/node/deg_node_time.hh"

namespace deg = blender::deg;

namespace blender::deg {

Depsgraph::Depsgraph(Main *bmain, Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
    : time_source(nullptr),
      has_animated_visibility(false),
      need_update_relations(true),
      need_update_nodes_visibility(true),
      need_tag_id_on_graph_visibility_update(true),
      need_tag_id_on_graph_visibility_time_update(false),
      bmain(bmain),
      scene(scene),
      view_layer(view_layer),
      mode(mode),
      frame(BKE_scene_frame_get(scene)),
      ctime(BKE_scene_ctime_get(scene)),
      scene_cow(nullptr),
      is_active(false),
      use_visibility_optimization(true),
      is_evaluating(false),
      is_render_pipeline_depsgraph(false),
      use_editors_update(false),
      update_count(0),
      sync_writeback(DEG_EVALUATE_SYNC_WRITEBACK_NO)
{
  BLI_spin_init(&lock);
  memset(id_type_updated, 0, sizeof(id_type_updated));
  memset(id_type_updated_backup, 0, sizeof(id_type_updated_backup));
  memset(id_type_exist, 0, sizeof(id_type_exist));
  memset(physics_relations, 0, sizeof(physics_relations));

  add_time_source();
}

Depsgraph::~Depsgraph()
{
  clear_id_nodes();
  delete time_source;
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

void Depsgraph::tag_time_source()
{
  time_source->tag_update(this, DEG_UPDATE_SOURCE_TIME);
}

IDNode *Depsgraph::find_id_node(const ID *id) const
{
  return id_hash.lookup_default(id, nullptr);
}

IDNode *Depsgraph::add_id_node(ID *id, ID *id_cow_hint)
{
  BLI_assert((id->tag & ID_TAG_COPIED_ON_EVAL) == 0);
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

template<typename FilterFunc>
static void clear_id_nodes_conditional(Depsgraph::IDDepsNodes *id_nodes, const FilterFunc &filter)
{
  for (IDNode *id_node : *id_nodes) {
    if (id_node->id_cow == nullptr) {
      /* This means builder "stole" ownership of the evaluated
       * datablock for its own dirty needs. */
      continue;
    }
    if (id_node->id_cow == id_node->id_orig) {
      /* Evaluated copy is not needed for this ID type.
       *
       * NOTE: Is important to not de-reference the original datablock here because it might be
       * freed already (happens during main database free when some IDs are freed prior to a
       * scene). */
      continue;
    }
    if (!deg_eval_copy_is_expanded(id_node->id_cow)) {
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
  clear_id_nodes_conditional(&id_nodes, [](ID_Type id_type) { return id_type == ID_SCE; });
  clear_id_nodes_conditional(&id_nodes, [](ID_Type id_type) { return id_type != ID_PA; });

  for (IDNode *id_node : id_nodes) {
    delete id_node;
  }
  /* Clear containers. */
  id_hash.clear();
  id_nodes.clear();
  /* Clear physics relation caches. */
  clear_physics_relations(this);

  light_linking_cache.clear();
}

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
    BLI_assert(operation_to->owner->type != NodeType::COPY_ON_EVAL ||
               operation_from->owner->type == NodeType::COPY_ON_EVAL);
  }
#endif

  /* Create new relation, and add it to the graph. The type must be trivially destructible for
   * `.release()` to be okay. If it weren't, we could store the relations with #destruct_ptr on
   * either the `inlinks` or `outlinks`. But since so many #Relation structs are allocated, it's
   * probably better for it be a simple type anyway. */
  static_assert(std::is_trivially_destructible_v<Relation>);
  rel = this->build_allocator.construct<Relation>(from, to, description).release();
  from->outlinks.append(rel);
  to->inlinks.append(rel);
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
  delete time_source;
  time_source = nullptr;
  /* Memory used by the build allocator is now unused. Rebuild it from scratch. */
  std::destroy_at(&this->build_allocator);
  new (&this->build_allocator) LinearAllocator<>();
}

ID *Depsgraph::get_cow_id(const ID *id_orig) const
{
  IDNode *id_node = find_id_node(id_orig);
  if (id_node == nullptr) {
    /* This function is used from places where we expect ID to be either
     * already a copy-on-evaluation version or have a corresponding copy-on-evaluation
     * version.
     *
     * We try to enforce that in debug builds, for release we play a bit
     * safer game here. */
    if ((id_orig->tag & ID_TAG_COPIED_ON_EVAL) == 0) {
      /* TODO(sergey): This is nice sanity check to have, but it fails
       * in following situations:
       *
       * - Material has link to texture, which is not needed by new
       *   shading system and hence can be ignored at construction.
       * - Object or mesh has material at a slot which is not used (for
       *   example, object has material slot by materials are set to
       *   object data). */
      // BLI_assert_msg(0, "Request for non-existing copy-on-evaluation ID");
    }
    return (ID *)id_orig;
  }
  return id_node->id_cow;
}

}  // namespace blender::deg

/* **************** */
/* Public Graph API */

Depsgraph *DEG_graph_new(Main *bmain, Scene *scene, ViewLayer *view_layer, eEvaluationMode mode)
{
  deg::Depsgraph *deg_depsgraph = new deg::Depsgraph(bmain, scene, view_layer, mode);
  deg::register_graph(deg_depsgraph);
  return reinterpret_cast<Depsgraph *>(deg_depsgraph);
}

void DEG_graph_replace_owners(Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              ViewLayer *view_layer)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);

  const bool do_update_register = deg_graph->bmain != bmain;
  if (do_update_register && deg_graph->bmain != nullptr) {
    deg::unregister_graph(deg_graph);
  }

  deg_graph->bmain = bmain;
  deg_graph->scene = scene;
  deg_graph->view_layer = view_layer;

  if (do_update_register) {
    deg::register_graph(deg_graph);
  }
}

void DEG_graph_free(Depsgraph *graph)
{
  if (graph == nullptr) {
    return;
  }
  using deg::Depsgraph;
  deg::Depsgraph *deg_depsgraph = reinterpret_cast<deg::Depsgraph *>(graph);
  deg::unregister_graph(deg_depsgraph);
  delete deg_depsgraph;
}

bool DEG_is_evaluating(const Depsgraph *depsgraph)
{
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(depsgraph);
  return deg_graph->is_evaluating;
}

bool DEG_is_active(const Depsgraph *depsgraph)
{
  if (depsgraph == nullptr) {
    /* Happens for such cases as work object in what_does_obaction(),
     * and sine render pipeline parts. Shouldn't really be accepting
     * nullptr depsgraph, but is quite hard to get proper one in those
     * cases. */
    return false;
  }
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(depsgraph);
  return deg_graph->is_active;
}

void DEG_make_active(Depsgraph *depsgraph)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);
  deg_graph->is_active = true;
  /* TODO(sergey): Copy data from evaluated state to original. */
}

void DEG_make_inactive(Depsgraph *depsgraph)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);
  deg_graph->is_active = false;
}

void DEG_disable_visibility_optimization(Depsgraph *depsgraph)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);
  deg_graph->use_visibility_optimization = false;
}

uint64_t DEG_get_update_count(const Depsgraph *depsgraph)
{
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(depsgraph);
  return deg_graph->update_count;
}
