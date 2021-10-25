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
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_mesh.c
 *  \ingroup bmesh
 *
 * BM mesh level functions.
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_object_types.h"

#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"

#include "intern/bmesh_private.h"

/* used as an extern, defined in bmesh.h */
const BMAllocTemplate bm_mesh_allocsize_default = {512, 1024, 2048, 512};
const BMAllocTemplate bm_mesh_chunksize_default = {512, 1024, 2048, 512};

static void bm_mempool_init_ex(
        const BMAllocTemplate *allocsize, const bool use_toolflags,
        BLI_mempool **r_vpool, BLI_mempool **r_epool, BLI_mempool **r_lpool, BLI_mempool **r_fpool)
{
	size_t vert_size, edge_size, loop_size, face_size;

	if (use_toolflags == true) {
		vert_size = sizeof(BMVert_OFlag);
		edge_size = sizeof(BMEdge_OFlag);
		loop_size = sizeof(BMLoop);
		face_size = sizeof(BMFace_OFlag);
	}
	else {
		vert_size = sizeof(BMVert);
		edge_size = sizeof(BMEdge);
		loop_size = sizeof(BMLoop);
		face_size = sizeof(BMFace);
	}

	if (r_vpool) {
		*r_vpool = BLI_mempool_create(
		        vert_size, allocsize->totvert,
		        bm_mesh_chunksize_default.totvert, BLI_MEMPOOL_ALLOW_ITER);
	}
	if (r_epool) {
		*r_epool = BLI_mempool_create(
		        edge_size, allocsize->totedge,
		        bm_mesh_chunksize_default.totedge, BLI_MEMPOOL_ALLOW_ITER);
	}
	if (r_lpool) {
		*r_lpool = BLI_mempool_create(
		        loop_size, allocsize->totloop,
		        bm_mesh_chunksize_default.totloop, BLI_MEMPOOL_NOP);
	}
	if (r_fpool) {
		*r_fpool = BLI_mempool_create(
		        face_size, allocsize->totface,
		        bm_mesh_chunksize_default.totface, BLI_MEMPOOL_ALLOW_ITER);
	}
}

static void bm_mempool_init(BMesh *bm, const BMAllocTemplate *allocsize, const bool use_toolflags)
{
	bm_mempool_init_ex(
	        allocsize, use_toolflags,
	        &bm->vpool, &bm->epool, &bm->lpool, &bm->fpool);

#ifdef USE_BMESH_HOLES
	bm->looplistpool = BLI_mempool_create(sizeof(BMLoopList), 512, 512, BLI_MEMPOOL_NOP);
#endif
}

void BM_mesh_elem_toolflags_ensure(BMesh *bm)
{
	BLI_assert(bm->use_toolflags);

	if (bm->vtoolflagpool && bm->etoolflagpool && bm->ftoolflagpool) {
		return;
	}

	bm->vtoolflagpool = BLI_mempool_create(sizeof(BMFlagLayer), bm->totvert, 512, BLI_MEMPOOL_NOP);
	bm->etoolflagpool = BLI_mempool_create(sizeof(BMFlagLayer), bm->totedge, 512, BLI_MEMPOOL_NOP);
	bm->ftoolflagpool = BLI_mempool_create(sizeof(BMFlagLayer), bm->totface, 512, BLI_MEMPOOL_NOP);

#pragma omp parallel sections if (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT)
	{
#pragma omp section
		{
			BLI_mempool *toolflagpool = bm->vtoolflagpool;
			BMIter iter;
			BMVert_OFlag *ele;
			BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
				ele->oflags = BLI_mempool_calloc(toolflagpool);
			}
		}
#pragma omp section
		{
			BLI_mempool *toolflagpool = bm->etoolflagpool;
			BMIter iter;
			BMEdge_OFlag *ele;
			BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
				ele->oflags = BLI_mempool_calloc(toolflagpool);
			}
		}
#pragma omp section
		{
			BLI_mempool *toolflagpool = bm->ftoolflagpool;
			BMIter iter;
			BMFace_OFlag *ele;
			BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
				ele->oflags = BLI_mempool_calloc(toolflagpool);
			}
		}
	}


	bm->totflags = 1;
}

void BM_mesh_elem_toolflags_clear(BMesh *bm)
{
	if (bm->vtoolflagpool) {
		BLI_mempool_destroy(bm->vtoolflagpool);
		bm->vtoolflagpool = NULL;
	}
	if (bm->etoolflagpool) {
		BLI_mempool_destroy(bm->etoolflagpool);
		bm->etoolflagpool = NULL;
	}
	if (bm->ftoolflagpool) {
		BLI_mempool_destroy(bm->ftoolflagpool);
		bm->ftoolflagpool = NULL;
	}
}

/**
 * \brief BMesh Make Mesh
 *
 * Allocates a new BMesh structure.
 *
 * \return The New bmesh
 *
 * \note ob is needed by multires
 */
BMesh *BM_mesh_create(
        const BMAllocTemplate *allocsize,
        const struct BMeshCreateParams *params)
{
	/* allocate the structure */
	BMesh *bm = MEM_callocN(sizeof(BMesh), __func__);
	
	/* allocate the memory pools for the mesh elements */
	bm_mempool_init(bm, allocsize, params->use_toolflags);

	/* allocate one flag pool that we don't get rid of. */
	bm->use_toolflags = params->use_toolflags;
	bm->toolflag_index = 0;
	bm->totflags = 0;

	CustomData_reset(&bm->vdata);
	CustomData_reset(&bm->edata);
	CustomData_reset(&bm->ldata);
	CustomData_reset(&bm->pdata);

	return bm;
}

/**
 * \brief BMesh Free Mesh Data
 *
 *	Frees a BMesh structure.
 *
 * \note frees mesh, but not actual BMesh struct
 */
void BM_mesh_data_free(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMLoop *l;
	BMFace *f;

	BMIter iter;
	BMIter itersub;

	const bool is_ldata_free = CustomData_bmesh_has_free(&bm->ldata);
	const bool is_pdata_free = CustomData_bmesh_has_free(&bm->pdata);

	/* Check if we have to call free, if not we can avoid a lot of looping */
	if (CustomData_bmesh_has_free(&(bm->vdata))) {
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			CustomData_bmesh_free_block(&(bm->vdata), &(v->head.data));
		}
	}
	if (CustomData_bmesh_has_free(&(bm->edata))) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			CustomData_bmesh_free_block(&(bm->edata), &(e->head.data));
		}
	}

	if (is_ldata_free || is_pdata_free) {
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (is_pdata_free)
				CustomData_bmesh_free_block(&(bm->pdata), &(f->head.data));
			if (is_ldata_free) {
				BM_ITER_ELEM (l, &itersub, f, BM_LOOPS_OF_FACE) {
					CustomData_bmesh_free_block(&(bm->ldata), &(l->head.data));
				}
			}
		}
	}

	/* Free custom data pools, This should probably go in CustomData_free? */
	if (bm->vdata.totlayer) BLI_mempool_destroy(bm->vdata.pool);
	if (bm->edata.totlayer) BLI_mempool_destroy(bm->edata.pool);
	if (bm->ldata.totlayer) BLI_mempool_destroy(bm->ldata.pool);
	if (bm->pdata.totlayer) BLI_mempool_destroy(bm->pdata.pool);

	/* free custom data */
	CustomData_free(&bm->vdata, 0);
	CustomData_free(&bm->edata, 0);
	CustomData_free(&bm->ldata, 0);
	CustomData_free(&bm->pdata, 0);

	/* destroy element pools */
	BLI_mempool_destroy(bm->vpool);
	BLI_mempool_destroy(bm->epool);
	BLI_mempool_destroy(bm->lpool);
	BLI_mempool_destroy(bm->fpool);

	if (bm->vtable) MEM_freeN(bm->vtable);
	if (bm->etable) MEM_freeN(bm->etable);
	if (bm->ftable) MEM_freeN(bm->ftable);

	/* destroy flag pool */
	BM_mesh_elem_toolflags_clear(bm);

#ifdef USE_BMESH_HOLES
	BLI_mempool_destroy(bm->looplistpool);
#endif

	BLI_freelistN(&bm->selected);

	BMO_error_clear(bm);
}

/**
 * \brief BMesh Clear Mesh
 *
 * Clear all data in bm
 */
void BM_mesh_clear(BMesh *bm)
{
	const bool use_toolflags = bm->use_toolflags;

	/* free old mesh */
	BM_mesh_data_free(bm);
	memset(bm, 0, sizeof(BMesh));

	/* allocate the memory pools for the mesh elements */
	bm_mempool_init(bm, &bm_mesh_allocsize_default, use_toolflags);

	bm->use_toolflags = use_toolflags;
	bm->toolflag_index = 0;
	bm->totflags = 0;

	CustomData_reset(&bm->vdata);
	CustomData_reset(&bm->edata);
	CustomData_reset(&bm->ldata);
	CustomData_reset(&bm->pdata);
}

/**
 * \brief BMesh Free Mesh
 *
 *	Frees a BMesh data and its structure.
 */
void BM_mesh_free(BMesh *bm)
{
	BM_mesh_data_free(bm);

	if (bm->py_handle) {
		/* keep this out of 'BM_mesh_data_free' because we want python
		 * to be able to clear the mesh and maintain access. */
		bpy_bm_generic_invalidate(bm->py_handle);
		bm->py_handle = NULL;
	}

	MEM_freeN(bm);
}

