/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Public API for Querying and Filtering Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_QUERY_H__
#define __DEG_DEPSGRAPH_QUERY_H__

struct ListBase;
struct ID;

struct Depsgraph;
struct DepsNode;
struct DepsRelation;

#ifdef __cplusplus
extern "C" {
#endif

/* ************************************************ */
/* Type Defines */

/* FilterPredicate Callback 
 *
 * Defines a callback function which can be supplied to check whether a 
 * node is relevant or not.
 *
 * < graph: Depsgraph that we're traversing
 * < node: The node to check
 * < userdata: FilterPredicate state data (as needed)
 * > returns: True if node is relevant
 */
typedef bool (*DEG_FilterPredicate)(const struct Depsgraph *graph, const struct DepsNode *node, void *userdata);


/* Node Operation 
 *
 * Performs some action on the given node, provided that the node was
 * deemed to be relevant to operate on.
 *
 * < graph: Depsgraph that we're traversing
 * < node: The node to perform operation on/with
 * < userdata: Node Operation's state data (as needed)
 * > returns: True if traversal should be aborted at this point
 */
typedef bool (*DEG_NodeOperation)(const struct Depsgraph *graph, struct DepsNode *node, void *userdata);

/* ************************************************ */
/* Low-Level Filtering API */

/* Create a filtered copy of the given graph which contains only the
 * nodes which fulfill the criteria specified using the FilterPredicate
 * passed in.
 *
 * < graph: The graph to be copied and filtered
 * < filter: FilterPredicate used to check which nodes should be included
 *           (If null, full graph is copied as-is)
 * < userdata: State data for filter (as necessary)
 *
 * > returns: a full copy of all the relevant nodes - the matching subgraph
 */
// XXX: is there any need for extra settings/options for how the filtering goes?
Depsgraph *DEG_graph_filter(const struct Depsgraph *graph, DEG_FilterPredicate *filter, void *userdata);


/* Traverse nodes in graph which are deemed relevant,
 * performing the provided operation on the nodes.
 *
 * < graph: The graph to perform operations on
 * < filter: FilterPredicate used to check which nodes should be included
 *           (If null, all nodes are considered valid targets)
 * < filter_data: Custom state data for FilterPredicate
 *                (Note: This can be the same as op_data, where appropriate)
 * < op: NodeOperation to perform on each node
 *       (If null, no graph traversal is performed for efficiency)
 * < op_data: Custom state data for NodeOperation
 *            (Note: This can be the same as filter_data, where appropriate)
 */
void DEG_graph_traverse(const struct Depsgraph *graph,
                        DEG_FilterPredicate *filter, void *filter_data,
                        DEG_NodeOperation *op, void *op_data);

/* ************************************************ */
/* Node-Based Operations */
// XXX: do we want to be able to attach conditional requirements here?

/* Find an (outer) node matching given conditions 
 * ! Assumes that there will only be one such node, or that only the first one matters
 *
 * < graph: a dependency graph which may or may not contain a node matching these requirements
 * < query: query conditions for the criteria that the node must satisfy 
 */
//DepsNode *DEG_node_find(const Depsgraph *graph, DEG_QueryConditions *query);

/* Topology Queries (Direct) ---------------------- */

/* Get list of nodes which directly depend on given node  
 *
 * > result: list to write results to
 * < node: the node to find the children/dependents of
 */
void DEG_node_get_children(struct ListBase *result, const struct DepsNode *node);


/* Get list of nodes which given node directly depends on 
 *
 * > result: list to write results to
 * < node: the node to find the dependencies of
 */
void DEG_node_get_dependencies(struct ListBase *result, const struct DepsNode *node);


/* Topology Queries (Subgraph) -------------------- */
// XXX: given that subgraphs potentially involve many interconnected nodes, we currently
//      just spit out a copy of the subgraph which matches. This works well for the cases
//      where these are used - mostly for efficient updating of subsets of the nodes.

// XXX: allow supplying a filter predicate to provide further filtering/pruning?


/* Get all descendants of a node
 *
 * That is, get the subgraph / subset of nodes which are dependent
 * on the results of the given node.
 */
Depsgraph *DEG_node_get_descendants(const struct Depsgraph *graph, const struct DepsNode *node);


/* Get all ancestors of a node 
 *
 * That is, get the subgraph / subset of nodes which the given node
 * is dependent on in order to be evaluated.
 */
Depsgraph *DEG_node_get_ancestors(const struct Depsgraph *graph, const struct DepsNode *node);

/* ************************************************ */
/* Higher-Level Queries */

/* Get ID-blocks which would be affected if specified ID is modified 
 * < only_direct: True = Only ID-blocks with direct relationships to ID-block will be returned
 *
 * > result: (LinkData : ID) a list of ID-blocks matching the specified criteria
 * > returns: number of matching ID-blocks
 */
size_t DEG_query_affected_ids(struct ListBase *result, const struct ID *id, const bool only_direct);


/* Get ID-blocks which are needed to update/evaluate specified ID 
 * < only_direct: True = Only ID-blocks with direct relationships to ID-block will be returned
 *
 * > result: (LinkData : ID) a list of ID-blocks matching the specified criteria
 * > returns: number of matching ID-blocks
 */
size_t DEG_query_required_ids(struct ListBase *result, const struct ID *id, const bool only_direct);

/* ************************************************ */

/* Check if given ID type was tagged for update. */
bool DEG_id_type_tagged(struct Main *bmain, short idtype);

/* Get additional evaluation flags for the given ID. */
short DEG_get_eval_flags_for_id(struct Depsgraph *graph, struct ID *id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_QUERY_H__ */
