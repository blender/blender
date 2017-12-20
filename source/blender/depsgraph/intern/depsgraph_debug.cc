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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_debug.cc
 *  \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_scene_types.h"
}  /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_build.h"

#include "intern/depsgraph_intern.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_time.h"

#include "util/deg_util_foreach.h"

bool DEG_debug_compare(const struct Depsgraph *graph1,
                       const struct Depsgraph *graph2)
{
	BLI_assert(graph1 != NULL);
	BLI_assert(graph2 != NULL);
	const DEG::Depsgraph *deg_graph1 = reinterpret_cast<const DEG::Depsgraph *>(graph1);
	const DEG::Depsgraph *deg_graph2 = reinterpret_cast<const DEG::Depsgraph *>(graph2);
	if (deg_graph1->operations.size() != deg_graph2->operations.size()) {
		return false;
	}
	/* TODO(sergey): Currently we only do real stupid check,
	 * which is fast but which isn't 100% reliable.
	 *
	 * Would be cool to make it more robust, but it's good enough
	 * for now. Also, proper graph check is actually NP-complex
	 * problem..
	 */
	return true;
}

bool DEG_debug_scene_relations_validate(Main *bmain,
                                        Scene *scene)
{
	Depsgraph *depsgraph = DEG_graph_new();
	bool valid = true;
	DEG_graph_build_from_scene(depsgraph, bmain, scene);
	if (!DEG_debug_compare(depsgraph, scene->depsgraph)) {
		fprintf(stderr, "ERROR! Depsgraph wasn't tagged for update when it should have!\n");
		BLI_assert(!"This should not happen!");
		valid = false;
	}
	DEG_graph_free(depsgraph);
	return valid;
}

bool DEG_debug_consistency_check(Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);

	/* Validate links exists in both directions. */
	foreach (DEG::OperationDepsNode *node, deg_graph->operations) {
		foreach (DEG::DepsRelation *rel, node->outlinks) {
			int counter1 = 0;
			foreach (DEG::DepsRelation *tmp_rel, node->outlinks) {
				if (tmp_rel == rel) {
					++counter1;
				}
			}

			int counter2 = 0;
			foreach (DEG::DepsRelation *tmp_rel, rel->to->inlinks) {
				if (tmp_rel == rel) {
					++counter2;
				}
			}

			if (counter1 != counter2) {
				printf("Relation exists in outgoing direction but not in incoming (%d vs. %d).\n",
				       counter1, counter2);
				return false;
			}
		}
	}

	foreach (DEG::OperationDepsNode *node, deg_graph->operations) {
		foreach (DEG::DepsRelation *rel, node->inlinks) {
			int counter1 = 0;
			foreach (DEG::DepsRelation *tmp_rel, node->inlinks) {
				if (tmp_rel == rel) {
					++counter1;
				}
			}

			int counter2 = 0;
			foreach (DEG::DepsRelation *tmp_rel, rel->from->outlinks) {
				if (tmp_rel == rel) {
					++counter2;
				}
			}

			if (counter1 != counter2) {
				printf("Relation exists in incoming direction but not in outcoming (%d vs. %d).\n",
				       counter1, counter2);
			}
		}
	}

	/* Validate node valency calculated in both directions. */
	foreach (DEG::OperationDepsNode *node, deg_graph->operations) {
		node->num_links_pending = 0;
		node->done = 0;
	}

	foreach (DEG::OperationDepsNode *node, deg_graph->operations) {
		if (node->done) {
			printf("Node %s is twice in the operations!\n",
			       node->identifier().c_str());
			return false;
		}
		foreach (DEG::DepsRelation *rel, node->outlinks) {
			if (rel->to->type == DEG::DEG_NODE_TYPE_OPERATION) {
				DEG::OperationDepsNode *to = (DEG::OperationDepsNode *)rel->to;
				BLI_assert(to->num_links_pending < to->inlinks.size());
				++to->num_links_pending;
			}
		}
		node->done = 1;
	}

	foreach (DEG::OperationDepsNode *node, deg_graph->operations) {
		int num_links_pending = 0;
		foreach (DEG::DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG::DEG_NODE_TYPE_OPERATION) {
				++num_links_pending;
			}
		}
		if (node->num_links_pending != num_links_pending) {
			printf("Valency mismatch: %s, %u != %d\n",
			       node->identifier().c_str(),
			       node->num_links_pending, num_links_pending);
			printf("Number of inlinks: %d\n", (int)node->inlinks.size());
			return false;
		}
	}
	return true;
}

/* ------------------------------------------------ */

/**
 * Obtain simple statistics about the complexity of the depsgraph
 * \param[out] r_outer       The number of outer nodes in the graph
 * \param[out] r_operations  The number of operation nodes in the graph
 * \param[out] r_relations   The number of relations between (executable) nodes in the graph
 */
void DEG_stats_simple(const Depsgraph *graph, size_t *r_outer,
                      size_t *r_operations, size_t *r_relations)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);

	/* number of operations */
	if (r_operations) {
		/* All operations should be in this list, allowing us to count the total
		 * number of nodes.
		 */
		*r_operations = deg_graph->operations.size();
	}

	/* Count number of outer nodes and/or relations between these. */
	if (r_outer || r_relations) {
		size_t tot_outer = 0;
		size_t tot_rels = 0;

		foreach (DEG::IDDepsNode *id_node, deg_graph->id_nodes) {
			tot_outer++;
			GHASH_FOREACH_BEGIN(DEG::ComponentDepsNode *, comp_node, id_node->components)
			{
				tot_outer++;
				foreach (DEG::OperationDepsNode *op_node, comp_node->operations) {
					tot_rels += op_node->inlinks.size();
				}
			}
			GHASH_FOREACH_END();
		}

		DEG::TimeSourceDepsNode *time_source = deg_graph->find_time_source();
		if (time_source != NULL) {
			tot_rels += time_source->inlinks.size();
		}

		if (r_relations) *r_relations = tot_rels;
		if (r_outer)     *r_outer     = tot_outer;
	}
}
