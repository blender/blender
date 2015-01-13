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
#include "BKE_report.h"

#include "BLI_strict_flags.h"

#include "mikktspace.h"

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

/* Calculate vertex and face normals, face normals are returned in *r_faceNors if non-NULL
 * and vertex normals are stored in actual mverts.
 */
void BKE_mesh_calc_normals_mapping(MVert *mverts, int numVerts,
                                   MLoop *mloop, MPoly *mpolys, int numLoops, int numPolys, float (*r_polyNors)[3],
                                   MFace *mfaces, int numFaces, const int *origIndexFace, float (*r_faceNors)[3])
{
	BKE_mesh_calc_normals_mapping_ex(mverts, numVerts, mloop, mpolys,
	                                 numLoops, numPolys, r_polyNors, mfaces, numFaces,
	                                 origIndexFace, r_faceNors, false);
}
/* extended version of 'BKE_mesh_calc_normals_poly' with option not to calc vertex normals */
void BKE_mesh_calc_normals_mapping_ex(
        MVert *mverts, int numVerts,
        MLoop *mloop, MPoly *mpolys,
        int numLoops, int numPolys, float (*r_polyNors)[3],
        MFace *mfaces, int numFaces, const int *origIndexFace, float (*r_faceNors)[3],
        const bool only_face_normals)
{
	float (*pnors)[3] = r_polyNors, (*fnors)[3] = r_faceNors;
	int i;
	MFace *mf;
	MPoly *mp;

	if (numPolys == 0) {
		if (only_face_normals == false) {
			mesh_calc_normals_vert_fallback(mverts, numVerts);
		}
		return;
	}

	/* if we are not calculating verts and no verts were passes then we have nothing to do */
	if ((only_face_normals == true) && (r_polyNors == NULL) && (r_faceNors == NULL)) {
		printf("%s: called with nothing to do\n", __func__);
		return;
	}

	if (!pnors) pnors = MEM_callocN(sizeof(float[3]) * (size_t)numPolys, __func__);
	/* if (!fnors) fnors = MEM_callocN(sizeof(float[3]) * numFaces, "face nors mesh.c"); */ /* NO NEED TO ALLOC YET */


	if (only_face_normals == false) {
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
	    /* fnors == r_faceNors */ /* NO NEED TO ALLOC YET */
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

	if (pnors != r_polyNors) MEM_freeN(pnors);
	/* if (fnors != r_faceNors) MEM_freeN(fnors); */ /* NO NEED TO ALLOC YET */

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
		const float *v_prev = mvert[ml[i_prev].v].co;
		const float *v_curr;

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

void BKE_mesh_calc_normals_tessface(MVert *mverts, int numVerts, MFace *mfaces, int numFaces, float (*r_faceNors)[3])
{
	float (*tnorms)[3] = MEM_callocN(sizeof(*tnorms) * (size_t)numVerts, "tnorms");
	float (*fnors)[3] = (r_faceNors) ? r_faceNors : MEM_callocN(sizeof(*fnors) * (size_t)numFaces, "meshnormals");
	int i;

	for (i = 0; i < numFaces; i++) {
		MFace *mf = &mfaces[i];
		float *f_no = fnors[i];
		float *n4 = (mf->v4) ? tnorms[mf->v4] : NULL;
		const float *c4 = (mf->v4) ? mverts[mf->v4].co : NULL;

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

	if (fnors != r_faceNors)
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
				 * An edge is sharp if it is tagged as such, or its face is not smooth,
				 * or both poly have opposed (flipped) normals, i.e. both loops on the same edge share the same vertex,
				 * or angle between both its polys' normals is above split_angle value.
				 */
				if (!(mp->flag & ME_SMOOTH) || (medges[ml_curr->e].flag & ME_SHARP) ||
				    ml_curr->v == mloops[e2l[0]].v ||
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
			/* We *do not need* to check/tag loops as already computed!
			 * Due to the fact a loop only links to one of its two edges, a same fan *will never be walked more than
			 * once!*
			 * Since we consider edges having neighbor polys with inverted (flipped) normals as sharp, we are sure that
			 * no fan will be skipped, even only considering the case (sharp curr_edge, smooth prev_edge), and not the
			 * alternative (smooth curr_edge, sharp prev_edge).
			 * All this due/thanks to link between normals and loop ordering.
			 */
			else {
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

				BLI_assert(mlfan_curr_index >= 0);
				BLI_assert(mlfan_vert_index >= 0);
				BLI_assert(mpfan_curr_index >= 0);

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

					BLI_assert(mlfan_curr_index >= 0);
					BLI_assert(mpfan_curr_index >= 0);

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
				else {
					/* We still have to clear the stack! */
					while (BLI_SMALLSTACK_POP(normal));
				}
			}

			ml_prev = ml_curr;
			ml_prev_index = ml_curr_index;
		}
	}

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

/** \name Mesh Tangent Calculations
 * \{ */

/* Tangent space utils. */

/* User data. */
typedef struct {
	MPoly *mpolys;         /* faces */
	MLoop *mloops;         /* faces's vertices */
	MVert *mverts;         /* vertices */
	MLoopUV *luvs;         /* texture coordinates */
	float (*lnors)[3];     /* loops' normals */
	float (*tangents)[4];  /* output tangents */
	int num_polys;         /* number of polygons */
} BKEMeshToTangent;

/* Mikktspace's API */
static int get_num_faces(const SMikkTSpaceContext *pContext)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	return p_mesh->num_polys;
}

static int get_num_verts_of_face(const SMikkTSpaceContext *pContext, const int face_idx)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	return p_mesh->mpolys[face_idx].totloop;
}

static void get_position(const SMikkTSpaceContext *pContext, float r_co[3], const int face_idx, const int vert_idx)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	const int loop_idx = p_mesh->mpolys[face_idx].loopstart + vert_idx;
	copy_v3_v3(r_co, p_mesh->mverts[p_mesh->mloops[loop_idx].v].co);
}

