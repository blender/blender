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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, Mike Erwin, Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file draw_cache_impl_mesh.c
 *  \ingroup draw
 *
 * \brief Mesh API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_math_bits.h"
#include "BLI_string.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"
#include "BKE_texture.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_material.h"

#include "draw_cache_impl.h"  /* own include */

static void mesh_batch_cache_clear(Mesh *me);

/* ---------------------------------------------------------------------- */

/** \name Mesh/BMesh Interface (direct access to basic data).
 * \{ */

static int mesh_render_verts_len_get(Mesh *me)
{
	return me->edit_btmesh ? me->edit_btmesh->bm->totvert : me->totvert;
}

static int mesh_render_edges_len_get(Mesh *me)
{
	return me->edit_btmesh ? me->edit_btmesh->bm->totedge : me->totedge;
}

static int mesh_render_looptri_len_get(Mesh *me)
{
	return me->edit_btmesh ? me->edit_btmesh->tottri : poly_to_tri_count(me->totpoly, me->totloop);
}

static int mesh_render_polys_len_get(Mesh *me)
{
	return me->edit_btmesh ? me->edit_btmesh->bm->totface : me->totpoly;
}

static int mesh_render_mat_len_get(Mesh *me)
{
	return MAX2(1, me->totcol);
}

static int UNUSED_FUNCTION(mesh_render_loops_len_get)(Mesh *me)
{
	return me->edit_btmesh ? me->edit_btmesh->bm->totloop : me->totloop;
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

typedef struct EdgeAdjacentPolys {
	int count;
	int face_index[2];
} EdgeAdjacentPolys;

typedef struct EdgeDrawAttr {
	unsigned char v_flag;
	unsigned char e_flag;
	unsigned char crease;
	unsigned char bweight;
} EdgeDrawAttr;

typedef struct MeshRenderData {
	int types;

	int vert_len;
	int edge_len;
	int tri_len;
	int loop_len;
	int poly_len;
	int mat_len;
	int loose_vert_len;
	int loose_edge_len;

	BMEditMesh *edit_bmesh;
	MVert *mvert;
	MEdge *medge;
	MLoop *mloop;
	MPoly *mpoly;
	float (*orco)[3];
	MDeformVert *dvert;
	MLoopUV *mloopuv;
	MLoopCol *mloopcol;

	/* CustomData 'cd' cache for efficient access. */
	struct {
		struct {
			MLoopUV **uv;
			int       uv_len;
			int       uv_active;

			MLoopCol **vcol;
			int        vcol_len;
			int        vcol_active;

			float (**tangent)[4];
			int      tangent_len;
			int      tangent_active;

			bool *auto_vcol;
		} layers;

		/* Custom-data offsets (only needed for BMesh access) */
		struct {
			int crease;
			int bweight;
			int *uv;
			int *vcol;
		} offset;

		struct {
			char (*auto_mix)[32];
			char (*uv)[32];
			char (*vcol)[32];
			char (*tangent)[32];
		} uuid;

		/* for certain cases we need an output loop-data storage (bmesh tangents) */
		struct {
			CustomData ldata;
			/* grr, special case variable (use in place of 'dm->tangent_mask') */
			char tangent_mask;
		} output;
	} cd;

	BMVert *eve_act;
	BMEdge *eed_act;
	BMFace *efa_act;

	/* Data created on-demand (usually not for bmesh-based data). */
	EdgeAdjacentPolys *edges_adjacent_polys;
	MLoopTri *mlooptri;
	int *loose_edges;
	int *loose_verts;

	float (*poly_normals)[3];
	float (*vert_weight_color)[3];
	char (*vert_color)[3];
	short (*poly_normals_short)[3];
	short (*vert_normals_short)[3];
	bool *edge_select_bool;
} MeshRenderData;

enum {
	MR_DATATYPE_VERT       = 1 << 0,
	MR_DATATYPE_EDGE       = 1 << 1,
	MR_DATATYPE_LOOPTRI    = 1 << 2,
	MR_DATATYPE_LOOP       = 1 << 3,
	MR_DATATYPE_POLY       = 1 << 4,
	MR_DATATYPE_OVERLAY    = 1 << 5,
	MR_DATATYPE_SHADING    = 1 << 6,
	MR_DATATYPE_DVERT      = 1 << 7,
	MR_DATATYPE_LOOPCOL    = 1 << 8,
	MR_DATATYPE_LOOPUV     = 1 << 9,
};

/**
 * These functions look like they would be slow but they will typically return true on the first iteration.
 * Only false when all attached elements are hidden.
 */
static bool bm_vert_has_visible_edge(const BMVert *v)
{
	const BMEdge *e_iter, *e_first;

	e_iter = e_first = v->e;
	do {
		if (!BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN)) {
			return true;
		}
	} while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
	return false;
}

static bool bm_edge_has_visible_face(const BMEdge *e)
{
	const BMLoop *l_iter, *l_first;
	l_iter = l_first = e->l;
	do {
		if (!BM_elem_flag_test(l_iter->f, BM_ELEM_HIDDEN)) {
			return true;
		}
	} while ((l_iter = l_iter->radial_next) != l_first);
	return false;
}


static void mesh_cd_calc_used_gpu_layers(
        CustomData *UNUSED(cd_vdata), uchar cd_vused[CD_NUMTYPES],
        CustomData *cd_ldata, uchar cd_lused[CD_NUMTYPES],
        struct GPUMaterial **gpumat_array, int gpumat_array_len)
{
	GPUVertexAttribs gattribs = {0};

	for (int i = 0; i < gpumat_array_len; i++) {
		GPUMaterial *gpumat = gpumat_array[i];
		if (gpumat) {
			GPU_material_vertex_attributes(gpumat, &gattribs);
			for (int j = 0; j < gattribs.totlayer; j++) {
				const char *name = gattribs.layer[j].name;
				switch (gattribs.layer[j].type) {
					case CD_MTFACE:
					{
						int index = (name[0] != '\0') ?
						        CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
						        CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
						if (index != -1) {
							cd_lused[CD_MLOOPUV] |= (1 << index);
						}
						break;
					}
					case CD_TANGENT:
					{
						int index = (name[0] != '\0') ?
						        CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
						        CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
						if (index != -1) {
							cd_lused[CD_TANGENT] |= (1 << index);

							/* TODO(campbell): investigate why this is needed T51919. */
							cd_lused[CD_MLOOPUV] |= (1 << index);
						}
						break;
					}
					case CD_MCOL:
					{
						int index = (name[0] != '\0') ?
						        CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name) :
						        CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
						if (index != -1) {
							cd_lused[CD_MLOOPCOL] |= (1 << index);
						}
						break;
					}
					case CD_ORCO:
					{
						cd_vused[CD_ORCO] |= 1;
						break;
					}
				}
			}
		}
	}
}

/**
 * TODO(campbell): 'gpumat_array' may include materials linked to the object.
 * While not default, object materials should be supported.
 * Although this only impacts the data thats generated, not the materials that display.
 */
