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

/** \file blender/bmesh/intern/bmesh_mesh_conv.c
 *  \ingroup bmesh
 *
 * BM mesh conversion functions.
 *
 * \section bm_mesh_conv_shapekey Converting Shape Keys
 *
 * When converting to/from a Mesh/BMesh you can optionally pass a shape key to edit.
 * This has the effect of editing the shape key-block rather then the original mesh vertex coords
 * (although additional geometry is still allowed and uses fallback locations on converting).
 *
 * While this works for any mesh/bmesh this is made use of by entering and exiting edit-mode.
 *
 * There are comments in code but this should help explain the general
 * intention as to how this works converting from/to bmesh.
 *
 *
 * \subsection user_pov User Perspective
 *
 * - Editmode operations when a shape key-block is active edits only that key-block.
 * - The first Basis key-block always matches the Mesh verts.
 * - Changing vertex locations of _any_ Basis will apply offsets to those shape keys using this as their Basis.
 *
 *
 * \subsection enter_editmode Entering EditMode - #BM_mesh_bm_from_me
 *
 * - the active key-block is used for BMesh vertex locations on entering edit-mode.
 * So obviously the meshes vertex locations remain unchanged and the shape key its self is not being edited directly.
 * Simply the #BMVert.co is a initialized from active shape key (when its set).
 * - all key-blocks are added as CustomData layers (read code for details).
 *
 *
 * \subsection exit_editmode Exiting EditMode - #BM_mesh_bm_to_me
 *
 * This is where the most confusing code is! Won't attempt to document the details here, for that read the code.
 * But basics are as follows.
 *
 * - Vertex locations (possibly modified from initial active key-block) are copied directly into #MVert.co
 * (special confusing note that these may be restored later, when editing the 'Basis', read on).
 * - if the 'Key' is relative, and the active key-block is the basis for ANY other key-blocks - get an array of offsets
 * between the new vertex locations and the original shape key (before entering edit-mode),
 * these offsets get applied later on to inactive key-blocks using the active one (which we are editing) as their Basis.
 *
 * Copying the locations back to the shape keys is quite confusing...
 * One main area of confusion is that when editing a 'Basis' key-block 'me->key->refkey'
 * The coords are written into the mesh, from the users perspective the Basis coords are written into the mesh
 * when exiting edit-mode.
 *
 * When _not_ editing the 'Basis', the original vertex locations (stored in the mesh and unchanged during edit-mode),
 * are copied back into the mesh.
 *
 * This has the effect from the users POV of leaving the mesh un-touched, and only editing the active shape key-block.
 *
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_key_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_alloca.h"
#include "BLI_math_vector.h"

#include "BKE_mesh.h"
#include "BKE_customdata.h"
#include "BKE_multires.h"

#include "BKE_global.h" /* ugh - for looping over all objects */
#include "BKE_main.h"
#include "BKE_key.h"

#include "bmesh.h"
#include "intern/bmesh_private.h" /* for element checking */

/**
 * Currently this is only used for Python scripts
 * which may fail to keep matching UV/TexFace layers.
 *
 * \note This should only perform any changes in exceptional cases,
 * if we need this to be faster we could inline #BM_data_layer_add and only
 * call #update_data_blocks once at the end.
 */
void BM_mesh_cd_validate(BMesh *bm)
{
	int totlayer_mtex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	int totlayer_uv = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);

	if (LIKELY(totlayer_mtex == totlayer_uv)) {
		/* pass */
	}
	else if (totlayer_mtex < totlayer_uv) {
		const int uv_index_first = CustomData_get_layer_index(&bm->ldata, CD_MLOOPUV);
		do {
			const char *from_name =  bm->ldata.layers[uv_index_first + totlayer_mtex].name;
			BM_data_layer_add_named(bm, &bm->pdata, CD_MTEXPOLY, from_name);
			CustomData_set_layer_unique_name(&bm->pdata, totlayer_mtex);
		} while (totlayer_uv != ++totlayer_mtex);
	}
	else if (totlayer_uv < totlayer_mtex) {
		const int mtex_index_first = CustomData_get_layer_index(&bm->pdata, CD_MTEXPOLY);
		do {
			const char *from_name = bm->pdata.layers[mtex_index_first + totlayer_uv].name;
			BM_data_layer_add_named(bm, &bm->ldata, CD_MLOOPUV, from_name);
			CustomData_set_layer_unique_name(&bm->ldata, totlayer_uv);
		} while (totlayer_mtex != ++totlayer_uv);
	}

	BLI_assert(totlayer_mtex == totlayer_uv);
}

