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
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editface.c
 *  \ingroup edmesh
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_context.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* own include */
#include "mesh_intern.h"

/* copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting) */
void paintface_flush_flags(Object *ob)
{
	Mesh *me = get_mesh(ob);
	DerivedMesh *dm = ob->derivedFinal;
	MPoly *polys, *mp_orig;
	MFace *faces;
	int *index_array = NULL;
	int totface, totpoly;
	int i;
	
	if (me == NULL || dm == NULL)
		return;

	/*
	 * Try to push updated mesh poly flags to three other data sets:
	 *  - Mesh polys => Mesh tess faces
	 *  - Mesh polys => Final derived polys
	 *  - Final derived polys => Final derived tessfaces
	 */

	if ((index_array = CustomData_get_layer(&me->fdata, CD_POLYINDEX))) {
		faces = me->mface;
		totface = me->totface;
		
		/* loop over tessfaces */
		for (i = 0; i < totface; i++) {
			/* Copy flags onto the original tessface from its original poly */
			mp_orig = me->mpoly + index_array[i];
			faces[i].flag = mp_orig->flag;
		}
	}

	if ((index_array = CustomData_get_layer(&dm->polyData, CD_ORIGINDEX))) {
		polys = dm->getPolyArray(dm);
		totpoly = dm->getNumPolys(dm);

		/* loop over final derived polys */
		for (i = 0; i < totpoly; i++) {
			/* Copy flags onto the final derived poly from the original mesh poly */
			mp_orig = me->mpoly + index_array[i];
			polys[i].flag = mp_orig->flag;
		}
	}

	if ((index_array = CustomData_get_layer(&dm->faceData, CD_POLYINDEX))) {
		polys = dm->getPolyArray(dm);
		faces = dm->getTessFaceArray(dm);
		totface = dm->getNumTessFaces(dm);

		/* loop over tessfaces */
		for (i = 0; i < totface; i++) {
			/* Copy flags onto the final tessface from its final poly */
			mp_orig = polys + index_array[i];
			faces[i].flag = mp_orig->flag;
		}
	}
}

/* returns 0 if not found, otherwise 1 */
static int facesel_face_pick(struct bContext *C, Mesh *me, Object *ob, const int mval[2], unsigned int *index, short rect)
{
	Scene *scene = CTX_data_scene(C);
	ViewContext vc;
	view3d_set_viewcontext(C, &vc);

	if (!me || me->totpoly == 0)
		return 0;

	makeDerivedMesh(scene, ob, NULL, CD_MASK_BAREMESH, 0);

	// XXX  if (v3d->flag & V3D_INVALID_BACKBUF) {
// XXX drawview.c!		check_backbuf();
// XXX		persp(PERSP_VIEW);
// XXX  }

	if (rect) {
		/* sample rect to increase changes of selecting, so that when clicking
		 * on an edge in the backbuf, we can still select a face */

		int dist;
		*index = view3d_sample_backbuf_rect(&vc, mval, 3, 1, me->totpoly + 1, &dist, 0, NULL, NULL);
	}
	else {
		/* sample only on the exact position */
		*index = view3d_sample_backbuf(&vc, mval[0], mval[1]);
	}

	if ((*index) <= 0 || (*index) > (unsigned int)me->totpoly)
		return 0;

	(*index)--;
	
	return 1;
}

void paintface_hide(Object *ob, const int unselected)
{
	Mesh *me;
	MPoly *mpoly;
	int a;
	
	me = get_mesh(ob);
	if (me == NULL || me->totpoly == 0) return;

	mpoly = me->mpoly;
	a = me->totpoly;
	while (a--) {
		if ((mpoly->flag & ME_HIDE) == 0) {
			if (unselected) {
				if ( (mpoly->flag & ME_FACE_SEL) == 0) mpoly->flag |= ME_HIDE;
			}
			else {
				if ( (mpoly->flag & ME_FACE_SEL)) mpoly->flag |= ME_HIDE;
			}
		}
		if (mpoly->flag & ME_HIDE) mpoly->flag &= ~ME_FACE_SEL;
		
		mpoly++;
	}
	
	paintface_flush_flags(ob);
}


void paintface_reveal(Object *ob)
{
	Mesh *me;
	MPoly *mpoly;
	int a;

	me = get_mesh(ob);
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

	paintface_flush_flags(ob);
}

/* Set tface seams based on edge data, uses hash table to find seam edges. */

