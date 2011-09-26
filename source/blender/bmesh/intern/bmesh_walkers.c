/**
 *  bmesh_walkers.c    april 2011
 *
 *	BMesh Walker API.
 *
 * $Id: $
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
 functions save their state onto a stack, and also push new states
 to implement recursive or looping behaviour.  generally only one
 state push per call with a specific state is desired.

 basic design pattern: the walker step function goes through it's
 list of possible choices for recursion, and recurses (by pushing a new state)
 using the first non-visited one.  this choise is the flagged as visited using
 the ghash.  each step may push multiple new states onto the stack at once.

 * walkers use tool flags, not header flags
 * walkers now use ghash for storing visited elements, 
   rather then stealing flags.  ghash can be rewritten 
   to be faster if necassary, in the far future :) .
 * tools should ALWAYS have necassary error handling
   for if walkers fail.
*/


/* Pointer hiding*/
typedef struct bmesh_walkerGeneric{
	struct bmesh_walkerGeneric *prev;
} bmesh_walkerGeneric;


void *BMW_Begin(BMWalker *walker, void *start) {
	walker->begin(walker, start);
	
	return walker->currentstate ? walker->step(walker) : NULL;
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
	}
	
	walker->stack = BLI_mempool_create(walker->structsize, 100, 100, 1, 0);
	walker->currentstate = NULL;
}

/*
 * BMW_End
 *
 * Frees a walker's stack.
 *
*/

void BMW_End(BMWalker *walker)
{
	BLI_mempool_destroy(walker->stack);
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

	while(walker->currentstate){
		current = walker->step(walker);
		if(current) return current;
	}
	return NULL;
}

/*
 * BMW_popstate
 *
 * Pops the current walker state off the stack
 * and makes the previous state current
 *
*/

void BMW_popstate(BMWalker *walker)
{
	void *oldstate;
	oldstate = walker->currentstate;
	walker->currentstate 
		= ((bmesh_walkerGeneric*)walker->currentstate)->prev;
	BLI_mempool_free(walker->stack, oldstate);
}

/*
 * BMW_pushstate
 *
 * Pushes the current state down the stack and allocates
 * a new one.
 *
*/

void BMW_pushstate(BMWalker *walker)
{
	bmesh_walkerGeneric *newstate;
	newstate = BLI_mempool_alloc(walker->stack);
	newstate->prev = walker->currentstate;
	walker->currentstate = newstate;
}

void BMW_reset(BMWalker *walker) {
	while (walker->currentstate) {
		BMW_popstate(walker);
	}
}
