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

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_array.h"

#include "BKE_DerivedMesh.h"
#include "BKE_bmesh.h"
#include "BKE_tessmesh.h"

/* main function for copying DerivedMesh data into BMesh */
void DM_to_bmesh_ex(DerivedMesh *dm, BMesh *bm)
{
	MVert *mv, *mvert;
	MEdge *me, *medge;
	MPoly /* *mpoly, */ /* UNUSED */ *mp;
	MLoop *mloop, *ml;
	BMVert *v, **vtable, **verts = NULL;
	BMEdge *e, **etable, **edges = NULL;
	float (*face_normals)[3];
	BMFace *f;
	BMIter liter;
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	int i, j, k, totvert, totedge /* , totface */ /* UNUSED */ ;
	int is_init = (bm->totvert == 0) && (bm->totedge == 0) && (bm->totface == 0);

	/*merge custom data layout*/
	CustomData_bmesh_merge(&dm->vertData, &bm->vdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_VERT);
	CustomData_bmesh_merge(&dm->edgeData, &bm->edata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_EDGE);
	CustomData_bmesh_merge(&dm->loopData, &bm->ldata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_LOOP);
	CustomData_bmesh_merge(&dm->polyData, &bm->pdata, CD_MASK_DERIVEDMESH, CD_CALLOC, bm, BM_FACE);

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	/* totface = dm->getNumPolys(dm); */ /* UNUSED */

	/* add crease layer */
	BM_data_layer_add(bm, &bm->edata, CD_CREASE);
	/* add bevel weight layers */
	BM_data_layer_add(bm, &bm->edata, CD_BWEIGHT);
	BM_data_layer_add(bm, &bm->vdata, CD_BWEIGHT);

	vtable = MEM_callocN(sizeof(void **) * totvert, __func__);
	etable = MEM_callocN(sizeof(void **) * totedge, __func__);

	/*do verts*/
	mv = mvert = dm->dupVertArray(dm);
	for (i = 0; i < totvert; i++, mv++) {
		v = BM_vert_create(bm, mv->co, NULL, BM_CREATE_SKIP_CD);
		normal_short_to_float_v3(v->no, mv->no);
		v->head.hflag = BM_vert_flag_from_mflag(mv->flag);
		BM_elem_index_set(v, i); /* set_inline */

		CustomData_to_bmesh_block(&dm->vertData, &bm->vdata, i, &v->head.data);

		/* add bevel weight */
		BM_elem_float_data_set(&bm->vdata, v, CD_BWEIGHT, (float)mv->bweight / 255.0f);
		vtable[i] = v;
	}
	MEM_freeN(mvert);
	if (is_init) bm->elem_index_dirty &= ~BM_VERT;

	/*do edges*/
	me = medge = dm->dupEdgeArray(dm);
	for (i = 0; i < totedge; i++, me++) {
		//BLI_assert(BM_edge_exists(vtable[me->v1], vtable[me->v2]) == NULL);
		e = BM_edge_create(bm, vtable[me->v1], vtable[me->v2], NULL, BM_CREATE_SKIP_CD);

		e->head.hflag = BM_edge_flag_from_mflag(me->flag);
		BM_elem_index_set(e, i); /* set_inline */

		CustomData_to_bmesh_block(&dm->edgeData, &bm->edata, i, &e->head.data);
		etable[i] = e;

		/* add crease */
		BM_elem_float_data_set(&bm->edata, e, CD_CREASE, (float)me->crease / 255.0f);
		/* add bevel weight */
		BM_elem_float_data_set(&bm->edata, e, CD_BWEIGHT, (float)me->bweight / 255.0f);
	}
	MEM_freeN(medge);
	if (is_init) bm->elem_index_dirty &= ~BM_EDGE;

	/* do faces */
	/* note: i_alt is aligned with bmesh faces which may not always align with mpolys */
	mp = dm->getPolyArray(dm);
	mloop = dm->getLoopArray(dm);
	face_normals = CustomData_get_layer(&dm->polyData, CD_NORMAL);  /* can be NULL */
	for (i = 0; i < dm->numPolyData; i++, mp++) {
		BMLoop *l;

		BLI_array_empty(verts);
		BLI_array_empty(edges);

		BLI_array_grow_items(verts, mp->totloop);
		BLI_array_grow_items(edges, mp->totloop);

		ml = mloop + mp->loopstart;
		for (j = 0; j < mp->totloop; j++, ml++) {

			verts[j] = vtable[ml->v];
			edges[j] = etable[ml->e];
		}

		f = BM_face_create_ngon(bm, verts[0], verts[1], edges, mp->totloop, BM_CREATE_SKIP_CD);

		if (UNLIKELY(f == NULL)) {
			continue;
		}

		f->head.hflag = BM_face_flag_from_mflag(mp->flag);
		BM_elem_index_set(f, bm->totface - 1); /* set_inline */
		f->mat_nr = mp->mat_nr;

		l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);

		for (k = mp->loopstart; l; l = BM_iter_step(&liter), k++) {
			CustomData_to_bmesh_block(&dm->loopData, &bm->ldata, k, &l->head.data);
		}

		CustomData_to_bmesh_block(&dm->polyData, &bm->pdata, i, &f->head.data);

		if (face_normals) {
			copy_v3_v3(f->no, face_normals[i]);
		}
		else {
			BM_face_normal_update(f);
		}
	}
	if (is_init) bm->elem_index_dirty &= ~BM_FACE;

	MEM_freeN(vtable);
	MEM_freeN(etable);

	BLI_array_free(verts);
	BLI_array_free(edges);
}

/* converts a cddm to a BMEditMesh.  if existing is non-NULL, the
 * new geometry will be put in there.*/
BMEditMesh *DM_to_editbmesh(DerivedMesh *dm, BMEditMesh *existing, int do_tessellate)
{
	BMEditMesh *em = existing;
	BMesh *bm;

	if (em) {
		bm = em->bm;
	}
	else {
		bm = BM_mesh_create(&bm_mesh_allocsize_default);
	}

	DM_to_bmesh_ex(dm, bm);

	if (!em) {
		em = BMEdit_Create(bm, do_tessellate);
	}
	else {
		if (do_tessellate) {
			BMEdit_RecalcTessellation(em);
		}
	}

	return em;
}

BMesh *DM_to_bmesh(DerivedMesh *dm)
{
	BMesh *bm;

	bm = BM_mesh_create(&bm_mesh_allocsize_default);

	DM_to_bmesh_ex(dm, bm);

	return bm;
}
