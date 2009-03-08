#include <string.h>

#include "bmesh.h"
#include "bmesh_private.h"

/*
 *	BMESH ITERATOR STEP
 *
 *  Calls an iterators step fucntion to return
 *  the next element.
*/

void *BMIter_Step(BMIter *iter)
{
	return iter->step(iter);
}

/*
 * INIT ITERATOR
 *
 * Clears the internal state of an iterator
 * For begin() callbacks.
 *
*/

static void init_iterator(BMIter *iter)
{
	iter->firstvert = iter->nextvert = NULL;
	iter->firstedge = iter->nextedge = NULL;
	iter->firstloop = iter->nextloop = NULL;
	iter->firstpoly = iter->nextpoly = NULL;
	iter->ldata = NULL;
}

/*
 * Notes on iterator implementation:
 *
 * Iterators keep track of the next element
 * in a sequence. When a step() callback is
 * invoked the current value of 'next' is stored
 * to be returned later and the next variable is
 * incremented.
 *
 * When the end of a sequence is
 * reached, next should always equal NULL
 *
*/

/*
 * VERT OF MESH CALLBACKS
 *
*/

static void vert_of_mesh_begin(BMIter *iter)
{
	init_iterator(iter);
	if(iter->bm->verts.first){
		iter->firstvert = iter->bm->verts.first;
		iter->nextvert = iter->bm->verts.first;
	}
}

static void *vert_of_mesh_step(BMIter *iter)
{
	BMVert *current = iter->nextvert;

	if(iter->nextvert)
		iter->nextvert = (BMVert*)(iter->nextvert->head.next);	

	return current;
}

/*
 * EDGE OF MESH CALLBACKS
 *
*/

static void edge_of_mesh_begin(BMIter *iter)
{
	init_iterator(iter);
	if(iter->bm->edges.first){
		iter->firstedge = iter->bm->edges.first;
		iter->nextedge = iter->bm->edges.first;
	}
}

static void *edge_of_mesh_step(BMIter *iter)
{
	BMEdge *current = iter->nextedge;

	if(iter->nextedge)
		iter->nextedge = (BMEdge*)(iter->nextedge->head.next);

	return current;
}

/*
 * FACE OF MESH CALLBACKS
 *
*/

static void face_of_mesh_begin(BMIter *iter)
{
	init_iterator(iter);
	if(iter->bm->polys.first){
		iter->firstpoly = iter->bm->polys.first;
		iter->nextpoly = iter->bm->polys.first;
	}
}

static void *face_of_mesh_step(BMIter *iter)
{
	BMFace *current = iter->nextpoly;

	if(iter->nextpoly)
		iter->nextpoly = (BMFace*)(iter->nextpoly->head.next);
	return current;
}

/*
 * EDGE OF VERT CALLBACKS
 *
*/

static void edge_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	if(iter->vdata->edge){
		iter->firstedge = iter->vdata->edge;
		iter->nextedge = iter->vdata->edge;
	}
}

static void *edge_of_vert_step(BMIter *iter)
{
	BMEdge *current = iter->nextedge;

	if(iter->nextedge)
		iter->nextedge = bmesh_disk_nextedge(iter->nextedge, iter->vdata);
	
	if(iter->nextedge == iter->firstedge) iter->nextedge = NULL; 

	return current;
}

/*
 * FACE OF VERT CALLBACKS
 *
*/

static void face_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->count = 0;
	if(iter->vdata->edge)
		iter->count = bmesh_disk_count_facevert(iter->vdata);
	if(iter->count){
		iter->firstedge = bmesh_disk_find_first_faceedge(iter->vdata->edge, iter->vdata);
		iter->nextedge = iter->firstedge;
		iter->firstloop = bmesh_radial_find_first_facevert(iter->firstedge->loop, iter->vdata);
		iter->nextloop = iter->firstloop;
	}
}
static void *face_of_vert_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->count){
		iter->count--;
		iter->nextloop = bmesh_radial_find_next_facevert(iter->nextloop, iter->vdata);
		if(iter->nextloop == iter->firstloop){
			iter->nextedge = bmesh_disk_find_next_faceedge(iter->nextedge, iter->vdata);
			iter->firstloop = bmesh_radial_find_first_facevert(iter->nextedge->loop, iter->vdata);
			iter->nextloop = iter->firstloop;
		}
	}
	
	if(!iter->count) iter->nextloop = NULL;

	
	if(current) return current->f;
	return NULL;
}

static void loops_of_loop_begin(BMIter *iter)
{
	BMLoop *l;

	l = iter->ldata;

	/*note sure why this sets ldata. . .*/
	init_iterator(iter);

	iter->firstloop = l;
	iter->nextloop = bmesh_radial_nextloop(iter->firstloop);
}

static void *loops_of_loop_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) iter->nextloop = bmesh_radial_nextloop(iter->nextloop);

	if(iter->nextloop == iter->firstloop) iter->nextloop = NULL;
	if(current) return current;
	return NULL;
}

/*
 * FACE OF EDGE CALLBACKS
 *
*/

