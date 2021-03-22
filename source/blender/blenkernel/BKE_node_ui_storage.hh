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
#include "BLI_multi_value_map.hh"
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
    const uint64_t hash1 = blender::DefaultHash<std::string>{}(object_name_);
    const uint64_t hash2 = BLI_session_uuid_hash_uint64(&modifier_session_uuid_);
    return hash1 ^ (hash2 * 33); /* Copied from DefaultHash for std::pair. */
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
  AttributeDomain domain;
  CustomDataType data_type;

  uint64_t hash() const
  {
    uint64_t domain_hash = (uint64_t)domain;
    uint64_t data_type_hash = (uint64_t)data_type;
    return (domain_hash * 33) ^ (data_type_hash * 89);
  }

  friend bool operator==(const AvailableAttributeInfo &a, const AvailableAttributeInfo &b)
  {
    return a.domain == b.domain && a.data_type == b.data_type;
  }
};

struct NodeUIStorage {
  blender::Vector<NodeWarning> warnings;
  blender::MultiValueMap<std::string, AvailableAttributeInfo> attribute_hints;
};

struct NodeTreeUIStorage {
  blender::Map<NodeTreeEvaluationContext, blender::Map<std::string, NodeUIStorage>> context_map;
  std::mutex context_map_mutex;
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