void BM_mesh_cd_flag_ensure(BMesh *bm, Mesh *mesh, const char cd_flag)
{
	const char cd_flag_all = BM_mesh_cd_flag_from_bmesh(bm) | cd_flag;
	BM_mesh_cd_flag_apply(bm, cd_flag_all);
	if (mesh) {
		mesh->cd_flag = cd_flag_all;
	}
}

void BM_mesh_cd_flag_apply(BMesh *bm, const char cd_flag)
{
	/* CustomData_bmesh_init_pool() must run first */
	BLI_assert(bm->vdata.totlayer == 0 || bm->vdata.pool != NULL);
	BLI_assert(bm->edata.totlayer == 0 || bm->edata.pool != NULL);
	BLI_assert(bm->pdata.totlayer == 0 || bm->pdata.pool != NULL);

	if (cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
		if (!CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
			BM_data_layer_add(bm, &bm->vdata, CD_BWEIGHT);
		}
	}
	else {
		if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
			BM_data_layer_free(bm, &bm->vdata, CD_BWEIGHT);
		}
	}

	if (cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
		if (!CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
			BM_data_layer_add(bm, &bm->edata, CD_BWEIGHT);
		}
	}
	else {
		if (CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
			BM_data_layer_free(bm, &bm->edata, CD_BWEIGHT);
		}
	}

	if (cd_flag & ME_CDFLAG_EDGE_CREASE) {
		if (!CustomData_has_layer(&bm->edata, CD_CREASE)) {
			BM_data_layer_add(bm, &bm->edata, CD_CREASE);
		}
	}
	else {
		if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
			BM_data_layer_free(bm, &bm->edata, CD_CREASE);
		}
	}
}

char BM_mesh_cd_flag_from_bmesh(BMesh *bm)
{
	char cd_flag = 0;
	if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
		cd_flag |= ME_CDFLAG_VERT_BWEIGHT;
	}
	if (CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
		cd_flag |= ME_CDFLAG_EDGE_BWEIGHT;
	}
	if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
		cd_flag |= ME_CDFLAG_EDGE_CREASE;
	}
	return cd_flag;
}

/* Static function for alloc (duplicate in modifiers_bmesh.c) */
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
 * \brief Mesh -> BMesh
 *
 * \warning This function doesn't calculate face normals.
 */
