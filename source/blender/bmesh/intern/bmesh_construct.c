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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_construct.c
 *  \ingroup bmesh
 *
 * BM construction functions.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "bmesh_private.h"

#define SELECT 1

/* prototypes */
static void bm_loop_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMLoop *source_loop, BMLoop *target_loop);

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
 */

BMFace *BM_face_create_quad_tri(BMesh *bm,
                                BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                const BMFace *example, const int nodouble)
{
	BMVert *vtar[4] = {v1, v2, v3, v4};
	return BM_face_create_quad_tri_v(bm, vtar, v4 ? 4 : 3, example, nodouble);
}

/* remove the edge array bits from this. Its not really needed? */
BMFace *BM_face_create_quad_tri_v(BMesh *bm, BMVert **verts, int len, const BMFace *example, const int nodouble)
{
	BMEdge *edar[4] = {NULL};
	BMFace *f = NULL;
	int overlap = 0;

	edar[0] = BM_edge_exists(verts[0], verts[1]);
	edar[1] = BM_edge_exists(verts[1], verts[2]);
	if (len == 4) {
		edar[2] = BM_edge_exists(verts[2], verts[3]);
		edar[3] = BM_edge_exists(verts[3], verts[0]);
	}
	else {
		edar[2] = BM_edge_exists(verts[2], verts[0]);
	}

	if (nodouble) {
		/* check if face exists or overlaps */
		if (len == 4) {
			overlap = BM_face_exists_overlap(bm, verts, len, &f, FALSE);
		}
		else {
			overlap = BM_face_exists_overlap(bm, verts, len, &f, FALSE);
		}
	}

	/* make new face */
	if ((!f) && (!overlap)) {
		if (!edar[0]) edar[0] = BM_edge_create(bm, verts[0], verts[1], NULL, FALSE);
		if (!edar[1]) edar[1] = BM_edge_create(bm, verts[1], verts[2], NULL, FALSE);
		if (len == 4) {
			if (!edar[2]) edar[2] = BM_edge_create(bm, verts[2], verts[3], NULL, FALSE);
			if (!edar[3]) edar[3] = BM_edge_create(bm, verts[3], verts[0], NULL, FALSE);
		}
		else {
			if (!edar[2]) edar[2] = BM_edge_create(bm, verts[2], verts[0], NULL, FALSE);
		}

		f = BM_face_create(bm, verts, edar, len, FALSE);

		if (example && f) {
			BM_elem_attrs_copy(bm, bm, example, f);
		}
	}

	return f;
}


