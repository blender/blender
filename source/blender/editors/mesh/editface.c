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
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editface.c
 *  \ingroup edmesh
 */


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_bitmap.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_draw.h"
#include "GPU_buffers.h"

/* own include */

/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void paintface_flush_flags(Object *ob, short flag)
{
	Mesh *me = BKE_mesh_from_object(ob);
	DerivedMesh *dm = ob->derivedFinal;
	MPoly *polys, *mp_orig;
	const int *index_array = NULL;
	int totpoly;
	int i;
	
	BLI_assert((flag & ~(SELECT | ME_HIDE)) == 0);

	if (me == NULL)
		return;

	/* note, call #BKE_mesh_flush_hidden_from_verts_ex first when changing hidden flags */

	/* we could call this directly in all areas that change selection,
	 * since this could become slow for realtime updates (circle-select for eg) */
	if (flag & SELECT) {
		BKE_mesh_flush_select_from_polys(me);
	}

	if (dm == NULL)
		return;

	/* Mesh polys => Final derived polys */

	if ((index_array = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX))) {
		polys = dm->getPolyArray(dm);
		totpoly = dm->getNumPolys(dm);

		/* loop over final derived polys */
		for (i = 0; i < totpoly; i++) {
			if (index_array[i] != ORIGINDEX_NONE) {
				/* Copy flags onto the final derived poly from the original mesh poly */
				mp_orig = me->mpoly + index_array[i];
				polys[i].flag = mp_orig->flag;

			}
		}
	}

	if (flag & ME_HIDE) {
		/* draw-object caches hidden faces, force re-generation T46867 */
		GPU_drawobject_free(dm);
	}
}

void paintface_hide(Object *ob, const bool unselected)
{
	Mesh *me;
	MPoly *mpoly;
	int a;
	
	me = BKE_mesh_from_object(ob);
	if (me == NULL || me->totpoly == 0) return;

	mpoly = me->mpoly;
	a = me->totpoly;
	while (a--) {
		if ((mpoly->flag & ME_HIDE) == 0) {
			if (((mpoly->flag & ME_FACE_SEL) == 0) == unselected) {
				mpoly->flag |= ME_HIDE;
			}
		}

		if (mpoly->flag & ME_HIDE) {
			mpoly->flag &= ~ME_FACE_SEL;
		}
		
		mpoly++;
	}
	
	BKE_mesh_flush_hidden_from_polys(me);

	paintface_flush_flags(ob, SELECT | ME_HIDE);
}


void paintface_reveal(Object *ob)
{
	Mesh *me;
	MPoly *mpoly;
	int a;

	me = BKE_mesh_from_object(ob);
	if (me == NULL || me->totpoly == 0) return;

	mpoly = me->mpoly;
	a = me->totpoly;
	while (a--) {
		if (mpoly->flag & ME_HIDE) {
			mpoly->flag |= ME_FACE_SEL;
			mpoly->flag -= ME_HIDE;
		}
		mpoly++;
	}

	BKE_mesh_flush_hidden_from_polys(me);

	paintface_flush_flags(ob, SELECT | ME_HIDE);
}

/* Set tface seams based on edge data, uses hash table to find seam edges. */