static MeshRenderData *mesh_render_data_create_ex(
        Mesh *me, const int types,
        struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	MeshRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;
	rdata->mat_len = mesh_render_mat_len_get(me);

	CustomData_reset(&rdata->cd.output.ldata);

	if (me->edit_btmesh) {
		BMEditMesh *embm = me->edit_btmesh;
		BMesh *bm = embm->bm;

		rdata->edit_bmesh = embm;

		int bm_ensure_types = 0;
		if (types & (MR_DATATYPE_VERT)) {
			rdata->vert_len = bm->totvert;
			bm_ensure_types |= BM_VERT;
		}
		if (types & (MR_DATATYPE_EDGE)) {
			rdata->edge_len = bm->totedge;
			bm_ensure_types |= BM_EDGE;
		}
		if (types & MR_DATATYPE_LOOPTRI) {
			BKE_editmesh_tessface_calc(embm);
			rdata->tri_len = embm->tottri;
		}
		if (types & MR_DATATYPE_LOOP) {
			rdata->loop_len = bm->totloop;
			bm_ensure_types |= BM_LOOP;
		}
		if (types & MR_DATATYPE_POLY) {
			rdata->poly_len = bm->totface;
			bm_ensure_types |= BM_FACE;
		}
		if (types & MR_DATATYPE_OVERLAY) {
			rdata->efa_act = BM_mesh_active_face_get(bm, false, true);
			rdata->eed_act = BM_mesh_active_edge_get(bm);
			rdata->eve_act = BM_mesh_active_vert_get(bm);
			rdata->cd.offset.crease = CustomData_get_offset(&bm->edata, CD_CREASE);
			rdata->cd.offset.bweight = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
		}
		if (types & (MR_DATATYPE_DVERT)) {
			bm_ensure_types |= BM_VERT;
		}

		BM_mesh_elem_index_ensure(bm, bm_ensure_types);
		BM_mesh_elem_table_ensure(bm, bm_ensure_types & ~BM_LOOP);
		if (types & MR_DATATYPE_OVERLAY) {
			rdata->loose_vert_len = rdata->loose_edge_len = 0;

			int *lverts = rdata->loose_verts = MEM_mallocN(rdata->vert_len * sizeof(int), "Loose Vert");
			int *ledges = rdata->loose_edges = MEM_mallocN(rdata->edge_len * sizeof(int), "Loose Edges");

			{
				BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
				BMVert **vtable = bm->vtable;
				for (int i = 0; i < bm->totvert; i++) {
					const BMVert *eve = vtable[i];
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						/* Loose vert */
						if (eve->e == NULL || !bm_vert_has_visible_edge(eve)) {
							lverts[rdata->loose_vert_len++] = i;
						}
					}
				}
			}

			{
				BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
				BMEdge **etable = bm->etable;
				for (int i = 0; i < bm->totedge; i++) {
					const BMEdge *eed = etable[i];
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						/* Loose edge */
						if (eed->l == NULL || !bm_edge_has_visible_face(eed)) {
							ledges[rdata->loose_edge_len++] = i;
						}
					}
				}
			}

			rdata->loose_verts = MEM_reallocN(rdata->loose_verts, rdata->loose_vert_len * sizeof(int));
			rdata->loose_edges = MEM_reallocN(rdata->loose_edges, rdata->loose_edge_len * sizeof(int));
		}
	}
	else {
		if (types & (MR_DATATYPE_VERT)) {
			rdata->vert_len = me->totvert;
			rdata->mvert = CustomData_get_layer(&me->vdata, CD_MVERT);
		}
		if (types & (MR_DATATYPE_EDGE)) {
			rdata->edge_len = me->totedge;
			rdata->medge = CustomData_get_layer(&me->edata, CD_MEDGE);
		}
		if (types & MR_DATATYPE_LOOPTRI) {
			const int tri_len = rdata->tri_len = poly_to_tri_count(me->totpoly, me->totloop);
			rdata->mlooptri = MEM_mallocN(sizeof(*rdata->mlooptri) * tri_len, __func__);
			BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, rdata->mlooptri);
		}
		if (types & MR_DATATYPE_LOOP) {
			rdata->loop_len = me->totloop;
			rdata->mloop = CustomData_get_layer(&me->ldata, CD_MLOOP);
		}
		if (types & MR_DATATYPE_POLY) {
			rdata->poly_len = me->totpoly;
			rdata->mpoly = CustomData_get_layer(&me->pdata, CD_MPOLY);
		}
		if (types & MR_DATATYPE_DVERT) {
			rdata->vert_len = me->totvert;
			rdata->dvert = CustomData_get_layer(&me->vdata, CD_MDEFORMVERT);
		}
		if (types & MR_DATATYPE_LOOPCOL) {
			rdata->loop_len = me->totloop;
			rdata->mloopcol = CustomData_get_layer(&me->ldata, CD_MLOOPCOL);
		}
		if (types & MR_DATATYPE_LOOPUV) {
			rdata->loop_len = me->totloop;
			rdata->mloopuv = CustomData_get_layer(&me->ldata, CD_MLOOPUV);
		}
	}

	if (types & MR_DATATYPE_SHADING) {
		CustomData *cd_vdata, *cd_ldata;

		if (me->edit_btmesh) {
			BMesh *bm = me->edit_btmesh->bm;
			cd_vdata = &bm->vdata;
			cd_ldata = &bm->ldata;
		}
		else {
			cd_vdata = &me->vdata;
			cd_ldata = &me->ldata;
		}

		/* Add edge/poly if we need them */
		uchar cd_vused[CD_NUMTYPES] = {0};
		uchar cd_lused[CD_NUMTYPES] = {0};

		mesh_cd_calc_used_gpu_layers(
		        cd_vdata, cd_vused,
		        cd_ldata, cd_lused,
		        gpumat_array, gpumat_array_len);


		rdata->cd.layers.uv_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
		rdata->cd.layers.vcol_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
		rdata->cd.layers.tangent_active = rdata->cd.layers.uv_active;

		if ((cd_lused[CD_MLOOPUV] & (1 << rdata->cd.layers.uv_active)) == 0) {
			rdata->cd.layers.uv_active = -1;
		}
		if ((cd_lused[CD_TANGENT] & (1 << rdata->cd.layers.tangent_active)) == 0) {
			rdata->cd.layers.tangent_active = -1;
		}
		if ((cd_lused[CD_MLOOPCOL] & (1 << rdata->cd.layers.vcol_active)) == 0) {
			rdata->cd.layers.vcol_active = -1;
		}

		if (cd_vused[CD_ORCO] & 1) {
			rdata->orco = CustomData_get_layer(cd_vdata, CD_ORCO);
			/* If orco is not available compute it ourselves */
			if (!rdata->orco) {
				if (me->edit_btmesh) {
					BMesh *bm = me->edit_btmesh->bm;
					rdata->orco = MEM_mallocN(sizeof(*rdata->orco) * rdata->vert_len, "orco mesh");
					BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
					BMVert **vtable = bm->vtable;
					for (int i = 0; i < bm->totvert; i++) {
						copy_v3_v3(rdata->orco[i], vtable[i]->co);
					}
					BKE_mesh_orco_verts_transform(me, rdata->orco, rdata->vert_len, 0);
				}
				else {
					rdata->orco = MEM_mallocN(sizeof(*rdata->orco) * rdata->vert_len, "orco mesh");
					MVert *mvert = rdata->mvert;
					for (int a = 0; a < rdata->vert_len; a++, mvert++) {
						copy_v3_v3(rdata->orco[a], mvert->co);
					}
					BKE_mesh_orco_verts_transform(me, rdata->orco, rdata->vert_len, 0);
				}
			}
		}
		else {
			rdata->orco = NULL;
		}

		const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;

		/* don't access mesh directly, instead use vars taken from BMesh or Mesh */
#define me DONT_USE_THIS
#ifdef  me /* quiet warning */
#endif
		struct {
			uint uv_len;
			uint vcol_len;
		} cd_layers_src = {
			.uv_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPUV),
			.vcol_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPCOL),
		};

		rdata->cd.layers.uv_len = count_bits_i(cd_lused[CD_MLOOPUV]);
		rdata->cd.layers.tangent_len = count_bits_i(cd_lused[CD_TANGENT]);
		rdata->cd.layers.vcol_len = count_bits_i(cd_lused[CD_MLOOPCOL]);

		rdata->cd.layers.uv = MEM_mallocN(sizeof(*rdata->cd.layers.uv) * rdata->cd.layers.uv_len, __func__);
		rdata->cd.layers.vcol = MEM_mallocN(sizeof(*rdata->cd.layers.vcol) * rdata->cd.layers.vcol_len, __func__);
		rdata->cd.layers.tangent = MEM_mallocN(sizeof(*rdata->cd.layers.tangent) * rdata->cd.layers.tangent_len, __func__);

		rdata->cd.uuid.uv = MEM_mallocN(sizeof(*rdata->cd.uuid.uv) * rdata->cd.layers.uv_len, __func__);
		rdata->cd.uuid.vcol = MEM_mallocN(sizeof(*rdata->cd.uuid.vcol) * rdata->cd.layers.vcol_len, __func__);
		rdata->cd.uuid.tangent = MEM_mallocN(sizeof(*rdata->cd.uuid.tangent) * rdata->cd.layers.tangent_len, __func__);

		rdata->cd.offset.uv = MEM_mallocN(sizeof(*rdata->cd.offset.uv) * rdata->cd.layers.uv_len, __func__);
		rdata->cd.offset.vcol = MEM_mallocN(sizeof(*rdata->cd.offset.vcol) * rdata->cd.layers.vcol_len, __func__);

		/* Allocate max */
		rdata->cd.layers.auto_vcol = MEM_callocN(
		        sizeof(*rdata->cd.layers.auto_vcol) * rdata->cd.layers.vcol_len, __func__);
		rdata->cd.uuid.auto_mix = MEM_mallocN(
		        sizeof(*rdata->cd.uuid.auto_mix) * (rdata->cd.layers.vcol_len + rdata->cd.layers.uv_len), __func__);

		/* XXX FIXME XXX */
		/* We use a hash to identify each data layer based on its name.
		 * Gawain then search for this name in the current shader and bind if it exists.
		 * NOTE : This is prone to hash collision.
		 * One solution to hash collision would be to format the cd layer name
		 * to a safe glsl var name, but without name clash.
		 * NOTE 2 : Replicate changes to code_generate_vertex_new() in gpu_codegen.c */
		if (rdata->cd.layers.vcol_len != 0) {
			for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.vcol_len; i_src++, i_dst++) {
				if ((cd_lused[CD_MLOOPCOL] & (1 << i_src)) == 0) {
					i_dst--;
					if (rdata->cd.layers.vcol_active >= i_src) {
						rdata->cd.layers.vcol_active--;
					}
				}
				else {
					const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i_src);
					unsigned int hash = BLI_ghashutil_strhash_p(name);
					BLI_snprintf(rdata->cd.uuid.vcol[i_dst], sizeof(*rdata->cd.uuid.vcol), "c%u", hash);
					rdata->cd.layers.vcol[i_dst] = CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i_src);
					if (rdata->edit_bmesh) {
						rdata->cd.offset.vcol[i_dst] = CustomData_get_n_offset(
						        &rdata->edit_bmesh->bm->ldata, CD_MLOOPCOL, i_src);
					}

					/* Gather number of auto layers. */
					/* We only do vcols that are not overridden by uvs */
					if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, name) == -1) {
						BLI_snprintf(
						        rdata->cd.uuid.auto_mix[rdata->cd.layers.uv_len + i_dst],
						        sizeof(*rdata->cd.uuid.auto_mix), "a%u", hash);
						rdata->cd.layers.auto_vcol[i_dst] = true;
					}
				}
			}
		}

		/* Start Fresh */
		CustomData_free_layers(cd_ldata, CD_TANGENT, rdata->loop_len);
		CustomData_free_layers(cd_ldata, CD_MLOOPTANGENT, rdata->loop_len);

		if (rdata->cd.layers.uv_len != 0) {
			for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
				if ((cd_lused[CD_MLOOPUV] & (1 << i_src)) == 0) {
					i_dst--;
					if (rdata->cd.layers.uv_active >= i_src) {
						rdata->cd.layers.uv_active--;
					}
				}
				else {
					const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src);
					unsigned int hash = BLI_ghashutil_strhash_p(name);

					BLI_snprintf(rdata->cd.uuid.uv[i_dst], sizeof(*rdata->cd.uuid.uv), "u%u", hash);
					rdata->cd.layers.uv[i_dst] = CustomData_get_layer_n(cd_ldata, CD_MLOOPUV, i_src);
					if (rdata->edit_bmesh) {
						rdata->cd.offset.uv[i_dst] = CustomData_get_n_offset(
						        &rdata->edit_bmesh->bm->ldata, CD_MLOOPUV, i_src);
					}
					BLI_snprintf(rdata->cd.uuid.auto_mix[i_dst], sizeof(*rdata->cd.uuid.auto_mix), "a%u", hash);
				}
			}
		}

		if (rdata->cd.layers.tangent_len != 0) {

			/* -------------------------------------------------------------------- */
			/* Pre-calculate tangents into 'rdata->cd.output.ldata' */

			BLI_assert(!CustomData_has_layer(&rdata->cd.output.ldata, CD_TANGENT));

			/* Tangent Names */
			char tangent_names[MAX_MTFACE][MAX_NAME];
			for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
				if ((cd_lused[CD_TANGENT] & (1 << i_src)) == 0) {
					i_dst--;
				}
				else {
					BLI_strncpy(
					        tangent_names[i_dst],
					        CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src), MAX_NAME);
				}
			}

			if (rdata->edit_bmesh) {
				BMEditMesh *em = rdata->edit_bmesh;
				BMesh *bm = em->bm;

				if (is_auto_smooth) {
					/* TODO: split normals, see below */
				}

				bool calc_active_tangent = false;
				float (*poly_normals)[3] = rdata->poly_normals;
				float (*loop_normals)[3] = CustomData_get_layer(cd_ldata, CD_NORMAL);

				BKE_editmesh_loop_tangent_calc(
				        em, calc_active_tangent,
				        tangent_names, rdata->cd.layers.tangent_len,
				        poly_normals, loop_normals,
				        rdata->orco,
				        &rdata->cd.output.ldata, bm->totloop,
				        &rdata->cd.output.tangent_mask);
			}
			else {
#undef me

				if (is_auto_smooth) {
					if (!CustomData_has_layer(cd_ldata, CD_NORMAL)) {
						BKE_mesh_calc_normals_split(me);
					}
				}

				bool calc_active_tangent = false;
				const float (*poly_normals)[3] = rdata->poly_normals;
				const float (*loop_normals)[3] = CustomData_get_layer(cd_ldata, CD_NORMAL);

				BKE_mesh_calc_loop_tangent_ex(
				        me->mvert,
				        me->mpoly, me->totpoly,
				        me->mloop,
				        rdata->mlooptri, rdata->tri_len,
				        cd_ldata,
				        calc_active_tangent,
				        tangent_names, rdata->cd.layers.tangent_len,
				        poly_normals, loop_normals,
				        rdata->orco,
				        &rdata->cd.output.ldata, me->totloop,
				        &rdata->cd.output.tangent_mask);

					/* If we store tangents in the mesh, set temporary. */
#if 0
				CustomData_set_layer_flag(cd_ldata, CD_TANGENT, CD_FLAG_TEMPORARY);
#endif

#define me DONT_USE_THIS
#ifdef  me /* quiet warning */
#endif
			}

			/* End tangent calculation */
			/* -------------------------------------------------------------------- */

			BLI_assert(CustomData_number_of_layers(&rdata->cd.output.ldata, CD_TANGENT) == rdata->cd.layers.tangent_len);

			for (int i_src = 0, i_dst = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
				if ((cd_lused[CD_TANGENT] & (1 << i_src)) == 0) {
					i_dst--;
					if (rdata->cd.layers.tangent_active >= i_src) {
						rdata->cd.layers.tangent_active--;
					}
				}
				else {
					const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src);
					unsigned int hash = BLI_ghashutil_strhash_p(name);

					BLI_snprintf(rdata->cd.uuid.tangent[i_dst], sizeof(*rdata->cd.uuid.tangent), "t%u", hash);

					/* Done adding tangents. */

					/* note: BKE_editmesh_loop_tangent_calc calculates 'CD_TANGENT',
					 * not 'CD_MLOOPTANGENT' (as done below). It's OK, they're compatible. */

					/* note: normally we'd use 'i_src' here, but 'i_dst' is in sync with 'rdata->cd.output' */
					rdata->cd.layers.tangent[i_dst] = CustomData_get_layer_n(&rdata->cd.output.ldata, CD_TANGENT, i_dst);
					if (rdata->tri_len != 0) {
						BLI_assert(rdata->cd.layers.tangent[i_dst] != NULL);
					}
				}
			}
		}

#undef me
	}

	return rdata;
}

static void mesh_render_data_free(MeshRenderData *rdata)
{
	MEM_SAFE_FREE(rdata->orco);
	MEM_SAFE_FREE(rdata->cd.offset.uv);
	MEM_SAFE_FREE(rdata->cd.offset.vcol);
	MEM_SAFE_FREE(rdata->cd.uuid.auto_mix);
	MEM_SAFE_FREE(rdata->cd.uuid.uv);
	MEM_SAFE_FREE(rdata->cd.uuid.vcol);
	MEM_SAFE_FREE(rdata->cd.uuid.tangent);
	MEM_SAFE_FREE(rdata->cd.layers.uv);
	MEM_SAFE_FREE(rdata->cd.layers.vcol);
	MEM_SAFE_FREE(rdata->cd.layers.tangent);
	MEM_SAFE_FREE(rdata->cd.layers.auto_vcol);
	MEM_SAFE_FREE(rdata->loose_verts);
	MEM_SAFE_FREE(rdata->loose_edges);
	MEM_SAFE_FREE(rdata->edges_adjacent_polys);
	MEM_SAFE_FREE(rdata->mlooptri);
	MEM_SAFE_FREE(rdata->poly_normals);
	MEM_SAFE_FREE(rdata->poly_normals_short);
	MEM_SAFE_FREE(rdata->vert_normals_short);
	MEM_SAFE_FREE(rdata->vert_weight_color);
	MEM_SAFE_FREE(rdata->edge_select_bool);
	MEM_SAFE_FREE(rdata->vert_color);

	CustomData_free(&rdata->cd.output.ldata, rdata->loop_len);

	MEM_freeN(rdata);
}

static MeshRenderData *mesh_render_data_create(Mesh *me, const int types)
{
	return mesh_render_data_create_ex(me, types, NULL, 0);
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Accessor Functions
 * \{ */

static const char *mesh_render_data_uv_auto_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd.uuid.auto_mix[layer];
}

static const char *mesh_render_data_vcol_auto_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd.uuid.auto_mix[rdata->cd.layers.uv_len + layer];
}