/* copies face data from shared adjacent faces */
void BM_face_copy_shared(BMesh *bm, BMFace *f)
{
	BMIter iter;
	BMLoop *l, *l2;

	if (!f) return;

	l = BM_iter_new(&iter, bm, BM_LOOPS_OF_FACE, f);
	for ( ; l; l = BM_iter_step(&iter)) {
		l2 = l->radial_next;
		
		if (l2 && l2 != l) {
			if (l2->v == l->v) {
				bm_loop_attrs_copy(bm, bm, l2, l);
			}
			else {
				l2 = l2->next;
				bm_loop_attrs_copy(bm, bm, l2, l);
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
 *
 * Note that while this function will work fine when the edges
 * are already sorted, if the edges are always going to be sorted,
 * BM_face_create should be considered over this function as it
 * avoids some unnecessary work.
 */
BMFace *BM_face_create_ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble)
{
	BMEdge **edges2 = NULL;
	BLI_array_staticdeclare(edges2, BM_NGON_STACK_SIZE);
	BMVert **verts = NULL, *v;
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	BMFace *f = NULL;
	BMEdge *e;
	BMVert *ev1, *ev2;
	int i, /* j, */ v1found, reverse;

	/* this code is hideous, yeek.  I'll have to think about ways of
	 *  cleaning it up.  basically, it now combines the old BM_face_create_ngon
	 *  _and_ the old bmesh_mf functions, so its kindof smashed together
	 * - joeedh */

	if (!len || !v1 || !v2 || !edges || !bm)
		return NULL;

	/* put edges in correct order */
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_ENABLE(edges[i], _FLAG_MF);
	}

	ev1 = edges[0]->v1;
	ev2 = edges[0]->v2;

	if (v1 == ev2) {
		/* Swapping here improves performance and consistency of face
		 * structure in the special case that the edges are already in
		 * the correct order and winding */
		SWAP(BMVert *, ev1, ev2);
	}

	BLI_array_append(verts, ev1);
	v = ev2;
	e = edges[0];
	do {
		BMEdge *e2 = e;

		BLI_array_append(verts, v);
		BLI_array_append(edges2, e);

		do {
			e2 = bmesh_disk_edge_next(e2, v);
			if (e2 != e && BM_ELEM_API_FLAG_TEST(e2, _FLAG_MF)) {
				v = BM_edge_other_vert(e2, v);
				break;
			}
		} while (e2 != e);

		if (e2 == e)
			goto err; /* the edges do not form a closed loop */

		e = e2;
	} while (e != edges[0]);

	if (BLI_array_count(edges2) != len) {
		goto err; /* we didn't use all edges in forming the boundary loop */
	}

	/* ok, edges are in correct order, now ensure they are going
	 * in the correct direction */
	v1found = reverse = FALSE;
	for (i = 0; i < len; i++) {
		if (BM_vert_in_edge(edges2[i], v1)) {
			/* see if v1 and v2 are in the same edge */
			if (BM_vert_in_edge(edges2[i], v2)) {
				/* if v1 is shared by the *next* edge, then the winding
				 * is incorrect */
				if (BM_vert_in_edge(edges2[(i + 1) % len], v1)) {
					reverse = TRUE;
					break;
				}
			}

			v1found = TRUE;
		}

		if ((v1found == FALSE) && BM_vert_in_edge(edges2[i], v2)) {
			reverse = TRUE;
			break;
		}
	}

	if (reverse) {
		for (i = 0; i < len / 2; i++) {
			v = verts[i];
			verts[i] = verts[len - i - 1];
			verts[len - i - 1] = v;
		}
	}

	for (i = 0; i < len; i++) {
		edges2[i] = BM_edge_exists(verts[i], verts[(i + 1) % len]);
		if (!edges2[i]) {
			goto err;
		}
	}

	f = BM_face_create(bm, verts, edges2, len, nodouble);

	/* clean up flags */
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edges2[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return f;

err:
	for (i = 0; i < len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edges[i], _FLAG_MF);
	}

	BLI_array_free(verts);
	BLI_array_free(edges2);

	return NULL;
}


/* bmesh_make_face_from_face(BMesh *bm, BMFace *source, BMFace *target) */


/*
 * REMOVE TAGGED XXX
 *
 * Called by operators to remove elements that they have marked for
 * removal.
 *
 */

void BMO_remove_tagged_faces(BMesh *bm, const short oflag)
{
	BMFace *f;
	BMIter iter;

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, f, oflag)) {
			BM_face_kill(bm, f);
		}
	}
}

void BMO_remove_tagged_edges(BMesh *bm, const short oflag)
{
	BMEdge *e;
	BMIter iter;

	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, e, oflag)) {
			BM_edge_kill(bm, e);
		}
	}
}

void BMO_remove_tagged_verts(BMesh *bm, const short oflag)
{
	BMVert *v;
	BMIter iter;

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, v, oflag)) {
			BM_vert_kill(bm, v);
		}
	}
}

/*************************************************************/
/* you need to make remove tagged verts/edges/faces
 * api functions that take a filter callback.....
 * and this new filter type will be for opstack flags.
 * This is because the BM_remove_taggedXXX functions bypass iterator API.
 *  - Ops dont care about 'UI' considerations like selection state, hide state, ect.
 *    If you want to work on unhidden selections for instance,
 *    copy output from a 'select context' operator to another operator....
 */

static void bmo_remove_tagged_context_verts(BMesh *bm, const short oflag)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;

	for (v = BM_iter_new(&verts, bm, BM_VERTS_OF_MESH, bm); v; v = BM_iter_step(&verts)) {
		if (BMO_elem_flag_test(bm, v, oflag)) {
			/* Visit edge */
			for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_VERT, v); e; e = BM_iter_step(&edges))
				BMO_elem_flag_enable(bm, e, oflag);
			/* Visit face */
			for (f = BM_iter_new(&faces, bm, BM_FACES_OF_VERT, v); f; f = BM_iter_step(&faces))
				BMO_elem_flag_enable(bm, f, oflag);
		}
	}

	BMO_remove_tagged_faces(bm, oflag);
	BMO_remove_tagged_edges(bm, oflag);
	BMO_remove_tagged_verts(bm, oflag);
}