static void select_linked_tfaces_with_seams(Mesh *me, const unsigned int index, const bool select)
{
	MPoly *mp;
	MLoop *ml;
	int a, b;
	bool do_it = true;
	bool mark = false;

	BLI_bitmap *edge_tag = BLI_BITMAP_NEW(me->totedge, __func__);
	BLI_bitmap *poly_tag = BLI_BITMAP_NEW(me->totpoly, __func__);

	if (index != (unsigned int)-1) {
		/* only put face under cursor in array */
		mp = &me->mpoly[index];
		BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
		BLI_BITMAP_ENABLE(poly_tag, index);
	}
	else {
		/* fill array by selection */
		mp = me->mpoly;
		for (a = 0; a < me->totpoly; a++, mp++) {
			if (mp->flag & ME_HIDE) {
				/* pass */
			}
			else if (mp->flag & ME_FACE_SEL) {
				BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
				BLI_BITMAP_ENABLE(poly_tag, a);
			}
		}
	}

	while (do_it) {
		do_it = false;

		/* expand selection */
		mp = me->mpoly;
		for (a = 0; a < me->totpoly; a++, mp++) {
			if (mp->flag & ME_HIDE)
				continue;

			if (!BLI_BITMAP_TEST(poly_tag, a)) {
				mark = false;

				ml = me->mloop + mp->loopstart;
				for (b = 0; b < mp->totloop; b++, ml++) {
					if ((me->medge[ml->e].flag & ME_SEAM) == 0) {
						if (BLI_BITMAP_TEST(edge_tag, ml->e)) {
							mark = true;
							break;
						}
					}
				}

				if (mark) {
					BLI_BITMAP_ENABLE(poly_tag, a);
					BKE_mesh_poly_edgebitmap_insert(edge_tag, mp, me->mloop + mp->loopstart);
					do_it = true;
				}
			}
		}
	}

	MEM_freeN(edge_tag);

	for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++) {
		if (BLI_BITMAP_TEST(poly_tag, a)) {
			BKE_BIT_TEST_SET(mp->flag, select, ME_FACE_SEL);
		}
	}

	MEM_freeN(poly_tag);
}

void paintface_select_linked(bContext *C, Object *ob, const int mval[2], const bool select)
{
	Mesh *me;
	unsigned int index = (unsigned int)-1;

	me = BKE_mesh_from_object(ob);
	if (me == NULL || me->totpoly == 0) return;

	if (mval) {
		if (!ED_mesh_pick_face(C, ob, mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE)) {
			return;
		}
	}

	select_linked_tfaces_with_seams(me, index, select);

	paintface_flush_flags(ob, SELECT);
}

void paintface_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
	Mesh *me;
	MPoly *mpoly;
	int a;

	me = BKE_mesh_from_object(ob);
	if (me == NULL) return;
	
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		mpoly = me->mpoly;
		a = me->totpoly;
		while (a--) {
			if ((mpoly->flag & ME_HIDE) == 0 && mpoly->flag & ME_FACE_SEL) {
				action = SEL_DESELECT;
				break;
			}
			mpoly++;
		}
	}

	mpoly = me->mpoly;
	a = me->totpoly;
	while (a--) {
		if ((mpoly->flag & ME_HIDE) == 0) {
			switch (action) {
				case SEL_SELECT:
					mpoly->flag |= ME_FACE_SEL;
					break;
				case SEL_DESELECT:
					mpoly->flag &= ~ME_FACE_SEL;
					break;
				case SEL_INVERT:
					mpoly->flag ^= ME_FACE_SEL;
					break;
			}
		}
		mpoly++;
	}

	if (flush_flags) {
		paintface_flush_flags(ob, SELECT);
	}
}

bool paintface_minmax(Object *ob, float r_min[3], float r_max[3])
{
	const Mesh *me;
	const MPoly *mp;
	const MLoop *ml;
	const MVert *mvert;
	int a, b;
	bool ok = false;
	float vec[3], bmat[3][3];

	me = BKE_mesh_from_object(ob);
	if (!me || !me->mloopuv) {
		return ok;
	}
	
	copy_m3_m4(bmat, ob->obmat);

	mvert = me->mvert;
	mp = me->mpoly;
	for (a = me->totpoly; a > 0; a--, mp++) {
		if (mp->flag & ME_HIDE || !(mp->flag & ME_FACE_SEL))
			continue;

		ml = me->mloop + mp->totloop;
		for (b = 0; b < mp->totloop; b++, ml++) {
			mul_v3_m3v3(vec, bmat, mvert[ml->v].co);
			add_v3_v3v3(vec, vec, ob->obmat[3]);
			minmax_v3v3_v3(r_min, r_max, vec);
		}

		ok = true;
	}

	return ok;
}