static const char *mesh_render_data_uv_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd.uuid.uv[layer];
}

static const char *mesh_render_data_vcol_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd.uuid.vcol[layer];
}

static const char *mesh_render_data_tangent_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd.uuid.tangent[layer];
}

static int mesh_render_data_verts_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);
	return rdata->vert_len;
}

static int mesh_render_data_loose_verts_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return rdata->loose_vert_len;
}

static int mesh_render_data_edges_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_EDGE);
	return rdata->edge_len;
}

static int mesh_render_data_loose_edges_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return rdata->loose_edge_len;
}

static int mesh_render_data_looptri_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
	return rdata->tri_len;
}

static int mesh_render_data_mat_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_POLY);
	return rdata->mat_len;
}

static int UNUSED_FUNCTION(mesh_render_data_loops_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_LOOP);
	return rdata->loop_len;
}

static int mesh_render_data_polys_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_POLY);
	return rdata->poly_len;
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Internal Cache (Lazy Initialization)
 * \{ */

/** Ensure #MeshRenderData.poly_normals_short */
static void mesh_render_data_ensure_poly_normals_short(MeshRenderData *rdata)
{
	short (*pnors_short)[3] = rdata->poly_normals_short;
	if (pnors_short == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter fiter;
			BMFace *efa;
			int i;

			pnors_short = rdata->poly_normals_short = MEM_mallocN(sizeof(*pnors_short) * rdata->poly_len, __func__);
			BM_ITER_MESH_INDEX(efa, &fiter, bm, BM_FACES_OF_MESH, i) {
				normal_float_to_short_v3(pnors_short[i], efa->no);
			}
		}
		else {
			float (*pnors)[3] = rdata->poly_normals;

			if (!pnors) {
				pnors = rdata->poly_normals = MEM_mallocN(sizeof(*pnors) * rdata->poly_len, __func__);
				BKE_mesh_calc_normals_poly(
				        rdata->mvert, NULL, rdata->vert_len,
				        rdata->mloop, rdata->mpoly, rdata->loop_len, rdata->poly_len, pnors, true);
			}

			pnors_short = rdata->poly_normals_short = MEM_mallocN(sizeof(*pnors_short) * rdata->poly_len, __func__);
			for (int i = 0; i < rdata->poly_len; i++) {
				normal_float_to_short_v3(pnors_short[i], pnors[i]);
			}
		}
	}
}

/** Ensure #MeshRenderData.vert_normals_short */
static void mesh_render_data_ensure_vert_normals_short(MeshRenderData *rdata)
{
	short (*vnors_short)[3] = rdata->vert_normals_short;
	if (vnors_short == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter viter;
			BMVert *eve;
			int i;

			vnors_short = rdata->vert_normals_short = MEM_mallocN(sizeof(*vnors_short) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(eve, &viter, bm, BM_VERT, i) {
				normal_float_to_short_v3(vnors_short[i], eve->no);
			}
		}
		else {
			/* data from mesh used directly */
			BLI_assert(0);
		}
	}
}


/** Ensure #MeshRenderData.vert_color */
static void mesh_render_data_ensure_vert_color(MeshRenderData *rdata)
{
	char (*vcol)[3] = rdata->vert_color;
	if (vcol == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			const int cd_loop_color_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPCOL);
			if (cd_loop_color_offset == -1) {
				goto fallback;
			}

			vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

			BMIter fiter;
			BMFace *efa;
			int i = 0;

			BM_ITER_MESH(efa, &fiter, bm, BM_FACES_OF_MESH) {
				BMLoop *l_iter, *l_first;
				l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
				do {
					const MLoopCol *lcol = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_color_offset);
					vcol[i][0] = lcol->r;
					vcol[i][1] = lcol->g;
					vcol[i][2] = lcol->b;
					i += 1;
				} while ((l_iter = l_iter->next) != l_first);
			}
			BLI_assert(i == rdata->loop_len);
		}
		else {
			if (rdata->mloopcol == NULL) {
				goto fallback;
			}

			vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

			for (int i = 0; i < rdata->loop_len; i++) {
				vcol[i][0] = rdata->mloopcol[i].r;
				vcol[i][1] = rdata->mloopcol[i].g;
				vcol[i][2] = rdata->mloopcol[i].b;
			}
		}
	}
	return;

fallback:
	vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

	for (int i = 0; i < rdata->loop_len; i++) {
		vcol[i][0] = 255;
		vcol[i][1] = 255;
		vcol[i][2] = 255;
	}
}

/* TODO, move into shader? */
static void rgb_from_weight(float r_rgb[3], const float weight)
{
	const float blend = ((weight / 2.0f) + 0.5f);

	if (weight <= 0.25f) {    /* blue->cyan */
		r_rgb[0] = 0.0f;
		r_rgb[1] = blend * weight * 4.0f;
		r_rgb[2] = blend;
	}
	else if (weight <= 0.50f) {  /* cyan->green */
		r_rgb[0] = 0.0f;
		r_rgb[1] = blend;
		r_rgb[2] = blend * (1.0f - ((weight - 0.25f) * 4.0f));
	}
	else if (weight <= 0.75f) {  /* green->yellow */
		r_rgb[0] = blend * ((weight - 0.50f) * 4.0f);
		r_rgb[1] = blend;
		r_rgb[2] = 0.0f;
	}
	else if (weight <= 1.0f) {  /* yellow->red */
		r_rgb[0] = blend;
		r_rgb[1] = blend * (1.0f - ((weight - 0.75f) * 4.0f));
		r_rgb[2] = 0.0f;
	}
	else {
		/* exceptional value, unclamped or nan,
		 * avoid uninitialized memory use */
		r_rgb[0] = 1.0f;
		r_rgb[1] = 0.0f;
		r_rgb[2] = 1.0f;
	}
}


/** Ensure #MeshRenderData.vert_weight_color */
static void mesh_render_data_ensure_vert_weight_color(MeshRenderData *rdata, const int defgroup)
{
	float (*vweight)[3] = rdata->vert_weight_color;
	if (vweight == NULL) {
		if (defgroup == -1) {
			goto fallback;
		}

		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
			if (cd_dvert_offset == -1) {
				goto fallback;
			}

			BMIter viter;
			BMVert *eve;
			int i;

			vweight = rdata->vert_weight_color = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(eve, &viter, bm, BM_VERT, i) {
				const MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				float weight = defvert_find_weight(dvert, defgroup);
				if (U.flag & USER_CUSTOM_RANGE) {
					do_colorband(&U.coba_weight, weight, vweight[i]);
				}
				else {
					rgb_from_weight(vweight[i], weight);
				}
			}
		}
		else {
			if (rdata->dvert == NULL) {
				goto fallback;
			}

			vweight = rdata->vert_weight_color = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
			for (int i = 0; i < rdata->vert_len; i++) {
				float weight = defvert_find_weight(&rdata->dvert[i], defgroup);
				if (U.flag & USER_CUSTOM_RANGE) {
					do_colorband(&U.coba_weight, weight, vweight[i]);
				}
				else {
					rgb_from_weight(vweight[i], weight);
				}
			}
		}
	}
	return;

fallback:
	vweight = rdata->vert_weight_color = MEM_callocN(sizeof(*vweight) * rdata->vert_len, __func__);

	for (int i = 0; i < rdata->vert_len; i++) {
		vweight[i][2] = 0.5f;
	}
}

/** Ensure #MeshRenderData.edge_select_bool */
static void mesh_render_data_ensure_edge_select_bool(MeshRenderData *rdata, bool use_wire)
{
	bool *edge_select_bool = rdata->edge_select_bool;
	if (edge_select_bool == NULL) {
		edge_select_bool = rdata->edge_select_bool =
		        MEM_callocN(sizeof(*edge_select_bool) * rdata->edge_len, __func__);

		for (int i = 0; i < rdata->poly_len; i++) {
			MPoly *poly = &rdata->mpoly[i];

			if (poly->flag & ME_FACE_SEL) {
				for (int j = 0; j < poly->totloop; j++) {
					MLoop *loop = &rdata->mloop[poly->loopstart + j];
					if (use_wire) {
						edge_select_bool[loop->e] = true;
					}
					else {
						/* Not totally correct, will cause problems for edges with 3x faces. */
						edge_select_bool[loop->e] = !edge_select_bool[loop->e];
					}
				}
			}
		}
	}
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Internal Cache Generation
 * \{ */

static bool mesh_render_data_pnors_pcenter_select_get(
        MeshRenderData *rdata, const int poly,
        float r_pnors[3], float r_center[3], bool *r_selected)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (rdata->edit_bmesh) {
		const BMFace *efa = BM_face_at_index(rdata->edit_bmesh->bm, poly);
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
			return false;
		}
		BM_face_calc_center_mean(efa, r_center);
		BM_face_calc_normal(efa, r_pnors);
		*r_selected = (BM_elem_flag_test(efa, BM_ELEM_SELECT) != 0) ? true : false;
	}
	else {
		MVert *mvert = rdata->mvert;
		const MPoly *mpoly = rdata->mpoly + poly;
		const MLoop *mloop = rdata->mloop + mpoly->loopstart;

		BKE_mesh_calc_poly_center(mpoly, mloop, mvert, r_center);
		BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, r_pnors);

		*r_selected = false; /* No selection if not in edit mode */
	}

	return true;
}

static bool mesh_render_data_edge_vcos_manifold_pnors(
        MeshRenderData *rdata, const int edge_index,
        float **r_vco1, float **r_vco2, float **r_pnor1, float **r_pnor2, bool *r_is_manifold)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMEdge *eed = BM_edge_at_index(bm, edge_index);
		if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
			return false;
		}
		*r_vco1 = eed->v1->co;
		*r_vco2 = eed->v2->co;
		if (BM_edge_is_manifold(eed)) {
			*r_pnor1 = eed->l->f->no;
			*r_pnor2 = eed->l->radial_next->f->no;
			*r_is_manifold = true;
		}
		else {
			*r_is_manifold = false;
		}
	}
	else {
		MVert *mvert = rdata->mvert;
		MEdge *medge = rdata->medge;
		EdgeAdjacentPolys *eap = rdata->edges_adjacent_polys;
		float (*pnors)[3] = rdata->poly_normals;

		if (!eap) {
			const MLoop *mloop = rdata->mloop;
			const MPoly *mpoly = rdata->mpoly;
			const int poly_len = rdata->poly_len;
			const bool do_pnors = (pnors == NULL);

			eap = rdata->edges_adjacent_polys = MEM_callocN(sizeof(*eap) * rdata->edge_len, __func__);
			if (do_pnors) {
				pnors = rdata->poly_normals = MEM_mallocN(sizeof(*pnors) * poly_len, __func__);
			}

			for (int i = 0; i < poly_len; i++, mpoly++) {
				if (do_pnors) {
					BKE_mesh_calc_poly_normal(mpoly, mloop + mpoly->loopstart, mvert, pnors[i]);
				}

				const int loopend = mpoly->loopstart + mpoly->totloop;
				for (int j = mpoly->loopstart; j < loopend; j++) {
					const int edge_idx = mloop[j].e;
					if (eap[edge_idx].count < 2) {
						eap[edge_idx].face_index[eap[edge_idx].count] = i;
					}
					eap[edge_idx].count++;
				}
			}
		}
		BLI_assert(eap && pnors);

		*r_vco1 = mvert[medge[edge_index].v1].co;
		*r_vco2 = mvert[medge[edge_index].v2].co;
		if (eap[edge_index].count == 2) {
			*r_pnor1 = pnors[eap[edge_index].face_index[0]];
			*r_pnor2 = pnors[eap[edge_index].face_index[1]];
			*r_is_manifold = true;
		}
		else {
			*r_is_manifold = false;
		}
	}

	return true;
}

static void mesh_render_data_looptri_uvs_get(
        MeshRenderData *rdata, const int tri_idx, const int uv_layer,
        float *(*r_vert_uvs)[3])
{
	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		(*r_vert_uvs)[0] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[0], rdata->cd.offset.uv[uv_layer]))->uv;
		(*r_vert_uvs)[1] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[1], rdata->cd.offset.uv[uv_layer]))->uv;
		(*r_vert_uvs)[2] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[2], rdata->cd.offset.uv[uv_layer]))->uv;
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_uvs)[0] = rdata->cd.layers.uv[uv_layer][mlt->tri[0]].uv;
		(*r_vert_uvs)[1] = rdata->cd.layers.uv[uv_layer][mlt->tri[1]].uv;
		(*r_vert_uvs)[2] = rdata->cd.layers.uv[uv_layer][mlt->tri[2]].uv;
	}
}

