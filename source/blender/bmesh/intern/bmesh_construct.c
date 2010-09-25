/**
 * bmesh_construct.c    August 2008
 *
 *	BM construction functions.
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BKE_customdata.h" 
#include "BKE_utildefines.h"

#include "BLI_array.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include "math.h"
#include "stdio.h"
#include "string.h"

#define SELECT 1
	#define BM_EDGEVERT	(1<<14)

/*prototypes*/
static void bm_copy_loop_attributes(BMesh *source_mesh, BMesh *target_mesh,
                                    BMLoop *source_loop, BMLoop *target_loop);
#if 0

/*
 * BM_CONSTRUCT.C
 *
 * This file contains functions for making and destroying
 * individual elements like verts, edges and faces.
 *
*/

/*
 * BMESH MAKE VERT
 *
 * Creates a new vertex and returns a pointer
 * to it. If a pointer to an example vertex is
 * passed in, it's custom data and properties
 * will be copied to the new vertex.
 *
*/

BMVert *BM_Make_Vert(BMesh *bm, float co[3], BMVert *example)
{
	BMVert *v = NULL;
	v = bmesh_mv(bm, co);
	if(example)
		CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, example->head.data, &v->head.data);
	return v;
}

/*
 * BMESH MAKE EDGE
 *
 * Creates a new edge betweeen two vertices and returns a
 * pointer to it. If 'nodouble' equals 1, then a check is
 * is done to make sure that an edge between those two vertices
 * does not already exist. If it does, that edge is returned instead
 * of creating a new one.
 *
 * If a new edge is created, and a pointer to an example edge is
 * provided, it's custom data and properties will be copied to the
 * new edge.
 *
*/

BMEdge *BM_Make_Edge(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge *example, int nodouble)
{
	BMEdge *e = NULL;
	
	if(nodouble) /*test if edge already exists.*/
		e = BM_Edge_Exist(v1, v2);

	if(!e){
		e = bmesh_me(bm, v1, v2);

		if(example)
			CustomData_bmesh_copy_data(&bm->edata, &bm->edata, example->head.data, &e->head.data);
	}
	
	return e;
	
}
#endif

/*
 * BMESH MAKE QUADTRIANGLE
 *
 * Creates a new quad or triangle from
 * a list of 3 or 4 vertices. If nodouble
 * equals 1, then a check is done to see
 * if a face with these vertices already
 * exists and returns it instead. If a pointer
 * to an example face is provided, it's custom
 * data and properties will be copied to the new
 * face.
 *
 * Note that the winding of the face is determined
 * by the order of the vertices in the vertex array
 *
*/

BMFace *BM_Make_QuadTri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, 
			BMVert *v4, BMFace *example, int nodouble)
{
	BMEdge *edar[4];
	BMVert *vtar[4];

	edar[0] = BM_Edge_Exist(v1, v2);
	edar[1] = BM_Edge_Exist(v2, v3);
	edar[2] = BM_Edge_Exist(v3, v4? v4 : v1);
	if (v4) edar[3] = BM_Edge_Exist(v4, v1);
	else edar[3] = NULL;

	if (!edar[0]) edar[0] = BM_Make_Edge(bm, v1, v2, NULL, 0);
	if (!edar[1]) edar[1] = BM_Make_Edge(bm, v2, v3, NULL, 0);
	if (!edar[2]) edar[2] = BM_Make_Edge(bm, v3, v4?v4:v1, NULL, 0);
	if (!edar[0] && v4) edar[0] = BM_Make_Edge(bm, v4, v1, NULL, 0);

	vtar[0] = v1;
	vtar[1] = v2;
	vtar[2] = v3;
	vtar[3] = v4;

	return BM_Make_Quadtriangle(bm, vtar, edar, v4?4:3, example, nodouble);
}