static void bmo_remove_tagged_context_edges(BMesh *bm, const short oflag)
{
	BMEdge *e;
	BMFace *f;

	BMIter edges;
	BMIter faces;

	for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_MESH, bm); e; e = BM_iter_step(&edges)) {
		if (BMO_elem_flag_test(bm, e, oflag)) {
			for (f = BM_iter_new(&faces, bm, BM_FACES_OF_EDGE, e); f; f = BM_iter_step(&faces)) {
				BMO_elem_flag_enable(bm, f, oflag);
			}
		}
	}
	BMO_remove_tagged_faces(bm, oflag);
	BMO_remove_tagged_edges(bm, oflag);
}

#define DEL_WIREVERT	(1 << 10)

/* warning, oflag applies to different types in some contexts,
 * not just the type being removed */
void BMO_remove_tagged_context(BMesh *bm, const short oflag, const int type)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;

	switch (type) {
		case DEL_VERTS:
		{
			bmo_remove_tagged_context_verts(bm, oflag);

			break;
		}
		case DEL_EDGES:
		{
			/* flush down to vert */
			for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_MESH, bm); e; e = BM_iter_step(&edges)) {
				if (BMO_elem_flag_test(bm, e, oflag)) {
					BMO_elem_flag_enable(bm, e->v1, oflag);
					BMO_elem_flag_enable(bm, e->v2, oflag);
				}
			}
			bmo_remove_tagged_context_edges(bm, oflag);
			/* remove loose vertice */
			for (v = BM_iter_new(&verts, bm, BM_VERTS_OF_MESH, bm); v; v = BM_iter_step(&verts)) {
				if (BMO_elem_flag_test(bm, v, oflag) && (!(v->e)))
					BMO_elem_flag_enable(bm, v, DEL_WIREVERT);
			}
			BMO_remove_tagged_verts(bm, DEL_WIREVERT);

			break;
		}
		case DEL_EDGESFACES:
		{
			bmo_remove_tagged_context_edges(bm, oflag);

			break;
		}
		case DEL_ONLYFACES:
		{
			BMO_remove_tagged_faces(bm, oflag);

			break;
		}
		case DEL_ONLYTAGGED:
		{
			BMO_remove_tagged_faces(bm, oflag);
			BMO_remove_tagged_edges(bm, oflag);
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
		case DEL_FACES:
		{
			/* go through and mark all edges and all verts of all faces for delet */
			for (f = BM_iter_new(&faces, bm, BM_FACES_OF_MESH, bm); f; f = BM_iter_step(&faces)) {
				if (BMO_elem_flag_test(bm, f, oflag)) {
					for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_FACE, f); e; e = BM_iter_step(&edges))
						BMO_elem_flag_enable(bm, e, oflag);
					for (v = BM_iter_new(&verts, bm, BM_VERTS_OF_FACE, f); v; v = BM_iter_step(&verts))
						BMO_elem_flag_enable(bm, v, oflag);
				}
			}
			/* now go through and mark all remaining faces all edges for keeping */
			for (f = BM_iter_new(&faces, bm, BM_FACES_OF_MESH, bm); f; f = BM_iter_step(&faces)) {
				if (!BMO_elem_flag_test(bm, f, oflag)) {
					for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_FACE, f); e; e = BM_iter_step(&edges)) {
						BMO_elem_flag_disable(bm, e, oflag);
					}
					for (v = BM_iter_new(&verts, bm, BM_VERTS_OF_FACE, f); v; v = BM_iter_step(&verts)) {
						BMO_elem_flag_disable(bm, v, oflag);
					}
				}
			}
			/* also mark all the vertices of remaining edges for keeping */
			for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_MESH, bm); e; e = BM_iter_step(&edges)) {
				if (!BMO_elem_flag_test(bm, e, oflag)) {
					BMO_elem_flag_disable(bm, e->v1, oflag);
					BMO_elem_flag_disable(bm, e->v2, oflag);
				}
			}
			/* now delete marked face */
			BMO_remove_tagged_faces(bm, oflag);
			/* delete marked edge */
			BMO_remove_tagged_edges(bm, oflag);
			/* remove loose vertice */
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
		case DEL_ALL:
		{
			/* does this option even belong in here? */
			for (f = BM_iter_new(&faces, bm, BM_FACES_OF_MESH, bm); f; f = BM_iter_step(&faces))
				BMO_elem_flag_enable(bm, f, oflag);
			for (e = BM_iter_new(&edges, bm, BM_EDGES_OF_MESH, bm); e; e = BM_iter_step(&edges))
				BMO_elem_flag_enable(bm, e, oflag);
			for (v = BM_iter_new(&verts, bm, BM_VERTS_OF_MESH, bm); v; v = BM_iter_step(&verts))
				BMO_elem_flag_enable(bm, v, oflag);

			BMO_remove_tagged_faces(bm, oflag);
			BMO_remove_tagged_edges(bm, oflag);
			BMO_remove_tagged_verts(bm, oflag);

			break;
		}
	}
}
/*************************************************************/