void BM_mesh_bm_from_me(BMesh *bm, Mesh *me,
                        const bool calc_face_normal, const bool set_key, int act_key_nr)
{
	MVert *mvert;
	MEdge *medge;
	MLoop *mloop;
	MPoly *mp;
	KeyBlock *actkey, *block;
	BMVert *v, **vtable = NULL;
	BMEdge *e, **etable = NULL;
	BMFace *f;
	float (*keyco)[3] = NULL;
	int totuv, totloops, i, j;

	int cd_vert_bweight_offset;
	int cd_edge_bweight_offset;
	int cd_edge_crease_offset;
	int cd_shape_keyindex_offset;

	/* free custom data */
	/* this isnt needed in most cases but do just incase */
	CustomData_free(&bm->vdata, bm->totvert);
	CustomData_free(&bm->edata, bm->totedge);
	CustomData_free(&bm->ldata, bm->totloop);
	CustomData_free(&bm->pdata, bm->totface);

	if (!me || !me->totvert) {
		if (me) { /*no verts? still copy customdata layout*/
			CustomData_copy(&me->vdata, &bm->vdata, CD_MASK_BMESH, CD_ASSIGN, 0);
			CustomData_copy(&me->edata, &bm->edata, CD_MASK_BMESH, CD_ASSIGN, 0);
			CustomData_copy(&me->ldata, &bm->ldata, CD_MASK_BMESH, CD_ASSIGN, 0);
			CustomData_copy(&me->pdata, &bm->pdata, CD_MASK_BMESH, CD_ASSIGN, 0);

			CustomData_bmesh_init_pool(&bm->vdata, me->totvert, BM_VERT);
			CustomData_bmesh_init_pool(&bm->edata, me->totedge, BM_EDGE);
			CustomData_bmesh_init_pool(&bm->ldata, me->totloop, BM_LOOP);
			CustomData_bmesh_init_pool(&bm->pdata, me->totpoly, BM_FACE);
		}
		return; /* sanity check */
	}

	vtable = MEM_mallocN(sizeof(void **) * me->totvert, "mesh to bmesh vtable");

	CustomData_copy(&me->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->ldata, &bm->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->pdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	/* make sure uv layer names are consisten */
	totuv = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	for (i = 0; i < totuv; i++) {
		int li = CustomData_get_layer_index_n(&bm->pdata, CD_MTEXPOLY, i);
		CustomData_set_layer_name(&bm->ldata, CD_MLOOPUV, i, bm->pdata.layers[li].name);
	}

	if ((act_key_nr != 0) && (me->key != NULL)) {
		actkey = BLI_findlink(&me->key->block, act_key_nr - 1);
	}
	else {
		actkey = NULL;
	}

	if (me->key) {
		CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);

		/* check if we need to generate unique ids for the shapekeys.
		 * this also exists in the file reading code, but is here for
		 * a sanity check */
		if (!me->key->uidgen) {
			fprintf(stderr,
			        "%s had to generate shape key uid's in a situation we shouldn't need to! "
			        "(bmesh internal error)\n",
			        __func__);

			me->key->uidgen = 1;
			for (block = me->key->block.first; block; block = block->next) {
				block->uid = me->key->uidgen++;
			}
		}

		if (actkey && actkey->totelem == me->totvert) {
			keyco = actkey->data;
			bm->shapenr = act_key_nr;
		}

		for (i = 0, block = me->key->block.first; block; block = block->next, i++) {
			CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY,
			                           CD_ASSIGN, NULL, 0, block->name);

			j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
			bm->vdata.layers[j].uid = block->uid;
		}
	}

	CustomData_bmesh_init_pool(&bm->vdata, me->totvert, BM_VERT);
	CustomData_bmesh_init_pool(&bm->edata, me->totedge, BM_EDGE);
	CustomData_bmesh_init_pool(&bm->ldata, me->totloop, BM_LOOP);
	CustomData_bmesh_init_pool(&bm->pdata, me->totpoly, BM_FACE);

	BM_mesh_cd_flag_apply(bm, me->cd_flag);

	cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
	cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
	cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);
	cd_shape_keyindex_offset = me->key ? CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX) : -1;

	for (i = 0, mvert = me->mvert; i < me->totvert; i++, mvert++) {
		v = vtable[i] = BM_vert_create(bm, keyco && set_key ? keyco[i] : mvert->co, NULL, BM_CREATE_SKIP_CD);
		BM_elem_index_set(v, i); /* set_ok */

		/* transfer flag */
		v->head.hflag = BM_vert_flag_from_mflag(mvert->flag & ~SELECT);

		/* this is necessary for selection counts to work properly */
		if (mvert->flag & SELECT) {
			BM_vert_select_set(bm, v, true);
		}

		normal_short_to_float_v3(v->no, mvert->no);

		/* Copy Custom Data */
		CustomData_to_bmesh_block(&me->vdata, &bm->vdata, i, &v->head.data, true);

		if (cd_vert_bweight_offset != -1) BM_ELEM_CD_SET_FLOAT(v, cd_vert_bweight_offset, (float)mvert->bweight / 255.0f);

		/* set shapekey data */
		if (me->key) {
			/* set shape key original index */
			if (cd_shape_keyindex_offset != -1) BM_ELEM_CD_SET_INT(v, cd_shape_keyindex_offset, i);

			for (block = me->key->block.first, j = 0; block; block = block->next, j++) {
				float *co = CustomData_bmesh_get_n(&bm->vdata, v->head.data, CD_SHAPEKEY, j);

				if (co) {
					copy_v3_v3(co, ((float *)block->data) + 3 * i);
				}
			}
		}
	}

	bm->elem_index_dirty &= ~BM_VERT; /* added in order, clear dirty flag */

	if (!me->totedge) {
		MEM_freeN(vtable);
		return;
	}

	etable = MEM_mallocN(sizeof(void **) * me->totedge, "mesh to bmesh etable");

	medge = me->medge;
	for (i = 0; i < me->totedge; i++, medge++) {
		e = etable[i] = BM_edge_create(bm, vtable[medge->v1], vtable[medge->v2], NULL, BM_CREATE_SKIP_CD);
		BM_elem_index_set(e, i); /* set_ok */

		/* transfer flags */
		e->head.hflag = BM_edge_flag_from_mflag(medge->flag & ~SELECT);

		/* this is necessary for selection counts to work properly */
		if (medge->flag & SELECT) {
			BM_edge_select_set(bm, e, true);
		}

		/* Copy Custom Data */
		CustomData_to_bmesh_block(&me->edata, &bm->edata, i, &e->head.data, true);

		if (cd_edge_bweight_offset != -1) BM_ELEM_CD_SET_FLOAT(e, cd_edge_bweight_offset, (float)medge->bweight / 255.0f);
		if (cd_edge_crease_offset  != -1) BM_ELEM_CD_SET_FLOAT(e, cd_edge_crease_offset,  (float)medge->crease  / 255.0f);

	}

	bm->elem_index_dirty &= ~BM_EDGE; /* added in order, clear dirty flag */

	mloop = me->mloop;
	mp = me->mpoly;
	for (i = 0, totloops = 0; i < me->totpoly; i++, mp++) {
		BMLoop *l_iter;
		BMLoop *l_first;

		f = bm_face_create_from_mpoly(mp, mloop + mp->loopstart,
		                              bm, vtable, etable);

		if (UNLIKELY(f == NULL)) {
			printf("%s: Warning! Bad face in mesh"
			       " \"%s\" at index %d!, skipping\n",
			       __func__, me->id.name + 2, i);
			continue;
		}

		/* don't use 'i' since we may have skipped the face */
		BM_elem_index_set(f, bm->totface - 1); /* set_ok */

		/* transfer flag */
		f->head.hflag = BM_face_flag_from_mflag(mp->flag & ~ME_FACE_SEL);

		/* this is necessary for selection counts to work properly */
		if (mp->flag & ME_FACE_SEL) {
			BM_face_select_set(bm, f, true);
		}

		f->mat_nr = mp->mat_nr;
		if (i == me->act_face) bm->act_face = f;

		j = mp->loopstart;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			/* don't use 'j' since we may have skipped some faces, hence some loops. */
			BM_elem_index_set(l_iter, totloops++); /* set_ok */

			/* Save index of correspsonding MLoop */
			CustomData_to_bmesh_block(&me->ldata, &bm->ldata, j++, &l_iter->head.data, true);
		} while ((l_iter = l_iter->next) != l_first);

		/* Copy Custom Data */
		CustomData_to_bmesh_block(&me->pdata, &bm->pdata, i, &f->head.data, true);

		if (calc_face_normal) {
			BM_face_normal_update(f);
		}
	}

	bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP); /* added in order, clear dirty flag */

	if (me->mselect && me->totselect != 0) {

		BMVert **vert_array = MEM_mallocN(sizeof(BMVert *) * bm->totvert, "VSelConv");
		BMEdge **edge_array = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, "ESelConv");
		BMFace **face_array = MEM_mallocN(sizeof(BMFace *) * bm->totface, "FSelConv");
		MSelect *msel;