static void hash_add_face(EdgeHash *ehash, MPoly *mp, MLoop *mloop)
{
	MLoop *ml;
	int i;

	for (i = 0, ml = mloop; i < mp->totloop; i++, ml++) {
		BLI_edgehash_insert(ehash, ml->v, ME_POLY_LOOP_NEXT(mloop, mp, i)->v, NULL);
	}
}


static void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
{
	EdgeHash *ehash, *seamhash;
	MPoly *mp;
	MLoop *ml;
	MEdge *med;
	char *linkflag;
	int a, b, doit = 1, mark = 0;

	ehash = BLI_edgehash_new();
	seamhash = BLI_edgehash_new();
	linkflag = MEM_callocN(sizeof(char) * me->totpoly, "linkflaguv");

	for (med = me->medge, a = 0; a < me->totedge; a++, med++)
		if (med->flag & ME_SEAM)
			BLI_edgehash_insert(seamhash, med->v1, med->v2, NULL);

	if (mode == 0 || mode == 1) {
		/* only put face under cursor in array */
		mp = ((MPoly *)me->mpoly) + index;
		hash_add_face(ehash, mp, me->mloop + mp->loopstart);
		linkflag[index] = 1;
	}
	else {
		/* fill array by selection */
		mp = me->mpoly;
		for (a = 0; a < me->totpoly; a++, mp++) {
			if (mp->flag & ME_HIDE) ;
			else if (mp->flag & ME_FACE_SEL) {
				hash_add_face(ehash, mp, me->mloop + mp->loopstart);
				linkflag[a] = 1;
			}
		}
	}

	while (doit) {
		doit = 0;

		/* expand selection */
		mp = me->mpoly;
		for (a = 0; a < me->totpoly; a++, mp++) {
			if (mp->flag & ME_HIDE)
				continue;

			if (!linkflag[a]) {
				MLoop *mnextl;
				mark = 0;

				ml = me->mloop + mp->loopstart;
				for (b = 0; b < mp->totloop; b++, ml++) {
					mnextl = b < mp->totloop - 1 ? ml - 1 : me->mloop + mp->loopstart;
					if (!BLI_edgehash_haskey(seamhash, ml->v, mnextl->v))
						if (!BLI_edgehash_haskey(ehash, ml->v, mnextl->v))
							mark = 1;
				}

				if (mark) {
					linkflag[a] = 1;
					hash_add_face(ehash, mp, me->mloop + mp->loopstart);
					doit = 1;
				}
			}
		}

	}

	BLI_edgehash_free(ehash, NULL);
	BLI_edgehash_free(seamhash, NULL);

	if (mode == 0 || mode == 2) {
		for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++)
			if (linkflag[a])
				mp->flag |= ME_FACE_SEL;
			else
				mp->flag &= ~ME_FACE_SEL;
	}
	else if (mode == 1) {
		for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++)
			if (linkflag[a] && (mp->flag & ME_FACE_SEL))
				break;

		if (a < me->totpoly) {
			for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++)
				if (linkflag[a])
					mp->flag &= ~ME_FACE_SEL;
		}
		else {
			for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++)
				if (linkflag[a])
					mp->flag |= ME_FACE_SEL;
		}
	}

	MEM_freeN(linkflag);
}

void paintface_select_linked(bContext *UNUSED(C), Object *ob, int UNUSED(mval[2]), int mode)
{
	Mesh *me;
	unsigned int index = 0;

	me = get_mesh(ob);
	if (me == NULL || me->totpoly == 0) return;

	if (mode == 0 || mode == 1) {
		// XXX - Causes glitches, not sure why
#if 0
		if (!facesel_face_pick(C, me, mval, &index, 1))
			return;
#endif
	}

	select_linked_tfaces_with_seams(mode, me, index);

	paintface_flush_flags(ob);
}

void paintface_deselect_all_visible(Object *ob, int action, short flush_flags)
{
	Mesh *me;
	MPoly *mpoly;
	int a;

	me = get_mesh(ob);
	if (me == NULL) return;
	
	if (action == SEL_INVERT) {
		mpoly = me->mpoly;
		a = me->totpoly;
		while (a--) {
			if ((mpoly->flag & ME_HIDE) == 0) {
				mpoly->flag ^= ME_FACE_SEL;
			}
			mpoly++;
		}
	}
	else {
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
	}

	if (flush_flags) {
		paintface_flush_flags(ob);
	}
}

