/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_visibility.h"

#include "DNA_layer_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_assert.h"
#include "BLI_stack.h"

#include "DEG_depsgraph.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

void deg_evaluate_object_node_visibility(::Depsgraph *depsgraph, IDNode *id_node)
{
  BLI_assert(GS(id_node->id_cow->name) == ID_OB);

  Depsgraph *graph = reinterpret_cast<Depsgraph *>(depsgraph);
  const Object *object = reinterpret_cast<const Object *>(id_node->id_cow);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, &object->id);

  const int required_flags = (graph->mode == DAG_EVAL_VIEWPORT) ? BASE_ENABLED_VIEWPORT :
                                                                  BASE_ENABLED_RENDER;

  const bool is_enabled = !graph->use_visibility_optimization ||
                          object->base_flag & required_flags;

  if (id_node->is_enabled_on_eval != is_enabled) {
    id_node->is_enabled_on_eval = is_enabled;

    /* Tag dependency graph for changed visibility, so that it is updated on all dependencies prior
     * to a pass of an actual evaluation. */
    graph->need_update_nodes_visibility = true;
  }
}

void deg_evaluate_object_modifiers_mode_node_visibility(::Depsgraph *depsgraph, IDNode *id_node)
{
  BLI_assert(GS(id_node->id_cow->name) == ID_OB);

  Depsgraph *graph = reinterpret_cast<Depsgraph *>(depsgraph);
  const Object *object = reinterpret_cast<const Object *>(id_node->id_cow);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, &object->id);

  if (BLI_listbase_is_empty(&object->modifiers)) {
    return;
  }

  const ModifierMode modifier_mode = (graph->mode == DAG_EVAL_VIEWPORT) ? eModifierMode_Realtime :
                                                                          eModifierMode_Render;

  const ComponentNode *geometry_component = id_node->find_component(NodeType::GEOMETRY);
  LISTBASE_FOREACH (ModifierData *, modifier, &object->modifiers) {
    OperationNode *modifier_node = geometry_component->find_operation(OperationCode::MODIFIER,
                                                                      modifier->name);

    BLI_assert_msg(modifier_node != nullptr,
                   "Modifier node in depsgraph is not found. Likely due to missing "
                   "DEG_relations_tag_update().");

    const bool modifier_enabled = !graph->use_visibility_optimization ||
                                  (modifier->mode & modifier_mode);
    const int mute_flag = modifier_enabled ? 0 : DEPSOP_FLAG_MUTE;
    if ((modifier_node->flag & DEPSOP_FLAG_MUTE) != mute_flag) {
      modifier_node->flag &= ~DEPSOP_FLAG_MUTE;
      modifier_node->flag |= mute_flag;

      graph->need_update_nodes_visibility = true;
    }
  }
}