/*remove the edge array bits from this. Its not really needed?*/
BMFace *BM_Make_Quadtriangle(BMesh *bm, BMVert **verts, BMEdge **edges, int len, BMFace *example, int nodouble)
{
	BMEdge *edar[4];
	BMFace *f = NULL;
	int overlap = 0;

	edar[0] = edar[1] = edar[2] = edar[3] = NULL;
	
	if(edges){
		edar[0] = edges[0];
		edar[1] = edges[1];
		edar[2] = edges[2];
		if(len == 4) edar[3] = edges[3];
	}else{
		edar[0] = BM_Edge_Exist(verts[0],verts[1]);
		edar[1] = BM_Edge_Exist(verts[1],verts[2]);
		if(len == 4){
			edar[2] = BM_Edge_Exist(verts[2],verts[3]);
			edar[3] = BM_Edge_Exist(verts[3],verts[0]);

		}else{
			edar[2] = BM_Edge_Exist(verts[2],verts[0]);
		}
	}
	
	if(nodouble){
		/*check if face exists or overlaps*/
		if(len == 4){
			overlap = BM_Exist_Face_Overlaps(bm, verts, len, &f);
		}else{
			overlap = BM_Exist_Face_Overlaps(bm, verts, len, &f);
		}
	}

	/*make new face*/
	if((!f) && (!overlap)){
		if(!edar[0]) edar[0] = BM_Make_Edge(bm, verts[0], verts[1], NULL, 0);
		if(!edar[1]) edar[1] = BM_Make_Edge(bm, verts[1], verts[2], NULL, 0);
		if(len == 4){
			if(!edar[2]) edar[2] = BM_Make_Edge(bm, verts[2], verts[3], NULL, 0);
			if(!edar[3]) edar[3] = BM_Make_Edge(bm, verts[3], verts[0], NULL, 0);
		} else {
			if(!edar[2]) edar[2] = BM_Make_Edge(bm, verts[2], verts[0], NULL, 0);
		}
	
		f = BM_Make_Face(bm, verts, edar, len);
	
		if(example)
			CustomData_bmesh_copy_data(&bm->pdata, &bm->pdata, example->head.data, &f->head.data);

	}

	return f;
}


/*copies face data from shared adjacent faces*/
void BM_Face_CopyShared(BMesh *bm, BMFace *f) {
	BMIter iter;
	BMLoop *l, *l2;

	if (!f) return;

	l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f);
	for (; l; l=BMIter_Step(&iter)) {
		l2 = l->radial_next;
		
		if (l2 && l2 != l) {
			if (l2->v == l->v) {
				bm_copy_loop_attributes(bm, bm, l2, l);
			} else {
				l2 = (BMLoop*) l2->next;
				bm_copy_loop_attributes(bm, bm, l2, l);
			}
		}
	}
}