static void get_texture_coordinate(const SMikkTSpaceContext *pContext, float r_uv[2], const int face_idx,
                                   const int vert_idx)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	copy_v2_v2(r_uv, p_mesh->luvs[p_mesh->mpolys[face_idx].loopstart + vert_idx].uv);
}

static void get_normal(const SMikkTSpaceContext *pContext, float r_no[3], const int face_idx, const int vert_idx)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	copy_v3_v3(r_no, p_mesh->lnors[p_mesh->mpolys[face_idx].loopstart + vert_idx]);
}

static void set_tspace(const SMikkTSpaceContext *pContext, const float fv_tangent[3], const float face_sign,
                       const int face_idx, const int vert_idx)
{
	BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
	float *p_res = p_mesh->tangents[p_mesh->mpolys[face_idx].loopstart + vert_idx];
	copy_v3_v3(p_res, fv_tangent);
	p_res[3] = face_sign;
}

/**
 * Compute simplified tangent space normals, i.e. tangent vector + sign of bi-tangent one, which combined with
 * split normals can be used to recreate the full tangent space.
 * Note: * The mesh should be made of only tris and quads!
 */
void BKE_mesh_loop_tangents_ex(MVert *mverts, const int UNUSED(numVerts), MLoop *mloops,
                               float (*r_looptangent)[4], float (*loopnors)[3], MLoopUV *loopuvs,
                               const int UNUSED(numLoops), MPoly *mpolys, const int numPolys, ReportList *reports)
{
	BKEMeshToTangent mesh_to_tangent = {NULL};
	SMikkTSpaceContext s_context = {NULL};
	SMikkTSpaceInterface s_interface = {NULL};

	MPoly *mp;
	int mp_index;

	/* First check we do have a tris/quads only mesh. */
	for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
		if (mp->totloop > 4) {
			BKE_report(reports, RPT_ERROR, "Tangent space can only be computed for tris/quads, aborting");
			return;
		}
	}

	/* Compute Mikktspace's tangent normals. */
	mesh_to_tangent.mpolys = mpolys;
	mesh_to_tangent.mloops = mloops;
	mesh_to_tangent.mverts = mverts;
	mesh_to_tangent.luvs = loopuvs;
	mesh_to_tangent.lnors = loopnors;
	mesh_to_tangent.tangents = r_looptangent;
	mesh_to_tangent.num_polys = numPolys;

	s_context.m_pUserData = &mesh_to_tangent;
	s_context.m_pInterface = &s_interface;
	s_interface.m_getNumFaces = get_num_faces;
	s_interface.m_getNumVerticesOfFace = get_num_verts_of_face;
	s_interface.m_getPosition = get_position;
	s_interface.m_getTexCoord = get_texture_coordinate;
	s_interface.m_getNormal = get_normal;
	s_interface.m_setTSpaceBasic = set_tspace;

	/* 0 if failed */
	if (genTangSpaceDefault(&s_context) == false) {
		BKE_report(reports, RPT_ERROR, "Mikktspace failed to generate tangents for this mesh!");
	}
}