static void mesh_render_data_looptri_cols_get(
        MeshRenderData *rdata, const int tri_idx, const int vcol_layer,
        unsigned char *(*r_vert_cols)[3])
{
	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		(*r_vert_cols)[0] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[0], rdata->cd.offset.vcol[vcol_layer]))->r;
		(*r_vert_cols)[1] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[1], rdata->cd.offset.vcol[vcol_layer]))->r;
		(*r_vert_cols)[2] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[2], rdata->cd.offset.vcol[vcol_layer]))->r;
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_cols)[0] = &rdata->cd.layers.vcol[vcol_layer][mlt->tri[0]].r;
		(*r_vert_cols)[1] = &rdata->cd.layers.vcol[vcol_layer][mlt->tri[1]].r;
		(*r_vert_cols)[2] = &rdata->cd.layers.vcol[vcol_layer][mlt->tri[2]].r;
	}
}

static void mesh_render_data_looptri_tans_get(
        MeshRenderData *rdata, const int tri_idx, const int tangent_layer,
        float *(*r_vert_tans)[3])
{
	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		(*r_vert_tans)[0] = rdata->cd.layers.tangent[tangent_layer][BM_elem_index_get(bm_looptri[0])];
		(*r_vert_tans)[1] = rdata->cd.layers.tangent[tangent_layer][BM_elem_index_get(bm_looptri[1])];
		(*r_vert_tans)[2] = rdata->cd.layers.tangent[tangent_layer][BM_elem_index_get(bm_looptri[2])];
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_tans)[0] = rdata->cd.layers.tangent[tangent_layer][mlt->tri[0]];
		(*r_vert_tans)[1] = rdata->cd.layers.tangent[tangent_layer][mlt->tri[1]];
		(*r_vert_tans)[2] = rdata->cd.layers.tangent[tangent_layer][mlt->tri[2]];
	}
}

static bool mesh_render_data_looptri_cos_nors_smooth_get(
        MeshRenderData *rdata, const int tri_idx, const bool use_hide,
        float *(*r_vert_cos)[3], short **r_tri_nor, short *(*r_vert_nors)[3], bool *r_is_smooth)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);
	BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
	BLI_assert(rdata->types & MR_DATATYPE_LOOP);
	BLI_assert(rdata->types & MR_DATATYPE_POLY);

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];

		/* Assume 'use_hide' */
		if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}

		mesh_render_data_ensure_poly_normals_short(rdata);
		mesh_render_data_ensure_vert_normals_short(rdata);

		short (*pnors_short)[3] = rdata->poly_normals_short;
		short (*vnors_short)[3] = rdata->vert_normals_short;

		(*r_vert_cos)[0] = bm_looptri[0]->v->co;
		(*r_vert_cos)[1] = bm_looptri[1]->v->co;
		(*r_vert_cos)[2] = bm_looptri[2]->v->co;
		*r_tri_nor = pnors_short[BM_elem_index_get(bm_looptri[0]->f)];
		(*r_vert_nors)[0] = vnors_short[BM_elem_index_get(bm_looptri[0]->v)];
		(*r_vert_nors)[1] = vnors_short[BM_elem_index_get(bm_looptri[1]->v)];
		(*r_vert_nors)[2] = vnors_short[BM_elem_index_get(bm_looptri[2]->v)];

		*r_is_smooth = BM_elem_flag_test_bool(bm_looptri[0]->f, BM_ELEM_SMOOTH);
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];

		if (use_hide && (rdata->mpoly[mlt->poly].flag & ME_HIDE)) {
			return false;
		}

		mesh_render_data_ensure_poly_normals_short(rdata);

		short (*pnors_short)[3] = rdata->poly_normals_short;

		(*r_vert_cos)[0] = rdata->mvert[rdata->mloop[mlt->tri[0]].v].co;
		(*r_vert_cos)[1] = rdata->mvert[rdata->mloop[mlt->tri[1]].v].co;
		(*r_vert_cos)[2] = rdata->mvert[rdata->mloop[mlt->tri[2]].v].co;
		*r_tri_nor = pnors_short[mlt->poly];
		(*r_vert_nors)[0] = rdata->mvert[rdata->mloop[mlt->tri[0]].v].no;
		(*r_vert_nors)[1] = rdata->mvert[rdata->mloop[mlt->tri[1]].v].no;
		(*r_vert_nors)[2] = rdata->mvert[rdata->mloop[mlt->tri[2]].v].no;

		*r_is_smooth = (rdata->mpoly[mlt->poly].flag & ME_SMOOTH) != 0;
	}
	return true;
}

/* First 2 bytes are bit flags
 * 3rd is for sharp edges
 * 4rd is for creased edges */
enum {
	VFLAG_VERTEX_ACTIVE   = 1 << 0,
	VFLAG_VERTEX_SELECTED = 1 << 1,
	VFLAG_FACE_ACTIVE     = 1 << 2,
	VFLAG_FACE_SELECTED   = 1 << 3,
};

enum {
	VFLAG_EDGE_EXISTS   = 1 << 0,
	VFLAG_EDGE_ACTIVE   = 1 << 1,
	VFLAG_EDGE_SELECTED = 1 << 2,
	VFLAG_EDGE_SEAM     = 1 << 3,
	VFLAG_EDGE_SHARP    = 1 << 4,
	/* Beware to not go over 1 << 7
	 * (see gpu_shader_edit_mesh_overlay_geom.glsl) */
};

static unsigned char mesh_render_data_looptri_flag(MeshRenderData *rdata, const BMFace *efa)
{
	unsigned char fflag = 0;

	if (efa == rdata->efa_act)
		fflag |= VFLAG_FACE_ACTIVE;

	if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
		fflag |= VFLAG_FACE_SELECTED;

	return fflag;
}

static void mesh_render_data_edge_flag(
        const MeshRenderData *rdata, const BMEdge *eed,
        EdgeDrawAttr *eattr)
{
	eattr->e_flag |= VFLAG_EDGE_EXISTS;

	if (eed == rdata->eed_act)
		eattr->e_flag |= VFLAG_EDGE_ACTIVE;

	if (BM_elem_flag_test(eed, BM_ELEM_SELECT))
		eattr->e_flag |= VFLAG_EDGE_SELECTED;

	if (BM_elem_flag_test(eed, BM_ELEM_SEAM))
		eattr->e_flag |= VFLAG_EDGE_SEAM;

	if (!BM_elem_flag_test(eed, BM_ELEM_SMOOTH))
		eattr->e_flag |= VFLAG_EDGE_SHARP;

	/* Use a byte for value range */
	if (rdata->cd.offset.crease != -1) {
		float crease = BM_ELEM_CD_GET_FLOAT(eed, rdata->cd.offset.crease);
		if (crease > 0) {
			eattr->crease = (char)(crease * 255.0f);
		}
	}

	/* Use a byte for value range */
	if (rdata->cd.offset.bweight != -1) {
		float bweight = BM_ELEM_CD_GET_FLOAT(eed, rdata->cd.offset.bweight);
		if (bweight > 0) {
			eattr->bweight = (char)(bweight * 255.0f);
		}
	}
}

static unsigned char mesh_render_data_vertex_flag(MeshRenderData *rdata, const BMVert *eve)
{

	unsigned char vflag = 0;

	/* Current vertex */
	if (eve == rdata->eve_act)
		vflag |= VFLAG_VERTEX_ACTIVE;

	if (BM_elem_flag_test(eve, BM_ELEM_SELECT))
		vflag |= VFLAG_VERTEX_SELECTED;

	return vflag;
}

static void add_overlay_tri(
        MeshRenderData *rdata, Gwn_VertBuf *vbo_pos, Gwn_VertBuf *vbo_nor, Gwn_VertBuf *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int lnor_id, const unsigned int data_id,
        const BMLoop **bm_looptri, const int base_vert_idx)
{
	unsigned char fflag;
	unsigned char vflag;

	if (vbo_pos) {
		for (uint i = 0; i < 3; i++) {
			const float *pos = bm_looptri[i]->v->co;
			GWN_vertbuf_attr_set(vbo_pos, pos_id, base_vert_idx + i, pos);
		}
	}

	if (vbo_nor) {
		/* TODO real loop normal */
		PackedNormal lnor = convert_i10_v3(bm_looptri[0]->f->no);
		for (uint i = 0; i < 3; i++) {
			PackedNormal vnor = convert_i10_v3(bm_looptri[i]->v->no);
			GWN_vertbuf_attr_set(vbo_nor, vnor_id, base_vert_idx + i, &vnor);
			GWN_vertbuf_attr_set(vbo_nor, lnor_id, base_vert_idx + i, &lnor);
		}
	}

	if (vbo_data) {
		fflag = mesh_render_data_looptri_flag(rdata, bm_looptri[0]->f);
		uint i_prev = 1, i = 2;
		for (uint i_next = 0; i_next < 3; i_next++) {
			vflag = mesh_render_data_vertex_flag(rdata, bm_looptri[i]->v);
			EdgeDrawAttr eattr = {0};
			if (bm_looptri[i_next] == bm_looptri[i_prev]->prev) {
				mesh_render_data_edge_flag(rdata, bm_looptri[i_next]->e, &eattr);
			}
			eattr.v_flag = fflag | vflag;
			GWN_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr);

			i_prev = i;
			i = i_next;
		}
	}
}

static void add_overlay_loose_edge(
        MeshRenderData *rdata, Gwn_VertBuf *vbo_pos, Gwn_VertBuf *vbo_nor, Gwn_VertBuf *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int data_id,
        const BMEdge *eed, const int base_vert_idx)
{
	if (vbo_pos) {
		for (int i = 0; i < 2; ++i) {
			const float *pos = (&eed->v1)[i]->co;
			GWN_vertbuf_attr_set(vbo_pos, pos_id, base_vert_idx + i, pos);
		}
	}

	if (vbo_nor) {
		for (int i = 0; i < 2; ++i) {
			PackedNormal vnor = convert_i10_v3((&eed->v1)[i]->no);
			GWN_vertbuf_attr_set(vbo_nor, vnor_id, base_vert_idx + i, &vnor);
		}
	}

	if (vbo_data) {
		EdgeDrawAttr eattr = {0};
		mesh_render_data_edge_flag(rdata, eed, &eattr);
		for (int i = 0; i < 2; ++i) {
			eattr.v_flag = mesh_render_data_vertex_flag(rdata, (&eed->v1)[i]);
			GWN_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr);
		}
	}
}

