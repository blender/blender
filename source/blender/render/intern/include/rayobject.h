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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#ifdef __cplusplus
extern "C" {
#endif

#include "RE_raytrace.h"
#include "render_types.h"
#include <stdio.h>
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
	2 least significant bits (with the mask 0x03) to define the
	type of RayObject.
	
	This leads to 4 possible types of RayObject:

	 addr&3  - type of object
		 0		Self (reserved for each structure)
		1     	RayFace (tri/quad primitive)
		2		RayObject (generic with API callbacks)
		3		VlakPrimitive
				(vlak primitive - to be used when we have a vlak describing the data
				 eg.: on render code)

	0 means it's reserved and has it own meaning inside each ray acceleration structure
	(this way each structure can use the allign offset to determine if a node represents a
	 RayObject primitive, which can be used to save memory)

	You actually don't need to care about this if you are only using the API
	described on RE_raytrace.h
 */

/* used to align a given ray object */
#define RE_rayobject_align(o)				((RayObject*)(((intptr_t)o)&(~3)))

/* used to unalign a given ray object */
#define RE_rayobject_unalignRayFace(o)		((RayObject*)(((intptr_t)o)|1))
#define RE_rayobject_unalignRayAPI(o)		((RayObject*)(((intptr_t)o)|2))
#define RE_rayobject_unalignVlakPrimitive(o)	((RayObject*)(((intptr_t)o)|3))

/* used to test the type of ray object */
#define RE_rayobject_isAligned(o)	((((intptr_t)o)&3) == 0)
#define RE_rayobject_isRayFace(o)	((((intptr_t)o)&3) == 1)
#define RE_rayobject_isRayAPI(o)	((((intptr_t)o)&3) == 2)
#define RE_rayobject_isVlakPrimitive(o)	((((intptr_t)o)&3) == 3)



/*
 * This class is intended as a place holder for control, configuration of the rayobject like:
 *	- stop building (TODO maybe when porting build to threads this could be implemented with some thread_cancel function)
 *  - max number of threads and threads callback to use during build
 *	...
 */	
typedef int  (*RE_rayobjectcontrol_test_break_callback)(void *data);
typedef struct RayObjectControl RayObjectControl;
struct RayObjectControl
{
	void *data;
	RE_rayobjectcontrol_test_break_callback test_break;	
};

/*
 * This rayobject represents a generic object. With it's own callbacks for raytrace operations.
 * It's suitable to implement things like LOD.
 */
struct RayObject
{
	struct RayObjectAPI *api;

	struct RayObjectControl control;
};




typedef int  (*RE_rayobject_raycast_callback)(RayObject *, Isect *);
typedef void (*RE_rayobject_add_callback)(RayObject *raytree, RayObject *rayobject);
typedef void (*RE_rayobject_done_callback)(RayObject *);
typedef void (*RE_rayobject_free_callback)(RayObject *);
typedef void (*RE_rayobject_merge_bb_callback)(RayObject *, float *min, float *max);
typedef float (*RE_rayobject_cost_callback)(RayObject *);
typedef void (*RE_rayobject_hint_bb_callback)(RayObject *, RayHint *, float *, float *);

typedef struct RayObjectAPI
{
	RE_rayobject_raycast_callback	raycast;
	RE_rayobject_add_callback		add;
	RE_rayobject_done_callback		done;
	RE_rayobject_free_callback		free;
	RE_rayobject_merge_bb_callback	bb;
	RE_rayobject_cost_callback		cost;
	RE_rayobject_hint_bb_callback	hint_bb;
	
} RayObjectAPI;


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
/* float RE_rayobject_bb_intersect(const Isect *i, const float *bb); */
int RE_rayobject_bb_intersect_test(const Isect *i, const float *bb); /* same as bb_intersect but doens't calculates distance */

/*
 * Returns the expected cost of raycast on this node, primitives have a cost of 1
 */
float RE_rayobject_cost(RayObject *r);


/*
 * Returns true if for some reason a heavy processing function should stop
 * (eg.: user asked to stop during a tree a build)
 */
int RE_rayobjectcontrol_test_break(RayObjectControl *c);


#define ISECT_EPSILON ((float)FLT_EPSILON)



#if !defined(_WIN32) && !defined(_WIN64)

#include <sys/time.h>
#include <time.h>

#define BENCH(a,name)	\
	{			\
		double _t1, _t2;				\
		struct timeval _tstart, _tend;	\
		clock_t _clock_init = clock();	\
		gettimeofday ( &_tstart, NULL);	\
		(a);							\
		gettimeofday ( &_tend, NULL);	\
		_t1 = ( double ) _tstart.tv_sec + ( double ) _tstart.tv_usec/ ( 1000*1000 );	\
		_t2 = ( double )   _tend.tv_sec + ( double )   _tend.tv_usec/ ( 1000*1000 );	\
		printf("BENCH:%s: %fs (real) %fs (cpu)\n", #name, _t2-_t1, (float)(clock()-_clock_init)/CLOCKS_PER_SEC);\
	}
#else

#define BENCH(a,name)	(a)

#endif



#ifdef __cplusplus
}
#endif


#endif