/*
 * BMESH MAKE NGON
 *
 * Attempts to make a new Ngon from a list of edges.
 * If nodouble equals one, a check for overlaps or existing
 *
 * The edges are not required to be ordered, simply to to form
 * a single closed loop as a whole
*/
#define VERT_BUF_SIZE 100
BMFace *BM_Make_Ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble)
{
	BMEdge **edges2 = NULL;
	BLI_array_staticdeclare(edges2, VERT_BUF_SIZE);
	BMVert **verts = NULL, *v;
	BLI_array_staticdeclare(verts, VERT_BUF_SIZE);
	BMFace *f = NULL;
	BMEdge *e;
	int overlap = 0, i, j, v1found, reverse;

	/*this code is hideous, yeek.  I'll have to think about ways of
	  cleaning it up.  basically, it now combines the old BM_Make_Ngon
	  *and* the old bmesh_mf functions, so its kindof smashed together
		- joeedh*/

	if (!len || !v1 || !v2 || !edges || !bm)
		return NULL;

	/*put edges in correct order*/
	for (i=0; i<len; i++) {
		bmesh_api_setflag(edges[i], _FLAG_MF);
	}

	BLI_array_append(verts, edges[0]->v1);

	v = edges[0]->v2;
	e = edges[0];
	do {
		BMEdge *e2 = e;

		BLI_array_append(verts, v);
		BLI_array_append(edges2, e);

		do {
			e2 = bmesh_disk_nextedge(e2, v);
			if (e2 != e && bmesh_api_getflag(e2, _FLAG_MF)) {
				v = BM_OtherEdgeVert(e2, v);
				break;
			}
		} while (e2 != e);

		if (e2 == e)
			goto err; /*the edges do not form a closed loop*/

		e = e2;
	} while (e != edges[0]);

	if (BLI_array_count(edges2) != len)
		goto err; /*we didn't use all edges in forming the boundary loop*/

	/*ok, edges are in correct order, now ensure they are going
	  in the correct direction*/
	v1found = reverse = 0;
	for (i=0; i<len; i++) {
		if (BM_Vert_In_Edge(edges2[i], v1)) {
			/*see if v1 and v2 are in the same edge*/
			if (BM_Vert_In_Edge(edges2[i], v2)) {
				/*if v1 is shared by the *next* edge, then the winding
				  is incorrect*/
				if (BM_Vert_In_Edge(edges2[(i+1)%len], v1)) {
					reverse = 1;
					break;
				}
			}

			v1found = 1;
		}

		if (!v1found && BM_Vert_In_Edge(edges2[i], v2)) {
			reverse = 1;
			break;
		}
	}

	if (reverse) {
		for (i=0; i<len/2; i++) {
			v = verts[i];
			verts[i] = verts[len-i-1];
			verts[len-i-1] = v;
		}
	}

	for (i=0; i<len; i++) {
		edges2[i] = BM_Edge_Exist(verts[i], verts[(i+1)%len]);
		if (!edges2[i])
			return NULL;
	}

	/*check if face already exists*/
	if(nodouble)
		overlap = BM_Face_Exists(bm, verts, len, &f);

	/*create the face, if necassary*/
	if (!f && !overlap)
		f = BM_Make_Face(bm, verts, edges2, len);
	else if (!overlap)
		f = NULL;

	/*clean up flags*/
	for (i=0; i<len; i++) {
		bmesh_api_clearflag(edges2[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return f;

err:
	for (i=0; i<len; i++) {
		bmesh_api_clearflag(edges[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return NULL;
}


/*bmesh_make_face_from_face(BMesh *bm, BMFace *source, BMFace *target) */


/*
 * REMOVE TAGGED XXX
 *
 * Called by operators to remove elements that they have marked for
 * removal.
 *
*/

void BM_remove_tagged_faces(BMesh *bm, int flag)
{
	BMFace *f;
	BMIter iter;

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if(BMO_TestFlag(bm, f, flag)) BM_Kill_Face(bm, f);
	}
}

void BM_remove_tagged_edges(BMesh *bm, int flag)
{
	BMEdge *e;
	BMIter iter;

	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if(BMO_TestFlag(bm, e, flag)) BM_Kill_Edge(bm, e);
	}
}

void BM_remove_tagged_verts(BMesh *bm, int flag)
{
	BMVert *v;
	BMIter iter;

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if(BMO_TestFlag(bm, v, flag)) BM_Kill_Vert(bm, v);
	}
}

static void bm_copy_vert_attributes(BMesh *source_mesh, BMesh *target_mesh, BMVert *source_vertex, BMVert *target_vertex)
{
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata, source_vertex->head.data, &target_vertex->head.data);	
}

static void bm_copy_edge_attributes(BMesh *source_mesh, BMesh *target_mesh, BMEdge *source_edge, BMEdge *target_edge)
{
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata, source_edge->head.data, &target_edge->head.data);
}

static void bm_copy_loop_attributes(BMesh *source_mesh, BMesh *target_mesh, BMLoop *source_loop, BMLoop *target_loop)
{
	CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata, source_loop->head.data, &target_loop->head.data);
}

static void bm_copy_face_attributes(BMesh *source_mesh, BMesh *target_mesh, BMFace *source_face, BMFace *target_face)
{
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata, source_face->head.data, &target_face->head.data);	
	target_face->mat_nr = source_face->mat_nr;
}