#pragma omp parallel sections if (bm->totvert + bm->totedge + bm->totface >= BM_OMP_LIMIT)
		{
#pragma omp section
			{ BM_iter_as_array(bm, BM_VERTS_OF_MESH, NULL, (void **)vert_array, bm->totvert); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_EDGES_OF_MESH, NULL, (void **)edge_array, bm->totedge); }
#pragma omp section
			{ BM_iter_as_array(bm, BM_FACES_OF_MESH, NULL, (void **)face_array, bm->totface); }
		}

		for (i = 0, msel = me->mselect; i < me->totselect; i++, msel++) {
			switch (msel->type) {
				case ME_VSEL:
					BM_select_history_store(bm, (BMElem *)vert_array[msel->index]);
					break;
				case ME_ESEL:
					BM_select_history_store(bm, (BMElem *)edge_array[msel->index]);
					break;
				case ME_FSEL:
					BM_select_history_store(bm, (BMElem *)face_array[msel->index]);
					break;
			}
		}

		MEM_freeN(vert_array);
		MEM_freeN(edge_array);
		MEM_freeN(face_array);
	}
	else {
		me->totselect = 0;
		if (me->mselect) {
			MEM_freeN(me->mselect);
			me->mselect = NULL;
		}
	}

	MEM_freeN(vtable);
	MEM_freeN(etable);
}


