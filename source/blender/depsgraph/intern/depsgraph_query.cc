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
 */

/** \file blender/depsgraph/intern/depsgraph_query.cc
 *  \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_main.h"

#include "DEG_depsgraph_query.h"
} /* extern "C" */

#include "depsgraph_queue.h"
#include "depsnode.h"
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

/* ************************* */
/* Low-Level Graph Traversal */

#if 0
/* Prepare for graph traversal, by tagging nodes, etc. */
static void DEG_graph_traverse_begin(Depsgraph * /*graph*/)
{
	/* go over all nodes, initialising the valence counts */
	// XXX: this will end up being O(|V|), which is bad when we're just updating a few nodes...
}

/* Perform a traversal of graph from given starting node (in execution order) */
// TODO: additional flags for controlling the process?
void DEG_graph_traverse_from_node(Depsgraph *graph, OperationDepsNode *start_node,
                                  DEG_FilterPredicate filter, void *filter_data,
                                  DEG_NodeOperation op, void *operation_data)
{
	DepsgraphQueue *q;

	/* sanity checks */
	if (ELEM(NULL, graph, start_node, op))
		return;

	/* add node as starting node to be evaluated, with value of 0 */
	q = DEG_queue_new();

	start_node->num_links_pending = 0;
	DEG_queue_push(q, start_node, 0.0f);

	/* while we still have nodes in the queue, grab and work on next one */
	do {
		/* grab item at front of queue */
		// XXX: in practice, we may need to wait until one becomes available...
		OperationDepsNode *node = (OperationDepsNode *)DEG_queue_pop(q);

		/* perform operation on node */
		op(graph, node, operation_data);

		/* schedule up operations which depend on this */
		DEPSNODE_RELATIONS_ITER_BEGIN(node->outlinks, rel)
		{
			/* ensure that relationship is not tagged for ignoring (i.e. cyclic, etc.) */
			// TODO: cyclic refs should probably all get clustered towards the end, so that we can just stop on the first one
			if ((rel->flag & DEPSREL_FLAG_CYCLIC) == 0) {
				OperationDepsNode *child_node = (OperationDepsNode *)rel->to;

				/* only visit node if the filtering function agrees */
				if ((filter == NULL) || filter(graph, child_node, filter_data)) {
					/* schedule up node... */
					child_node->num_links_pending--;
					DEG_queue_push(q, child_node, (float)child_node->num_links_pending);
				}
			}
		}
		DEPSNODE_RELATIONS_ITER_END;
	} while (DEG_queue_is_empty(q) == false);

	/* cleanup */
	DEG_queue_free(q);
}
#endif

/* ************************************************************** */
/* Filtering API - Basically, making a copy of the existing graph */

/* Create filtering context */
// TODO: allow passing in a number of criteria?
DepsgraphCopyContext *DEG_filter_init()
{
	DepsgraphCopyContext *dcc = (DepsgraphCopyContext *)MEM_callocN(sizeof(DepsgraphCopyContext), "DepsgraphCopyContext");

	/* init hashes for easy lookups */
	dcc->nodes_hash = BLI_ghash_ptr_new("Depsgraph Filter NodeHash");
	dcc->rels_hash = BLI_ghash_ptr_new("Depsgraph Filter Relationship Hash"); // XXX?

	/* store filtering criteria? */
	// xxx...

	return dcc;
}

/* Cleanup filtering context */
void DEG_filter_cleanup(DepsgraphCopyContext *dcc)
{
	/* sanity check */
	if (dcc == NULL)
		return;

	/* free hashes - contents are weren't copied, so are ok... */
	BLI_ghash_free(dcc->nodes_hash, NULL, NULL);
	BLI_ghash_free(dcc->rels_hash, NULL, NULL);

	/* clear filtering criteria */
	// ...

	/* free dcc itself */
	MEM_freeN(dcc);
}

/* -------------------------------------------------- */

/* Create a copy of provided node */
// FIXME: the handling of sub-nodes and links will need to be subject to filtering options...
// XXX: perhaps this really shouldn't be exposed, as it will just be a sub-step of the evaluation process?
DepsNode *DEG_copy_node(DepsgraphCopyContext *dcc, const DepsNode *src)
{
	/* sanity check */
	if (src == NULL)
		return NULL;

	DepsNodeFactory *factory = DEG_get_node_factory(src->type);
	BLI_assert(factory != NULL);
	DepsNode *dst = factory->copy_node(dcc, src);

	/* add this node-pair to the hash... */
	BLI_ghash_insert(dcc->nodes_hash, (DepsNode *)src, dst);

#if 0 /* XXX TODO */
	/* now, fix up any links in standard "node header" (i.e. DepsNode struct, that all
	 * all others are derived from) that are now corrupt
	 */
	{
		/* relationships to other nodes... */
		// FIXME: how to handle links? We may only have partial set of all nodes still?
		// XXX: the exact details of how to handle this are really part of the querying API...

		// XXX: BUT, for copying subgraphs, we'll need to define an API for doing this stuff anyways
		// (i.e. for resolving and patching over links that exist within subtree...)
		dst->inlinks.clear();
		dst->outlinks.clear();

		/* clear traversal data */
		dst->num_links_pending = 0;
		dst->lasttime = 0;
	}

	/* fix links */
	// XXX...
#endif

	/* return copied node */
	return dst;
}

bool DEG_id_type_tagged(Main *bmain, short idtype)
{
	return bmain->id_tag_update[((unsigned char *)&idtype)[0]] != 0;
}

short DEG_get_eval_flags_for_id(Depsgraph *graph, ID *id)
{
	if (graph == NULL) {
		/* Happens when converting objects to mesh from a python script
		 * after modifying scene graph.
		 *
		 * Currently harmless because it's only called for temporary
		 * objects which are out of the DAG anyway.
		 */
		return 0;
	}

	IDDepsNode *id_node = graph->find_id_node(id);
	if (id_node == NULL) {
		/* TODO(sergey): Does it mean we need to check set scene? */
		return 0;
	}

	return id_node->eval_flags;
}