void deg_graph_flush_visibility_flags(Depsgraph *graph)
{
  enum {
    DEG_NODE_VISITED = (1 << 0),
  };

  for (IDNode *id_node : graph->id_nodes) {
    for (ComponentNode *comp_node : id_node->components.values()) {
      comp_node->possibly_affects_visible_id = id_node->is_visible_on_build;
      comp_node->affects_visible_id = id_node->is_visible_on_build && id_node->is_enabled_on_eval;

      /* Visibility component is always to be considered to have the same visibility as the
       * `id_node->is_visible_on_build`. This is because the visibility is to be evaluated
       * regardless of its current state as it might get changed due to animation. */
      if (comp_node->type == NodeType::VISIBILITY) {
        comp_node->affects_visible_id = id_node->is_visible_on_build;
      }

      /* Enforce "visibility" of the synchronization component.
       *
       * This component is never connected to other ID nodes, and hence can not be handled in the
       * same way as other components needed for evaluation. It is only needed for proper
       * evaluation of the ID node it belongs to.
       *
       * The design is such that the synchronization is supposed to happen whenever any part of the
       * ID changed/evaluated. Here we mark the component as "visible" so that genetic recalc flag
       * flushing and scheduling will handle the component in a generic manner. */
      if (comp_node->type == NodeType::SYNCHRONIZATION) {
        comp_node->possibly_affects_visible_id = true;
        comp_node->affects_visible_id = true;
      }
    }
  }

  BLI_Stack *stack = BLI_stack_new(sizeof(OperationNode *), "DEG flush layers stack");

  for (OperationNode *op_node : graph->operations) {
    op_node->custom_flags = 0;
    op_node->num_links_pending = 0;
    for (Relation *rel : op_node->outlinks) {
      if ((rel->from->type == NodeType::OPERATION) && (rel->flag & RELATION_FLAG_CYCLIC) == 0) {
        ++op_node->num_links_pending;
      }
    }
    if (op_node->num_links_pending == 0) {
      BLI_stack_push(stack, &op_node);
      op_node->custom_flags |= DEG_NODE_VISITED;
    }
  }

  while (!BLI_stack_is_empty(stack)) {
    OperationNode *op_node;
    BLI_stack_pop(stack, &op_node);

    /* Flush flags to parents. */
    for (Relation *rel : op_node->inlinks) {
      if (rel->from->type == NodeType::OPERATION) {
        const OperationNode *op_to = reinterpret_cast<const OperationNode *>(rel->to);
        const ComponentNode *comp_to = op_to->owner;

        /* Ignore the synchronization target.
         * It is always visible and should not affect on other components. */
        if (comp_to->type == NodeType::SYNCHRONIZATION) {
          continue;
        }

        OperationNode *op_from = reinterpret_cast<OperationNode *>(rel->from);
        ComponentNode *comp_from = op_from->owner;

        op_from->flag |= (op_to->flag & OperationFlag::DEPSOP_FLAG_AFFECTS_VISIBILITY);

        if (rel->flag & RELATION_NO_VISIBILITY_CHANGE) {
          continue;
        }

        const bool target_possibly_affects_visible_id = comp_to->possibly_affects_visible_id;

        bool target_affects_visible_id = comp_to->affects_visible_id;

        /* This is a bit arbitrary but the idea here is following:
         *
         *  - When another object is used by a disabled modifier we do not want that object to
         *    be considered needed for evaluation.
         *
         *  - However, we do not want to take mute flag during visibility propagation within the
         *    same object. Otherwise drivers and transform dependencies of the geometry component
         *    entry component might not be properly handled.
         *
         * This code works fine for muting modifiers, but might need tweaks when mute is used for
         * something else. */
        if (comp_from != comp_to && (op_to->flag & DEPSOP_FLAG_MUTE)) {
          target_affects_visible_id = false;
        }

        /* Visibility component forces all components of the current ID to be considered as
         * affecting directly visible. */
        if (comp_from->type == NodeType::VISIBILITY) {
          const IDNode *id_node_from = comp_from->owner;
          if (target_possibly_affects_visible_id) {
            for (ComponentNode *comp_node : id_node_from->components.values()) {
              comp_node->possibly_affects_visible_id |= target_possibly_affects_visible_id;
            }
          }
          if (target_affects_visible_id) {
            for (ComponentNode *comp_node : id_node_from->components.values()) {
              comp_node->affects_visible_id |= target_affects_visible_id;
            }
          }
        }
        else {
          comp_from->possibly_affects_visible_id |= target_possibly_affects_visible_id;
          comp_from->affects_visible_id |= target_affects_visible_id;
        }
      }
    }

    /* Schedule parent nodes. */
    for (Relation *rel : op_node->inlinks) {
      if (rel->from->type == NodeType::OPERATION) {
        OperationNode *op_from = (OperationNode *)rel->from;
        if ((rel->flag & RELATION_FLAG_CYCLIC) == 0) {
          BLI_assert(op_from->num_links_pending > 0);
          --op_from->num_links_pending;
        }
        if ((op_from->num_links_pending == 0) && (op_from->custom_flags & DEG_NODE_VISITED) == 0) {
          BLI_stack_push(stack, &op_from);
          op_from->custom_flags |= DEG_NODE_VISITED;
        }
      }
    }
  }
  BLI_stack_free(stack);

  graph->need_update_nodes_visibility = false;
}

void deg_graph_flush_visibility_flags_if_needed(Depsgraph *graph)
{
  if (!graph->need_update_nodes_visibility) {
    return;
  }

  deg_graph_flush_visibility_flags(graph);
}

}  // namespace blender::deg
