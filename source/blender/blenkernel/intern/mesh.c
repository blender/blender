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

/** \file blender/blenkernel/intern/mesh.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_ipo_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_edgehash.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_main.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_mball.h"
#include "BKE_depsgraph.h"
/* these 2 are only used by conversion functions */
#include "BKE_curve.h"
/* -- */
#include "BKE_object.h"
#include "BKE_editmesh.h"


enum {
	MESHCMP_DVERT_WEIGHTMISMATCH = 1,
	MESHCMP_DVERT_GROUPMISMATCH,
	MESHCMP_DVERT_TOTGROUPMISMATCH,
	MESHCMP_LOOPCOLMISMATCH,
	MESHCMP_LOOPUVMISMATCH,
	MESHCMP_LOOPMISMATCH,
	MESHCMP_POLYVERTMISMATCH,
	MESHCMP_POLYMISMATCH,
	MESHCMP_EDGEUNKNOWN,
	MESHCMP_VERTCOMISMATCH,
	MESHCMP_CDLAYERS_MISMATCH
};

static const char *cmpcode_to_str(int code)
{
	switch (code) {
		case MESHCMP_DVERT_WEIGHTMISMATCH:
			return "Vertex Weight Mismatch";
		case MESHCMP_DVERT_GROUPMISMATCH:
			return "Vertex Group Mismatch";
		case MESHCMP_DVERT_TOTGROUPMISMATCH:
			return "Vertex Doesn't Belong To Same Number Of Groups";
		case MESHCMP_LOOPCOLMISMATCH:
			return "Vertex Color Mismatch";
		case MESHCMP_LOOPUVMISMATCH:
			return "UV Mismatch";
		case MESHCMP_LOOPMISMATCH:
			return "Loop Mismatch";
		case MESHCMP_POLYVERTMISMATCH:
			return "Loop Vert Mismatch In Poly Test";
		case MESHCMP_POLYMISMATCH:
			return "Loop Vert Mismatch";
		case MESHCMP_EDGEUNKNOWN:
			return "Edge Mismatch";
		case MESHCMP_VERTCOMISMATCH:
			return "Vertex Coordinate Mismatch";
		case MESHCMP_CDLAYERS_MISMATCH:
			return "CustomData Layer Count Mismatch";
		default:
			return "Mesh Comparison Code Unknown";
	}
}

/* thresh is threshold for comparing vertices, uvs, vertex colors,
 * weights, etc.*/
static int customdata_compare(CustomData *c1, CustomData *c2, Mesh *m1, Mesh *m2, const float thresh)
{
	const float thresh_sq = thresh * thresh;
	CustomDataLayer *l1, *l2;
	int i, i1 = 0, i2 = 0, tot, j;
	
	for (i = 0; i < c1->totlayer; i++) {
		if (ELEM(c1->layers[i].type, CD_MVERT, CD_MEDGE, CD_MPOLY,
		         CD_MLOOPUV, CD_MLOOPCOL, CD_MTEXPOLY, CD_MDEFORMVERT))
		{
			i1++;
		}
	}

	for (i = 0; i < c2->totlayer; i++) {
		if (ELEM(c2->layers[i].type, CD_MVERT, CD_MEDGE, CD_MPOLY,
		         CD_MLOOPUV, CD_MLOOPCOL, CD_MTEXPOLY, CD_MDEFORMVERT))
		{
			i2++;
		}
	}

	if (i1 != i2)
		return MESHCMP_CDLAYERS_MISMATCH;
	
	l1 = c1->layers; l2 = c2->layers;
	tot = i1;
	i1 = 0; i2 = 0;
	for (i = 0; i < tot; i++) {
		while (i1 < c1->totlayer && !ELEM(l1->type, CD_MVERT, CD_MEDGE, CD_MPOLY,
		                                  CD_MLOOPUV, CD_MLOOPCOL, CD_MTEXPOLY, CD_MDEFORMVERT))
		{
			i1++, l1++;
		}

		while (i2 < c2->totlayer && !ELEM(l2->type, CD_MVERT, CD_MEDGE, CD_MPOLY,
		                                  CD_MLOOPUV, CD_MLOOPCOL, CD_MTEXPOLY, CD_MDEFORMVERT))
		{
			i2++, l2++;
		}
		
		if (l1->type == CD_MVERT) {
			MVert *v1 = l1->data;
			MVert *v2 = l2->data;
			int vtot = m1->totvert;
			
			for (j = 0; j < vtot; j++, v1++, v2++) {
				if (len_squared_v3v3(v1->co, v2->co) > thresh_sq)
					return MESHCMP_VERTCOMISMATCH;
				/* I don't care about normals, let's just do coodinates */
			}
		}
		
		/*we're order-agnostic for edges here*/
		if (l1->type == CD_MEDGE) {
			MEdge *e1 = l1->data;
			MEdge *e2 = l2->data;
			int etot = m1->totedge;
			EdgeHash *eh = BLI_edgehash_new_ex(__func__, etot);
		
			for (j = 0; j < etot; j++, e1++) {
				BLI_edgehash_insert(eh, e1->v1, e1->v2, e1);
			}
			
			for (j = 0; j < etot; j++, e2++) {
				if (!BLI_edgehash_lookup(eh, e2->v1, e2->v2))
					return MESHCMP_EDGEUNKNOWN;
			}
			BLI_edgehash_free(eh, NULL);
		}
		
		if (l1->type == CD_MPOLY) {
			MPoly *p1 = l1->data;
			MPoly *p2 = l2->data;
			int ptot = m1->totpoly;
		
			for (j = 0; j < ptot; j++, p1++, p2++) {
				MLoop *lp1, *lp2;
				int k;
				
				if (p1->totloop != p2->totloop)
					return MESHCMP_POLYMISMATCH;
				
				lp1 = m1->mloop + p1->loopstart;
				lp2 = m2->mloop + p2->loopstart;
				
				for (k = 0; k < p1->totloop; k++, lp1++, lp2++) {
					if (lp1->v != lp2->v)
						return MESHCMP_POLYVERTMISMATCH;
				}
			}
		}
		if (l1->type == CD_MLOOP) {
			MLoop *lp1 = l1->data;
			MLoop *lp2 = l2->data;
			int ltot = m1->totloop;
		
			for (j = 0; j < ltot; j++, lp1++, lp2++) {
				if (lp1->v != lp2->v)
					return MESHCMP_LOOPMISMATCH;
			}
		}
		if (l1->type == CD_MLOOPUV) {
			MLoopUV *lp1 = l1->data;
			MLoopUV *lp2 = l2->data;
			int ltot = m1->totloop;
		
			for (j = 0; j < ltot; j++, lp1++, lp2++) {
				if (len_squared_v2v2(lp1->uv, lp2->uv) > thresh_sq)
					return MESHCMP_LOOPUVMISMATCH;
			}
		}
		
		if (l1->type == CD_MLOOPCOL) {
			MLoopCol *lp1 = l1->data;
			MLoopCol *lp2 = l2->data;
			int ltot = m1->totloop;
		
			for (j = 0; j < ltot; j++, lp1++, lp2++) {
				if (ABS(lp1->r - lp2->r) > thresh || 
				    ABS(lp1->g - lp2->g) > thresh || 
				    ABS(lp1->b - lp2->b) > thresh || 
				    ABS(lp1->a - lp2->a) > thresh)
				{
					return MESHCMP_LOOPCOLMISMATCH;
				}
			}
		}

		if (l1->type == CD_MDEFORMVERT) {
			MDeformVert *dv1 = l1->data;
			MDeformVert *dv2 = l2->data;
			int dvtot = m1->totvert;
		
			for (j = 0; j < dvtot; j++, dv1++, dv2++) {
				int k;
				MDeformWeight *dw1 = dv1->dw, *dw2 = dv2->dw;
				
				if (dv1->totweight != dv2->totweight)
					return MESHCMP_DVERT_TOTGROUPMISMATCH;
				
				for (k = 0; k < dv1->totweight; k++, dw1++, dw2++) {
					if (dw1->def_nr != dw2->def_nr)
						return MESHCMP_DVERT_GROUPMISMATCH;
					if (fabsf(dw1->weight - dw2->weight) > thresh)
						return MESHCMP_DVERT_WEIGHTMISMATCH;
				}
			}
		}
	}
	
	return 0;
}

/**
 * Used for unit testing; compares two meshes, checking only
 * differences we care about.  should be usable with leaf's
 * testing framework I get RNA work done, will use hackish
 * testing code for now.
 */
const char *BKE_mesh_cmp(Mesh *me1, Mesh *me2, float thresh)
{
	int c;
	
	if (!me1 || !me2)
		return "Requires two input meshes";
	
	if (me1->totvert != me2->totvert) 
		return "Number of verts don't match";
	
	if (me1->totedge != me2->totedge)
		return "Number of edges don't match";
	
	if (me1->totpoly != me2->totpoly)
		return "Number of faces don't match";
				
	if (me1->totloop != me2->totloop)
		return "Number of loops don't match";
	
	if ((c = customdata_compare(&me1->vdata, &me2->vdata, me1, me2, thresh)))
		return cmpcode_to_str(c);

	if ((c = customdata_compare(&me1->edata, &me2->edata, me1, me2, thresh)))
		return cmpcode_to_str(c);

	if ((c = customdata_compare(&me1->ldata, &me2->ldata, me1, me2, thresh)))
		return cmpcode_to_str(c);

	if ((c = customdata_compare(&me1->pdata, &me2->pdata, me1, me2, thresh)))
		return cmpcode_to_str(c);
	
	return NULL;
}