/*Todo: Special handling for hide flags?*/

void BM_Copy_Attributes(BMesh *source_mesh, BMesh *target_mesh, void *source, void *target)
{
	BMHeader *sheader = source, *theader = target;
	
	if(sheader->type != theader->type)
		return;

	/*First we copy select*/
	if(BM_Selected(source_mesh, source)) BM_Select(target_mesh, target, 1);
	
	/*Now we copy flags*/
	theader->flag = sheader->flag;
	
	/*Copy specific attributes*/
	if(theader->type == BM_VERT)
		bm_copy_vert_attributes(source_mesh, target_mesh, (BMVert*)source, (BMVert*)target);
	else if(theader->type == BM_EDGE)
		bm_copy_edge_attributes(source_mesh, target_mesh, (BMEdge*)source, (BMEdge*)target);
	else if(theader->type == BM_LOOP)
		bm_copy_loop_attributes(source_mesh, target_mesh, (BMLoop*)source, (BMLoop*)target);
	else if(theader->type == BM_FACE)
		bm_copy_face_attributes(source_mesh, target_mesh, (BMFace*)source, (BMFace*)target);
}

BMesh *BM_Copy_Mesh(BMesh *bmold)
{
	BMesh *bm;
	BMVert *v, *v2, **vtable = NULL;
	BLI_array_declare(vtable);
	BMEdge *e, *e2, **edges = NULL, **etable = NULL;
	BLI_array_declare(edges);
	BLI_array_declare(etable);
	BMLoop *l, *l2, **loops = NULL;
	BLI_array_declare(loops);
	BMFace *f, *f2, **ftable = NULL;
	BLI_array_declare(ftable);
	BMEditSelection *ese;
	BMIter iter, liter;
	int allocsize[4] = {512,512,2048,512}, numTex, numCol;
	int i, j;

	/*allocate a bmesh*/
	bm = BM_Make_Mesh(allocsize);

	CustomData_copy(&bmold->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->ldata, &bm->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->pdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&bm->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&bm->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&bm->pdata, allocsize[3]);

	/*needed later*/
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	v = BMIter_New(&iter, bmold, BM_VERTS_OF_MESH, NULL);
	for (i=0; v; v=BMIter_Step(&iter), i++) {
		v2 = BM_Make_Vert(bm, v->co, NULL);
		BM_Copy_Attributes(bmold, bm, v, v2);
		BLI_array_growone(vtable);
		VECCOPY(v2->no, v->no);

		vtable[BLI_array_count(vtable)-1] = v2;

		BMINDEX_SET(v, i);
		BMINDEX_SET(v2, i);
	}
	
	e = BMIter_New(&iter, bmold, BM_EDGES_OF_MESH, NULL);
	for (i=0; e; e=BMIter_Step(&iter), i++) {
		e2 = BM_Make_Edge(bm, vtable[BMINDEX_GET(e->v1)],
			          vtable[BMINDEX_GET(e->v2)], e, 0);

		BM_Copy_Attributes(bmold, bm, e, e2);
		BLI_array_growone(etable);
		etable[BLI_array_count(etable)-1] = e2;

		BMINDEX_SET(e, i);
		BMINDEX_SET(e2, i);
	}
	
	f = BMIter_New(&iter, bmold, BM_FACES_OF_MESH, NULL);
	for (i=0; f; f=BMIter_Step(&iter), i++) {
		BLI_array_empty(loops);
		BLI_array_empty(edges);
		l = BMIter_New(&liter, bmold, BM_LOOPS_OF_FACE, f);
		for (j=0; j<f->len; j++, l = BMIter_Step(&liter)) {
			BLI_array_growone(loops);
			BLI_array_growone(edges);
			loops[j] = l;
			edges[j] = etable[BMINDEX_GET(l->e)];
		}

		v = vtable[BMINDEX_GET(loops[0]->v)];
		v2 = vtable[BMINDEX_GET(loops[1]->v)];

		if (!bmesh_verts_in_edge(v, v2, edges[0])) {
			v = vtable[BMINDEX_GET(loops[BLI_array_count(loops)-1]->v)];
			v2 = vtable[BMINDEX_GET(loops[0]->v)];
		}

		f2 = BM_Make_Ngon(bm, v, v2, edges, f->len, 0);
		if (!f2)
			continue;
		
		BMINDEX_SET(f, i);
		BLI_array_growone(ftable);
		ftable[i] = f2;
		
		BM_Copy_Attributes(bmold, bm, f, f2);
		VECCOPY(f2->no, f->no);

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f2);
		for (j=0; j<f->len; j++, l = BMIter_Step(&liter)) {
			BM_Copy_Attributes(bmold, bm, loops[j], l);
		}

		if (f == bmold->act_face) bm->act_face = f2;
	}

	/*copy over edit selection history*/
	for (ese=bmold->selected.first; ese; ese=ese->next) {
		void *ele;

		if (ese->type == BM_VERT)
			ele = vtable[BMINDEX_GET(ese->data)];
		else if (ese->type == BM_EDGE)
			ele = etable[BMINDEX_GET(ese->data)];
		else if (ese->type == BM_FACE) {
			ele = ftable[BMINDEX_GET(ese->data)];
		}

		BM_store_selection(bm, ele);
	}

	BLI_array_free(etable);
	BLI_array_free(vtable);
	BLI_array_free(ftable);

	BLI_array_free(loops);
	BLI_array_free(edges);

	return bm;
}

