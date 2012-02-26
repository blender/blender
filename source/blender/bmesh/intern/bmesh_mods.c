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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_mods.c
 *  \ingroup bmesh
 *
 * This file contains functions for locally modifying
 * the topology of existing mesh data. (split, join, flip etc).
 */

#include "MEM_guardedalloc.h"


#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_private.h"

/**
 *			bmesh_dissolve_disk
 *
 *  Turns the face region surrounding a manifold vertex into
 *  A single polygon.
 *
 *
 * Example:
 *
 *          |=========|             |=========|
 *          |  \   /  |             |         |
 * Before:  |    V    |      After: |         |
 *          |  /   \  |             |         |
 *          |=========|             |=========|
 *
 *
 */
#if 1
int BM_vert_dissolve(BMesh *bm, BMVert *v)
{
	const int len = BM_vert_edge_count(v);
	
	if (len == 1) {
		BM_vert_kill(bm, v); /* will kill edges too */
		return TRUE;
	}
	else if (!BM_vert_is_manifold(bm, v)) {
		if (!v->e) {
			BM_vert_kill(bm, v);
			return TRUE;
		}
		else if (!v->e->l) {
			if (len == 2) {
				return (BM_vert_collapse_edge(bm, v->e, v, TRUE) != NULL);
			}
			else {
				/* used to kill the vertex here, but it may be connected to faces.
				 * so better do nothing */
				return FALSE;
			}
		}
		else {
			return FALSE;
		}
	}
	else if (len == 2 && BM_vert_face_count(v) == 1) {
		/* boundry vertex on a face */
		return (BM_vert_collapse_edge(bm, v->e, v, TRUE) != NULL);
	}
	else {
		return BM_disk_dissolve(bm, v);
	}
}