static void mesh_ensure_tessellation_customdata(Mesh *me)
{
	if (UNLIKELY((me->totface != 0) && (me->totpoly == 0))) {
		/* Pass, otherwise this function  clears 'mface' before
		 * versioning 'mface -> mpoly' code kicks in [#30583]
		 *
		 * Callers could also check but safer to do here - campbell */
	}
	else {
		const int tottex_original = CustomData_number_of_layers(&me->pdata, CD_MTEXPOLY);
		const int totcol_original = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);

		const int tottex_tessface = CustomData_number_of_layers(&me->fdata, CD_MTFACE);
		const int totcol_tessface = CustomData_number_of_layers(&me->fdata, CD_MCOL);

		if (tottex_tessface != tottex_original ||
		    totcol_tessface != totcol_original)
		{
			BKE_mesh_tessface_clear(me);

			CustomData_from_bmeshpoly(&me->fdata, &me->pdata, &me->ldata, me->totface);

			/* TODO - add some --debug-mesh option */
			if (G.debug & G_DEBUG) {
				/* note: this warning may be un-called for if we are initializing the mesh for the
				 * first time from bmesh, rather then giving a warning about this we could be smarter
				 * and check if there was any data to begin with, for now just print the warning with
				 * some info to help troubleshoot whats going on - campbell */
				printf("%s: warning! Tessellation uvs or vcol data got out of sync, "
				       "had to reset!\n    CD_MTFACE: %d != CD_MTEXPOLY: %d || CD_MCOL: %d != CD_MLOOPCOL: %d\n",
				       __func__, tottex_tessface, tottex_original, totcol_tessface, totcol_original);
			}
		}
	}
}

void BKE_mesh_ensure_skin_customdata(Mesh *me)
{
	BMesh *bm = me->edit_btmesh ? me->edit_btmesh->bm : NULL;
	MVertSkin *vs;

	if (bm) {
		if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
			BMVert *v;
			BMIter iter;

			BM_data_layer_add(bm, &bm->vdata, CD_MVERT_SKIN);

			/* Mark an arbitrary vertex as root */
			BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
				vs = CustomData_bmesh_get(&bm->vdata, v->head.data,
				                          CD_MVERT_SKIN);
				vs->flag |= MVERT_SKIN_ROOT;
				break;
			}
		}
	}
	else {
		if (!CustomData_has_layer(&me->vdata, CD_MVERT_SKIN)) {
			vs = CustomData_add_layer(&me->vdata,
			                          CD_MVERT_SKIN,
			                          CD_DEFAULT,
			                          NULL,
			                          me->totvert);

			/* Mark an arbitrary vertex as root */
			if (vs) {
				vs->flag |= MVERT_SKIN_ROOT;
			}
		}
	}
}

/* this ensures grouped customdata (e.g. mtexpoly and mloopuv and mtface, or
 * mloopcol and mcol) have the same relative active/render/clone/mask indices.
 *
 * note that for undo mesh data we want to skip 'ensure_tess_cd' call since
 * we don't want to store memory for tessface when its only used for older
 * versions of the mesh. - campbell*/
static void mesh_update_linked_customdata(Mesh *me, const bool do_ensure_tess_cd)
{
	if (me->edit_btmesh)
		BKE_editmesh_update_linked_customdata(me->edit_btmesh);

	if (do_ensure_tess_cd) {
		mesh_ensure_tessellation_customdata(me);
	}

	CustomData_bmesh_update_active_layers(&me->fdata, &me->pdata, &me->ldata);
}

void BKE_mesh_update_customdata_pointers(Mesh *me, const bool do_ensure_tess_cd)
{
	mesh_update_linked_customdata(me, do_ensure_tess_cd);

	me->mvert = CustomData_get_layer(&me->vdata, CD_MVERT);
	me->dvert = CustomData_get_layer(&me->vdata, CD_MDEFORMVERT);

	me->medge = CustomData_get_layer(&me->edata, CD_MEDGE);

	me->mface = CustomData_get_layer(&me->fdata, CD_MFACE);
	me->mcol = CustomData_get_layer(&me->fdata, CD_MCOL);
	me->mtface = CustomData_get_layer(&me->fdata, CD_MTFACE);
	
	me->mpoly = CustomData_get_layer(&me->pdata, CD_MPOLY);
	me->mloop = CustomData_get_layer(&me->ldata, CD_MLOOP);

	me->mtpoly = CustomData_get_layer(&me->pdata, CD_MTEXPOLY);
	me->mloopcol = CustomData_get_layer(&me->ldata, CD_MLOOPCOL);
	me->mloopuv = CustomData_get_layer(&me->ldata, CD_MLOOPUV);
}

/* Note: unlinking is called when me->id.us is 0, question remains how
 * much unlinking of Library data in Mesh should be done... probably
 * we need a more generic method, like the expand() functions in
 * readfile.c */

void BKE_mesh_unlink(Mesh *me)
{
	int a;
	
	if (me == NULL) return;

	if (me->mat) {
		for (a = 0; a < me->totcol; a++) {
			if (me->mat[a]) me->mat[a]->id.us--;
			me->mat[a] = NULL;
		}
	}

	if (me->key) {
		me->key->id.us--;
	}
	me->key = NULL;
	
	if (me->texcomesh) me->texcomesh = NULL;
}

/* do not free mesh itself */
void BKE_mesh_free(Mesh *me, int unlink)
{
	if (unlink)
		BKE_mesh_unlink(me);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	if (me->adt) {
		BKE_free_animdata(&me->id);
		me->adt = NULL;
	}
	
	if (me->mat) MEM_freeN(me->mat);
	
	if (me->bb) MEM_freeN(me->bb);
	if (me->mselect) MEM_freeN(me->mselect);
	if (me->edit_btmesh) MEM_freeN(me->edit_btmesh);
}

static void mesh_tessface_clear_intern(Mesh *mesh, int free_customdata)
{
	if (free_customdata) {
		CustomData_free(&mesh->fdata, mesh->totface);
	}
	else {
		CustomData_reset(&mesh->fdata);
	}

	mesh->mface = NULL;
	mesh->mtface = NULL;
	mesh->mcol = NULL;
	mesh->totface = 0;
}

Mesh *BKE_mesh_add(Main *bmain, const char *name)
{
	Mesh *me;
	
	me = BKE_libblock_alloc(bmain, ID_ME, name);
	
	me->size[0] = me->size[1] = me->size[2] = 1.0;
	me->smoothresh = 30;
	me->texflag = ME_AUTOSPACE;

	/* disable because its slow on many GPU's, see [#37518] */
#if 0
	me->flag = ME_TWOSIDED;
#endif
	me->drawflag = ME_DRAWEDGES | ME_DRAWFACES | ME_DRAWCREASES;

	CustomData_reset(&me->vdata);
	CustomData_reset(&me->edata);
	CustomData_reset(&me->fdata);
	CustomData_reset(&me->pdata);
	CustomData_reset(&me->ldata);

	return me;
}

Mesh *BKE_mesh_copy_ex(Main *bmain, Mesh *me)
{
	Mesh *men;
	MTFace *tface;
	MTexPoly *txface;
	int a, i;
	const int do_tessface = ((me->totface != 0) && (me->totpoly == 0)); /* only do tessface if we have no polys */
	
	men = BKE_libblock_copy_ex(bmain, &me->id);
	
	men->mat = MEM_dupallocN(me->mat);
	for (a = 0; a < men->totcol; a++) {
		id_us_plus((ID *)men->mat[a]);
	}
	id_us_plus((ID *)men->texcomesh);

	CustomData_copy(&me->vdata, &men->vdata, CD_MASK_MESH, CD_DUPLICATE, men->totvert);
	CustomData_copy(&me->edata, &men->edata, CD_MASK_MESH, CD_DUPLICATE, men->totedge);
	CustomData_copy(&me->ldata, &men->ldata, CD_MASK_MESH, CD_DUPLICATE, men->totloop);
	CustomData_copy(&me->pdata, &men->pdata, CD_MASK_MESH, CD_DUPLICATE, men->totpoly);
	if (do_tessface) {
		CustomData_copy(&me->fdata, &men->fdata, CD_MASK_MESH, CD_DUPLICATE, men->totface);
	}
	else {
		mesh_tessface_clear_intern(men, false);
	}

	BKE_mesh_update_customdata_pointers(men, do_tessface);

	/* ensure indirect linked data becomes lib-extern */
	for (i = 0; i < me->fdata.totlayer; i++) {
		if (me->fdata.layers[i].type == CD_MTFACE) {
			tface = (MTFace *)me->fdata.layers[i].data;

			for (a = 0; a < me->totface; a++, tface++)
				if (tface->tpage)
					id_lib_extern((ID *)tface->tpage);
		}
	}
	
	for (i = 0; i < me->pdata.totlayer; i++) {
		if (me->pdata.layers[i].type == CD_MTEXPOLY) {
			txface = (MTexPoly *)me->pdata.layers[i].data;

			for (a = 0; a < me->totpoly; a++, txface++)
				if (txface->tpage)
					id_lib_extern((ID *)txface->tpage);
		}
	}

	men->edit_btmesh = NULL;

	men->mselect = MEM_dupallocN(men->mselect);
	men->bb = MEM_dupallocN(men->bb);
	
	men->key = BKE_key_copy(me->key);
	if (men->key) men->key->from = (ID *)men;

	return men;
}

