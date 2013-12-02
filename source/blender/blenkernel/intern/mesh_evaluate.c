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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_evaluate.c
 *  \ingroup bke
 *
 * Functions to evaluate mesh data.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_memarena.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_bitmap.h"
#include "BLI_polyfill2d.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_alloca.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"

#include "BLI_strict_flags.h"


// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

/* -------------------------------------------------------------------- */

/** \name Mesh Normal Calculation
 * \{ */

/**
 * Call when there are no polygons.
 */
static void mesh_calc_normals_vert_fallback(MVert *mverts, int numVerts)
{
	int i;
	for (i = 0; i < numVerts; i++) {
		MVert *mv = &mverts[i];
		float no[3];

		normalize_v3_v3(no, mv->co);
		normal_float_to_short_v3(mv->no, no);
	}
}

/* Calculate vertex and face normals, face normals are returned in *faceNors_r if non-NULL
 * and vertex normals are stored in actual mverts.
 */
void BKE_mesh_calc_normals_mapping(MVert *mverts, int numVerts,
                                   MLoop *mloop, MPoly *mpolys, int numLoops, int numPolys, float (*polyNors_r)[3],
                                   MFace *mfaces, int numFaces, int *origIndexFace, float (*faceNors_r)[3])
{
	BKE_mesh_calc_normals_mapping_ex(mverts, numVerts, mloop, mpolys,
	                                 numLoops, numPolys, polyNors_r, mfaces, numFaces,
	                                 origIndexFace, faceNors_r, FALSE);
}
/* extended version of 'BKE_mesh_calc_normals_poly' with option not to calc vertex normals */
void BKE_mesh_calc_normals_mapping_ex(
        MVert *mverts, int numVerts,
        MLoop *mloop, MPoly *mpolys,
        int numLoops, int numPolys, float (*polyNors_r)[3],
        MFace *mfaces, int numFaces, int *origIndexFace, float (*faceNors_r)[3],
        const bool only_face_normals)
{
	float (*pnors)[3] = polyNors_r, (*fnors)[3] = faceNors_r;
	int i;
	MFace *mf;
	MPoly *mp;

	if (numPolys == 0) {
		if (only_face_normals == FALSE) {
			mesh_calc_normals_vert_fallback(mverts, numVerts);
		}
		return;
	}

	/* if we are not calculating verts and no verts were passes then we have nothing to do */
	if ((only_face_normals == TRUE) && (polyNors_r == NULL) && (faceNors_r == NULL)) {
		printf("%s: called with nothing to do\n", __func__);
		return;
	}

	if (!pnors) pnors = MEM_callocN(sizeof(float[3]) * (size_t)numPolys, __func__);
	/* if (!fnors) fnors = MEM_callocN(sizeof(float[3]) * numFaces, "face nors mesh.c"); */ /* NO NEED TO ALLOC YET */


	if (only_face_normals == FALSE) {
		/* vertex normals are optional, they require some extra calculations,
		 * so make them optional */
		BKE_mesh_calc_normals_poly(mverts, numVerts, mloop, mpolys, numLoops, numPolys, pnors, false);
	}
	else {
		/* only calc poly normals */
		mp = mpolys;
		for (i = 0; i < numPolys; i++, mp++) {
			BKE_mesh_calc_poly_normal(mp, mloop + mp->loopstart, mverts, pnors[i]);
		}
	}

	if (origIndexFace &&
	    /* fnors == faceNors_r */ /* NO NEED TO ALLOC YET */
	    fnors != NULL &&
	    numFaces)
	{
		mf = mfaces;
		for (i = 0; i < numFaces; i++, mf++, origIndexFace++) {
			if (*origIndexFace < numPolys) {
				copy_v3_v3(fnors[i], pnors[*origIndexFace]);
			}
			else {
				/* eek, we're not corresponding to polys */
				printf("error in %s: tessellation face indices are incorrect.  normals may look bad.\n", __func__);
			}
		}
	}

	if (pnors != polyNors_r) MEM_freeN(pnors);
	/* if (fnors != faceNors_r) MEM_freeN(fnors); */ /* NO NEED TO ALLOC YET */

	fnors = pnors = NULL;
	
}

static void mesh_calc_normals_poly_accum(MPoly *mp, MLoop *ml,
                                         MVert *mvert, float polyno[3], float (*tnorms)[3])
{
	const int nverts = mp->totloop;
	float (*edgevecbuf)[3] = BLI_array_alloca(edgevecbuf, (size_t)nverts);
	int i;

	/* Polygon Normal and edge-vector */
	/* inline version of #BKE_mesh_calc_poly_normal, also does edge-vectors */
	{
		int i_prev = nverts - 1;
		float const *v_prev = mvert[ml[i_prev].v].co;
		float const *v_curr;

		zero_v3(polyno);
		/* Newell's Method */
		for (i = 0; i < nverts; i++) {
			v_curr = mvert[ml[i].v].co;
			add_newell_cross_v3_v3v3(polyno, v_prev, v_curr);

			/* Unrelated to normalize, calculate edge-vector */
			sub_v3_v3v3(edgevecbuf[i_prev], v_prev, v_curr);
			normalize_v3(edgevecbuf[i_prev]);
			i_prev = i;

			v_prev = v_curr;
		}
		if (UNLIKELY(normalize_v3(polyno) == 0.0f)) {
			polyno[2] = 1.0f; /* other axis set to 0.0 */
		}
	}

	/* accumulate angle weighted face normal */
	/* inline version of #accumulate_vertex_normals_poly */
	{
		const float *prev_edge = edgevecbuf[nverts - 1];

		for (i = 0; i < nverts; i++) {
			const float *cur_edge = edgevecbuf[i];

			/* calculate angle between the two poly edges incident on
			 * this vertex */
			const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

			/* accumulate */
			madd_v3_v3fl(tnorms[ml[i].v], polyno, fac);
			prev_edge = cur_edge;
		}
	}

}

void BKE_mesh_calc_normals_poly(MVert *mverts, int numVerts, MLoop *mloop, MPoly *mpolys,
                                int UNUSED(numLoops), int numPolys, float (*r_polynors)[3],
                                const bool only_face_normals)
{
	float (*pnors)[3] = r_polynors;
	float (*tnorms)[3];
	int i;
	MPoly *mp;

	if (only_face_normals) {
		BLI_assert(pnors != NULL);

#pragma omp parallel for if (numPolys > BKE_MESH_OMP_LIMIT)
		for (i = 0; i < numPolys; i++) {
			BKE_mesh_calc_poly_normal(&mpolys[i], mloop + mpolys[i].loopstart, mverts, pnors[i]);
		}
		return;
	}

	/* first go through and calculate normals for all the polys */
	tnorms = MEM_callocN(sizeof(*tnorms) * (size_t)numVerts, __func__);

	if (pnors) {
		mp = mpolys;
		for (i = 0; i < numPolys; i++, mp++) {
			mesh_calc_normals_poly_accum(mp, mloop + mp->loopstart, mverts, pnors[i], tnorms);
		}
	}
	else {
		float tpnor[3];  /* temp poly normal */
		mp = mpolys;
		for (i = 0; i < numPolys; i++, mp++) {
			mesh_calc_normals_poly_accum(mp, mloop + mp->loopstart, mverts, tpnor, tnorms);
		}
	}

	/* following Mesh convention; we use vertex coordinate itself for normal in this case */
	for (i = 0; i < numVerts; i++) {
		MVert *mv = &mverts[i];
		float *no = tnorms[i];

		if (UNLIKELY(normalize_v3(no) == 0.0f)) {
			normalize_v3_v3(no, mv->co);
		}

		normal_float_to_short_v3(mv->no, no);
	}

	MEM_freeN(tnorms);
}

void BKE_mesh_calc_normals(Mesh *mesh)
{
#ifdef DEBUG_TIME
	TIMEIT_START(BKE_mesh_calc_normals);
#endif
	BKE_mesh_calc_normals_poly(mesh->mvert, mesh->totvert,
	                           mesh->mloop, mesh->mpoly, mesh->totloop, mesh->totpoly,
	                           NULL, false);
#ifdef DEBUG_TIME
	TIMEIT_END(BKE_mesh_calc_normals);
#endif
}

void BKE_mesh_calc_normals_tessface(MVert *mverts, int numVerts, MFace *mfaces, int numFaces, float (*faceNors_r)[3])
{
	float (*tnorms)[3] = MEM_callocN(sizeof(*tnorms) * (size_t)numVerts, "tnorms");
	float (*fnors)[3] = (faceNors_r) ? faceNors_r : MEM_callocN(sizeof(*fnors) * (size_t)numFaces, "meshnormals");
	int i;

	for (i = 0; i < numFaces; i++) {
		MFace *mf = &mfaces[i];
		float *f_no = fnors[i];
		float *n4 = (mf->v4) ? tnorms[mf->v4] : NULL;
		float *c4 = (mf->v4) ? mverts[mf->v4].co : NULL;

		if (mf->v4)
			normal_quad_v3(f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, mverts[mf->v4].co);
		else
			normal_tri_v3(f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co);

		accumulate_vertex_normals(tnorms[mf->v1], tnorms[mf->v2], tnorms[mf->v3], n4,
		                          f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, c4);
	}

	/* following Mesh convention; we use vertex coordinate itself for normal in this case */
	for (i = 0; i < numVerts; i++) {
		MVert *mv = &mverts[i];
		float *no = tnorms[i];
		
		if (UNLIKELY(normalize_v3(no) == 0.0f)) {
			normalize_v3_v3(no, mv->co);
		}

		normal_float_to_short_v3(mv->no, no);
	}
	
	MEM_freeN(tnorms);

	if (fnors != faceNors_r)
		MEM_freeN(fnors);
}

