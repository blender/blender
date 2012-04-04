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
#include "intern/bmesh_private.h"

/**
 * \brief Dissolve Vert
 *
 * Turns the face region surrounding a manifold vertex into a single polygon.
 *
 * \par Example:
 *
 *              +---------+             +---------+
 *              |  \   /  |             |         |
 *     Before:  |    v    |      After: |         |
 *              |  /   \  |             |         |
 *              +---------+             +---------+
 *
 *
 * This function can also collapse edges too
 * in cases when it cant merge into faces.
 *
 * \par Example:
 *
 *     Before:  +----v----+      After: +---------+
 *
 * \note dissolves vert, in more situations then BM_disk_dissolve
 * (e.g. if the vert is part of a wire edge, etc).
 */
int BM_vert_dissolve(BMesh *bm, BMVert *v)
{
	const int len = BM_vert_edge_count(v);
	
	if (len == 1) {
		BM_vert_kill(bm, v); /* will kill edges too */
		return TRUE;
	}
	else if (!BM_vert_is_manifold(v)) {
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
		/* boundary vertex on a face */
		return (BM_vert_collapse_edge(bm, v->e, v, TRUE) != NULL);
	}
	else {
		return BM_disk_dissolve(bm, v);
	}
}

/**
 * dissolves all faces around a vert, and removes it.
 */
