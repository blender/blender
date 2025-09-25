/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node_id.hh"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "BKE_lib_id.hh"

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node_component.hh"
#include "intern/node/deg_node_factory.hh"

namespace blender::deg {

const char *linkedStateAsString(eDepsNode_LinkedState_Type linked_state)
{
  switch (linked_state) {
    case DEG_ID_LINKED_INDIRECTLY:
      return "INDIRECTLY";
    case DEG_ID_LINKED_VIA_SET:
      return "VIA_SET";
    case DEG_ID_LINKED_DIRECTLY:
      return "DIRECTLY";
  }
  BLI_assert_msg(0, "Unhandled linked state, should never happen.");
  return "UNKNOWN";
}

void IDNode::init(const ID *id, const char * /*subdata*/)
{
  BLI_assert(id != nullptr);
  /* Store ID-pointer. */
  id_type = GS(id->name);
  id_orig = (ID *)id;
  id_orig_session_uid = id->session_uid;
  eval_flags = 0;
  previous_eval_flags = 0;
  customdata_masks = DEGCustomDataMeshMasks();
  previous_customdata_masks = DEGCustomDataMeshMasks();
  linked_state = DEG_ID_LINKED_INDIRECTLY;
  is_visible_on_build = true;
  is_enabled_on_eval = true;
  is_collection_fully_expanded = false;
  has_base = false;
  is_user_modified = false;
  id_cow_recalc_backup = 0;

  visible_components_mask = 0;
  previously_visible_components_mask = 0;
}

void IDNode::init_copy_on_write(ID *id_cow_hint)
{
  /* Create pointer as early as possible, so we can use it for function
   * bindings. Rest of data we'll be copying to the new datablock when
   * it is actually needed. */
  if (id_cow_hint != nullptr) {
    // BLI_assert(deg_eval_copy_is_needed(id_orig));
    if (deg_eval_copy_is_needed(id_orig)) {
      id_cow = id_cow_hint;

      /* While `id_cow->orig_id == id` should be `true` most of the time (a same 'orig' ID should
       * keep a same pointer in most cases), in can happen that the same 'orig' ID got a new
       * address, e.g. after being deleted and re-loaded from memfile undo, without any update of
       * the depsgraph in-between (e.g. when the depsgraph belongs to an inactive viewlayer).
       * Ref. #145848. */
      id_cow->orig_id = this->id_orig;
    }
    else {
      id_cow = id_orig;
    }
  }
  else if (deg_eval_copy_is_needed(id_orig)) {
    id_cow = BKE_libblock_alloc_notest(GS(id_orig->name));
    /* No need to call #BKE_libblock_runtime_ensure here, this will be done by
     * #BKE_libblock_copy_in_lib when #deg_expand_eval_copy_datablock is executed. */
    DEG_COW_PRINT(
        "Allocate memory for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
  }
  else {
    id_cow = id_orig;
  }
}

/* Free 'id' node. */
IDNode::~IDNode()
{
  destroy();
}

void IDNode::destroy()
{
  if (id_orig == nullptr) {
    return;
  }

  /* Free memory used by this evaluated ID. */
  if (!ELEM(id_cow, id_orig, nullptr)) {
    deg_free_eval_copy_datablock(id_cow);
    MEM_freeN(id_cow);
    id_cow = nullptr;
    DEG_COW_PRINT(
        "Destroy evaluated ID for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
  }

  for (ComponentNode *comp_node : components.values()) {
    delete comp_node;
  }

  /* Tag that the node is freed. */
  id_orig = nullptr;
}

std::string IDNode::identifier() const
{
  char orig_ptr[24], cow_ptr[24];
  SNPRINTF(orig_ptr, "%p", id_orig);
  SNPRINTF(cow_ptr, "%p", id_cow);
  return std::string(nodeTypeAsString(type)) + " : " + name + " (orig: " + orig_ptr +
         ", eval: " + cow_ptr + ", is_visible_on_build " +
         (is_visible_on_build ? "true" : "false") + ")";
}

ComponentNode *IDNode::find_component(NodeType type, const StringRef name) const
{
  ComponentIDKey key(type, name);
  return components.lookup_default(key, nullptr);
}

ComponentNode *IDNode::add_component(NodeType type, const StringRef name)
{
  ComponentNode *comp_node = find_component(type, name);
  if (!comp_node) {
    DepsNodeFactory *factory = type_get_factory(type);
    BLI_assert(factory);
    comp_node = (ComponentNode *)factory->create_node(this->id_orig, "", name);

    /* Register. */
    ComponentIDKey key(type, comp_node->name);
    components.add_new(key, comp_node);
    comp_node->owner = this;
  }
  return comp_node;
}

void IDNode::tag_update(Depsgraph *graph, eUpdateSource source)
{
  for (ComponentNode *comp_node : components.values()) {
    /* Relations update does explicit animation update when needed. Here we ignore animation
     * component to avoid loss of possible unkeyed changes. */
    if (comp_node->type == NodeType::ANIMATION && source == DEG_UPDATE_SOURCE_RELATIONS) {
      continue;
    }
    comp_node->tag_update(graph, source);
  }
}

void IDNode::finalize_build(Depsgraph *graph)
{
  /* Finalize build of all components. */
  for (ComponentNode *comp_node : components.values()) {
    comp_node->finalize_build(graph);
  }
  visible_components_mask = get_visible_components_mask();
}

IDComponentsMask IDNode::get_visible_components_mask() const
{
  IDComponentsMask result = 0;
  for (ComponentNode *comp_node : components.values()) {
    if (comp_node->possibly_affects_visible_id) {
      const int component_type_as_int = int(comp_node->type);
      BLI_assert(component_type_as_int < 64);
      result |= (1ULL << component_type_as_int);
    }
  }
  return result;
}

}  // namespace blender::deg
