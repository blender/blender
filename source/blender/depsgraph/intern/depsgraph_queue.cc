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
 * Implementation of special queue type for use in Depsgraph traversals
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_heap.h"
#include "BLI_ghash.h"
} /* extern "C" */

#include "depsgraph_queue.h"

/* ****************************** */
/* Depsgraph Queue implementation */

/* Data Management ----------------------------------------- */

DepsgraphQueue *DEG_queue_new(void)
{
	DepsgraphQueue *q = (DepsgraphQueue *)MEM_callocN(sizeof(DepsgraphQueue), "DEG_queue_new()");

	/* init data structures for use here */
	q->pending_heap = BLI_heap_new();
	q->pending_hash = BLI_ghash_ptr_new("DEG Queue Pending Hash");

	q->ready_heap   = BLI_heap_new();

	/* init settings */
	q->idx = 0;
	q->tot = 0;

	/* return queue */
	return q;
}

void DEG_queue_free(DepsgraphQueue *q)
{
	/* free data structures */
	BLI_assert(BLI_heap_size(q->pending_heap) == 0);
	BLI_assert(BLI_heap_size(q->ready_heap) == 0);
	BLI_assert(BLI_ghash_size(q->pending_hash) == 0);

	BLI_heap_free(q->pending_heap, NULL);
	BLI_heap_free(q->ready_heap, NULL);
	BLI_ghash_free(q->pending_hash, NULL, NULL);

	/* free queue itself */
	MEM_freeN(q);
}

/* Statistics --------------------------------------------- */

/* Get the number of nodes which are we should visit, but are not able to yet */
size_t DEG_queue_num_pending(DepsgraphQueue *q)
{
	return BLI_heap_size(q->pending_heap);
}

/* Get the number of nodes which are now ready to be visited */
size_t DEG_queue_num_ready(DepsgraphQueue *q)
{
	return BLI_heap_size(q->ready_heap);
}

/* Get total size of queue */
size_t DEG_queue_size(DepsgraphQueue *q)
{
	return DEG_queue_num_pending(q) + DEG_queue_num_ready(q);
}

/* Check if queue has any items in it (still passing through) */
bool DEG_queue_is_empty(DepsgraphQueue *q)
{
	return DEG_queue_size(q) == 0;
}

/* Queue Operations --------------------------------------- */

/**
 * Add DepsNode to the queue
 * \param dnode: ``(DepsNode *)`` node to add to the queue
 * Each node is only added once to the queue; Subsequent pushes
 * merely update its status (e.g. moving it from "pending" to "ready")
 * \param cost: new "num_links_pending" count for node *after* it has encountered
 * via an outlink from the node currently being visited
 * (i.e. we're one of the dependencies which may now be able to be processed)
 */
void DEG_queue_push(DepsgraphQueue *q, void *dnode, float cost)
{
	HeapNode *hnode = NULL;

	/* Shortcut: Directly add to ready if node isn't waiting on anything now... */
	if (cost == 0) {
		/* node is now ready to be visited - schedule it up for such */
		if (BLI_ghash_haskey(q->pending_hash, dnode)) {
			/* remove from pending queue - we're moving it to the scheduling queue */
			hnode = (HeapNode *)BLI_ghash_lookup(q->pending_hash, dnode);
			BLI_heap_remove(q->pending_heap, hnode);

			BLI_ghash_remove(q->pending_hash, dnode, NULL, NULL);
		}

		/* schedule up node using latest count (of ready nodes) */
		BLI_heap_insert(q->ready_heap, (float)q->idx, dnode);
		q->idx++;
	}
	else {
		/* node is still waiting on some other ancestors,
		 * so add it to the pending heap in the meantime...
		 */
		// XXX: is this even necessary now?
		if (BLI_ghash_haskey(q->pending_hash, dnode)) {
			/* just update cost on pending node */
			hnode = (HeapNode *)BLI_ghash_lookup(q->pending_hash, dnode);
			BLI_heap_remove(q->pending_heap, hnode);
			BLI_heap_insert(q->pending_heap, cost, hnode);
		}
		else {
			/* add new node to pending queue, and increase size of overall queue */
			hnode = BLI_heap_insert(q->pending_heap, cost, dnode);
			q->tot++;
		}
	}
}

/* Grab a "ready" node from the queue */
void *DEG_queue_pop(DepsgraphQueue *q)
{
	/* sanity check: if there are no "ready" nodes,
	 * start pulling from "pending" to keep things moving,
	 * but throw a warning so that we know that something's up here...
	 */
	if (BLI_heap_is_empty(q->ready_heap)) {
		// XXX: this should never happen
		// XXX: if/when it does happen, we may want instead to just wait until something pops up here...
		printf("DepsgraphHeap Warning: No more ready nodes available. Trying from pending (idx = %d, tot = %d, pending = %d, ready = %d)\n",
		       (int)q->idx, (int)q->tot, (int)DEG_queue_num_pending(q), (int)DEG_queue_num_ready(q));

		return BLI_heap_popmin(q->pending_heap);
	}
	else {
		/* only grab "ready" nodes */
		return BLI_heap_popmin(q->ready_heap);
	}
}
