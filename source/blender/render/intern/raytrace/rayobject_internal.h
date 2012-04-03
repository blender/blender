/** \file blender/render/intern/raytrace/rayobject_internal.h
 *  \ingroup render
 */

#ifndef __RAYOBJECT_INTERNAL_H__
#define __RAYOBJECT_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* RayObjectControl
 *
 * This class is intended as a place holder for control, configuration of the
 * rayobject like:
 *	- stop building (TODO maybe when porting build to threads this could be
 *    implemented with some thread_cancel function)
 *  - max number of threads and threads callback to use during build
 *	...
 */	

typedef int  (*RE_rayobjectcontrol_test_break_callback)(void *data);

typedef struct RayObjectControl {
	void *data;
	RE_rayobjectcontrol_test_break_callback test_break;	
} RayObjectControl;

/* Returns true if for some reason a heavy processing function should stop
 * (eg.: user asked to stop during a tree a build)
 */

int RE_rayobjectcontrol_test_break(RayObjectControl *c);

/* RayObject
 *
 *  A ray object is everything where we can cast rays like:
 *      * a face/triangle
 *      * an octree
 *      * a bvh tree
 *      * an octree of bvh's
 *      * a bvh of bvh's
 *
 *
 *  All types of RayObjects can be created by implementing the
 *  callbacks of the RayObject.
 *
 *  Due to high computing time evolved with casting on faces
 *  there is a special type of RayObject (named RayFace)
 *  which won't use callbacks like other generic nodes.
 *
 *  In order to allow a mixture of RayFace+RayObjects,
 *  all RayObjects must be 4byte aligned, allowing us to use the
 *  2 least significant bits (with the mask 0x03) to define the
 *  type of RayObject.
 *
 *  This leads to 4 possible types of RayObject:
 *
 *   addr&3  - type of object
 *      0       Self (reserved for each structure)
 *      1       RayFace (tri/quad primitive)
 *      2       RayObject (generic with API callbacks)
 *      3       VlakPrimitive
 *              (vlak primitive - to be used when we have a vlak describing the data
 *               eg.: on render code)
 *
 *  0 means it's reserved and has it own meaning inside each ray acceleration structure
 *  (this way each structure can use the allign offset to determine if a node represents a
 *   RayObject primitive, which can be used to save memory)
 */

/* used to test the type of ray object */
#define RE_rayobject_isAligned(o)		((((intptr_t)o)&3) == 0)
#define RE_rayobject_isRayFace(o)		((((intptr_t)o)&3) == 1)
#define RE_rayobject_isRayAPI(o)		((((intptr_t)o)&3) == 2)
#define RE_rayobject_isVlakPrimitive(o)	((((intptr_t)o)&3) == 3)

/* used to align a given ray object */
#define RE_rayobject_align(o)					((RayObject*)(((intptr_t)o)&(~3)))

/* used to unalign a given ray object */
#define RE_rayobject_unalignRayFace(o)			((RayObject*)(((intptr_t)o)|1))
#define RE_rayobject_unalignRayAPI(o)			((RayObject*)(((intptr_t)o)|2))
#define RE_rayobject_unalignVlakPrimitive(o)	((RayObject*)(((intptr_t)o)|3))

/*
 * This rayobject represents a generic object. With it's own callbacks for raytrace operations.
 * It's suitable to implement things like LOD.
 */

struct RayObject {
	struct RayObjectAPI *api;
	struct RayObjectControl control;
};

typedef int  (*RE_rayobject_raycast_callback)(RayObject *, struct Isect *);
typedef void (*RE_rayobject_add_callback)(RayObject *raytree, RayObject *rayobject);
typedef void (*RE_rayobject_done_callback)(RayObject *);
typedef void (*RE_rayobject_free_callback)(RayObject *);
typedef void (*RE_rayobject_merge_bb_callback)(RayObject *, float *min, float *max);
typedef float (*RE_rayobject_cost_callback)(RayObject *);
typedef void (*RE_rayobject_hint_bb_callback)(RayObject *, struct RayHint *, float *, float *);

typedef struct RayObjectAPI {
	RE_rayobject_raycast_callback	raycast;
	RE_rayobject_add_callback		add;
	RE_rayobject_done_callback		done;
	RE_rayobject_free_callback		free;
	RE_rayobject_merge_bb_callback	bb;
	RE_rayobject_cost_callback		cost;
	RE_rayobject_hint_bb_callback	hint_bb;
} RayObjectAPI;

/*
 * Returns the expected cost of raycast on this node, primitives have a cost of 1
 */
float RE_rayobject_cost(RayObject *r);

/*
 * This function differs from RE_rayobject_raycast
 * RE_rayobject_intersect does NOT perform last-hit optimization
 * So this is probably a function to call inside raytrace structures
 */
int RE_rayobject_intersect(RayObject *r, struct Isect *i);

#ifdef __cplusplus
}
#endif

#endif