/**
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry (splitting edges).
 */
void BKE_mesh_normals_loop_split(MVert *mverts, const int UNUSED(numVerts), MEdge *medges, const int numEdges,
                                 MLoop *mloops, float (*r_loopnors)[3], const int numLoops,
                                 MPoly *mpolys, float (*polynors)[3], const int numPolys, float split_angle)
{
#define INDEX_UNSET INT_MIN
#define INDEX_INVALID -1
/* See comment about edge_to_loops below. */
#define IS_EDGE_SHARP(_e2l) (ELEM((_e2l)[1], INDEX_UNSET, INDEX_INVALID))

	/* Mapping edge -> loops.
	 * If that edge is used by more than two loops (polys), it is always sharp (and tagged as such, see below).
	 * We also use the second loop index as a kind of flag: smooth edge: > 0,
	 *                                                      sharp edge: < 0 (INDEX_INVALID || INDEX_UNSET),
	 *                                                      unset: INDEX_UNSET
	 * Note that currently we only have two values for second loop of sharp edges. However, if needed, we can
	 * store the negated value of loop index instead of INDEX_INVALID to retrieve the real value later in code).
	 * Note also that lose edges always have both values set to 0!
	 */
	int (*edge_to_loops)[2] = MEM_callocN(sizeof(int[2]) * (size_t)numEdges, __func__);

	/* Simple mapping from a loop to its polygon index. */
	int *loop_to_poly = MEM_mallocN(sizeof(int) * (size_t)numLoops, __func__);

	MPoly *mp;
	int mp_index;
	const bool check_angle = (split_angle < (float)M_PI);

	/* Temp normal stack. */
	BLI_SMALLSTACK_DECLARE(normal, float *);

#ifdef DEBUG_TIME
	TIMEIT_START(BKE_mesh_normals_loop_split);
#endif

	if (check_angle) {
		split_angle = cosf(split_angle);
	}

	/* This first loop check which edges are actually smooth, and compute edge vectors. */
	for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
		MLoop *ml_curr;
		int *e2l;
		int ml_curr_index = mp->loopstart;
		const int ml_last_index = (ml_curr_index + mp->totloop) - 1;

		ml_curr = &mloops[ml_curr_index];

		for (; ml_curr_index <= ml_last_index; ml_curr++, ml_curr_index++) {
			e2l = edge_to_loops[ml_curr->e];

			loop_to_poly[ml_curr_index] = mp_index;

			/* Pre-populate all loop normals as if their verts were all-smooth, this way we don't have to compute
			 * those later!
			 */
			normal_short_to_float_v3(r_loopnors[ml_curr_index], mverts[ml_curr->v].no);

			/* Check whether current edge might be smooth or sharp */
			if ((e2l[0] | e2l[1]) == 0) {
				/* 'Empty' edge until now, set e2l[0] (and e2l[1] to INDEX_UNSET to tag it as unset). */
				e2l[0] = ml_curr_index;
				/* We have to check this here too, else we might miss some flat faces!!! */
				e2l[1] = (mp->flag & ME_SMOOTH) ? INDEX_UNSET : INDEX_INVALID;
			}
			else if (e2l[1] == INDEX_UNSET) {
				/* Second loop using this edge, time to test its sharpness.
				 * An edge is sharp if it is tagged as such, or its face is not smooth, or angle between
				 * both its polys' normals is above split_angle value...
				 */
				if (!(mp->flag & ME_SMOOTH) || (medges[ml_curr->e].flag & ME_SHARP) ||
				    (check_angle && dot_v3v3(polynors[loop_to_poly[e2l[0]]], polynors[mp_index]) < split_angle))
				{
					/* Note: we are sure that loop != 0 here ;) */
					e2l[1] = INDEX_INVALID;
				}
				else {
					e2l[1] = ml_curr_index;
				}
			}
			else if (!IS_EDGE_SHARP(e2l)) {
				/* More than two loops using this edge, tag as sharp if not yet done. */
				e2l[1] = INDEX_INVALID;
			}
			/* Else, edge is already 'disqualified' (i.e. sharp)! */
		}
	}

	/* We now know edges that can be smoothed (with their vector, and their two loops), and edges that will be hard!
	 * Now, time to generate the normals.
	 */
	for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
		MLoop *ml_curr, *ml_prev;
		float (*lnors)[3];
		const int ml_last_index = (mp->loopstart + mp->totloop) - 1;
		int ml_curr_index = mp->loopstart;
		int ml_prev_index = ml_last_index;

		ml_curr = &mloops[ml_curr_index];
		ml_prev = &mloops[ml_prev_index];
		lnors = &r_loopnors[ml_curr_index];

		for (; ml_curr_index <= ml_last_index; ml_curr++, ml_curr_index++, lnors++) {
			const int *e2l_curr = edge_to_loops[ml_curr->e];
			const int *e2l_prev = edge_to_loops[ml_prev->e];

			if (!IS_EDGE_SHARP(e2l_curr)) {
				/* A smooth edge.
				 * We skip it because it is either:
				 * - in the middle of a 'smooth fan' already computed (or that will be as soon as we hit
				 *   one of its ends, i.e. one of its two sharp edges), or...
				 * - the related vertex is a "full smooth" one, in which case pre-populated normals from vertex
				 *   are just fine!
				 */
			}
			else if (IS_EDGE_SHARP(e2l_prev)) {
				/* Simple case (both edges around that vertex are sharp in current polygon),
				 * this vertex just takes its poly normal.
				 */
				copy_v3_v3(*lnors, polynors[mp_index]);
				/* No need to mark loop as done here, we won't run into it again anyway! */
			}
			/* This loop may have been already computed, in which case its 'to_poly' map is set to -1... */
			else if (loop_to_poly[ml_curr_index] != -1) {
				/* Gah... We have to fan around current vertex, until we find the other non-smooth edge,
				 * and accumulate face normals into the vertex!
				 * Note in case this vertex has only one sharp edges, this is a waste because the normal is the same as
				 * the vertex normal, but I do not see any easy way to detect that (would need to count number
				 * of sharp edges per vertex, I doubt the additional memory usage would be worth it, especially as
				 * it should not be a common case in real-life meshes anyway).
				 */
				const unsigned int mv_pivot_index = ml_curr->v;  /* The vertex we are "fanning" around! */
				const MVert *mv_pivot = &mverts[mv_pivot_index];
				const int *e2lfan_curr;
				float vec_curr[3], vec_prev[3];
				MLoop *mlfan_curr, *mlfan_next;
				MPoly *mpfan_next;
				float lnor[3] = {0.0f, 0.0f, 0.0f};
				/* mlfan_vert_index: the loop of our current edge might not be the loop of our current vertex! */
				int mlfan_curr_index, mlfan_vert_index, mpfan_curr_index;

				e2lfan_curr = e2l_prev;
				mlfan_curr = ml_prev;
				mlfan_curr_index = ml_prev_index;
				mlfan_vert_index = ml_curr_index;
				mpfan_curr_index = mp_index;

				/* Only need to compute previous edge's vector once, then we can just reuse old current one! */
				{
					const MEdge *me_prev = &medges[ml_curr->e];  /* ml_curr would be mlfan_prev if we needed that one */
					const MVert *mv_2 = (me_prev->v1 == mv_pivot_index) ? &mverts[me_prev->v2] : &mverts[me_prev->v1];

					sub_v3_v3v3(vec_prev, mv_2->co, mv_pivot->co);
					normalize_v3(vec_prev);
				}

				while (true) {
					/* Compute edge vectors.
					 * NOTE: We could pre-compute those into an array, in the first iteration, instead of computing them
					 *       twice (or more) here. However, time gained is not worth memory and time lost,
					 *       given the fact that this code should not be called that much in real-life meshes...
					 */
					{
						const MEdge *me_curr = &medges[mlfan_curr->e];
						const MVert *mv_2 = (me_curr->v1 == mv_pivot_index) ? &mverts[me_curr->v2] :
						                                                      &mverts[me_curr->v1];

						sub_v3_v3v3(vec_curr, mv_2->co, mv_pivot->co);
						normalize_v3(vec_curr);
					}

					{
						/* Code similar to accumulate_vertex_normals_poly. */
						/* Calculate angle between the two poly edges incident on this vertex. */
						const float fac = saacos(dot_v3v3(vec_curr, vec_prev));
						/* Accumulate */
						madd_v3_v3fl(lnor, polynors[mpfan_curr_index], fac);
					}

					/* We store here a pointer to all loop-normals processed. */
					BLI_SMALLSTACK_PUSH(normal, &(r_loopnors[mlfan_vert_index][0]));

					/* And we are done with this loop, mark it as such! */
					loop_to_poly[mlfan_vert_index] = -1;

					if (IS_EDGE_SHARP(e2lfan_curr)) {
						/* Current edge is sharp, we have finished with this fan of faces around this vert! */
						break;
					}

					copy_v3_v3(vec_prev, vec_curr);

					/* Warning! This is rather complex!
					 * We have to find our next edge around the vertex (fan mode).
					 * First we find the next loop, which is either previous or next to mlfan_curr_index, depending
					 * whether both loops using current edge are in the same direction or not, and whether
					 * mlfan_curr_index actually uses the vertex we are fanning around!
					 * mlfan_curr_index is the index of mlfan_next here, and mlfan_next is not the real next one
					 * (i.e. not the future mlfan_curr)...
					 */
					mlfan_curr_index = (e2lfan_curr[0] == mlfan_curr_index) ? e2lfan_curr[1] : e2lfan_curr[0];
					mpfan_curr_index = loop_to_poly[mlfan_curr_index];
					mlfan_next = &mloops[mlfan_curr_index];
					mpfan_next = &mpolys[mpfan_curr_index];
					if ((mlfan_curr->v == mlfan_next->v && mlfan_curr->v == mv_pivot_index) ||
					    (mlfan_curr->v != mlfan_next->v && mlfan_curr->v != mv_pivot_index))
					{
						/* We need the previous loop, but current one is our vertex's loop. */
						mlfan_vert_index = mlfan_curr_index;
						if (--mlfan_curr_index < mpfan_next->loopstart) {
							mlfan_curr_index = mpfan_next->loopstart + mpfan_next->totloop - 1;
						}
					}
					else {
						/* We need the next loop, which is also our vertex's loop. */
						if (++mlfan_curr_index >= mpfan_next->loopstart + mpfan_next->totloop) {
							mlfan_curr_index = mpfan_next->loopstart;
						}
						mlfan_vert_index = mlfan_curr_index;
					}
					mlfan_curr = &mloops[mlfan_curr_index];
					/* And now we are back in sync, mlfan_curr_index is the index of mlfan_curr! Pff! */

					e2lfan_curr = edge_to_loops[mlfan_curr->e];
				}

				/* In case we get a zero normal here, just use vertex normal already set! */
				if (LIKELY(normalize_v3(lnor) != 0.0f)) {
					/* Copy back the final computed normal into all related loop-normals. */
					float *nor;
					while ((nor = BLI_SMALLSTACK_POP(normal))) {
						copy_v3_v3(nor, lnor);
					}
				}
			}

			ml_prev = ml_curr;
			ml_prev_index = ml_curr_index;
		}
	}

	BLI_SMALLSTACK_FREE(normal);

	MEM_freeN(edge_to_loops);
	MEM_freeN(loop_to_poly);