/**
 * Helpers for #BM_mesh_normals_update and #BM_verts_calc_normal_vcos
 */
static void bm_mesh_edges_calc_vectors(BMesh *bm, float (*edgevec)[3], const float (*vcos)[3])
{
	BMIter eiter;
	BMEdge *e;
	int index;

	if (vcos) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, index) {
		BM_elem_index_set(e, index); /* set_inline */

		if (e->l) {
			const float *v1_co = vcos ? vcos[BM_elem_index_get(e->v1)] : e->v1->co;
			const float *v2_co = vcos ? vcos[BM_elem_index_get(e->v2)] : e->v2->co;
			sub_v3_v3v3(edgevec[index], v2_co, v1_co);
			normalize_v3(edgevec[index]);
		}
		else {
			/* the edge vector will not be needed when the edge has no radial */
		}
	}
	bm->elem_index_dirty &= ~BM_EDGE;
}

static void bm_mesh_verts_calc_normals(
        BMesh *bm, const float (*edgevec)[3], const float (*fnos)[3],
        const float (*vcos)[3], float (*vnos)[3])
{
	BM_mesh_elem_index_ensure(bm, (vnos) ? (BM_EDGE | BM_VERT) : BM_EDGE);

	/* add weighted face normals to vertices */
	{
		BMIter fiter;
		BMFace *f;
		int i;

		BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
			BMLoop *l_first, *l_iter;
			const float *f_no = fnos ? fnos[i] : f->no;

			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				const float *e1diff, *e2diff;
				float dotprod;
				float fac;
				float *v_no = vnos ? vnos[BM_elem_index_get(l_iter->v)] : l_iter->v->no;

				/* calculate the dot product of the two edges that
				 * meet at the loop's vertex */
				e1diff = edgevec[BM_elem_index_get(l_iter->prev->e)];
				e2diff = edgevec[BM_elem_index_get(l_iter->e)];
				dotprod = dot_v3v3(e1diff, e2diff);

				/* edge vectors are calculated from e->v1 to e->v2, so
				 * adjust the dot product if one but not both loops
				 * actually runs from from e->v2 to e->v1 */
				if ((l_iter->prev->e->v1 == l_iter->prev->v) ^ (l_iter->e->v1 == l_iter->v)) {
					dotprod = -dotprod;
				}

				fac = saacos(-dotprod);

				/* accumulate weighted face normal into the vertex's normal */
				madd_v3_v3fl(v_no, f_no, fac);
			} while ((l_iter = l_iter->next) != l_first);
		}
	}


	/* normalize the accumulated vertex normals */
	{
		BMIter viter;
		BMVert *v;
		int i;

		BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
			float *v_no = vnos ? vnos[i] : v->no;
			if (UNLIKELY(normalize_v3(v_no) == 0.0f)) {
				const float *v_co = vcos ? vcos[i] : v->co;
				normalize_v3_v3(v_no, v_co);
			}
		}
	}
}

/**
 * \brief BMesh Compute Normals
 *
 * Updates the normals of a mesh.
 */
void BM_mesh_normals_update(BMesh *bm)
{
	float (*edgevec)[3] = MEM_mallocN(sizeof(*edgevec) * bm->totedge, __func__);

#pragma omp parallel sections if (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT)
	{
#pragma omp section
		{
			/* calculate all face normals */
			BMIter fiter;
			BMFace *f;
			int i;

			BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
				BM_elem_index_set(f, i); /* set_inline */
				BM_face_normal_update(f);
			}
			bm->elem_index_dirty &= ~BM_FACE;
		}
#pragma omp section
		{
			/* Zero out vertex normals */
			BMIter viter;
			BMVert *v;
			int i;

			BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
				BM_elem_index_set(v, i); /* set_inline */
				zero_v3(v->no);
			}
			bm->elem_index_dirty &= ~BM_VERT;
		}
#pragma omp section
		{
			/* Compute normalized direction vectors for each edge.
			 * Directions will be used for calculating the weights of the face normals on the vertex normals.
			 */
			bm_mesh_edges_calc_vectors(bm, edgevec, NULL);
		}
	}
	/* end omp */

	/* Add weighted face normals to vertices, and normalize vert normals. */
	bm_mesh_verts_calc_normals(bm, (const float(*)[3])edgevec, NULL, NULL, NULL);
	MEM_freeN(edgevec);
}

/**
 * \brief BMesh Compute Normals from/to external data.
 *
 * Computes the vertex normals of a mesh into vnos, using given vertex coordinates (vcos) and polygon normals (fnos).
 */
void BM_verts_calc_normal_vcos(BMesh *bm, const float (*fnos)[3], const float (*vcos)[3], float (*vnos)[3])
{
	float (*edgevec)[3] = MEM_mallocN(sizeof(*edgevec) * bm->totedge, __func__);

	/* Compute normalized direction vectors for each edge.
	 * Directions will be used for calculating the weights of the face normals on the vertex normals.
	 */
	bm_mesh_edges_calc_vectors(bm, edgevec, vcos);

	/* Add weighted face normals to vertices, and normalize vert normals. */
	bm_mesh_verts_calc_normals(bm, (const float(*)[3])edgevec, fnos, vcos, vnos);
	MEM_freeN(edgevec);
}

/**
 * Helpers for #BM_mesh_loop_normals_update and #BM_loops_calc_normals_vnos
 */
static void bm_mesh_edges_sharp_tag(
        BMesh *bm, const float (*vnos)[3], const float (*fnos)[3], float split_angle,
        float (*r_lnos)[3])
{
	BMIter eiter;
	BMEdge *e;
	int i;

	const bool check_angle = (split_angle < (float)M_PI);

	if (check_angle) {
		split_angle = cosf(split_angle);
	}

	{
		char htype = BM_VERT | BM_LOOP;
		if (fnos) {
			htype |= BM_FACE;
		}
		BM_mesh_elem_index_ensure(bm, htype);
	}

	/* This first loop checks which edges are actually smooth, and pre-populate lnos with vnos (as if they were
	 * all smooth).
	 */
	BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
		BMLoop *l_a, *l_b;

		BM_elem_index_set(e, i); /* set_inline */
		BM_elem_flag_disable(e, BM_ELEM_TAG); /* Clear tag (means edge is sharp). */

		/* An edge with only two loops, might be smooth... */
		if (BM_edge_loop_pair(e, &l_a, &l_b)) {
			bool is_angle_smooth = true;
			if (check_angle) {
				const float *no_a = fnos ? fnos[BM_elem_index_get(l_a->f)] : l_a->f->no;
				const float *no_b = fnos ? fnos[BM_elem_index_get(l_b->f)] : l_b->f->no;
				is_angle_smooth = (dot_v3v3(no_a, no_b) >= split_angle);
			}

			/* We only tag edges that are *really* smooth:
			 * If the angle between both its polys' normals is below split_angle value,
			 * and it is tagged as such,
			 * and both its faces are smooth,
			 * and both its faces have compatible (non-flipped) normals,
			 * i.e. both loops on the same edge do not share the same vertex.
			 */
			if (is_angle_smooth &&
			    BM_elem_flag_test(e, BM_ELEM_SMOOTH) &&
			    BM_elem_flag_test(l_a->f, BM_ELEM_SMOOTH) &&
			    BM_elem_flag_test(l_b->f, BM_ELEM_SMOOTH) &&
			    l_a->v != l_b->v)
			{
				const float *no;
				BM_elem_flag_enable(e, BM_ELEM_TAG);

				/* linked vertices might be fully smooth, copy their normals to loop ones. */
				no = vnos ? vnos[BM_elem_index_get(l_a->v)] : l_a->v->no;
				copy_v3_v3(r_lnos[BM_elem_index_get(l_a)], no);
				no = vnos ? vnos[BM_elem_index_get(l_b->v)] : l_b->v->no;
				copy_v3_v3(r_lnos[BM_elem_index_get(l_b)], no);
			}
		}
	}

	bm->elem_index_dirty &= ~BM_EDGE;
}

/* Check whether gievn loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point', and yet we need to walk them once, and only once. */
static bool bm_mesh_loop_check_cyclic_smooth_fan(BMLoop *l_curr)
{
	BMLoop *lfan_pivot_next = l_curr;
	BMEdge *e_next = l_curr->e;

	BLI_assert(!BM_elem_flag_test(lfan_pivot_next, BM_ELEM_TAG));
	BM_elem_flag_enable(lfan_pivot_next, BM_ELEM_TAG);

	while (true) {
		/* Much simpler than in sibling code with basic Mesh data! */
		lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot_next, &e_next);

		if (!lfan_pivot_next || !BM_elem_flag_test(e_next, BM_ELEM_TAG)) {
			/* Sharp loop/edge, so not a cyclic smooth fan... */
			return false;
		}
		/* Smooth loop/edge... */
		else if (BM_elem_flag_test(lfan_pivot_next, BM_ELEM_TAG)) {
			if (lfan_pivot_next == l_curr) {
				/* We walked around a whole cyclic smooth fan without finding any already-processed loop, means we can
				 * use initial l_curr/l_prev edge as start for this smooth fan. */
				return true;
			}
			/* ... already checked in some previous looping, we can abort. */
			return false;
		}
		else {
			/* ... we can skip it in future, and keep checking the smooth fan. */
			BM_elem_flag_enable(lfan_pivot_next, BM_ELEM_TAG);
		}
	}
}