Mesh *BKE_mesh_copy(Mesh *me)
{
	return BKE_mesh_copy_ex(G.main, me);
}

BMesh *BKE_mesh_to_bmesh(Mesh *me, Object *ob)
{
	BMesh *bm;
	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

	bm = BM_mesh_create(&allocsize);

	BM_mesh_bm_from_me(bm, me, false, true, ob->shapenr);

	return bm;
}

static void expand_local_mesh(Mesh *me)
{
	id_lib_extern((ID *)me->texcomesh);

	if (me->mtface || me->mtpoly) {
		int a, i;

		for (i = 0; i < me->pdata.totlayer; i++) {
			if (me->pdata.layers[i].type == CD_MTEXPOLY) {
				MTexPoly *txface = (MTexPoly *)me->pdata.layers[i].data;

				for (a = 0; a < me->totpoly; a++, txface++) {
					/* special case: ima always local immediately */
					if (txface->tpage) {
						id_lib_extern((ID *)txface->tpage);
					}
				}
			}
		}

		for (i = 0; i < me->fdata.totlayer; i++) {
			if (me->fdata.layers[i].type == CD_MTFACE) {
				MTFace *tface = (MTFace *)me->fdata.layers[i].data;

				for (a = 0; a < me->totface; a++, tface++) {
					/* special case: ima always local immediately */
					if (tface->tpage) {
						id_lib_extern((ID *)tface->tpage);
					}
				}
			}
		}
	}

	if (me->mat) {
		extern_local_matarar(me->mat, me->totcol);
	}
}

void BKE_mesh_make_local(Mesh *me)
{
	Main *bmain = G.main;
	Object *ob;
	bool is_local = false, is_lib = false;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (me->id.lib == NULL) return;
	if (me->id.us == 1) {
		id_clear_lib_data(bmain, &me->id);
		expand_local_mesh(me);
		return;
	}

	for (ob = bmain->object.first; ob && ELEM(0, is_lib, is_local); ob = ob->id.next) {
		if (me == ob->data) {
			if (ob->id.lib) is_lib = true;
			else is_local = true;
		}
	}

	if (is_local && is_lib == false) {
		id_clear_lib_data(bmain, &me->id);
		expand_local_mesh(me);
	}
	else if (is_local && is_lib) {
		Mesh *me_new = BKE_mesh_copy(me);
		me_new->id.us = 0;


		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, me->id.lib, &me_new->id);

		for (ob = bmain->object.first; ob; ob = ob->id.next) {
			if (me == ob->data) {
				if (ob->id.lib == NULL) {
					BKE_mesh_assign_object(ob, me_new);
				}
			}
		}
	}
}

bool BKE_mesh_uv_cdlayer_rename_index(Mesh *me, const int poly_index, const int loop_index, const int face_index,
                                      const char *new_name, const bool do_tessface)
{
	CustomData *pdata, *ldata, *fdata;
	CustomDataLayer *cdlp, *cdlu, *cdlf;
	const int step = do_tessface ? 3 : 2;
	int i;

	if (me->edit_btmesh) {
		pdata = &me->edit_btmesh->bm->pdata;
		ldata = &me->edit_btmesh->bm->ldata;
		fdata = NULL;  /* No tessellated data in BMesh! */
	}
	else {
		pdata = &me->pdata;
		ldata = &me->ldata;
		fdata = &me->fdata;
	}
	cdlp = &pdata->layers[poly_index];
	cdlu = &ldata->layers[loop_index];
	cdlf = fdata && do_tessface ? &fdata->layers[face_index] : NULL;

	if (cdlp->name != new_name) {
		/* Mesh validate passes a name from the CD layer as the new name,
		 * Avoid memcpy from self to self in this case.
		 */
		BLI_strncpy(cdlp->name, new_name, sizeof(cdlp->name));
		CustomData_set_layer_unique_name(pdata, cdlp - pdata->layers);
	}

	/* Loop until we do have exactly the same name for all layers! */
	for (i = 1; (strcmp(cdlp->name, cdlu->name) != 0 || (cdlf && strcmp(cdlp->name, cdlf->name) != 0)); i++) {
		switch (i % step) {
			case 0:
				BLI_strncpy(cdlp->name, cdlu->name, sizeof(cdlp->name));
				CustomData_set_layer_unique_name(pdata, cdlp - pdata->layers);
				break;
			case 1:
				BLI_strncpy(cdlu->name, cdlp->name, sizeof(cdlu->name));
				CustomData_set_layer_unique_name(ldata, cdlu - ldata->layers);
				break;
			case 2:
				if (cdlf) {
					BLI_strncpy(cdlf->name, cdlp->name, sizeof(cdlf->name));
					CustomData_set_layer_unique_name(fdata, cdlf - fdata->layers);
				}
				break;
		}
	}

	return true;
}

bool BKE_mesh_uv_cdlayer_rename(Mesh *me, const char *old_name, const char *new_name, bool do_tessface)
{
	CustomData *pdata, *ldata, *fdata;
	if (me->edit_btmesh) {
		pdata = &me->edit_btmesh->bm->pdata;
		ldata = &me->edit_btmesh->bm->ldata;
		/* No tessellated data in BMesh! */
		fdata = NULL;
		do_tessface = false;
	}
	else {
		pdata = &me->pdata;
		ldata = &me->ldata;
		fdata = &me->fdata;
		do_tessface = (do_tessface && fdata->totlayer);
	}

	{
		const int pidx_start = CustomData_get_layer_index(pdata, CD_MTEXPOLY);
		const int lidx_start = CustomData_get_layer_index(ldata, CD_MLOOPUV);
		const int fidx_start = do_tessface ? CustomData_get_layer_index(fdata, CD_MTFACE) : -1;
		int pidx = CustomData_get_named_layer(pdata, CD_MTEXPOLY, old_name);
		int lidx = CustomData_get_named_layer(ldata, CD_MLOOPUV, old_name);
		int fidx = do_tessface ? CustomData_get_named_layer(fdata, CD_MTFACE, old_name) : -1;

		/* None of those cases should happen, in theory!
		 * Note this assume we have the same number of mtexpoly, mloopuv and mtface layers!
		 */
		if (pidx == -1) {
			if (lidx == -1) {
				if (fidx == -1) {
					/* No layer found with this name! */
					return false;
				}
				else {
					lidx = fidx;
				}
			}
			pidx = lidx;
		}
		else {
			if (lidx == -1) {
				lidx = pidx;
			}
			if (fidx == -1 && do_tessface) {
				fidx = pidx;
			}
		}
#if 0
		/* For now, we do not consider mismatch in indices (i.e. same name leading to (relative) different indices). */
		else if (pidx != lidx) {
			lidx = pidx;
		}
#endif

		/* Go back to absolute indices! */
		pidx += pidx_start;
		lidx += lidx_start;
		if (fidx != -1)
			fidx += fidx_start;

		return BKE_mesh_uv_cdlayer_rename_index(me, pidx, lidx, fidx, new_name, do_tessface);
	}
}

void BKE_mesh_boundbox_calc(Mesh *me, float r_loc[3], float r_size[3])
{
	BoundBox *bb;
	float min[3], max[3];
	float mloc[3], msize[3];
	
	if (me->bb == NULL) me->bb = MEM_callocN(sizeof(BoundBox), "boundbox");
	bb = me->bb;

	if (!r_loc) r_loc = mloc;
	if (!r_size) r_size = msize;
	
	INIT_MINMAX(min, max);
	if (!BKE_mesh_minmax(me, min, max)) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}

	mid_v3_v3v3(r_loc, min, max);
		
	r_size[0] = (max[0] - min[0]) / 2.0f;
	r_size[1] = (max[1] - min[1]) / 2.0f;
	r_size[2] = (max[2] - min[2]) / 2.0f;
	
	BKE_boundbox_init_from_minmax(bb, min, max);

	bb->flag &= ~BOUNDBOX_DIRTY;
}

void BKE_mesh_texspace_calc(Mesh *me)
{
	float loc[3], size[3];
	int a;

	BKE_mesh_boundbox_calc(me, loc, size);

	if (me->texflag & ME_AUTOSPACE) {
		for (a = 0; a < 3; a++) {
			if (size[a] == 0.0f) size[a] = 1.0f;
			else if (size[a] > 0.0f && size[a] < 0.00001f) size[a] = 0.00001f;
			else if (size[a] < 0.0f && size[a] > -0.00001f) size[a] = -0.00001f;
		}

		copy_v3_v3(me->loc, loc);
		copy_v3_v3(me->size, size);
		zero_v3(me->rot);
	}
}

