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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenkernel/intern/modifiers_bmesh.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_alloca.h"

#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"

/* Static function for alloc */
static BMFace *bm_face_create_from_mpoly(MPoly *mp, MLoop *ml,
                                         BMesh *bm, BMVert **vtable, BMEdge **etable)
{
	BMVert **verts = BLI_array_alloca(verts, mp->totloop);
	BMEdge **edges = BLI_array_alloca(edges, mp->totloop);
	int j;

	for (j = 0; j < mp->totloop; j++, ml++) {
		verts[j] = vtable[ml->v];
		edges[j] = etable[ml->e];
	}

	return BM_face_create(bm, verts, edges, mp->totloop, NULL, BM_CREATE_SKIP_CD);
}

/**
 * The main function for copying DerivedMesh data into BMesh.
 *
 * \note The mesh may already have geometry. see 'is_init'
 */
void DM_to_bmesh_ex(DerivedMesh *dm, BMesh *bm, const bool calc_face_normal)
{
	MVert *mv, *mvert;
	MEdge *me, *medge;
	MPoly /* *mpoly, */ /* UNUSED */ *mp;
	MLoop *mloop;
	BMVert *v, **vtable;
	BMEdge *e, **etable;
	float (*face_normals)[3];
	BMFace *f;
	int i, j, totvert, totedge /* , totface */ /* UNUSED */ ;
	bool is_init = (bm->totvert == 0) && (bm->totedge == 0) && (bm->totface == 0);
	bool is_cddm = (dm->type == DM_TYPE_CDDM);  /* duplicate the arrays for non cddm */
	char has_orig_htype = 0;

	int cd_vert_bweight_offset;
	int cd_edge_bweight_offset;
	int cd_edge_crease_offset;

	if (is_init == false) {
		/* check if we have an origflag */
		has_orig_htype |= CustomData_has_layer(&bm->vdata, CD_ORIGINDEX) ? BM_VERT : 0;
		has_orig_htype |= CustomData_has_layer(&bm->edata, CD_ORIGINDEX) ? BM_EDGE : 0;
		has_orig_htype |= CustomData_has_layer(&bm->pdata, CD_ORIGINDEX) ? BM_FACE : 0;
	}

	/*merge custom data layout*/
	CustomData_bmesh_merge(&dm->vertData, &bm->vdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_VERT);
	CustomData_bmesh_merge(&dm->edgeData, &bm->edata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_EDGE);
	CustomData_bmesh_merge(&dm->loopData, &bm->ldata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_LOOP);
	CustomData_bmesh_merge(&dm->polyData, &bm->pdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_FACE);

	if (is_init) {
		BM_mesh_cd_flag_apply(bm, dm->cd_flag);
	}

	cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
	cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
	cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	/* totface = dm->getNumPolys(dm); */ /* UNUSED */

	vtable = MEM_mallocN(sizeof(*vtable) * totvert, __func__);
	etable = MEM_mallocN(sizeof(*etable) * totedge, __func__);

	/*do verts*/
	mv = mvert = is_cddm ? dm->getVertArray(dm) : dm->dupVertArray(dm);
	for (i = 0; i < totvert; i++, mv++) {
		v = BM_vert_create(bm, mv->co, NULL, BM_CREATE_SKIP_CD);
		normal_short_to_float_v3(v->no, mv->no);
		v->head.hflag = BM_vert_flag_from_mflag(mv->flag);
		BM_elem_index_set(v, i); /* set_inline */

		CustomData_to_bmesh_block(&dm->vertData, &bm->vdata, i, &v->head.data, true);
		vtable[i] = v;

		/* add bevel weight */
		if (cd_vert_bweight_offset != -1) BM_ELEM_CD_SET_FLOAT(v, cd_vert_bweight_offset, (float)mv->bweight / 255.0f);

		if (UNLIKELY(has_orig_htype & BM_VERT)) {
			int *orig_index = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_ORIGINDEX);
			*orig_index = ORIGINDEX_NONE;
		}
	}
	if (!is_cddm) MEM_freeN(mvert);
	if (is_init) bm->elem_index_dirty &= ~BM_VERT;

	/*do edges*/
	me = medge = is_cddm ? dm->getEdgeArray(dm) : dm->dupEdgeArray(dm);
	for (i = 0; i < totedge; i++, me++) {
		//BLI_assert(BM_edge_exists(vtable[me->v1], vtable[me->v2]) == NULL);
		e = BM_edge_create(bm, vtable[me->v1], vtable[me->v2], NULL, BM_CREATE_SKIP_CD);

		e->head.hflag = BM_edge_flag_from_mflag(me->flag);
		BM_elem_index_set(e, i); /* set_inline */

		CustomData_to_bmesh_block(&dm->edgeData, &bm->edata, i, &e->head.data, true);
		etable[i] = e;

		if (cd_edge_bweight_offset != -1) BM_ELEM_CD_SET_FLOAT(e, cd_edge_bweight_offset, (float)me->bweight / 255.0f);
		if (cd_edge_crease_offset  != -1) BM_ELEM_CD_SET_FLOAT(e, cd_edge_crease_offset,  (float)me->crease  / 255.0f);

		if (UNLIKELY(has_orig_htype & BM_EDGE)) {
			int *orig_index = CustomData_bmesh_get(&bm->edata, e->head.data, CD_ORIGINDEX);
			*orig_index = ORIGINDEX_NONE;
		}
	}
	if (!is_cddm) MEM_freeN(medge);
	if (is_init) bm->elem_index_dirty &= ~BM_EDGE;

	/* do faces */
	/* note: i_alt is aligned with bmesh faces which may not always align with mpolys */
	mp = dm->getPolyArray(dm);
	mloop = dm->getLoopArray(dm);
	face_normals = (dm->dirty & DM_DIRTY_NORMALS) ? NULL : CustomData_get_layer(&dm->polyData, CD_NORMAL);
	for (i = 0; i < dm->numPolyData; i++, mp++) {
		BMLoop *l_iter;
		BMLoop *l_first;

		f = bm_face_create_from_mpoly(mp, mloop + mp->loopstart,
		                              bm, vtable, etable);

		if (UNLIKELY(f == NULL)) {
			continue;
		}

		f->head.hflag = BM_face_flag_from_mflag(mp->flag);
		BM_elem_index_set(f, bm->totface - 1); /* set_inline */
		f->mat_nr = mp->mat_nr;

		j = mp->loopstart;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			/* Save index of correspsonding MLoop */
			CustomData_to_bmesh_block(&dm->loopData, &bm->ldata, j, &l_iter->head.data, true);
			BM_elem_index_set(l_iter, j++); /* set_inline */
		} while ((l_iter = l_iter->next) != l_first);

		CustomData_to_bmesh_block(&dm->polyData, &bm->pdata, i, &f->head.data, true);

		if (calc_face_normal) {
			if (face_normals) {
				copy_v3_v3(f->no, face_normals[i]);
			}
			else {
				BM_face_normal_update(f);
			}
		}

		if (UNLIKELY(has_orig_htype & BM_FACE)) {
			int *orig_index = CustomData_bmesh_get(&bm->pdata, f->head.data, CD_ORIGINDEX);
			*orig_index = ORIGINDEX_NONE;
		}
	}
	if (is_init) bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

	MEM_freeN(vtable);
	MEM_freeN(etable);
}

/* converts a cddm to a BMEditMesh.  if existing is non-NULL, the
 * new geometry will be put in there.*/
BMEditMesh *DM_to_editbmesh(DerivedMesh *dm, BMEditMesh *existing, const bool do_tessellate)
{
	BMEditMesh *em = existing;
	BMesh *bm;

	if (em) {
		bm = em->bm;
	}
	else {
		bm = BM_mesh_create(&bm_mesh_allocsize_default);
	}

	DM_to_bmesh_ex(dm, bm, do_tessellate);

	if (!em) {
		em = BKE_editmesh_create(bm, do_tessellate);
	}
	else {
		if (do_tessellate) {
			BKE_editmesh_tessface_calc(em);
		}
	}

	return em;
}

BMesh *DM_to_bmesh(DerivedMesh *dm, const bool calc_face_normal)
{
	BMesh *bm;
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_DM(dm);

	bm = BM_mesh_create(&allocsize);

	DM_to_bmesh_ex(dm, bm, calc_face_normal);

	return bm;
}
