/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace deg = blender::deg;

/* ************************ DEG TRAVERSAL ********************* */

namespace blender::deg {
namespace {

using TraversalQueue = deque<OperationNode *>;

using DEGForeachOperation = void (*)(OperationNode *, void *);

bool deg_foreach_needs_visit(const OperationNode *op_node, const int flags)
{
  if (flags & DEG_FOREACH_COMPONENT_IGNORE_TRANSFORM_SOLVERS) {
    if (op_node->opcode == OperationCode::RIGIDBODY_SIM) {
      return false;
    }
  }
  return true;
}

void deg_foreach_dependent_operation(const Depsgraph * /*graph*/,
                                     const IDNode *target_id_node,
                                     eDepsObjectComponentType source_component_type,
                                     int flags,
                                     DEGForeachOperation callback,
                                     void *user_data)
{
  if (target_id_node == nullptr) {
    /* TODO(sergey): Shall we inform or assert here about attempt to start
     * iterating over non-existing ID? */
    return;
  }
  /* Start with scheduling all operations from ID node. */
  TraversalQueue queue;
  Set<OperationNode *> scheduled;
  for (ComponentNode *comp_node : target_id_node->components.values()) {
    if (comp_node->type == NodeType::VISIBILITY) {
      /* Visibility component is only used internally. It is not to be reporting dependencies to
       * the outer world. */
      continue;
    }

    if (source_component_type != DEG_OB_COMP_ANY &&
        nodeTypeToObjectComponent(comp_node->type) != source_component_type)
    {
      continue;
    }
    for (OperationNode *op_node : comp_node->operations) {
      if (!deg_foreach_needs_visit(op_node, flags)) {
        continue;
      }
      queue.push_back(op_node);
      scheduled.add(op_node);
    }
  }
  /* Process the queue. */
  while (!queue.empty()) {
    /* get next operation node to process. */
    OperationNode *op_node = queue.front();
    queue.pop_front();
    for (;;) {
      callback(op_node, user_data);
      /* Schedule outgoing operation nodes. */
      if (op_node->outlinks.size() == 1) {
        OperationNode *to_node = (OperationNode *)op_node->outlinks[0]->to;
        if (!scheduled.contains(to_node) && deg_foreach_needs_visit(to_node, flags)) {
          scheduled.add_new(to_node);
          op_node = to_node;
        }
        else {
          break;
        }
      }
      else {
        for (Relation *rel : op_node->outlinks) {
          OperationNode *to_node = (OperationNode *)rel->to;
          if (!scheduled.contains(to_node) && deg_foreach_needs_visit(to_node, flags)) {
            queue.push_front(to_node);
            scheduled.add_new(to_node);
          }
        }
        break;
      }
    }
  }
}

struct ForeachIDComponentData {
  DEGForeachIDComponentCallback callback;
  void *user_data;
  IDNode *target_id_node;
  Set<ComponentNode *> visited;
};

void deg_foreach_dependent_component_callback(OperationNode *op_node, void *user_data_v)
{
  ForeachIDComponentData *user_data = reinterpret_cast<ForeachIDComponentData *>(user_data_v);
  ComponentNode *comp_node = op_node->owner;
  IDNode *id_node = comp_node->owner;
  if (id_node != user_data->target_id_node && !user_data->visited.contains(comp_node)) {
    user_data->callback(
        id_node->id_orig, nodeTypeToObjectComponent(comp_node->type), user_data->user_data);
    user_data->visited.add_new(comp_node);
  }
}

void deg_foreach_dependent_ID_component(const Depsgraph *graph,
                                        const ID *id,
                                        eDepsObjectComponentType source_component_type,
                                        int flags,
                                        DEGForeachIDComponentCallback callback,
                                        void *user_data)
{
  ForeachIDComponentData data;
  data.callback = callback;
  data.user_data = user_data;
  data.target_id_node = graph->find_id_node(id);
  deg_foreach_dependent_operation(graph,
                                  data.target_id_node,
                                  source_component_type,
                                  flags,
                                  deg_foreach_dependent_component_callback,
                                  &data);
}

struct ForeachIDData {
  DEGForeachIDCallback callback;
  void *user_data;
  IDNode *target_id_node;
  Set<IDNode *> visited;
};

void deg_foreach_dependent_ID_callback(OperationNode *op_node, void *user_data_v)
{
  ForeachIDData *user_data = reinterpret_cast<ForeachIDData *>(user_data_v);
  ComponentNode *comp_node = op_node->owner;
  IDNode *id_node = comp_node->owner;
  if (id_node != user_data->target_id_node && !user_data->visited.contains(id_node)) {
    user_data->callback(id_node->id_orig, user_data->user_data);
    user_data->visited.add_new(id_node);
  }
}

void deg_foreach_dependent_ID(const Depsgraph *graph,
                              const ID *id,
                              DEGForeachIDCallback callback,
                              void *user_data)
{
  ForeachIDData data;
  data.callback = callback;
  data.user_data = user_data;
  data.target_id_node = graph->find_id_node(id);
  deg_foreach_dependent_operation(
      graph, data.target_id_node, DEG_OB_COMP_ANY, 0, deg_foreach_dependent_ID_callback, &data);
}

void deg_foreach_ancestor_ID(const Depsgraph *graph,
                             const ID *id,
                             DEGForeachIDCallback callback,
                             void *user_data)
{
  /* Start with getting ID node from the graph. */
  IDNode *target_id_node = graph->find_id_node(id);
  if (target_id_node == nullptr) {
    /* TODO(sergey): Shall we inform or assert here about attempt to start
     * iterating over non-existing ID? */
    return;
  }
  /* Start with scheduling all operations from ID node. */
  TraversalQueue queue;
  Set<OperationNode *> scheduled;
  for (ComponentNode *comp_node : target_id_node->components.values()) {
    for (OperationNode *op_node : comp_node->operations) {
      queue.push_back(op_node);
      scheduled.add(op_node);
    }
  }
  Set<IDNode *> visited;
  visited.add_new(target_id_node);
  /* Process the queue. */
  while (!queue.empty()) {
    /* get next operation node to process. */
    OperationNode *op_node = queue.front();
    queue.pop_front();
    for (;;) {
      /* Check whether we need to inform callee about corresponding ID node. */
      ComponentNode *comp_node = op_node->owner;
      IDNode *id_node = comp_node->owner;
      if (!visited.contains(id_node)) {
        /* TODO(sergey): Is it orig or CoW? */
        callback(id_node->id_orig, user_data);
        visited.add_new(id_node);
      }
      /* Schedule incoming operation nodes. */
      if (op_node->inlinks.size() == 1) {
        Node *from = op_node->inlinks[0]->from;
        if (from->get_class() == NodeClass::OPERATION) {
          OperationNode *from_node = (OperationNode *)from;
          if (scheduled.add(from_node)) {
            op_node = from_node;
          }
          else {
            break;
          }
        }
      }
      else {
        for (Relation *rel : op_node->inlinks) {
          Node *from = rel->from;
          if (from->get_class() == NodeClass::OPERATION) {
            OperationNode *from_node = (OperationNode *)from;
            if (scheduled.add(from_node)) {
              queue.push_front(from_node);
            }
          }
        }
        break;
      }
    }
  }
}

void deg_foreach_id(const Depsgraph *depsgraph, DEGForeachIDCallback callback, void *user_data)
{
  for (const IDNode *id_node : depsgraph->id_nodes) {
    callback(id_node->id_orig, user_data);
  }
}

}  // namespace
}  // namespace blender::deg

void DEG_foreach_dependent_ID(const Depsgraph *depsgraph,
                              const ID *id,
                              DEGForeachIDCallback callback,
                              void *user_data)
{
  deg::deg_foreach_dependent_ID((const deg::Depsgraph *)depsgraph, id, callback, user_data);
}

void DEG_foreach_dependent_ID_component(const Depsgraph *depsgraph,
                                        const ID *id,
                                        eDepsObjectComponentType source_component_type,
                                        int flags,
                                        DEGForeachIDComponentCallback callback,
                                        void *user_data)
{
  deg::deg_foreach_dependent_ID_component(
      (const deg::Depsgraph *)depsgraph, id, source_component_type, flags, callback, user_data);
}

void DEG_foreach_ancestor_ID(const Depsgraph *depsgraph,
                             const ID *id,
                             DEGForeachIDCallback callback,
                             void *user_data)
{
  deg::deg_foreach_ancestor_ID((const deg::Depsgraph *)depsgraph, id, callback, user_data);
}

void DEG_foreach_ID(const Depsgraph *depsgraph, DEGForeachIDCallback callback, void *user_data)
{
  deg::deg_foreach_id((const deg::Depsgraph *)depsgraph, callback, user_data);
}