int BM_disk_dissolve(BMesh *bm, BMVert *v)
{
	BMFace *f, *f2;
	BMEdge *e, *keepedge = NULL, *baseedge = NULL;
	int len = 0;

	if (!BM_vert_is_manifold(bm, v)) {
		return FALSE;
	}
	
	if (v->e) {
		/* v->e we keep, what else */
		e = v->e;
		do {
			e = bmesh_disk_nextedge(e, v);
			if (!(BM_edge_share_face_count(e, v->e))) {
				keepedge = e;
				baseedge = v->e;
				break;
			}
			len++;
		} while (e != v->e);
	}
	
	/* this code for handling 2 and 3-valence verts
	 * may be totally bad */
	if (keepedge == NULL && len == 3) {
		/* handle specific case for three-valence.  solve it by
		 * increasing valence to four.  this may be hackish. .  */
		BMLoop *loop = e->l;
		if (loop->v == v) loop = loop->next;
		if (!BM_face_split(bm, loop->f, v, loop->v, NULL, NULL))
			return FALSE;

		if (!BM_disk_dissolve(bm, v)) {
			return FALSE;
		}
		return TRUE;
	}
	else if (keepedge == NULL && len == 2) {
		/* collapse the verte */
		e = BM_vert_collapse_faces(bm, v->e, v, 1.0, TRUE, TRUE);

		if (!e) {
			return FALSE;
		}

		/* handle two-valenc */
		f = e->l->f;
		f2 = e->l->radial_next->f;

		if (f != f2 && !BM_faces_join_pair(bm, f, f2, e)) {
			return FALSE;
		}

		return TRUE;
	}

	if (keepedge) {
		int done = 0;

		while (!done) {
			done = 1;
			e = v->e;
			do {
				f = NULL;
				len = bmesh_radial_length(e->l);
				if (len == 2 && (e != baseedge) && (e != keepedge)) {
					f = BM_faces_join_pair(bm, e->l->f, e->l->radial_next->f, e);
					/* return if couldn't join faces in manifold
					 * conditions */
					//!disabled for testing why bad things happen
					if (!f) {
						return FALSE;
					}
				}

				if (f) {
					done = 0;
					break;
				}
				e = bmesh_disk_nextedge(e, v);
			} while (e != v->e);
		}

		/* collapse the verte */
		e = BM_vert_collapse_faces(bm, baseedge, v, 1.0, TRUE, TRUE);

		if (!e) {
			return FALSE;
		}
		
		/* get remaining two face */
		f = e->l->f;
		f2 = e->l->radial_next->f;

		if (f != f2) {
			/* join two remaining face */
			if (!BM_faces_join_pair(bm, f, f2, e)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}
#else
void BM_disk_dissolve(BMesh *bm, BMVert *v)
{
	BMFace *f;
	BMEdge *e;
	BMIter iter;
	int done, len;
	
	if (v->e) {
		done = 0;
		while (!done) {
			done = 1;
			
			/* loop the edges looking for an edge to dissolv */
			for (e = BM_iter_new(&iter, bm, BM_EDGES_OF_VERT, v); e;
			     e = BM_iter_step(&iter)) {
				f = NULL;
				len = bmesh_cycle_length(&(e->l->radial));
				if (len == 2) {
					f = BM_faces_join_pair(bm, e->l->f, ((BMLoop *)(e->l->radial_next))->f, e);
				}
				if (f) {
					done = 0;
					break;
				}
			};
		}
		BM_vert_collapse_faces(bm, v->e, v, 1.0, TRUE);
	}
}
#endif

/**
 * BM_faces_join_pair
 *
 *  Joins two adjacenct faces togather.
 *
 *  Because this method calls to BM_faces_join to do its work, ff a pair
 *  of faces share multiple edges, the pair of faces will be joined at
 *  every edge (not just edge e). This part of the functionality might need
 *  to be reconsidered.
 *
 *  If the windings do not match the winding of the new face will follow
 *  f1's winding (i.e. f2 will be reversed before the join).
 *
 * Returns:
 *	 pointer to the combined face
 */

BMFace *BM_faces_join_pair(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	BMLoop *l1, *l2;
	BMEdge *jed = NULL;
	BMFace *faces[2] = {f1, f2};
	
	jed = e;
	if (!jed) {
		BMLoop *l_first;
		/* search for an edge that has both these faces in its radial cycl */
		l1 = l_first = BM_FACE_FIRST_LOOP(f1);
		do {
			if (l1->radial_next->f == f2) {
				jed = l1->e;
				break;
			}
		} while ((l1 = l1->next) != l_first);
	}

	if (UNLIKELY(!jed)) {
		BMESH_ASSERT(0);
		return NULL;
	}
	
	l1 = jed->l;
	
	if (UNLIKELY(!l1)) {
		BMESH_ASSERT(0);
		return NULL;
	}
	
	l2 = l1->radial_next;
	if (l1->v == l2->v) {
		bmesh_loop_reverse(bm, f2);
	}

	f1 = BM_faces_join(bm, faces, 2);
	
	return f1;
}

/* connects two verts together, automatically (if very naively) finding the
 * face they both share (if there is one) and splittling it.  use this at your
 * own risk, as it doesn't handle the many complex cases it should (like zero-area faces,
 * multiple faces, etc).
 *
 * this is really only meant for cases where you don't know before hand the face
 * the two verts belong to for splitting (e.g. the subdivision operator).
 */

BMEdge *BM_verts_connect(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf)
{
	BMIter iter, iter2;
	BMVert *v;
	BMLoop *nl;
	BMFace *face;

	/* be warned: this can do weird things in some ngon situation, see BM_face_legal_splits */
	for (face = BM_iter_new(&iter, bm, BM_FACES_OF_VERT, v1); face; face = BM_iter_step(&iter)) {
		for (v = BM_iter_new(&iter2, bm, BM_VERTS_OF_FACE, face); v; v = BM_iter_step(&iter2)) {
			if (v == v2) {
				face = BM_face_split(bm, face, v1, v2, &nl, NULL);

				if (nf) *nf = face;
				return nl->e;
			}
		}
	}

	return NULL;
}

/**
 * BM_face_split
 *
 *  Splits a single face into two.
 *
 *   f - the original face
 *   v1 & v2 - vertices which define the split edge, must be different
 *   nl - pointer which will receive the BMLoop for the split edge in the new face
 *
 *  Notes: the

 *  Returns -
 *	  Pointer to the newly created face representing one side of the split
 *   if the split is successful (and the original original face will be the
 *   other side). NULL if the split fails.
 *
 */

BMFace *BM_face_split(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, BMLoop **nl, BMEdge *example)
{
	const int has_mdisp = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	BMFace *nf, *of;

	BLI_assert(v1 != v2);

	/* do we have a multires layer */
	if (has_mdisp) {
		of = BM_face_copy(bm, f, FALSE, FALSE);
	}
	
#ifdef USE_BMESH_HOLES
	nf = bmesh_sfme(bm, f, v1, v2, nl, NULL, example);
#else
	nf = bmesh_sfme(bm, f, v1, v2, nl, example);
#endif
	
	if (nf) {
		BM_elem_attrs_copy(bm, bm, f, nf);
		copy_v3_v3(nf->no, f->no);

		/* handle multires update */
		if (has_mdisp && (nf != f)) {
			BMLoop *l_iter;
			BMLoop *l_first;

			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				BM_loop_interp_from_face(bm, l_iter, of, FALSE, TRUE);
			} while ((l_iter = l_iter->next) != l_first);

			l_iter = l_first = BM_FACE_FIRST_LOOP(nf);
			do {
				BM_loop_interp_from_face(bm, l_iter, of, FALSE, TRUE);
			} while ((l_iter = l_iter->next) != l_first);

			BM_face_kill(bm, of);

#if 0
			/* BM_face_multires_bounds_smooth doesn't flip displacement correct */
			BM_face_multires_bounds_smooth(bm, f);
			BM_face_multires_bounds_smooth(bm, nf);
#endif
		}
	}

	return nf;
}

/**
 *			BM_vert_collapse_faces
 *
 *  Collapses a vertex that has only two manifold edges
 *  onto a vertex it shares an edge with. Fac defines
 *  the amount of interpolation for Custom Data.
 *
 *  Note that this is not a general edge collapse function.
 *
 * Note this function is very close to 'BM_vert_collapse_edge', both collapse
 * a vertex and return a new edge. Except this takes a factor and merges
 * custom data.
 *
 *  BMESH_TODO:
 *    Insert error checking for KV valance.
 *
 * @param fac The factor along the edge
 * @param join_faces When true the faces around the vertex will be joined
 * otherwise collapse the vertex by merging the 2 edges this vert touches into one.
 *  @returns The New Edge
 */

BMEdge *BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac,
                               const short join_faces, const short kill_degenerate_faces)
{
	BMEdge *ne = NULL;
	BMVert *tv = bmesh_edge_getothervert(ke, kv);

	BMEdge *e2;
	BMVert *tv2;

	BMIter iter;
	BMLoop *l_iter = NULL, *kvloop = NULL, *tvloop = NULL;

	void *src[2];
	float w[2];

	/* Only intended to be called for 2-valence vertices */
	BLI_assert(bmesh_disk_count(kv) <= 2);


	/* first modify the face loop data  */
	w[0] = 1.0f - fac;
	w[1] = fac;

	if (ke->l) {
		l_iter = ke->l;
		do {
			if (l_iter->v == tv && l_iter->next->v == kv) {
				tvloop = l_iter;
				kvloop = l_iter->next;

				src[0] = kvloop->head.data;
				src[1] = tvloop->head.data;
				CustomData_bmesh_interp(&bm->ldata, src, w, NULL, 2, kvloop->head.data);
			}
		} while ((l_iter = l_iter->radial_next) != ke->l);
	}

	/* now interpolate the vertex data */
	BM_data_interp_from_verts(bm, kv, tv, kv, fac);

	e2 = bmesh_disk_nextedge(ke, kv);
	tv2 = BM_edge_other_vert(e2, kv);

	if (join_faces) {
		BMFace **faces = NULL, *f;
		BLI_array_staticdeclare(faces, 8);

		BM_ITER(f, &iter, bm, BM_FACES_OF_VERT, kv) {
			BLI_array_append(faces, f);
		}

		if (BLI_array_count(faces) >= 2) {
			BMFace *f2 = BM_faces_join(bm, faces, BLI_array_count(faces));
			if (f2) {
				BMLoop *nl = NULL;
				if (BM_face_split(bm, f2, tv, tv2, &nl, NULL)) {
					ne = nl->e;
				}
			}
		}

		BLI_array_free(faces);
	}
	else {
		/* single face or no faces */
		/* same as BM_vert_collapse_edge() however we already
		 * have vars to perform this operation so dont call. */
		ne = bmesh_jekv(bm, ke, kv, TRUE);
		/* ne = BM_edge_exists(tv, tv2); */ /* same as return above */

		if (kill_degenerate_faces) {
			BMIter fiter;
			BMFace *f;
			BMVert *verts[2] = {ne->v1, ne->v2};
			int i;
			for (i = 0; i < 2; i++) {
				BM_ITER(f, &fiter, bm, BM_FACES_OF_VERT, verts[i]) {
					if (f->len < 3) {
						BM_face_kill(bm, f);
					}
				}
			}
		}
	}

	return ne;
}


/**
 *			BM_vert_collapse_edge
 *
 * Collapses a vertex onto another vertex it shares an edge with.
 *
 * Returns -
 * The New Edge
 */

BMEdge *BM_vert_collapse_edge(BMesh *bm, BMEdge *ke, BMVert *kv,
                              const short kill_degenerate_faces)
{
	/* nice example implementation but we want loops to have their customdata
	 * accounted for */
#if 0
	BMEdge *ne = NULL;

	/* Collapse between 2 edges */

	/* in this case we want to keep all faces and not join them,
	 * rather just get rid of the veretex - see bug [#28645] */
	BMVert *tv  = bmesh_edge_getothervert(ke, kv);
	if (tv) {
		BMEdge *e2 = bmesh_disk_nextedge(ke, kv);
		if (e2) {
			BMVert *tv2 = BM_edge_other_vert(e2, kv);
			if (tv2) {
				/* only action, other calls here only get the edge to return */
				ne = bmesh_jekv(bm, ke, kv);

				/* ne = BM_edge_exists(tv, tv2); */ /* same as return above */
			}
		}
	}

	return ne;
#else
	/* with these args faces are never joined, same as above
	 * but account for loop customdata */
	return BM_vert_collapse_faces(bm, ke, kv, 1.0f, FALSE, kill_degenerate_faces);
#endif
}

#undef DO_V_INTERP

/**
 *			BM_split_edge
 *
 *	Splits an edge. v should be one of the vertices in e and
 *  defines the direction of the splitting operation for interpolation
 *  purposes.
 *
 *  Returns -
 *	the new vert
 */

BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **ne, float percent)
{
	BMVert *nv, *v2;
	BMFace **oldfaces = NULL;
	BMEdge *dummy;
	BLI_array_staticdeclare(oldfaces, 32);
	SmallHash hash;

	/* we need this for handling multire */
	if (!ne)
		ne = &dummy;

	/* do we have a multires layer */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS) && e->l) {
		BMLoop *l;
		int i;
		
		l = e->l;
		do {
			BLI_array_append(oldfaces, l->f);
			l = l->radial_next;
		} while (l != e->l);
		
		/* create a hash so we can differentiate oldfaces from new face */
		BLI_smallhash_init(&hash);
		
		for (i = 0; i < BLI_array_count(oldfaces); i++) {
			oldfaces[i] = BM_face_copy(bm, oldfaces[i], TRUE, TRUE);
			BLI_smallhash_insert(&hash, (intptr_t)oldfaces[i], NULL);
		}
	}

	v2 = bmesh_edge_getothervert(e, v);
	nv = bmesh_semv(bm, v, e, ne);
	if (nv == NULL) {
		return NULL;
	}

	sub_v3_v3v3(nv->co, v2->co, v->co);
	madd_v3_v3v3fl(nv->co, v->co, nv->co, percent);

	if (ne) {
		(*ne)->head.hflag = e->head.hflag;
		BM_elem_attrs_copy(bm, bm, e, *ne);
	}

	/* v->nv->v2 */
	BM_data_interp_face_vert_edge(bm, v2, v, nv, e, percent);
	BM_data_interp_from_verts(bm, v, v2, nv, percent);

	if (CustomData_has_layer(&bm->ldata, CD_MDISPS) && e->l && nv) {
		int i, j;

		/* interpolate new/changed loop data from copied old face */
		for (j = 0; j < 2; j++) {
			for (i = 0; i < BLI_array_count(oldfaces); i++) {
				BMEdge *e1 = j ? *ne : e;
				BMLoop *l, *l2;
				
				l = e1->l;

				if (UNLIKELY(!l)) {
					BMESH_ASSERT(0);
					break;
				}
				
				do {
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l->f)) {
						BMLoop *l2_first;

						l2 = l2_first = BM_FACE_FIRST_LOOP(l->f);
						do {
							BM_loop_interp_multires(bm, l2, oldfaces[i]);
						} while ((l2 = l2->next) != l2_first);
					}
					l = l->radial_next;
				} while (l != e1->l);
			}
		}
		
		/* destroy the old face */
		for (i = 0; i < BLI_array_count(oldfaces); i++) {
			BM_face_verts_kill(bm, oldfaces[i]);
		}
		
		/* fix boundaries a bit, doesn't work too well quite ye */