BoundBox *BKE_mesh_boundbox_get(Object *ob)
{
	Mesh *me = ob->data;

	if (ob->bb)
		return ob->bb;

	if (me->bb == NULL || (me->bb->flag & BOUNDBOX_DIRTY)) {
		BKE_mesh_texspace_calc(me);
	}

	return me->bb;
}

void BKE_mesh_texspace_get(Mesh *me, float r_loc[3], float r_rot[3], float r_size[3])
{
	if (me->bb == NULL || (me->bb->flag & BOUNDBOX_DIRTY)) {
		BKE_mesh_texspace_calc(me);
	}

	if (r_loc) copy_v3_v3(r_loc,  me->loc);
	if (r_rot) copy_v3_v3(r_rot,  me->rot);
	if (r_size) copy_v3_v3(r_size, me->size);
}

void BKE_mesh_texspace_copy_from_object(Mesh *me, Object *ob)
{
	float *texloc, *texrot, *texsize;
	short *texflag;

	if (BKE_object_obdata_texspace_get(ob, &texflag, &texloc, &texsize, &texrot)) {
		me->texflag = *texflag;
		copy_v3_v3(me->loc, texloc);
		copy_v3_v3(me->size, texsize);
		copy_v3_v3(me->rot, texrot);
	}
}

float (*BKE_mesh_orco_verts_get(Object *ob))[3]
{
	Mesh *me = ob->data;
	MVert *mvert = NULL;
	Mesh *tme = me->texcomesh ? me->texcomesh : me;
	int a, totvert;
	float (*vcos)[3] = NULL;

	/* Get appropriate vertex coordinates */
	vcos = MEM_callocN(sizeof(*vcos) * me->totvert, "orco mesh");
	mvert = tme->mvert;
	totvert = min_ii(tme->totvert, me->totvert);

	for (a = 0; a < totvert; a++, mvert++) {
		copy_v3_v3(vcos[a], mvert->co);
	}

	return vcos;
}

void BKE_mesh_orco_verts_transform(Mesh *me, float (*orco)[3], int totvert, int invert)
{
	float loc[3], size[3];
	int a;

	BKE_mesh_texspace_get(me->texcomesh ? me->texcomesh : me, loc, NULL, size);

	if (invert) {
		for (a = 0; a < totvert; a++) {
			float *co = orco[a];
			madd_v3_v3v3v3(co, loc, co, size);
		}
	}
	else {
		for (a = 0; a < totvert; a++) {
			float *co = orco[a];
			co[0] = (co[0] - loc[0]) / size[0];
			co[1] = (co[1] - loc[1]) / size[1];
			co[2] = (co[2] - loc[2]) / size[2];
		}
	}
}

/* rotates the vertices of a face in case v[2] or v[3] (vertex index) is = 0.
 * this is necessary to make the if (mface->v4) check for quads work */
int test_index_face(MFace *mface, CustomData *fdata, int mfindex, int nr)
{
	/* first test if the face is legal */
	if ((mface->v3 || nr == 4) && mface->v3 == mface->v4) {
		mface->v4 = 0;
		nr--;
	}
	if ((mface->v2 || mface->v4) && mface->v2 == mface->v3) {
		mface->v3 = mface->v4;
		mface->v4 = 0;
		nr--;
	}
	if (mface->v1 == mface->v2) {
		mface->v2 = mface->v3;
		mface->v3 = mface->v4;
		mface->v4 = 0;
		nr--;
	}

	/* check corrupt cases, bow-tie geometry, cant handle these because edge data wont exist so just return 0 */
	if (nr == 3) {
		if (
		    /* real edges */
		    mface->v1 == mface->v2 ||
		    mface->v2 == mface->v3 ||
		    mface->v3 == mface->v1)
		{
			return 0;
		}
	}
	else if (nr == 4) {
		if (
		    /* real edges */
		    mface->v1 == mface->v2 ||
		    mface->v2 == mface->v3 ||
		    mface->v3 == mface->v4 ||
		    mface->v4 == mface->v1 ||
		    /* across the face */
		    mface->v1 == mface->v3 ||
		    mface->v2 == mface->v4)
		{
			return 0;
		}
	}

	/* prevent a zero at wrong index location */
	if (nr == 3) {
		if (mface->v3 == 0) {
			static int corner_indices[4] = {1, 2, 0, 3};

			SWAP(unsigned int, mface->v1, mface->v2);
			SWAP(unsigned int, mface->v2, mface->v3);

			if (fdata)
				CustomData_swap(fdata, mfindex, corner_indices);
		}
	}
	else if (nr == 4) {
		if (mface->v3 == 0 || mface->v4 == 0) {
			static int corner_indices[4] = {2, 3, 0, 1};

			SWAP(unsigned int, mface->v1, mface->v3);
			SWAP(unsigned int, mface->v2, mface->v4);

			if (fdata)
				CustomData_swap(fdata, mfindex, corner_indices);
		}
	}

	return nr;
}

Mesh *BKE_mesh_from_object(Object *ob)
{
	
	if (ob == NULL) return NULL;
	if (ob->type == OB_MESH) return ob->data;
	else return NULL;
}

void BKE_mesh_assign_object(Object *ob, Mesh *me)
{
	Mesh *old = NULL;

	multires_force_update(ob);
	
	if (ob == NULL) return;
	
	if (ob->type == OB_MESH) {
		old = ob->data;
		if (old)
			old->id.us--;
		ob->data = me;
		id_us_plus((ID *)me);
	}
	
	test_object_materials(G.main, (ID *)me);

	test_object_modifiers(ob);
}

void BKE_mesh_from_metaball(ListBase *lb, Mesh *me)
{
	DispList *dl;
	MVert *mvert;
	MLoop *mloop, *allloop;
	MPoly *mpoly;
	const float *nors, *verts;
	int a, *index;
	
	dl = lb->first;
	if (dl == NULL) return;

	if (dl->type == DL_INDEX4) {
		mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, dl->nr);
		allloop = mloop = CustomData_add_layer(&me->ldata, CD_MLOOP, CD_CALLOC, NULL, dl->parts * 4);
		mpoly = CustomData_add_layer(&me->pdata, CD_MPOLY, CD_CALLOC, NULL, dl->parts);
		me->mvert = mvert;
		me->mloop = mloop;
		me->mpoly = mpoly;
		me->totvert = dl->nr;
		me->totpoly = dl->parts;

		a = dl->nr;
		nors = dl->nors;
		verts = dl->verts;
		while (a--) {
			copy_v3_v3(mvert->co, verts);
			normal_float_to_short_v3(mvert->no, nors);
			mvert++;
			nors += 3;
			verts += 3;
		}
		
		a = dl->parts;
		index = dl->index;
		while (a--) {
			int count = index[2] != index[3] ? 4 : 3;

			mloop[0].v = index[0];
			mloop[1].v = index[1];
			mloop[2].v = index[2];
			if (count == 4)
				mloop[3].v = index[3];

			mpoly->totloop = count;
			mpoly->loopstart = (int)(mloop - allloop);
			mpoly->flag = ME_SMOOTH;


			mpoly++;
			mloop += count;
			me->totloop += count;
			index += 4;
		}

		BKE_mesh_update_customdata_pointers(me, true);

		BKE_mesh_calc_normals(me);

		BKE_mesh_calc_edges(me, true, false);
	}
}

/**
 * Specialized function to use when we _know_ existing edges don't overlap with poly edges.
 */
static void make_edges_mdata_extend(MEdge **r_alledge, int *r_totedge,
                                    const MPoly *mpoly, MLoop *mloop,
                                    const int totpoly)
{
	int totedge = *r_totedge;
	int totedge_new;
	EdgeHash *eh;
	unsigned int eh_reserve;
	const MPoly *mp;
	int i;

	eh_reserve = max_ii(totedge, BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totpoly));
	eh = BLI_edgehash_new_ex(__func__, eh_reserve);

	for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
		BKE_mesh_poly_edgehash_insert(eh, mp, mloop + mp->loopstart);
	}

	totedge_new = BLI_edgehash_size(eh);

#ifdef DEBUG
	/* ensure that theres no overlap! */
	if (totedge_new) {
		MEdge *medge = *r_alledge;
		for (i = 0; i < totedge; i++, medge++) {
			BLI_assert(BLI_edgehash_haskey(eh, medge->v1, medge->v2) == false);
		}
	}