/*
  BM FLAGS TO ME FLAGS

  Returns the flags stored in element,
  which much be either a BMVert, BMEdge,
  or BMFace, converted to mesh flags.
*/

int BMFlags_To_MEFlags(void *element) {
	BMHeader *h = element;
	int f = 0;

	if (h->flag & BM_PINNED) f |= ME_PIN;
	if (h->flag & BM_HIDDEN) f |= ME_HIDE;

	if (h->type == BM_FACE) {
		if (h->flag & BM_SELECT) f |= ME_FACE_SEL;
		if (h->flag & BM_SMOOTH) f |= ME_SMOOTH;
	} else if (h->type == BM_EDGE) {
		if (h->flag & BM_SELECT) f |= BM_SELECT;
		if (h->flag & BM_SEAM) f |= ME_SEAM;
		if (h->flag & BM_SHARP) f |= ME_SHARP;
		if (BM_Wire_Edge(NULL, element)) f |= ME_LOOSEEDGE;
		f |= ME_EDGEDRAW;
	} else if (h->type == BM_VERT) {
		if (h->flag & BM_SELECT) f |= BM_SELECT;
	}

	return f;
}

/*
  BM FLAGS TO ME FLAGS

  Returns the flags stored in element,
  which much be either a MVert, MEdge,
  or MPoly, converted to mesh flags.
  type must be either BM_VERT, BM_EDGE,
  or BM_FACE.
*/
int MEFlags_To_BMFlags(int flag, int type) {
	int f = 0;
	if (flag & ME_PIN) f |= BM_PINNED;

	if (type == BM_FACE) {
		if (flag & ME_FACE_SEL) f |= BM_SELECT;
		if (flag & ME_SMOOTH) f |= BM_SMOOTH;
		if (flag & ME_HIDE) f |= BM_HIDDEN;
	} else if (type == BM_EDGE) {
		if (flag & SELECT) f |= BM_SELECT;
		if (flag & ME_SEAM) f |= BM_SEAM;
		if (flag & ME_SHARP) f |= BM_SHARP;
		if (flag & ME_HIDE) f |= BM_HIDDEN;
	} else if (type == BM_VERT) {
		if (flag & SELECT) f |= BM_SELECT;
		if (flag & ME_HIDE) f |= BM_HIDDEN;
	}

	return f;
}