#if 0
		for (j = 0; j < 2; j++) {
			BMEdge *e1 = j ? *ne : e;
			BMLoop *l, *l2;
			
			l = e1->l;
			if (UNLIKELY(!l)) {
				BMESH_ASSERT(0);
				break;
			}
			
			do {
				BM_face_multires_bounds_smooth(bm, l->f);
				l = l->radial_next;
			} while (l != e1->l);
		}
#endif
		
		BLI_array_free(oldfaces);
		BLI_smallhash_release(&hash);
	}
	
	return nv;
}

BMVert  *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts)
{
	int i;
	float percent;
	BMVert *nv = NULL;
	
	for (i = 0; i < numcuts; i++) {
		percent = 1.0f / (float)(numcuts + 1 - i);
		nv = BM_edge_split(bm, e, e->v2, NULL, percent);
	}
	return nv;
}

int BM_face_validate(BMesh *bm, BMFace *face, FILE *err)
{
	BMIter iter;
	BLI_array_declare(verts);
	BMVert **verts = NULL;
	BMLoop *l;
	int ret = 1, i, j;
	
	if (face->len == 2) {
		fprintf(err, "warning: found two-edged face. face ptr: %p\n", face);
		fflush(err);
	}

	for (l = BM_iter_new(&iter, bm, BM_LOOPS_OF_FACE, face); l; l = BM_iter_step(&iter)) {
		BLI_array_growone(verts);
		verts[BLI_array_count(verts) - 1] = l->v;
		
		if (l->e->v1 == l->e->v2) {
			fprintf(err, "Found bmesh edge with identical verts!\n");
			fprintf(err, "  edge ptr: %p, vert: %p\n",  l->e, l->e->v1);
			fflush(err);
			ret = 0;
		}
	}

	for (i = 0; i < BLI_array_count(verts); i++) {
		for (j = 0; j < BLI_array_count(verts); j++) {
			if (j == i) {
				continue;
			}

			if (verts[i] == verts[j]) {
				fprintf(err, "Found duplicate verts in bmesh face!\n");
				fprintf(err, "  face ptr: %p, vert: %p\n", face, verts[i]);
				fflush(err);
				ret = 0;
			}
		}
	}
	
	BLI_array_free(verts);
	return ret;
}

