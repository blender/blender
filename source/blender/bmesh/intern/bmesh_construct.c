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

#include "bmesh.h"
#include "bmesh_private.h"

#include "math.h"
#include "stdio.h"
#include "string.h"

/*prototypes*/
static void bm_copy_loop_attributes(BMesh *source_mesh, BMesh *target_mesh,
                                    BMLoop *source_loop, BMLoop *target_loop);

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
		CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, example->data, &v->data);
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
		e = bmesh_disk_existedge(v1, v2);

	if(!e){
		e = bmesh_me(bm, v1, v2);

		if(example)
			CustomData_bmesh_copy_data(&bm->edata, &bm->edata, example->data, &e->data);
	}
	
	return e;
	
}

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

BMFace *BM_Make_QuadTri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4, BMFace *example)
{
	BMEdge *edar[4];
	BMVert *vtar[4];

	edar[0] = v1->edge;
	edar[1] = v1->edge;
	edar[2] = v1->edge;
	if (v4) edar[3] = v1->edge;
	else edar[3] = NULL;

	vtar[0] = v1;
	vtar[1] = v2;
	vtar[2] = v3;
	vtar[3] = v4;

	return BM_Make_Quadtriangle(bm, vtar, edar, v4?4:3, example, 0);
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
		edar[0] = bmesh_disk_existedge(verts[0],verts[1]);
		edar[1] = bmesh_disk_existedge(verts[1],verts[2]);
		if(len == 4){
			edar[2] = bmesh_disk_existedge(verts[2],verts[3]);
			edar[3] = bmesh_disk_existedge(verts[3],verts[0]);

		}else{
			edar[2] = bmesh_disk_existedge(verts[2],verts[0]);
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
		if(!edar[0]) edar[0] = bmesh_me(bm, verts[0], verts[1]);
		if(!edar[1]) edar[1] = bmesh_me(bm, verts[1], verts[2]);
		if(len == 4){
			if(!edar[2]) edar[2] = bmesh_me(bm, verts[2], verts[3]);
			if(!edar[3]) edar[3] = bmesh_me(bm, verts[3], verts[0]); 
		} else {
			if(!edar[2]) edar[2] = bmesh_me(bm, verts[2], verts[0]);
		}
	
		if(len == 4) f = bmesh_mf(bm, verts[0], verts[1], edar, 4);
		else f = bmesh_mf(bm, verts[0], verts[1], edar, 3);
	
		if(example)
			CustomData_bmesh_copy_data(&bm->pdata, &bm->pdata, example->data, &f->data);

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
		l2 = l->radial.next->data;
		
		if (l2 && l2 != l) {
			if (l2->v == l->v) {
				bm_copy_loop_attributes(bm, bm, l2, l);
			} else {
				l2 = (BMLoop*) l2->head.next;
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
 * 
 *
*/
#define VERT_BUF_SIZE 100
BMFace *BM_Make_Ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble)
{
	BMVert *vert_buf[VERT_BUF_SIZE];
	BMVert **verts = vert_buf, *lastv;
	BMFace *f = NULL;
	int overlap = 0, i, j;

	if(nodouble){
		if(len > VERT_BUF_SIZE)
			verts = MEM_callocN(sizeof(BMVert *) * len, "bmesh make ngon vertex array");
		
		/*if ((edges[i]->v1 == edges[i]->v1) || 
		   (edges[i]->v1 == edges[i]->v2))
		{
			lastv = edges[i]->v2;
		} else lastv = edges[i]->v1;
		verts[0] = lastv;

		for (i=1; i<len; i++) {
			if (!BMO_TestFlag
		}*/

		for(i = 0, j=0; i < len; i++){
			if(!BMO_TestFlag(bm, edges[i]->v1, BM_EDGEVERT)){
				BMO_SetFlag(bm, edges[i]->v1, BM_EDGEVERT);
				verts[j++] = edges[i]->v1;
			}
			if(!BMO_TestFlag(bm, edges[i]->v2, BM_EDGEVERT)) {
				BMO_SetFlag(bm, edges[i]->v2, BM_EDGEVERT);
				verts[j++] = edges[i]->v2;
			}
		}
		
		if (j != len) {
			/*sanity check*/
			return NULL;
		}

		overlap = BM_Face_Exists(bm, verts, len, &f);
		
		/*clear flags*/
		for(i = 0; i < len; i++){
			BMO_ClearFlag(bm, edges[i]->v1, BM_EDGEVERT);
			BMO_ClearFlag(bm, edges[i]->v2, BM_EDGEVERT);
		}
		
		if(len > VERT_BUF_SIZE)
			MEM_freeN(verts);
	}

	if((!f) && (!overlap)) {
		f = bmesh_mf(bm, v1, v2, edges, len);
	}

	return f;
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
	BMHeader *current, *next;

	current = bm->polys.first;
	while(current){
		next = current->next;
		if(BMO_TestFlag(bm, current, flag)) bmesh_kf(bm, (BMFace*)current);
		current = next;
	}
}
void BM_remove_tagged_edges(BMesh *bm, int flag)
{
	BMHeader *current, *next;
	
	current = bm->edges.first;
	
	while(current){
		next = current->next;
		if(BMO_TestFlag(bm, current, flag)) bmesh_ke(bm, (BMEdge*)current);
		current = next;
	}
}

void BM_remove_tagged_verts(BMesh *bm, int flag)
{
	BMHeader *current, *next;

	current = bm->verts.first;

	while(current){
		next = current->next;
		if(BMO_TestFlag(bm, current, flag)) bmesh_kv(bm,(BMVert*)current);
		current = next;
	}
}

static void bm_copy_vert_attributes(BMesh *source_mesh, BMesh *target_mesh, BMVert *source_vertex, BMVert *target_vertex)
{
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata, source_vertex->data, &target_vertex->data);	
	target_vertex->bweight = source_vertex->bweight;
}

static void bm_copy_edge_attributes(BMesh *source_mesh, BMesh *target_mesh, BMEdge *source_edge, BMEdge *target_edge)
{
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata, source_edge->data, &target_edge->data);
	target_edge->crease = source_edge->crease;
	target_edge->bweight = source_edge->bweight;
}

static void bm_copy_loop_attributes(BMesh *source_mesh, BMesh *target_mesh, BMLoop *source_loop, BMLoop *target_loop)
{
	CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata, source_loop->data, &target_loop->data);
}