int BM_disk_dissolve(BMesh *bm, BMVert *v)
{
	BMFace *f, *f2;
	BMEdge *e, *keepedge = NULL, *baseedge = NULL;
	int len = 0;

	if (!BM_vert_is_manifold(v)) {
		return FALSE;
	}
	
	if (v->e) {
		/* v->e we keep, what else */
		e = v->e;
		do {
			e = bmesh_disk_edge_next(e, v);
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
		if (!BM_face_split(bm, loop->f, v, loop->v, NULL, NULL, FALSE))
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

		if (f != f2 && !BM_faces_join_pair(bm, f, f2, e, TRUE)) {
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
					f = BM_faces_join_pair(bm, e->l->f, e->l->radial_next->f, e, TRUE);
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
				e = bmesh_disk_edge_next(e, v);
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
			if (!BM_faces_join_pair(bm, f, f2, e, TRUE)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * \brief Faces Join Pair
 *
 * Joins two adjacent faces togather.
 *
 * Because this method calls to #BM_faces_join to do its work, if a pair
 * of faces share multiple edges, the pair of faces will be joined at
 * every edge (not just edge \a e). This part of the functionality might need
 * to be reconsidered.
 *
 * If the windings do not match the winding of the new face will follow
 * \a f1's winding (i.e. \a f2 will be reversed before the join).
 *
 * \return pointer to the combined face
 */
BMFace *BM_faces_join_pair(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e, const short do_del)
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

	f1 = BM_faces_join(bm, faces, 2, do_del);
	
	return f1;
}

/**
 * \brief Connect Verts, Split Face
 *
 * connects two verts together, automatically (if very naively) finding the
 * face they both share (if there is one) and splittling it.  Use this at your
 * own risk, as it doesn't handle the many complex cases it should (like zero-area faces,
 * multiple faces, etc).
 *
 * this is really only meant for cases where you don't know before hand the face
 * the two verts belong to for splitting (e.g. the subdivision operator).
 *
 * \return The newly created edge.
 */
BMEdge *BM_verts_connect(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **r_f)
{
	BMIter fiter;
	BMIter viter;
	BMVert *v_iter;
	BMFace *f_iter;

	/* be warned: this can do weird things in some ngon situation, see BM_face_legal_splits */
	BM_ITER(f_iter, &fiter, bm, BM_FACES_OF_VERT, v1) {
		BM_ITER(v_iter, &viter, bm, BM_FACES_OF_VERT, f_iter) {
			if (v_iter == v2) {
				BMLoop *nl;

				f_iter = BM_face_split(bm, f_iter, v1, v2, &nl, NULL, FALSE);

				if (r_f) {
					*r_f = f_iter;
				}
				return nl->e;
			}
		}
	}

	if (r_f) {
		*r_f = NULL;
	}
	return NULL;
}

/**
 * \brief Face Split
 *
 * Split a face along two vertices. returns the newly made face, and sets
 * the \a r_l member to a loop in the newly created edge.
 *
 * \param bm The bmesh
 * \param f the original face
 * \param v1, v2 vertices which define the split edge, must be different
 * \param r_l pointer which will receive the BMLoop for the split edge in the new face
 * \param example Edge used for attributes of splitting edge, if non-NULL
 * \param nodouble Use an existing edge if found
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original original face will be the
 * other side). NULL if the split fails.
 */
BMFace *BM_face_split(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, BMLoop **r_l,
                      BMEdge *example, const short nodouble)
{
	const int has_mdisp = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	BMFace *nf, *of;

	BLI_assert(v1 != v2);

	/* do we have a multires layer */
	if (has_mdisp) {
		of = BM_face_copy(bm, f, FALSE, FALSE);
	}
	
#ifdef USE_BMESH_HOLES
	nf = bmesh_sfme(bm, f, v1, v2, r_l, NULL, example, nodouble);
#else
	nf = bmesh_sfme(bm, f, v1, v2, r_l, example, nodouble);
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
 * \brief Face Split with intermediate points
 *
 * Like BM_face_split, but with an edge split by \a n intermediate points with given coordinates.
 *
 * \param bm The bmesh
 * \param f the original face
 * \param v1, v2 vertices which define the split edge, must be different
 * \param co Array of coordinates for intermediate points
 * \param n Length of \a cos (must be > 0)
 * \param r_l pointer which will receive the BMLoop for the first split edge (from \a v1) in the new face
 * \param example Edge used for attributes of splitting edge, if non-NULL
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original original face will be the
 * other side). NULL if the split fails.
 */
BMFace *BM_face_split_n(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2, float cos[][3], int n,
                        BMLoop **r_l, BMEdge *example)
{
	BMFace *nf, *of;
	BMLoop *l_dummy;
	BMEdge *e, *newe;
	BMVert *newv;
	int i, j;

	BLI_assert(v1 != v2);

	of = BM_face_copy(bm, f, TRUE, TRUE);

	if (!r_l)
		r_l = &l_dummy;
	
#ifdef USE_BMESH_HOLES
	nf = bmesh_sfme(bm, f, v1, v2, r_l, NULL, example, FALSE);
#else
	nf = bmesh_sfme(bm, f, v1, v2, r_l, example, FALSE);
#endif
	/* bmesh_sfme returns in r_l a Loop for nf going from v1 to v2.
	 * The radial_next is for f and goes from v2 to v1  */

	if (nf) {
		BM_elem_attrs_copy(bm, bm, f, nf);
		copy_v3_v3(nf->no, f->no);

		e = (*r_l)->e;
		for (i = 0; i < n; i++) {
			newv = bmesh_semv(bm, v2, e, &newe);
			BLI_assert(newv != NULL);
			/* bmesh_semv returns in newe the edge going from newv to tv */
			copy_v3_v3(newv->co, cos[i]);

			/* interpolate the loop data for the loops with v==newv, using orig face */
			for (j = 0; j < 2; j++) {
				BMEdge *e_iter = (j == 0) ? e : newe;
				BMLoop *l_iter = e_iter->l;
				do {
					if (l_iter->v == newv) {
						/* this interpolates both loop and vertex data */
						BM_loop_interp_from_face(bm, l_iter, of, TRUE, TRUE);
					}
				} while ((l_iter = l_iter->radial_next) != e_iter->l);
			}
			e = newe;
		}
	}

	BM_face_verts_kill(bm, of);

	return nf;
}

/**
 * \brief Vert Collapse Faces
 *
 * Collapses vertex \a kv that has only two manifold edges
 * onto a vertex it shares an edge with.
 * \a fac defines the amount of interpolation for Custom Data.
 *
 * \note that this is not a general edge collapse function.
 *
 * \note this function is very close to #BM_vert_collapse_edge,
 * both collapse a vertex and return a new edge.
 * Except this takes a factor and merges custom data.
 *
 *  BMESH_TODO:
 *    Insert error checking for KV valance.
 *
 * \param bm The bmesh
 * \param ke The edge to collapse
 * \param kv The vertex  to collapse into the edge
 * \param fac The factor along the edge
 * \param join_faces When true the faces around the vertex will be joined
 * otherwise collapse the vertex by merging the 2 edges this vert touches into one.
 * \param kill_degenerate_faces Removes faces with less than 3 verts after collapsing.
 *
 * \returns The New Edge
 */
BMEdge *BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac,
                               const short join_faces, const short kill_degenerate_faces)
{
	BMEdge *ne = NULL;
	BMVert *tv = bmesh_edge_other_vert_get(ke, kv);

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

	e2 = bmesh_disk_edge_next(ke, kv);
	tv2 = BM_edge_other_vert(e2, kv);

	if (join_faces) {
		BMFace **faces = NULL;
		BMFace *f;
		BLI_array_staticdeclare(faces, 8);

		BM_ITER(f, &iter, bm, BM_FACES_OF_VERT, kv) {
			BLI_array_append(faces, f);
		}

		if (BLI_array_count(faces) >= 2) {
			BMFace *f2 = BM_faces_join(bm, faces, BLI_array_count(faces), TRUE);
			if (f2) {
				BMLoop *nl = NULL;
				if (BM_face_split(bm, f2, tv, tv2, &nl, NULL, FALSE)) {
					ne = nl->e;
				}
			}
		}

		BLI_array_free(faces);
	}
	else {
		/* single face or no faces */
		/* same as BM_vert_collapse_edge() however we already
		 * have vars to perform this operation so don't call. */
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
 * \brief Vert Collapse Faces
 *
 * Collapses a vertex onto another vertex it shares an edge with.
 *
 * \return The New Edge
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
	 * rather just get rid of the vertex - see bug [#28645] */
	BMVert *tv  = bmesh_edge_other_vert_get(ke, kv);
	if (tv) {
		BMEdge *e2 = bmesh_disk_edge_next(ke, kv);
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
 * \brief Edge Split
 *
 * Splits an edge. \a v should be one of the vertices in \a e and defines
 * the "from" end of the splitting operation: the new vertex will be
 * \a percent of the way from \a v to the other end.
 * The newly created edge is attached to \a v and is returned in \a r_e.
 * The original edge \a e will be the other half of the split.
 *
 * \return The new vert
 */
BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float percent)
{
	BMVert *nv, *v2;
	BMFace **oldfaces = NULL;
	BMEdge *e_dummy;
	BLI_array_staticdeclare(oldfaces, 32);
	SmallHash hash;
	const int do_mdisp = (e->l && CustomData_has_layer(&bm->ldata, CD_MDISPS));

	/* we need this for handling multire */
	if (!r_e) {
		r_e = &e_dummy;
	}

	/* do we have a multires layer */
	if (do_mdisp) {
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

	v2 = bmesh_edge_other_vert_get(e, v);
	nv = bmesh_semv(bm, v, e, r_e);

	BLI_assert(nv != NULL);

	sub_v3_v3v3(nv->co, v2->co, v->co);
	madd_v3_v3v3fl(nv->co, v->co, nv->co, percent);

	if (r_e) {
		(*r_e)->head.hflag = e->head.hflag;
		BM_elem_attrs_copy(bm, bm, e, *r_e);
	}

	/* v->nv->v2 */
	BM_data_interp_face_vert_edge(bm, v2, v, nv, e, percent);
	BM_data_interp_from_verts(bm, v, v2, nv, percent);

	if (do_mdisp) {
		int i, j;

		/* interpolate new/changed loop data from copied old face */
		for (j = 0; j < 2; j++) {
			for (i = 0; i < BLI_array_count(oldfaces); i++) {
				BMEdge *e1 = j ? *r_e : e;
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
			BMEdge *e1 = j ? *r_e : e;
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

/**
 * \brief Split an edge multiple times evenly
 */
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

/**
 * Checks if a face is valid in the data structure
 */
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


/**
 * Calculate the 2 loops which _would_ make up the newly rotated Edge
 * but don't actually change anything.
 *
 * Use this to further inspect if the loops to be connected have issues:
 *
 * Examples:
 * - the newly formed edge already exists
 * - the new face would be degenerate (zero area / concave /  bow-tie)
 * - may want to measure if the new edge gives improved results topology.
 *   over the old one, as with beauty fill.
 *
 * \note #BM_edge_rotate_check must have already run.
 */
void BM_edge_rotate_calc(BMesh *bm, BMEdge *e, int ccw,
                         BMLoop **r_l1, BMLoop **r_l2)
{
	BMVert *v1, *v2;
	BMFace *fa, *fb;

	/* this should have already run */
	BLI_assert(BM_edge_rotate_check(bm, e) == TRUE);

	/* we know this will work */
	BM_edge_face_pair(e, &fa, &fb);

	/* so we can use ccw variable correctly,
	 * otherwise we could use the egdes verts direct */
	BM_edge_ordered_verts(e, &v1, &v2);

	/* we could swap the verts _or_ the faces, swapping faces
	 * gives more predictable results since that way the next vert
	 * just stitches from face fa / fb */
	if (ccw) {
		SWAP(BMFace *, fa, fb);
	}

	*r_l1 = BM_face_other_vert_loop(fb, v2, v1);
	*r_l2 = BM_face_other_vert_loop(fa, v1, v2);

	/* when assert isn't used */
	(void)bm;
}

/**
 * \brief Check if Rotate Edge is OK
 *
 * Quick check to see if we could rotate the edge,
 * use this to avoid calling exceptions on common cases.
 */
int BM_edge_rotate_check(BMesh *UNUSED(bm), BMEdge *e)
{
	BMFace *fa, *fb;
	if (BM_edge_face_pair(e, &fa, &fb)) {
		BMLoop *la, *lb;

		la = BM_face_other_vert_loop(fa, e->v2, e->v1);
		lb = BM_face_other_vert_loop(fb, e->v2, e->v1);

		/* check that the next vert in both faces isn't the same
		 * (ie - the next edge doesnt share the same faces).
		 * since we can't rotate usefully in this case. */
		if (la->v == lb->v) {
			return FALSE;
		}

		/* mirror of the check above but in the opposite direction */
		la = BM_face_other_vert_loop(fa, e->v1, e->v2);
		lb = BM_face_other_vert_loop(fb, e->v1, e->v2);

		if (la->v == lb->v) {
			return FALSE;
		}

		return TRUE;
	}
	else {
		return FALSE;
	}
}

/**
 * \brief Check if Edge Rotate Gives Degenerate Faces
 *
 * Check 2 cases
 * 1) does the newly forms edge form a flipped face (compare with previous cross product)
 * 2) does the newly formed edge cause a zero area corner (or close enough to be almost zero)
 *
 * \param l1,l2 are the loops of the proposed verts to rotate too and should
 * be the result of calling #BM_edge_rotate_calc
 */
int BM_edge_rotate_check_degenerate(BMesh *bm, BMEdge *e,
                                    BMLoop *l1, BMLoop *l2)
{
	/* note: for these vars 'old' just means initial edge state. */

	float ed_dir_old[3]; /* edge vector */
	float ed_dir_new[3]; /* edge vector */
	float ed_dir_new_flip[3]; /* edge vector */

	float ed_dir_v1_old[3];
	float ed_dir_v2_old[3];

	float ed_dir_v1_new[3];
	float ed_dir_v2_new[3];

	float cross_old[3];
	float cross_new[3];

	/* original verts - these will be in the edge 'e' */
	BMVert *v1_old, *v2_old;

	/* verts from the loops passed */

	BMVert *v1, *v2;
	/* these are the opposite verts - the verts that _would_ be used if 'ccw' was inverted*/
	BMVert *v1_alt, *v2_alt;

	/* this should have already run */
	BLI_assert(BM_edge_rotate_check(bm, e) == TRUE);

	BM_edge_ordered_verts(e, &v1_old, &v2_old);

	v1 = l1->v;
	v2 = l2->v;

	/* get the next vert along */
	v1_alt = BM_face_other_vert_loop(l1->f, v1_old, v1)->v;
	v2_alt = BM_face_other_vert_loop(l2->f, v2_old, v2)->v;

	/* normalize all so comparisons are scale independent */

	BLI_assert(BM_edge_exists(v1_old, v1));
	BLI_assert(BM_edge_exists(v1, v1_alt));

	BLI_assert(BM_edge_exists(v2_old, v2));
	BLI_assert(BM_edge_exists(v2, v2_alt));

	/* old and new edge vecs */
	sub_v3_v3v3(ed_dir_old, v1_old->co, v2_old->co);
	sub_v3_v3v3(ed_dir_new, v1->co, v2->co);
	normalize_v3(ed_dir_old);
	normalize_v3(ed_dir_new);

	/* old edge corner vecs */
	sub_v3_v3v3(ed_dir_v1_old, v1_old->co, v1->co);
	sub_v3_v3v3(ed_dir_v2_old, v2_old->co, v2->co);
	normalize_v3(ed_dir_v1_old);
	normalize_v3(ed_dir_v2_old);

	/* old edge corner vecs */
	sub_v3_v3v3(ed_dir_v1_new, v1->co, v1_alt->co);
	sub_v3_v3v3(ed_dir_v2_new, v2->co, v2_alt->co);
	normalize_v3(ed_dir_v1_new);
	normalize_v3(ed_dir_v2_new);

	/* compare */
	cross_v3_v3v3(cross_old, ed_dir_old, ed_dir_v1_old);
	cross_v3_v3v3(cross_new, ed_dir_new, ed_dir_v1_new);
	if (dot_v3v3(cross_old, cross_new) < 0.0f) { /* does this flip? */
		return FALSE;
	}
	cross_v3_v3v3(cross_old, ed_dir_old, ed_dir_v2_old);
	cross_v3_v3v3(cross_new, ed_dir_new, ed_dir_v2_new);
	if (dot_v3v3(cross_old, cross_new) < 0.0f) { /* does this flip? */
		return FALSE;
	}

	negate_v3_v3(ed_dir_new_flip, ed_dir_new);

	/* result is zero area corner */
	if ((dot_v3v3(ed_dir_new,      ed_dir_v1_new) > 0.999f) ||
	    (dot_v3v3(ed_dir_new_flip, ed_dir_v2_new) > 0.999f))
	{
		return FALSE;
	}

	return TRUE;

	/* when assert isn't used */
	(void)bm;
}

int BM_edge_rotate_check_beauty(BMesh *UNUSED(bm), BMEdge *e,
                                BMLoop *l1, BMLoop *l2)
{
	/* Stupid check for now:
	 * Could compare angles of surrounding edges
	 * before & after, but this is OK.*/
	return (len_squared_v3v3(e->v1->co, e->v2->co) >
	        len_squared_v3v3(l1->v->co, l2->v->co));
}

/**
 * \brief Rotate Edge
 *
 * Spins an edge topologically,
 * either counter-clockwise or clockwise depending on \a ccw.
 *
 * \return The spun edge, NULL on error
 * (e.g., if the edge isn't surrounded by exactly two faces).
 *
 * \note This works by dissolving the edge then re-creating it,
 * so the returned edge won't have the same pointer address as the original one.
 *
 * \see header definition for \a check_flag enum.
 */
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, const short ccw, const short check_flag)
{
	BMVert *v1, *v2;
	BMLoop *l1, *l2;
	BMFace *f;
	BMEdge *e_new = NULL;
	char f_hflag_prev_1;
	char f_hflag_prev_2;

	if (!BM_edge_rotate_check(bm, e)) {
		return NULL;
	}

	BM_edge_rotate_calc(bm, e, ccw, &l1, &l2);

	/* the loops will be freed so assign verts */
	v1 = l1->v;
	v2 = l2->v;

	/* --------------------------------------- */
	/* Checking Code - make sure we can rotate */

	if (check_flag & BM_EDGEROT_CHECK_BEAUTY) {
		if (!BM_edge_rotate_check_beauty(bm, e, l1, l2)) {
			return NULL;
		}
	}

	/* check before applying */
	if (check_flag & BM_EDGEROT_CHECK_EXISTS) {
		if (BM_edge_exists(v1, v2)) {
			return NULL;
		}
	}

	/* slowest, check last */
	if (check_flag & BM_EDGEROT_CHECK_DEGENERATE) {
		if (!BM_edge_rotate_check_degenerate(bm, e, l1, l2)) {
			return NULL;
		}
	}
	/* Done Checking */
	/* ------------- */



	/* --------------- */
	/* Rotate The Edge */

	/* first create the new edge, this is so we can copy the customdata from the old one
	 * if splice if disabled, always add in a new edge even if theres one there. */
	e_new = BM_edge_create(bm, v1, v2, e, (check_flag & BM_EDGEROT_CHECK_SPLICE)!=0);

	f_hflag_prev_1 = l1->f->head.hflag;
	f_hflag_prev_2 = l2->f->head.hflag;

	/* don't delete the edge, manually remove the egde after so we can copy its attributes */
	f = BM_faces_join_pair(bm, l1->f, l2->f, NULL, TRUE);

	if (f == NULL) {
		return NULL;
	}

	/* note, this assumes joining the faces _didnt_ also remove the verts.
	 * the #BM_edge_rotate_check will ensure this, but its possibly corrupt state or future edits
	 * break this */
	if (!BM_face_split(bm, f, v1, v2, NULL, NULL, TRUE)) {
		return NULL;
	}
	else {
		/* we should really be able to know the faces some other way,
		 * rather then fetching them back from the edge, but this is predictable
		 * where using the return values from face split isn't. - campbell */
		BMFace *fa, *fb;
		if (BM_edge_face_pair(e_new, &fa, &fb)) {
			fa->head.hflag = f_hflag_prev_1;
			fb->head.hflag = f_hflag_prev_2;
		}
	}

	return e_new;
}

/**
 * \brief Rip a single face from a vertex fan
 */
BMVert *BM_face_vert_separate(BMesh *bm, BMFace *sf, BMVert *sv)
{
	return bmesh_urmv(bm, sf, sv);
}

/**
 * \brief Rip a single face from a vertex fan
 *
 * \note same as #BM_face_vert_separate but faster (avoids a loop lookup)
 */
BMVert *BM_face_loop_separate(BMesh *bm, BMLoop *sl)
{
	return bmesh_urmv_loop(bm, sl);
}