/*
 *         BM Rotate Edge
 *
 * Spins an edge topologically, either counter-clockwise or clockwise.
 * If ccw is true, the edge is spun counter-clockwise, otherwise it is
 * spun clockwise.
 *
 * Returns the spun edge.  Note that this works by dissolving the edge
 * then re-creating it, so the returned edge won't have the same pointer
 * address as the original one.
 *
 * Returns NULL on error (e.g., if the edge isn't surrounded by exactly
 * two faces).
 */
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, int ccw)
{
	BMVert *v1, *v2;
	BMLoop *l, *l1, *l2, *nl;
	BMFace *f;
	BMIter liter;

	v1 = e->v1;
	v2 = e->v2;

	if (BM_edge_face_count(e) != 2)
		return NULL;

	/* If either of e's vertices has valence 2, then
	 * dissolving the edge would leave a spur, so not allowed */
	if (BM_vert_edge_count(e->v1) == 2 || BM_vert_edge_count(e->v2) == 2)
		return NULL;

	f = BM_faces_join_pair(bm, e->l->f, e->l->radial_next->f, e);

	if (f == NULL)
		return NULL;

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if (l->v == v1)
			l1 = l;
		else if (l->v == v2)
			l2 = l;
	}
	
	if (ccw) {
		l1 = l1->prev;
		l2 = l2->prev;
	}
	else {
		l1 = l1->next;
		l2 = l2->next;
	}

	if (!BM_face_split(bm, f, l1->v, l2->v, &nl, NULL))
		return NULL;

	return nl->e;
}

BMVert *BM_vert_rip(BMesh *bm, BMFace *sf, BMVert *sv)
{
	return bmesh_urmv(bm, sf, sv);
}
