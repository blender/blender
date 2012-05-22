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
 * Contributor(s): Geoffrey Bantle, Levi Schooley, Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_CLASS_H__
#define __BMESH_CLASS_H__

/** \file blender/bmesh/bmesh_class.h
 *  \ingroup bmesh
 */

/* bmesh data structures */

/* dissable holes for now, these are ifdef'd because they use more memory and cant be saved in DNA currently */
// #define USE_BMESH_HOLES

struct BMesh;
struct BMVert;
struct BMEdge;
struct BMLoop;
struct BMFace;

struct BLI_mempool;
struct Object;

/* note: it is very important for BMHeader to start with two
 * pointers. this is a requirement of mempool's method of
 * iteration.
 *
 * hrm. it doesn't but stull works ok, remove the comment above? - campbell.
 */

/**
 * BMHeader
 *
 * All mesh elements begin with a BMHeader. This structure
 * hold several types of data
 *
 * 1: The type of the element (vert, edge, loop or face)
 * 2: Persistent "header" flags/markings (smooth, seam, select, hidden, etc)
 *     note that this is different from the "tool" flags.
 * 3: Unique ID in the bmesh.
 * 4: some elements for internal record keeping.
 */
typedef struct BMHeader {
	void *data; /* customdata layers */
	int index; /* notes:
	            * - Use BM_elem_index_get/set macros for index
	            * - Unitialized to -1 so we can easily tell its not set.
	            * - Used for edge/vert/face, check BMesh.elem_index_dirty for valid index values,
	            *   this is abused by various tools which set it dirty.
	            * - For loops this is used for sorting during tessellation. */

	char htype; /* element geometric type (verts/edges/loops/faces) */
	char hflag; /* this would be a CD layer, see below */
} BMHeader;

/* note: need some way to specify custom locations for custom data layers.  so we can
 * make them point directly into structs.  and some way to make it only happen to the
 * active layer, and properly update when switching active layers.*/

typedef struct BMVert {
	BMHeader head;
	struct BMFlagLayer *oflags; /* keep after header, an array of flags, mostly used by the operator stack */

	float co[3];
	float no[3];
	struct BMEdge *e;
} BMVert;

/* disk link structure, only used by edges */
typedef struct BMDiskLink {
	struct BMEdge *next, *prev;
} BMDiskLink;

typedef struct BMEdge {
	BMHeader head;
	struct BMFlagLayer *oflags; /* keep after header, an array of flags, mostly used by the operator stack */

	struct BMVert *v1, *v2;
	struct BMLoop *l;
	
	/* disk cycle pointers */
	BMDiskLink v1_disk_link, v2_disk_link;
} BMEdge;

typedef struct BMLoop {
	BMHeader head;
	/* notice no flags layer */

	struct BMVert *v;
	struct BMEdge *e; /* edge, using verts (v, next->v) */
	struct BMFace *f;

	/* circular linked list of loops which all use the same edge as this one '->e',
	 * but not necessarily the same vertex (can be either v1 or v2 of our own '->e') */
	struct BMLoop *radial_next, *radial_prev;

	/* these were originally commented as private but are used all over the code */
	/* can't use ListBase API, due to head */
	struct BMLoop *next, *prev; /* next/prev verts around the face */
} BMLoop;

/* can cast BMFace/BMEdge/BMVert, but NOT BMLoop, since these don't have a flag layer */
typedef struct BMElemF {
	BMHeader head;
	struct BMFlagLayer *oflags; /* keep after header, an array of flags, mostly used by the operator stack */
} BMElemF;

/* can cast anything to this, including BMLoop */
typedef struct BMElem {
	BMHeader head;
} BMElem;

#ifdef USE_BMESH_HOLES
/* eventually, this structure will be used for supporting holes in faces */
typedef struct BMLoopList {
	struct BMLoopList *next, *prev;
	struct BMLoop *first, *last;
} BMLoopList;
#endif

