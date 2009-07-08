/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef RE_RAYOBJECT_H
#define RE_RAYOBJECT_H

#include "RE_raytrace.h"
#include <float.h>

/* RayObject
	
	A ray object is everything where we can cast rays like:
		* a face/triangle
		* an octree
		* a bvh tree
		* an octree of bvh's
		* a bvh of bvh's
	
		
	All types of RayObjects can be created by implementing the
	callbacks of the RayObject.

	Due to high computing time evolved with casting on faces
	there is a special type of RayObject (named RayFace)
	which won't use callbacks like other generic nodes.
	
	In order to allow a mixture of RayFace+RayObjects,
	all RayObjects must be 4byte aligned, allowing us to use the
	2 least significant bits (with the mask 0x02) to define the
	type of RayObject.
	
	This leads to 4 possible types of RayObject, but at the moment
	only 2 are used:

	 addr&2  - type of object
	 	0		Self (reserved for each structure)
		1     	RayFace
		2		RayObject (generic with API callbacks)
		3		unused

	0 means it's reserved and has it own meaning inside each ray acceleration structure
	(this way each structure can use the allign offset to determine if a node represents a
	 RayObject primitive, which can be used to save memory)

	You actually don't need to care about this if you are only using the API
	described on RE_raytrace.h
 */
 
typedef struct RayFace
{
	float *v1, *v2, *v3, *v4;
	
	void *ob;
	void *face;
	
} RayFace;

struct RayObject
{
	struct RayObjectAPI *api;
	
};


typedef int  (*RE_rayobject_raycast_callback)(RayObject *, Isect *);
typedef void (*RE_rayobject_add_callback)(RayObject *raytree, RayObject *rayobject);
typedef void (*RE_rayobject_done_callback)(RayObject *);
typedef void (*RE_rayobject_free_callback)(RayObject *);
typedef void (*RE_rayobject_merge_bb_callback)(RayObject *, float *min, float *max);
typedef float (*RE_rayobject_cost_callback)(RayObject *);

typedef struct RayObjectAPI
{
	RE_rayobject_raycast_callback	raycast;
	RE_rayobject_add_callback		add;
	RE_rayobject_done_callback		done;
	RE_rayobject_free_callback		free;
	RE_rayobject_merge_bb_callback	bb;
	RE_rayobject_cost_callback		cost;
	
} RayObjectAPI;

#define RayObject_align(o)				((RayObject*)(((intptr_t)o)&(~3)))
#define RayObject_unalignRayFace(o)		((RayObject*)(((intptr_t)o)|1))
#define RayObject_unalignRayAPI(o)		((RayObject*)(((intptr_t)o)|2))

#define RayObject_isAligned(o)	((((intptr_t)o)&3) == 0)
#define RayObject_isRayFace(o)	((((intptr_t)o)&3) == 1)
#define RayObject_isRayAPI(o)	((((intptr_t)o)&3) == 2)

/*
 * Extend min/max coords so that the rayobject is inside them
 */
void RE_rayobject_merge_bb(RayObject *ob, float *min, float *max);

/*
 * This function differs from RE_rayobject_raycast
 * RE_rayobject_intersect does NOT perform last-hit optimization
 * So this is probably a function to call inside raytrace structures
 */
int RE_rayobject_intersect(RayObject *r, Isect *i);

/*
 * Returns distance ray must travel to hit the given bounding box
 * BB should be in format [2][3]
 */
float RE_rayobject_bb_intersect(const Isect *i, const float *bb);

/*
 * Returns the expected cost of raycast on this node, primitives have a cost of 1
 */
float RE_rayobject_cost(RayObject *r);


#define ISECT_EPSILON ((float)FLT_EPSILON)



#if !defined(_WIN32)

#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#define BENCH(a,name)	\
	do {			\
		double _t1, _t2;				\
		struct timeval _tstart, _tend;	\
		clock_t _clock_init = clock();	\
		gettimeofday ( &_tstart, NULL);	\
		(a);							\
		gettimeofday ( &_tend, NULL);	\
		_t1 = ( double ) _tstart.tv_sec + ( double ) _tstart.tv_usec/ ( 1000*1000 );	\
		_t2 = ( double )   _tend.tv_sec + ( double )   _tend.tv_usec/ ( 1000*1000 );	\
		printf("BENCH:%s: %fs (real) %fs (cpu)\n", #name, _t2-_t1, (float)(clock()-_clock_init)/CLOCKS_PER_SEC);\
	} while(0)
#else

#define BENCH(a)	(a)

#endif


#endif
