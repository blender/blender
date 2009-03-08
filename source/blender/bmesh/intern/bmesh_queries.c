#include <string.h>

#include "bmesh.h"
#include "bmesh_private.h"

#include "BLI_arithb.h"
#include "MTC_vectorops.h"

/*
 * BM_QUERIES.C
 *
 * This file contains functions for answering common
 * Topological and geometric queries about a mesh, such
 * as, "What is the angle between these two faces?" or,
 * "How many faces are incident upon this vertex?" Tool
 * authors should use the functions in this file instead
 * of inspecting the mesh structure directly.
 *
*/

/*
 * BMESH COUNT ELEMENT
 *
 * Return the amount of element of
 * type 'type' in a given bmesh.
 *
 *
*/

int BM_Count_Element(BMesh *bm, int type)
{
	if(type == BM_VERT) return bm->totvert;
	else if(type == BM_EDGE) return bm->totedge;
	else if(type == BM_FACE) return bm->totface;

	return 0;
}

/*
 * BMESH VERT IN EDGE
 *
 * Returns whether or not a given vertex is
 * is part of a given edge.
 *
*/

int BM_Vert_In_Edge(BMEdge *e, BMVert *v)
{
	return bmesh_vert_in_edge(e, v);
}

/*
 * BMESH OTHER EDGE IN FACE SHARING A VERTEX
 *
 * Returns an opposing edge that shares the same face.
 *
*/

BMLoop *BM_OtherFaceLoop(BMEdge *e, BMFace *f, BMVert *v)
{
	BMLoop *l = f->loopbase, *l2, *l3;
	int found = 0;
	
	do {
		if (l->e == e) break;
		found = 1;
		l = l->head.next;
	} while (l != f->loopbase);
	
	return l->v == v ? l->head.prev : l->head.next;
}

int BM_FacesAroundEdge(BMEdge *e)
{
	if (!e->loop) return 0;

	return bmesh_cycle_length(&(e->loop->radial));
}

/*
 * BMESH VERT IN FACE
 *
 * Returns whether or not a given vertex is
 * is part of a given face.
 *
*/

int BM_Vert_In_Face(BMFace *f, BMVert *v)
{
	BMLoop *l;

	l = f->loopbase;
	do{
		if(l->v == v) return 1;
		l = ((BMLoop*)(l->head.next));
	}while(l != f->loopbase);
	return 0;
}

/*
 * BMESH VERTS IN FACE
 *
 * Compares the number of vertices in an array
 * that appear in a given face
 *
*/
int BM_Verts_In_Face(BMesh *bm, BMFace *f, BMVert **varr, int len)
{
	BMLoop *curloop = NULL;
	int i, count = 0;
	
	for(i=0; i < len; i++) BMO_SetFlag(bm, varr[i], BM_OVERLAP);

	curloop = f->loopbase;
	do{
		if(BMO_TestFlag(bm, curloop->v, BM_OVERLAP)) count++;
		curloop = ((BMLoop*)(curloop->head.next));
	} while(curloop != f->loopbase);

	for(i=0; i < len; i++) BMO_ClearFlag(bm, varr[i], BM_OVERLAP);

	return count;
}

/*
 * BMESH EDGE IN FACE
 *
 * Returns whether or not a given edge is
 * is part of a given face.
 *
*/

int BM_Edge_In_Face(BMFace *f, BMEdge *e)
{
	BMLoop *l;

	l = f->loopbase;
	do{

		if(l->e == e) return 1;
		l = ((BMLoop*)(l->head.next));
	}while(l != f->loopbase);

	return 0;
}

/*
 * BMESH VERTS IN EDGE
 *
 * Returns whether or not two vertices are in 
 * a given edge
 *
*/

int BM_Verts_In_Edge(BMVert *v1, BMVert *v2, BMEdge *e)
{
	return bmesh_verts_in_edge(v1,v2,e);
}

/*
 * BMESH GET OTHER EDGEVERT
 *
 * Given a edge and one of its vertices, returns
 * the other vertex.
 *
*/

BMVert *BM_OtherEdgeVert(BMEdge *e, BMVert *v)
{
	return bmesh_edge_getothervert(e,v);
}