#ifdef DEBUG_TIME
	TIMEIT_END(BKE_mesh_normals_loop_split);
#endif

#undef INDEX_UNSET
#undef INDEX_INVALID
#undef IS_EDGE_SHARP
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Polygon Calculations
 * \{ */

/*
 * COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 *
 */
static void mesh_calc_ngon_normal(MPoly *mpoly, MLoop *loopstart,
                                  MVert *mvert, float normal[3])
{
	const int nverts = mpoly->totloop;
	float const *v_prev = mvert[loopstart[nverts - 1].v].co;
	float const *v_curr;
	int i;

	zero_v3(normal);

	/* Newell's Method */
	for (i = 0; i < nverts; i++) {
		v_curr = mvert[loopstart[i].v].co;
		add_newell_cross_v3_v3v3(normal, v_prev, v_curr);
		v_prev = v_curr;
	}

	if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}
}

void BKE_mesh_calc_poly_normal(MPoly *mpoly, MLoop *loopstart,
                               MVert *mvarray, float no[3])
{
	if (mpoly->totloop > 4) {
		mesh_calc_ngon_normal(mpoly, loopstart, mvarray, no);
	}
	else if (mpoly->totloop == 3) {
		normal_tri_v3(no,
		              mvarray[loopstart[0].v].co,
		              mvarray[loopstart[1].v].co,
		              mvarray[loopstart[2].v].co
		              );
	}
	else if (mpoly->totloop == 4) {
		normal_quad_v3(no,
		               mvarray[loopstart[0].v].co,
		               mvarray[loopstart[1].v].co,
		               mvarray[loopstart[2].v].co,
		               mvarray[loopstart[3].v].co
		               );
	}
	else { /* horrible, two sided face! */
		no[0] = 0.0;
		no[1] = 0.0;
		no[2] = 1.0;
	}
}
/* duplicate of function above _but_ takes coords rather then mverts */
static void mesh_calc_ngon_normal_coords(MPoly *mpoly, MLoop *loopstart,
                                         const float (*vertex_coords)[3], float normal[3])
{
	const int nverts = mpoly->totloop;
	float const *v_prev = vertex_coords[loopstart[nverts - 1].v];
	float const *v_curr;
	int i;

	zero_v3(normal);

	/* Newell's Method */
	for (i = 0; i < nverts; i++) {
		v_curr = vertex_coords[loopstart[i].v];
		add_newell_cross_v3_v3v3(normal, v_prev, v_curr);
		v_prev = v_curr;
	}

	if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}
}

void BKE_mesh_calc_poly_normal_coords(MPoly *mpoly, MLoop *loopstart,
                                      const float (*vertex_coords)[3], float no[3])
{
	if (mpoly->totloop > 4) {
		mesh_calc_ngon_normal_coords(mpoly, loopstart, vertex_coords, no);
	}
	else if (mpoly->totloop == 3) {
		normal_tri_v3(no,
		              vertex_coords[loopstart[0].v],
		              vertex_coords[loopstart[1].v],
		              vertex_coords[loopstart[2].v]
		              );
	}
	else if (mpoly->totloop == 4) {
		normal_quad_v3(no,
		               vertex_coords[loopstart[0].v],
		               vertex_coords[loopstart[1].v],
		               vertex_coords[loopstart[2].v],
		               vertex_coords[loopstart[3].v]
		               );
	}
	else { /* horrible, two sided face! */
		no[0] = 0.0;
		no[1] = 0.0;
		no[2] = 1.0;
	}
}

static void mesh_calc_ngon_center(MPoly *mpoly, MLoop *loopstart,
                                  MVert *mvert, float cent[3])
{
	const float w = 1.0f / (float)mpoly->totloop;
	int i;

	zero_v3(cent);

	for (i = 0; i < mpoly->totloop; i++) {
		madd_v3_v3fl(cent, mvert[(loopstart++)->v].co, w);
	}
}

void BKE_mesh_calc_poly_center(MPoly *mpoly, MLoop *loopstart,
                               MVert *mvarray, float cent[3])
{
	if (mpoly->totloop == 3) {
		cent_tri_v3(cent,
		            mvarray[loopstart[0].v].co,
		            mvarray[loopstart[1].v].co,
		            mvarray[loopstart[2].v].co
		            );
	}
	else if (mpoly->totloop == 4) {
		cent_quad_v3(cent,
		             mvarray[loopstart[0].v].co,
		             mvarray[loopstart[1].v].co,
		             mvarray[loopstart[2].v].co,
		             mvarray[loopstart[3].v].co
		             );
	}
	else {
		mesh_calc_ngon_center(mpoly, loopstart, mvarray, cent);
	}
}

/* note, passing polynormal is only a speedup so we can skip calculating it */
float BKE_mesh_calc_poly_area(MPoly *mpoly, MLoop *loopstart,
                              MVert *mvarray, const float polynormal[3])
{
	if (mpoly->totloop == 3) {
		return area_tri_v3(mvarray[loopstart[0].v].co,
		                   mvarray[loopstart[1].v].co,
		                   mvarray[loopstart[2].v].co
		                   );
	}
	else if (mpoly->totloop == 4) {
		return area_quad_v3(mvarray[loopstart[0].v].co,
		                    mvarray[loopstart[1].v].co,
		                    mvarray[loopstart[2].v].co,
		                    mvarray[loopstart[3].v].co
		                    );
	}
	else {
		int i;
		MLoop *l_iter = loopstart;
		float area, polynorm_local[3];
		float (*vertexcos)[3] = BLI_array_alloca(vertexcos, (size_t)mpoly->totloop);
		const float *no = polynormal ? polynormal : polynorm_local;

		/* pack vertex cos into an array for area_poly_v3 */
		for (i = 0; i < mpoly->totloop; i++, l_iter++) {
			copy_v3_v3(vertexcos[i], mvarray[l_iter->v].co);
		}

		/* need normal for area_poly_v3 as well */
		if (polynormal == NULL) {
			BKE_mesh_calc_poly_normal(mpoly, loopstart, mvarray, polynorm_local);
		}

		/* finally calculate the area */
		area = area_poly_v3(mpoly->totloop, vertexcos, no);

		return area;
	}
}

/* note, results won't be correct if polygon is non-planar */
static float mesh_calc_poly_planar_area_centroid(MPoly *mpoly, MLoop *loopstart, MVert *mvarray, float cent[3])
{
	int i;
	float tri_area;
	float total_area = 0.0f;
	float v1[3], v2[3], v3[3], normal[3], tri_cent[3];

	BKE_mesh_calc_poly_normal(mpoly, loopstart, mvarray, normal);
	copy_v3_v3(v1, mvarray[loopstart[0].v].co);
	copy_v3_v3(v2, mvarray[loopstart[1].v].co);
	zero_v3(cent);

	for (i = 2; i < mpoly->totloop; i++) {
		copy_v3_v3(v3, mvarray[loopstart[i].v].co);

		tri_area = area_tri_signed_v3(v1, v2, v3, normal);
		total_area += tri_area;

		cent_tri_v3(tri_cent, v1, v2, v3);
		madd_v3_v3fl(cent, tri_cent, tri_area);

		copy_v3_v3(v2, v3);
	}

	mul_v3_fl(cent, 1.0f / total_area);

	return total_area;
}