/**
 * \brief BMesh -> Mesh
 */
static BMVert **bm_to_mesh_vertex_map(BMesh *bm, int ototvert)
{
	const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);
	BMVert **vertMap = NULL;
	BMVert *eve;
	int i = 0;
	BMIter iter;

	/* caller needs to ensure this */
	BLI_assert(ototvert > 0);

	vertMap = MEM_callocN(sizeof(*vertMap) * ototvert, "vertMap");
	if (cd_shape_keyindex_offset != -1) {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
			if ((keyi != ORIGINDEX_NONE) && (keyi < ototvert)) {
				vertMap[keyi] = eve;
			}
		}
	}
	else {
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			if (i < ototvert) {
				vertMap[i] = eve;
			}
			else {
				break;
			}
		}
	}

	return vertMap;
}

/**
 * returns customdata shapekey index from a keyblock or -1
 * \note could split this out into a more generic function */
static int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey)
{
	int i;
	int j = 0;

	for (i = 0; i < bm->vdata.totlayer; i++) {
		if (bm->vdata.layers[i].type == CD_SHAPEKEY) {
			if (currkey->uid == bm->vdata.layers[i].uid) {
				return j;
			}
			j++;
		}
	}
	return -1;
}

BLI_INLINE void bmesh_quick_edgedraw_flag(MEdge *med, BMEdge *e)
{
	/* this is a cheap way to set the edge draw, its not precise and will
	 * pick the first 2 faces an edge uses.
	 * The dot comparison is a little arbitrary, but set so that a 5 subd
	 * IcoSphere won't vanish but subd 6 will (as with pre-bmesh blender) */


	if ( /* (med->flag & ME_EDGEDRAW) && */ /* assume to be true */
	     (e->l && (e->l != e->l->radial_next)) &&
	     (dot_v3v3(e->l->f->no, e->l->radial_next->f->no) > 0.9995f))
	{
		med->flag &= ~ME_EDGEDRAW;
	}
	else {
		med->flag |= ME_EDGEDRAW;
	}
}

