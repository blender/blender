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

#include "bmesh.h"
#include "bmesh_private.h"

/*
 * BMESH_CONSTRUCT.C
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
	BMVert **verts = vert_buf;
	BMFace *f = NULL;
	int overlap = 0, i;


	if(nodouble){
		if(len > VERT_BUF_SIZE)
			verts = MEM_callocN(sizeof(BMVert *) * len, "bmesh make ngon vertex array");
		for(i = 0; i < len; i++){
			if(!bmesh_test_sysflag((BMHeader*)(edges[i]->v1), BMESH_EDGEVERT)){
				bmesh_set_sysflag((BMHeader*)(edges[i]->v1), BMESH_EDGEVERT);
				verts[i] = edges[i]->v1;
			} else if(!bmesh_test_sysflag((BMHeader*)(edges[i]->v2), BMESH_EDGEVERT)) {
				bmesh_set_sysflag((BMHeader*)(edges[i]->v2), BMESH_EDGEVERT);
				verts[i] = 	edges[i]->v2;
			}
		}
		
		overlap = BM_Exist_Face_Overlaps(bm, verts, len, &f);
		
		/*clear flags*/
		for(i = 0; i < len; i++){
			bmesh_clear_sysflag((BMHeader*)(edges[i]->v1), BMESH_EDGEVERT);
			bmesh_clear_sysflag((BMHeader*)(edges[i]->v2), BMESH_EDGEVERT);
		}
		
		if(len > VERT_BUF_SIZE)
			MEM_freeN(verts);
	}
		
	if((!f) && (!overlap))
		f = bmesh_mf(bm, v1, v2, edges, len);

	return f;
}


/*bmesh_make_face_from_face(BMesh *bm, BMFace *source, BMFace *target) */

/*
 * BMESH DELETE XXX FUNCTIONS
 *
 * Functions for deleting the vertices, edges
 * and faces of a mesh. Note that these functions
 * only flag geometry for removal. The actual deletion
 * is done by the remove tagged XXX functions
 *
 * TO CONSIDER: This may be better to use the private flag
 * layers allocated for each operator rather than using the system flag.
 *
*/

void BM_Delete_Face(BMesh *bm, BMFace *f)
{
	bmesh_set_sysflag(&(f->head), BMESH_DELETE);
}

void BM_Delete_Edge(BMesh *bm, BMVert *e)
{
	BMFace *f = NULL;
	BMIter edgefaces;
	
	for(f = BMIter_New(&edgefaces, bm, BM_FACES_OF_EDGE, e ); f; f = BMIter_Step(&edgefaces))
		bmesh_set_sysflag((BMHeader*)f, BMESH_DELETE);
	bmesh_set_sysflag((BMHeader*)e, BMESH_DELETE);
}
void BM_Delete_Vert(BMesh *bm, BMVert *v)
{
	BMFace *f = NULL;
	BMEdge *e = NULL;
	BMIter vertfaces;
	BMIter vertedges;
	
	/*first delete the faces around the vertex*/
	for(f = BMIter_New(&vertfaces, bm, BM_FACES_OF_VERT, v ); f; f = BMIter_Step(&vertfaces))
		bmesh_set_sysflag((BMHeader*)f, BMESH_DELETE);	
	

	for(e = BMIter_New(&vertedges, bm, BM_EDGES_OF_VERT, v ); e; e = BMIter_Step(&vertedges))
		bmesh_set_sysflag((BMHeader*)e, BMESH_DELETE);
	
	bmesh_set_sysflag((BMHeader*)v, BMESH_DELETE);
}

/*
 * REMOVE TAGGED XXX
 *
 * Called at the end of bmesh_end_edit. Removes
 * Elements that have been marked for removal in the modelling loop.
 * We bypass iterator API for this to ensure correct results.
 *
*/

static void remove_tagged_faces(BMesh *bm)
{
	BMHeader *current, *next;

	current = bm->polys.first;
	while(current){
		next = current->next;
		if( bmesh_test_sysflag(current, BMESH_DELETE) ) bmesh_kf(bm, (BMFace*)current);
		current = next;
	}
}
static void remove_tagged_edges(BMesh *bm)
{
	BMHeader *current, *next;
	
	current = bm->edges.first;
	
	while(current){
		next = current->next;
		if( bmesh_test_sysflag(current, BMESH_DELETE) ) bmesh_ke(bm, (BMEdge*)current);
		current = next;
	}
}

static void remove_tagged_verts(BMesh *bm)
{
	BMHeader *current, *next;

	current = bm->verts.first;

	while(current){
		next = current->next;
		if( bmesh_test_sysflag(current, BMESH_DELETE) ) bmesh_kv(bm,(BMVert*)current);
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
	if(theader->type == BMESH_VERT)
		bm_copy_vert_attributes(source_mesh, target_mesh, (BMVert*)source, (BMVert*)target);
	else if(theader->type == BMESH_EDGE)
		bm_copy_edge_attributes(source_mesh, target_mesh, (BMEdge*)source, (BMEdge*)target);
	else if(theader->type == BMESH_LOOP)
		bm_copy_loop_attributes(source_mesh, target_mesh, (BMLoop*)source, (BMLoop*)target);
	else if(theader->type == BMESH_FACE)
		bm_copy_face_attributes(source_mesh, target_mesh, (BMFace*)source, (BMFace*)target);
}