static void add_overlay_loose_vert(
        MeshRenderData *rdata, Gwn_VertBuf *vbo_pos, Gwn_VertBuf *vbo_nor, Gwn_VertBuf *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int data_id,
        const BMVert *eve, const int base_vert_idx)
{
	if (vbo_pos) {
		const float *pos = eve->co;
		GWN_vertbuf_attr_set(vbo_pos, pos_id, base_vert_idx, pos);
	}

	if (vbo_nor) {
		PackedNormal vnor = convert_i10_v3(eve->no);
		GWN_vertbuf_attr_set(vbo_nor, vnor_id, base_vert_idx, &vnor);
	}

	if (vbo_data) {
		unsigned char vflag[4] = {0, 0, 0, 0};
		vflag[0] = mesh_render_data_vertex_flag(rdata, eve);
		GWN_vertbuf_attr_set(vbo_data, data_id, base_vert_idx, vflag);
	}
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Mesh Gwn_Batch Cache
 * \{ */

typedef struct MeshBatchCache {
	Gwn_VertBuf *pos_in_order;
	Gwn_VertBuf *nor_in_order;
	Gwn_IndexBuf *edges_in_order;
	Gwn_IndexBuf *triangles_in_order;
	Gwn_IndexBuf *overlay_triangles_vpaint;

	Gwn_Batch *all_verts;
	Gwn_Batch *all_edges;
	Gwn_Batch *all_triangles;

	Gwn_VertBuf *pos_with_normals;
	Gwn_VertBuf *pos_with_normals_visible_only;  /* for paint modes with vert/face hide support. */
	Gwn_VertBuf *tri_aligned_weights;
	Gwn_VertBuf *tri_aligned_vert_colors;
	Gwn_VertBuf *tri_aligned_select_id;
	Gwn_VertBuf *tri_aligned_uv;  /* Active UV layer (mloopuv) */
	Gwn_VertBuf *edge_pos_with_select_bool;
	Gwn_VertBuf *pos_with_select_bool;
	Gwn_Batch *triangles_with_normals;

	/* Skip hidden (depending on paint select mode),
	 * 'pos_with_normals' or 'pos_with_normals_visible_only'. */
	Gwn_Batch *triangles_with_weights;
	Gwn_Batch *triangles_with_vert_colors;
	/* Always skip hidden */
	Gwn_Batch *triangles_with_select_id;

	Gwn_Batch *points_with_normals;
	Gwn_Batch *fancy_edges; /* owns its vertex buffer (not shared) */

	/* Maybe have shaded_triangles_data split into pos_nor and uv_tangent
	 * to minimise data transfer for skinned mesh. */
	Gwn_VertFormat shaded_triangles_format;
	Gwn_VertBuf *shaded_triangles_data;
	Gwn_IndexBuf **shaded_triangles_in_order;
	Gwn_Batch **shaded_triangles;

	/* Texture Paint.*/
	/* per-texture batch */
	Gwn_Batch **texpaint_triangles;
	Gwn_Batch  *texpaint_triangles_single;

	/* Edit Cage Mesh buffers */
	Gwn_VertBuf *ed_tri_pos;
	Gwn_VertBuf *ed_tri_nor; /* LoopNor, VertNor */
	Gwn_VertBuf *ed_tri_data;

	Gwn_VertBuf *ed_ledge_pos;
	Gwn_VertBuf *ed_ledge_nor; /* VertNor */
	Gwn_VertBuf *ed_ledge_data;

	Gwn_VertBuf *ed_lvert_pos;
	Gwn_VertBuf *ed_lvert_nor; /* VertNor */
	Gwn_VertBuf *ed_lvert_data;

	Gwn_VertBuf *ed_fcenter_pos;
	Gwn_VertBuf *ed_fcenter_nor;

	Gwn_Batch *overlay_triangles;
	Gwn_Batch *overlay_triangles_nor; /* GWN_PRIM_POINTS */
	Gwn_Batch *overlay_loose_edges;
	Gwn_Batch *overlay_loose_edges_nor; /* GWN_PRIM_POINTS */
	Gwn_Batch *overlay_loose_verts;
	Gwn_Batch *overlay_facedots;

	Gwn_Batch *overlay_weight_faces;
	Gwn_Batch *overlay_weight_verts;
	Gwn_Batch *overlay_paint_edges;

	/* settings to determine if cache is invalid */
	bool is_dirty;
	bool is_really_dirty; /* Instantly invalidates cache, skipping mesh check */
	int edge_len;
	int tri_len;
	int poly_len;
	int vert_len;
	int mat_len;
	bool is_editmode;
} MeshBatchCache;

/* Gwn_Batch cache management. */

static bool mesh_batch_cache_valid(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;

	if (cache == NULL) {
		return false;
	}

	/* XXX find another place for this */
	if (cache->mat_len != mesh_render_mat_len_get(me)) {
		cache->is_dirty = true;
	}

	if (cache->is_editmode != (me->edit_btmesh != NULL)) {
		return false;
	}

	if (cache->is_really_dirty) {
		return false;
	}

	if (cache->is_dirty == false) {
		return true;
	}
	else {
		if (cache->is_editmode) {
			return false;
		}
		else if ((cache->vert_len != mesh_render_verts_len_get(me)) ||
		         (cache->edge_len != mesh_render_edges_len_get(me)) ||
		         (cache->tri_len  != mesh_render_looptri_len_get(me)) ||
		         (cache->poly_len != mesh_render_polys_len_get(me)) ||
		         (cache->mat_len   != mesh_render_mat_len_get(me)))
		{
			return false;
		}
	}

	return true;
}

static void mesh_batch_cache_init(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;

	if (!cache) {
		cache = me->batch_cache = MEM_callocN(sizeof(*cache), __func__);
	}
	else {
		memset(cache, 0, sizeof(*cache));
	}

	cache->is_editmode = me->edit_btmesh != NULL;

	if (cache->is_editmode == false) {
		cache->edge_len = mesh_render_edges_len_get(me);
		cache->tri_len = mesh_render_looptri_len_get(me);
		cache->poly_len = mesh_render_polys_len_get(me);
		cache->vert_len = mesh_render_verts_len_get(me);
	}

	cache->mat_len = mesh_render_mat_len_get(me);

	cache->is_dirty = false;
	cache->is_really_dirty = false;
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
	if (!mesh_batch_cache_valid(me)) {
		mesh_batch_cache_clear(me);
		mesh_batch_cache_init(me);
	}
	return me->batch_cache;
}

void DRW_mesh_batch_cache_dirty(Mesh *me, int mode)
{
	MeshBatchCache *cache = me->batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_MESH_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		case BKE_MESH_BATCH_DIRTY_SELECT:
			GWN_VERTBUF_DISCARD_SAFE(cache->ed_tri_data);
			GWN_VERTBUF_DISCARD_SAFE(cache->ed_ledge_data);
			GWN_VERTBUF_DISCARD_SAFE(cache->ed_lvert_data);
			GWN_VERTBUF_DISCARD_SAFE(cache->ed_fcenter_nor); /* Contains select flag */
			GWN_BATCH_DISCARD_SAFE(cache->overlay_triangles);
			GWN_BATCH_DISCARD_SAFE(cache->overlay_loose_verts);
			GWN_BATCH_DISCARD_SAFE(cache->overlay_loose_edges);

			BATCH_DISCARD_ALL_SAFE(cache->overlay_facedots);
			break;
		case BKE_MESH_BATCH_DIRTY_NOCHECK:
			cache->is_really_dirty = true;
			break;
		default:
			BLI_assert(0);
	}
}

static void mesh_batch_cache_clear(Mesh *me)
{
	MeshBatchCache *cache = me->batch_cache;
	if (!cache) {
		return;
	}

	GWN_BATCH_DISCARD_SAFE(cache->all_verts);
	GWN_BATCH_DISCARD_SAFE(cache->all_edges);
	GWN_BATCH_DISCARD_SAFE(cache->all_triangles);

	GWN_VERTBUF_DISCARD_SAFE(cache->pos_in_order);
	GWN_VERTBUF_DISCARD_SAFE(cache->pos_with_select_bool);
	GWN_INDEXBUF_DISCARD_SAFE(cache->edges_in_order);
	GWN_INDEXBUF_DISCARD_SAFE(cache->triangles_in_order);
	GWN_INDEXBUF_DISCARD_SAFE(cache->overlay_triangles_vpaint);

	GWN_VERTBUF_DISCARD_SAFE(cache->ed_tri_pos);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_tri_nor);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_tri_data);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_ledge_pos);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_ledge_nor);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_ledge_data);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_lvert_pos);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_lvert_nor);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_lvert_data);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_fcenter_pos);
	GWN_VERTBUF_DISCARD_SAFE(cache->ed_fcenter_nor);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_triangles);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_triangles_nor);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_loose_verts);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_loose_edges);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_loose_edges_nor);

	GWN_BATCH_DISCARD_SAFE(cache->overlay_weight_faces);
	GWN_BATCH_DISCARD_SAFE(cache->overlay_weight_verts);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_paint_edges);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_facedots);

	GWN_BATCH_DISCARD_SAFE(cache->triangles_with_normals);
	GWN_BATCH_DISCARD_SAFE(cache->points_with_normals);
	GWN_VERTBUF_DISCARD_SAFE(cache->pos_with_normals);
	GWN_VERTBUF_DISCARD_SAFE(cache->pos_with_normals_visible_only);
	GWN_VERTBUF_DISCARD_SAFE(cache->tri_aligned_vert_colors);
	GWN_VERTBUF_DISCARD_SAFE(cache->tri_aligned_weights);
	GWN_BATCH_DISCARD_SAFE(cache->triangles_with_weights);
	GWN_BATCH_DISCARD_SAFE(cache->triangles_with_vert_colors);
	GWN_VERTBUF_DISCARD_SAFE(cache->tri_aligned_select_id);
	GWN_VERTBUF_DISCARD_SAFE(cache->tri_aligned_uv);
	GWN_BATCH_DISCARD_SAFE(cache->triangles_with_select_id);

	BATCH_DISCARD_ALL_SAFE(cache->fancy_edges);

	GWN_VERTBUF_DISCARD_SAFE(cache->shaded_triangles_data);
	if (cache->shaded_triangles_in_order) {
		for (int i = 0; i < cache->mat_len; ++i) {
			GWN_INDEXBUF_DISCARD_SAFE(cache->shaded_triangles_in_order[i]);
		}
	}
	if (cache->shaded_triangles) {
		for (int i = 0; i < cache->mat_len; ++i) {
			GWN_BATCH_DISCARD_SAFE(cache->shaded_triangles[i]);
		}
	}

	MEM_SAFE_FREE(cache->shaded_triangles_in_order);
	MEM_SAFE_FREE(cache->shaded_triangles);

	if (cache->texpaint_triangles) {
		for (int i = 0; i < cache->mat_len; ++i) {
			GWN_BATCH_DISCARD_SAFE(cache->texpaint_triangles[i]);
		}
	}
	MEM_SAFE_FREE(cache->texpaint_triangles);

	GWN_BATCH_DISCARD_SAFE(cache->texpaint_triangles_single);

}

void DRW_mesh_batch_cache_free(Mesh *me)
{
	mesh_batch_cache_clear(me);
	MEM_SAFE_FREE(me->batch_cache);
}

/* Gwn_Batch cache usage. */