typedef struct BMFace {
	BMHeader head;
	struct BMFlagLayer *oflags; /* an array of flags, mostly used by the operator stack */

	int len; /*includes all boundary loops*/
#ifdef USE_BMESH_HOLES
	int totbounds; /*total boundaries, is one plus the number of holes in the face*/
	ListBase loops;
#else
	BMLoop *l_first;
#endif
	float no[3]; /*yes, we do store this here*/
	short mat_nr;
} BMFace;

typedef struct BMFlagLayer {
	short f, pflag; /* flags */
} BMFlagLayer;

typedef struct BMesh {
	int totvert, totedge, totloop, totface;
	int totvertsel, totedgesel, totfacesel;

	/* flag index arrays as being dirty so we can check if they are clean and
	 * avoid looping over the entire vert/edge/face array in those cases.
	 * valid flags are - BM_VERT | BM_EDGE | BM_FACE.
	 * BM_LOOP isn't handled so far. */
	char elem_index_dirty;
	
	/*element pools*/
	struct BLI_mempool *vpool, *epool, *lpool, *fpool;

	/*operator api stuff*/
	struct BLI_mempool *toolflagpool;
	int stackdepth;
	struct BMOperator *currentop;
	
	CustomData vdata, edata, ldata, pdata;

#ifdef USE_BMESH_HOLES
	struct BLI_mempool *looplistpool;
#endif

	/* should be copy of scene select mode */
	/* stored in BMEditMesh too, this is a bit confusing,
	 * make sure they're in sync!
	 * Only use when the edit mesh cant be accessed - campbell */
	short selectmode;
	
	/*ID of the shape key this bmesh came from*/
	int shapenr;
	
	int walkers, totflags;
	ListBase selected, error_stack;
	
	BMFace *act_face;

	ListBase errorstack;

	void *py_handle;

	int opflag; /* current operator flag */
} BMesh;

/* BMHeader->htype (char) */
enum {
	BM_VERT = 1,
	BM_EDGE = 2,
	BM_LOOP = 4,
	BM_FACE = 8
};

#define BM_ALL (BM_VERT | BM_EDGE | BM_LOOP | BM_FACE)

/* BMHeader->hflag (char) */
enum {
	BM_ELEM_SELECT  = (1 << 0),
	BM_ELEM_HIDDEN  = (1 << 1),
	BM_ELEM_SEAM    = (1 << 2),
	BM_ELEM_SMOOTH  = (1 << 3), /* used for faces and edges, note from the user POV,
                                 * this is a sharp edge when disabled */

	BM_ELEM_TAG     = (1 << 4), /* internal flag, used for ensuring correct normals
                                 * during multires interpolation, and any other time
                                 * when temp tagging is handy.
                                 * always assume dirty & clear before use. */

	BM_ELEM_DRAW    = (1 << 5), /* edge display */

	/* we have 1 spare flag which is awesome but since we're limited to 8
	 * only add new flags with care! - campbell */
	/* BM_ELEM_SPARE  = (1 << 6), */

	BM_ELEM_INTERNAL_TAG = (1 << 7) /* for low level internal API tagging,
                                     * since tools may want to tag verts and
                                     * not have functions clobber them */
};

/* defines */

/*forward declarations*/

#ifdef USE_BMESH_HOLES
#  define BM_FACE_FIRST_LOOP(p) (((BMLoopList *)((p)->loops.first))->first)
#else
#  define BM_FACE_FIRST_LOOP(p) ((p)->l_first)
#endif

/* size to use for static arrays when dealing with NGons,
 * alloc after this limit is reached.
 * this value is rather arbitrary */
#define BM_NGON_STACK_SIZE 32

/* avoid inf loop, this value is arbitrary
 * but should not error on valid cases */
#define BM_LOOP_RADIAL_MAX 10000
#define BM_NGON_MAX 100000

#endif /* __BMESH_CLASS_H__ */