static void bm_vert_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMVert *source_vertex, BMVert *target_vertex)
{
	if ((source_mesh == target_mesh) && (source_vertex == target_vertex)) {
		return;
	}
	copy_v3_v3(target_vertex->no, source_vertex->no);
	CustomData_bmesh_free_block(&target_mesh->vdata, &target_vertex->head.data);
	CustomData_bmesh_copy_data(&source_mesh->vdata, &target_mesh->vdata,
	                           source_vertex->head.data, &target_vertex->head.data);
}

static void bm_edge_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMEdge *source_edge, BMEdge *target_edge)
{
	if ((source_mesh == target_mesh) && (source_edge == target_edge)) {
		return;
	}
	CustomData_bmesh_free_block(&target_mesh->edata, &target_edge->head.data);
	CustomData_bmesh_copy_data(&source_mesh->edata, &target_mesh->edata,
	                           source_edge->head.data, &target_edge->head.data);
}

static void bm_loop_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMLoop *source_loop, BMLoop *target_loop)
{
	if ((source_mesh == target_mesh) && (source_loop == target_loop)) {
		return;
	}
	CustomData_bmesh_free_block(&target_mesh->ldata, &target_loop->head.data);
	CustomData_bmesh_copy_data(&source_mesh->ldata, &target_mesh->ldata,
	                           source_loop->head.data, &target_loop->head.data);
}

static void bm_face_attrs_copy(BMesh *source_mesh, BMesh *target_mesh,
                               const BMFace *source_face, BMFace *target_face)
{
	if ((source_mesh == target_mesh) && (source_face == target_face)) {
		return;
	}
	copy_v3_v3(target_face->no, source_face->no);
	CustomData_bmesh_free_block(&target_mesh->pdata, &target_face->head.data);
	CustomData_bmesh_copy_data(&source_mesh->pdata, &target_mesh->pdata,
	                           source_face->head.data, &target_face->head.data);
	target_face->mat_nr = source_face->mat_nr;
}

/* BMESH_TODO: Special handling for hide flags? */

void BM_elem_attrs_copy(BMesh *source_mesh, BMesh *target_mesh, const void *source, void *target)
{
	const BMHeader *sheader = source;
	BMHeader *theader = target;

	BLI_assert(sheader->htype == theader->htype);

	if (sheader->htype != theader->htype)
		return;

	/* First we copy select */
	if (BM_elem_flag_test((BMElem *)sheader, BM_ELEM_SELECT)) {
		BM_elem_select_set(target_mesh, (BMElem *)target, TRUE);
	}
	
	/* Now we copy flags */
	theader->hflag = sheader->hflag;
	
	/* Copy specific attributes */
	switch (theader->htype) {
		case BM_VERT:
			bm_vert_attrs_copy(source_mesh, target_mesh, (const BMVert *)source, (BMVert *)target);
			break;
		case BM_EDGE:
			bm_edge_attrs_copy(source_mesh, target_mesh, (const BMEdge *)source, (BMEdge *)target);
			break;
		case BM_LOOP:
			bm_loop_attrs_copy(source_mesh, target_mesh, (const BMLoop *)source, (BMLoop *)target);
			break;
		case BM_FACE:
			bm_face_attrs_copy(source_mesh, target_mesh, (const BMFace *)source, (BMFace *)target);
			break;
		default:
			BLI_assert(0);
	}
}