static Gwn_VertBuf *mesh_batch_cache_get_tri_shading_data(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));
#define USE_COMP_MESH_DATA

	if (cache->shaded_triangles_data == NULL) {
		unsigned int vidx = 0;
		const char *attrib_name;

		if (rdata->cd.layers.uv_len + rdata->cd.layers.vcol_len == 0) {
			return NULL;
		}

		Gwn_VertFormat *format = &cache->shaded_triangles_format;

		GWN_vertformat_clear(format);

		/* initialize vertex format */
		unsigned int *uv_id = MEM_mallocN(sizeof(*uv_id) * rdata->cd.layers.uv_len, "UV attrib format");
		unsigned int *vcol_id = MEM_mallocN(sizeof(*vcol_id) * rdata->cd.layers.vcol_len, "Vcol attrib format");
		unsigned int *tangent_id = MEM_mallocN(sizeof(*tangent_id) * rdata->cd.layers.uv_len, "Tangent attrib format");

		for (int i = 0; i < rdata->cd.layers.uv_len; i++) {
			/* UV */
			attrib_name = mesh_render_data_uv_layer_uuid_get(rdata, i);
#if defined(USE_COMP_MESH_DATA) && 0 /* these are clamped. Maybe use them as an option in the future */
			uv_id[i] = GWN_vertformat_attr_add(format, attrib_name, GWN_COMP_I16, 2, GWN_FETCH_INT_TO_FLOAT_UNIT);
#else
			uv_id[i] = GWN_vertformat_attr_add(format, attrib_name, GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
#endif

			/* Auto Name */
			attrib_name = mesh_render_data_uv_auto_layer_uuid_get(rdata, i);
			GWN_vertformat_alias_add(format, attrib_name);

			if (i == rdata->cd.layers.uv_active) {
				GWN_vertformat_alias_add(format, "u");
			}
		}

		for (int i = 0; i < rdata->cd.layers.tangent_len; i++) {
			attrib_name = mesh_render_data_tangent_layer_uuid_get(rdata, i);
			/* WATCH IT : only specifying 3 component instead of 4 (4th is sign).
			 * That may cause some problem but I could not make it to fail (fclem) */
#ifdef USE_COMP_MESH_DATA
			/* Tangents need more precision than 10_10_10 */
			tangent_id[i] = GWN_vertformat_attr_add(format, attrib_name, GWN_COMP_I16, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
#else
			tangent_id[i] = GWN_vertformat_attr_add(format, attrib_name, GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
#endif

			if (i == rdata->cd.layers.tangent_active) {
				GWN_vertformat_alias_add(format, "t");
			}
		}

		for (int i = 0; i < rdata->cd.layers.vcol_len; i++) {
			attrib_name = mesh_render_data_vcol_layer_uuid_get(rdata, i);
			vcol_id[i] = GWN_vertformat_attr_add(format, attrib_name, GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);

			/* Auto layer */
			if (rdata->cd.layers.auto_vcol[i]) {
				attrib_name = mesh_render_data_vcol_auto_layer_uuid_get(rdata, i);
				GWN_vertformat_alias_add(format, attrib_name);
			}

			if (i == rdata->cd.layers.vcol_active) {
				GWN_vertformat_alias_add(format, "c");
			}
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = cache->shaded_triangles_data = GWN_vertbuf_create_with_format(format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		/* TODO deduplicate all verts and make use of Gwn_IndexBuf in
		 * mesh_batch_cache_get_triangles_in_order_split_by_material. */
		for (int i = 0; i < tri_len; i++) {
			float *tri_uvs[3], *tri_tans[3];
			unsigned char *tri_cols[3];

			if (rdata->edit_bmesh == NULL ||
			    BM_elem_flag_test((rdata->edit_bmesh->looptris[i])[0]->f, BM_ELEM_HIDDEN) == 0)
			{
				/* UVs */
				for (int j = 0; j < rdata->cd.layers.uv_len; j++) {
					/* UVs */
					mesh_render_data_looptri_uvs_get(rdata, i, j, &tri_uvs);
#if defined(USE_COMP_MESH_DATA) && 0 /* these are clamped. Maybe use them as an option in the future */
					short s_uvs[3][2];
					normal_float_to_short_v2(s_uvs[0], tri_uvs[0]);
					normal_float_to_short_v2(s_uvs[1], tri_uvs[1]);
					normal_float_to_short_v2(s_uvs[2], tri_uvs[2]);
#else
					float **s_uvs = tri_uvs;
#endif
					GWN_vertbuf_attr_set(vbo, uv_id[j], vidx + 0, s_uvs[0]);
					GWN_vertbuf_attr_set(vbo, uv_id[j], vidx + 1, s_uvs[1]);
					GWN_vertbuf_attr_set(vbo, uv_id[j], vidx + 2, s_uvs[2]);
				}


				/* TANGENTs */
				for (int j = 0; j < rdata->cd.layers.tangent_len; j++) {
					mesh_render_data_looptri_tans_get(rdata, i, j, &tri_tans);
#ifdef USE_COMP_MESH_DATA
					/* Tangents need more precision than 10_10_10 */
					short s_tan[3][3];
					normal_float_to_short_v3(s_tan[0], tri_tans[0]);
					normal_float_to_short_v3(s_tan[1], tri_tans[1]);
					normal_float_to_short_v3(s_tan[2], tri_tans[2]);
#else
					float **s_tan = tri_tans;
#endif
					GWN_vertbuf_attr_set(vbo, tangent_id[j], vidx + 0, s_tan[0]);
					GWN_vertbuf_attr_set(vbo, tangent_id[j], vidx + 1, s_tan[1]);
					GWN_vertbuf_attr_set(vbo, tangent_id[j], vidx + 2, s_tan[2]);
				}

				/* VCOLs */
				for (int j = 0; j < rdata->cd.layers.vcol_len; j++) {
					mesh_render_data_looptri_cols_get(rdata, i, j, &tri_cols);
					GWN_vertbuf_attr_set(vbo, vcol_id[j], vidx + 0, tri_cols[0]);
					GWN_vertbuf_attr_set(vbo, vcol_id[j], vidx + 1, tri_cols[1]);
					GWN_vertbuf_attr_set(vbo, vcol_id[j], vidx + 2, tri_cols[2]);
				}

				vidx += 3;
			}
		}
		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}

		MEM_freeN(uv_id);
		MEM_freeN(vcol_id);
		MEM_freeN(tangent_id);
	}

#undef USE_COMP_MESH_DATA

	return cache->shaded_triangles_data;
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_uv_active(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPUV));
	BLI_assert(rdata->edit_bmesh == NULL);

	if (cache->tri_aligned_uv == NULL) {
		unsigned int vidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint uv; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.uv = GWN_vertformat_attr_add(&format, "uv", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = cache->tri_aligned_uv = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		const MLoopUV *mloopuv = rdata->mloopuv;

		for (int i = 0; i < tri_len; i++) {
			const MLoopTri *mlt = &rdata->mlooptri[i];
			GWN_vertbuf_attr_set(vbo, attr_id.uv, vidx++, mloopuv[mlt->tri[0]].uv);
			GWN_vertbuf_attr_set(vbo, attr_id.uv, vidx++, mloopuv[mlt->tri[1]].uv);
			GWN_vertbuf_attr_set(vbo, attr_id.uv, vidx++, mloopuv[mlt->tri[2]].uv);
		}
		vbo_len_used = vidx;

		BLI_assert(vbo_len_capacity == vbo_len_used);
	}

	return cache->tri_aligned_uv;
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_pos_and_normals_ex(
        MeshRenderData *rdata, const bool use_hide,
        Gwn_VertBuf **r_vbo)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (*r_vbo == NULL) {
		unsigned int vidx = 0, nidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = *r_vbo = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			float *tri_vert_cos[3];
			short *tri_nor, *tri_vert_nors[3];
			bool is_smooth;

			if (mesh_render_data_looptri_cos_nors_smooth_get(
			        rdata, i, use_hide, &tri_vert_cos, &tri_nor, &tri_vert_nors, &is_smooth))
			{
				if (is_smooth) {
					PackedNormal snor_pack[3] = {
						convert_i10_s3(tri_vert_nors[0]),
						convert_i10_s3(tri_vert_nors[1]),
						convert_i10_s3(tri_vert_nors[2])
					};
					PackedNormal *snor[3] = { &snor_pack[0], &snor_pack[1], &snor_pack[2] };

					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor[0]);
					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor[1]);
					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor[2]);
				}
				else {
					PackedNormal snor_pack = convert_i10_s3(tri_nor);
					PackedNormal *snor = &snor_pack;

					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor);
					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor);
					GWN_vertbuf_attr_set(vbo, attr_id.nor, nidx++, snor);
				}

				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, tri_vert_cos[0]);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, tri_vert_cos[1]);
				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, tri_vert_cos[2]);
			}
		}
		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}
	return *r_vbo;
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_pos_and_normals(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	return mesh_batch_cache_get_tri_pos_and_normals_ex(
	        rdata, false,
	        &cache->pos_with_normals);
}
static Gwn_VertBuf *mesh_batch_cache_get_tri_pos_and_normals_visible_only(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	return mesh_batch_cache_get_tri_pos_and_normals_ex(
	        rdata, true,
	        &cache->pos_with_normals_visible_only);
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_weights(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_hide, int defgroup)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT));

	if (cache->tri_aligned_weights == NULL) {
		unsigned int cidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint col; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.col = GWN_vertformat_attr_add(&format, "color", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = cache->tri_aligned_weights = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		mesh_render_data_ensure_vert_weight_color(rdata, defgroup);
		const float (*vert_weight_color)[3] = rdata->vert_weight_color;

		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; i++) {
				const BMLoop **ltri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				/* Assume 'use_hide' */
				if (!BM_elem_flag_test(ltri[0]->f, BM_ELEM_HIDDEN)) {
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						const int v_index = BM_elem_index_get(ltri[tri_corner]->v);
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, vert_weight_color[v_index]);
					}
				}
			}
		}
		else {
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				if (!(use_hide && (rdata->mpoly[mlt->poly].flag & ME_HIDE))) {
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						const uint v_index = rdata->mloop[mlt->tri[tri_corner]].v;
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, vert_weight_color[v_index]);
					}
				}
			}
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_weights;
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_vert_colors(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_hide)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPCOL));

	if (cache->tri_aligned_vert_colors == NULL) {
		unsigned int cidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint col; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.col = GWN_vertformat_attr_add(&format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = cache->tri_aligned_vert_colors = GWN_vertbuf_create_with_format(&format);

		const uint vbo_len_capacity = tri_len * 3;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		mesh_render_data_ensure_vert_color(rdata);
		const char (*vert_color)[3] = rdata->vert_color;

		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; i++) {
				const BMLoop **ltri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				/* Assume 'use_hide' */
				if (!BM_elem_flag_test(ltri[0]->f, BM_ELEM_HIDDEN)) {
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						const int l_index = BM_elem_index_get(ltri[tri_corner]);
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, vert_color[l_index]);
					}
				}
			}
		}
		else {
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				if (!(use_hide && (rdata->mpoly[mlt->poly].flag & ME_HIDE))) {
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						const uint l_index = mlt->tri[tri_corner];
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, vert_color[l_index]);
					}
				}
			}
		}
		const uint vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_vert_colors;
}

static Gwn_VertBuf *mesh_batch_cache_get_tri_select_id(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_hide)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (cache->tri_aligned_select_id == NULL) {
		unsigned int cidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint col; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.col = GWN_vertformat_attr_add(&format, "color", GWN_COMP_I32, 1, GWN_FETCH_INT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_VertBuf *vbo = cache->tri_aligned_select_id = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; i++) {
				const BMLoop **ltri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				/* Assume 'use_hide' */
				if (!BM_elem_flag_test(ltri[0]->f, BM_ELEM_HIDDEN)) {
					const int poly_index = BM_elem_index_get(ltri[0]->f);
					int select_id;
					GPU_select_index_get(poly_index + 1, &select_id);
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, &select_id);
					}
				}
			}
		}
		else {
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				const int poly_index = mlt->poly;
				if (!(use_hide && (rdata->mpoly[poly_index].flag & ME_HIDE))) {
					int select_id;
					GPU_select_index_get(poly_index + 1, &select_id);
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						GWN_vertbuf_attr_set(vbo, attr_id.col, cidx++, &select_id);
					}
				}
			}
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_select_id;
}

static Gwn_VertBuf *mesh_batch_cache_get_vert_pos_and_nor_in_order(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->pos_in_order == NULL) {
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, nor; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.nor = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_I16, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		}

		Gwn_VertBuf *vbo = cache->pos_in_order = GWN_vertbuf_create_with_format(&format);
		const int vbo_len_capacity = mesh_render_data_verts_len_get(rdata);
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter;
			BMVert *eve;
			uint i;

			BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
				static short no_short[3];
				normal_float_to_short_v3(no_short, eve->no);

				GWN_vertbuf_attr_set(vbo, attr_id.pos, i, eve->co);
				GWN_vertbuf_attr_set(vbo, attr_id.nor, i, no_short);
			}
			BLI_assert(i == vbo_len_capacity);
		}
		else {
			for (int i = 0; i < vbo_len_capacity; ++i) {
				GWN_vertbuf_attr_set(vbo, attr_id.pos, i, rdata->mvert[i].co);
				GWN_vertbuf_attr_set(vbo, attr_id.nor, i, rdata->mvert[i].no);
			}
		}
	}

	return cache->pos_in_order;
}

