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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder.h"

#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_layer_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_stack.h"

extern "C" {
#include "BKE_animsys.h"
}

#include "intern/depsgraph.h"
#include "intern/depsgraph_tag.h"
#include "intern/depsgraph_type.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

#include "DEG_depsgraph.h"

namespace DEG {

/*******************************************************************************
 * Base class for builders.
 */

DepsgraphBuilder::DepsgraphBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache)
    : bmain_(bmain), graph_(graph), cache_(cache)
{
}

namespace {

struct VisibilityCheckData {
  eEvaluationMode eval_mode;
  bool is_visibility_animated;
};

void visibility_animated_check_cb(ID * /*id*/, FCurve *fcu, void *user_data)
{
  VisibilityCheckData *data = reinterpret_cast<VisibilityCheckData *>(user_data);
  if (data->is_visibility_animated) {
    return;
  }
  if (data->eval_mode == DAG_EVAL_VIEWPORT) {
    if (STREQ(fcu->rna_path, "hide_viewport")) {
      data->is_visibility_animated = true;
    }
  }
  else if (data->eval_mode == DAG_EVAL_RENDER) {
    if (STREQ(fcu->rna_path, "hide_render")) {
      data->is_visibility_animated = true;
    }
  }
}

bool is_object_visibility_animated(const Depsgraph *graph, Object *object)
{
  AnimData *anim_data = BKE_animdata_from_id(&object->id);
  if (anim_data == NULL) {
    return false;
  }
  VisibilityCheckData data;
  data.eval_mode = graph->mode;
  data.is_visibility_animated = false;
  BKE_fcurves_id_cb(&object->id, visibility_animated_check_cb, &data);
  return data.is_visibility_animated;
}

}  // namespace

bool deg_check_base_available_for_build(const Depsgraph *graph, Base *base)
{
  const int base_flag = (graph->mode == DAG_EVAL_VIEWPORT) ? BASE_ENABLED_VIEWPORT :
                                                             BASE_ENABLED_RENDER;
  if (base->flag & base_flag) {
    return true;
  }
  if (is_object_visibility_animated(graph, base->object)) {
    return true;
  }
  return false;
}

bool DepsgraphBuilder::need_pull_base_into_graph(Base *base)
{
  return deg_check_base_available_for_build(graph_, base);
}

/*******************************************************************************
 * Builder finalizer.
 */

namespace {

void deg_graph_build_flush_visibility(Depsgraph *graph)
{
  enum {
    DEG_NODE_VISITED = (1 << 0),
  };

  BLI_Stack *stack = BLI_stack_new(sizeof(OperationNode *), "DEG flush layers stack");
  for (IDNode *id_node : graph->id_nodes) {
    GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
      comp_node->affects_directly_visible |= id_node->is_directly_visible;
    }
    GHASH_FOREACH_END();
  }
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
    /* Flush layers to parents. */
    for (Relation *rel : op_node->inlinks) {
      if (rel->from->type == NodeType::OPERATION) {
        OperationNode *op_from = (OperationNode *)rel->from;
        op_from->owner->affects_directly_visible |= op_node->owner->affects_directly_visible;
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
}

}  // namespace

void deg_graph_build_finalize(Main *bmain, Depsgraph *graph)
{
  /* Make sure dependencies of visible ID datablocks are visible. */
  deg_graph_build_flush_visibility(graph);
  /* Re-tag IDs for update if it was tagged before the relations
   * update tag. */
  for (IDNode *id_node : graph->id_nodes) {
    ID *id = id_node->id_orig;
    id_node->finalize_build(graph);
    int flag = 0;
    /* Tag rebuild if special evaluation flags changed. */
    if (id_node->eval_flags != id_node->previous_eval_flags) {
      flag |= ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY;
    }
    /* Tag rebuild if the custom data mask changed. */
    if (id_node->customdata_masks != id_node->previous_customdata_masks) {
      flag |= ID_RECALC_GEOMETRY;
    }
    if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
      flag |= ID_RECALC_COPY_ON_WRITE;
      /* This means ID is being added to the dependency graph first
       * time, which is similar to "ob-visible-change" */
      if (GS(id->name) == ID_OB) {
        flag |= ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY;
      }
    }
    if (flag != 0) {
      graph_id_tag_update(bmain, graph, id_node->id_orig, flag, DEG_UPDATE_SOURCE_RELATIONS);
    }
  }
}

}  // namespace DEG