/* BMesh version of BKE_mesh_normals_loop_split() in mesh_evaluate.c
 * Will use first clnors_data array, and fallback to cd_loop_clnors_offset (use NULL and -1 to not use clnors). */
static void bm_mesh_loops_calc_normals(
        BMesh *bm, const float (*vcos)[3], const float (*fnos)[3], float (*r_lnos)[3],
        MLoopNorSpaceArray *r_lnors_spacearr, short (*clnors_data)[2], const int cd_loop_clnors_offset)
{
	BMIter fiter;
	BMFace *f_curr;
	const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

	MLoopNorSpaceArray _lnors_spacearr = {NULL};

	/* Temp normal stack. */
	BLI_SMALLSTACK_DECLARE(normal, float *);
	/* Temp clnors stack. */
	BLI_SMALLSTACK_DECLARE(clnors, short *);
	/* Temp edge vectors stack, only used when computing lnor spacearr. */
	BLI_Stack *edge_vectors = NULL;

	{
		char htype = 0;
		if (vcos) {
			htype |= BM_VERT;
		}
		/* Face/Loop indices are set inline below. */
		BM_mesh_elem_index_ensure(bm, htype);
	}

	if (!r_lnors_spacearr && has_clnors) {
		/* We need to compute lnor spacearr if some custom lnor data are given to us! */
		r_lnors_spacearr = &_lnors_spacearr;
	}
	if (r_lnors_spacearr) {
		BKE_lnor_spacearr_init(r_lnors_spacearr, bm->totloop);
		edge_vectors = BLI_stack_new(sizeof(float[3]), __func__);
	}

	/* Clear all loops' tags (means none are to be skipped for now). */
	int index_face, index_loop = 0;
	BM_ITER_MESH_INDEX (f_curr, &fiter, bm, BM_FACES_OF_MESH, index_face) {
		BMLoop *l_curr, *l_first;

		BM_elem_index_set(f_curr, index_face); /* set_inline */

		l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
		do {
			BM_elem_index_set(l_curr, index_loop++); /* set_inline */
			BM_elem_flag_disable(l_curr, BM_ELEM_TAG);
		} while ((l_curr = l_curr->next) != l_first);
	}
	bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

	/* We now know edges that can be smoothed (they are tagged), and edges that will be hard (they aren't).
	 * Now, time to generate the normals.
	 */
	BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_curr, *l_first;

		l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
		do {
			/* A smooth edge, we have to check for cyclic smooth fan case.
			 * If we find a new, never-processed cyclic smooth fan, we can do it now using that loop/edge as
			 * 'entry point', otherwise we can skip it. */
			/* Note: In theory, we could make bm_mesh_loop_check_cyclic_smooth_fan() store mlfan_pivot's in a stack,
			 * to avoid having to fan again around the vert during actual computation of clnor & clnorspace.
			 * However, this would complicate the code, add more memory usage, and BM_vert_step_fan_loop()
			 * is quite cheap in term of CPU cycles, so really think it's not worth it. */
			if (BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
			    (BM_elem_flag_test(l_curr, BM_ELEM_TAG) || !bm_mesh_loop_check_cyclic_smooth_fan(l_curr)))
			{
			}
			else if (!BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
			         !BM_elem_flag_test(l_curr->prev->e, BM_ELEM_TAG))
			{
				/* Simple case (both edges around that vertex are sharp in related polygon),
				 * this vertex just takes its poly normal.
				 */
				const int l_curr_index = BM_elem_index_get(l_curr);
				const float *no = fnos ? fnos[BM_elem_index_get(f_curr)] : f_curr->no;
				copy_v3_v3(r_lnos[l_curr_index], no);

				/* If needed, generate this (simple!) lnor space. */
				if (r_lnors_spacearr) {
					float vec_curr[3], vec_prev[3];
					MLoopNorSpace *lnor_space = BKE_lnor_space_create(r_lnors_spacearr);

					{
						const BMVert *v_pivot = l_curr->v;
						const float *co_pivot = vcos ? vcos[BM_elem_index_get(v_pivot)] : v_pivot->co;
						const BMVert *v_1 = BM_edge_other_vert(l_curr->e, v_pivot);
						const float *co_1 = vcos ? vcos[BM_elem_index_get(v_1)] : v_1->co;
						const BMVert *v_2 = BM_edge_other_vert(l_curr->prev->e, v_pivot);
						const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

						sub_v3_v3v3(vec_curr, co_1, co_pivot);
						normalize_v3(vec_curr);
						sub_v3_v3v3(vec_prev, co_2, co_pivot);
						normalize_v3(vec_prev);
					}

					BKE_lnor_space_define(lnor_space, r_lnos[l_curr_index], vec_curr, vec_prev, NULL);
					/* We know there is only one loop in this space, no need to create a linklist in this case... */
					BKE_lnor_space_add_loop(r_lnors_spacearr, lnor_space, l_curr_index, false);

					if (has_clnors) {
						short (*clnor)[2] = clnors_data ? &clnors_data[l_curr_index] :
						                                  BM_ELEM_CD_GET_VOID_P(l_curr, cd_loop_clnors_offset);
						BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor, r_lnos[l_curr_index]);
					}
				}
			}
			/* We *do not need* to check/tag loops as already computed!
			 * Due to the fact a loop only links to one of its two edges, a same fan *will never be walked more than
			 * once!*
			 * Since we consider edges having neighbor faces with inverted (flipped) normals as sharp, we are sure that
			 * no fan will be skipped, even only considering the case (sharp curr_edge, smooth prev_edge), and not the
			 * alternative (smooth curr_edge, sharp prev_edge).
			 * All this due/thanks to link between normals and loop ordering.
			 */
			else {
				/* We have to fan around current vertex, until we find the other non-smooth edge,
				 * and accumulate face normals into the vertex!
				 * Note in case this vertex has only one sharp edge, this is a waste because the normal is the same as
				 * the vertex normal, but I do not see any easy way to detect that (would need to count number
				 * of sharp edges per vertex, I doubt the additional memory usage would be worth it, especially as
				 * it should not be a common case in real-life meshes anyway).
				 */
				BMVert *v_pivot = l_curr->v;
				BMEdge *e_next;
				const BMEdge *e_org = l_curr->e;
				BMLoop *lfan_pivot, *lfan_pivot_next;
				int lfan_pivot_index;
				float lnor[3] = {0.0f, 0.0f, 0.0f};
				float vec_curr[3], vec_next[3], vec_org[3];

				/* We validate clnors data on the fly - cheapest way to do! */
				int clnors_avg[2] = {0, 0};
				short (*clnor_ref)[2] = NULL;
				int clnors_nbr = 0;
				bool clnors_invalid = false;

				const float *co_pivot = vcos ? vcos[BM_elem_index_get(v_pivot)] : v_pivot->co;

				MLoopNorSpace *lnor_space = r_lnors_spacearr ? BKE_lnor_space_create(r_lnors_spacearr) : NULL;

				BLI_assert((edge_vectors == NULL) || BLI_stack_is_empty(edge_vectors));

				lfan_pivot = l_curr;
				lfan_pivot_index = BM_elem_index_get(lfan_pivot);
				e_next = lfan_pivot->e;  /* Current edge here, actually! */

				/* Only need to compute previous edge's vector once, then we can just reuse old current one! */
				{
					const BMVert *v_2 = BM_edge_other_vert(e_next, v_pivot);
					const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

					sub_v3_v3v3(vec_org, co_2, co_pivot);
					normalize_v3(vec_org);
					copy_v3_v3(vec_curr, vec_org);

					if (r_lnors_spacearr) {
						BLI_stack_push(edge_vectors, vec_org);
					}
				}

				while (true) {
					/* Much simpler than in sibling code with basic Mesh data! */
					lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot, &e_next);
					if (lfan_pivot_next) {
						BLI_assert(lfan_pivot_next->v == v_pivot);
					}
					else {
						/* next edge is non-manifold, we have to find it ourselves! */
						e_next = (lfan_pivot->e == e_next) ? lfan_pivot->prev->e : lfan_pivot->e;
					}

					/* Compute edge vector.
					 * NOTE: We could pre-compute those into an array, in the first iteration, instead of computing them
					 *       twice (or more) here. However, time gained is not worth memory and time lost,
					 *       given the fact that this code should not be called that much in real-life meshes...
					 */
					{
						const BMVert *v_2 = BM_edge_other_vert(e_next, v_pivot);
						const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

						sub_v3_v3v3(vec_next, co_2, co_pivot);
						normalize_v3(vec_next);
					}

					{
						/* Code similar to accumulate_vertex_normals_poly. */
						/* Calculate angle between the two poly edges incident on this vertex. */
						const BMFace *f = lfan_pivot->f;
						const float fac = saacos(dot_v3v3(vec_next, vec_curr));
						const float *no = fnos ? fnos[BM_elem_index_get(f)] : f->no;
						/* Accumulate */
						madd_v3_v3fl(lnor, no, fac);

						if (has_clnors) {
							/* Accumulate all clnors, if they are not all equal we have to fix that! */
							short (*clnor)[2] = clnors_data ? &clnors_data[lfan_pivot_index] :
							                                  BM_ELEM_CD_GET_VOID_P(lfan_pivot, cd_loop_clnors_offset);
							if (clnors_nbr) {
								clnors_invalid |= ((*clnor_ref)[0] != (*clnor)[0] || (*clnor_ref)[1] != (*clnor)[1]);
							}
							else {
								clnor_ref = clnor;
							}
							clnors_avg[0] += (*clnor)[0];
							clnors_avg[1] += (*clnor)[1];
							clnors_nbr++;
							/* We store here a pointer to all custom lnors processed. */
							BLI_SMALLSTACK_PUSH(clnors, (short *)*clnor);
						}
					}

					/* We store here a pointer to all loop-normals processed. */
					BLI_SMALLSTACK_PUSH(normal, (float *)r_lnos[lfan_pivot_index]);

					if (r_lnors_spacearr) {
						/* Assign current lnor space to current 'vertex' loop. */
						BKE_lnor_space_add_loop(r_lnors_spacearr, lnor_space, lfan_pivot_index, true);
						if (e_next != e_org) {
							/* We store here all edges-normalized vectors processed. */
							BLI_stack_push(edge_vectors, vec_next);
						}
					}

					if (!BM_elem_flag_test(e_next, BM_ELEM_TAG) || (e_next == e_org)) {
						/* Next edge is sharp, we have finished with this fan of faces around this vert! */
						break;
					}

					/* Copy next edge vector to current one. */
					copy_v3_v3(vec_curr, vec_next);
					/* Next pivot loop to current one. */
					lfan_pivot = lfan_pivot_next;
					lfan_pivot_index = BM_elem_index_get(lfan_pivot);
				}

				{
					float lnor_len = normalize_v3(lnor);

					/* If we are generating lnor spacearr, we can now define the one for this fan. */
					if (r_lnors_spacearr) {
						if (UNLIKELY(lnor_len == 0.0f)) {
							/* Use vertex normal as fallback! */
							copy_v3_v3(lnor, r_lnos[lfan_pivot_index]);
							lnor_len = 1.0f;
						}

						BKE_lnor_space_define(lnor_space, lnor, vec_org, vec_next, edge_vectors);

						if (has_clnors) {
							if (clnors_invalid) {
								short *clnor;

								clnors_avg[0] /= clnors_nbr;
								clnors_avg[1] /= clnors_nbr;
								/* Fix/update all clnors of this fan with computed average value. */
								printf("Invalid clnors in this fan!\n");
								while ((clnor = BLI_SMALLSTACK_POP(clnors))) {
									//print_v2("org clnor", clnor);
									clnor[0] = (short)clnors_avg[0];
									clnor[1] = (short)clnors_avg[1];
								}
								//print_v2("new clnors", clnors_avg);
							}
							else {
								/* We still have to consume the stack! */
								while (BLI_SMALLSTACK_POP(clnors));
							}
							BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor_ref, lnor);
						}
					}

					/* In case we get a zero normal here, just use vertex normal already set! */
					if (LIKELY(lnor_len != 0.0f)) {
						/* Copy back the final computed normal into all related loop-normals. */
						float *nor;

						while ((nor = BLI_SMALLSTACK_POP(normal))) {
							copy_v3_v3(nor, lnor);
						}
					}
					else {
						/* We still have to consume the stack! */
						while (BLI_SMALLSTACK_POP(normal));
					}
				}

				/* Tag related vertex as sharp, to avoid fanning around it again (in case it was a smooth one). */
				if (r_lnors_spacearr) {
					BM_elem_flag_enable(l_curr->v, BM_ELEM_TAG);
				}
			}
		} while ((l_curr = l_curr->next) != l_first);
	}

	if (r_lnors_spacearr) {
		BLI_stack_free(edge_vectors);
		if (r_lnors_spacearr == &_lnors_spacearr) {
			BKE_lnor_spacearr_free(r_lnors_spacearr);
		}
	}
}