static Gwn_VertFormat *edit_mesh_overlay_pos_format(unsigned int *r_pos_id)
{
	static Gwn_VertFormat format_pos = { 0 };
	static unsigned pos_id;
	if (format_pos.attrib_ct == 0) {
		pos_id = GWN_vertformat_attr_add(&format_pos, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}
	*r_pos_id = pos_id;
	return &format_pos;
}

static Gwn_VertFormat *edit_mesh_overlay_nor_format(unsigned int *r_vnor_id, unsigned int *r_lnor_id)
{
	static Gwn_VertFormat format_nor = { 0 };
	static Gwn_VertFormat format_nor_loop = { 0 };
	static unsigned vnor_id, vnor_loop_id, lnor_id;
	if (format_nor.attrib_ct == 0) {
		vnor_id = GWN_vertformat_attr_add(&format_nor, "vnor", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		vnor_loop_id = GWN_vertformat_attr_add(&format_nor_loop, "vnor", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		lnor_id = GWN_vertformat_attr_add(&format_nor_loop, "lnor", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
	}
	if (r_lnor_id) {
		*r_vnor_id = vnor_loop_id;
		*r_lnor_id = lnor_id;
		return &format_nor_loop;
	}
	else {
		*r_vnor_id = vnor_id;
		return &format_nor;
	}
}

static Gwn_VertFormat *edit_mesh_overlay_data_format(unsigned int *r_data_id)
{
	static Gwn_VertFormat format_flag = { 0 };
	static unsigned data_id;
	if (format_flag.attrib_ct == 0) {
		data_id = GWN_vertformat_attr_add(&format_flag, "data", GWN_COMP_U8, 4, GWN_FETCH_INT);
	}
	*r_data_id = data_id;
	return &format_flag;
}

static void mesh_batch_cache_create_overlay_tri_buffers(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	const int tri_len = mesh_render_data_looptri_len_get(rdata);

	const int vbo_len_capacity = tri_len * 3;
	int vbo_len_used = 0;

	/* Positions */
	Gwn_VertBuf *vbo_pos = NULL;
	static struct { uint pos, vnor, lnor, data; } attr_id;
	if (cache->ed_tri_pos == NULL) {
		vbo_pos = cache->ed_tri_pos =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_pos_format(&attr_id.pos));
		GWN_vertbuf_data_alloc(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	Gwn_VertBuf *vbo_nor = NULL;
	if (cache->ed_tri_nor == NULL) {
		vbo_nor = cache->ed_tri_nor =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_nor_format(&attr_id.vnor, &attr_id.lnor));
		GWN_vertbuf_data_alloc(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	Gwn_VertBuf *vbo_data = NULL;
	if (cache->ed_tri_data == NULL) {
		vbo_data = cache->ed_tri_data =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_data_format(&attr_id.data));
		GWN_vertbuf_data_alloc(vbo_data, vbo_len_capacity);
	}

	for (int i = 0; i < tri_len; i++) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
		if (!BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			add_overlay_tri(
			        rdata, vbo_pos, vbo_nor, vbo_data,
			        attr_id.pos, attr_id.vnor, attr_id.lnor, attr_id.data,
			        bm_looptri, vbo_len_used);

			vbo_len_used += 3;
		}
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			GWN_vertbuf_data_resize(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			GWN_vertbuf_data_resize(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			GWN_vertbuf_data_resize(vbo_data, vbo_len_used);
		}
	}
}

static void mesh_batch_cache_create_overlay_ledge_buffers(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	const int ledge_len = mesh_render_data_loose_edges_len_get(rdata);

	const int vbo_len_capacity = ledge_len * 2;
	int vbo_len_used = 0;

	/* Positions */
	Gwn_VertBuf *vbo_pos = NULL;
	static struct { uint pos, vnor, data; } attr_id;
	if (cache->ed_ledge_pos == NULL) {
		vbo_pos = cache->ed_ledge_pos =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_pos_format(&attr_id.pos));
		GWN_vertbuf_data_alloc(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	Gwn_VertBuf *vbo_nor = NULL;
	if (cache->ed_ledge_nor == NULL) {
		vbo_nor = cache->ed_ledge_nor =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_nor_format(&attr_id.vnor, NULL));
		GWN_vertbuf_data_alloc(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	Gwn_VertBuf *vbo_data = NULL;
	if (cache->ed_ledge_data == NULL) {
		vbo_data = cache->ed_ledge_data =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_data_format(&attr_id.data));
		GWN_vertbuf_data_alloc(vbo_data, vbo_len_capacity);
	}

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		for (uint i = 0; i < ledge_len; i++) {
			const BMEdge *eed = BM_edge_at_index(bm, rdata->loose_edges[i]);
			if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
				add_overlay_loose_edge(
				        rdata, vbo_pos, vbo_nor, vbo_data,
				        attr_id.pos, attr_id.vnor, attr_id.data,
				        eed, vbo_len_used);
				vbo_len_used += 2;
			}
		}
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			GWN_vertbuf_data_resize(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			GWN_vertbuf_data_resize(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			GWN_vertbuf_data_resize(vbo_data, vbo_len_used);
		}
	}
}

static void mesh_batch_cache_create_overlay_lvert_buffers(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	BMesh *bm = rdata->edit_bmesh->bm;
	const int lvert_len = mesh_render_data_loose_verts_len_get(rdata);

	const int vbo_len_capacity = lvert_len;
	int vbo_len_used = 0;

	static struct { uint pos, vnor, data; } attr_id;

	/* Positions */
	Gwn_VertBuf *vbo_pos = NULL;
	if (cache->ed_lvert_pos == NULL) {
		vbo_pos = cache->ed_lvert_pos =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_pos_format(&attr_id.pos));
		GWN_vertbuf_data_alloc(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	Gwn_VertBuf *vbo_nor = NULL;
	if (cache->ed_lvert_nor == NULL) {
		vbo_nor = cache->ed_lvert_nor =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_nor_format(&attr_id.vnor, NULL));
		GWN_vertbuf_data_alloc(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	Gwn_VertBuf *vbo_data = NULL;
	if (cache->ed_lvert_data == NULL) {
		vbo_data = cache->ed_lvert_data =
		        GWN_vertbuf_create_with_format(edit_mesh_overlay_data_format(&attr_id.data));
		GWN_vertbuf_data_alloc(vbo_data, vbo_len_capacity);
	}

	for (uint i = 0; i < lvert_len; i++) {
		BMVert *eve = BM_vert_at_index(bm, rdata->loose_verts[i]);
		add_overlay_loose_vert(
		        rdata, vbo_pos, vbo_nor, vbo_data,
		        attr_id.pos, attr_id.vnor, attr_id.data,
		        eve, vbo_len_used);
		vbo_len_used += 1;
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			GWN_vertbuf_data_resize(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			GWN_vertbuf_data_resize(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			GWN_vertbuf_data_resize(vbo_data, vbo_len_used);
		}
	}
}

/* Position */
static Gwn_VertBuf *mesh_batch_cache_get_edit_tri_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_pos == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_pos;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_ledge_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_pos == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_pos;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_lvert_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_pos == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_pos;
}

/* Normal */
static Gwn_VertBuf *mesh_batch_cache_get_edit_tri_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_nor == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_nor;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_ledge_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_nor == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_nor;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_lvert_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_nor == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_nor;
}

/* Data */
static Gwn_VertBuf *mesh_batch_cache_get_edit_tri_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_data == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_data;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_ledge_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_data == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_data;
}

static Gwn_VertBuf *mesh_batch_cache_get_edit_lvert_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_data == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_data;
}

static Gwn_IndexBuf *mesh_batch_cache_get_edges_in_order(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE));

	if (cache->edges_in_order == NULL) {
		const int vert_len = mesh_render_data_verts_len_get(rdata);
		const int edge_len = mesh_render_data_edges_len_get(rdata);

		Gwn_IndexBufBuilder elb;
		GWN_indexbuf_init(&elb, GWN_PRIM_LINES, edge_len, vert_len);

		BLI_assert(rdata->types & MR_DATATYPE_EDGE);

		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter eiter;
			BMEdge *eed;
			BM_ITER_MESH(eed, &eiter, bm, BM_EDGES_OF_MESH) {
				if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
					GWN_indexbuf_add_line_verts(&elb, BM_elem_index_get(eed->v1),  BM_elem_index_get(eed->v2));
				}
			}
		}
		else {
			const MEdge *ed = rdata->medge;
			for (int i = 0; i < edge_len; i++, ed++) {
				GWN_indexbuf_add_line_verts(&elb, ed->v1, ed->v2);
			}
		}
		cache->edges_in_order = GWN_indexbuf_build(&elb);
	}

	return cache->edges_in_order;
}

static Gwn_IndexBuf *mesh_batch_cache_get_triangles_in_order(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	if (cache->triangles_in_order == NULL) {
		const int vert_len = mesh_render_data_verts_len_get(rdata);
		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_IndexBufBuilder elb;
		GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tri_len, vert_len);

		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; ++i) {
				const BMLoop **ltri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				if (!BM_elem_flag_test(ltri[0]->f, BM_ELEM_HIDDEN)) {
					for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
						GWN_indexbuf_add_generic_vert(&elb, BM_elem_index_get(ltri[tri_corner]->v));
					}
				}
			}
		}
		else {
			for (int i = 0; i < tri_len; ++i) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
					GWN_indexbuf_add_generic_vert(&elb, mlt->tri[tri_corner]);
				}
			}
		}
		cache->triangles_in_order = GWN_indexbuf_build(&elb);
	}

	return cache->triangles_in_order;
}

static Gwn_IndexBuf **mesh_batch_cache_get_triangles_in_order_split_by_material(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_POLY));

	if (cache->shaded_triangles_in_order == NULL) {
		const int poly_len = mesh_render_data_polys_len_get(rdata);
		const int tri_len = mesh_render_data_looptri_len_get(rdata);
		const int mat_len = mesh_render_data_mat_len_get(rdata);

		int *mat_tri_len = MEM_callocN(sizeof(*mat_tri_len) * mat_len, __func__);
		cache->shaded_triangles_in_order = MEM_callocN(sizeof(*cache->shaded_triangles) * mat_len, __func__);
		Gwn_IndexBufBuilder *elb = MEM_callocN(sizeof(*elb) * mat_len, __func__);

		/* Note that polygons (not triangles) are used here.
		 * This OK because result is _guaranteed_ to be the same. */
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter fiter;
			BMFace *efa;

			BM_ITER_MESH(efa, &fiter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					const short ma_id = efa->mat_nr < mat_len ? efa->mat_nr : 0;
					mat_tri_len[ma_id] += (efa->len - 2);
				}
			}
		}
		else {
			for (uint i = 0; i < poly_len; i++) {
				const MPoly *mp = &rdata->mpoly[i]; ;
				const short ma_id = mp->mat_nr < mat_len ? mp->mat_nr : 0;
				mat_tri_len[ma_id] += (mp->totloop - 2);
			}
		}

		/* Init ELBs. */
		for (int i = 0; i < mat_len; ++i) {
			GWN_indexbuf_init(&elb[i], GWN_PRIM_TRIS, mat_tri_len[i], tri_len * 3);
		}

		/* Populate ELBs. */
		uint nidx = 0;
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter fiter;
			BMFace *efa;

			BM_ITER_MESH(efa, &fiter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					const short ma_id = efa->mat_nr < mat_len ? efa->mat_nr : 0;
					for (int j = 2; j < efa->len; j++) {
						GWN_indexbuf_add_tri_verts(&elb[ma_id], nidx + 0, nidx + 1, nidx + 2);
						nidx += 3;
					}
				}
			}
		}
		else {
			for (uint i = 0; i < poly_len; i++) {
				const MPoly *mp = &rdata->mpoly[i]; ;
				const short ma_id = mp->mat_nr < mat_len ? mp->mat_nr : 0;
				for (int j = 2; j < mp->totloop; j++) {
					GWN_indexbuf_add_tri_verts(&elb[ma_id], nidx + 0, nidx + 1, nidx + 2);
					nidx += 3;
				}
			}
		}

		/* Build ELBs. */
		for (int i = 0; i < mat_len; ++i) {
			cache->shaded_triangles_in_order[i] = GWN_indexbuf_build(&elb[i]);
		}

		MEM_freeN(mat_tri_len);
		MEM_freeN(elb);
	}

	return cache->shaded_triangles_in_order;
}

static Gwn_VertBuf *mesh_batch_cache_get_edge_pos_with_sel(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_wire, bool use_select_bool)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_POLY | MR_DATATYPE_LOOP));
	BLI_assert(rdata->edit_bmesh == NULL);

	if (!cache->edge_pos_with_select_bool) {
		unsigned int vidx = 0, cidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, sel; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.sel = GWN_vertformat_attr_add(&format, "select", GWN_COMP_U8, 1, GWN_FETCH_INT);
		}

		const int edge_len = mesh_render_data_edges_len_get(rdata);

		Gwn_VertBuf *vbo = cache->edge_pos_with_select_bool = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = edge_len * 2;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (use_select_bool) {
			mesh_render_data_ensure_edge_select_bool(rdata, use_wire);
		}
		bool *edge_select_bool = use_select_bool ? rdata->edge_select_bool : NULL;

		for (int i = 0; i < edge_len; i++) {
			const MEdge *ed = &rdata->medge[i];

			uchar edge_vert_sel;
			if (use_select_bool && edge_select_bool[i]) {
				edge_vert_sel = true;
			}
			else if (use_wire) {
				edge_vert_sel = false;
			}
			else {
				continue;
			}

			GWN_vertbuf_attr_set(vbo, attr_id.sel, cidx++, &edge_vert_sel);
			GWN_vertbuf_attr_set(vbo, attr_id.sel, cidx++, &edge_vert_sel);

			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, rdata->mvert[ed->v1].co);
			GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx++, rdata->mvert[ed->v2].co);
		}
		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return cache->edge_pos_with_select_bool;
}

static Gwn_IndexBuf *mesh_batch_cache_get_tri_overlay_weight_faces(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	if (cache->overlay_triangles_vpaint == NULL) {
		const int vert_len = mesh_render_data_verts_len_get(rdata);
		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		Gwn_IndexBufBuilder elb;
		GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tri_len, vert_len);

		for (int i = 0; i < tri_len; ++i) {
			const MLoopTri *mlt = &rdata->mlooptri[i];
			if (!(rdata->mpoly[mlt->poly].flag & (ME_FACE_SEL | ME_HIDE))) {
				for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
					GWN_indexbuf_add_generic_vert(&elb, rdata->mloop[mlt->tri[tri_corner]].v);
				}
			}
		}
		cache->overlay_triangles_vpaint = GWN_indexbuf_build(&elb);
	}

	return cache->overlay_triangles_vpaint;
}

/**
 * Non-edit mode vertices (only used for weight-paint mode).
 */
static Gwn_VertBuf *mesh_batch_cache_get_vert_pos_with_overlay_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT));
	BLI_assert(rdata->edit_bmesh == NULL);

	if (cache->pos_with_select_bool == NULL) {
		unsigned int cidx = 0;

		static Gwn_VertFormat format = { 0 };
		static struct { uint data; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.data = GWN_vertformat_attr_add(&format, "data", GWN_COMP_I8, 1, GWN_FETCH_INT);
		}

		const int vert_len = mesh_render_data_verts_len_get(rdata);

		Gwn_VertBuf *vbo = cache->pos_with_select_bool = GWN_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = vert_len;
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);

		for (int i = 0; i < vert_len; i++) {
			const MVert *mv = &rdata->mvert[i];
			const char data = mv->flag & (SELECT | ME_HIDE);
			GWN_vertbuf_attr_set(vbo, attr_id.data, cidx++, &data);
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return cache->pos_with_select_bool;
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Public API
 * \{ */

Gwn_Batch *DRW_mesh_batch_cache_get_all_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_edges == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_EDGE;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->all_edges = GWN_batch_create(
		         GWN_PRIM_LINES, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache),
		         mesh_batch_cache_get_edges_in_order(rdata, cache));

		mesh_render_data_free(rdata);
	}

	return cache->all_edges;
}

Gwn_Batch *DRW_mesh_batch_cache_get_all_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_triangles == NULL) {
		/* create batch from DM */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->all_triangles = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache),
		        mesh_batch_cache_get_triangles_in_order(rdata, cache));

		mesh_render_data_free(rdata);
	}

	return cache->all_triangles;
}