#if 0 /* slow version of the function below */
void BKE_mesh_calc_poly_angles(MPoly *mpoly, MLoop *loopstart,
                               MVert *mvarray, float angles[])
{
	MLoop *ml;
	MLoop *mloop = &loopstart[-mpoly->loopstart];

	int j;
	for (j = 0, ml = loopstart; j < mpoly->totloop; j++, ml++) {
		MLoop *ml_prev = ME_POLY_LOOP_PREV(mloop, mpoly, j);
		MLoop *ml_next = ME_POLY_LOOP_NEXT(mloop, mpoly, j);

		float e1[3], e2[3];

		sub_v3_v3v3(e1, mvarray[ml_next->v].co, mvarray[ml->v].co);
		sub_v3_v3v3(e2, mvarray[ml_prev->v].co, mvarray[ml->v].co);

		angles[j] = (float)M_PI - angle_v3v3(e1, e2);
	}
}

#else /* equivalent the function above but avoid multiple subtractions + normalize */

void BKE_mesh_calc_poly_angles(MPoly *mpoly, MLoop *loopstart,
                               MVert *mvarray, float angles[])
{
	float nor_prev[3];
	float nor_next[3];

	int i_this = mpoly->totloop - 1;
	int i_next = 0;

	sub_v3_v3v3(nor_prev, mvarray[loopstart[i_this - 1].v].co, mvarray[loopstart[i_this].v].co);
	normalize_v3(nor_prev);

	while (i_next < mpoly->totloop) {
		sub_v3_v3v3(nor_next, mvarray[loopstart[i_this].v].co, mvarray[loopstart[i_next].v].co);
		normalize_v3(nor_next);
		angles[i_this] = angle_normalized_v3v3(nor_prev, nor_next);

		/* step */
		copy_v3_v3(nor_prev, nor_next);
		i_this = i_next;
		i_next++;
	}
}
#endif

void BKE_mesh_poly_edgehash_insert(EdgeHash *ehash, const MPoly *mp, const MLoop *mloop)
{
	const MLoop *ml, *ml_next;
	int i = mp->totloop;

	ml_next = mloop;       /* first loop */
	ml = &ml_next[i - 1];  /* last loop */

	while (i-- != 0) {
		BLI_edgehash_reinsert(ehash, ml->v, ml_next->v, NULL);

		ml = ml_next;
		ml_next++;
	}
}

void BKE_mesh_poly_edgebitmap_insert(unsigned int *edge_bitmap, const MPoly *mp, const MLoop *mloop)
{
	const MLoop *ml;
	int i = mp->totloop;

	ml = mloop;

	while (i-- != 0) {
		BLI_BITMAP_SET(edge_bitmap, ml->e);
		ml++;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Center Calculation
 * \{ */

bool BKE_mesh_center_median(Mesh *me, float cent[3])
{
	int i = me->totvert;
	MVert *mvert;
	zero_v3(cent);
	for (mvert = me->mvert; i--; mvert++) {
		add_v3_v3(cent, mvert->co);
	}
	/* otherwise we get NAN for 0 verts */
	if (me->totvert) {
		mul_v3_fl(cent, 1.0f / (float)me->totvert);
	}

	return (me->totvert != 0);
}

bool BKE_mesh_center_bounds(Mesh *me, float cent[3])
{
	float min[3], max[3];
	INIT_MINMAX(min, max);
	if (BKE_mesh_minmax(me, min, max)) {
		mid_v3_v3v3(cent, min, max);
		return true;
	}

	return false;
}

bool BKE_mesh_center_centroid(Mesh *me, float cent[3])
{
	int i = me->totpoly;
	MPoly *mpoly;
	float poly_area;
	float total_area = 0.0f;
	float poly_cent[3];

	zero_v3(cent);

	/* calculate a weighted average of polygon centroids */
	for (mpoly = me->mpoly; i--; mpoly++) {
		poly_area = mesh_calc_poly_planar_area_centroid(mpoly, me->mloop + mpoly->loopstart, me->mvert, poly_cent);

		madd_v3_v3fl(cent, poly_cent, poly_area);
		total_area += poly_area;
	}
	/* otherwise we get NAN for 0 polys */
	if (me->totpoly) {
		mul_v3_fl(cent, 1.0f / total_area);
	}

	/* zero area faces cause this, fallback to median */
	if (UNLIKELY(!is_finite_v3(cent))) {
		return BKE_mesh_center_median(me, cent);
	}

	return (me->totpoly != 0);
}
/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Connectivity Mapping
 * \{ */


/* ngon version wip, based on BM_uv_vert_map_create */
/* this replaces the non bmesh function (in trunk) which takes MTFace's, if we ever need it back we could
 * but for now this replaces it because its unused. */

UvVertMap *BKE_mesh_uv_vert_map_create(struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv,
                                       unsigned int totpoly, unsigned int totvert, int selected, float *limit)
{
	UvVertMap *vmap;
	UvMapVert *buf;
	MPoly *mp;
	unsigned int a;
	int i, totuv, nverts;

	totuv = 0;

	/* generate UvMapVert array */
	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++)
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL)))
			totuv += mp->totloop;

	if (totuv == 0)
		return NULL;

	vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
	if (!vmap)
		return NULL;

	vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totvert, "UvMapVert*");
	buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * (size_t)totuv, "UvMapVert");

	if (!vmap->vert || !vmap->buf) {
		BKE_mesh_uv_vert_map_free(vmap);
		return NULL;
	}

	mp = mpoly;
	for (a = 0; a < totpoly; a++, mp++) {
		if (!selected || (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL))) {
			nverts = mp->totloop;

			for (i = 0; i < nverts; i++) {
				buf->tfindex = (unsigned char)i;
				buf->f = a;
				buf->separate = 0;
				buf->next = vmap->vert[mloop[mp->loopstart + i].v];
				vmap->vert[mloop[mp->loopstart + i].v] = buf;
				buf++;
			}
		}
	}

	/* sort individual uvs for each vert */
	for (a = 0; a < totvert; a++) {
		UvMapVert *newvlist = NULL, *vlist = vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while (vlist) {
			v = vlist;
			vlist = vlist->next;
			v->next = newvlist;
			newvlist = v;

			uv = mloopuv[mpoly[v->f].loopstart + v->tfindex].uv;
			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;

				uv2 = mloopuv[mpoly[iterv->f].loopstart + iterv->tfindex].uv;
				sub_v2_v2v2(uvdiff, uv2, uv);


				if (fabsf(uv[0] - uv2[0]) < limit[0] && fabsf(uv[1] - uv2[1]) < limit[1]) {
					if (lastv) lastv->next = next;
					else vlist = next;
					iterv->next = newvlist;
					newvlist = iterv;
				}
				else
					lastv = iterv;

				iterv = next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a] = newvlist;
	}

	return vmap;
}

UvMapVert *BKE_mesh_uv_vert_map_get_vert(UvVertMap *vmap, unsigned int v)
{
	return vmap->vert[v];
}

void BKE_mesh_uv_vert_map_free(UvVertMap *vmap)
{
	if (vmap) {
		if (vmap->vert) MEM_freeN(vmap->vert);
		if (vmap->buf) MEM_freeN(vmap->buf);
		MEM_freeN(vmap);
	}
}

/* Generates a map where the key is the vertex and the value is a list
 * of polys that use that vertex as a corner. The lists are allocated
 * from one memory pool. */
void BKE_mesh_vert_poly_map_create(MeshElemMap **r_map, int **r_mem,
                                   const MPoly *mpoly, const MLoop *mloop,
                                   int totvert, int totpoly, int totloop)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, "vert poly map");
	int *indices, *index_iter;
	int i, j;

	indices = index_iter = MEM_mallocN(sizeof(int) * (size_t)totloop, "vert poly map mem");

	/* Count number of polys for each vertex */
	for (i = 0; i < totpoly; i++) {
		const MPoly *p = &mpoly[i];

		for (j = 0; j < p->totloop; j++)
			map[mloop[p->loopstart + j].v].count++;
	}

	/* Assign indices mem */
	for (i = 0; i < totvert; i++) {
		map[i].indices = index_iter;
		index_iter += map[i].count;

		/* Reset 'count' for use as index in last loop */
		map[i].count = 0;
	}

	/* Find the users */
	for (i = 0; i < totpoly; i++) {
		const MPoly *p = &mpoly[i];

		for (j = 0; j < p->totloop; j++) {
			unsigned int v = mloop[p->loopstart + j].v;

			map[v].indices[map[v].count] = i;
			map[v].count++;
		}
	}

	*r_map = map;
	*r_mem = indices;
}

/* Generates a map where the key is the vertex and the value is a list
 * of edges that use that vertex as an endpoint. The lists are allocated
 * from one memory pool. */
void BKE_mesh_vert_edge_map_create(MeshElemMap **r_map, int **r_mem,
                                   const MEdge *medge, int totvert, int totedge)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totvert, "vert-edge map");
	int *indices = MEM_mallocN(sizeof(int[2]) * (size_t)totedge, "vert-edge map mem");
	int *i_pt = indices;

	int i;

	/* Count number of edges for each vertex */
	for (i = 0; i < totedge; i++) {
		map[medge[i].v1].count++;
		map[medge[i].v2].count++;
	}

	/* Assign indices mem */
	for (i = 0; i < totvert; i++) {
		map[i].indices = i_pt;
		i_pt += map[i].count;

		/* Reset 'count' for use as index in last loop */
		map[i].count = 0;
	}

	/* Find the users */
	for (i = 0; i < totedge; i++) {
		const unsigned int v[2] = {medge[i].v1, medge[i].v2};

		map[v[0]].indices[map[v[0]].count] = i;
		map[v[1]].indices[map[v[1]].count] = i;

		map[v[0]].count++;
		map[v[1]].count++;
	}

	*r_map = map;
	*r_mem = indices;
}