bool paintface_mouse_select(struct bContext *C, Object *ob, const int mval[2], bool extend, bool deselect, bool toggle)
{
	Mesh *me;
	MPoly *mpoly, *mpoly_sel;
	unsigned int a, index;
	
	/* Get the face under the cursor */
	me = BKE_mesh_from_object(ob);

	if (!ED_mesh_pick_face(C, ob, mval, &index, ED_MESH_PICK_DEFAULT_FACE_SIZE))
		return false;
	
	if (index >= me->totpoly)
		return false;

	mpoly_sel = me->mpoly + index;
	if (mpoly_sel->flag & ME_HIDE) return false;
	
	/* clear flags */
	mpoly = me->mpoly;
	a = me->totpoly;
	if (!extend && !deselect && !toggle) {
		while (a--) {
			mpoly->flag &= ~ME_FACE_SEL;
			mpoly++;
		}
	}
	
	me->act_face = (int)index;

	if (extend) {
		mpoly_sel->flag |= ME_FACE_SEL;
	}
	else if (deselect) {
		mpoly_sel->flag &= ~ME_FACE_SEL;
	}
	else if (toggle) {
		if (mpoly_sel->flag & ME_FACE_SEL)
			mpoly_sel->flag &= ~ME_FACE_SEL;
		else
			mpoly_sel->flag |= ME_FACE_SEL;
	}
	else {
		mpoly_sel->flag |= ME_FACE_SEL;
	}
	
	/* image window redraw */
	
	paintface_flush_flags(ob, SELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return true;
}

int do_paintface_box_select(ViewContext *vc, rcti *rect, bool select, bool extend)
{
	Object *ob = vc->obact;
	Mesh *me;
	MPoly *mpoly;
	struct ImBuf *ibuf;
	unsigned int *rt;
	char *selar;
	int a, index;
	const int size[2] = {
	    BLI_rcti_size_x(rect) + 1,
	    BLI_rcti_size_y(rect) + 1};
	
	me = BKE_mesh_from_object(ob);

	if ((me == NULL) || (me->totpoly == 0) || (size[0] * size[1] <= 0)) {
		return OPERATOR_CANCELLED;
	}

	selar = MEM_callocN(me->totpoly + 1, "selar");

	if (extend == false && select) {
		paintface_deselect_all_visible(vc->obact, SEL_DESELECT, false);

		mpoly = me->mpoly;
		for (a = 1; a <= me->totpoly; a++, mpoly++) {
			if ((mpoly->flag & ME_HIDE) == 0)
				mpoly->flag &= ~ME_FACE_SEL;
		}
	}

	ED_view3d_backbuf_validate(vc);

	ibuf = IMB_allocImBuf(size[0], size[1], 32, IB_rect);
	rt = ibuf->rect;
	view3d_opengl_read_pixels(vc->ar, rect->xmin, rect->ymin, size[0], size[1], GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	if (ENDIAN_ORDER == B_ENDIAN) {
		IMB_convert_rgba_to_abgr(ibuf);
	}
	GPU_select_to_index_array(ibuf->rect, size[0] * size[1]);

	a = size[0] * size[1];
	while (a--) {
		if (*rt) {
			index = *rt;
			if (index <= me->totpoly) {
				selar[index] = 1;
			}
		}
		rt++;
	}

	mpoly = me->mpoly;
	for (a = 1; a <= me->totpoly; a++, mpoly++) {
		if (selar[a]) {
			if (mpoly->flag & ME_HIDE) {
				/* pass */
			}
			else {
				if (select) mpoly->flag |= ME_FACE_SEL;
				else mpoly->flag &= ~ME_FACE_SEL;
			}
		}
	}

	IMB_freeImBuf(ibuf);
	MEM_freeN(selar);

#ifdef __APPLE__	
	glReadBuffer(GL_BACK);
#endif

	paintface_flush_flags(vc->obact, SELECT);

	return OPERATOR_FINISHED;
}


/*  (similar to void paintface_flush_flags(Object *ob))
 * copy the vertex flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting vertices (while painting) */
void paintvert_flush_flags(Object *ob)
{
	Mesh *me = BKE_mesh_from_object(ob);
	DerivedMesh *dm = ob->derivedFinal;
	MVert *dm_mvert, *dm_mv;
	const int *index_array = NULL;
	int totvert;
	int i;

	if (me == NULL)
		return;

	/* we could call this directly in all areas that change selection,
	 * since this could become slow for realtime updates (circle-select for eg) */
	BKE_mesh_flush_select_from_verts(me);

	if (dm == NULL)
		return;

	index_array = dm->getVertDataArray(dm, CD_ORIGINDEX);

	dm_mvert = dm->getVertArray(dm);
	totvert = dm->getNumVerts(dm);

	dm_mv = dm_mvert;

	if (index_array) {
		int orig_index;
		for (i = 0; i < totvert; i++, dm_mv++) {
			orig_index = index_array[i];
			if (orig_index != ORIGINDEX_NONE) {
				dm_mv->flag = me->mvert[index_array[i]].flag;
			}
		}
	}
	else {
		for (i = 0; i < totvert; i++, dm_mv++) {
			dm_mv->flag = me->mvert[i].flag;
		}
	}
}
/*  note: if the caller passes false to flush_flags, then they will need to run paintvert_flush_flags(ob) themselves */
void paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
	Mesh *me;
	MVert *mvert;
	int a;

	me = BKE_mesh_from_object(ob);
	if (me == NULL) return;
	
	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		mvert = me->mvert;
		a = me->totvert;
		while (a--) {
			if ((mvert->flag & ME_HIDE) == 0 && mvert->flag & SELECT) {
				action = SEL_DESELECT;
				break;
			}
			mvert++;
		}
	}

	mvert = me->mvert;
	a = me->totvert;
	while (a--) {
		if ((mvert->flag & ME_HIDE) == 0) {
			switch (action) {
				case SEL_SELECT:
					mvert->flag |= SELECT;
					break;
				case SEL_DESELECT:
					mvert->flag &= ~SELECT;
					break;
				case SEL_INVERT:
					mvert->flag ^= SELECT;
					break;
			}
		}
		mvert++;
	}

	/* handle mselect */
	if (action == SEL_SELECT) {
		/* pass */
	}
	else if (ELEM(action, SEL_DESELECT, SEL_INVERT)) {
		BKE_mesh_mselect_clear(me);
	}
	else {
		BKE_mesh_mselect_validate(me);
	}

	if (flush_flags) {
		paintvert_flush_flags(ob);
	}
}

