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

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_tessmesh.h"
#include "BKE_multires.h"

#include "ED_mesh.h"

#include "intern/bmesh_private.h"

/* used as an extern, defined in bmesh.h */
BMAllocTemplate bm_mesh_allocsize_default = {512, 1024, 2048, 512};
BMAllocTemplate bm_mesh_chunksize_default = {512, 1024, 2048, 512};

static void bm_mempool_init(BMesh *bm, const BMAllocTemplate *allocsize)
{
	bm->vpool = BLI_mempool_create(sizeof(BMVert), allocsize->totvert, bm_mesh_chunksize_default.totvert, BLI_MEMPOOL_ALLOW_ITER);
	bm->epool = BLI_mempool_create(sizeof(BMEdge), allocsize->totedge, bm_mesh_chunksize_default.totedge, BLI_MEMPOOL_ALLOW_ITER);
	bm->lpool = BLI_mempool_create(sizeof(BMLoop), allocsize->totloop, bm_mesh_chunksize_default.totloop, 0);
	bm->fpool = BLI_mempool_create(sizeof(BMFace), allocsize->totface, bm_mesh_chunksize_default.totface, BLI_MEMPOOL_ALLOW_ITER);

#ifdef USE_BMESH_HOLES
	bm->looplistpool = BLI_mempool_create(sizeof(BMLoopList), allocsize[3], allocsize[3], FALSE, FALSE);
#endif

	/* allocate one flag pool that we don't get rid of. */
	bm->toolflagpool = BLI_mempool_create(sizeof(BMFlagLayer), 512, 512, 0);
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
BMesh *BM_mesh_create(BMAllocTemplate *allocsize)
{
	/* allocate the structure */
	BMesh *bm = MEM_callocN(sizeof(BMesh), __func__);
	
	/* allocate the memory pools for the mesh elements */
	bm_mempool_init(bm, allocsize);

	/* allocate one flag pool that we don't get rid of. */
	bm->stackdepth = 1;
	bm->totflags = 1;

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
	
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		CustomData_bmesh_free_block(&(bm->vdata), &(v->head.data));
	}
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		CustomData_bmesh_free_block(&(bm->edata), &(e->head.data));
	}
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		CustomData_bmesh_free_block(&(bm->pdata), &(f->head.data));
		BM_ITER(l, &itersub, bm, BM_LOOPS_OF_FACE, f) {
			CustomData_bmesh_free_block(&(bm->ldata), &(l->head.data));
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

	/* destroy flag pool */
	BLI_mempool_destroy(bm->toolflagpool);

#ifdef USE_BMESH_HOLES
	BLI_mempool_destroy(bm->looplistpool);
#endif

	/* These tables aren't used yet, so it's not strictly necessary
	 * to 'end' them (with 'e' param) but if someone tries to start
	 * using them, having these in place will save a lot of pain */
	mesh_octree_table(NULL, NULL, NULL, 'e');
	mesh_mirrtopo_table(NULL, 'e');

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
	/* free old mesh */
	BM_mesh_data_free(bm);
	memset(bm, 0, sizeof(BMesh));

	/* allocate the memory pools for the mesh elements */
	bm_mempool_init(bm, &bm_mesh_allocsize_default);

	bm->stackdepth = 1;
	bm->totflags = 1;
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
		/* keep this out of 'BM_mesh_data_free' because we wan't python
		 * to be able to clear the mesh and maintain access. */
		extern void bpy_bm_generic_invalidate(void *self);

		bpy_bm_generic_invalidate(bm->py_handle);
		bm->py_handle = NULL;
	}

	MEM_freeN(bm);
}

/**
 * \brief BMesh Compute Normals
 *
 * Updates the normals of a mesh.
 */