void BKE_mesh_edge_poly_map_create(MeshElemMap **r_map, int **r_mem,
                                   const MEdge *UNUSED(medge), const int totedge,
                                   const MPoly *mpoly, const int totpoly,
                                   const MLoop *mloop, const int totloop)
{
	MeshElemMap *map = MEM_callocN(sizeof(MeshElemMap) * (size_t)totedge, "edge-poly map");
	int *indices = MEM_mallocN(sizeof(int) * (size_t)totloop, "edge-poly map mem");
	int *index_step;
	const MPoly *mp;
	int i;

	/* count face users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		int j = mp->totloop;
		for (ml = &mloop[mp->loopstart]; j--; ml++) {
			map[ml->e].count++;
		}
	}

	/* create offsets */
	index_step = indices;
	for (i = 0; i < totedge; i++) {
		map[i].indices = index_step;
		index_step += map[i].count;

		/* re-count, using this as an index below */
		map[i].count = 0;

	}

	/* assign poly-edge users */
	for (i = 0, mp = mpoly; i < totpoly; mp++, i++) {
		const MLoop *ml;
		int j = mp->totloop;
		for (ml = &mloop[mp->loopstart]; j--; ml++) {
			MeshElemMap *map_ele = &map[ml->e];
			map_ele->indices[map_ele->count++] = i;
		}
	}

	*r_map = map;
	*r_mem = indices;
}


/** \} */


/* -------------------------------------------------------------------- */

/** \name NGon Tessellation (NGon/Tessface Conversion)
 * \{ */

/**
 * Convert a triangle or quadrangle of loop/poly data to tessface data
 */
void BKE_mesh_loops_to_mface_corners(
        CustomData *fdata, CustomData *ldata,
        CustomData *pdata, int lindex[4], int findex,
        const int polyindex,
        const int mf_len, /* 3 or 4 */

        /* cache values to avoid lookups every time */
        const int numTex, /* CustomData_number_of_layers(pdata, CD_MTEXPOLY) */
        const int numCol, /* CustomData_number_of_layers(ldata, CD_MLOOPCOL) */
        const bool hasPCol, /* CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL) */
        const bool hasOrigSpace /* CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP) */
)
{
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for (i = 0; i < numTex; i++) {
		texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_get_n(pdata, CD_MTEXPOLY, polyindex, i);

		ME_MTEXFACE_CPY(texface, texpoly);

		for (j = 0; j < mf_len; j++) {
			mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, lindex[j], i);
			copy_v2_v2(texface->uv[j], mloopuv->uv);
		}
	}

	for (i = 0; i < numCol; i++) {
		mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

		for (j = 0; j < mf_len; j++) {
			mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, lindex[j], i);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}

	if (hasPCol) {
		mcol = CustomData_get(fdata,  findex, CD_PREVIEW_MCOL);

		for (j = 0; j < mf_len; j++) {
			mloopcol = CustomData_get(ldata, lindex[j], CD_PREVIEW_MLOOPCOL);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}

	if (hasOrigSpace) {
		OrigSpaceFace *of = CustomData_get(fdata, findex, CD_ORIGSPACE);
		OrigSpaceLoop *lof;

		for (j = 0; j < mf_len; j++) {
			lof = CustomData_get(ldata, lindex[j], CD_ORIGSPACE_MLOOP);
			copy_v2_v2(of->uv[j], lof->uv);
		}
	}
}

/**
 * Recreate tessellation.
 *
 * use_poly_origindex sets whether or not the tessellation faces' origindex
 * layer should point to original poly indices or real poly indices.
 *
 * use_face_origindex sets the tessellation faces' origindex layer
 * to point to the tessellation faces themselves, not the polys.
 *
 * if both of the above are 0, it'll use the indices of the mpolys of the MPoly
 * data in pdata, and ignore the origindex layer altogether.
 *
 * \return number of tessellation faces.
 */
int BKE_mesh_recalc_tessellation(CustomData *fdata,
                                 CustomData *ldata, CustomData *pdata,
                                 MVert *mvert, int totface, int totloop,
                                 int totpoly,
                                 /* when tessellating to recalculate normals after
                                  * we can skip copying here */
                                 const bool do_face_nor_cpy)
{
	/* use this to avoid locking pthread for _every_ polygon
	 * and calling the fill function */

#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS // NEEDS FURTHER TESTING

#define TESSFACE_SCANFILL (1 << 0)
#define TESSFACE_IS_QUAD  (1 << 1)

	const int looptris_tot = poly_to_tri_count(totpoly, totloop);

	MPoly *mp, *mpoly;
	MLoop *ml, *mloop;
	MFace *mface, *mf;
	MemArena *arena = NULL;
	int *mface_to_poly_map;
	int lindex[4]; /* only ever use 3 in this case */
	int poly_index, j, mface_index;

	const int numTex = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
	const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);

	mpoly = CustomData_get_layer(pdata, CD_MPOLY);
	mloop = CustomData_get_layer(ldata, CD_MLOOP);

	/* allocate the length of totfaces, avoid many small reallocs,
	 * if all faces are tri's it will be correct, quads == 2x allocs */
	/* take care. we are _not_ calloc'ing so be sure to initialize each field */
	mface_to_poly_map = MEM_mallocN(sizeof(*mface_to_poly_map) * (size_t)looptris_tot, __func__);
	mface             = MEM_mallocN(sizeof(*mface) *             (size_t)looptris_tot, __func__);

	mface_index = 0;
	mp = mpoly;
	for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
		const unsigned int mp_loopstart = (unsigned int)mp->loopstart;
		if (mp->totloop < 3) {
			/* do nothing */
		}

#ifdef USE_TESSFACE_SPEEDUP

#define ML_TO_MF(i1, i2, i3)                                                  \
		mface_to_poly_map[mface_index] = poly_index;                          \
		mf = &mface[mface_index];                                             \
		/* set loop indices, transformed to vert indices later */             \
		mf->v1 = mp_loopstart + i1;                                          \
		mf->v2 = mp_loopstart + i2;                                          \
		mf->v3 = mp_loopstart + i3;                                          \
		mf->v4 = 0;                                                           \
		mf->mat_nr = mp->mat_nr;                                              \
		mf->flag = mp->flag;                                                  \
		mf->edcode = 0;                                                       \
		(void)0

/* ALMOST IDENTICAL TO DEFINE ABOVE (see EXCEPTION) */
#define ML_TO_MF_QUAD()                                                       \
		mface_to_poly_map[mface_index] = poly_index;                          \
		mf = &mface[mface_index];                                             \
		/* set loop indices, transformed to vert indices later */             \
		mf->v1 = mp_loopstart + 0; /* EXCEPTION */                           \
		mf->v2 = mp_loopstart + 1; /* EXCEPTION */                           \
		mf->v3 = mp_loopstart + 2; /* EXCEPTION */                           \
		mf->v4 = mp_loopstart + 3; /* EXCEPTION */                           \
		mf->mat_nr = mp->mat_nr;                                              \
		mf->flag = mp->flag;                                                  \
		mf->edcode = TESSFACE_IS_QUAD; /* EXCEPTION */                        \
		(void)0


		else if (mp->totloop == 3) {
			ML_TO_MF(0, 1, 2);
			mface_index++;
		}
		else if (mp->totloop == 4) {
#ifdef USE_TESSFACE_QUADS
			ML_TO_MF_QUAD();
			mface_index++;
#else
			ML_TO_MF(0, 1, 2);
			mface_index++;
			ML_TO_MF(0, 2, 3);
			mface_index++;
#endif
		}
