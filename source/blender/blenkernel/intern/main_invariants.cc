/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_main_invariants.hh"
#include "BKE_node_tree_update.hh"

#include "DEG_depsgraph.hh"

#include "DNA_node_types.h"

#include "WM_api.hh"
#include "WM_types.hh"
#include <optional>

static void send_notifiers_after_node_tree_change(ID *id, bNodeTree *ntree)
{
  WM_main_add_notifier(NC_NODE | NA_EDITED, id);

  if (ntree->type == NTREE_SHADER && id != nullptr) {
    if (GS(id->name) == ID_MA) {
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
    }
    else if (GS(id->name) == ID_LA) {
      WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
    }
    else if (GS(id->name) == ID_WO) {
      WM_main_add_notifier(NC_WORLD | ND_WORLD, id);
    }
  }
  else if (ntree->type == NTREE_COMPOSIT) {
    WM_main_add_notifier(NC_SCENE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_TEXTURE) {
    WM_main_add_notifier(NC_TEXTURE | ND_NODES, id);
  }
  else if (ntree->type == NTREE_GEOMETRY) {
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, id);
  }
}

static void propagate_node_tree_changes(Main &bmain,
                                        const std::optional<blender::Span<ID *>> modified_ids)
{
  NodeTreeUpdateExtraParams params;
  params.tree_changed_fn = [](bNodeTree &ntree, ID &owner_id) {
    send_notifiers_after_node_tree_change(&owner_id, &ntree);
    DEG_id_tag_update(&ntree.id, ID_RECALC_SYNC_TO_EVAL);
  };
  params.tree_output_changed_fn = [](bNodeTree &ntree, ID & /*owner_id*/) {
    DEG_id_tag_update(&ntree.id, ID_RECALC_NTREE_OUTPUT);
  };

  std::optional<blender::Vector<bNodeTree *>> modified_trees;
  if (modified_ids.has_value()) {
    modified_trees.emplace();
    for (ID *id : *modified_ids) {
      if (GS(id->name) == ID_NT) {
        modified_trees->append(reinterpret_cast<bNodeTree *>(id));
      }
    }
  }

  BKE_ntree_update(bmain, modified_trees, params);
}

void BKE_main_ensure_invariants(Main &bmain, const std::optional<blender::Span<ID *>> modified_ids)
{
  propagate_node_tree_changes(bmain, modified_ids);
}

void BKE_main_ensure_invariants(Main &bmain, ID &modified_id)
{
  BKE_main_ensure_invariants(bmain, {{&modified_id}});
}