/**
 *			BMESH EDGE EXIST
 *
 *  Finds out if two vertices already have an edge
 *  connecting them. Note that multiple edges may
 *  exist between any two vertices, and therefore
 *  This function only returns the first one found.
 * 
 *  Returns -
 *	BMEdge pointer
 */

BMEdge *BM_Edge_Exist(BMVert *v1, BMVert *v2)
{
	BMNode *diskbase;
	BMEdge *curedge;
	int i, len=0;
	
	if(v1->edge){
		diskbase = bmesh_disk_getpointer(v1->edge,v1);
		len = bmesh_cycle_length(diskbase);
		
		for(i=0,curedge=v1->edge;i<len;i++,curedge = bmesh_disk_nextedge(curedge,v1)){
			if(bmesh_verts_in_edge(v1,v2,curedge)) return curedge;
		}
	}
	
	return NULL;
}

/*
 *  BMESH VERT EDGECOUNT
 *
 *	Returns the number of edges around this vertex.
 */

int BM_Vert_EdgeCount(BMVert *v)
{
	int count = 0;
	BMEdge *curedge = NULL;

	if(v->edge){
		curedge = v->edge;
		do{
			count++;
			curedge = bmesh_disk_nextedge(curedge,v);
		}while(curedge !=v->edge);
	}
	return count;
}

/**
 *  BMESH EDGE FACECOUNT
 *
 *	Returns the number of faces around this edge
*/

int BM_Edge_FaceCount(BMEdge *e)
{
	int count = 0;
	BMLoop *curloop = NULL;

	if(e->loop){
		curloop = e->loop;
		do{
			count++;
			curloop = bmesh_radial_nextloop(curloop);
		}while(curloop != e->loop);
	}

	return count;
}	

/**
 *  BMESH VERT FACECOUNT
 *
 *	Returns the number of faces around this vert
*/

int BM_Vert_FaceCount(BMVert *v){
	int count = 0;
	BMEdge *curedge = NULL;

	if(v->edge){
		curedge = v->edge;
		do{
			if(curedge->loop) count += BM_Edge_FaceCount(curedge);
			curedge = bmesh_disk_nextedge(curedge,v);
		}while(curedge != v->edge);
	}
	return count;
}

/**
 *			BMESH WIRE VERT
 *
 *	Tests whether or not the vertex is part of a wire edge.
 *  (ie: has no faces attached to it)
 *
 *  Returns -
 *	1 for true, 0 for false.
 */

int BM_Wire_Vert(BMesh *bm, BMVert *v)
{
	BMEdge *curedge;

	if(!(v->edge)) return 0;
	
	curedge = v->edge;
	do{
		if(curedge->loop) return 0;
		curedge = bmesh_disk_nextedge(curedge, v);
	}while(curedge != v->edge);

	return 1;
}

/**
 *			BMESH WIRE EDGE
 *
 *	Tests whether or not the edge is part of a wire.
 *  (ie: has no faces attached to it)
 *
 *  Returns -
 *	1 for true, 0 for false.
 */

int BM_Wire_Edge(BMesh *bm, BMEdge *e)
{
	if(e->loop) return 0;
	return 1;
}

/**
 *			BMESH NONMANIFOLD VERT
 *
 *  A vertex is non-manifold if it meets the following conditions:
 *    1: Loose - (has no edges/faces incident upon it)
 *    2: Joins two distinct regions - (two pyramids joined at the tip)
 *    3: Is part of a non-manifold edge (edge with more than 2 faces)
 *    4: Is part of a wire edge
 * 
 *  Returns -
 *	1 for true, 0 for false.
 */

int BM_Nonmanifold_Vert(BMesh *bm, BMVert *v) {
	BMEdge *e, *oe;
	BMLoop *l;
	int len, count, flag;

	if (v->edge == NULL) {
		/* loose vert */
		return 1;
	}

	/* count edges while looking for non-manifold edges */
	oe = v->edge;
	for (len=0,e=v->edge; e != oe || (e == oe && len == 0); len++,e=bmesh_disk_nextedge(e,v)) {
		if (e->loop == NULL) {
			/* loose edge */
			return 1;
		}

		if (bmesh_cycle_length(&(e->loop->radial)) > 2) {
			/* edge shared by more than two faces */
			return 1;
		}
	}

	count = 1;
	flag = 1;
	e = NULL;
	oe = v->edge;
	l = oe->loop;
	while(e != oe) {
		if (l->v == v) l = ((BMLoop*)(l->head.prev));
		else l = ((BMLoop*)(l->head.next));
		e = l->e;
		count++; /* count the edges */

		if (flag && l->radial.next->data == l) {
			/* we've hit the edge of an open mesh, reset once */
			flag = 0;
			count = 1;
			oe = e;
			e = NULL;
			l = oe->loop;
		}
		else if (l->radial.next->data == l) {
			/* break the loop */
			e = oe;
		}
		else {
			l = l->radial.next->data;
		}
	}

	if (count < len) {
		/* vert shared by multiple regions */
		return 1;
	}

	return 0;
}