void paintvert_select_ungrouped(Object *ob, bool extend, bool flush_flags)
{
	Mesh *me = BKE_mesh_from_object(ob);
	MVert *mv;
	MDeformVert *dv;
	int a, tot;

	if (me == NULL || me->dvert == NULL) {
		return;
	}

	if (!extend) {
		paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
	}

	dv = me->dvert;
	tot = me->totvert;

	for (a = 0, mv = me->mvert; a < tot; a++, mv++, dv++) {
		if ((mv->flag & ME_HIDE) == 0) {
			if (dv->dw == NULL) {
				/* if null weight then not grouped */
				mv->flag |= SELECT;
			}
		}
	}

	if (flush_flags) {
		paintvert_flush_flags(ob);
	}
}

/* ********************* MESH VERTEX MIRR TOPO LOOKUP *************** */
/* note, this is not the best place for the function to be but moved
 * here for the purpose of syncing with bmesh */

typedef unsigned int MirrTopoHash_t;

typedef struct MirrTopoVert_t {
	MirrTopoHash_t hash;
	int v_index;
} MirrTopoVert_t;

static int mirrtopo_hash_sort(const void *l1, const void *l2)
{
	if      ((MirrTopoHash_t)(intptr_t)l1 > (MirrTopoHash_t)(intptr_t)l2) return 1;
	else if ((MirrTopoHash_t)(intptr_t)l1 < (MirrTopoHash_t)(intptr_t)l2) return -1;
	return 0;
}

static int mirrtopo_vert_sort(const void *v1, const void *v2)
{
	if      (((MirrTopoVert_t *)v1)->hash > ((MirrTopoVert_t *)v2)->hash) return 1;
	else if (((MirrTopoVert_t *)v1)->hash < ((MirrTopoVert_t *)v2)->hash) return -1;
	return 0;
}