#endif

	if (totedge_new) {
		EdgeHashIterator *ehi;
		MEdge *medge;
		unsigned int e_index = totedge;

		*r_alledge = medge = (*r_alledge ? MEM_reallocN(*r_alledge, sizeof(MEdge) * (totedge + totedge_new)) :
		                                   MEM_callocN(sizeof(MEdge) * totedge_new, __func__));
		medge += totedge;

		totedge += totedge_new;

		/* --- */
		for (ehi = BLI_edgehashIterator_new(eh);
		     BLI_edgehashIterator_isDone(ehi) == false;
		     BLI_edgehashIterator_step(ehi), ++medge, e_index++)
		{
			BLI_edgehashIterator_getKey(ehi, &medge->v1, &medge->v2);
			BLI_edgehashIterator_setValue(ehi, SET_UINT_IN_POINTER(e_index));

			medge->crease = medge->bweight = 0;
			medge->flag = ME_EDGEDRAW | ME_EDGERENDER;
		}
		BLI_edgehashIterator_free(ehi);

		*r_totedge = totedge;


		for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
			MLoop *l = &mloop[mp->loopstart];
			MLoop *l_prev = (l + (mp->totloop - 1));
			int j;
			for (j = 0; j < mp->totloop; j++, l++) {
				/* lookup hashed edge index */
				l_prev->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(eh, l_prev->v, l->v));
				l_prev = l;
			}
		}
	}

	BLI_edgehash_free(eh, NULL);
}


/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* return non-zero on error */
int BKE_mesh_nurbs_to_mdata(Object *ob, MVert **allvert, int *totvert,
                            MEdge **alledge, int *totedge, MLoop **allloop, MPoly **allpoly,
                            int *totloop, int *totpoly)
{
	ListBase disp = {NULL, NULL};

	if (ob->curve_cache) {
		disp = ob->curve_cache->disp;
	}

	return BKE_mesh_nurbs_displist_to_mdata(ob, &disp,
	                                        allvert, totvert,
	                                        alledge, totedge,
	                                        allloop, allpoly, NULL,
	                                        totloop, totpoly);
}

/* BMESH: this doesn't calculate all edges from polygons,
 * only free standing edges are calculated */

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* use specified dispbase */
int BKE_mesh_nurbs_displist_to_mdata(Object *ob, ListBase *dispbase,
                                     MVert **allvert, int *_totvert,
                                     MEdge **alledge, int *_totedge,
                                     MLoop **allloop, MPoly **allpoly,
                                     MLoopUV **alluv,
                                     int *_totloop, int *_totpoly)
{
	Curve *cu = ob->data;
	DispList *dl;
	MVert *mvert;
	MPoly *mpoly;
	MLoop *mloop;
	MLoopUV *mloopuv = NULL;
	MEdge *medge;
	const float *data;
	int a, b, ofs, vertcount, startvert, totvert = 0, totedge = 0, totloop = 0, totvlak = 0;
	int p1, p2, p3, p4, *index;
	const bool conv_polys = ((CU_DO_2DFILL(cu) == false) ||  /* 2d polys are filled with DL_INDEX3 displists */
	                         (ob->type == OB_SURF));  /* surf polys are never filled */

	/* count */
	dl = dispbase->first;
	while (dl) {
		if (dl->type == DL_SEGM) {
			totvert += dl->parts * dl->nr;
			totedge += dl->parts * (dl->nr - 1);
		}
		else if (dl->type == DL_POLY) {
			if (conv_polys) {
				totvert += dl->parts * dl->nr;
				totedge += dl->parts * dl->nr;
			}
		}
		else if (dl->type == DL_SURF) {
			int tot;
			totvert += dl->parts * dl->nr;
			tot = (dl->parts - 1 + ((dl->flag & DL_CYCL_V) == 2)) * (dl->nr - 1 + (dl->flag & DL_CYCL_U));
			totvlak += tot;
			totloop += tot * 4;
		}
		else if (dl->type == DL_INDEX3) {
			int tot;
			totvert += dl->nr;
			tot = dl->parts;
			totvlak += tot;
			totloop += tot * 3;
		}
		dl = dl->next;
	}

	if (totvert == 0) {
		/* error("can't convert"); */
		/* Make Sure you check ob->data is a curve */
		return -1;
	}

	*allvert = mvert = MEM_callocN(sizeof(MVert) * totvert, "nurbs_init mvert");
	*alledge = medge = MEM_callocN(sizeof(MEdge) * totedge, "nurbs_init medge");
	*allloop = mloop = MEM_callocN(sizeof(MLoop) * totvlak * 4, "nurbs_init mloop"); // totloop
	*allpoly = mpoly = MEM_callocN(sizeof(MPoly) * totvlak, "nurbs_init mloop");

	if (alluv)
		*alluv = mloopuv = MEM_callocN(sizeof(MLoopUV) * totvlak * 4, "nurbs_init mloopuv");
	
	/* verts and faces */
	vertcount = 0;

	dl = dispbase->first;
	while (dl) {
		int smooth = dl->rt & CU_SMOOTH ? 1 : 0;

		if (dl->type == DL_SEGM) {
			startvert = vertcount;
			a = dl->parts * dl->nr;
			data = dl->verts;
			while (a--) {
				copy_v3_v3(mvert->co, data);
				data += 3;
				vertcount++;
				mvert++;
			}

			for (a = 0; a < dl->parts; a++) {
				ofs = a * dl->nr;
				for (b = 1; b < dl->nr; b++) {
					medge->v1 = startvert + ofs + b - 1;
					medge->v2 = startvert + ofs + b;
					medge->flag = ME_LOOSEEDGE | ME_EDGERENDER | ME_EDGEDRAW;

					medge++;
				}
			}

		}
		else if (dl->type == DL_POLY) {
			if (conv_polys) {
				startvert = vertcount;
				a = dl->parts * dl->nr;
				data = dl->verts;
				while (a--) {
					copy_v3_v3(mvert->co, data);
					data += 3;
					vertcount++;
					mvert++;
				}

				for (a = 0; a < dl->parts; a++) {
					ofs = a * dl->nr;
					for (b = 0; b < dl->nr; b++) {
						medge->v1 = startvert + ofs + b;
						if (b == dl->nr - 1) medge->v2 = startvert + ofs;
						else medge->v2 = startvert + ofs + b + 1;
						medge->flag = ME_LOOSEEDGE | ME_EDGERENDER | ME_EDGEDRAW;
						medge++;
					}
				}
			}
		}
		else if (dl->type == DL_INDEX3) {
			startvert = vertcount;
			a = dl->nr;
			data = dl->verts;
			while (a--) {
				copy_v3_v3(mvert->co, data);
				data += 3;
				vertcount++;
				mvert++;
			}

			a = dl->parts;
			index = dl->index;
			while (a--) {
				mloop[0].v = startvert + index[0];
				mloop[1].v = startvert + index[2];
				mloop[2].v = startvert + index[1];
				mpoly->loopstart = (int)(mloop - (*allloop));
				mpoly->totloop = 3;
				mpoly->mat_nr = dl->col;

				if (mloopuv) {
					int i;

					for (i = 0; i < 3; i++, mloopuv++) {
						mloopuv->uv[0] = (mloop[i].v - startvert) / (float)(dl->nr - 1);
						mloopuv->uv[1] = 0.0f;
					}
				}

				if (smooth) mpoly->flag |= ME_SMOOTH;
				mpoly++;
				mloop += 3;
				index += 3;
			}
		}
		else if (dl->type == DL_SURF) {
			startvert = vertcount;
			a = dl->parts * dl->nr;
			data = dl->verts;
			while (a--) {
				copy_v3_v3(mvert->co, data);
				data += 3;
				vertcount++;
				mvert++;
			}

			for (a = 0; a < dl->parts; a++) {

				if ( (dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) break;

				if (dl->flag & DL_CYCL_U) {         /* p2 -> p1 -> */
					p1 = startvert + dl->nr * a;    /* p4 -> p3 -> */
					p2 = p1 + dl->nr - 1;       /* -----> next row */
					p3 = p1 + dl->nr;
					p4 = p2 + dl->nr;
					b = 0;
				}
				else {
					p2 = startvert + dl->nr * a;
					p1 = p2 + 1;
					p4 = p2 + dl->nr;
					p3 = p1 + dl->nr;
					b = 1;
				}
				if ( (dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
					p3 -= dl->parts * dl->nr;
					p4 -= dl->parts * dl->nr;
				}

				for (; b < dl->nr; b++) {
					mloop[0].v = p1;
					mloop[1].v = p3;
					mloop[2].v = p4;
					mloop[3].v = p2;
					mpoly->loopstart = (int)(mloop - (*allloop));
					mpoly->totloop = 4;
					mpoly->mat_nr = dl->col;

					if (mloopuv) {
						int orco_sizeu = dl->nr - 1;
						int orco_sizev = dl->parts - 1;
						int i;

						/* exception as handled in convertblender.c too */
						if (dl->flag & DL_CYCL_U) {
							orco_sizeu++;
							if (dl->flag & DL_CYCL_V)
								orco_sizev++;
						}
						else if (dl->flag & DL_CYCL_V) {
							orco_sizev++;
						}

						for (i = 0; i < 4; i++, mloopuv++) {
							/* find uv based on vertex index into grid array */
							int v = mloop[i].v - startvert;

							mloopuv->uv[0] = (v / dl->nr) / (float)orco_sizev;
							mloopuv->uv[1] = (v % dl->nr) / (float)orco_sizeu;

							/* cyclic correction */
							if ((i == 1 || i == 2) && mloopuv->uv[0] == 0.0f)
								mloopuv->uv[0] = 1.0f;
							if ((i == 0 || i == 1) && mloopuv->uv[1] == 0.0f)
								mloopuv->uv[1] = 1.0f;
						}
					}

					if (smooth) mpoly->flag |= ME_SMOOTH;
					mpoly++;
					mloop += 4;

					p4 = p3;
					p3++;
					p2 = p1;
					p1++;
				}
			}
		}

		dl = dl->next;
	}
	
	if (totvlak) {
		make_edges_mdata_extend(alledge, &totedge,
		                        *allpoly, *allloop, totvlak);
	}

	*_totpoly = totvlak;
	*_totloop = totloop;
	*_totedge = totedge;
	*_totvert = totvert;

	return 0;
}


/* this may fail replacing ob->data, be sure to check ob->type */
void BKE_mesh_from_nurbs_displist(Object *ob, ListBase *dispbase, const bool use_orco_uv)
{
	Main *bmain = G.main;
	Object *ob1;
	DerivedMesh *dm = ob->derivedFinal;
	Mesh *me;
	Curve *cu;
	MVert *allvert = NULL;
	MEdge *alledge = NULL;
	MLoop *allloop = NULL;
	MLoopUV *alluv = NULL;
	MPoly *allpoly = NULL;
	int totvert, totedge, totloop, totpoly;

	cu = ob->data;

	if (dm == NULL) {
		if (BKE_mesh_nurbs_displist_to_mdata(ob, dispbase, &allvert, &totvert,
		                                     &alledge, &totedge, &allloop,
		                                     &allpoly, (use_orco_uv) ? &alluv : NULL,
		                                     &totloop, &totpoly) != 0)
		{
			/* Error initializing */
			return;
		}

		/* make mesh */
		me = BKE_mesh_add(G.main, "Mesh");
		me->totvert = totvert;
		me->totedge = totedge;
		me->totloop = totloop;
		me->totpoly = totpoly;

		me->mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, allvert, me->totvert);
		me->medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, alledge, me->totedge);
		me->mloop = CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, allloop, me->totloop);
		me->mpoly = CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, allpoly, me->totpoly);

		if (alluv) {
			const char *uvname = "Orco";
			me->mtpoly = CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DEFAULT, NULL, me->totpoly, uvname);
			me->mloopuv = CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_ASSIGN, alluv, me->totloop, uvname);
		}

		BKE_mesh_calc_normals(me);
	}
	else {
		me = BKE_mesh_add(G.main, "Mesh");
		DM_to_mesh(dm, me, ob, CD_MASK_MESH);
	}

	me->totcol = cu->totcol;
	me->mat = cu->mat;

	BKE_mesh_texspace_calc(me);

	cu->mat = NULL;
	cu->totcol = 0;

	if (ob->data) {
		BKE_libblock_free(bmain, ob->data);
	}
	ob->data = me;
	ob->type = OB_MESH;

	/* other users */
	ob1 = bmain->object.first;
	while (ob1) {
		if (ob1->data == cu) {
			ob1->type = OB_MESH;
		
			ob1->data = ob->data;
			id_us_plus((ID *)ob->data);
		}
		ob1 = ob1->id.next;
	}
}