static void bm_mesh_loops_calc_normals_no_autosmooth(
        BMesh *bm, const float (*vnos)[3], const float (*fnos)[3], float (*r_lnos)[3])
{
	BMIter fiter;
	BMFace *f_curr;

	{
		char htype = BM_LOOP;
		if (vnos) {
			htype |= BM_VERT;
		}
		if (fnos) {
			htype |= BM_FACE;
		}
		BM_mesh_elem_index_ensure(bm, htype);
	}

	BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_curr, *l_first;
		const bool is_face_flat = !BM_elem_flag_test(f_curr, BM_ELEM_SMOOTH);

		l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
		do {
			const float *no = is_face_flat ? (fnos ? fnos[BM_elem_index_get(f_curr)] : f_curr->no) :
			                                 (vnos ? vnos[BM_elem_index_get(l_curr->v)] : l_curr->v->no);
			copy_v3_v3(r_lnos[BM_elem_index_get(l_curr)], no);

		} while ((l_curr = l_curr->next) != l_first);
	}
}

#if 0  /* Unused currently */
/**
 * \brief BMesh Compute Loop Normals
 *
 * Updates the loop normals of a mesh. Assumes vertex and face normals are valid (else call BM_mesh_normals_update()
 * first)!
 */
void BM_mesh_loop_normals_update(
        BMesh *bm, const bool use_split_normals, const float split_angle, float (*r_lnos)[3],
        MLoopNorSpaceArray *r_lnors_spacearr, short (*clnors_data)[2], const int cd_loop_clnors_offset)
{
	const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

	if (use_split_normals) {
	        /* Tag smooth edges and set lnos from vnos when they might be completely smooth...
	         * When using custom loop normals, disable the angle feature! */
	        bm_mesh_edges_sharp_tag(bm, NULL, NULL, has_clnors ? (float)M_PI : split_angle, r_lnos);

	        /* Finish computing lnos by accumulating face normals in each fan of faces defined by sharp edges. */
	        bm_mesh_loops_calc_normals(bm, NULL, NULL, r_lnos, r_lnors_spacearr, clnors_data, cd_loop_clnors_offset);
	}
	else {
		BLI_assert(!r_lnors_spacearr);
		bm_mesh_loops_calc_normals_no_autosmooth(bm, NULL, NULL, r_lnos);
	}
}
#endif

/**
 * \brief BMesh Compute Loop Normals from/to external data.
 *
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry (splitting edges).
 */
void BM_loops_calc_normal_vcos(
        BMesh *bm, const float (*vcos)[3], const float (*vnos)[3], const float (*fnos)[3],
        const bool use_split_normals, const float split_angle, float (*r_lnos)[3],
        MLoopNorSpaceArray *r_lnors_spacearr, short (*clnors_data)[2], const int cd_loop_clnors_offset)
{
	const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

	if (use_split_normals) {
		/* Tag smooth edges and set lnos from vnos when they might be completely smooth...
		 * When using custom loop normals, disable the angle feature! */
		bm_mesh_edges_sharp_tag(bm, vnos, fnos, has_clnors ? (float)M_PI : split_angle, r_lnos);

		/* Finish computing lnos by accumulating face normals in each fan of faces defined by sharp edges. */
		bm_mesh_loops_calc_normals(bm, vcos, fnos, r_lnos, r_lnors_spacearr, clnors_data, cd_loop_clnors_offset);
	}
	else {
		BLI_assert(!r_lnors_spacearr);
		bm_mesh_loops_calc_normals_no_autosmooth(bm, vnos, fnos, r_lnos);
	}
}

static void UNUSED_FUNCTION(bm_mdisps_space_set)(Object *ob, BMesh *bm, int from, int to)
{
	/* switch multires data out of tangent space */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		BMEditMesh *em = BKE_editmesh_create(bm, false);
		DerivedMesh *dm = CDDM_from_editbmesh(em, true, false);
		MDisps *mdisps;
		BMFace *f;
		BMIter iter;
		// int i = 0; // UNUSED
		
		multires_set_space(dm, ob, from, to);
		
		mdisps = CustomData_get_layer(&dm->loopData, CD_MDISPS);
		
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			BMLoop *l;
			BMIter liter;
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				MDisps *lmd = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_MDISPS);
				
				if (!lmd->disps) {
					printf("%s: warning - 'lmd->disps' == NULL\n", __func__);
				}
				
				if (lmd->disps && lmd->totdisp == mdisps->totdisp) {
					memcpy(lmd->disps, mdisps->disps, sizeof(float) * 3 * lmd->totdisp);
				}
				else if (mdisps->disps) {
					if (lmd->disps)
						MEM_freeN(lmd->disps);
					
					lmd->disps = MEM_dupallocN(mdisps->disps);
					lmd->totdisp = mdisps->totdisp;
					lmd->level = mdisps->level;
				}
				
				mdisps++;
				// i += 1;
			}
		}
		
		dm->needsFree = 1;
		dm->release(dm);
		
		/* setting this to NULL prevents BKE_editmesh_free from freeing it */
		em->bm = NULL;
		BKE_editmesh_free(em);
		MEM_freeN(em);
	}
}

/**
 * \brief BMesh Begin Edit
 *
 * Functions for setting up a mesh for editing and cleaning up after
 * the editing operations are done. These are called by the tools/operator
 * API for each time a tool is executed.
 */