int paintface_minmax(Object *ob, float r_min[3], float r_max[3])
{
	Mesh *me;
	MPoly *mp;
	MTexPoly *tf;
	MLoop *ml;
	MVert *mvert;
	int a, b, ok = FALSE;
	float vec[3], bmat[3][3];

	me = get_mesh(ob);
	if (!me || !me->mtpoly) return ok;
	
	copy_m3_m4(bmat, ob->obmat);

	mvert = me->mvert;
	mp = me->mpoly;
	tf = me->mtpoly;
	for (a = me->totpoly; a > 0; a--, mp++, tf++) {
		if (mp->flag & ME_HIDE || !(mp->flag & ME_FACE_SEL))
			continue;

		ml = me->mloop + mp->totloop;
		for (b = 0; b < mp->totloop; b++, ml++) {
			copy_v3_v3(vec, (mvert[ml->v].co));
			mul_m3_v3(bmat, vec);
			add_v3_v3v3(vec, vec, ob->obmat[3]);
			DO_MINMAX(vec, r_min, r_max);
		}

		ok = TRUE;
	}

	return ok;
}

/* *************************************** */
#if 0
static void seam_edgehash_insert_face(EdgeHash *ehash, MPoly *mp, MLoop *loopstart)
{
	MLoop *ml1, *ml2;
	int a;

	for (a = 0; a < mp->totloop; a++) {
		ml1 = loopstart + a;
		ml2 = loopstart + (a + 1) % mp->totloop;

		BLI_edgehash_insert(ehash, ml1->v, ml2->v, NULL);
	}
}

void seam_mark_clear_tface(Scene *scene, short mode)
{
	Mesh *me;
	MPoly *mp;
	MLoop *ml1, *ml2;
	MEdge *med;
	int a, b;
	
	me = get_mesh(OBACT);
	if (me == 0 ||  me->totpoly == 0) return;

	if (mode == 0)
		mode = pupmenu("Seams%t|Mark Border Seam %x1|Clear Seam %x2");

	if (mode != 1 && mode != 2)
		return;

	if (mode == 2) {
		EdgeHash *ehash = BLI_edgehash_new();

		for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++)
			if (!(mp->flag & ME_HIDE) && (mp->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash, mp, me->mloop + mp->loopstart);

		for (a = 0, med = me->medge; a < me->totedge; a++, med++)
			if (BLI_edgehash_haskey(ehash, med->v1, med->v2))
				med->flag &= ~ME_SEAM;

		BLI_edgehash_free(ehash, NULL);
	}
	else {
		/* mark edges that are on both selected and deselected faces */
		EdgeHash *ehash1 = BLI_edgehash_new();
		EdgeHash *ehash2 = BLI_edgehash_new();

		for (a = 0, mp = me->mpoly; a < me->totpoly; a++, mp++) {
			if ((mp->flag & ME_HIDE) || !(mp->flag & ME_FACE_SEL))
				seam_edgehash_insert_face(ehash1, mp, me->mloop + mp->loopstart);
			else
				seam_edgehash_insert_face(ehash2, mp, me->mloop + mp->loopstart);
		}

		for (a = 0, med = me->medge; a < me->totedge; a++, med++)
			if (BLI_edgehash_haskey(ehash1, med->v1, med->v2) &&
			    BLI_edgehash_haskey(ehash2, med->v1, med->v2))
				med->flag |= ME_SEAM;

		BLI_edgehash_free(ehash1, NULL);
		BLI_edgehash_free(ehash2, NULL);
	}

// XXX	if (G.rt == 8)
//		unwrap_lscm(1);

	me->drawflag |= ME_DRAWSEAMS;
}
#endif

int paintface_mouse_select(struct bContext *C, Object *ob, const int mval[2], int extend)
{
	Mesh *me;
	MPoly *mpoly, *mpoly_sel;
	unsigned int a, index;
	
	/* Get the face under the cursor */
	me = get_mesh(ob);

	if (!facesel_face_pick(C, me, ob, mval, &index, 1))
		return 0;
	
	if (index >= me->totpoly || index < 0)
		return 0;

	mpoly_sel = me->mpoly + index;
	if (mpoly_sel->flag & ME_HIDE) return 0;
	
	/* clear flags */
	mpoly = me->mpoly;
	a = me->totpoly;
	if (!extend) {
		while (a--) {
			mpoly->flag &= ~ME_FACE_SEL;
			mpoly++;
		}
	}
	
	me->act_face = (int)index;

	if (extend) {
		if (mpoly_sel->flag & ME_FACE_SEL)
			mpoly_sel->flag &= ~ME_FACE_SEL;
		else
			mpoly_sel->flag |= ME_FACE_SEL;
	}
	else mpoly_sel->flag |= ME_FACE_SEL;
	
	/* image window redraw */
	
	paintface_flush_flags(ob);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return 1;
}