bool ED_mesh_mirrtopo_recalc_check(Mesh *me, DerivedMesh *dm, const int ob_mode, MirrTopoStore_t *mesh_topo_store)
{
	int totvert;
	int totedge;

	if (dm) {
		totvert = dm->getNumVerts(dm);
		totedge = dm->getNumEdges(dm);
	}
	else if (me->edit_btmesh) {
		totvert = me->edit_btmesh->bm->totvert;
		totedge = me->edit_btmesh->bm->totedge;
	}
	else {
		totvert = me->totvert;
		totedge = me->totedge;
	}

	if ((mesh_topo_store->index_lookup == NULL) ||
	    (mesh_topo_store->prev_ob_mode != ob_mode) ||
	    (totvert != mesh_topo_store->prev_vert_tot) ||
	    (totedge != mesh_topo_store->prev_edge_tot))
	{
		return true;
	}
	else {
		return false;
	}

}

void ED_mesh_mirrtopo_init(Mesh *me, DerivedMesh *dm, const int ob_mode, MirrTopoStore_t *mesh_topo_store,
                           const bool skip_em_vert_array_init)
{
	MEdge *medge = NULL, *med;
	BMEditMesh *em = dm ?  NULL : me->edit_btmesh;

	/* editmode*/
	BMEdge *eed;
	BMIter iter;

	int a, last;
	int totvert, totedge;
	int tot_unique = -1, tot_unique_prev = -1;
	int tot_unique_edges = 0, tot_unique_edges_prev;

	MirrTopoHash_t *topo_hash = NULL;
	MirrTopoHash_t *topo_hash_prev = NULL;
	MirrTopoVert_t *topo_pairs;
	MirrTopoHash_t  topo_pass = 1;

	intptr_t *index_lookup; /* direct access to mesh_topo_store->index_lookup */

	/* reallocate if needed */
	ED_mesh_mirrtopo_free(mesh_topo_store);

	mesh_topo_store->prev_ob_mode = ob_mode;

	if (em) {
		BM_mesh_elem_index_ensure(em->bm, BM_VERT);

		totvert = em->bm->totvert;
	}
	else {
		totvert = dm ? dm->getNumVerts(dm) : me->totvert;
	}

	topo_hash = MEM_callocN(totvert * sizeof(MirrTopoHash_t), "TopoMirr");

	/* Initialize the vert-edge-user counts used to detect unique topology */
	if (em) {
		totedge = me->edit_btmesh->bm->totedge;

		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			const int i1 = BM_elem_index_get(eed->v1), i2 = BM_elem_index_get(eed->v2);
			topo_hash[i1]++;
			topo_hash[i2]++;
		}
	}
	else {
		totedge = dm ? dm->getNumEdges(dm) : me->totedge;
		medge = dm ? dm->getEdgeArray(dm) : me->medge;

		for (a = 0, med = medge; a < totedge; a++, med++) {
			const unsigned int i1 = med->v1, i2 = med->v2;
			topo_hash[i1]++;
			topo_hash[i2]++;
		}
	}

	topo_hash_prev = MEM_dupallocN(topo_hash);

	tot_unique_prev = -1;
	tot_unique_edges_prev = -1;
	while (1) {
		/* use the number of edges per vert to give verts unique topology IDs */

		tot_unique_edges = 0;

		/* This can make really big numbers, wrapping around here is fine */
		if (em) {
			BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
				const int i1 = BM_elem_index_get(eed->v1), i2 = BM_elem_index_get(eed->v2);
				topo_hash[i1] += topo_hash_prev[i2] * topo_pass;
				topo_hash[i2] += topo_hash_prev[i1] * topo_pass;
				tot_unique_edges += (topo_hash[i1] != topo_hash[i2]);
			}
		}
		else {
			for (a = 0, med = medge; a < totedge; a++, med++) {
				const unsigned int i1 = med->v1, i2 = med->v2;
				topo_hash[i1] += topo_hash_prev[i2] * topo_pass;
				topo_hash[i2] += topo_hash_prev[i1] * topo_pass;
				tot_unique_edges += (topo_hash[i1] != topo_hash[i2]);
			}
		}
		memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

		/* sort so we can count unique values */
		qsort(topo_hash_prev, totvert, sizeof(MirrTopoHash_t), mirrtopo_hash_sort);

		tot_unique = 1; /* account for skiping the first value */
		for (a = 1; a < totvert; a++) {
			if (topo_hash_prev[a - 1] != topo_hash_prev[a]) {
				tot_unique++;
			}
		}

		if ((tot_unique <= tot_unique_prev) && (tot_unique_edges <= tot_unique_edges_prev)) {
			/* Finish searching for unique values when 1 loop dosnt give a
			 * higher number of unique values compared to the previous loop */
			break;
		}
		else {
			tot_unique_prev = tot_unique;
			tot_unique_edges_prev = tot_unique_edges;
		}
		/* Copy the hash calculated this iter, so we can use them next time */
		memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);

		topo_pass++;
	}

	/* Hash/Index pairs are needed for sorting to find index pairs */
	topo_pairs = MEM_callocN(sizeof(MirrTopoVert_t) * totvert, "MirrTopoPairs");

	/* since we are looping through verts, initialize these values here too */
	index_lookup = MEM_mallocN(totvert * sizeof(*index_lookup), "mesh_topo_lookup");

	if (em) {
		if (skip_em_vert_array_init == false) {
			BM_mesh_elem_table_ensure(em->bm, BM_VERT);
		}
	}

	for (a = 0; a < totvert; a++) {
		topo_pairs[a].hash    = topo_hash[a];
		topo_pairs[a].v_index = a;

		/* initialize lookup */
		index_lookup[a] = -1;
	}

	qsort(topo_pairs, totvert, sizeof(MirrTopoVert_t), mirrtopo_vert_sort);

	last = 0;

	/* Get the pairs out of the sorted hashes, note, totvert+1 means we can use the previous 2,
	 * but you cant ever access the last 'a' index of MirrTopoPairs */
	if (em) {
		BMVert **vtable = em->bm->vtable;
		for (a = 1; a <= totvert; a++) {
			/* printf("I %d %ld %d\n", (a - last), MirrTopoPairs[a].hash, MirrTopoPairs[a].v_indexs); */
			if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
				const int match_count = a - last;
				if (match_count == 2) {
					const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
					index_lookup[j] = (intptr_t)vtable[k];
					index_lookup[k] = (intptr_t)vtable[j];
				}
				else if (match_count == 1) {
					/* Center vertex. */
					const int j = topo_pairs[a - 1].v_index;
					index_lookup[j] = (intptr_t)vtable[j];
				}
				last = a;
			}
		}
	}
	else {
		/* same as above, for mesh */
		for (a = 1; a <= totvert; a++) {
			if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
				const int match_count = a - last;
				if (match_count == 2) {
					const int j = topo_pairs[a - 1].v_index, k = topo_pairs[a - 2].v_index;
					index_lookup[j] = k;
					index_lookup[k] = j;
				}
				else if (match_count == 1) {
					/* Center vertex. */
					const int j = topo_pairs[a - 1].v_index;
					index_lookup[j] = j;
				}
				last = a;
			}
		}
	}

	MEM_freeN(topo_pairs);
	topo_pairs = NULL;

	MEM_freeN(topo_hash);
	MEM_freeN(topo_hash_prev);

	mesh_topo_store->index_lookup  = index_lookup;
	mesh_topo_store->prev_vert_tot = totvert;
	mesh_topo_store->prev_edge_tot = totedge;
}

void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store)
{
	if (mesh_topo_store->index_lookup) {
		MEM_freeN(mesh_topo_store->index_lookup);
	}
	mesh_topo_store->index_lookup  = NULL;
	mesh_topo_store->prev_vert_tot = -1;
	mesh_topo_store->prev_edge_tot = -1;
}