void bmesh_edit_begin(BMesh *UNUSED(bm), BMOpTypeFlag UNUSED(type_flag))
{
	/* Most operators seem to be using BMO_OPTYPE_FLAG_UNTAN_MULTIRES to change the MDisps to
	 * absolute space during mesh edits. With this enabled, changes to the topology
	 * (loop cuts, edge subdivides, etc) are not reflected in the higher levels of
	 * the mesh at all, which doesn't seem right. Turning off completely for now,
	 * until this is shown to be better for certain types of mesh edits. */
#ifdef BMOP_UNTAN_MULTIRES_ENABLED
	/* switch multires data out of tangent space */
	if ((type_flag & BMO_OPTYPE_FLAG_UNTAN_MULTIRES) && CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		bmesh_mdisps_space_set(bm, MULTIRES_SPACE_TANGENT, MULTIRES_SPACE_ABSOLUTE);

		/* ensure correct normals, if possible */
		bmesh_rationalize_normals(bm, 0);
		BM_mesh_normals_update(bm);
	}
#endif
}

/**
 * \brief BMesh End Edit
 */
void bmesh_edit_end(BMesh *bm, BMOpTypeFlag type_flag)
{
	ListBase select_history;

	/* BMO_OPTYPE_FLAG_UNTAN_MULTIRES disabled for now, see comment above in bmesh_edit_begin. */
#ifdef BMOP_UNTAN_MULTIRES_ENABLED
	/* switch multires data into tangent space */
	if ((flag & BMO_OPTYPE_FLAG_UNTAN_MULTIRES) && CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		/* set normals to their previous winding */
		bmesh_rationalize_normals(bm, 1);
		bmesh_mdisps_space_set(bm, MULTIRES_SPACE_ABSOLUTE, MULTIRES_SPACE_TANGENT);
	}
	else if (flag & BMO_OP_FLAG_RATIONALIZE_NORMALS) {
		bmesh_rationalize_normals(bm, 1);
	}
#endif

	/* compute normals, clear temp flags and flush selections */
	if (type_flag & BMO_OPTYPE_FLAG_NORMALS_CALC) {
		BM_mesh_normals_update(bm);
	}


	if ((type_flag & BMO_OPTYPE_FLAG_SELECT_VALIDATE) == 0) {
		select_history = bm->selected;
		BLI_listbase_clear(&bm->selected);
	}

	if (type_flag & BMO_OPTYPE_FLAG_SELECT_FLUSH) {
		BM_mesh_select_mode_flush(bm);
	}

	if ((type_flag & BMO_OPTYPE_FLAG_SELECT_VALIDATE) == 0) {
		bm->selected = select_history;
	}
}

void BM_mesh_elem_index_ensure(BMesh *bm, const char htype)
{
	const char htype_needed = bm->elem_index_dirty & htype;

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "Should Never Fail!", __func__);
#endif

	if (htype_needed == 0) {
		goto finally;
	}

	/* skip if we only need to operate on one element */
#pragma omp parallel sections if ((!ELEM(htype_needed, BM_VERT, BM_EDGE, BM_FACE, BM_LOOP, BM_FACE | BM_LOOP)) && \
	                              (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT))
	{
#pragma omp section

		{
			if (htype & BM_VERT) {
				if (bm->elem_index_dirty & BM_VERT) {
					BMIter iter;
					BMElem *ele;

					int index;
					BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, index) {
						BM_elem_index_set(ele, index); /* set_ok */
					}
					BLI_assert(index == bm->totvert);
				}
				else {
					// printf("%s: skipping vert index calc!\n", __func__);
				}
			}
		}

#pragma omp section
		{
			if (htype & BM_EDGE) {
				if (bm->elem_index_dirty & BM_EDGE) {
					BMIter iter;
					BMElem *ele;

					int index;
					BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, index) {
						BM_elem_index_set(ele, index); /* set_ok */
					}
					BLI_assert(index == bm->totedge);
				}
				else {
					// printf("%s: skipping edge index calc!\n", __func__);
				}
			}
		}

#pragma omp section
		{
			if (htype & (BM_FACE | BM_LOOP)) {
				if (bm->elem_index_dirty & (BM_FACE | BM_LOOP)) {
					BMIter iter;
					BMElem *ele;

					const bool update_face = (htype & BM_FACE) && (bm->elem_index_dirty & BM_FACE);
					const bool update_loop = (htype & BM_LOOP) && (bm->elem_index_dirty & BM_LOOP);

					int index;
					int index_loop = 0;

					BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, index) {
						if (update_face) {
							BM_elem_index_set(ele, index); /* set_ok */
						}

						if (update_loop) {
							BMLoop *l_iter, *l_first;

							l_iter = l_first = BM_FACE_FIRST_LOOP((BMFace *)ele);
							do {
								BM_elem_index_set(l_iter, index_loop++); /* set_ok */
							} while ((l_iter = l_iter->next) != l_first);
						}
					}

					BLI_assert(index == bm->totface);
					if (update_loop) {
						BLI_assert(index_loop == bm->totloop);
					}
				}
				else {
					// printf("%s: skipping face/loop index calc!\n", __func__);
				}
			}
		}
	}


finally:
	bm->elem_index_dirty &= ~htype;
}


/**
 * Array checking/setting macros
 *
 * Currently vert/edge/loop/face index data is being abused, in a few areas of the code.
 *
 * To avoid correcting them afterwards, set 'bm->elem_index_dirty' however its possible
 * this flag is set incorrectly which could crash blender.
 *
 * These functions ensure its correct and are called more often in debug mode.
 */

void BM_mesh_elem_index_validate(
        BMesh *bm, const char *location, const char *func,
        const char *msg_a, const char *msg_b)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};
	const char *type_names[3] = {"vert", "edge", "face"};

	BMIter iter;
	BMElem *ele;
	int i;
	bool is_any_error = 0;

	for (i = 0; i < 3; i++) {
		const bool is_dirty = (flag_types[i] & bm->elem_index_dirty) != 0;
		int index = 0;
		bool is_error = false;
		int err_val = 0;
		int err_idx = 0;

		BM_ITER_MESH (ele, &iter, bm, iter_types[i]) {
			if (!is_dirty) {
				if (BM_elem_index_get(ele) != index) {
					err_val = BM_elem_index_get(ele);
					err_idx = index;
					is_error = true;
				}
			}

			BM_elem_index_set(ele, index); /* set_ok */
			index++;
		}

		if ((is_error == true) && (is_dirty == false)) {
			is_any_error = true;
			fprintf(stderr,
			        "Invalid Index: at %s, %s, %s[%d] invalid index %d, '%s', '%s'\n",
			        location, func, type_names[i], err_idx, err_val, msg_a, msg_b);
		}
		else if ((is_error == false) && (is_dirty == true)) {

#if 0       /* mostly annoying */

			/* dirty may have been incorrectly set */
			fprintf(stderr,
			        "Invalid Dirty: at %s, %s (%s), dirty flag was set but all index values are correct, '%s', '%s'\n",
			        location, func, type_names[i], msg_a, msg_b);
#endif
		}
	}

#if 0 /* mostly annoying, even in debug mode */
#ifdef DEBUG
	if (is_any_error == 0) {
		fprintf(stderr,
		        "Valid Index Success: at %s, %s, '%s', '%s'\n",
		        location, func, msg_a, msg_b);
	}
#endif
#endif
	(void) is_any_error; /* shut up the compiler */

}

/* debug check only - no need to optimize */
#ifndef NDEBUG
bool BM_mesh_elem_table_check(BMesh *bm)
{
	BMIter iter;
	BMElem *ele;
	int i;

	if (bm->vtable && ((bm->elem_table_dirty & BM_VERT) == 0)) {
		BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (ele != (BMElem *)bm->vtable[i]) {
				return false;
			}
		}
	}

	if (bm->etable && ((bm->elem_table_dirty & BM_EDGE) == 0)) {
		BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (ele != (BMElem *)bm->etable[i]) {
				return false;
			}
		}
	}

	if (bm->ftable && ((bm->elem_table_dirty & BM_FACE) == 0)) {
		BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, i) {
			if (ele != (BMElem *)bm->ftable[i]) {
				return false;
			}
		}
	}

	return true;
}
#endif



