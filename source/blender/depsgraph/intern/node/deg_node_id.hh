/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "BLI_struct_equality_utils.hh"
#include "intern/node/deg_node.hh"

#include "DNA_ID.h"

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"

namespace blender::deg {

struct ComponentNode;

using IDComponentsMask = uint64_t;

/* NOTE: We use max comparison to mark an id node that is linked more than once
 * So keep this enum ordered accordingly. */
enum eDepsNode_LinkedState_Type {
  /* Generic indirectly linked id node. */
  DEG_ID_LINKED_INDIRECTLY = 0,
  /* Id node present in the set (background) only. */
  DEG_ID_LINKED_VIA_SET = 1,
  /* Id node directly linked via the SceneLayer. */
  DEG_ID_LINKED_DIRECTLY = 2,
};
const char *linkedStateAsString(eDepsNode_LinkedState_Type linked_state);

/* ID-Block Reference */
struct IDNode : public Node {
  struct ComponentIDKey {

    NodeType type;
    StringRef name;

    ComponentIDKey(NodeType type, StringRef name = "") : type(type), name(name) {}
    BLI_STRUCT_EQUALITY_OPERATORS_2(ComponentIDKey, type, name);
    uint64_t hash() const
    {
      return get_default_hash(type, name);
    }
  };

  /** Initialize 'id' node - from pointer data given. */
  void init(const ID *id, const char *subdata) override;
  void init_copy_on_write(ID *id_cow_hint = nullptr);
  ~IDNode() override;
  void destroy();

  std::string identifier() const override;

  ComponentNode *find_component(NodeType type, StringRef name = "") const;
  ComponentNode *add_component(NodeType type, StringRef name = "");

  void tag_update(Depsgraph *graph, eUpdateSource source) override;

  void finalize_build(Depsgraph *graph);

  IDComponentsMask get_visible_components_mask() const;

  /* Type of the ID stored separately, so it's possible to perform check whether evaluated copy is
   * needed without de-referencing the id_cow (which is not safe when ID is NOT covered by
   * copy-on-evaluation and has been deleted from the main database.) */
  ID_Type id_type;

  /* ID Block referenced. */
  ID *id_orig;

  /* Session-wide UUID of the id_orig.
   * Is used on relations update to map evaluated state from old nodes to the new ones, without
   * relying on pointers (which are not guaranteed to be unique) and without dereferencing id_orig
   * which could be "stale" pointer. */
  uint id_orig_session_uid;

  /* Evaluated data-block.
   * Will be covered by the copy-on-evaluation system if the ID Type needs it. */
  ID *id_cow;

  /* Hash to make it faster to look up components. */
  Map<ComponentIDKey, ComponentNode *> components;

  /* Additional flags needed for scene evaluation.
   * TODO(sergey): Only needed for until really granular updates
   * of all the entities. */
  uint32_t eval_flags;
  uint32_t previous_eval_flags;

  /* Extra customdata mask which needs to be evaluated for the mesh object. */
  DEGCustomDataMeshMasks customdata_masks;
  DEGCustomDataMeshMasks previous_customdata_masks;

  eDepsNode_LinkedState_Type linked_state;

  /* Indicates the data-block is to be considered visible in the evaluated scene.
   *
   * This flag is set during dependency graph build where check for an actual visibility might not
   * be available yet due to driven or animated restriction flags. So it is more of an intent or,
   * in other words, plausibility of the data-block to be visible. */
  bool is_visible_on_build;

  /* Evaluated state of whether evaluation considered this data-block "enabled".
   *
   * For objects this is derived from the base restriction flags, which might be animated or
   * driven. It is set to `BASE_ENABLED_<VIEWPORT, RENDER>` (depending on the graph mode) after
   * the object's flags from layer were evaluated.
   *
   * For other data-types is currently always true. */
  bool is_enabled_on_eval;

  /* For the collection type of ID, denotes whether collection was fully
   * recursed into. */
  bool is_collection_fully_expanded;

  /* Is used to figure out whether object came to the dependency graph via a base. */
  bool has_base;

  /* Accumulated flag from operation. Is initialized and used during updates flush. */
  bool is_user_modified;

  /* Copy-on-Write component has been explicitly tagged for update. */
  bool is_cow_explicitly_tagged;

  /* Accumulate recalc flags from multiple update passes. */
  int id_cow_recalc_backup;

  IDComponentsMask visible_components_mask;
  IDComponentsMask previously_visible_components_mask;

  DEG_DEPSNODE_DECLARE;
};

}  // namespace blender::deg