BMesh *BM_mesh_copy(BMesh *bmold)
{
	BMesh *bm;
	BMVert *v, *v2, **vtable = NULL;
	BMEdge *e, *e2, **edges = NULL, **etable = NULL;
	BLI_array_declare(edges);
	BMLoop *l, /* *l2, */ **loops = NULL;
	BLI_array_declare(loops);
	BMFace *f, *f2, **ftable = NULL;
	BMEditSelection *ese;
	BMIter iter, liter;
	int i, j;

	/* allocate a bmesh */
	bm = BM_mesh_create(bmold->ob, bm_mesh_allocsize_default);

	CustomData_copy(&bmold->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->ldata, &bm->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bmold->pdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm->vdata, bm_mesh_allocsize_default[0]);
	CustomData_bmesh_init_pool(&bm->edata, bm_mesh_allocsize_default[1]);
	CustomData_bmesh_init_pool(&bm->ldata, bm_mesh_allocsize_default[2]);
	CustomData_bmesh_init_pool(&bm->pdata, bm_mesh_allocsize_default[3]);

	vtable = MEM_mallocN(sizeof(BMVert *) * bmold->totvert, "BM_mesh_copy vtable");
	etable = MEM_mallocN(sizeof(BMEdge *) * bmold->totedge, "BM_mesh_copy etable");
	ftable = MEM_mallocN(sizeof(BMFace *) * bmold->totface, "BM_mesh_copy ftable");

	v = BM_iter_new(&iter, bmold, BM_VERTS_OF_MESH, NULL);
	for (i = 0; v; v = BM_iter_step(&iter), i++) {
		v2 = BM_vert_create(bm, v->co, NULL); /* copy between meshes so cant use 'example' argument */
		BM_elem_attrs_copy(bmold, bm, v, v2);
		vtable[i] = v2;
		BM_elem_index_set(v, i); /* set_inline */
		BM_elem_index_set(v2, i); /* set_inline */
	}
	bmold->elem_index_dirty &= ~BM_VERT;
	bm->elem_index_dirty &= ~BM_VERT;

	/* safety check */
	BLI_assert(i == bmold->totvert);
	
	e = BM_iter_new(&iter, bmold, BM_EDGES_OF_MESH, NULL);
	for (i = 0; e; e = BM_iter_step(&iter), i++) {
		e2 = BM_edge_create(bm,
		                    vtable[BM_elem_index_get(e->v1)],
		                    vtable[BM_elem_index_get(e->v2)],
		                    e, FALSE);

		BM_elem_attrs_copy(bmold, bm, e, e2);
		etable[i] = e2;
		BM_elem_index_set(e, i); /* set_inline */
		BM_elem_index_set(e2, i); /* set_inline */
	}
	bmold->elem_index_dirty &= ~BM_EDGE;
	bm->elem_index_dirty &= ~BM_EDGE;

	/* safety check */
	BLI_assert(i == bmold->totedge);
	
	f = BM_iter_new(&iter, bmold, BM_FACES_OF_MESH, NULL);
	for (i = 0; f; f = BM_iter_step(&iter), i++) {
		BM_elem_index_set(f, i); /* set_inline */

		BLI_array_empty(loops);
		BLI_array_empty(edges);
		BLI_array_growitems(loops, f->len);
		BLI_array_growitems(edges, f->len);

		l = BM_iter_new(&liter, bmold, BM_LOOPS_OF_FACE, f);
		for (j = 0; j < f->len; j++, l = BM_iter_step(&liter)) {
			loops[j] = l;
			edges[j] = etable[BM_elem_index_get(l->e)];
		}

		v = vtable[BM_elem_index_get(loops[0]->v)];
		v2 = vtable[BM_elem_index_get(loops[1]->v)];

		if (!bmesh_verts_in_edge(v, v2, edges[0])) {
			v = vtable[BM_elem_index_get(loops[BLI_array_count(loops) - 1]->v)];
			v2 = vtable[BM_elem_index_get(loops[0]->v)];
		}

		f2 = BM_face_create_ngon(bm, v, v2, edges, f->len, FALSE);
		if (!f2)
			continue;
		/* use totface incase adding some faces fails */
		BM_elem_index_set(f2, (bm->totface - 1)); /* set_inline */

		ftable[i] = f2;

		BM_elem_attrs_copy(bmold, bm, f, f2);
		copy_v3_v3(f2->no, f->no);

		l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f2);
		for (j = 0; j < f->len; j++, l = BM_iter_step(&liter)) {
			BM_elem_attrs_copy(bmold, bm, loops[j], l);
		}

		if (f == bmold->act_face) bm->act_face = f2;
	}
	bmold->elem_index_dirty &= ~BM_FACE;
	bm->elem_index_dirty &= ~BM_FACE;

	/* safety check */
	BLI_assert(i == bmold->totface);

	/* copy over edit selection history */
	for (ese = bmold->selected.first; ese; ese = ese->next) {
		void *ele = NULL;

		if (ese->htype == BM_VERT)
			ele = vtable[BM_elem_index_get(ese->ele)];
		else if (ese->htype == BM_EDGE)
			ele = etable[BM_elem_index_get(ese->ele)];
		else if (ese->htype == BM_FACE) {
			ele = ftable[BM_elem_index_get(ese->ele)];
		}
		else {
			BLI_assert(0);
		}
		
		if (ele)
			BM_select_history_store(bm, ele);
	}

	MEM_freeN(etable);
	MEM_freeN(vtable);
	MEM_freeN(ftable);

	BLI_array_free(loops);
	BLI_array_free(edges);

	return bm;
}