static void face_of_edge_begin(BMIter *iter)
{
	init_iterator(iter);
	
	if(iter->edata->loop){
		iter->firstloop = iter->edata->loop;
		iter->nextloop = iter->edata->loop;
	}
}

static void *face_of_edge_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) iter->nextloop = bmesh_radial_nextloop(iter->nextloop);

	if(iter->nextloop == iter->firstloop) iter->nextloop = NULL;
	if(current) return current->f;
	return NULL;
}

/*
 * VERT OF FACE CALLBACKS
 *
*/

static void vert_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = iter->pdata->loopbase;
}

static void *vert_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) iter->nextloop = ((BMLoop*)(iter->nextloop->head.next));
	if(iter->nextloop == iter->firstloop) iter->nextloop = NULL;

	if(current) return current->v;
	return NULL;
}

/*
 * EDGE OF FACE CALLBACKS
 *
*/

static void edge_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = iter->pdata->loopbase;
}

static void *edge_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) iter->nextloop = ((BMLoop*)(iter->nextloop->head.next));
	if(iter->nextloop == iter->firstloop) iter->nextloop = NULL;
	
	if(current) return current->e;
	return NULL;
}

/*
 * LOOP OF FACE CALLBACKS
 *
*/

static void loop_of_face_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstloop = iter->nextloop = iter->pdata->loopbase;
}

static void *loop_of_face_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) iter->nextloop = ((BMLoop*)(iter->nextloop->head.next));
	if(iter->nextloop == iter->firstloop) iter->nextloop = NULL;

	return current;
}

/*
 * LOOPS OF VERT CALLBACKS
 *
 */
/*
	BMEdge *current = iter->nextedge;

	if(iter->nextedge)
		iter->nextedge = bmesh_disk_nextedge(iter->nextedge, iter->vdata);
	
	if(iter->nextedge == iter->firstedge) iter->nextedge = NULL; 

	return current;
*/
	

static void loop_of_vert_begin(BMIter *iter)
{
	init_iterator(iter);
	iter->firstedge = iter->vdata->edge;
	if (!iter->vdata->edge) return NULL;

	iter->firstloop = iter->nextloop = iter->vdata->edge->loop;
	iter->l = iter->firstloop;
}

static void *loop_of_vert_step(BMIter *iter)
{
	BMLoop *current = iter->nextloop;

	if(iter->nextloop) {
		iter->nextloop = bmesh_radial_nextloop(iter->nextloop);
		if (iter->nextloop == iter->l) {
			iter->nextloop = bmesh_disk_nextedge(iter->nextloop->e, 
			                                    iter->vdata)->loop;
			iter->l = iter->nextloop;

			if (iter->nextloop->e == iter->firstedge)
				iter->nextloop = NULL;
		}
	}
	
	return current;
}

/*
 * BMESH ITERATOR INIT
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type and then calls BMeshIter_step()
 * to return the first element of the iterator.
 *
*/
void *BMIter_New(BMIter *iter, BMesh *bm, int type, void *data)
{
	int argtype;
	iter->type = type;
	iter->bm = bm;

	switch(type){
		case BM_VERTS:
			iter->begin = vert_of_mesh_begin;
			iter->step = vert_of_mesh_step;
			iter->bm = bm;
			break;
		case BM_EDGES:
			iter->begin = edge_of_mesh_begin;
			iter->step = edge_of_mesh_step;
			iter->bm = bm;
			break;
		case BM_FACES:
			iter->begin = face_of_mesh_begin;
			iter->step = face_of_mesh_step;
			iter->bm = bm;
			break;
		case BM_EDGES_OF_VERT:
			iter->begin = edge_of_vert_begin;
			iter->step = edge_of_vert_step;
			iter->vdata = data;
			break;
		case BM_FACES_OF_VERT:
			iter->begin = face_of_vert_begin;
			iter->step = face_of_vert_step;
			iter->vdata = data;
			break;
		case BM_FACES_OF_EDGE:
			iter->begin = face_of_edge_begin;
			iter->step = face_of_edge_step;
			iter->edata = data;
			break;
		case BM_VERTS_OF_FACE:
			iter->begin = vert_of_face_begin;
			iter->step = vert_of_face_step;
			iter->pdata = data;
			break;
		case BM_EDGES_OF_FACE:
			iter->begin = edge_of_face_begin;
			iter->step = edge_of_face_step;
			iter->pdata = data;
			break;
		case BM_LOOPS_OF_FACE:
			iter->begin = loop_of_face_begin;
			iter->step = loop_of_face_step;
			iter->pdata = data;
			break;
		case BM_LOOPS_OF_VERT:
			iter->begin = loop_of_vert_begin;
			iter->step = loop_of_vert_step;
			iter->vdata = data;
			break;
		case BM_LOOPS_OF_LOOP:
			iter->begin = loops_of_loop_begin;
			iter->step = loops_of_loop_step;
			iter->ldata = data;
			break;
		default:
			break;
	}

	iter->begin(iter);
	return BMIter_Step(iter);
}