static void bm_copy_face_attributes(BMesh *source_mesh, BMesh *target_mesh, BMFace *source_face, BMFace *target_face)
{
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata, source_face->data, &target_face->data);	
	target_face->mat_nr = source_face->mat_nr;
}

/*Todo: Special handling for hide flags?*/

void BM_Copy_Attributes(BMesh *source_mesh, BMesh *target_mesh, void *source, void *target)
{
	BMHeader *sheader = source, *theader = target;
	
	if(sheader->type != theader->type)
		return;

	/*First we copy select*/
	if(BM_Is_Selected(source_mesh, source)) BM_Select(target_mesh, target, 1);
	
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
	V_DECLARE(vtable);
	BMEdge *e, *e2, **edges = NULL, **etable = NULL;
	V_DECLARE(edges);
	V_DECLARE(etable);
	BMLoop *l, *l2, **loops = NULL;
	V_DECLARE(loops);
	BMFace *f, *f2;

	BMIter iter, liter;
	int allocsize[4] = {512,512,2048,512}, numTex, numCol;
	int i;

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
		V_GROW(vtable);
		VECCOPY(v2->no, v->no);

		vtable[V_COUNT(vtable)-1] = v2;

		BMINDEX_SET(v, i);
		BMINDEX_SET(v2, i);
	}
	
	e = BMIter_New(&iter, bmold, BM_EDGES_OF_MESH, NULL);
	for (i=0; e; e=BMIter_Step(&iter), i++) {
		e2 = BM_Make_Edge(bm, vtable[BMINDEX_GET(e->v1)],
			          vtable[BMINDEX_GET(e->v2)], e, 0);

		BM_Copy_Attributes(bmold, bm, e, e2);
		V_GROW(etable);
		etable[V_COUNT(etable)-1] = e2;

		BMINDEX_SET(e, i);
		BMINDEX_SET(e2, i);
	}
	
	f = BMIter_New(&iter, bmold, BM_FACES_OF_MESH, NULL);
	for (; f; f=BMIter_Step(&iter)) {
		V_RESET(loops);
		V_RESET(edges);
		l = BMIter_New(&liter, bmold, BM_LOOPS_OF_FACE, f);
		for (i=0; i<f->len; i++, l = BMIter_Step(&liter)) {
			V_GROW(loops);
			V_GROW(edges);
			loops[i] = l;
			edges[i] = etable[BMINDEX_GET(l->e)];
		}

		v = vtable[BMINDEX_GET(loops[0]->v)];
		v2 = vtable[BMINDEX_GET(loops[1]->v)];

		if (!bmesh_verts_in_edge(v, v2, edges[0])) {
			v = vtable[BMINDEX_GET(loops[V_COUNT(loops)-1]->v)];
			v2 = vtable[BMINDEX_GET(loops[0]->v)];
		}

		f2 = BM_Make_Ngon(bm, v, v2, edges, f->len, 0);
		BM_Copy_Attributes(bmold, bm, f, f2);
		VECCOPY(f2->no, f->no);

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f2);
		for (i=0; i<f->len; i++, l = BMIter_Step(&liter)) {
			BM_Copy_Attributes(bmold, bm, loops[i], l);
		}
	}

	return bm;
}
