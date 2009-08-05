/*
 * BMESH ITERATORS
 * 
 * The functions and structures in this file 
 * provide a unified method for iterating over 
 * the elements of a mesh and answering simple
 * adjacency queries. Tool authors should use
 * the iterators provided in this file instead
 * of inspecting the structure directly.
 *
*/

#ifndef BM_ITERATORS_H
#define BM_ITERATORS_H

/*Defines for passing to BMIter_New.
 
 "OF" can be substituted for "around"
  so BM_VERTS_OF_MESH_OF_FACE means "vertices
  around a face."
 */

/*these iterator over all elements of a specific
  type in the mesh.*/
#define BM_VERTS_OF_MESH 			1
#define BM_EDGES_OF_MESH 			2
#define BM_FACES_OF_MESH 			3

#define BM_ITER(ele, iter, bm, type, data) \
	ele = BMIter_New(iter, bm, type, data); \
	for ( ; ele; ele=BMIter_Step(iter))

#define BM_ITER_SELECT(ele, iter, bm, type, data)\
for (ele = BMIter_New(iter, bm, type, data); ele; ele=BMIter_Step(iter)) {\
	if (BM_TestHFlag(ele, BM_HIDDEN) || !BM_TestHFlag(ele, BM_SELECT)) continue;

#define BM_ITER_NOTSELECT(ele, iter, bm, type, data)\
for (ele = BMIter_New(iter, bm, type, data); ele; ele=BMIter_Step(iter)) {\
	if (BM_TestHFlag(ele, BM_HIDDEN) || BM_TestHFlag(ele, BM_SELECT)) continue;

/*these are topological iterators.*/
#define BM_EDGES_OF_VERT 			4
#define BM_FACES_OF_VERT 			5
#define BM_FACES_OF_EDGE 			6
#define BM_VERTS_OF_FACE 			7
#define BM_FACEVERTS_OF_FACE 			8
#define BM_EDGES_OF_FACE 			9
#define BM_LOOPS_OF_FACE 			10

/*iterate through loops around this loop, which are fetched
  from the other faces in the radial cycle surrounding the
  input loop's edge.*/
#define BM_LOOPS_OF_LOOP		11


/*Iterator Structure*/
typedef struct BMIter{
	struct BMVert *firstvert, *nextvert, *vdata;
	struct BMEdge *firstedge, *nextedge, *edata;
	struct BMLoop *firstloop, *nextloop, *ldata, *l;
	struct BMFace *firstpoly, *nextpoly, *pdata;
	struct BMesh *bm;
	void (*begin)(struct BMIter *iter);
	void *(*step)(struct BMIter *iter);
	union{
		void		*p;
		int			i;
		long		l;
		float		f;
	}filter;
	int type, count;
}BMIter;

void *BMIter_New(struct BMIter *iter, struct BMesh *bm, int type, void *data);
void *BMIter_Step(struct BMIter *iter);
void *BMIter_AtIndex(struct BMesh *bm, int type, void *data, int index);

#endif