void BM_mesh_elem_table_ensure(BMesh *bm, const char htype)
{
	/* assume if the array is non-null then its valid and no need to recalc */
	const char htype_needed = (((bm->vtable && ((bm->elem_table_dirty & BM_VERT) == 0)) ? 0 : BM_VERT) |
	                           ((bm->etable && ((bm->elem_table_dirty & BM_EDGE) == 0)) ? 0 : BM_EDGE) |
	                           ((bm->ftable && ((bm->elem_table_dirty & BM_FACE) == 0)) ? 0 : BM_FACE)) & htype;

	BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

	/* in debug mode double check we didn't need to recalculate */
	BLI_assert(BM_mesh_elem_table_check(bm) == true);

	if (htype_needed == 0) {
		goto finally;
	}

	if (htype_needed & BM_VERT) {
		if (bm->vtable && bm->totvert <= bm->vtable_tot && bm->totvert * 2 >= bm->vtable_tot) {
			/* pass (re-use the array) */
		}
		else {
			if (bm->vtable)
				MEM_freeN(bm->vtable);
			bm->vtable = MEM_mallocN(sizeof(void **) * bm->totvert, "bm->vtable");
			bm->vtable_tot = bm->totvert;
		}
	}
	if (htype_needed & BM_EDGE) {
		if (bm->etable && bm->totedge <= bm->etable_tot && bm->totedge * 2 >= bm->etable_tot) {
			/* pass (re-use the array) */
		}
		else {
			if (bm->etable)
				MEM_freeN(bm->etable);
			bm->etable = MEM_mallocN(sizeof(void **) * bm->totedge, "bm->etable");
			bm->etable_tot = bm->totedge;
		}
	}
	if (htype_needed & BM_FACE) {
		if (bm->ftable && bm->totface <= bm->ftable_tot && bm->totface * 2 >= bm->ftable_tot) {
			/* pass (re-use the array) */
		}
		else {
			if (bm->ftable)
				MEM_freeN(bm->ftable);
			bm->ftable = MEM_mallocN(sizeof(void **) * bm->totface, "bm->ftable");
			bm->ftable_tot = bm->totface;
		}
	}

	/* skip if we only need to operate on one element */
#pragma omp parallel sections if ((!ELEM(htype_needed, BM_VERT, BM_EDGE, BM_FACE)) && \
	                              (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT))
	{
#pragma omp section
		{
			if (htype_needed & BM_VERT) {
				BM_iter_as_array(bm, BM_VERTS_OF_MESH, NULL, (void **)bm->vtable, bm->totvert);
			}
		}
#pragma omp section
		{
			if (htype_needed & BM_EDGE) {
				BM_iter_as_array(bm, BM_EDGES_OF_MESH, NULL, (void **)bm->etable, bm->totedge);
			}
		}
#pragma omp section
		{
			if (htype_needed & BM_FACE) {
				BM_iter_as_array(bm, BM_FACES_OF_MESH, NULL, (void **)bm->ftable, bm->totface);
			}
		}
	}

finally:
	/* Only clear dirty flags when all the pointers and data are actually valid.
	 * This prevents possible threading issues when dirty flag check failed but
	 * data wasn't ready still.
	 */
	bm->elem_table_dirty &= ~htype_needed;
}

/* use BM_mesh_elem_table_ensure where possible to avoid full rebuild */
void BM_mesh_elem_table_init(BMesh *bm, const char htype)
{
	BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

	/* force recalc */
	BM_mesh_elem_table_free(bm, BM_ALL_NOLOOP);
	BM_mesh_elem_table_ensure(bm, htype);
}

void BM_mesh_elem_table_free(BMesh *bm, const char htype)
{
	if (htype & BM_VERT) {
		MEM_SAFE_FREE(bm->vtable);
	}

	if (htype & BM_EDGE) {
		MEM_SAFE_FREE(bm->etable);
	}

	if (htype & BM_FACE) {
		MEM_SAFE_FREE(bm->ftable);
	}
}

BMVert *BM_vert_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totvert));
	BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
	return bm->vtable[index];
}

BMEdge *BM_edge_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totedge));
	BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
	return bm->etable[index];
}

BMFace *BM_face_at_index(BMesh *bm, const int index)
{
	BLI_assert((index >= 0) && (index < bm->totface));
	BLI_assert((bm->elem_table_dirty & BM_FACE) == 0);
	return bm->ftable[index];
}


BMVert *BM_vert_at_index_find(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->vpool, index);
}

BMEdge *BM_edge_at_index_find(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->epool, index);
}

BMFace *BM_face_at_index_find(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->fpool, index);
}

/**
 * Use lookup table when available, else use slower find functions.
 *
 * \note Try to use #BM_mesh_elem_table_ensure instead.
 */
BMVert *BM_vert_at_index_find_or_table(BMesh *bm, const int index)
{
	if ((bm->elem_table_dirty & BM_VERT) == 0) {
		return (index < bm->totvert) ? bm->vtable[index] : NULL;
	}
	else {
		return BM_vert_at_index_find(bm, index);
	}
}

BMEdge *BM_edge_at_index_find_or_table(BMesh *bm, const int index)
{
	if ((bm->elem_table_dirty & BM_EDGE) == 0) {
		return (index < bm->totedge) ? bm->etable[index] : NULL;
	}
	else {
		return BM_edge_at_index_find(bm, index);
	}
}

BMFace *BM_face_at_index_find_or_table(BMesh *bm, const int index)
{
	if ((bm->elem_table_dirty & BM_FACE) == 0) {
		return (index < bm->totface) ? bm->ftable[index] : NULL;
	}
	else {
		return BM_face_at_index_find(bm, index);
	}
}


/**
 * Return the amount of element of type 'type' in a given bmesh.
 */
int BM_mesh_elem_count(BMesh *bm, const char htype)
{
	BLI_assert((htype & ~BM_ALL_NOLOOP) == 0);

	switch (htype) {
		case BM_VERT: return bm->totvert;
		case BM_EDGE: return bm->totedge;
		case BM_FACE: return bm->totface;
		default:
		{
			BLI_assert(0);
			return 0;
		}
	}
}


/**
 * Remaps the vertices, edges and/or faces of the bmesh as indicated by vert/edge/face_idx arrays
 * (xxx_idx[org_index] = new_index).
 *
 * A NULL array means no changes.
 *
 * Note: - Does not mess with indices, just sets elem_index_dirty flag.
 *       - For verts/edges/faces only (as loops must remain "ordered" and "aligned"
 *         on a per-face basis...).
 *
 * WARNING: Be careful if you keep pointers to affected BM elements, or arrays, when using this func!
 */