#endif /* USE_TESSFACE_SPEEDUP */
		else {
			const float *co_curr, *co_prev;

			float normal[3];

			float axis_mat[3][3];
			float (*projverts)[2];
			unsigned int (*tris)[3];

			const unsigned int loopstart = (unsigned int)mp->loopstart;
			const int totfilltri = mp->totloop - 2;

			if (UNLIKELY(arena == NULL)) {
				arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
			}

			tris = BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
			projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp->totloop);

			zero_v3(normal);

			/* calc normal */
			ml = mloop + loopstart;
			co_prev = mvert[ml[mp->totloop - 1].v].co;
			for (j = 0; j < mp->totloop; j++, ml++) {
				co_curr = mvert[ml->v].co;
				add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
				co_prev = co_curr;
			}
			if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
				normal[2] = 1.0f;
			}

			/* project verts to 2d */
			axis_dominant_v3_to_m3(axis_mat, normal);

			ml = mloop + loopstart;
			for (j = 0; j < mp->totloop; j++, ml++) {
				mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
			}

			BLI_polyfill_calc_arena((const float (*)[2])projverts, (unsigned int)mp->totloop, tris, arena);

			/* apply fill */
			ml = mloop + loopstart;
			for (j = 0; j < totfilltri; j++) {
				unsigned int *tri = tris[j];

				mface_to_poly_map[mface_index] = poly_index;
				mf = &mface[mface_index];

				/* set loop indices, transformed to vert indices later */
				mf->v1 = loopstart + tri[0];
				mf->v2 = loopstart + tri[1];
				mf->v3 = loopstart + tri[2];
				mf->v4 = 0;

				mf->mat_nr = mp->mat_nr;
				mf->flag = mp->flag;

#ifdef USE_TESSFACE_SPEEDUP
				mf->edcode = TESSFACE_SCANFILL; /* tag for sorting loop indices */
#endif

				mface_index++;
			}

			BLI_memarena_clear(arena);
		}
	}

	if (arena) {
		BLI_memarena_free(arena);
		arena = NULL;
	}

	CustomData_free(fdata, totface);
	totface = mface_index;

	BLI_assert(totface <= looptris_tot);

	/* not essential but without this we store over-alloc'd memory in the CustomData layers */
	if (LIKELY(looptris_tot != totface)) {
		mface = MEM_reallocN(mface, sizeof(*mface) * (size_t)totface);
		mface_to_poly_map = MEM_reallocN(mface_to_poly_map, sizeof(*mface_to_poly_map) * (size_t)totface);
	}

	CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

	/* CD_ORIGINDEX will contain an array of indices from tessfaces to the polygons
	 * they are directly tessellated from */
	CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
	CustomData_from_bmeshpoly(fdata, pdata, ldata, totface);

	if (do_face_nor_cpy) {
		/* If polys have a normals layer, copying that to faces can help
		 * avoid the need to recalculate normals later */
		if (CustomData_has_layer(pdata, CD_NORMAL)) {
			float (*pnors)[3] = CustomData_get_layer(pdata, CD_NORMAL);
			float (*fnors)[3] = CustomData_add_layer(fdata, CD_NORMAL, CD_CALLOC, NULL, totface);
			for (mface_index = 0; mface_index < totface; mface_index++) {
				copy_v3_v3(fnors[mface_index], pnors[mface_to_poly_map[mface_index]]);
			}
		}
	}

	mf = mface;
	for (mface_index = 0; mface_index < totface; mface_index++, mf++) {

#ifdef USE_TESSFACE_QUADS
		const int mf_len = mf->edcode & TESSFACE_IS_QUAD ? 4 : 3;
#endif

#ifdef USE_TESSFACE_SPEEDUP
		/* skip sorting when not using ngons */
		if (UNLIKELY(mf->edcode & TESSFACE_SCANFILL))
#endif
		{
			/* sort loop indices to ensure winding is correct */
			if (mf->v1 > mf->v2) SWAP(unsigned int, mf->v1, mf->v2);
			if (mf->v2 > mf->v3) SWAP(unsigned int, mf->v2, mf->v3);
			if (mf->v1 > mf->v2) SWAP(unsigned int, mf->v1, mf->v2);

			if (mf->v1 > mf->v2) SWAP(unsigned int, mf->v1, mf->v2);
			if (mf->v2 > mf->v3) SWAP(unsigned int, mf->v2, mf->v3);
			if (mf->v1 > mf->v2) SWAP(unsigned int, mf->v1, mf->v2);
		}

		/* end abusing the edcode */
#if defined(USE_TESSFACE_QUADS) || defined(USE_TESSFACE_SPEEDUP)
		mf->edcode = 0;
#endif


		lindex[0] = (int)mf->v1;
		lindex[1] = (int)mf->v2;
		lindex[2] = (int)mf->v3;
#ifdef USE_TESSFACE_QUADS
		if (mf_len == 4) lindex[3] = (int)mf->v4;
#endif

		/*transform loop indices to vert indices*/
		mf->v1 = mloop[mf->v1].v;
		mf->v2 = mloop[mf->v2].v;
		mf->v3 = mloop[mf->v3].v;
#ifdef USE_TESSFACE_QUADS
		if (mf_len == 4) mf->v4 = mloop[mf->v4].v;
#endif

		BKE_mesh_loops_to_mface_corners(fdata, ldata, pdata,
		                                lindex, mface_index, mface_to_poly_map[mface_index],
#ifdef USE_TESSFACE_QUADS
		                                mf_len,
#else
		                                3,
#endif
		                                numTex, numCol, hasPCol, hasOrigSpace);


#ifdef USE_TESSFACE_QUADS
		test_index_face(mf, fdata, mface_index, mf_len);
#endif

	}

	return totface;

#undef USE_TESSFACE_SPEEDUP

}


#ifdef USE_BMESH_SAVE_AS_COMPAT

/**
 * This function recreates a tessellation.
 * returns number of tessellation faces.
 *
 * for forwards compat only quad->tri polys to mface, skip ngons.
 */
int BKE_mesh_mpoly_to_mface(struct CustomData *fdata, struct CustomData *ldata,
                            struct CustomData *pdata, int totface, int UNUSED(totloop), int totpoly)
{
	MLoop *mloop;

	int lindex[4];
	int i;
	int k;

	MPoly *mp, *mpoly;
	MFace *mface, *mf;

	const int numTex = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
	const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);

	/* over-alloc, ngons will be skipped */
	mface = MEM_mallocN(sizeof(*mface) * (size_t)totpoly, __func__);

	mpoly = CustomData_get_layer(pdata, CD_MPOLY);
	mloop = CustomData_get_layer(ldata, CD_MLOOP);

	mp = mpoly;
	k = 0;
	for (i = 0; i < totpoly; i++, mp++) {
		if (ELEM(mp->totloop, 3, 4)) {
			const unsigned int mp_loopstart = (unsigned int)mp->loopstart;
			mf = &mface[k];

			mf->mat_nr = mp->mat_nr;
			mf->flag = mp->flag;

			mf->v1 = mp_loopstart + 0;
			mf->v2 = mp_loopstart + 1;
			mf->v3 = mp_loopstart + 2;
			mf->v4 = (mp->totloop == 4) ? (mp_loopstart + 3) : 0;

			/* abuse edcode for temp storage and clear next loop */
			mf->edcode = (char)mp->totloop; /* only ever 3 or 4 */

			k++;
		}
	}

	CustomData_free(fdata, totface);

	totface = k;

	CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

	CustomData_from_bmeshpoly(fdata, pdata, ldata, totface);

	mp = mpoly;
	k = 0;
	for (i = 0; i < totpoly; i++, mp++) {
		if (ELEM(mp->totloop, 3, 4)) {
			mf = &mface[k];

			if (mf->edcode == 3) {
				/* sort loop indices to ensure winding is correct */
				/* NO SORT - looks like we can skip this */

				lindex[0] = (int)mf->v1;
				lindex[1] = (int)mf->v2;
				lindex[2] = (int)mf->v3;
				lindex[3] = 0; /* unused */

				/* transform loop indices to vert indices */
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;

				BKE_mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                                lindex, k, i, 3,
				                                numTex, numCol, hasPCol, hasOrigSpace);
				test_index_face(mf, fdata, k, 3);
			}
			else {
				/* sort loop indices to ensure winding is correct */
				/* NO SORT - looks like we can skip this */

				lindex[0] = (int)mf->v1;
				lindex[1] = (int)mf->v2;
				lindex[2] = (int)mf->v3;
				lindex[3] = (int)mf->v4;

				/* transform loop indices to vert indices */
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;
				mf->v4 = mloop[mf->v4].v;

				BKE_mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                                lindex, k, i, 4,
				                                numTex, numCol, hasPCol, hasOrigSpace);
				test_index_face(mf, fdata, k, 4);
			}

			mf->edcode = 0;

			k++;
		}
	}

	return k;
}
#endif /* USE_BMESH_SAVE_AS_COMPAT */


static void bm_corners_to_loops_ex(ID *id, CustomData *fdata, CustomData *ldata, CustomData *pdata,
                                   MFace *mface, int totloop, int findex, int loopstart, int numTex, int numCol)
{
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	MFace *mf;
	int i;

	mf = mface + findex;

	for (i = 0; i < numTex; i++) {
		texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_get_n(pdata, CD_MTEXPOLY, findex, i);

		ME_MTEXFACE_CPY(texpoly, texface);

		mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, loopstart, i);
		copy_v2_v2(mloopuv->uv, texface->uv[0]); mloopuv++;
		copy_v2_v2(mloopuv->uv, texface->uv[1]); mloopuv++;
		copy_v2_v2(mloopuv->uv, texface->uv[2]); mloopuv++;

		if (mf->v4) {
			copy_v2_v2(mloopuv->uv, texface->uv[3]); mloopuv++;
		}
	}

	for (i = 0; i < numCol; i++) {
		mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, loopstart, i);
		mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

		MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[0]); mloopcol++;
		MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[1]); mloopcol++;
		MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[2]); mloopcol++;
		if (mf->v4) {
			MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[3]); mloopcol++;
		}
	}

	if (CustomData_has_layer(fdata, CD_MDISPS)) {
		MDisps *ld = CustomData_get(ldata, loopstart, CD_MDISPS);
		MDisps *fd = CustomData_get(fdata, findex, CD_MDISPS);
		float (*disps)[3] = fd->disps;
		int tot = mf->v4 ? 4 : 3;
		int corners;

		if (CustomData_external_test(fdata, CD_MDISPS)) {
			if (id && fdata->external) {
				CustomData_external_add(ldata, id, CD_MDISPS,
				                        totloop, fdata->external->filename);
			}
		}

		corners = multires_mdisp_corners(fd);

		if (corners == 0) {
			/* Empty MDisp layers appear in at least one of the sintel.blend files.
			 * Not sure why this happens, but it seems fine to just ignore them here.
			 * If (corners == 0) for a non-empty layer though, something went wrong. */
			BLI_assert(fd->totdisp == 0);
		}
		else {
			const int side = (int)sqrtf((float)(fd->totdisp / corners));
			const int side_sq = side * side;
			const size_t disps_size = sizeof(float[3]) * (size_t)side_sq;

			for (i = 0; i < tot; i++, disps += side_sq, ld++) {
				ld->totdisp = side_sq;
				ld->level = (int)(logf((float)side - 1.0f) / (float)M_LN2) + 1;

				if (ld->disps)
					MEM_freeN(ld->disps);

				ld->disps = MEM_mallocN(disps_size, "converted loop mdisps");
				if (fd->disps) {
					memcpy(ld->disps, disps, disps_size);
				}
				else {
					memset(ld->disps, 0, disps_size);
				}
			}
		}
	}
}


