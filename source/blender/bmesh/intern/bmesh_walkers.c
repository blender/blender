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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_walkers.c
 *  \ingroup bmesh
 *
 * BMesh Walker API.
 */

#include <stdlib.h>



#include "BLI_listbase.h"

#include "bmesh.h"

#include "bmesh_walkers_private.h"

/* - joeedh -
 * design notes:
 *
 * original desing: walkers directly emulation recursive functions.
 * functions save their state onto a worklist, and also add new states
 * to implement recursive or looping behaviour.  generally only one
 * state push per call with a specific state is desired.
 *
 * basic design pattern: the walker step function goes through it's
 * list of possible choices for recursion, and recurses (by pushing a new state)
 * using the first non-visited one.  this choise is the flagged as visited using
 * the ghash.  each step may push multiple new states onto the worklist at once.
 *
 * - walkers use tool flags, not header flags
 * - walkers now use ghash for storing visited elements,
 *   rather then stealing flags.  ghash can be rewritten
 *   to be faster if necassary, in the far future :) .
 * - tools should ALWAYS have necassary error handling
 *   for if walkers fail.
 */

void *BMW_begin(BMWalker *walker, void *start)
{
	walker->begin(walker, start);
	
	return BMW_current_state(walker) ? walker->step(walker) : NULL;
}

/*
 * BMW_CREATE
 *
 * Allocates and returns a new mesh walker of
 * a given type. The elements visited are filtered
 * by the bitmask 'searchmask'.
 */

void BMW_init(BMWalker *walker, BMesh *bm, int type,
              short mask_vert, short mask_edge, short mask_loop, short mask_face,
              int layer)
{
	memset(walker, 0, sizeof(BMWalker));

	walker->layer = layer;
	walker->bm = bm;

	walker->mask_vert = mask_vert;
	walker->mask_edge = mask_edge;
	walker->mask_loop = mask_loop;
	walker->mask_face = mask_face;

	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 1");
	
	if (UNLIKELY(type >= BMW_MAXWALKERS || type < 0)) {
		fprintf(stderr,
		        "Invalid walker type in BMW_init; type: %d, "
		        "searchmask: (v:%d, e:%d, l:%d, f:%d), flag: %d\n",
		        type, mask_vert, mask_edge, mask_loop, mask_face, layer);
		BMESH_ASSERT(0);
	}
	
	if (type != BMW_CUSTOM) {
		walker->begin = bm_walker_types[type]->begin;
		walker->yield = bm_walker_types[type]->yield;
		walker->step = bm_walker_types[type]->step;
		walker->structsize = bm_walker_types[type]->structsize;
		walker->order = bm_walker_types[type]->order;
		walker->valid_mask = bm_walker_types[type]->valid_mask;

		/* safety checks */
		/* if this raises an error either the caller is wrong or
		 * 'bm_walker_types' needs updating */
		BLI_assert(mask_vert == 0 || (walker->valid_mask & BM_VERT));
		BLI_assert(mask_edge == 0 || (walker->valid_mask & BM_EDGE));
		BLI_assert(mask_loop == 0 || (walker->valid_mask & BM_LOOP));
		BLI_assert(mask_face == 0 || (walker->valid_mask & BM_FACE));
	}
	
	walker->worklist = BLI_mempool_create(walker->structsize, 100, 100, TRUE, FALSE);
	walker->states.first = walker->states.last = NULL;
}

/*
 * BMW_end
 *
 * Frees a walker's worklist.
 */

void BMW_end(BMWalker *walker)
{
	BLI_mempool_destroy(walker->worklist);
	BLI_ghash_free(walker->visithash, NULL, NULL);
}


/*
 * BMW_step
 */

void *BMW_step(BMWalker *walker)
{
	BMHeader *head;

	head = BMW_walk(walker);

	return head;
}

/*
 * BMW_current_depth
 *
 * Returns the current depth of the walker.
 */

int BMW_current_depth(BMWalker *walker)
{
	return walker->depth;
}

/*
 * BMW_WALK
 *
 * Steps a mesh walker forward by one element
 *
 * BMESH_TODO:
 *  -add searchmask filtering
 */

void *BMW_walk(BMWalker *walker)
{
	void *current = NULL;

	while (BMW_current_state(walker)) {
		current = walker->step(walker);
		if (current) {
			return current;
		}
	}
	return NULL;
}

/*
 * BMW_current_state
 *
 * Returns the first state from the walker state
 * worklist. This state is the the next in the
 * worklist for processing.
 */

void *BMW_current_state(BMWalker *walker)
{
	bmesh_walkerGeneric *currentstate = walker->states.first;
	if (currentstate) {
		/* Automatic update of depth. For most walkers that
		 * follow the standard "Step" pattern of:
		 *  - read current state
		 *  - remove current state
		 *  - push new states
		 *  - return walk result from just-removed current state
		 * this simple automatic update should keep track of depth
		 * just fine. Walkers that deviate from that pattern may
		 * need to manually update the depth if they care about
		 * keeping it correct. */
		walker->depth = currentstate->depth + 1;
	}
	return currentstate;
}

/*
 * BMW_state_remove
 *
 * Remove and free an item from the end of the walker state
 * worklist.
 */

void BMW_state_remove(BMWalker *walker)
{
	void *oldstate;
	oldstate = BMW_current_state(walker);
	BLI_remlink(&walker->states, oldstate);
	BLI_mempool_free(walker->worklist, oldstate);
}

/*
 * BMW_newstate
 *
 * Allocate a new empty state and put it on the worklist.
 * A pointer to the new state is returned so that the caller
 * can fill in the state data. The new state will be inserted
 * at the front for depth-first walks, and at the end for
 * breadth-first walks.
 */

void *BMW_state_add(BMWalker *walker)
{
	bmesh_walkerGeneric *newstate;
	newstate = BLI_mempool_alloc(walker->worklist);
	newstate->depth = walker->depth;
	switch (walker->order)
	{
		case BMW_DEPTH_FIRST:
			BLI_addhead(&walker->states, newstate);
			break;
		case BMW_BREADTH_FIRST:
			BLI_addtail(&walker->states, newstate);
			break;
		default:
			BLI_assert(0);
			break;
	}
	return newstate;
}

/*
 * BMW_reset
 *
 * Frees all states from the worklist, resetting the walker
 * for reuse in a new walk.
 */

void BMW_reset(BMWalker *walker)
{
	while (BMW_current_state(walker)) {
		BMW_state_remove(walker);
	}
	walker->depth = 0;
	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 1");
}