Gwn_Batch *DRW_mesh_batch_cache_get_triangles_with_normals(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_normals == NULL) {
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_normals = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_tri_pos_and_normals(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_normals;
}

Gwn_Batch *DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(Mesh *me, int defgroup)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_weights == NULL) {
		const bool use_hide = (me->editflag & (ME_EDIT_PAINT_VERT_SEL | ME_EDIT_PAINT_FACE_SEL)) != 0;
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_weights = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_tri_weights(rdata, cache, use_hide, defgroup), NULL);

		Gwn_VertBuf *vbo_tris = use_hide ?
		        mesh_batch_cache_get_tri_pos_and_normals_visible_only(rdata, cache) :
		        mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);

		GWN_batch_vertbuf_add(cache->triangles_with_weights, vbo_tris);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_weights;
}

Gwn_Batch *DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_vert_colors == NULL) {
		const bool use_hide = (me->editflag & (ME_EDIT_PAINT_VERT_SEL | ME_EDIT_PAINT_FACE_SEL)) != 0;
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPCOL;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_vert_colors = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_tri_vert_colors(rdata, cache, use_hide), NULL);

		Gwn_VertBuf *vbo_tris = use_hide ?
		        mesh_batch_cache_get_tri_pos_and_normals_visible_only(rdata, cache) :
		        mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		GWN_batch_vertbuf_add(cache->triangles_with_vert_colors, vbo_tris);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_vert_colors;
}


struct Gwn_Batch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me, bool use_hide)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_select_id == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_select_id = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_tri_select_id(rdata, cache, use_hide), NULL);

		Gwn_VertBuf *vbo_tris = use_hide ?
		        mesh_batch_cache_get_tri_pos_and_normals_visible_only(rdata, cache) :
		        mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		GWN_batch_vertbuf_add(cache->triangles_with_select_id, vbo_tris);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_select_id;
}

Gwn_Batch *DRW_mesh_batch_cache_get_points_with_normals(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->points_with_normals == NULL) {
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->points_with_normals = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_tri_pos_and_normals(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->points_with_normals;
}

Gwn_Batch *DRW_mesh_batch_cache_get_all_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_verts == NULL) {
		/* create batch from DM */
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT);

		cache->all_verts = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->all_verts;
}

Gwn_Batch *DRW_mesh_batch_cache_get_fancy_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->fancy_edges == NULL) {
		/* create batch from DM */
		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, n1, n2; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);

			attr_id.n1 = GWN_vertformat_attr_add(&format, "N1", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
			attr_id.n2 = GWN_vertformat_attr_add(&format, "N2", GWN_COMP_I10, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
		}
		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);

		MeshRenderData *rdata = mesh_render_data_create(
		        me, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_POLY);

		const int edge_len = mesh_render_data_edges_len_get(rdata);

		const int vbo_len_capacity = edge_len * 2; /* these are PRIM_LINE verts, not mesh verts */
		int vbo_len_used = 0;
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);
		for (int i = 0; i < edge_len; ++i) {
			float *vcos1, *vcos2;
			float *pnor1 = NULL, *pnor2 = NULL;
			bool is_manifold;

			if (mesh_render_data_edge_vcos_manifold_pnors(rdata, i, &vcos1, &vcos2, &pnor1, &pnor2, &is_manifold)) {

				PackedNormal n1value = { .x = 0, .y = 0, .z = +511 };
				PackedNormal n2value = { .x = 0, .y = 0, .z = -511 };

				if (is_manifold) {
					n1value = convert_i10_v3(pnor1);
					n2value = convert_i10_v3(pnor2);
				}

				const PackedNormal *n1 = &n1value;
				const PackedNormal *n2 = &n2value;

				GWN_vertbuf_attr_set(vbo, attr_id.pos, 2 * i, vcos1);
				GWN_vertbuf_attr_set(vbo, attr_id.n1, 2 * i, n1);
				GWN_vertbuf_attr_set(vbo, attr_id.n2, 2 * i, n2);

				GWN_vertbuf_attr_set(vbo, attr_id.pos, 2 * i + 1, vcos2);
				GWN_vertbuf_attr_set(vbo, attr_id.n1, 2 * i + 1, n1);
				GWN_vertbuf_attr_set(vbo, attr_id.n2, 2 * i + 1, n2);

				vbo_len_used += 2;
			}
		}
		if (vbo_len_used != vbo_len_capacity) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}

		cache->fancy_edges = GWN_batch_create(GWN_PRIM_LINES, vbo, NULL);

		mesh_render_data_free(rdata);
	}

	return cache->fancy_edges;
}

static void mesh_batch_cache_create_overlay_batches(Mesh *me)
{
	BLI_assert(me->edit_btmesh != NULL);

	/* Since MR_DATATYPE_OVERLAY is slow to generate, generate them all at once */
	const int options =
	        MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_POLY |
	        MR_DATATYPE_LOOPTRI | MR_DATATYPE_OVERLAY;

	MeshBatchCache *cache = mesh_batch_cache_get(me);
	MeshRenderData *rdata = mesh_render_data_create(me, options);

	if (cache->overlay_triangles == NULL) {
		cache->overlay_triangles = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_edit_tri_pos(rdata, cache), NULL);
		GWN_batch_vertbuf_add(cache->overlay_triangles, mesh_batch_cache_get_edit_tri_nor(rdata, cache));
		GWN_batch_vertbuf_add(cache->overlay_triangles, mesh_batch_cache_get_edit_tri_data(rdata, cache));
	}

	if (cache->overlay_loose_edges == NULL) {
		cache->overlay_loose_edges = GWN_batch_create(
		        GWN_PRIM_LINES, mesh_batch_cache_get_edit_ledge_pos(rdata, cache), NULL);
		GWN_batch_vertbuf_add(cache->overlay_loose_edges, mesh_batch_cache_get_edit_ledge_nor(rdata, cache));
		GWN_batch_vertbuf_add(cache->overlay_loose_edges, mesh_batch_cache_get_edit_ledge_data(rdata, cache));
	}

	if (cache->overlay_loose_verts == NULL) {
		cache->overlay_loose_verts = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_edit_lvert_pos(rdata, cache), NULL);
		GWN_batch_vertbuf_add(cache->overlay_loose_verts, mesh_batch_cache_get_edit_lvert_nor(rdata, cache));
		GWN_batch_vertbuf_add(cache->overlay_loose_verts, mesh_batch_cache_get_edit_lvert_data(rdata, cache));
	}

	if (cache->overlay_triangles_nor == NULL) {
		cache->overlay_triangles_nor = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_edit_tri_pos(rdata, cache), NULL);
		GWN_batch_vertbuf_add(cache->overlay_triangles_nor, mesh_batch_cache_get_edit_tri_nor(rdata, cache));
	}

	if (cache->overlay_loose_edges_nor == NULL) {
		cache->overlay_loose_edges_nor = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_edit_ledge_pos(rdata, cache), NULL);
		GWN_batch_vertbuf_add(cache->overlay_loose_edges_nor, mesh_batch_cache_get_edit_ledge_nor(rdata, cache));
	}

	mesh_render_data_free(rdata);
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_triangles == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_triangles;
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_loose_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_edges == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_edges;
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_loose_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_verts == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_verts;
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_triangles_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_triangles_nor == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_triangles_nor;
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_loose_edges_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_edges_nor == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_edges_nor;
}

Gwn_Batch *DRW_mesh_batch_cache_get_overlay_facedots(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_facedots == NULL) {
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY);

		static Gwn_VertFormat format = { 0 };
		static struct { uint pos, data; } attr_id;
		if (format.attrib_ct == 0) {
			attr_id.pos = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
			attr_id.data = GWN_vertformat_attr_add(&format, "norAndFlag", GWN_COMP_I10, 4, GWN_FETCH_INT_TO_FLOAT_UNIT);
		}

		const int vbo_len_capacity = mesh_render_data_polys_len_get(rdata);
		int vidx = 0;

		Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(vbo, vbo_len_capacity);
		for (int i = 0; i < vbo_len_capacity; ++i) {
			float pcenter[3], pnor[3];
			bool selected = false;

			if (mesh_render_data_pnors_pcenter_select_get(rdata, i, pnor, pcenter, &selected)) {

				PackedNormal nor = { .x = 0, .y = 0, .z = -511 };
				nor = convert_i10_v3(pnor);
				nor.w = selected ? 1 : 0;
				GWN_vertbuf_attr_set(vbo, attr_id.data, vidx, &nor);

				GWN_vertbuf_attr_set(vbo, attr_id.pos, vidx, pcenter);

				vidx += 1;

			}
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GWN_vertbuf_data_resize(vbo, vbo_len_used);
		}

		cache->overlay_facedots = GWN_batch_create(GWN_PRIM_POINTS, vbo, NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_facedots;
}

Gwn_Batch **DRW_mesh_batch_cache_get_surface_shaded(
        Mesh *me, struct GPUMaterial **gpumat_array, uint gpumat_array_len)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->shaded_triangles == NULL) {
		/* create batch from DM */
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI |
		        MR_DATATYPE_POLY | MR_DATATYPE_SHADING;
		MeshRenderData *rdata = mesh_render_data_create_ex(me, datatype, gpumat_array, gpumat_array_len);

		const int mat_len = mesh_render_data_mat_len_get(rdata);

		cache->shaded_triangles = MEM_callocN(sizeof(*cache->shaded_triangles) * mat_len, __func__);

		Gwn_IndexBuf **el = mesh_batch_cache_get_triangles_in_order_split_by_material(rdata, cache);

		Gwn_VertBuf *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		for (int i = 0; i < mat_len; ++i) {
			cache->shaded_triangles[i] = GWN_batch_create(
			        GWN_PRIM_TRIS, vbo, el[i]);
			Gwn_VertBuf *vbo_shading = mesh_batch_cache_get_tri_shading_data(rdata, cache);
			if (vbo_shading) {
				GWN_batch_vertbuf_add(cache->shaded_triangles[i], vbo_shading);
			}
		}


		mesh_render_data_free(rdata);
	}

	return cache->shaded_triangles;
}

Gwn_Batch **DRW_mesh_batch_cache_get_surface_texpaint(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->texpaint_triangles == NULL) {
		/* create batch from DM */
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOPUV;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		const int mat_len = mesh_render_data_mat_len_get(rdata);

		cache->texpaint_triangles = MEM_callocN(sizeof(*cache->texpaint_triangles) * mat_len, __func__);

		Gwn_IndexBuf **el = mesh_batch_cache_get_triangles_in_order_split_by_material(rdata, cache);

		Gwn_VertBuf *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		for (int i = 0; i < mat_len; ++i) {
			cache->texpaint_triangles[i] = GWN_batch_create(
			        GWN_PRIM_TRIS, vbo, el[i]);
			Gwn_VertBuf *vbo_uv = mesh_batch_cache_get_tri_uv_active(rdata, cache);
			if (vbo_uv) {
				GWN_batch_vertbuf_add(cache->texpaint_triangles[i], vbo_uv);
			}
		}
		mesh_render_data_free(rdata);
	}

	return cache->texpaint_triangles;
}

Gwn_Batch *DRW_mesh_batch_cache_get_surface_texpaint_single(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->texpaint_triangles_single == NULL) {
		/* create batch from DM */
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOPUV;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		Gwn_VertBuf *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);

		cache->texpaint_triangles_single = GWN_batch_create(
		        GWN_PRIM_TRIS, vbo, NULL);
		Gwn_VertBuf *vbo_uv = mesh_batch_cache_get_tri_uv_active(rdata, cache);
		if (vbo_uv) {
			GWN_batch_vertbuf_add(cache->texpaint_triangles_single, vbo_uv);
		}
		mesh_render_data_free(rdata);
	}
	return cache->texpaint_triangles_single;
}

Gwn_Batch *DRW_mesh_batch_cache_get_weight_overlay_edges(Mesh *me, bool use_wire, bool use_sel)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_paint_edges == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_POLY | MR_DATATYPE_LOOP;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->overlay_paint_edges = GWN_batch_create(
		        GWN_PRIM_LINES, mesh_batch_cache_get_edge_pos_with_sel(rdata, cache, use_wire, use_sel), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_paint_edges;
}

Gwn_Batch *DRW_mesh_batch_cache_get_weight_overlay_faces(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_weight_faces == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->overlay_weight_faces = GWN_batch_create(
		        GWN_PRIM_TRIS, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache),
		        mesh_batch_cache_get_tri_overlay_weight_faces(rdata, cache));

		mesh_render_data_free(rdata);
	}

	return cache->overlay_weight_faces;
}

Gwn_Batch *DRW_mesh_batch_cache_get_weight_overlay_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_weight_verts == NULL) {
		/* create batch from Mesh */
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT);

		cache->overlay_weight_verts = GWN_batch_create(
		        GWN_PRIM_POINTS, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache), NULL);

		GWN_batch_vertbuf_add(
		        cache->overlay_weight_verts,
		        mesh_batch_cache_get_vert_pos_with_overlay_data(rdata, cache));
		mesh_render_data_free(rdata);
	}

	return cache->overlay_weight_verts;
}

/** \} */

#undef MESH_RENDER_FUNCTION