void BKE_mesh_convert_mfaces_to_mpolys(Mesh *mesh)
{
	BKE_mesh_convert_mfaces_to_mpolys_ex(&mesh->id, &mesh->fdata, &mesh->ldata, &mesh->pdata,
	                                     mesh->totedge, mesh->totface, mesh->totloop, mesh->totpoly,
	                                     mesh->medge, mesh->mface,
	                                     &mesh->totloop, &mesh->totpoly, &mesh->mloop, &mesh->mpoly);

	BKE_mesh_update_customdata_pointers(mesh, true);
}

/* the same as BKE_mesh_convert_mfaces_to_mpolys but oriented to be used in do_versions from readfile.c
 * the difference is how active/render/clone/stencil indices are handled here
 *
 * normally thay're being set from pdata which totally makes sense for meshes which are already
 * converted to bmesh structures, but when loading older files indices shall be updated in other
 * way around, so newly added pdata and ldata would have this indices set based on fdata layer
 *
 * this is normally only needed when reading older files, in all other cases BKE_mesh_convert_mfaces_to_mpolys
 * shall be always used
 */
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(Mesh *mesh)
{
	BKE_mesh_convert_mfaces_to_mpolys_ex(&mesh->id, &mesh->fdata, &mesh->ldata, &mesh->pdata,
	                                     mesh->totedge, mesh->totface, mesh->totloop, mesh->totpoly,
	                                     mesh->medge, mesh->mface,
	                                     &mesh->totloop, &mesh->totpoly, &mesh->mloop, &mesh->mpoly);

	CustomData_bmesh_do_versions_update_active_layers(&mesh->fdata, &mesh->pdata, &mesh->ldata);

	BKE_mesh_update_customdata_pointers(mesh, true);
}

void BKE_mesh_convert_mfaces_to_mpolys_ex(ID *id, CustomData *fdata, CustomData *ldata, CustomData *pdata,
                                          int totedge_i, int totface_i, int totloop_i, int totpoly_i,
                                          MEdge *medge, MFace *mface,
                                          int *totloop_r, int *totpoly_r,
                                          MLoop **mloop_r, MPoly **mpoly_r)
{
	MFace *mf;
	MLoop *ml, *mloop;
	MPoly *mp, *mpoly;
	MEdge *me;
	EdgeHash *eh;
	int numTex, numCol;
	int i, j, totloop, totpoly, *polyindex;

	/* just in case some of these layers are filled in (can happen with python created meshes) */
	CustomData_free(ldata, totloop_i);
	CustomData_free(pdata, totpoly_i);

	totpoly = totface_i;
	mpoly = MEM_callocN(sizeof(MPoly) * (size_t)totpoly, "mpoly converted");
	CustomData_add_layer(pdata, CD_MPOLY, CD_ASSIGN, mpoly, totpoly);

	numTex = CustomData_number_of_layers(fdata, CD_MTFACE);
	numCol = CustomData_number_of_layers(fdata, CD_MCOL);

	totloop = 0;
	mf = mface;
	for (i = 0; i < totface_i; i++, mf++) {
		totloop += mf->v4 ? 4 : 3;
	}

	mloop = MEM_callocN(sizeof(MLoop) * (size_t)totloop, "mloop converted");

	CustomData_add_layer(ldata, CD_MLOOP, CD_ASSIGN, mloop, totloop);

	CustomData_to_bmeshpoly(fdata, pdata, ldata, totloop, totpoly);

	if (id) {
		/* ensure external data is transferred */
		CustomData_external_read(fdata, id, CD_MASK_MDISPS, totface_i);
	}

	eh = BLI_edgehash_new_ex(__func__, (unsigned int)totedge_i);

	/* build edge hash */
	me = medge;
	for (i = 0; i < totedge_i; i++, me++) {
		BLI_edgehash_insert(eh, me->v1, me->v2, SET_UINT_IN_POINTER(i));

		/* unrelated but avoid having the FGON flag enabled, so we can reuse it later for something else */
		me->flag &= ~ME_FGON;
	}

	polyindex = CustomData_get_layer(fdata, CD_ORIGINDEX);

	j = 0; /* current loop index */
	ml = mloop;
	mf = mface;
	mp = mpoly;
	for (i = 0; i < totface_i; i++, mf++, mp++) {
		mp->loopstart = j;

		mp->totloop = mf->v4 ? 4 : 3;

		mp->mat_nr = mf->mat_nr;
		mp->flag = mf->flag;

#       define ML(v1, v2) { \
			ml->v = mf->v1; \
			ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(eh, mf->v1, mf->v2)); \
			ml++; j++; \
		} (void)0

		ML(v1, v2);
		ML(v2, v3);
		if (mf->v4) {
			ML(v3, v4);
			ML(v4, v1);
		}
		else {
			ML(v3, v1);
		}