/**
 *			BMESH NONMANIFOLD EDGE
 *
 *	Tests whether or not this edge is manifold.
 *  A manifold edge either has 1 or 2 faces attached
 *	to it.
 *
 *  Returns -
 *	1 for true, 0 for false.
 */

int BM_Nonmanifold_Edge(BMesh *bm, BMEdge *e)
{
	int count = BM_Edge_FaceCount(e);
	if(count != 2 && count != 1) return 1;
	return 0;
}

/**
 *			BMESH BOUNDARY EDGE
 *
 *	Tests whether or not an edge is on the boundary
 *  of a shell (has one face associated with it)
 *
 *  Returns -
 *	1 for true, 0 for false.
 */

int BM_Boundary_Edge(BMEdge *e)
{
	int count = BM_Edge_FaceCount(e);
	if(count == 1) return 1;
	return 0;
}

/**
 *			BMESH FACE SHAREDEDGES
 *
 *  Counts the number of edges two faces share (if any)
 *
 *  TODO:
 *    Move this to structure, and wrap.
 * 
 *  Returns -
 *	Integer
 */

int BM_Face_Sharededges(BMFace *f1, BMFace *f2){
	BMLoop *l;
	int count = 0;
	
	l = f1->loopbase;
	do{
		if(bmesh_radial_find_face(l->e,f2)) count++;
		l = ((BMLoop*)(l->head.next));
	}while(l != f1->loopbase);
	
	return count;
}

/**
 *
 *           BMESH EDGE SHARE FACES
 *
 *	Tests to see if e1 shares any faces with e2
 *
*/

int BM_Edge_Share_Faces(BMEdge *e1, BMEdge *e2)
{
	BMLoop *l;
	BMFace *f;

	if(e1->loop && e2->loop){
		l = e1->loop;
		do{
			f = l->f;
			if(bmesh_radial_find_face(e2,f)){
				return 1;
			}
			l = (BMLoop*)(l->radial.next->data);
		}while(l != e1->loop);
	}
	return 0;
}



/**
 *			BMESH FACE ANGLE
 *
 *  Calculates the angle between two faces. Assumes
 *  That face normals are correct.
 * 
 *  Returns -
 *	Float.
 */

float BM_Face_Angle(BMesh *bm, BMEdge *e)
{
	BMLoop *l1, *l2;
	int radlen;
	float edge_angle_cos = 0.0;

	radlen = BM_Edge_FaceCount(e);
	if(radlen == 2){
		l1 = e->loop;
		l2 = e->loop->radial.next->data;
		edge_angle_cos = MTC_dot3Float(l1->f->no, l2->f->no);
	}
	return edge_angle_cos;

}

/*
 * BMESH EXIST FACE OVERLAPS
 *
 * Given a set of vertices (varr), find out if
 * a face exists with those vertices already, or
 * if those vertices overlap an existing face.
 *
 * Returns:
 * 0 for no overlap
 * 1 for overlap
 * 
 *
*/

int BM_Exist_Face_Overlaps(BMesh *bm, BMVert **varr, int len, BMFace **existface)
{
	BMIter vertfaces;
	BMFace *f;
	int i, amount;

	*existface = NULL;

	for(i=0; i < len; i++){
		f = BMIter_New(&vertfaces, bm, BM_FACES_OF_VERT, varr[i] );
		while(f){
			amount = BM_Verts_In_Face(bm, f, varr, len);
			if(amount >= len){
				if((len == f->len) && existface)
					*existface = f;
				return 1;				
			}
			f = BMIter_Step(&vertfaces);
		}
	}
	return 0;
}
