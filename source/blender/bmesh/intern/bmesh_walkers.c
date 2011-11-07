/*
 *
 *	BMesh Walker API.
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"
#include "BLI_mempool.h"
#include "BLI_array.h"

#include "bmesh.h"

#include "bmesh_private.h"
#include "bmesh_walkers_private.h"

/*
 - joeedh -
 design notes:

 original desing: walkers directly emulation recursive functions.
 functions save their state onto a worklist, and also add new states
 to implement recursive or looping behaviour.  generally only one
 state push per call with a specific state is desired.

 basic design pattern: the walker step function goes through it's
 list of possible choices for recursion, and recurses (by pushing a new state)
 using the first non-visited one.  this choise is the flagged as visited using
 the ghash.  each step may push multiple new states onto the worklist at once.

 * walkers use tool flags, not header flags
 * walkers now use ghash for storing visited elements, 
   rather then stealing flags.  ghash can be rewritten 
   to be faster if necassary, in the far future :) .
 * tools should ALWAYS have necassary error handling
   for if walkers fail.
*/

void *BMW_Begin(BMWalker *walker, void *start)
{
	walker->begin(walker, start);
	
	return BMW_currentstate(walker) ? walker->step(walker) : NULL;
}

/*
 * BMW_CREATE
 * 
 * Allocates and returns a new mesh walker of 
 * a given type. The elements visited are filtered
 * by the bitmask 'searchmask'.
 *
*/

void BMW_Init(BMWalker *walker, BMesh *bm, int type, int searchmask, int flag)
{
	memset(walker, 0, sizeof(BMWalker));

	walker->flag = flag;
	walker->bm = bm;
	walker->restrictflag = searchmask;
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 1");
	
	if (type >= BMW_MAXWALKERS || type < 0) {
		bmesh_error();
		fprintf(stderr, "Invalid walker type in BMW_Init; type: %d, searchmask: %d, flag: %d\n", type, searchmask, flag);
	}
	
	if (type != BMW_CUSTOM) {
		walker->begin = bm_walker_types[type]->begin;
		walker->yield = bm_walker_types[type]->yield;
		walker->step = bm_walker_types[type]->step;
		walker->structsize = bm_walker_types[type]->structsize;
		walker->order = bm_walker_types[type]->order;
	}
	
	walker->worklist = BLI_mempool_create(walker->structsize, 100, 100, 1, 0);
	walker->states.first = walker->states.last = NULL;
}

/*
 * BMW_End
 *
 * Frees a walker's worklist.
 *
*/

void BMW_End(BMWalker *walker)
{
	BLI_mempool_destroy(walker->worklist);
	BLI_ghash_free(walker->visithash, NULL, NULL);
}


/*
 * BMW_Step
 *
*/

void *BMW_Step(BMWalker *walker)
{
	BMHeader *head;

	head = BMW_walk(walker);

	return head;
}

/*
 * BMW_CurrentDepth
 *
 * Returns the current depth of the walker.
 *
*/

int BMW_CurrentDepth(BMWalker *walker)
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
 *
*/

void *BMW_walk(BMWalker *walker)
{
	void *current = NULL;

	while(BMW_currentstate(walker)){
		current = walker->step(walker);
		if(current) return current;
	}
	return NULL;
}

/*
 * BMW_currentstate
 *
 * Returns the first state from the walker state
 * worklist. This state is the the next in the
 * worklist for processing.
 *
*/

void* BMW_currentstate(BMWalker *walker)
{
	bmesh_walkerGeneric *currentstate = walker->states.first;
	if (currentstate) {
		/* Automatic update of depth. For most walkers that
		   follow the standard "Step" pattern of:
		    - read current state
		    - remove current state
		    - push new states
		    - return walk result from just-removed current state
		   this simple automatic update should keep track of depth
		   just fine. Walkers that deviate from that pattern may
		   need to manually update the depth if they care about
		   keeping it correct. */
		walker->depth = currentstate->depth + 1;
	}
	return currentstate;
}

/*
 * BMW_removestate
 *
 * Remove and free an item from the end of the walker state
 * worklist.
 *
*/

void BMW_removestate(BMWalker *walker)
{
	void *oldstate;
	oldstate = BMW_currentstate(walker);
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
 *
*/

void* BMW_addstate(BMWalker *walker)
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
 *
*/

void BMW_reset(BMWalker *walker)
{
	while (BMW_currentstate(walker)) {
		BMW_removestate(walker);
	}
	walker->depth = 0;
	BLI_ghash_free(walker->visithash, NULL, NULL);
	walker->visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh walkers 1");
}