void BKE_mesh_from_nurbs(Object *ob)
{
	Curve *cu = (Curve *) ob->data;
	bool use_orco_uv = (cu->flag & CU_UV_ORCO) != 0;
	ListBase disp = {NULL, NULL};

	if (ob->curve_cache) {
		disp = ob->curve_cache->disp;
	}

	BKE_mesh_from_nurbs_displist(ob, &disp, use_orco_uv);
}

typedef struct EdgeLink {
	struct EdgeLink *next, *prev;
	void *edge;
} EdgeLink;

typedef struct VertLink {
	Link *next, *prev;
	unsigned int index;
} VertLink;

static void prependPolyLineVert(ListBase *lb, unsigned int index)
{
	VertLink *vl = MEM_callocN(sizeof(VertLink), "VertLink");
	vl->index = index;
	BLI_addhead(lb, vl);
}

static void appendPolyLineVert(ListBase *lb, unsigned int index)
{
	VertLink *vl = MEM_callocN(sizeof(VertLink), "VertLink");
	vl->index = index;
	BLI_addtail(lb, vl);
}

void BKE_mesh_to_curve_nurblist(DerivedMesh *dm, ListBase *nurblist, const int edge_users_test)
{
	MVert       *mvert = dm->getVertArray(dm);
	MEdge *med, *medge = dm->getEdgeArray(dm);
	MPoly *mp,  *mpoly = dm->getPolyArray(dm);
	MLoop       *mloop = dm->getLoopArray(dm);

	int dm_totedge = dm->getNumEdges(dm);
	int dm_totpoly = dm->getNumPolys(dm);
	int totedges = 0;
	int i;

	/* only to detect edge polylines */
	int *edge_users;

	ListBase edges = {NULL, NULL};

	/* get boundary edges */
	edge_users = MEM_callocN(sizeof(int) * dm_totedge, __func__);
	for (i = 0, mp = mpoly; i < dm_totpoly; i++, mp++) {
		MLoop *ml = &mloop[mp->loopstart];
		int j;
		for (j = 0; j < mp->totloop; j++, ml++) {
			edge_users[ml->e]++;
		}
	}

	/* create edges from all faces (so as to find edges not in any faces) */
	med = medge;
	for (i = 0; i < dm_totedge; i++, med++) {
		if (edge_users[i] == edge_users_test) {
			EdgeLink *edl = MEM_callocN(sizeof(EdgeLink), "EdgeLink");
			edl->edge = med;

			BLI_addtail(&edges, edl);   totedges++;
		}
	}
	MEM_freeN(edge_users);

	if (edges.first) {
		while (edges.first) {
			/* each iteration find a polyline and add this as a nurbs poly spline */

			ListBase polyline = {NULL, NULL}; /* store a list of VertLink's */
			bool closed = false;
			int totpoly = 0;
			MEdge *med_current = ((EdgeLink *)edges.last)->edge;
			unsigned int startVert = med_current->v1;
			unsigned int endVert = med_current->v2;
			bool ok = true;

			appendPolyLineVert(&polyline, startVert);   totpoly++;
			appendPolyLineVert(&polyline, endVert);     totpoly++;
			BLI_freelinkN(&edges, edges.last);          totedges--;

			while (ok) { /* while connected edges are found... */
				EdgeLink *edl = edges.last;
				ok = false;
				while (edl) {
					EdgeLink *edl_prev = edl->prev;

					med = edl->edge;

					if (med->v1 == endVert) {
						endVert = med->v2;
						appendPolyLineVert(&polyline, med->v2); totpoly++;
						BLI_freelinkN(&edges, edl);             totedges--;
						ok = true;
					}
					else if (med->v2 == endVert) {
						endVert = med->v1;
						appendPolyLineVert(&polyline, endVert); totpoly++;
						BLI_freelinkN(&edges, edl);             totedges--;
						ok = true;
					}
					else if (med->v1 == startVert) {
						startVert = med->v2;
						prependPolyLineVert(&polyline, startVert);  totpoly++;
						BLI_freelinkN(&edges, edl);                 totedges--;
						ok = true;
					}
					else if (med->v2 == startVert) {
						startVert = med->v1;
						prependPolyLineVert(&polyline, startVert);  totpoly++;
						BLI_freelinkN(&edges, edl);                 totedges--;
						ok = true;
					}

					edl = edl_prev;
				}
			}

			/* Now we have a polyline, make into a curve */
			if (startVert == endVert) {
				BLI_freelinkN(&polyline, polyline.last);
				totpoly--;
				closed = true;
			}

			/* --- nurbs --- */
			{
				Nurb *nu;
				BPoint *bp;
				VertLink *vl;

				/* create new 'nurb' within the curve */
				nu = (Nurb *)MEM_callocN(sizeof(Nurb), "MeshNurb");

				nu->pntsu = totpoly;
				nu->pntsv = 1;
				nu->orderu = 4;
				nu->flagu = CU_NURB_ENDPOINT | (closed ? CU_NURB_CYCLIC : 0);  /* endpoint */
				nu->resolu = 12;

				nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * totpoly, "bpoints");

				/* add points */
				vl = polyline.first;
				for (i = 0, bp = nu->bp; i < totpoly; i++, bp++, vl = (VertLink *)vl->next) {
					copy_v3_v3(bp->vec, mvert[vl->index].co);
					bp->f1 = SELECT;
					bp->radius = bp->weight = 1.0;
				}
				BLI_freelistN(&polyline);

				/* add nurb to curve */
				BLI_addtail(nurblist, nu);
			}
			/* --- done with nurbs --- */
		}
	}
}

void BKE_mesh_to_curve(Scene *scene, Object *ob)
{
	/* make new mesh data from the original copy */
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_MESH);
	ListBase nurblist = {NULL, NULL};
	bool needsFree = false;

	BKE_mesh_to_curve_nurblist(dm, &nurblist, 0);
	BKE_mesh_to_curve_nurblist(dm, &nurblist, 1);

	if (nurblist.first) {
		Curve *cu = BKE_curve_add(G.main, ob->id.name + 2, OB_CURVE);
		cu->flag |= CU_3D;

		cu->nurb = nurblist;

		((Mesh *)ob->data)->id.us--;
		ob->data = cu;
		ob->type = OB_CURVE;

		/* curve objects can't contain DM in usual cases, we could free memory */
		needsFree = true;
	}

	dm->needsFree = needsFree;
	dm->release(dm);

	if (needsFree) {
		ob->derivedFinal = NULL;

		/* curve object could have got bounding box only in special cases */
		if (ob->bb) {
			MEM_freeN(ob->bb);
			ob->bb = NULL;
		}
	}
}