#       undef ML

		bm_corners_to_loops_ex(id, fdata, ldata, pdata, mface, totloop, i, mp->loopstart, numTex, numCol);

		if (polyindex) {
			*polyindex = i;
			polyindex++;
		}
	}

	/* note, we don't convert NGons at all, these are not even real ngons,
	 * they have their own UV's, colors etc - its more an editing feature. */

	BLI_edgehash_free(eh, NULL);

	*totpoly_r = totpoly;
	*totloop_r = totloop;
	*mpoly_r = mpoly;
	*mloop_r = mloop;
}
/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Flag Flushing
 * \{ */

/* update the hide flag for edges and faces from the corresponding
 * flag in verts */
void BKE_mesh_flush_hidden_from_verts_ex(const MVert *mvert,
                                         const MLoop *mloop,
                                         MEdge *medge, const int totedge,
                                         MPoly *mpoly, const int totpoly)
{
	int i, j;

	for (i = 0; i < totedge; i++) {
		MEdge *e = &medge[i];
		if (mvert[e->v1].flag & ME_HIDE ||
		    mvert[e->v2].flag & ME_HIDE)
		{
			e->flag |= ME_HIDE;
		}
		else {
			e->flag &= ~ME_HIDE;
		}
	}
	for (i = 0; i < totpoly; i++) {
		MPoly *p = &mpoly[i];
		p->flag &= (char)~ME_HIDE;
		for (j = 0; j < p->totloop; j++) {
			if (mvert[mloop[p->loopstart + j].v].flag & ME_HIDE)
				p->flag |= ME_HIDE;
		}
	}
}
void BKE_mesh_flush_hidden_from_verts(Mesh *me)
{
	BKE_mesh_flush_hidden_from_verts_ex(me->mvert, me->mloop,
	                                    me->medge, me->totedge,
	                                    me->mpoly, me->totpoly);
}

void BKE_mesh_flush_hidden_from_polys_ex(MVert *mvert,
                                         const MLoop *mloop,
                                         MEdge *medge, const int UNUSED(totedge),
                                         const MPoly *mpoly, const int totpoly)
{
	const MPoly *mp;
	int i;

	i = totpoly;
	for (mp = mpoly; i--; mp++) {
		if (mp->flag & ME_HIDE) {
			const MLoop *ml;
			int j;
			j = mp->totloop;
			for (ml = &mloop[mp->loopstart]; j--; ml++) {
				mvert[ml->v].flag |= ME_HIDE;
				medge[ml->e].flag |= ME_HIDE;
			}
		}
	}

	i = totpoly;
	for (mp = mpoly; i--; mp++) {
		if ((mp->flag & ME_HIDE) == 0) {
			const MLoop *ml;
			int j;
			j = mp->totloop;
			for (ml = &mloop[mp->loopstart]; j--; ml++) {
				mvert[ml->v].flag &= (char)~ME_HIDE;
				medge[ml->e].flag &= (char)~ME_HIDE;
			}
		}
	}
}
void BKE_mesh_flush_hidden_from_polys(Mesh *me)
{
	BKE_mesh_flush_hidden_from_polys_ex(me->mvert, me->mloop,
	                                    me->medge, me->totedge,
	                                    me->mpoly, me->totpoly);
}

/**
 * simple poly -> vert/edge selection.
 */
void BKE_mesh_flush_select_from_polys_ex(MVert *mvert,       const int totvert,
                                         const MLoop *mloop,
                                         MEdge *medge,       const int totedge,
                                         const MPoly *mpoly, const int totpoly)
{
	MVert *mv;
	MEdge *med;
	const MPoly *mp;
	int i;

	i = totvert;
	for (mv = mvert; i--; mv++) {
		mv->flag &= (char)~SELECT;
	}

	i = totedge;
	for (med = medge; i--; med++) {
		med->flag &= ~SELECT;
	}

	i = totpoly;
	for (mp = mpoly; i--; mp++) {
		/* assume if its selected its not hidden and none of its verts/edges are hidden
		 * (a common assumption)*/
		if (mp->flag & ME_FACE_SEL) {
			const MLoop *ml;
			int j;
			j = mp->totloop;
			for (ml = &mloop[mp->loopstart]; j--; ml++) {
				mvert[ml->v].flag |= SELECT;
				medge[ml->e].flag |= SELECT;
			}
		}
	}
}
void BKE_mesh_flush_select_from_polys(Mesh *me)
{
	BKE_mesh_flush_select_from_polys_ex(me->mvert, me->totvert,
	                                 me->mloop,
	                                 me->medge, me->totedge,
	                                 me->mpoly, me->totpoly);
}

void BKE_mesh_flush_select_from_verts_ex(const MVert *mvert, const int UNUSED(totvert),
                                         const MLoop *mloop,
                                         MEdge *medge,       const int totedge,
                                         MPoly *mpoly,       const int totpoly)
{
	MEdge *med;
	MPoly *mp;
	int i;

	/* edges */
	i = totedge;
	for (med = medge; i--; med++) {
		if ((med->flag & ME_HIDE) == 0) {
			if ((mvert[med->v1].flag & SELECT) && (mvert[med->v2].flag & SELECT)) {
				med->flag |= SELECT;
			}
			else {
				med->flag &= ~SELECT;
			}
		}
	}

	/* polys */
	i = totpoly;
	for (mp = mpoly; i--; mp++) {
		if ((mp->flag & ME_HIDE) == 0) {
			int ok = TRUE;
			const MLoop *ml;
			int j;
			j = mp->totloop;
			for (ml = &mloop[mp->loopstart]; j--; ml++) {
				if ((mvert[ml->v].flag & SELECT) == 0) {
					ok = FALSE;
					break;
				}
			}

			if (ok) {
				mp->flag |= ME_FACE_SEL;
			}
			else {
				mp->flag &= (char)~ME_FACE_SEL;
			}
		}
	}
}
void BKE_mesh_flush_select_from_verts(Mesh *me)
{
	BKE_mesh_flush_select_from_verts_ex(me->mvert, me->totvert,
	                                    me->mloop,
	                                    me->medge, me->totedge,
	                                    me->mpoly, me->totpoly);
}
/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Smooth Groups
 * \{ */

/**
 * Calculate smooth groups from sharp edges.
 *
 * \param r_totgroup The total number of groups, 1 or more.
 * \return Polygon aligned array of group index values (bitflags if use_bitflags is true), starting at 1.
 */
int *BKE_mesh_calc_smoothgroups(const MEdge *medge, const int totedge,
                                const MPoly *mpoly, const int totpoly,
                                const MLoop *mloop, const int totloop,
                                int *r_totgroup, const bool use_bitflags)
{
	int *poly_groups;
	int *poly_stack;

	int poly_prev = 0;
	const int temp_poly_group_id = 3;  /* Placeholder value. */
	const int poly_group_id_overflowed = 5;  /* Group we could not find any available bit, will be reset to 0 at end */
	int tot_group = 0;
	bool group_id_overflow = false;

	/* map vars */
	MeshElemMap *edge_poly_map;
	int *edge_poly_mem;

	if (totpoly == 0) {
		*r_totgroup = 0;
		return NULL;
	}

	BKE_mesh_edge_poly_map_create(&edge_poly_map, &edge_poly_mem,
	                              medge, totedge,
	                              mpoly, totpoly,
	                              mloop, totloop);

	poly_groups = MEM_callocN(sizeof(int) * (size_t)totpoly, __func__);
	poly_stack  = MEM_mallocN(sizeof(int) * (size_t)totpoly, __func__);

	while (true) {
		int poly;
		int bit_poly_group_mask = 0;
		int poly_group_id;
		int ps_curr_idx = 0, ps_end_idx = 0;  /* stack indices */

		for (poly = poly_prev; poly < totpoly; poly++) {
			if (poly_groups[poly] == 0) {
				break;
			}
		}

		if (poly == totpoly) {
			/* all done */
			break;
		}

		poly_group_id = use_bitflags ? temp_poly_group_id : ++tot_group;

		/* start searching from here next time */
		poly_prev = poly + 1;

		poly_groups[poly] = poly_group_id;
		poly_stack[ps_end_idx++] = poly;

		while (ps_curr_idx != ps_end_idx) {
			const MPoly *mp;
			const MLoop *ml;
			int j;

			poly = poly_stack[ps_curr_idx++];
			BLI_assert(poly_groups[poly] == poly_group_id);

			mp = &mpoly[poly];
			for (ml = &mloop[mp->loopstart], j = mp->totloop; j--; ml++) {
				/* loop over poly users */
				const MeshElemMap *map_ele = &edge_poly_map[ml->e];
				int *p = map_ele->indices;
				int i = map_ele->count;
				if (!(medge[ml->e].flag & ME_SHARP)) {
					for (; i--; p++) {
						/* if we meet other non initialized its a bug */
						BLI_assert(ELEM(poly_groups[*p], 0, poly_group_id));

						if (poly_groups[*p] == 0) {
							poly_groups[*p] = poly_group_id;
							poly_stack[ps_end_idx++] = *p;
						}
					}
				}
				else if (use_bitflags) {
					/* Find contiguous smooth groups already assigned, these are the values we can't reuse! */
					for (; i--; p++) {
						int bit = poly_groups[*p];
						if (!ELEM3(bit, 0, poly_group_id, poly_group_id_overflowed) &&
						    !(bit_poly_group_mask & bit))
						{
							bit_poly_group_mask |= bit;
						}
					}
				}
			}
		}
		/* And now, we have all our poly from current group in poly_stack (from 0 to (ps_end_idx - 1)), as well as
		 * all smoothgroups bits we can't use in bit_poly_group_mask.
		 */
		if (use_bitflags) {
			int i, *p, gid_bit = 0;
			poly_group_id = 1;

			/* Find first bit available! */
			for (; (poly_group_id & bit_poly_group_mask) && (gid_bit < 32); gid_bit++) {
				poly_group_id <<= 1;  /* will 'overflow' on last possible iteration. */
			}
			if (UNLIKELY(gid_bit > 31)) {
				/* All bits used in contiguous smooth groups, we can't do much!
				 * Note: this is *very* unlikely - theoretically, four groups are enough, I don't think we can reach
				 *       this goal with such a simple algo, but I don't think either we'll never need all 32 groups!
				 */
				printf("Warning, could not find an available id for current smooth group, faces will me marked "
				       "as out of any smooth group...\n");
				poly_group_id = poly_group_id_overflowed; /* Can't use 0, will have to set them to this value later. */
				group_id_overflow = true;
			}
			if (gid_bit > tot_group) {
				tot_group = gid_bit;
			}
			/* And assign the final smooth group id to that poly group! */
			for (i = ps_end_idx, p = poly_stack; i--; p++) {
				poly_groups[*p] = poly_group_id;
			}
		}
	}

	if (UNLIKELY(group_id_overflow)) {
		int i = totpoly, *gid = poly_groups;
		for (; i--; gid++) {
			if (*gid == poly_group_id_overflowed) {
				*gid = 0;
			}
		}
	}

	MEM_freeN(edge_poly_map);
	MEM_freeN(edge_poly_mem);
	MEM_freeN(poly_stack);

	*r_totgroup = tot_group + 1;

	return poly_groups;
}
/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Spatial Calculation
 * \{ */

/**
 * This function takes the difference between 2 vertex-coord-arrays
 * (\a vert_cos_src, \a vert_cos_dst),
 * and applies the difference to \a vert_cos_new relative to \a vert_cos_org.
 *
 * \param vert_cos_src reference deform source.
 * \param vert_cos_dst reference deform destination.
 *
 * \param vert_cos_org reference for the output location.
 * \param vert_cos_new resulting coords.
 */
void BKE_mesh_calc_relative_deform(
        const MPoly *mpoly, const int totpoly,
        const MLoop *mloop, const int totvert,

        const float (*vert_cos_src)[3],
        const float (*vert_cos_dst)[3],

        const float (*vert_cos_org)[3],
              float (*vert_cos_new)[3])
{
	const MPoly *mp;
	int i;

	int *vert_accum = MEM_callocN(sizeof(*vert_accum) * (size_t)totvert, __func__);

	memset(vert_cos_new, '\0', sizeof(*vert_cos_new) * (size_t)totvert);

	for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
		const MLoop *loopstart = mloop + mp->loopstart;
		int j;

		for (j = 0; j < mp->totloop; j++) {
			unsigned int v_prev = loopstart[(mp->totloop + (j - 1)) % mp->totloop].v;
			unsigned int v_curr = loopstart[j].v;
			unsigned int v_next = loopstart[(j + 1) % mp->totloop].v;

			float tvec[3];

			barycentric_transform(
			            tvec, vert_cos_dst[v_curr],
			            vert_cos_org[v_prev], vert_cos_org[v_curr], vert_cos_org[v_next],
			            vert_cos_src[v_prev], vert_cos_src[v_curr], vert_cos_src[v_next]
			            );

			add_v3_v3(vert_cos_new[v_curr], tvec);
			vert_accum[v_curr] += 1;
		}
	}

	for (i = 0; i < totvert; i++) {
		if (vert_accum[i]) {
			mul_v3_fl(vert_cos_new[i], 1.0f / (float)vert_accum[i]);
		}
		else {
			copy_v3_v3(vert_cos_new[i], vert_cos_org[i]);
		}
	}

	MEM_freeN(vert_accum);
}
/** \} */