/**
 * Wrapper around BKE_mesh_loop_tangents_ex, which takes care of most boiling code.
 * Note: * There must be a valid loop's CD_NORMALS available.
 *       * The mesh should be made of only tris and quads!
 */
void BKE_mesh_loop_tangents(Mesh *mesh, const char *uvmap, float (*r_looptangents)[4], ReportList *reports)
{
	MLoopUV *loopuvs;
	float (*loopnors)[3];

	/* Check we have valid texture coordinates first! */
	if (uvmap) {
		loopuvs = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, uvmap);
	}
	else {
		loopuvs = CustomData_get_layer(&mesh->ldata, CD_MLOOPUV);
	}
	if (!loopuvs) {
		BKE_reportf(reports, RPT_ERROR, "Tangent space computation needs an UVMap, \"%s\" not found, aborting", uvmap);
		return;
	}

	loopnors = CustomData_get_layer(&mesh->ldata, CD_NORMAL);
	if (!loopnors) {
		BKE_report(reports, RPT_ERROR, "Tangent space computation needs loop normals, none found, aborting");
		return;
	}

	BKE_mesh_loop_tangents_ex(mesh->mvert, mesh->totvert, mesh->mloop, r_looptangents,
	                          loopnors, loopuvs, mesh->totloop, mesh->mpoly, mesh->totpoly, reports);
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
	const float *v_prev = mvert[loopstart[nverts - 1].v].co;
	const float *v_curr;
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
	const float *v_prev = vertex_coords[loopstart[nverts - 1].v];
	const float *v_curr;
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
                              MVert *mvarray)
{
	if (mpoly->totloop == 3) {
		return area_tri_v3(mvarray[loopstart[0].v].co,
		                   mvarray[loopstart[1].v].co,
		                   mvarray[loopstart[2].v].co
		                   );
	}
	else {
		int i;
		MLoop *l_iter = loopstart;
		float area;
		float (*vertexcos)[3] = BLI_array_alloca(vertexcos, (size_t)mpoly->totloop);

		/* pack vertex cos into an array for area_poly_v3 */
		for (i = 0; i < mpoly->totloop; i++, l_iter++) {
			copy_v3_v3(vertexcos[i], mvarray[l_iter->v].co);
		}

		/* finally calculate the area */
		area = area_poly_v3((const float (*)[3])vertexcos, (unsigned int)mpoly->totloop);

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
		BLI_BITMAP_ENABLE(edge_bitmap, ml->e);
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

/** \name Mesh Volume Calculation
 * \{ */

static bool mesh_calc_center_centroid_ex(MVert *mverts, int UNUSED(numVerts),
                                         MFace *mfaces, int numFaces,
                                         float center[3])
{
	float totweight;
	int f;
	
	zero_v3(center);
	
	if (numFaces == 0)
		return false;
	
	totweight = 0.0f;
	for (f = 0; f < numFaces; ++f) {
		MFace *face = &mfaces[f];
		MVert *v1 = &mverts[face->v1];
		MVert *v2 = &mverts[face->v2];
		MVert *v3 = &mverts[face->v3];
		MVert *v4 = &mverts[face->v4];
		float area;
		
		area = area_tri_v3(v1->co, v2->co, v3->co);
		madd_v3_v3fl(center, v1->co, area);
		madd_v3_v3fl(center, v2->co, area);
		madd_v3_v3fl(center, v3->co, area);
		totweight += area;
		
		if (face->v4) {
			area = area_tri_v3(v3->co, v4->co, v1->co);
			madd_v3_v3fl(center, v3->co, area);
			madd_v3_v3fl(center, v4->co, area);
			madd_v3_v3fl(center, v1->co, area);
			totweight += area;
		}
	}
	if (totweight == 0.0f)
		return false;
	
	mul_v3_fl(center, 1.0f / (3.0f * totweight));
	
	return true;
}

void BKE_mesh_calc_volume(MVert *mverts, int numVerts,
                          MFace *mfaces, int numFaces,
                          float *r_vol, float *r_com)
{
	float center[3];
	float totvol;
	int f;
	
	if (r_vol) *r_vol = 0.0f;
	if (r_com) zero_v3(r_com);
	
	if (numFaces == 0)
		return;
	
	if (!mesh_calc_center_centroid_ex(mverts, numVerts, mfaces, numFaces, center))
		return;
	
	totvol = 0.0f;
	for (f = 0; f < numFaces; ++f) {
		MFace *face = &mfaces[f];
		MVert *v1 = &mverts[face->v1];
		MVert *v2 = &mverts[face->v2];
		MVert *v3 = &mverts[face->v3];
		MVert *v4 = &mverts[face->v4];
		float vol;
		
		vol = volume_tetrahedron_signed_v3(center, v1->co, v2->co, v3->co);
		if (r_vol) {
			totvol += vol;
		}
		if (r_com) {
			/* averaging factor 1/4 is applied in the end */
			madd_v3_v3fl(r_com, center, vol); // XXX could extract this
			madd_v3_v3fl(r_com, v1->co, vol);
			madd_v3_v3fl(r_com, v2->co, vol);
			madd_v3_v3fl(r_com, v3->co, vol);
		}
		
		if (face->v4) {
			vol = volume_tetrahedron_signed_v3(center, v3->co, v4->co, v1->co);
			
			if (r_vol) {
				totvol += vol;
			}
			if (r_com) {
				/* averaging factor 1/4 is applied in the end */
				madd_v3_v3fl(r_com, center, vol); // XXX could extract this
				madd_v3_v3fl(r_com, v3->co, vol);
				madd_v3_v3fl(r_com, v4->co, vol);
				madd_v3_v3fl(r_com, v1->co, vol);
			}
		}
	}
	
	/* Note: Depending on arbitrary centroid position,
	 * totvol can become negative even for a valid mesh.
	 * The true value is always the positive value.
	 */
	if (r_vol) {
		*r_vol = fabsf(totvol);
	}
	if (r_com) {
		/* Note: Factor 1/4 is applied once for all vertices here.
		 * This also automatically negates the vector if totvol is negative.
		 */
		if (totvol != 0.0f)
			mul_v3_fl(r_com, 0.25f / totvol);
	}
}


/* -------------------------------------------------------------------- */

/** \name NGon Tessellation (NGon/Tessface Conversion)
 * \{ */

/**
 * Convert a triangle or quadrangle of loop/poly data to tessface data
 */
void BKE_mesh_loops_to_mface_corners(
        CustomData *fdata, CustomData *ldata,
        CustomData *pdata, unsigned int lindex[4], int findex,
        const int polyindex,
        const int mf_len, /* 3 or 4 */

        /* cache values to avoid lookups every time */
        const int numTex, /* CustomData_number_of_layers(pdata, CD_MTEXPOLY) */
        const int numCol, /* CustomData_number_of_layers(ldata, CD_MLOOPCOL) */
        const bool hasPCol, /* CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL) */
        const bool hasOrigSpace, /* CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP) */
        const bool hasLNor /* CustomData_has_layer(ldata, CD_NORMAL) */
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
			mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, (int)lindex[j], i);
			copy_v2_v2(texface->uv[j], mloopuv->uv);
		}
	}

	for (i = 0; i < numCol; i++) {
		mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

		for (j = 0; j < mf_len; j++) {
			mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, (int)lindex[j], i);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}

	if (hasPCol) {
		mcol = CustomData_get(fdata,  findex, CD_PREVIEW_MCOL);

		for (j = 0; j < mf_len; j++) {
			mloopcol = CustomData_get(ldata, (int)lindex[j], CD_PREVIEW_MLOOPCOL);
			MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
		}
	}

	if (hasOrigSpace) {
		OrigSpaceFace *of = CustomData_get(fdata, findex, CD_ORIGSPACE);
		OrigSpaceLoop *lof;

		for (j = 0; j < mf_len; j++) {
			lof = CustomData_get(ldata, (int)lindex[j], CD_ORIGSPACE_MLOOP);
			copy_v2_v2(of->uv[j], lof->uv);
		}
	}

	if (hasLNor) {
		short (*tlnors)[3] = CustomData_get(fdata, findex, CD_TESSLOOPNORMAL);

		for (j = 0; j < mf_len; j++) {
			normal_float_to_short_v3(tlnors[j], CustomData_get(ldata, (int)lindex[j], CD_NORMAL));
		}
	}
}

