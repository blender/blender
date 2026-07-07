/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_enums.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"

#include "BKE_context.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "BLT_translation.hh"

#include "ED_node.hh"
#include "ED_screen.hh"

#include "NOD_sync_sockets.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

static Vector<bNode *> get_nodes_to_sync(bContext &C, PointerRNA *ptr)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  if (!snode.edittree) {
    return {};
  }

  std::optional<std::string> node_name;
  PropertyRNA *node_name_prop = RNA_struct_find_property(ptr, "node_name");
  if (RNA_property_is_set(ptr, node_name_prop)) {
    node_name = RNA_property_string_get(ptr, node_name_prop);
  }

  Vector<bNode *> nodes_to_sync;
  bNodeTree &tree = *snode.edittree;
  for (bNode *node : tree.all_nodes()) {
    if (node_name.has_value()) {
      if (node->name != node_name) {
        continue;
      }
    }
    else {
      if (!(node->flag & NODE_SELECT)) {
        continue;
      }
    }
    nodes_to_sync.append(node);
  }
  return nodes_to_sync;
}

static wmOperatorStatus sockets_sync_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  if (!snode.edittree) {
    return OPERATOR_CANCELLED;
  }
  Vector<bNode *> nodes_to_sync = get_nodes_to_sync(*C, op->ptr);
  if (nodes_to_sync.is_empty()) {
    return OPERATOR_CANCELLED;
  }
  for (bNode *node : nodes_to_sync) {
    nodes::sync_node(*C, *node, op->reports);
  }
  BKE_main_ensure_invariants(bmain, snode.edittree->id);
  return OPERATOR_FINISHED;
}

static std::string sockets_sync_get_description(bContext *C, wmOperatorType *ot, PointerRNA *ptr)
{
  Vector<bNode *> nodes_to_sync = get_nodes_to_sync(*C, ptr);
  if (nodes_to_sync.size() != 1) {
    return TIP_(ot->description);
  }
  const bNode &node = *nodes_to_sync.first();
  std::string description = nodes::sync_node_description_get(*C, node);
  if (description.empty()) {
    return TIP_(ot->description);
  }
  return description;
}

void NODE_OT_sockets_sync(wmOperatorType *ot)
{
  ot->name = "Sync Sockets";
  ot->idname = "NODE_OT_sockets_sync";
  ot->description = "Update sockets to match what is actually used";
  ot->get_description = sockets_sync_get_description;

  ot->poll = ED_operator_node_editable;
  ot->exec = sockets_sync_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna, "node_name", nullptr, 0, "Node Name", nullptr);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

Map<int, bool> &node_can_sync_cache_get(SpaceNode &snode)
{
  return snode.runtime->node_can_sync_states;
}

}  // namespace blender::ed::space_node