void BKE_mesh_material_index_remove(Mesh *me, short index)
{
	MPoly *mp;
	MFace *mf;
	int i;

	for (mp = me->mpoly, i = 0; i < me->totpoly; i++, mp++) {
		if (mp->mat_nr && mp->mat_nr >= index) {
			mp->mat_nr--;
		}
	}

	for (mf = me->mface, i = 0; i < me->totface; i++, mf++) {
		if (mf->mat_nr && mf->mat_nr >= index) {
			mf->mat_nr--;
		}
	}
}

void BKE_mesh_material_index_clear(Mesh *me)
{
	MPoly *mp;
	MFace *mf;
	int i;

	for (mp = me->mpoly, i = 0; i < me->totpoly; i++, mp++) {
		mp->mat_nr = 0;
	}

	for (mf = me->mface, i = 0; i < me->totface; i++, mf++) {
		mf->mat_nr = 0;
	}
}

void BKE_mesh_smooth_flag_set(Object *meshOb, int enableSmooth) 
{
	Mesh *me = meshOb->data;
	int i;

	for (i = 0; i < me->totpoly; i++) {
		MPoly *mp = &((MPoly *) me->mpoly)[i];

		if (enableSmooth) {
			mp->flag |= ME_SMOOTH;
		}
		else {
			mp->flag &= ~ME_SMOOTH;
		}
	}
	
	for (i = 0; i < me->totface; i++) {
		MFace *mf = &((MFace *) me->mface)[i];

		if (enableSmooth) {
			mf->flag |= ME_SMOOTH;
		}
		else {
			mf->flag &= ~ME_SMOOTH;
		}
	}
}

/**
 * Return a newly MEM_malloc'd array of all the mesh vertex locations
 * \note \a r_numVerts may be NULL
 */
float (*BKE_mesh_vertexCos_get(Mesh *me, int *r_numVerts))[3]
{
	int i, numVerts = me->totvert;
	float (*cos)[3] = MEM_mallocN(sizeof(*cos) * numVerts, "vertexcos1");

	if (r_numVerts) *r_numVerts = numVerts;
	for (i = 0; i < numVerts; i++)
		copy_v3_v3(cos[i], me->mvert[i].co);

	return cos;
}

/**
 * Find the index of the loop in 'poly' which references vertex,
 * returns -1 if not found
 */
int poly_find_loop_from_vert(const MPoly *poly, const MLoop *loopstart,
                             unsigned vert)
{
	int j;
	for (j = 0; j < poly->totloop; j++, loopstart++) {
		if (loopstart->v == vert)
			return j;
	}
	
	return -1;
}

/**
 * Fill \a r_adj with the loop indices in \a poly adjacent to the
 * vertex. Returns the index of the loop matching vertex, or -1 if the
 * vertex is not in \a poly
 */
int poly_get_adj_loops_from_vert(unsigned r_adj[3], const MPoly *poly,
                                 const MLoop *mloop, unsigned vert)
{
	int corner = poly_find_loop_from_vert(poly,
	                                      &mloop[poly->loopstart],
	                                      vert);
		
	if (corner != -1) {
		const MLoop *ml = &mloop[poly->loopstart + corner];

		/* vertex was found */
		r_adj[0] = ME_POLY_LOOP_PREV(mloop, poly, corner)->v;
		r_adj[1] = ml->v;
		r_adj[2] = ME_POLY_LOOP_NEXT(mloop, poly, corner)->v;
	}

	return corner;
}

/**
 * Return the index of the edge vert that is not equal to \a v. If
 * neither edge vertex is equal to \a v, returns -1.
 */
int BKE_mesh_edge_other_vert(const MEdge *e, int v)
{
	if (e->v1 == v)
		return e->v2;
	else if (e->v2 == v)
		return e->v1;
	else
		return -1;
}

/* basic vertex data functions */
bool BKE_mesh_minmax(Mesh *me, float r_min[3], float r_max[3])
{
	int i = me->totvert;
	MVert *mvert;
	for (mvert = me->mvert; i--; mvert++) {
		minmax_v3v3_v3(r_min, r_max, mvert->co);
	}
	
	return (me->totvert != 0);
}

void BKE_mesh_transform(Mesh *me, float mat[4][4], bool do_keys)
{
	int i;
	MVert *mvert = me->mvert;

	for (i = 0; i < me->totvert; i++, mvert++)
		mul_m4_v3(mat, mvert->co);

	if (do_keys && me->key) {
		KeyBlock *kb;
		for (kb = me->key->block.first; kb; kb = kb->next) {
			float *fp = kb->data;
			for (i = kb->totelem; i--; fp += 3) {
				mul_m4_v3(mat, fp);
			}
		}
	}

	/* don't update normals, caller can do this explicitly */
}

void BKE_mesh_translate(Mesh *me, const float offset[3], const bool do_keys)
{
	int i = me->totvert;
	MVert *mvert;
	for (mvert = me->mvert; i--; mvert++) {
		add_v3_v3(mvert->co, offset);
	}
	
	if (do_keys && me->key) {
		KeyBlock *kb;
		for (kb = me->key->block.first; kb; kb = kb->next) {
			float *fp = kb->data;
			for (i = kb->totelem; i--; fp += 3) {
				add_v3_v3(fp, offset);
			}
		}
	}
}

void BKE_mesh_ensure_navmesh(Mesh *me)
{
	if (!CustomData_has_layer(&me->pdata, CD_RECAST)) {
		int i;
		int numFaces = me->totpoly;
		int *recastData;
		recastData = (int *)MEM_mallocN(numFaces * sizeof(int), __func__);
		for (i = 0; i < numFaces; i++) {
			recastData[i] = i + 1;
		}
		CustomData_add_layer_named(&me->pdata, CD_RECAST, CD_ASSIGN, recastData, numFaces, "recastData");
	}
}

void BKE_mesh_tessface_calc(Mesh *mesh)
{
	mesh->totface = BKE_mesh_recalc_tessellation(&mesh->fdata, &mesh->ldata, &mesh->pdata,
	                                             mesh->mvert,
	                                             mesh->totface, mesh->totloop, mesh->totpoly,
	                                             /* calc normals right after, don't copy from polys here */
	                                             false);

	BKE_mesh_update_customdata_pointers(mesh, true);
}

void BKE_mesh_tessface_ensure(Mesh *mesh)
{
	if (mesh->totpoly && mesh->totface == 0) {
		BKE_mesh_tessface_calc(mesh);
	}
}

void BKE_mesh_tessface_clear(Mesh *mesh)
{
	mesh_tessface_clear_intern(mesh, true);
}

void BKE_mesh_do_versions_cd_flag_init(Mesh *mesh)
{
	if (UNLIKELY(mesh->cd_flag)) {
		return;
	}
	else {
		MVert *mv;
		MEdge *med;
		int i;

		for (mv = mesh->mvert, i = 0; i < mesh->totvert; mv++, i++) {
			if (mv->bweight != 0) {
				mesh->cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
				break;
			}
		}

		for (med = mesh->medge, i = 0; i < mesh->totedge; med++, i++) {
			if (med->bweight != 0) {
				mesh->cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
				if (mesh->cd_flag & ME_CDFLAG_EDGE_CREASE) {
					break;
				}
			}
			if (med->crease != 0) {
				mesh->cd_flag |= ME_CDFLAG_EDGE_CREASE;
				if (mesh->cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
					break;
				}
			}
		}

	}
}


/* -------------------------------------------------------------------- */
/* MSelect functions (currently used in weight paint mode) */

void BKE_mesh_mselect_clear(Mesh *me)
{
	if (me->mselect) {
		MEM_freeN(me->mselect);
		me->mselect = NULL;
	}
	me->totselect = 0;
}

void BKE_mesh_mselect_validate(Mesh *me)
{
	MSelect *mselect_src, *mselect_dst;
	int i_src, i_dst;

	if (me->totselect == 0)
		return;

	mselect_src = me->mselect;
	mselect_dst = MEM_mallocN(sizeof(MSelect) * (me->totselect), "Mesh selection history");

	for (i_src = 0, i_dst = 0; i_src < me->totselect; i_src++) {
		int index = mselect_src[i_src].index;
		switch (mselect_src[i_src].type) {
			case ME_VSEL:
			{
				if (me->mvert[index].flag & SELECT) {
					mselect_dst[i_dst] = mselect_src[i_src];
					i_dst++;
				}
				break;
			}
			case ME_ESEL:
			{
				if (me->medge[index].flag & SELECT) {
					mselect_dst[i_dst] = mselect_src[i_src];
					i_dst++;
				}
				break;
			}
			case ME_FSEL:
			{
				if (me->mpoly[index].flag & SELECT) {
					mselect_dst[i_dst] = mselect_src[i_src];
					i_dst++;
				}
				break;
			}
			default:
			{
				BLI_assert(0);
				break;
			}
		}
	}

	MEM_freeN(mselect_src);

	if (i_dst == 0) {
		MEM_freeN(mselect_dst);
		mselect_dst = NULL;
	}
	else if (i_dst != me->totselect) {
		mselect_dst = MEM_reallocN(mselect_dst, sizeof(MSelect) * i_dst);
	}

	me->totselect = i_dst;
	me->mselect = mselect_dst;

}