void BM_mesh_remap(
        BMesh *bm,
        const uint *vert_idx,
        const uint *edge_idx,
        const uint *face_idx)
{
	/* Mapping old to new pointers. */
	GHash *vptr_map = NULL, *eptr_map = NULL, *fptr_map = NULL;
	BMIter iter, iterl;
	BMVert *ve;
	BMEdge *ed;
	BMFace *fa;
	BMLoop *lo;

	if (!(vert_idx || edge_idx || face_idx))
		return;

	BM_mesh_elem_table_ensure(
	        bm,
	        (vert_idx ? BM_VERT : 0) |
	        (edge_idx ? BM_EDGE : 0) |
	        (face_idx ? BM_FACE : 0));

	/* Remap Verts */
	if (vert_idx) {
		BMVert **verts_pool, *verts_copy, **vep;
		int i, totvert = bm->totvert;
		const uint *new_idx;
		/* Special case: Python uses custom - data layers to hold PyObject references.
		 * These have to be kept in - place, else the PyObject's we point to, wont point back to us. */
		const int cd_vert_pyptr  = CustomData_get_offset(&bm->vdata, CD_BM_ELEM_PYPTR);

		/* Init the old-to-new vert pointers mapping */
		vptr_map = BLI_ghash_ptr_new_ex("BM_mesh_remap vert pointers mapping", bm->totvert);

		/* Make a copy of all vertices. */
		verts_pool = bm->vtable;
		verts_copy = MEM_mallocN(sizeof(BMVert) * totvert, "BM_mesh_remap verts copy");
		void **pyptrs = (cd_vert_pyptr != -1) ? MEM_mallocN(sizeof(void *) * totvert, __func__) : NULL;
		for (i = totvert, ve = verts_copy + totvert - 1, vep = verts_pool + totvert - 1; i--; ve--, vep--) {
			*ve = **vep;
/*			printf("*vep: %p, verts_pool[%d]: %p\n", *vep, i, verts_pool[i]);*/
			if (cd_vert_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)ve), cd_vert_pyptr);
				pyptrs[i] = *pyptr;
			}
		}

		/* Copy back verts to their new place, and update old2new pointers mapping. */
		new_idx = vert_idx + totvert - 1;
		ve = verts_copy + totvert - 1;
		vep = verts_pool + totvert - 1; /* old, org pointer */
		for (i = totvert; i--; new_idx--, ve--, vep--) {
			BMVert *new_vep = verts_pool[*new_idx];
			*new_vep = *ve;
/*			printf("mapping vert from %d to %d (%p/%p to %p)\n", i, *new_idx, *vep, verts_pool[i], new_vep);*/
			BLI_ghash_insert(vptr_map, *vep, new_vep);
			if (cd_vert_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)new_vep), cd_vert_pyptr);
				*pyptr = pyptrs[*new_idx];
			}
		}
		bm->elem_index_dirty |= BM_VERT;
		bm->elem_table_dirty |= BM_VERT;

		MEM_freeN(verts_copy);
		if (pyptrs) {
			MEM_freeN(pyptrs);
		}
	}

	/* Remap Edges */
	if (edge_idx) {
		BMEdge **edges_pool, *edges_copy, **edp;
		int i, totedge = bm->totedge;
		const uint *new_idx;
		/* Special case: Python uses custom - data layers to hold PyObject references.
		 * These have to be kept in - place, else the PyObject's we point to, wont point back to us. */
		const int cd_edge_pyptr  = CustomData_get_offset(&bm->edata, CD_BM_ELEM_PYPTR);

		/* Init the old-to-new vert pointers mapping */
		eptr_map = BLI_ghash_ptr_new_ex("BM_mesh_remap edge pointers mapping", bm->totedge);

		/* Make a copy of all vertices. */
		edges_pool = bm->etable;
		edges_copy = MEM_mallocN(sizeof(BMEdge) * totedge, "BM_mesh_remap edges copy");
		void **pyptrs = (cd_edge_pyptr != -1) ? MEM_mallocN(sizeof(void *) * totedge, __func__) : NULL;
		for (i = totedge, ed = edges_copy + totedge - 1, edp = edges_pool + totedge - 1; i--; ed--, edp--) {
			*ed = **edp;
			if (cd_edge_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)ed), cd_edge_pyptr);
				pyptrs[i] = *pyptr;
			}
		}

		/* Copy back verts to their new place, and update old2new pointers mapping. */
		new_idx = edge_idx + totedge - 1;
		ed = edges_copy + totedge - 1;
		edp = edges_pool + totedge - 1; /* old, org pointer */
		for (i = totedge; i--; new_idx--, ed--, edp--) {
			BMEdge *new_edp = edges_pool[*new_idx];
			*new_edp = *ed;
			BLI_ghash_insert(eptr_map, *edp, new_edp);
/*			printf("mapping edge from %d to %d (%p/%p to %p)\n", i, *new_idx, *edp, edges_pool[i], new_edp);*/
			if (cd_edge_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)new_edp), cd_edge_pyptr);
				*pyptr = pyptrs[*new_idx];
			}
		}
		bm->elem_index_dirty |= BM_EDGE;
		bm->elem_table_dirty |= BM_EDGE;

		MEM_freeN(edges_copy);
		if (pyptrs) {
			MEM_freeN(pyptrs);
		}
	}

	/* Remap Faces */
	if (face_idx) {
		BMFace **faces_pool, *faces_copy, **fap;
		int i, totface = bm->totface;
		const uint *new_idx;
		/* Special case: Python uses custom - data layers to hold PyObject references.
		 * These have to be kept in - place, else the PyObject's we point to, wont point back to us. */
		const int cd_poly_pyptr  = CustomData_get_offset(&bm->pdata, CD_BM_ELEM_PYPTR);

		/* Init the old-to-new vert pointers mapping */
		fptr_map = BLI_ghash_ptr_new_ex("BM_mesh_remap face pointers mapping", bm->totface);

		/* Make a copy of all vertices. */
		faces_pool = bm->ftable;
		faces_copy = MEM_mallocN(sizeof(BMFace) * totface, "BM_mesh_remap faces copy");
		void **pyptrs = (cd_poly_pyptr != -1) ? MEM_mallocN(sizeof(void *) * totface, __func__) : NULL;
		for (i = totface, fa = faces_copy + totface - 1, fap = faces_pool + totface - 1; i--; fa--, fap--) {
			*fa = **fap;
			if (cd_poly_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)fa), cd_poly_pyptr);
				pyptrs[i] = *pyptr;
			}
		}

		/* Copy back verts to their new place, and update old2new pointers mapping. */
		new_idx = face_idx + totface - 1;
		fa = faces_copy + totface - 1;
		fap = faces_pool + totface - 1; /* old, org pointer */
		for (i = totface; i--; new_idx--, fa--, fap--) {
			BMFace *new_fap = faces_pool[*new_idx];
			*new_fap = *fa;
			BLI_ghash_insert(fptr_map, *fap, new_fap);
			if (cd_poly_pyptr != -1) {
				void **pyptr = BM_ELEM_CD_GET_VOID_P(((BMElem *)new_fap), cd_poly_pyptr);
				*pyptr = pyptrs[*new_idx];
			}
		}

		bm->elem_index_dirty |= BM_FACE | BM_LOOP;
		bm->elem_table_dirty |= BM_FACE;

		MEM_freeN(faces_copy);
		if (pyptrs) {
			MEM_freeN(pyptrs);
		}
	}

	/* And now, fix all vertices/edges/faces/loops pointers! */
	/* Verts' pointers, only edge pointers... */
	if (eptr_map) {
		BM_ITER_MESH (ve, &iter, bm, BM_VERTS_OF_MESH) {
/*			printf("Vert e: %p -> %p\n", ve->e, BLI_ghash_lookup(eptr_map, ve->e));*/
			if (ve->e) {
				ve->e = BLI_ghash_lookup(eptr_map, ve->e);
				BLI_assert(ve->e);
			}
		}
	}

	/* Edges' pointers, only vert pointers (as we don't mess with loops!), and - ack! - edge pointers,
	 * as we have to handle disklinks... */
	if (vptr_map || eptr_map) {
		BM_ITER_MESH (ed, &iter, bm, BM_EDGES_OF_MESH) {
			if (vptr_map) {
/*				printf("Edge v1: %p -> %p\n", ed->v1, BLI_ghash_lookup(vptr_map, ed->v1));*/
/*				printf("Edge v2: %p -> %p\n", ed->v2, BLI_ghash_lookup(vptr_map, ed->v2));*/
				ed->v1 = BLI_ghash_lookup(vptr_map, ed->v1);
				ed->v2 = BLI_ghash_lookup(vptr_map, ed->v2);
				BLI_assert(ed->v1);
				BLI_assert(ed->v2);
			}
			if (eptr_map) {
/*				printf("Edge v1_disk_link prev: %p -> %p\n", ed->v1_disk_link.prev,*/
/*				       BLI_ghash_lookup(eptr_map, ed->v1_disk_link.prev));*/
/*				printf("Edge v1_disk_link next: %p -> %p\n", ed->v1_disk_link.next,*/
/*				       BLI_ghash_lookup(eptr_map, ed->v1_disk_link.next));*/
/*				printf("Edge v2_disk_link prev: %p -> %p\n", ed->v2_disk_link.prev,*/
/*				       BLI_ghash_lookup(eptr_map, ed->v2_disk_link.prev));*/
/*				printf("Edge v2_disk_link next: %p -> %p\n", ed->v2_disk_link.next,*/
/*				       BLI_ghash_lookup(eptr_map, ed->v2_disk_link.next));*/
				ed->v1_disk_link.prev = BLI_ghash_lookup(eptr_map, ed->v1_disk_link.prev);
				ed->v1_disk_link.next = BLI_ghash_lookup(eptr_map, ed->v1_disk_link.next);
				ed->v2_disk_link.prev = BLI_ghash_lookup(eptr_map, ed->v2_disk_link.prev);
				ed->v2_disk_link.next = BLI_ghash_lookup(eptr_map, ed->v2_disk_link.next);
				BLI_assert(ed->v1_disk_link.prev);
				BLI_assert(ed->v1_disk_link.next);
				BLI_assert(ed->v2_disk_link.prev);
				BLI_assert(ed->v2_disk_link.next);
			}
		}
	}

	/* Faces' pointers (loops, in fact), always needed... */
	BM_ITER_MESH (fa, &iter, bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM (lo, &iterl, fa, BM_LOOPS_OF_FACE) {
			if (vptr_map) {
/*				printf("Loop v: %p -> %p\n", lo->v, BLI_ghash_lookup(vptr_map, lo->v));*/
				lo->v = BLI_ghash_lookup(vptr_map, lo->v);
				BLI_assert(lo->v);
			}
			if (eptr_map) {
/*				printf("Loop e: %p -> %p\n", lo->e, BLI_ghash_lookup(eptr_map, lo->e));*/
				lo->e = BLI_ghash_lookup(eptr_map, lo->e);
				BLI_assert(lo->e);
			}
			if (fptr_map) {
/*				printf("Loop f: %p -> %p\n", lo->f, BLI_ghash_lookup(fptr_map, lo->f));*/
				lo->f = BLI_ghash_lookup(fptr_map, lo->f);
				BLI_assert(lo->f);
			}
		}
	}

	/* Selection history */
	{
		BMEditSelection *ese;
		for (ese = bm->selected.first; ese; ese = ese->next) {
			switch (ese->htype) {
				case BM_VERT:
					if (vptr_map) {
						ese->ele = BLI_ghash_lookup(vptr_map, ese->ele);
						BLI_assert(ese->ele);
					}
					break;
				case BM_EDGE:
					if (eptr_map) {
						ese->ele = BLI_ghash_lookup(eptr_map, ese->ele);
						BLI_assert(ese->ele);
					}
					break;
				case BM_FACE:
					if (fptr_map) {
						ese->ele = BLI_ghash_lookup(fptr_map, ese->ele);
						BLI_assert(ese->ele);
					}
					break;
			}
		}
	}

	if (fptr_map) {
		if (bm->act_face) {
			bm->act_face = BLI_ghash_lookup(fptr_map, bm->act_face);
			BLI_assert(bm->act_face);
		}
	}

	if (vptr_map)
		BLI_ghash_free(vptr_map, NULL, NULL);
	if (eptr_map)
		BLI_ghash_free(eptr_map, NULL, NULL);
	if (fptr_map)
		BLI_ghash_free(fptr_map, NULL, NULL);
}

/**
 * Use new memory pools for this mesh.
 *
 * \note needed for re-sizing elements (adding/removing tool flags)
 * but could also be used for packing fragmented bmeshes.
 */