void BM_mesh_bm_to_me(BMesh *bm, Mesh *me, bool do_tessface)
{
	MLoop *mloop;
	MPoly *mpoly;
	MVert *mvert, *oldverts;
	MEdge *med, *medge;
	BMVert *v, *eve;
	BMEdge *e;
	BMFace *f;
	BMIter iter;
	int i, j, ototvert;

	const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
	const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
	const int cd_edge_crease_offset  = CustomData_get_offset(&bm->edata, CD_CREASE);

	ototvert = me->totvert;

	/* new vertex block */
	if (bm->totvert == 0) mvert = NULL;
	else mvert = MEM_callocN(bm->totvert * sizeof(MVert), "loadeditbMesh vert");

	/* new edge block */
	if (bm->totedge == 0) medge = NULL;
	else medge = MEM_callocN(bm->totedge * sizeof(MEdge), "loadeditbMesh edge");

	/* new ngon face block */
	if (bm->totface == 0) mpoly = NULL;
	else mpoly = MEM_callocN(bm->totface * sizeof(MPoly), "loadeditbMesh poly");

	/* new loop block */
	if (bm->totloop == 0) mloop = NULL;
	else mloop = MEM_callocN(bm->totloop * sizeof(MLoop), "loadeditbMesh loop");

	/* lets save the old verts just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldverts = me->mvert;

	/* don't free this yet */
	if (oldverts) {
		CustomData_set_layer(&me->vdata, CD_MVERT, NULL);
	}

	/* free custom data */
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	/* add new custom data */
	me->totvert = bm->totvert;
	me->totedge = bm->totedge;
	me->totloop = bm->totloop;
	me->totpoly = bm->totface;
	/* will be overwritten with a valid value if 'dotess' is set, otherwise we
	 * end up with 'me->totface' and me->mface == NULL which can crash [#28625]
	 */
	me->totface = 0;
	me->act_face = -1;

	CustomData_copy(&bm->vdata, &me->vdata, CD_MASK_MESH, CD_CALLOC, me->totvert);
	CustomData_copy(&bm->edata, &me->edata, CD_MASK_MESH, CD_CALLOC, me->totedge);
	CustomData_copy(&bm->ldata, &me->ldata, CD_MASK_MESH, CD_CALLOC, me->totloop);
	CustomData_copy(&bm->pdata, &me->pdata, CD_MASK_MESH, CD_CALLOC, me->totpoly);

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
	CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);
	CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);

	me->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

	/* this is called again, 'dotess' arg is used there */
	BKE_mesh_update_customdata_pointers(me, 0);

	i = 0;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		copy_v3_v3(mvert->co, v->co);
		normal_float_to_short_v3(mvert->no, v->no);

		mvert->flag = BM_vert_flag_to_mflag(v);

		BM_elem_index_set(v, i); /* set_inline */

		/* copy over customdat */
		CustomData_from_bmesh_block(&bm->vdata, &me->vdata, v->head.data, i);

		if (cd_vert_bweight_offset != -1) mvert->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(v, cd_vert_bweight_offset);

		i++;
		mvert++;

		BM_CHECK_ELEMENT(v);
	}
	bm->elem_index_dirty &= ~BM_VERT;

	med = medge;
	i = 0;
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		med->v1 = BM_elem_index_get(e->v1);
		med->v2 = BM_elem_index_get(e->v2);

		med->flag = BM_edge_flag_to_mflag(e);

		BM_elem_index_set(e, i); /* set_inline */

		/* copy over customdata */
		CustomData_from_bmesh_block(&bm->edata, &me->edata, e->head.data, i);

		bmesh_quick_edgedraw_flag(med, e);

		if (cd_edge_crease_offset  != -1) med->crease  = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_crease_offset);
		if (cd_edge_bweight_offset != -1) med->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_bweight_offset);

		i++;
		med++;
		BM_CHECK_ELEMENT(e);
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	i = 0;
	j = 0;
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter, *l_first;
		mpoly->loopstart = j;
		mpoly->totloop = f->len;
		mpoly->mat_nr = f->mat_nr;
		mpoly->flag = BM_face_flag_to_mflag(f);

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			mloop->e = BM_elem_index_get(l_iter->e);
			mloop->v = BM_elem_index_get(l_iter->v);

			/* copy over customdata */
			CustomData_from_bmesh_block(&bm->ldata, &me->ldata, l_iter->head.data, j);

			j++;
			mloop++;
			BM_CHECK_ELEMENT(l_iter);
			BM_CHECK_ELEMENT(l_iter->e);
			BM_CHECK_ELEMENT(l_iter->v);
		} while ((l_iter = l_iter->next) != l_first);

		if (f == bm->act_face) me->act_face = i;

		/* copy over customdata */
		CustomData_from_bmesh_block(&bm->pdata, &me->pdata, f->head.data, i);

		i++;
		mpoly++;
		BM_CHECK_ELEMENT(f);
	}

	/* patch hook indices and vertex parents */
	if (ototvert > 0) {
		Object *ob;
		ModifierData *md;
		BMVert **vertMap = NULL;
		int i, j;

		for (ob = G.main->object.first; ob; ob = ob->id.next) {
			if ((ob->parent) && (ob->parent->data == me) && ELEM(ob->partype, PARVERT1, PARVERT3)) {

				if (vertMap == NULL) {
					vertMap = bm_to_mesh_vertex_map(bm, ototvert);
				}

				if (ob->par1 < ototvert) {
					eve = vertMap[ob->par1];
					if (eve) ob->par1 = BM_elem_index_get(eve);
				}
				if (ob->par2 < ototvert) {
					eve = vertMap[ob->par2];
					if (eve) ob->par2 = BM_elem_index_get(eve);
				}
				if (ob->par3 < ototvert) {
					eve = vertMap[ob->par3];
					if (eve) ob->par3 = BM_elem_index_get(eve);
				}

			}
			if (ob->data == me) {
				for (md = ob->modifiers.first; md; md = md->next) {
					if (md->type == eModifierType_Hook) {
						HookModifierData *hmd = (HookModifierData *) md;

						if (vertMap == NULL) {
							vertMap = bm_to_mesh_vertex_map(bm, ototvert);
						}

						for (i = j = 0; i < hmd->totindex; i++) {
							if (hmd->indexar[i] < ototvert) {
								eve = vertMap[hmd->indexar[i]];

								if (eve) {
									hmd->indexar[j++] = BM_elem_index_get(eve);
								}
							}
							else {
								j++;
							}
						}

						hmd->totindex = j;
					}
				}
			}
		}

		if (vertMap) MEM_freeN(vertMap);
	}

	if (do_tessface) {
		BKE_mesh_tessface_calc(me);
	}

	BKE_mesh_update_customdata_pointers(me, do_tessface);

	{
		BMEditSelection *selected;
		me->totselect = BLI_countlist(&(bm->selected));

		if (me->mselect) MEM_freeN(me->mselect);

		me->mselect = MEM_callocN(sizeof(MSelect) * me->totselect, "Mesh selection history");


		for (i = 0, selected = bm->selected.first; selected; i++, selected = selected->next) {
			if (selected->htype == BM_VERT) {
				me->mselect[i].type = ME_VSEL;

			}
			else if (selected->htype == BM_EDGE) {
				me->mselect[i].type = ME_ESEL;

			}
			else if (selected->htype == BM_FACE) {
				me->mselect[i].type = ME_FSEL;
			}

			me->mselect[i].index = BM_elem_index_get(selected->ele);
		}
	}

	/* see comment below, this logic is in twice */

	if (me->key) {
		const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);

		KeyBlock *currkey;
		KeyBlock *actkey = BLI_findlink(&me->key->block, bm->shapenr - 1);

		float (*ofs)[3] = NULL;

		/* go through and find any shapekey customdata layers
		 * that might not have corresponding KeyBlocks, and add them if
		 * necessary */
		j = 0;
		for (i = 0; i < bm->vdata.totlayer; i++) {
			if (bm->vdata.layers[i].type != CD_SHAPEKEY)
				continue;

			for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
				if (currkey->uid == bm->vdata.layers[i].uid)
					break;
			}

			if (!currkey) {
				currkey = BKE_keyblock_add(me->key, bm->vdata.layers[i].name);
				currkey->uid = bm->vdata.layers[i].uid;
			}

			j++;
		}


		/* editing the base key should update others */
		if ((me->key->type == KEY_RELATIVE) && /* only need offsets for relative shape keys */
		    (actkey != NULL) &&                /* unlikely, but the active key may not be valid if the
		                                        * bmesh and the mesh are out of sync */
		    (oldverts != NULL))                /* not used here, but 'oldverts' is used later for applying 'ofs' */
		{
			bool act_is_basis = false;

			/* find if this key is a basis for any others */
			for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
				if (bm->shapenr - 1 == currkey->relative) {
					act_is_basis = true;
					break;
				}
			}

			/* active key is a base */
			if (act_is_basis && (cd_shape_keyindex_offset != -1)) {
				float (*fp)[3] = actkey->data;

				ofs = MEM_callocN(sizeof(float) * 3 * bm->totvert,  "currkey->data");
				mvert = me->mvert;
				BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
					const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);

					if (keyi != ORIGINDEX_NONE) {
						sub_v3_v3v3(ofs[i], mvert->co, fp[keyi]);
					}
					else {
						/* if there are new vertices in the mesh, we can't propagate the offset
						 * because it will only work for the existing vertices and not the new
						 * ones, creating a mess when doing e.g. subdivide + translate */
						MEM_freeN(ofs);
						ofs = NULL;
						break;
					}

					mvert++;
				}
			}
		}

		for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
			const bool apply_offset = (ofs && (currkey != actkey) && (bm->shapenr - 1 == currkey->relative));
			int cd_shape_offset;
			int keyi;
			float (*ofs_pt)[3] = ofs;
			float *newkey, (*oldkey)[3], *fp;

			j = bm_to_mesh_shape_layer_index_from_kb(bm, currkey);
			cd_shape_offset = CustomData_get_n_offset(&bm->vdata, CD_SHAPEKEY, j);


			fp = newkey = MEM_callocN(me->key->elemsize * bm->totvert,  "currkey->data");
			oldkey = currkey->data;

			mvert = me->mvert;
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {

				if (currkey == actkey) {
					copy_v3_v3(fp, eve->co);

					if (actkey != me->key->refkey) { /* important see bug [#30771] */
						if (cd_shape_keyindex_offset != -1) {
							if (oldverts) {
								keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
								if (keyi != ORIGINDEX_NONE && keyi < currkey->totelem) { /* valid old vertex */
									copy_v3_v3(mvert->co, oldverts[keyi].co);
								}
							}
						}
					}
				}
				else if (j != -1) {
					/* in most cases this runs */
					copy_v3_v3(fp, BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset));
				}
				else if ((oldkey != NULL) &&
				         (cd_shape_keyindex_offset != -1) &&
				         ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
				         (keyi < currkey->totelem))
				{
					/* old method of reconstructing keys via vertice's original key indices,
					 * currently used if the new method above fails (which is theoretically
					 * possible in certain cases of undo) */
					copy_v3_v3(fp, oldkey[keyi]);
				}
				else {
					/* fail! fill in with dummy value */
					copy_v3_v3(fp, mvert->co);
				}

				/* propagate edited basis offsets to other shapes */
				if (apply_offset) {
					add_v3_v3(fp, *ofs_pt++);
				}

				fp += 3;
				mvert++;
			}

			currkey->totelem = bm->totvert;
			if (currkey->data) {
				MEM_freeN(currkey->data);
			}
			currkey->data = newkey;
		}

		if (ofs) MEM_freeN(ofs);
	}

	if (oldverts) MEM_freeN(oldverts);

	/* topology could be changed, ensure mdisps are ok */
	multires_topology_changed(me);
}