/**
 * Return the index within me->mselect, or -1
 */
int BKE_mesh_mselect_find(Mesh *me, int index, int type)
{
	int i;

	BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

	for (i = 0; i < me->totselect; i++) {
		if ((me->mselect[i].index == index) &&
		    (me->mselect[i].type == type))
		{
			return i;
		}
	}

	return -1;
}

/**
 * Return The index of the active element.
 */
int BKE_mesh_mselect_active_get(Mesh *me, int type)
{
	BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

	if (me->totselect) {
		if (me->mselect[me->totselect - 1].type == type) {
			return me->mselect[me->totselect - 1].index;
		}
	}
	return -1;
}

void BKE_mesh_mselect_active_set(Mesh *me, int index, int type)
{
	const int msel_index = BKE_mesh_mselect_find(me, index, type);

	if (msel_index == -1) {
		/* add to the end */
		me->mselect = MEM_reallocN(me->mselect, sizeof(MSelect) * (me->totselect + 1));
		me->mselect[me->totselect].index = index;
		me->mselect[me->totselect].type  = type;
		me->totselect++;
	}
	else if (msel_index != me->totselect - 1) {
		/* move to the end */
		SWAP(MSelect, me->mselect[msel_index], me->mselect[me->totselect - 1]);
	}

	BLI_assert((me->mselect[me->totselect - 1].index == index) &&
	           (me->mselect[me->totselect - 1].type  == type));
}

/* settings: 1 - preview, 2 - render */
Mesh *BKE_mesh_new_from_object(
        Main *bmain, Scene *sce, Object *ob,
        int apply_modifiers, int settings, int calc_tessface, int calc_undeformed)
{
	Mesh *tmpmesh;
	Curve *tmpcu = NULL, *copycu;
	Object *tmpobj = NULL;
	int render = settings == eModifierMode_Render, i;
	int cage = !apply_modifiers;

	/* perform the mesh extraction based on type */
	switch (ob->type) {
		case OB_FONT:
		case OB_CURVE:
		case OB_SURF:
		{
			ListBase dispbase = {NULL, NULL};
			DerivedMesh *derivedFinal = NULL;
			int uv_from_orco;

			/* copies object and modifiers (but not the data) */
			tmpobj = BKE_object_copy_ex(bmain, ob, true);
			tmpcu = (Curve *)tmpobj->data;
			tmpcu->id.us--;

			/* if getting the original caged mesh, delete object modifiers */
			if (cage)
				BKE_object_free_modifiers(tmpobj);

			/* copies the data */
			copycu = tmpobj->data = BKE_curve_copy((Curve *) ob->data);

			/* temporarily set edit so we get updates from edit mode, but
			 * also because for text datablocks copying it while in edit
			 * mode gives invalid data structures */
			copycu->editfont = tmpcu->editfont;
			copycu->editnurb = tmpcu->editnurb;

			/* get updated display list, and convert to a mesh */
			BKE_displist_make_curveTypes_forRender(sce, tmpobj, &dispbase, &derivedFinal, false, render);

			copycu->editfont = NULL;
			copycu->editnurb = NULL;

			tmpobj->derivedFinal = derivedFinal;

			/* convert object type to mesh */
			uv_from_orco = (tmpcu->flag & CU_UV_ORCO) != 0;
			BKE_mesh_from_nurbs_displist(tmpobj, &dispbase, uv_from_orco);

			tmpmesh = tmpobj->data;

			BKE_displist_free(&dispbase);

			/* BKE_mesh_from_nurbs changes the type to a mesh, check it worked.
			 * if it didn't the curve did not have any segments or otherwise 
			 * would have generated an empty mesh */
			if (tmpobj->type != OB_MESH) {
				BKE_libblock_free_us(bmain, tmpobj);
				return NULL;
			}

			BKE_mesh_texspace_copy_from_object(tmpmesh, ob);

			BKE_libblock_free_us(bmain, tmpobj);
			break;
		}

		case OB_MBALL:
		{
			/* metaballs don't have modifiers, so just convert to mesh */
			Object *basis_ob = BKE_mball_basis_find(sce, ob);
			/* todo, re-generatre for render-res */
			/* metaball_polygonize(scene, ob) */

			if (ob != basis_ob)
				return NULL;  /* only do basis metaball */

			tmpmesh = BKE_mesh_add(bmain, "Mesh");
			/* BKE_mesh_add gives us a user count we don't need */
			tmpmesh->id.us--;

			if (render) {
				ListBase disp = {NULL, NULL};
				/* TODO(sergey): This is gonna to work for until EvaluationContext
				 *               only contains for_render flag. As soon as CoW is
				 *               implemented, this is to be rethinked.
				 */
				EvaluationContext eval_ctx = {0};
				eval_ctx.mode = DAG_EVAL_RENDER;
				BKE_displist_make_mball_forRender(&eval_ctx, sce, ob, &disp);
				BKE_mesh_from_metaball(&disp, tmpmesh);
				BKE_displist_free(&disp);
			}
			else {
				ListBase disp = {NULL, NULL};
				if (ob->curve_cache) {
					disp = ob->curve_cache->disp;
				}
				BKE_mesh_from_metaball(&disp, tmpmesh);
			}

			BKE_mesh_texspace_copy_from_object(tmpmesh, ob);

			break;

		}
		case OB_MESH:
			/* copies object and modifiers (but not the data) */
			if (cage) {
				/* copies the data */
				tmpmesh = BKE_mesh_copy_ex(bmain, ob->data);
				/* if not getting the original caged mesh, get final derived mesh */
			}
			else {
				/* Make a dummy mesh, saves copying */
				DerivedMesh *dm;
				/* CustomDataMask mask = CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL; */
				CustomDataMask mask = CD_MASK_MESH; /* this seems more suitable, exporter,
			                                         * for example, needs CD_MASK_MDEFORMVERT */

				if (calc_undeformed)
					mask |= CD_MASK_ORCO;

				/* Write the display mesh into the dummy mesh */
				if (render)
					dm = mesh_create_derived_render(sce, ob, mask);
				else
					dm = mesh_create_derived_view(sce, ob, mask);

				tmpmesh = BKE_mesh_add(bmain, "Mesh");
				DM_to_mesh(dm, tmpmesh, ob, mask);
				dm->release(dm);
			}

			/* BKE_mesh_add/copy gives us a user count we don't need */
			tmpmesh->id.us--;

			break;
		default:
			/* "Object does not have geometry data") */
			return NULL;
	}

	/* Copy materials to new mesh */
	switch (ob->type) {
		case OB_SURF:
		case OB_FONT:
		case OB_CURVE:
			tmpmesh->totcol = tmpcu->totcol;

			/* free old material list (if it exists) and adjust user counts */
			if (tmpcu->mat) {
				for (i = tmpcu->totcol; i-- > 0; ) {
					/* are we an object material or data based? */

					tmpmesh->mat[i] = ob->matbits[i] ? ob->mat[i] : tmpcu->mat[i];

					if (tmpmesh->mat[i]) {
						tmpmesh->mat[i]->id.us++;
					}
				}
			}
			break;

#if 0
		/* Crashes when assigning the new material, not sure why */
		case OB_MBALL:
			tmpmb = (MetaBall *)ob->data;
			tmpmesh->totcol = tmpmb->totcol;

			/* free old material list (if it exists) and adjust user counts */
			if (tmpmb->mat) {
				for (i = tmpmb->totcol; i-- > 0; ) {
					tmpmesh->mat[i] = tmpmb->mat[i]; /* CRASH HERE ??? */
					if (tmpmesh->mat[i]) {
						tmpmb->mat[i]->id.us++;
					}
				}
			}
			break;
#endif

		case OB_MESH:
			if (!cage) {
				Mesh *origmesh = ob->data;
				tmpmesh->flag = origmesh->flag;
				tmpmesh->mat = MEM_dupallocN(origmesh->mat);
				tmpmesh->totcol = origmesh->totcol;
				tmpmesh->smoothresh = origmesh->smoothresh;
				if (origmesh->mat) {
					for (i = origmesh->totcol; i-- > 0; ) {
						/* are we an object material or data based? */
						tmpmesh->mat[i] = ob->matbits[i] ? ob->mat[i] : origmesh->mat[i];

						if (tmpmesh->mat[i]) {
							tmpmesh->mat[i]->id.us++;
						}
					}
				}
			}
			break;
	} /* end copy materials */

	if (calc_tessface) {
		/* cycles and exporters rely on this still */
		BKE_mesh_tessface_ensure(tmpmesh);
	}

	/* make sure materials get updated in objects */
	test_object_materials(bmain, &tmpmesh->id);

	return tmpmesh;
}

