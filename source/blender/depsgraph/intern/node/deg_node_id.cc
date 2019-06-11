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
 */

#include "intern/node/deg_node_id.h"

#include <stdio.h>
#include <cstring> /* required for STREQ later on. */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_string.h"

extern "C" {
#include "DNA_ID.h"
#include "DNA_anim_types.h"

#include "BKE_animsys.h"
#include "BKE_library.h"
}

#include "DEG_depsgraph.h"

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_time.h"

namespace DEG {

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
  BLI_assert(!"Unhandled linked state, should never happen.");
  return "UNKNOW";
}

IDNode::ComponentIDKey::ComponentIDKey(NodeType type, const char *name) : type(type), name(name)
{
}

bool IDNode::ComponentIDKey::operator==(const ComponentIDKey &other) const
{
  return type == other.type && STREQ(name, other.name);
}

static unsigned int id_deps_node_hash_key(const void *key_v)
{
  const IDNode::ComponentIDKey *key = reinterpret_cast<const IDNode::ComponentIDKey *>(key_v);
  const int type_as_int = static_cast<int>(key->type);
  return BLI_ghashutil_combine_hash(BLI_ghashutil_uinthash(type_as_int),
                                    BLI_ghashutil_strhash_p(key->name));
}

static bool id_deps_node_hash_key_cmp(const void *a, const void *b)
{
  const IDNode::ComponentIDKey *key_a = reinterpret_cast<const IDNode::ComponentIDKey *>(a);
  const IDNode::ComponentIDKey *key_b = reinterpret_cast<const IDNode::ComponentIDKey *>(b);
  return !(*key_a == *key_b);
}

static void id_deps_node_hash_key_free(void *key_v)
{
  typedef IDNode::ComponentIDKey ComponentIDKey;
  ComponentIDKey *key = reinterpret_cast<ComponentIDKey *>(key_v);
  OBJECT_GUARDED_DELETE(key, ComponentIDKey);
}

static void id_deps_node_hash_value_free(void *value_v)
{
  ComponentNode *comp_node = reinterpret_cast<ComponentNode *>(value_v);
  OBJECT_GUARDED_DELETE(comp_node, ComponentNode);
}

/* Initialize 'id' node - from pointer data given. */
void IDNode::init(const ID *id, const char *UNUSED(subdata))
{
  BLI_assert(id != NULL);
  /* Store ID-pointer. */
  id_orig = (ID *)id;
  eval_flags = 0;
  previous_eval_flags = 0;
  customdata_masks = DEGCustomDataMeshMasks();
  previous_customdata_masks = DEGCustomDataMeshMasks();
  linked_state = DEG_ID_LINKED_INDIRECTLY;
  is_directly_visible = true;
  is_collection_fully_expanded = false;
  has_base = false;
  is_user_modified = false;

  visible_components_mask = 0;
  previously_visible_components_mask = 0;

  components = BLI_ghash_new(
      id_deps_node_hash_key, id_deps_node_hash_key_cmp, "Depsgraph id components hash");
}

void IDNode::init_copy_on_write(ID *id_cow_hint)
{
  /* Create pointer as early as possible, so we can use it for function
   * bindings. Rest of data we'll be copying to the new datablock when
   * it is actually needed. */
  if (id_cow_hint != NULL) {
    // BLI_assert(deg_copy_on_write_is_needed(id_orig));
    if (deg_copy_on_write_is_needed(id_orig)) {
      id_cow = id_cow_hint;
    }
    else {
      id_cow = id_orig;
    }
  }
  else if (deg_copy_on_write_is_needed(id_orig)) {
    id_cow = (ID *)BKE_libblock_alloc_notest(GS(id_orig->name));
    DEG_COW_PRINT(
        "Create shallow copy for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
    deg_tag_copy_on_write_id(id_cow, id_orig);
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
  if (id_orig == NULL) {
    return;
  }

  BLI_ghash_free(components, id_deps_node_hash_key_free, id_deps_node_hash_value_free);

  /* Free memory used by this CoW ID. */
  if (id_cow != id_orig && id_cow != NULL) {
    deg_free_copy_on_write_datablock(id_cow);
    MEM_freeN(id_cow);
    id_cow = NULL;
    DEG_COW_PRINT("Destroy CoW for %s: id_orig=%p id_cow=%p\n", id_orig->name, id_orig, id_cow);
  }

  /* Tag that the node is freed. */
  id_orig = NULL;
}

string IDNode::identifier() const
{
  char orig_ptr[24], cow_ptr[24];
  BLI_snprintf(orig_ptr, sizeof(orig_ptr), "%p", id_orig);
  BLI_snprintf(cow_ptr, sizeof(cow_ptr), "%p", id_cow);
  return string(nodeTypeAsString(type)) + " : " + name + " (orig: " + orig_ptr +
         ", eval: " + cow_ptr + ", is_directly_visible " +
         (is_directly_visible ? "true" : "false") + ")";
}

ComponentNode *IDNode::find_component(NodeType type, const char *name) const
{
  ComponentIDKey key(type, name);
  return reinterpret_cast<ComponentNode *>(BLI_ghash_lookup(components, &key));
}

ComponentNode *IDNode::add_component(NodeType type, const char *name)
{
  ComponentNode *comp_node = find_component(type, name);
  if (!comp_node) {
    DepsNodeFactory *factory = type_get_factory(type);
    comp_node = (ComponentNode *)factory->create_node(this->id_orig, "", name);

    /* Register. */
    ComponentIDKey *key = OBJECT_GUARDED_NEW(ComponentIDKey, type, name);
    BLI_ghash_insert(components, key, comp_node);
    comp_node->owner = this;
  }
  return comp_node;
}

void IDNode::tag_update(Depsgraph *graph, eUpdateSource source)
{
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, components) {
    /* Relations update does explicit animation update when needed. Here we ignore animation
     * component to avoid loss of possible unkeyed changes. */
    if (comp_node->type == NodeType::ANIMATION && source == DEG_UPDATE_SOURCE_RELATIONS) {
      continue;
    }
    comp_node->tag_update(graph, source);
  }
  GHASH_FOREACH_END();
}

void IDNode::finalize_build(Depsgraph *graph)
{
  /* Finalize build of all components. */
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, components) {
    comp_node->finalize_build(graph);
  }
  GHASH_FOREACH_END();
  visible_components_mask = get_visible_components_mask();
}

IDComponentsMask IDNode::get_visible_components_mask() const
{
  IDComponentsMask result = 0;
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, components) {
    if (comp_node->affects_directly_visible) {
      const int component_type_as_int = static_cast<int>(comp_node->type);
      BLI_assert(component_type_as_int < 64);
      result |= (1ULL << component_type_as_int);
    }
  }
  GHASH_FOREACH_END();
  return result;
}

}  // namespace DEG