int do_paintface_box_select(ViewContext *vc, rcti *rect, int select, int extend)
{
	Object *ob = vc->obact;
	Mesh *me;
	MPoly *mpoly;
	struct ImBuf *ibuf;
	unsigned int *rt;
	char *selar;
	int a, index;
	int sx = rect->xmax - rect->xmin + 1;
	int sy = rect->ymax - rect->ymin + 1;
	
	me = get_mesh(ob);

	if (me == NULL || me->totpoly == 0 || sx * sy <= 0)
		return OPERATOR_CANCELLED;

	selar = MEM_callocN(me->totpoly + 1, "selar");

	if (extend == 0 && select) {
		paintface_deselect_all_visible(vc->obact, SEL_DESELECT, FALSE);

		mpoly = me->mpoly;
		for (a = 1; a <= me->totpoly; a++, mpoly++) {
			if ((mpoly->flag & ME_HIDE) == 0)
				mpoly->flag &= ~ME_FACE_SEL;
		}
	}

	view3d_validate_backbuf(vc);

	ibuf = IMB_allocImBuf(sx, sy, 32, IB_rect);
	rt = ibuf->rect;
	glReadPixels(rect->xmin + vc->ar->winrct.xmin,  rect->ymin + vc->ar->winrct.ymin, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE,  ibuf->rect);
	if (ENDIAN_ORDER == B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a = sx * sy;
	while (a--) {
		if (*rt) {
			index = WM_framebuffer_to_index(*rt);
			if (index <= me->totpoly) selar[index] = 1;
		}
		rt++;
	}

	mpoly = me->mpoly;
	for (a = 1; a <= me->totpoly; a++, mpoly++) {
		if (selar[a]) {
			if (mpoly->flag & ME_HIDE) ;
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

	paintface_flush_flags(vc->obact);

	return OPERATOR_FINISHED;
}


/*  (similar to void paintface_flush_flags(Object *ob))
 * copy the vertex flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting vertices (while painting) */
void paintvert_flush_flags(Object *ob)
{
	Mesh *me = get_mesh(ob);
	DerivedMesh *dm = ob->derivedFinal;
	MVert *dm_mvert, *dm_mv;
	int *index_array = NULL;
	int totvert;
	int i;

	if (me == NULL || dm == NULL)
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
/*  note: if the caller passes FALSE to flush_flags, then they will need to run paintvert_flush_flags(ob) themselves */
void paintvert_deselect_all_visible(Object *ob, int action, short flush_flags)
{
	Mesh *me;
	MVert *mvert;
	int a;

	me = get_mesh(ob);
	if (me == NULL) return;
	
	if (action == SEL_INVERT) {
		mvert = me->mvert;
		a = me->totvert;
		while (a--) {
			if ((mvert->flag & ME_HIDE) == 0) {
				mvert->flag ^= SELECT;
			}
			mvert++;
		}
	}
	else {
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
	}

	if (flush_flags) {
		paintvert_flush_flags(ob);
	}
}


/* ********************* MESH VERTEX MIRR TOPO LOOKUP *************** */
/* note, this is not the best place for the function to be but moved
 * here to for the purpose of syncing with bmesh */

typedef int MirrTopoHash_t;

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

int ED_mesh_mirrtopo_recalc_check(Mesh *me, const int ob_mode, MirrTopoStore_t *mesh_topo_store)
{
	int totvert;
	int totedge;

	if (me->edit_btmesh) {
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
		return TRUE;
	}
	else {
		return FALSE;
	}

}

void ED_mesh_mirrtopo_init(Mesh *me, const int ob_mode, MirrTopoStore_t *mesh_topo_store,
                           const short skip_em_vert_array_init)
{
	MEdge *medge;
	BMEditMesh *em = me->edit_btmesh;

	/* editmode*/
	BMEdge *eed;
	BMIter iter;

	int a, last;
	int totvert, totedge;
	int tot_unique = -1, tot_unique_prev = -1;

	MirrTopoHash_t *topo_hash = NULL;
	MirrTopoHash_t *topo_hash_prev = NULL;
	MirrTopoVert_t *topo_pairs;

	intptr_t *index_lookup; /* direct access to mesh_topo_store->index_lookup */

	/* reallocate if needed */
	ED_mesh_mirrtopo_free(mesh_topo_store);

	mesh_topo_store->prev_ob_mode = ob_mode;

	if (em) {
		BM_mesh_elem_index_ensure(em->bm, BM_VERT);

		totvert = em->bm->totvert;
	}
	else {
		totvert = me->totvert;
	}

	topo_hash = MEM_callocN(totvert * sizeof(MirrTopoHash_t), "TopoMirr");

	/* Initialize the vert-edge-user counts used to detect unique topology */
	if (em) {
		totedge = me->edit_btmesh->bm->totedge;

		BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
			topo_hash[BM_elem_index_get(eed->v1)]++;
			topo_hash[BM_elem_index_get(eed->v2)]++;
		}
	}
	else {
		totedge = me->totedge;

		for (a = 0, medge = me->medge; a < me->totedge; a++, medge++) {
			topo_hash[medge->v1]++;
			topo_hash[medge->v2]++;
		}
	}

	topo_hash_prev = MEM_dupallocN(topo_hash);

	tot_unique_prev = -1;
	while (1) {
		/* use the number of edges per vert to give verts unique topology IDs */

		if (em) {
			BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
				topo_hash[BM_elem_index_get(eed->v1)] += topo_hash_prev[BM_elem_index_get(eed->v2)];
				topo_hash[BM_elem_index_get(eed->v2)] += topo_hash_prev[BM_elem_index_get(eed->v1)];
			}
		}
		else {
			for (a = 0, medge = me->medge; a < me->totedge; a++, medge++) {
				/* This can make really big numbers, wrapping around here is fine */
				topo_hash[medge->v1] += topo_hash_prev[medge->v2];
				topo_hash[medge->v2] += topo_hash_prev[medge->v1];
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

		if (tot_unique <= tot_unique_prev) {
			/* Finish searching for unique valus when 1 loop dosnt give a
			 * higher number of unique values compared to the previous loop */
			break;
		}
		else {
			tot_unique_prev = tot_unique;
		}
		/* Copy the hash calculated this iter, so we can use them next time */
		memcpy(topo_hash_prev, topo_hash, sizeof(MirrTopoHash_t) * totvert);
	}

	/* Hash/Index pairs are needed for sorting to find index pairs */
	topo_pairs = MEM_callocN(sizeof(MirrTopoVert_t) * totvert, "MirrTopoPairs");

	/* since we are looping through verts, initialize these values here too */
	index_lookup = MEM_mallocN(totvert * sizeof(*index_lookup), "mesh_topo_lookup");

	if (em) {
		if (skip_em_vert_array_init == FALSE) {
			EDBM_index_arrays_init(em, 1, 0, 0);
		}
	}


	for (a = 0; a < totvert; a++) {
		topo_pairs[a].hash    = topo_hash[a];
		topo_pairs[a].v_index = a;

		/* initialize lookup */
		index_lookup[a] = -1;
	}

	qsort(topo_pairs, totvert, sizeof(MirrTopoVert_t), mirrtopo_vert_sort);

	/* Since the loop starts at 2, we must define the last index where the hash's differ */
	last = ((totvert >= 2) && (topo_pairs[0].hash == topo_pairs[1].hash)) ? 0 : 1;

	/* Get the pairs out of the sorted hashes, note, totvert+1 means we can use the previous 2,
	 * but you cant ever access the last 'a' index of MirrTopoPairs */
	for (a = 2; a <= totvert; a++) {
		/* printf("I %d %ld %d\n", (a-last), MirrTopoPairs[a  ].hash, MirrTopoPairs[a  ].v_index ); */
		if ((a == totvert) || (topo_pairs[a - 1].hash != topo_pairs[a].hash)) {
			if (a - last == 2) {
				if (em) {
					index_lookup[topo_pairs[a - 1].v_index] = (intptr_t)EDBM_vert_at_index(em, topo_pairs[a - 2].v_index);
					index_lookup[topo_pairs[a - 2].v_index] = (intptr_t)EDBM_vert_at_index(em, topo_pairs[a - 1].v_index);
				}
				else {
					index_lookup[topo_pairs[a - 1].v_index] = topo_pairs[a - 2].v_index;
					index_lookup[topo_pairs[a - 2].v_index] = topo_pairs[a - 1].v_index;
				}
			}
			last = a;
		}
	}
	if (em) {
		if (skip_em_vert_array_init == FALSE) {
			EDBM_index_arrays_free(em);
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