/**
 * Convert all CD layers from loop/poly to tessface data.
 *
 * \param loopindices is an array of an int[4] per tessface, mapping tessface's verts to loops indices.
 *
 * \note when mface is not NULL, mface[face_index].v4 is used to test quads, else, loopindices[face_index][3] is used.
 */
void BKE_mesh_loops_to_tessdata(CustomData *fdata, CustomData *ldata, CustomData *pdata, MFace *mface,
                                int *polyindices, unsigned int (*loopindices)[4], const int num_faces)
{
	/* Note: performances are sub-optimal when we get a NULL mface, we could be ~25% quicker with dedicated code...
	 *       Issue is, unless having two different functions with nearly the same code, there's not much ways to solve
	 *       this. Better imho to live with it for now. :/ --mont29
	 */
	const int numTex = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
	const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
	const bool hasLoopNormal = CustomData_has_layer(ldata, CD_NORMAL);
	int findex, i, j;
	const int *pidx;
	unsigned int (*lidx)[4];

	for (i = 0; i < numTex; i++) {
		MTFace *texface = CustomData_get_layer_n(fdata, CD_MTFACE, i);
		MTexPoly *texpoly = CustomData_get_layer_n(pdata, CD_MTEXPOLY, i);
		MLoopUV *mloopuv = CustomData_get_layer_n(ldata, CD_MLOOPUV, i);

		for (findex = 0, pidx = polyindices, lidx = loopindices;
		     findex < num_faces;
		     pidx++, lidx++, findex++, texface++)
		{
			ME_MTEXFACE_CPY(texface, &texpoly[*pidx]);

			for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
				copy_v2_v2(texface->uv[j], mloopuv[(*lidx)[j]].uv);
			}
		}
	}

	for (i = 0; i < numCol; i++) {
		MCol (*mcol)[4] = CustomData_get_layer_n(fdata, CD_MCOL, i);
		MLoopCol *mloopcol = CustomData_get_layer_n(ldata, CD_MLOOPCOL, i);

		for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
			for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
				MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
			}
		}
	}

	if (hasPCol) {
		MCol (*mcol)[4] = CustomData_get_layer(fdata, CD_PREVIEW_MCOL);
		MLoopCol *mloopcol = CustomData_get_layer(ldata, CD_PREVIEW_MLOOPCOL);

		for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
			for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
				MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
			}
		}
	}

	if (hasOrigSpace) {
		OrigSpaceFace *of = CustomData_get_layer(fdata, CD_ORIGSPACE);
		OrigSpaceLoop *lof = CustomData_get_layer(ldata, CD_ORIGSPACE_MLOOP);

		for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, of++) {
			for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
				copy_v2_v2(of->uv[j], lof[(*lidx)[j]].uv);
			}
		}
	}

	if (hasLoopNormal) {
		short (*fnors)[4][3] = CustomData_get_layer(fdata, CD_TESSLOOPNORMAL);
		float (*lnors)[3] = CustomData_get_layer(ldata, CD_NORMAL);

		for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, fnors++) {
			for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
				normal_float_to_short_v3((*fnors)[j], lnors[(*lidx)[j]]);
			}
		}
	}
}