void BM_mesh_normals_update(BMesh *bm, const short skip_hidden)
{
	BMVert *v;
	BMFace *f;
	BMLoop *l;
	BMEdge *e;
	BMIter verts;
	BMIter faces;
	BMIter loops;
	BMIter edges;
	int index;
	float (*edgevec)[3];
	
	/* calculate all face normals */
	BM_ITER(f, &faces, bm, BM_FACES_OF_MESH, NULL) {
		if (skip_hidden && BM_elem_flag_test(f, BM_ELEM_HIDDEN))
			continue;
#if 0	/* UNUSED */
		if (f->head.flag & BM_NONORMCALC)
			continue;
#endif

		BM_face_normal_update(bm, f);
	}
	
	/* Zero out vertex normals */
	BM_ITER(v, &verts, bm, BM_VERTS_OF_MESH, NULL) {
		if (skip_hidden && BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			continue;

		zero_v3(v->no);
	}

	/* compute normalized direction vectors for each edge. directions will be
	 * used below for calculating the weights of the face normals on the vertex
	 * normals */
	index = 0;
	edgevec = MEM_callocN(sizeof(float) * 3 * bm->totedge, "BM normal computation array");
	BM_ITER(e, &edges, bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_index_set(e, index); /* set_inline */

		if (e->l) {
			sub_v3_v3v3(edgevec[index], e->v2->co, e->v1->co);
			normalize_v3(edgevec[index]);
		}
		else {
			/* the edge vector will not be needed when the edge has no radial */
		}

		index++;
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	/* add weighted face normals to vertices */
	BM_ITER(f, &faces, bm, BM_FACES_OF_MESH, NULL) {

		if (skip_hidden && BM_elem_flag_test(f, BM_ELEM_HIDDEN))
			continue;

		BM_ITER(l, &loops, bm, BM_LOOPS_OF_FACE, f) {
			float *e1diff, *e2diff;
			float dotprod;
			float fac;

			/* calculate the dot product of the two edges that
			 * meet at the loop's vertex */
			e1diff = edgevec[BM_elem_index_get(l->prev->e)];
			e2diff = edgevec[BM_elem_index_get(l->e)];
			dotprod = dot_v3v3(e1diff, e2diff);

			/* edge vectors are calculated from e->v1 to e->v2, so
			 * adjust the dot product if one but not both loops
			 * actually runs from from e->v2 to e->v1 */
			if ((l->prev->e->v1 == l->prev->v) ^ (l->e->v1 == l->v)) {
				dotprod = -dotprod;
			}

			fac = saacos(-dotprod);

			/* accumulate weighted face normal into the vertex's normal */
			madd_v3_v3fl(l->v->no, f->no, fac);
		}
	}
	
	/* normalize the accumulated vertex normals */
	BM_ITER(v, &verts, bm, BM_VERTS_OF_MESH, NULL) {
		if (skip_hidden && BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			continue;

		if (normalize_v3(v->no) == 0.0f) {
			normalize_v3_v3(v->no, v->co);
		}
	}
	
	MEM_freeN(edgevec);
}

/*
 * This function ensures correct normals for the mesh, but
 * sets the flag BM_ELEM_TAG in flipped faces, to allow restoration
 * of original normals.
 *
 * if undo is 0: calculate right normals
 * if undo is 1: restore original normals
 */

//keep in sycn with utils.c!
#define FACE_FLIP	8
static void bm_rationalize_normals(BMesh *bm, int undo)
{
	BMOperator bmop;
	BMFace *f;
	BMIter iter;
	
	if (undo) {
		BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
			if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
				BM_face_normal_flip(bm, f);
			}
			BM_elem_flag_disable(f, BM_ELEM_TAG);
		}
		
		return;
	}
	
	BMO_op_initf(bm, &bmop, "righthandfaces faces=%af do_flip=%b", FALSE);
	
	BMO_push(bm, &bmop);
	bmo_righthandfaces_exec(bm, &bmop);
	
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BM_elem_flag_set(f, BM_ELEM_TAG, BMO_elem_flag_test(bm, f, FACE_FLIP));
	}

	BMO_pop(bm);
	BMO_op_finish(bm, &bmop);
}

