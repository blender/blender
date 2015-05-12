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
 * Defines for special queue type for use in Depsgraph traversals
 */

#ifndef __DEPSGRAPH_QUEUE_H__
#define __DEPSGRAPH_QUEUE_H__

struct DepsNode;

struct Heap;
struct GHash;

/* *********************************************** */
/* Dependency Graph Traversal Queue
 *
 * There are two parts to this:
 * a) "Pending" Nodes - This part contains the set of nodes
 *    which are related to those which have been visited
 *    previously, but are not yet ready to actually be visited.
 * b) "Scheduled" Nodes - These are the nodes whose ancestors
 *    have all been evaluated already, which means that any
 *    or all of them can be picked (in practically in order) to
 *    be visited immediately.
 *
 * Internally, the queue makes sure that each node in the graph
 * only gets added to the queue once. This is because there can
 * be multiple inlinks to each node given the way that the relations
 * work.
 */

/* Depsgraph Queue Type */
typedef struct DepsgraphQueue {
	/* Pending */
	struct Heap *pending_heap;         /* (valence:int, DepsNode*) */
	struct GHash *pending_hash;        /* (DepsNode* : HeapNode*>) */

	/* Ready to be visited - fifo */
	struct Heap *ready_heap;           /* (idx:int, DepsNode*) */

	/* Size/Order counts */
	size_t idx;                        /* total number of nodes which are/have been ready so far (including those already visited) */
	size_t tot;                        /* total number of nodes which have passed through queue; mainly for debug */
} DepsgraphQueue;

/* ************************** */
/* Depsgraph Queue Operations */

/* Data management */
DepsgraphQueue *DEG_queue_new(void);
void DEG_queue_free(DepsgraphQueue *q);

/* Statistics */
size_t DEG_queue_num_pending(DepsgraphQueue *q);
size_t DEG_queue_num_ready(DepsgraphQueue *q);

size_t DEG_queue_size(DepsgraphQueue *q);
bool DEG_queue_is_empty(DepsgraphQueue *q);

/* Operations */
void DEG_queue_push(DepsgraphQueue *q, void *dnode, float cost = 0.0f);
void *DEG_queue_pop(DepsgraphQueue *q);

#endif  /* DEPSGRAPH_QUEUE_H */
