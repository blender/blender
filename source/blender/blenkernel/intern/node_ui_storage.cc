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

#include "CLG_log.h"

#include <mutex>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_node_ui_storage.hh"
#include "BKE_object.h"

static CLG_LogRef LOG = {"bke.node_ui_storage"};

using blender::Map;
using blender::StringRef;
using blender::Vector;

/* Use a global mutex because otherwise it would have to be stored directly in the
 * bNodeTree struct in DNA. This could change if the node tree had a runtime struct. */
static std::mutex global_ui_storage_mutex;

static void ui_storage_ensure(bNodeTree &ntree)
{
  /* As an optimization, only acquire a lock if the UI storage doesn't exist,
   * because it only needs to be allocated once for every node tree. */
  if (ntree.ui_storage == nullptr) {
    std::lock_guard<std::mutex> lock(global_ui_storage_mutex);
    /* Check again-- another thread may have allocated the storage while this one waited. */
    if (ntree.ui_storage == nullptr) {
      ntree.ui_storage = new NodeTreeUIStorage();
    }
  }
}

const NodeUIStorage *BKE_node_tree_ui_storage_get_from_context(const bContext *C,
                                                               const bNodeTree &ntree,
                                                               const bNode &node)
{
  const NodeTreeUIStorage *ui_storage = ntree.ui_storage;
  if (ui_storage == nullptr) {
    return nullptr;
  }

  const Object *active_object = CTX_data_active_object(C);
  if (active_object == nullptr) {
    return nullptr;
  }

  const ModifierData *active_modifier = BKE_object_active_modifier(active_object);
  if (active_modifier == nullptr) {
    return nullptr;
  }

  const NodeTreeEvaluationContext context(*active_object, *active_modifier);
  const Map<std::string, NodeUIStorage> *storage = ui_storage->context_map.lookup_ptr(context);
  if (storage == nullptr) {
    return nullptr;
  }

  return storage->lookup_ptr_as(StringRef(node.name));
}

/**
 * Removes only the UI data associated with a particular evaluation context. The same node tree
 * can be used for execution in multiple places, but the entire UI storage can't be removed when
 * one execution starts, or all of the data associated with the node tree would be lost.
 */
void BKE_nodetree_ui_storage_free_for_context(bNodeTree &ntree,
                                              const NodeTreeEvaluationContext &context)
{
  NodeTreeUIStorage *ui_storage = ntree.ui_storage;
  if (ui_storage != nullptr) {
    std::lock_guard<std::mutex> lock(ui_storage->context_map_mutex);
    ui_storage->context_map.remove(context);
  }
}

static void node_error_message_log(bNodeTree &ntree,
                                   const bNode &node,
                                   const StringRef message,
                                   const NodeWarningType type)
{
  switch (type) {
    case NodeWarningType::Error:
      CLOG_ERROR(&LOG,
                 "Node Tree: \"%s\", Node: \"%s\", %s",
                 ntree.id.name + 2,
                 node.name,
                 message.data());
      break;
    case NodeWarningType::Warning:
      CLOG_WARN(&LOG,
                "Node Tree: \"%s\", Node: \"%s\", %s",
                ntree.id.name + 2,
                node.name,
                message.data());
      break;
    case NodeWarningType::Info:
      CLOG_INFO(&LOG,
                2,
                "Node Tree: \"%s\", Node: \"%s\", %s",
                ntree.id.name + 2,
                node.name,
                message.data());
      break;
  }
}

static NodeUIStorage &node_ui_storage_ensure(bNodeTree &ntree,
                                             const NodeTreeEvaluationContext &context,
                                             const bNode &node)
{
  ui_storage_ensure(ntree);
  NodeTreeUIStorage &ui_storage = *ntree.ui_storage;

  std::lock_guard<std::mutex> lock(ui_storage.context_map_mutex);
  Map<std::string, NodeUIStorage> &node_tree_ui_storage =
      ui_storage.context_map.lookup_or_add_default(context);

  NodeUIStorage &node_ui_storage = node_tree_ui_storage.lookup_or_add_default_as(
      StringRef(node.name));

  return node_ui_storage;
}

void BKE_nodetree_error_message_add(bNodeTree &ntree,
                                    const NodeTreeEvaluationContext &context,
                                    const bNode &node,
                                    const NodeWarningType type,
                                    std::string message)
{
  node_error_message_log(ntree, node, message, type);

  NodeUIStorage &node_ui_storage = node_ui_storage_ensure(ntree, context, node);
  std::lock_guard lock{node_ui_storage.mutex};
  node_ui_storage.warnings.append({type, std::move(message)});
}

void BKE_nodetree_attribute_hint_add(bNodeTree &ntree,
                                     const NodeTreeEvaluationContext &context,
                                     const bNode &node,
                                     const StringRef attribute_name,
                                     const AttributeDomain domain,
                                     const CustomDataType data_type)
{
  NodeUIStorage &node_ui_storage = node_ui_storage_ensure(ntree, context, node);
  std::lock_guard lock{node_ui_storage.mutex};
  node_ui_storage.attribute_hints.add_as(
      AvailableAttributeInfo{attribute_name, domain, data_type});
}
