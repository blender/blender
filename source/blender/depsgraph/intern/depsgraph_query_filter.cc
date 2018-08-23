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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_query_filter.cc
 *  \ingroup depsgraph
 *
 * Implementation of Graph Filtering API
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include <string.h> // XXX: memcpy

#include "BLI_utildefines.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "BKE_action.h" // XXX: BKE_pose_channel_from_name
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DEG_depsgraph_debug.h"

#include "util/deg_util_foreach.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"


/* *************************************************** */
/* Graph Filtering Internals */

namespace DEG {

/* UserData for deg_add_retained_id_cb */
struct RetainedIdUserData {
	DEG_FilterQuery *query;
	GSet *set;
};

/* Helper for DEG_foreach_ancestor_id()
 * Keep track of all ID's encountered in a set
 */
void deg_add_retained_id_cb(ID *id, void *user_data)
{
	RetainedIdUserData *data = (RetainedIdUserData *)user_data;
	BLI_gset_add(data->set, (void *)id);
}

/* ------------------------------------------- */

/* Remove relations pointing to the given OperationDepsNode */
/* TODO: Make this part of OperationDepsNode? */
void deg_unlink_opnode(Depsgraph *graph, OperationDepsNode *op_node)
{
	std::vector<DepsRelation *> all_links;
	
	/* Collect all inlinks to this operation */
	foreach (DepsRelation *rel, op_node->inlinks) {
		all_links.push_back(rel);
	}
	/* Collect all outlinks from this operation */
	foreach (DepsRelation *rel, op_node->outlinks) {
		all_links.push_back(rel);
	}
	
	/* Delete all collected relations */
	foreach (DepsRelation *rel, all_links) {
		rel->unlink();
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
	
	/* Remove from entry tags */
	if (BLI_gset_haskey(graph->entry_tags, op_node)) {
		BLI_gset_remove(graph->entry_tags, op_node, NULL);
	}
}

/* Remove and free given ID Node */
// XXX: Use id_cow or id_orig?
bool deg_filter_free_idnode(Depsgraph *graph, IDDepsNode *id_node,
                            const std::function <bool (ID_Type id_type)>& filter)
{
	if (id_node->done == 0) {
		/* This node has not been marked for deletion */
		return false;
	}
	else {
		const ID_Type id_type = GS(id_node->id_orig->name);
		if (filter(id_type)) {
			printf("  id_type (T) = %d ");
			id_node->destroy();
			return true;
		}
		else {
			printf("  id_type (F) = %d ");
			return false;
		}
	}
}

/* Remove and free ID Nodes of a particular type from the graph
 *
 * See Depsgraph::clear_id_nodes() and Depsgraph::clear_id_nodes_conditional()
 * for more details about why we need these type filters
 */
void deg_filter_clear_ids_conditional(
        Depsgraph *graph,
        const std::function <bool (ID_Type id_type)>& filter)
{
	/* Based on Depsgraph::clear_id_nodes_conditional()... */
	for (Depsgraph::IDDepsNodes::const_iterator it = graph->id_nodes.begin();
	     it != graph->id_nodes.end();
	     )
	{
		IDDepsNode *id_node = *it;
		ID *id = id_node->id_orig;
		
		if (deg_filter_free_idnode(graph, id_node, filter)) {
			/* Node data got destroyed. Remove from collections, and free */
			printf("  culling %s\n", id->name);
			BLI_ghash_remove(graph->id_hash, id, NULL, NULL);
			it = graph->id_nodes.erase(it);
			OBJECT_GUARDED_DELETE(id_node, IDDepsNode);
		}
		else {
			/* Node wasn't freed. Increment iterator */
			printf("  skipping %s\n", id->name);
			++it;
		}
	}
}

/* Remove every ID Node (and its associated subnodes, COW data) */
void deg_filter_remove_unwanted_ids(Depsgraph *graph, GSet *retained_ids)
{
	/* 1) First pass over ID nodes + their operations
	 * - Identify and tag ID's (via "done = 1") to be removed
	 * - Remove all links to/from operations that will be removed
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		id_node->done = !BLI_gset_haskey(retained_ids, (void *)id_node->id_orig);
		if (id_node->done) {
			GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
			{
				foreach (OperationDepsNode *op_node, comp_node->operations) {
					deg_unlink_opnode(graph, op_node);
				}
			}
			GHASH_FOREACH_END();
		}
	}
	
	/* 2) Remove unwanted operations from graph->operations */
	for (Depsgraph::OperationNodes::const_iterator it_opnode = graph->operations.begin();
	     it_opnode != graph->operations.end();
	     )
	{
		OperationDepsNode *op_node = *it_opnode;
		IDDepsNode *id_node = op_node->owner->owner;
		if (id_node->done) {
			it_opnode = graph->operations.erase(it_opnode);
		}
		else {
			++it_opnode;
		}
	}
	
	/* Free ID nodes that are no longer wanted
	 * NOTE: See clear_id_nodes() for more details about what's happening here
	 *       (e.g. regarding the lambdas used for freeing order hacks)
	 */
	printf("Culling ID's scene:\n");
	deg_filter_clear_ids_conditional(graph,  [](ID_Type id_type) { return id_type == ID_SCE; });
	printf("Culling ID's other:\n");
	deg_filter_clear_ids_conditional(graph,  [](ID_Type id_type) { return id_type != ID_PA; });
}

} //namespace DEG

/* *************************************************** */
/* Graph Filtering API */

/* Obtain a new graph instance that only contains the nodes needed */
Depsgraph *DEG_graph_filter(const Depsgraph *graph_src, Main *bmain, DEG_FilterQuery *query)
{
	const DEG::Depsgraph *deg_graph_src = reinterpret_cast<const DEG::Depsgraph *>(graph_src);
	if (deg_graph_src == NULL) {
		return NULL;
	}
	
	/* Construct a full new depsgraph based on the one we got */
	/* TODO: Improve the builders to not add any ID nodes we don't need later (e.g. ProxyBuilder?) */
	Depsgraph *graph_new = DEG_graph_new(deg_graph_src->scene,
	                                     deg_graph_src->view_layer,
	                                     DAG_EVAL_BACKGROUND);
	DEG_graph_build_from_view_layer(graph_new,
	                                bmain,
	                                deg_graph_src->scene,
	                                deg_graph_src->view_layer);
	
	/* Build a set of all the id's we want to keep */
	GSet *retained_ids = BLI_gset_ptr_new(__func__);
	DEG::RetainedIdUserData retained_id_data = {query, retained_ids};
	
	LISTBASE_FOREACH(DEG_FilterTarget *, target, &query->targets) {
		/* Target Itself */
		BLI_gset_add(retained_ids, (void *)target->id);
		
		/* Target's Ancestors (i.e. things it depends on) */
		DEG_foreach_ancestor_ID(graph_new,
		                        target->id,
		                        DEG::deg_add_retained_id_cb,
		                        &retained_id_data);
	}
	
	/* Remove everything we don't want to keep around anymore */
	DEG::Depsgraph *deg_graph_new = reinterpret_cast<DEG::Depsgraph *>(graph_new);
	if (BLI_gset_len(retained_ids) > 0) {
		DEG::deg_filter_remove_unwanted_ids(deg_graph_new, retained_ids);
	}
	// TODO: query->LOD filters
	
	/* Free temp data */
	BLI_gset_free(retained_ids, NULL);
	retained_ids = NULL;
	
	/* Debug - Are the desired targets still in there? */
	printf("Filtered Graph Sanity Check - Do targets exist?:\n");
	LISTBASE_FOREACH(DEG_FilterTarget *, target, &query->targets) {
		printf("   %s -> %d\n", target->id->name, BLI_ghash_haskey(deg_graph_new->id_hash, target->id));
	}
	printf("Filtered Graph Sanity Check - Remaining ID Nodes:\n");
	size_t id_node_idx = 0;
	foreach (DEG::IDDepsNode *id_node, deg_graph_new->id_nodes) {
		printf("  %d: %s\n", id_node_idx++, id_node->id_orig->name);
	}
	
	/* Print Stats */
	// XXX: Hide behind debug flags
	size_t s_outer, s_operations, s_relations;
	size_t s_ids = deg_graph_src->id_nodes.size();
	unsigned int s_idh = BLI_ghash_len(deg_graph_src->id_hash);
	
	size_t n_outer, n_operations, n_relations;
	size_t n_ids = deg_graph_new->id_nodes.size();
	unsigned int n_idh = BLI_ghash_len(deg_graph_new->id_hash);
	
	DEG_stats_simple(graph_src, &s_outer, &s_operations, &s_relations);
	DEG_stats_simple(graph_new, &n_outer, &n_operations, &n_relations);
	
	printf("%s: src = (ID's: %u (%u), Out: %u, Op: %u, Rel: %u)\n", __func__, s_ids, s_idh, s_outer, s_operations, s_relations); // XXX
	printf("%s: new = (ID's: %u (%u), Out: %u, Op: %u, Rel: %u)\n", __func__, n_ids, n_idh, n_outer, n_operations, n_relations); // XXX
	
	/* Return this new graph instance */
	return graph_new;
}

/* *************************************************** */