/* ME -> BM */
char BM_vert_flag_from_mflag(const char  meflag)
{
	return ( ((meflag & SELECT)       ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_HIDE)      ? BM_ELEM_HIDDEN : 0)
	         );
}
char BM_edge_flag_from_mflag(const short meflag)
{
	return ( ((meflag & SELECT)        ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_SEAM)       ? BM_ELEM_SEAM   : 0) |
	         ((meflag & ME_SHARP) == 0 ? BM_ELEM_SMOOTH : 0) | /* invert */
	         ((meflag & ME_HIDE)       ? BM_ELEM_HIDDEN : 0)
	         );
}
char BM_face_flag_from_mflag(const char  meflag)
{
	return ( ((meflag & ME_FACE_SEL)  ? BM_ELEM_SELECT : 0) |
	         ((meflag & ME_SMOOTH)    ? BM_ELEM_SMOOTH : 0) |
	         ((meflag & ME_HIDE)      ? BM_ELEM_HIDDEN : 0)
	         );
}

/* BM -> ME */
char  BM_vert_flag_to_mflag(BMVert *eve)
{
	const char hflag = eve->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT)  ? SELECT  : 0) |
	         ((hflag & BM_ELEM_HIDDEN)  ? ME_HIDE : 0)
	         );
}
short BM_edge_flag_to_mflag(BMEdge *eed)
{
	const char hflag = eed->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT)       ? SELECT    : 0) |
	         ((hflag & BM_ELEM_SEAM)         ? ME_SEAM   : 0) |
	         ((hflag & BM_ELEM_SMOOTH) == 0  ? ME_SHARP  : 0) |
	         ((hflag & BM_ELEM_HIDDEN)       ? ME_HIDE   : 0) |
	         ((BM_edge_is_wire(NULL, eed)) ? ME_LOOSEEDGE : 0) | /* not typical */
	         (ME_EDGEDRAW | ME_EDGERENDER)
	         );
}
char  BM_face_flag_to_mflag(BMFace *efa)
{
	const char hflag = efa->head.hflag;

	return ( ((hflag & BM_ELEM_SELECT) ? ME_FACE_SEL : 0) |
	         ((hflag & BM_ELEM_SMOOTH) ? ME_SMOOTH   : 0) |
	         ((hflag & BM_ELEM_HIDDEN) ? ME_HIDE     : 0)
	         );
}