/**
 * Recreate tessellation.
 *
 * \param do_face_nor_copy controls whether the normals from the poly are copied to the tessellated faces.
 *
 * \return number of tessellation faces.
 */
int BKE_mesh_recalc_tessellation(CustomData *fdata, CustomData *ldata, CustomData *pdata,
                                 MVert *mvert, int totface, int totloop, int totpoly, const bool do_face_nor_cpy)
{
	/* use this to avoid locking pthread for _every_ polygon
	 * and calling the fill function */

#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS  /* NEEDS FURTHER TESTING */

/* We abuse MFace->edcode to tag quad faces. See below for details. */
#define TESSFACE_IS_QUAD 1

	const int looptris_tot = poly_to_tri_count(totpoly, totloop);

	MPoly *mp, *mpoly;
	MLoop *ml, *mloop;
	MFace *mface, *mf;
	MemArena *arena = NULL;
	int *mface_to_poly_map;
	unsigned int (*lindices)[4];
	int poly_index, mface_index;
	unsigned int j;

	mpoly = CustomData_get_layer(pdata, CD_MPOLY);
	mloop = CustomData_get_layer(ldata, CD_MLOOP);

	/* allocate the length of totfaces, avoid many small reallocs,
	 * if all faces are tri's it will be correct, quads == 2x allocs */
	/* take care. we are _not_ calloc'ing so be sure to initialize each field */
	mface_to_poly_map = MEM_mallocN(sizeof(*mface_to_poly_map) * (size_t)looptris_tot, __func__);
	mface             = MEM_mallocN(sizeof(*mface) *             (size_t)looptris_tot, __func__);
	lindices          = MEM_mallocN(sizeof(*lindices) *          (size_t)looptris_tot, __func__);

	mface_index = 0;
	mp = mpoly;
	for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
		const unsigned int mp_loopstart = (unsigned int)mp->loopstart;
		const unsigned int mp_totloop = (unsigned int)mp->totloop;
		unsigned int l1, l2, l3, l4;
		unsigned int *lidx;
		if (mp_totloop < 3) {
			/* do nothing */
		}

#ifdef USE_TESSFACE_SPEEDUP

#define ML_TO_MF(i1, i2, i3)                                                  \
		mface_to_poly_map[mface_index] = poly_index;                          \
		mf = &mface[mface_index];                                             \
		lidx = lindices[mface_index];                                         \
		/* set loop indices, transformed to vert indices later */             \
		l1 = mp_loopstart + i1;                                               \
		l2 = mp_loopstart + i2;                                               \
		l3 = mp_loopstart + i3;                                               \
		mf->v1 = mloop[l1].v;                                                 \
		mf->v2 = mloop[l2].v;                                                 \
		mf->v3 = mloop[l3].v;                                                 \
		mf->v4 = 0;                                                           \
		lidx[0] = l1;                                                         \
		lidx[1] = l2;                                                         \
		lidx[2] = l3;                                                         \
		lidx[3] = 0;                                                          \
		mf->mat_nr = mp->mat_nr;                                              \
		mf->flag = mp->flag;                                                  \
		mf->edcode = 0;                                                       \
		(void)0

/* ALMOST IDENTICAL TO DEFINE ABOVE (see EXCEPTION) */
#define ML_TO_MF_QUAD()                                                       \
		mface_to_poly_map[mface_index] = poly_index;                          \
		mf = &mface[mface_index];                                             \
		lidx = lindices[mface_index];                                         \
		/* set loop indices, transformed to vert indices later */             \
		l1 = mp_loopstart + 0; /* EXCEPTION */                                \
		l2 = mp_loopstart + 1; /* EXCEPTION */                                \
		l3 = mp_loopstart + 2; /* EXCEPTION */                                \
		l4 = mp_loopstart + 3; /* EXCEPTION */                                \
		mf->v1 = mloop[l1].v;                                                 \
		mf->v2 = mloop[l2].v;                                                 \
		mf->v3 = mloop[l3].v;                                                 \
		mf->v4 = mloop[l4].v;                                                 \
		lidx[0] = l1;                                                         \
		lidx[1] = l2;                                                         \
		lidx[2] = l3;                                                         \
		lidx[3] = l4;                                                         \
		mf->mat_nr = mp->mat_nr;                                              \
		mf->flag = mp->flag;                                                  \
		mf->edcode = TESSFACE_IS_QUAD;                                        \
		(void)0


		else if (mp_totloop == 3) {
			ML_TO_MF(0, 1, 2);
			mface_index++;
		}
		else if (mp_totloop == 4) {
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

			const unsigned int totfilltri = mp_totloop - 2;

			if (UNLIKELY(arena == NULL)) {
				arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
			}

			tris = BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
			projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp_totloop);

			zero_v3(normal);

			/* calc normal */
			ml = mloop + mp_loopstart;
			co_prev = mvert[ml[mp_totloop - 1].v].co;
			for (j = 0; j < mp_totloop; j++, ml++) {
				co_curr = mvert[ml->v].co;
				add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
				co_prev = co_curr;
			}
			if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
				normal[2] = 1.0f;
			}

			/* project verts to 2d */
			axis_dominant_v3_to_m3(axis_mat, normal);

			ml = mloop + mp_loopstart;
			for (j = 0; j < mp_totloop; j++, ml++) {
				mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
			}

			BLI_polyfill_calc_arena((const float (*)[2])projverts, mp_totloop, -1, tris, arena);

			/* apply fill */
			for (j = 0; j < totfilltri; j++) {
				unsigned int *tri = tris[j];
				lidx = lindices[mface_index];

				mface_to_poly_map[mface_index] = poly_index;
				mf = &mface[mface_index];

				/* set loop indices, transformed to vert indices later */
				l1 = mp_loopstart + tri[0];
				l2 = mp_loopstart + tri[1];
				l3 = mp_loopstart + tri[2];

				/* sort loop indices to ensure winding is correct */
				if (l1 > l2) SWAP(unsigned int, l1, l2);
				if (l2 > l3) SWAP(unsigned int, l2, l3);
				if (l1 > l2) SWAP(unsigned int, l1, l2);

				mf->v1 = mloop[l1].v;
				mf->v2 = mloop[l2].v;
				mf->v3 = mloop[l3].v;
				mf->v4 = 0;

				lidx[0] = l1;
				lidx[1] = l2;
				lidx[2] = l3;
				lidx[3] = 0;

				mf->mat_nr = mp->mat_nr;
				mf->flag = mp->flag;
				mf->edcode = 0;

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

	/* NOTE: quad detection issue - forth vertidx vs forth loopidx:
	 * Polygons take care of their loops ordering, hence not of their vertices ordering.
	 * Currently, our tfaces' forth vertex index might be 0 even for a quad. However, we know our forth loop index is
	 * never 0 for quads (because they are sorted for polygons, and our quads are still mere copies of their polygons).
	 * So we pass NULL as MFace pointer, and BKE_mesh_loops_to_tessdata will use the forth loop index as quad test.
	 * ...
	 */
	BKE_mesh_loops_to_tessdata(fdata, ldata, pdata, NULL, mface_to_poly_map, lindices, totface);

	/* NOTE: quad detection issue - forth vertidx vs forth loopidx:
	 * ...However, most TFace code uses 'MFace->v4 == 0' test to check whether it is a tri or quad.
	 * test_index_face() will check this and rotate the tessellated face if needed.
	 */
#ifdef USE_TESSFACE_QUADS
	mf = mface;
	for (mface_index = 0; mface_index < totface; mface_index++, mf++) {
		if (mf->edcode == TESSFACE_IS_QUAD) {
			test_index_face(mf, fdata, mface_index, 4);
			mf->edcode = 0;
		}
	}
#endif

	MEM_freeN(lindices);

	return totface;

#undef USE_TESSFACE_SPEEDUP
#undef USE_TESSFACE_QUADS

#undef ML_TO_MF
#undef ML_TO_MF_QUAD

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

	unsigned int lindex[4];
	int i;
	int k;

	MPoly *mp, *mpoly;
	MFace *mface, *mf;

	const int numTex = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
	const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
	const bool hasLNor = CustomData_has_layer(ldata, CD_NORMAL);

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

				lindex[0] = mf->v1;
				lindex[1] = mf->v2;
				lindex[2] = mf->v3;
				lindex[3] = 0; /* unused */

				/* transform loop indices to vert indices */
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;

				BKE_mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                                lindex, k, i, 3,
				                                numTex, numCol, hasPCol, hasOrigSpace, hasLNor);
				test_index_face(mf, fdata, k, 3);
			}
			else {
				/* sort loop indices to ensure winding is correct */
				/* NO SORT - looks like we can skip this */

				lindex[0] = mf->v1;
				lindex[1] = mf->v2;
				lindex[2] = mf->v3;
				lindex[3] = mf->v4;

				/* transform loop indices to vert indices */
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;
				mf->v4 = mloop[mf->v4].v;

				BKE_mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                                lindex, k, i, 4,
				                                numTex, numCol, hasPCol, hasOrigSpace, hasLNor);
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

	if (CustomData_has_layer(fdata, CD_TESSLOOPNORMAL)) {
		float (*lnors)[3] = CustomData_get(ldata, loopstart, CD_NORMAL);
		short (*tlnors)[3] = CustomData_get(fdata, findex, CD_TESSLOOPNORMAL);
		const int max = mf->v4 ? 4 : 3;

		for (i = 0; i < max; i++, lnors++, tlnors++) {
			normal_short_to_float_v3(*lnors, *tlnors);
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
                                          int *r_totloop, int *r_totpoly,
                                          MLoop **r_mloop, MPoly **r_mpoly)
{
	MFace *mf;
	MLoop *ml, *mloop;
	MPoly *mp, *mpoly;
	MEdge *me;
	EdgeHash *eh;
	int numTex, numCol;
	int i, j, totloop, totpoly, *polyindex;

	/* old flag, clear to allow for reuse */
#define ME_FGON (1 << 3)

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

	*r_totpoly = totpoly;
	*r_totloop = totloop;
	*r_mpoly = mpoly;
	*r_mloop = mloop;

#undef ME_FGON

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
			bool ok = true;
			const MLoop *ml;
			int j;
			j = mp->totloop;
			for (ml = &mloop[mp->loopstart]; j--; ml++) {
				if ((mvert[ml->v].flag & SELECT) == 0) {
					ok = false;
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

			transform_point_by_tri_v3(
			        tvec, vert_cos_dst[v_curr],
			        vert_cos_org[v_prev], vert_cos_org[v_curr], vert_cos_org[v_next],
			        vert_cos_src[v_prev], vert_cos_src[v_curr], vert_cos_src[v_next]);

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
