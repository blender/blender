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

static void ui_storage_ensure(bNodeTree &ntree)
{
  if (ntree.ui_storage == nullptr) {
    ntree.ui_storage = new NodeTreeUIStorage();
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
  const ModifierData *active_modifier = BKE_object_active_modifier(active_object);
  if (active_object == nullptr || active_modifier == nullptr) {
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

static NodeUIStorage &find_node_ui_storage(bNodeTree &ntree,
                                           const NodeTreeEvaluationContext &context,
                                           const bNode &node)
{
  ui_storage_ensure(ntree);
  NodeTreeUIStorage &ui_storage = *ntree.ui_storage;

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

  NodeUIStorage &node_ui_storage = find_node_ui_storage(ntree, context, node);
  node_ui_storage.warnings.append({type, std::move(message)});
}