void BM_mesh_rebuild(
        BMesh *bm, const struct BMeshCreateParams *params,
        BLI_mempool *vpool_dst, BLI_mempool *epool_dst, BLI_mempool *lpool_dst, BLI_mempool *fpool_dst)
{
	const char remap =
	        (vpool_dst ? BM_VERT : 0) |
	        (epool_dst ? BM_EDGE : 0) |
	        (lpool_dst ? BM_LOOP : 0) |
	        (fpool_dst ? BM_FACE : 0);

	BMVert **vtable_dst = (remap & BM_VERT) ? MEM_mallocN(bm->totvert * sizeof(BMVert *), __func__) : NULL;
	BMEdge **etable_dst = (remap & BM_EDGE) ? MEM_mallocN(bm->totedge * sizeof(BMEdge *), __func__) : NULL;
	BMLoop **ltable_dst = (remap & BM_LOOP) ? MEM_mallocN(bm->totloop * sizeof(BMLoop *), __func__) : NULL;
	BMFace **ftable_dst = (remap & BM_FACE) ? MEM_mallocN(bm->totface * sizeof(BMFace *), __func__) : NULL;

	const bool use_toolflags = params->use_toolflags;

	if (remap & BM_VERT) {
		BMIter iter;
		int index;
		BMVert *v_src;
		BM_ITER_MESH_INDEX (v_src, &iter, bm, BM_VERTS_OF_MESH, index) {
			BMVert *v_dst = BLI_mempool_alloc(vpool_dst);
			memcpy(v_dst, v_src, sizeof(BMVert));
			if (use_toolflags) {
				((BMVert_OFlag *)v_dst)->oflags = bm->vtoolflagpool ? BLI_mempool_calloc(bm->vtoolflagpool) : NULL;
			}

			vtable_dst[index] = v_dst;
			BM_elem_index_set(v_src, index);  /* set_ok */
		}
	}

	if (remap & BM_EDGE) {
		BMIter iter;
		int index;
		BMEdge *e_src;
		BM_ITER_MESH_INDEX (e_src, &iter, bm, BM_EDGES_OF_MESH, index) {
			BMEdge *e_dst = BLI_mempool_alloc(epool_dst);
			memcpy(e_dst, e_src, sizeof(BMEdge));
			if (use_toolflags) {
				((BMEdge_OFlag *)e_dst)->oflags = bm->etoolflagpool ? BLI_mempool_calloc(bm->etoolflagpool) : NULL;
			}

			etable_dst[index] = e_dst;
			BM_elem_index_set(e_src, index);  /* set_ok */
		}
	}

	if (remap & (BM_LOOP | BM_FACE)) {
		BMIter iter;
		int index, index_loop = 0;
		BMFace *f_src;
		BM_ITER_MESH_INDEX (f_src, &iter, bm, BM_FACES_OF_MESH, index) {

			if (remap & BM_FACE) {
				BMFace *f_dst = BLI_mempool_alloc(fpool_dst);
				memcpy(f_dst, f_src, sizeof(BMFace));
				if (use_toolflags) {
					((BMFace_OFlag *)f_dst)->oflags = bm->ftoolflagpool ? BLI_mempool_calloc(bm->ftoolflagpool) : NULL;
				}

				ftable_dst[index] = f_dst;
				BM_elem_index_set(f_src, index);  /* set_ok */
			}

			/* handle loops */
			if (remap & BM_LOOP) {
				BMLoop *l_iter_src, *l_first_src;
				l_iter_src = l_first_src = BM_FACE_FIRST_LOOP((BMFace *)f_src);
				do {
					BMLoop *l_dst = BLI_mempool_alloc(lpool_dst);
					memcpy(l_dst, l_iter_src, sizeof(BMLoop));
					ltable_dst[index_loop] = l_dst;
					BM_elem_index_set(l_iter_src, index_loop++); /* set_ok */
				} while ((l_iter_src = l_iter_src->next) != l_first_src);
			}
		}
	}

#define MAP_VERT(ele) vtable_dst[BM_elem_index_get(ele)]
#define MAP_EDGE(ele) etable_dst[BM_elem_index_get(ele)]
#define MAP_LOOP(ele) ltable_dst[BM_elem_index_get(ele)]
#define MAP_FACE(ele) ftable_dst[BM_elem_index_get(ele)]

#define REMAP_VERT(ele) { if (remap & BM_VERT) { ele = MAP_VERT(ele); }} ((void)0)
#define REMAP_EDGE(ele) { if (remap & BM_EDGE) { ele = MAP_EDGE(ele); }} ((void)0)
#define REMAP_LOOP(ele) { if (remap & BM_LOOP) { ele = MAP_LOOP(ele); }} ((void)0)
#define REMAP_FACE(ele) { if (remap & BM_FACE) { ele = MAP_FACE(ele); }} ((void)0)

	/* verts */
	{
		for (int i = 0; i < bm->totvert; i++) {
			BMVert *v = vtable_dst[i];
			if (v->e) {
				REMAP_EDGE(v->e);
			}
		}
	}

	/* edges */
	{
		for (int i = 0; i < bm->totedge; i++) {
			BMEdge *e = etable_dst[i];
			REMAP_VERT(e->v1);
			REMAP_VERT(e->v2);
			REMAP_EDGE(e->v1_disk_link.next);
			REMAP_EDGE(e->v1_disk_link.prev);
			REMAP_EDGE(e->v2_disk_link.next);
			REMAP_EDGE(e->v2_disk_link.prev);
			if (e->l) {
				REMAP_LOOP(e->l);
			}
		}
	}

	/* faces */
	{
		for (int i = 0; i < bm->totface; i++) {
			BMFace *f = ftable_dst[i];
			REMAP_LOOP(f->l_first);

			{
				BMLoop *l_iter, *l_first;
				l_iter = l_first = BM_FACE_FIRST_LOOP((BMFace *)f);
				do {
					REMAP_VERT(l_iter->v);
					REMAP_EDGE(l_iter->e);
					REMAP_FACE(l_iter->f);

					REMAP_LOOP(l_iter->radial_next);
					REMAP_LOOP(l_iter->radial_prev);
					REMAP_LOOP(l_iter->next);
					REMAP_LOOP(l_iter->prev);
				} while ((l_iter = l_iter->next) != l_first);
			}
		}
	}

	for (BMEditSelection *ese = bm->selected.first; ese; ese = ese->next) {
		switch (ese->htype) {
			case BM_VERT:
				if (remap & BM_VERT) {
					ese->ele = (BMElem *)MAP_VERT(ese->ele);
				}
				break;
			case BM_EDGE:
				if (remap & BM_EDGE) {
					ese->ele = (BMElem *)MAP_EDGE(ese->ele);
				}
				break;
			case BM_FACE:
				if (remap & BM_FACE) {
					ese->ele = (BMElem *)MAP_FACE(ese->ele);
				}
				break;
		}
	}

	if (bm->act_face) {
		REMAP_FACE(bm->act_face);
	}

#undef MAP_VERT
#undef MAP_EDGE
#undef MAP_LOOP
#undef MAP_EDGE

#undef REMAP_VERT
#undef REMAP_EDGE
#undef REMAP_LOOP
#undef REMAP_EDGE

	/* Cleanup, re-use local tables if the current mesh had tables allocated.
	 * could use irrespective but it may use more memory then the caller wants (and not be needed). */
	if (remap & BM_VERT) {
		if (bm->vtable) {
			SWAP(BMVert **, vtable_dst, bm->vtable);
			bm->vtable_tot = bm->totvert;
			bm->elem_table_dirty &= ~BM_VERT;
		}
		MEM_freeN(vtable_dst);
		BLI_mempool_destroy(bm->vpool);
		bm->vpool = vpool_dst;
	}

	if (remap & BM_EDGE) {
		if (bm->etable) {
			SWAP(BMEdge **, etable_dst, bm->etable);
			bm->etable_tot = bm->totedge;
			bm->elem_table_dirty &= ~BM_EDGE;
		}
		MEM_freeN(etable_dst);
		BLI_mempool_destroy(bm->epool);
		bm->epool = epool_dst;
	}

	if (remap & BM_LOOP) {
		/* no loop table */
		MEM_freeN(ltable_dst);
		BLI_mempool_destroy(bm->lpool);
		bm->lpool = lpool_dst;
	}

	if (remap & BM_FACE) {
		if (bm->ftable) {
			SWAP(BMFace **, ftable_dst, bm->ftable);
			bm->ftable_tot = bm->totface;
			bm->elem_table_dirty &= ~BM_FACE;
		}
		MEM_freeN(ftable_dst);
		BLI_mempool_destroy(bm->fpool);
		bm->fpool = fpool_dst;
	}
}

/**
 * Re-allocates mesh data with/without toolflags.
 */
void BM_mesh_toolflags_set(BMesh *bm, bool use_toolflags)
{
	if (bm->use_toolflags == use_toolflags) {
		return;
	}

	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_BM(bm);

	BLI_mempool *vpool_dst = NULL;
	BLI_mempool *epool_dst = NULL;
	BLI_mempool *fpool_dst = NULL;

	bm_mempool_init_ex(
	        &allocsize, use_toolflags,
	        &vpool_dst, &epool_dst, NULL, &fpool_dst);

	if (use_toolflags == false) {
		BLI_mempool_destroy(bm->vtoolflagpool);
		BLI_mempool_destroy(bm->etoolflagpool);
		BLI_mempool_destroy(bm->ftoolflagpool);

		bm->vtoolflagpool = NULL;
		bm->etoolflagpool = NULL;
		bm->ftoolflagpool = NULL;
	}

	BM_mesh_rebuild(
	        bm,
	        &((struct BMeshCreateParams){.use_toolflags = use_toolflags,}),
	        vpool_dst, epool_dst, NULL, fpool_dst);

	bm->use_toolflags = use_toolflags;
}
