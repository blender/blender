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
 */

#pragma once

#include <mutex>

#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_session_uuid.h"
#include "BLI_set.hh"

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_session_uuid_types.h"

#include "BKE_attribute.h"

struct ModifierData;
struct Object;
struct bNode;
struct bNodeTree;
struct bContext;

/**
 * Contains the context necessary to determine when to display settings for a certain node tree
 * that may be used for multiple modifiers and objects. The object name and modifier session UUID
 * are used instead of pointers because they are re-allocated between evaluations.
 *
 * \note This does not yet handle the context of nested node trees.
 */
class NodeTreeEvaluationContext {
 private:
  std::string object_name_;
  SessionUUID modifier_session_uuid_;

 public:
  NodeTreeEvaluationContext(const Object &object, const ModifierData &modifier)
  {
    object_name_ = reinterpret_cast<const ID &>(object).name;
    modifier_session_uuid_ = modifier.session_uuid;
  }

  uint64_t hash() const
  {
    return blender::get_default_hash_2(object_name_, modifier_session_uuid_);
  }

  friend bool operator==(const NodeTreeEvaluationContext &a, const NodeTreeEvaluationContext &b)
  {
    return a.object_name_ == b.object_name_ &&
           BLI_session_uuid_is_equal(&a.modifier_session_uuid_, &b.modifier_session_uuid_);
  }
};

enum class NodeWarningType {
  Error,
  Warning,
  Info,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

struct AvailableAttributeInfo {
  std::string name;
  AttributeDomain domain;
  CustomDataType data_type;

  uint64_t hash() const
  {
    return blender::get_default_hash(name);
  }

  friend bool operator==(const AvailableAttributeInfo &a, const AvailableAttributeInfo &b)
  {
    return a.name == b.name;
  }
};

struct NodeUIStorage {
  blender::Vector<NodeWarning> warnings;
  blender::Set<AvailableAttributeInfo> attribute_hints;
};

struct NodeTreeUIStorage {
  blender::Map<NodeTreeEvaluationContext, blender::Map<std::string, NodeUIStorage>> context_map;
  std::mutex context_map_mutex;

  /**
   * Attribute search uses this to store the fake info for the string typed into a node, in order
   * to pass the info to the execute callback that sets node socket values. This is mutable since
   * we can count on only one attribute search being open at a time, and there is no real data
   * stored here.
   */
  mutable AvailableAttributeInfo dummy_info_for_search;
};

const NodeUIStorage *BKE_node_tree_ui_storage_get_from_context(const bContext *C,
                                                               const bNodeTree &ntree,
                                                               const bNode &node);

void BKE_nodetree_ui_storage_free_for_context(bNodeTree &ntree,
                                              const NodeTreeEvaluationContext &context);

void BKE_nodetree_error_message_add(bNodeTree &ntree,
                                    const NodeTreeEvaluationContext &context,
                                    const bNode &node,
                                    const NodeWarningType type,
                                    std::string message);

void BKE_nodetree_attribute_hint_add(bNodeTree &ntree,
                                     const NodeTreeEvaluationContext &context,
                                     const bNode &node,
                                     const blender::StringRef attribute_name,
                                     const AttributeDomain domain,
                                     const CustomDataType data_type);