static void UNUSED_FUNCTION(bm_mdisps_space_set)(Object *ob, BMesh *bm, int from, int to)
{
	/* switch multires data out of tangent space */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		BMEditMesh *em = BMEdit_Create(bm, FALSE);
		DerivedMesh *dm = CDDM_from_BMEditMesh(em, NULL, TRUE, FALSE);
		MDisps *mdisps;
		BMFace *f;
		BMIter iter;
		// int i = 0; // UNUSED
		
		multires_set_space(dm, ob, from, to);
		
		mdisps = CustomData_get_layer(&dm->loopData, CD_MDISPS);
		
		BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
			BMLoop *l;
			BMIter liter;
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
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
		
		/* setting this to NULL prevents BMEdit_Free from freeing it */
		em->bm = NULL;
		BMEdit_Free(em);
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
void bmesh_edit_begin(BMesh *bm, int flag)
{
	bm->opflag = flag;
	
	/* Most operators seem to be using BMO_OP_FLAG_UNTAN_MULTIRES to change the MDisps to
	 * absolute space during mesh edits. With this enabled, changes to the topology
	 * (loop cuts, edge subdivides, etc) are not reflected in the higher levels of
	 * the mesh at all, which doesn't seem right. Turning off completely for now,
	 * until this is shown to be better for certain types of mesh edits. */
#if BMOP_UNTAN_MULTIRES_ENABLED
	/* switch multires data out of tangent space */
	if ((flag & BMO_OP_FLAG_UNTAN_MULTIRES) && CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		bmesh_mdisps_space_set(bm, MULTIRES_SPACE_TANGENT, MULTIRES_SPACE_ABSOLUTE);

		/* ensure correct normals, if possible */
		bmesh_rationalize_normals(bm, 0);
		BM_mesh_normals_update(bm);
	}
	else if (flag & BMO_OP_FLAG_RATIONALIZE_NORMALS) {
		bmesh_rationalize_normals(bm, 0);
	}
#else
	if (flag & BMO_OP_FLAG_RATIONALIZE_NORMALS) {
		bm_rationalize_normals(bm, 0);
	}
#endif
}

/**
 * \brief BMesh End Edit
 */
void bmesh_edit_end(BMesh *bm, int flag)
{
	/* BMO_OP_FLAG_UNTAN_MULTIRES disabled for now, see comment above in bmesh_edit_begin. */
#if BMOP_UNTAN_MULTIRES_ENABLED
	/* switch multires data into tangent space */
	if ((flag & BMO_OP_FLAG_UNTAN_MULTIRES) && CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		/* set normals to their previous winding */
		bmesh_rationalize_normals(bm, 1);
		bmesh_mdisps_space_set(bm, MULTIRES_SPACE_ABSOLUTE, MULTIRES_SPACE_TANGENT);
	}
	else if (flag & BMO_OP_FLAG_RATIONALIZE_NORMALS) {
		bmesh_rationalize_normals(bm, 1);
	}
#else
	if (flag & BMO_OP_FLAG_RATIONALIZE_NORMALS) {
		bm_rationalize_normals(bm, 1);
	}
#endif

	bm->opflag = 0;

	/* compute normals, clear temp flags and flush selections */
	BM_mesh_normals_update(bm, TRUE);
	BM_mesh_select_mode_flush(bm);
}

void BM_mesh_elem_index_ensure(BMesh *bm, const char hflag)
{
	BMIter iter;
	BMElem *ele;

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "Should Never Fail!", __func__);
#endif

	if (hflag & BM_VERT) {
		if (bm->elem_index_dirty & BM_VERT) {
			int index = 0;
			BM_ITER(ele, &iter, bm, BM_VERTS_OF_MESH, NULL) {
				BM_elem_index_set(ele, index); /* set_ok */
				index++;
			}
			bm->elem_index_dirty &= ~BM_VERT;
			BLI_assert(index == bm->totvert);
		}
		else {
			// printf("%s: skipping vert index calc!\n", __func__);
		}
	}

	if (hflag & BM_EDGE) {
		if (bm->elem_index_dirty & BM_EDGE) {
			int index = 0;
			BM_ITER(ele, &iter, bm, BM_EDGES_OF_MESH, NULL) {
				BM_elem_index_set(ele, index); /* set_ok */
				index++;
			}
			bm->elem_index_dirty &= ~BM_EDGE;
			BLI_assert(index == bm->totedge);
		}
		else {
			// printf("%s: skipping edge index calc!\n", __func__);
		}
	}

	if (hflag & BM_FACE) {
		if (bm->elem_index_dirty & BM_FACE) {
			int index = 0;
			BM_ITER(ele, &iter, bm, BM_FACES_OF_MESH, NULL) {
				BM_elem_index_set(ele, index); /* set_ok */
				index++;
			}
			bm->elem_index_dirty &= ~BM_FACE;
			BLI_assert(index == bm->totface);
		}
		else {
			// printf("%s: skipping face index calc!\n", __func__);
		}
	}
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

void BM_mesh_elem_index_validate(BMesh *bm, const char *location, const char *func,
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
	int is_any_error = 0;

	for (i = 0; i < 3; i++) {
		const int is_dirty = (flag_types[i] & bm->elem_index_dirty);
		int index = 0;
		int is_error = FALSE;
		int err_val = 0;
		int err_idx = 0;

		BM_ITER(ele, &iter, bm, iter_types[i], NULL) {
			if (!is_dirty) {
				if (BM_elem_index_get(ele) != index) {
					err_val = BM_elem_index_get(ele);
					err_idx = index;
					is_error = TRUE;
				}
			}

			BM_elem_index_set(ele, index); /* set_ok */
			index++;
		}

		if ((is_error == TRUE) && (is_dirty == FALSE)) {
			is_any_error = TRUE;
			fprintf(stderr,
			        "Invalid Index: at %s, %s, %s[%d] invalid index %d, '%s', '%s'\n",
			        location, func, type_names[i], err_idx, err_val, msg_a, msg_b);
		}
		else if ((is_error == FALSE) && (is_dirty == TRUE)) {

#if 0		/* mostly annoying */

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

BMVert *BM_vert_at_index(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->vpool, index);
}

BMEdge *BM_edge_at_index(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->epool, index);
}

BMFace *BM_face_at_index(BMesh *bm, const int index)
{
	return BLI_mempool_findelem(bm->fpool, index);
}
