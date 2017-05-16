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
#include "BKE_texture.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_draw.h"

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
	MLoopUV **mloopuv;
	MLoopCol **mloopcol;
	float (**mtangent)[4];
	MDeformVert *dvert;
	MLoopCol *loopcol;

	BMVert *eve_act;
	BMEdge *eed_act;
	BMFace *efa_act;

	int uv_len;
	int vcol_len;

	bool *auto_vcol;

	int uv_active;
	int vcol_active;
	int tangent_active;

	/* Custom-data offsets (only needed for BMesh access) */
	struct {
		int crease;
		int bweight;
		int *uv;
		int *vcol;
	} cd_offset;

	struct {
		char (*auto_mix)[32];
		char (*uv)[32];
		char (*vcol)[32];
		char (*tangent)[32];
	} cd_uuid;

	/* for certain cases we need an output loop-data storage (bmesh tangents) */
	struct {
		CustomData ldata;
		/* grr, special case variable (use in place of 'dm->tangent_mask') */
		char tangent_mask;
	} cd_output;

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
	bool *edge_selection;
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


static MeshRenderData *mesh_render_data_create(Mesh *me, const int types)
{
	MeshRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;
	rdata->mat_len = mesh_render_mat_len_get(me);

	CustomData_reset(&rdata->cd_output.ldata);

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
			rdata->cd_offset.crease = CustomData_get_offset(&bm->edata, CD_CREASE);
			rdata->cd_offset.bweight = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
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
					const BMVert *v = vtable[i];
					if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
						/* Loose vert */
						if (v->e == NULL || !bm_vert_has_visible_edge(v)) {
							lverts[rdata->loose_vert_len++] = i;
						}
					}
				}
			}

			{
				BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
				BMEdge **etable = bm->etable;
				for (int i = 0; i < bm->totedge; i++) {
					const BMEdge *e = etable[i];
					if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
						/* Loose edge */
						if (e->l == NULL || !bm_edge_has_visible_face(e)) {
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
			rdata->loopcol = CustomData_get_layer(&me->ldata, CD_MLOOPCOL);
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

		/* don't access mesh directly, instead use vars taken from BMesh or Mesh */
#define me DONT_USE_THIS

		rdata->uv_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPUV);
		rdata->vcol_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPCOL);

		rdata->mloopuv = MEM_mallocN(sizeof(*rdata->mloopuv) * rdata->uv_len, __func__);
		rdata->mloopcol = MEM_mallocN(sizeof(*rdata->mloopcol) * rdata->vcol_len, __func__);
		rdata->mtangent = MEM_mallocN(sizeof(*rdata->mtangent) * rdata->uv_len, __func__);

		rdata->cd_uuid.uv = MEM_mallocN(sizeof(*rdata->cd_uuid.uv) * rdata->uv_len, __func__);
		rdata->cd_uuid.vcol = MEM_mallocN(sizeof(*rdata->cd_uuid.vcol) * rdata->vcol_len, __func__);
		rdata->cd_uuid.tangent = MEM_mallocN(sizeof(*rdata->cd_uuid.tangent) * rdata->uv_len, __func__);

		rdata->cd_offset.uv = MEM_mallocN(sizeof(*rdata->cd_offset.uv) * rdata->uv_len, __func__);
		rdata->cd_offset.vcol = MEM_mallocN(sizeof(*rdata->cd_offset.vcol) * rdata->vcol_len, __func__);

		/* Allocate max */
		rdata->auto_vcol = MEM_callocN(
		        sizeof(*rdata->auto_vcol) * rdata->vcol_len, __func__);
		rdata->cd_uuid.auto_mix = MEM_mallocN(
		        sizeof(*rdata->cd_uuid.auto_mix) * (rdata->vcol_len + rdata->uv_len), __func__);

		/* XXX FIXME XXX */
		/* We use a hash to identify each data layer based on its name.
		 * Gawain then search for this name in the current shader and bind if it exists.
		 * NOTE : This is prone to hash collision.
		 * One solution to hash collision would be to format the cd layer name
		 * to a safe glsl var name, but without name clash.
		 * NOTE 2 : Replicate changes to code_generate_vertex_new() in gpu_codegen.c */
		for (int i = 0; i < rdata->vcol_len; ++i) {
			const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
			unsigned int hash = BLI_ghashutil_strhash_p(name);
			BLI_snprintf(rdata->cd_uuid.vcol[i], sizeof(*rdata->cd_uuid.vcol), "c%u", hash);
			rdata->mloopcol[i] = CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i);
			if (rdata->edit_bmesh) {
				rdata->cd_offset.vcol[i] = CustomData_get_n_offset(&rdata->edit_bmesh->bm->ldata, CD_MLOOPCOL, i);
			}

			/* Gather number of auto layers. */
			/* We only do vcols that are not overridden by uvs */
			if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, name) == -1) {
				BLI_snprintf(
				        rdata->cd_uuid.auto_mix[rdata->uv_len + i],
				        sizeof(*rdata->cd_uuid.auto_mix), "a%u", hash);
				rdata->auto_vcol[i] = true;
			}
		}

		/* Start Fresh */
		CustomData_free_layers(cd_ldata, CD_MLOOPTANGENT, rdata->loop_len);
		for (int i = 0; i < rdata->uv_len; ++i) {
			const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
			unsigned int hash = BLI_ghashutil_strhash_p(name);

			{
				/* UVs */
				BLI_snprintf(rdata->cd_uuid.uv[i], sizeof(*rdata->cd_uuid.uv), "u%u", hash);
				rdata->mloopuv[i] = CustomData_get_layer_n(cd_ldata, CD_MLOOPUV, i);
				if (rdata->edit_bmesh) {
					rdata->cd_offset.uv[i] = CustomData_get_n_offset(&rdata->edit_bmesh->bm->ldata, CD_MLOOPUV, i);
				}
				BLI_snprintf(rdata->cd_uuid.auto_mix[i], sizeof(*rdata->cd_uuid.auto_mix), "a%u", hash);
			}

			{
				/* Tangents*/
				BLI_snprintf(rdata->cd_uuid.tangent[i], sizeof(*rdata->cd_uuid.tangent), "t%u", hash);

				if (rdata->edit_bmesh) {
					BMEditMesh *em = rdata->edit_bmesh;
					BMesh *bm = em->bm;

					if (!CustomData_has_layer(&rdata->cd_output.ldata, CD_MLOOPTANGENT)) {
						bool calc_active_tangent = false;
						float (*poly_normals)[3] = rdata->poly_normals;
						float (*loop_normals)[3] = CustomData_get_layer(cd_ldata, CD_NORMAL);
						char tangent_names[MAX_MTFACE][MAX_NAME];
						int tangent_names_len = 0;
						for (tangent_names_len = 0; tangent_names_len < rdata->uv_len; tangent_names_len++) {
							BLI_strncpy(
							        tangent_names[tangent_names_len],
							        CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, tangent_names_len), MAX_NAME);
						}

						BKE_editmesh_loop_tangent_calc(
						        em, calc_active_tangent,
						        tangent_names, tangent_names_len,
						        poly_normals, loop_normals,
						        rdata->orco,
						        &rdata->cd_output.ldata, bm->totloop,
						        &rdata->cd_output.tangent_mask);
					}

					/* note: BKE_editmesh_loop_tangent_calc calculates 'CD_TANGENT',
					 * not 'CD_MLOOPTANGENT' (as done below). It's OK, they're compatible. */
					rdata->mtangent[i] = CustomData_get_layer_n(&rdata->cd_output.ldata, CD_TANGENT, i);
					BLI_assert(rdata->mtangent[i] != NULL);

					/* special case, we don't use offsets here */
				}
				else {
#undef me
					if (!CustomData_has_layer(cd_ldata, CD_NORMAL)) {
						BKE_mesh_calc_normals_split(me);
					}

					float (*loopnors)[3] = CustomData_get_layer(cd_ldata, CD_NORMAL);

					rdata->mtangent[i] = CustomData_add_layer(
					        cd_ldata, CD_MLOOPTANGENT, CD_CALLOC, NULL, me->totloop);
					CustomData_set_layer_flag(cd_ldata, CD_MLOOPTANGENT, CD_FLAG_TEMPORARY);

					BKE_mesh_loop_tangents_ex(me->mvert, me->totvert, me->mloop, rdata->mtangent[i],
					      loopnors, rdata->mloopuv[i], me->totloop, me->mpoly, me->totpoly, NULL);
#define me DONT_USE_THIS
				}
			}
		}

		rdata->uv_active = CustomData_get_active_layer_index(
		        cd_ldata, CD_MLOOPUV) - CustomData_get_layer_index(cd_ldata, CD_MLOOPUV);
		rdata->vcol_active = CustomData_get_active_layer_index(
		        cd_ldata, CD_MLOOPCOL) - CustomData_get_layer_index(cd_ldata, CD_MLOOPCOL);
		rdata->tangent_active = CustomData_get_active_layer_index(
		        cd_ldata, CD_MLOOPTANGENT) - CustomData_get_layer_index(cd_ldata, CD_MLOOPTANGENT);

#undef me
	}

	return rdata;
}

static void mesh_render_data_free(MeshRenderData *rdata)
{
	MEM_SAFE_FREE(rdata->auto_vcol);
	MEM_SAFE_FREE(rdata->cd_offset.uv);
	MEM_SAFE_FREE(rdata->cd_offset.vcol);
	MEM_SAFE_FREE(rdata->cd_uuid.auto_mix);
	MEM_SAFE_FREE(rdata->cd_uuid.uv);
	MEM_SAFE_FREE(rdata->cd_uuid.vcol);
	MEM_SAFE_FREE(rdata->cd_uuid.tangent);
	MEM_SAFE_FREE(rdata->orco);
	MEM_SAFE_FREE(rdata->mloopuv);
	MEM_SAFE_FREE(rdata->mloopcol);
	MEM_SAFE_FREE(rdata->mtangent);
	MEM_SAFE_FREE(rdata->loose_verts);
	MEM_SAFE_FREE(rdata->loose_edges);
	MEM_SAFE_FREE(rdata->edges_adjacent_polys);
	MEM_SAFE_FREE(rdata->mlooptri);
	MEM_SAFE_FREE(rdata->poly_normals);
	MEM_SAFE_FREE(rdata->poly_normals_short);
	MEM_SAFE_FREE(rdata->vert_normals_short);
	MEM_SAFE_FREE(rdata->vert_weight_color);
	MEM_SAFE_FREE(rdata->edge_selection);
	MEM_SAFE_FREE(rdata->vert_color);

	CustomData_free(&rdata->cd_output.ldata, rdata->loop_len);

	MEM_freeN(rdata);
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Accessor Functions
 * \{ */

static const char *mesh_render_data_uv_auto_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd_uuid.auto_mix[layer];
}

static const char *mesh_render_data_vcol_auto_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd_uuid.auto_mix[rdata->uv_len + layer];
}

static const char *mesh_render_data_uv_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd_uuid.uv[layer];
}

static const char *mesh_render_data_vcol_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd_uuid.vcol[layer];
}

static const char *mesh_render_data_tangent_layer_uuid_get(const MeshRenderData *rdata, int layer)
{
	BLI_assert(rdata->types & MR_DATATYPE_SHADING);
	return rdata->cd_uuid.tangent[layer];
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

static float *mesh_render_data_vert_co(const MeshRenderData *rdata, const int vert_idx)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMVert *bv = BM_vert_at_index(bm, vert_idx);
		return bv->co;
	}
	else {
		return rdata->mvert[vert_idx].co;
	}
}

static short *mesh_render_data_vert_nor(const MeshRenderData *rdata, const int vert_idx)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (rdata->edit_bmesh) {
		static short fno[3];
		BMesh *bm = rdata->edit_bmesh->bm;
		BMVert *bv = BM_vert_at_index(bm, vert_idx);
		normal_float_to_short_v3(fno, bv->no);
		return fno;
	}
	else {
		return rdata->mvert[vert_idx].no;
	}
}

static bool mesh_render_data_edge_verts_indices_get(
        const MeshRenderData *rdata, const int edge_idx,
        int r_vert_idx[2])
{
	BLI_assert(rdata->types & MR_DATATYPE_EDGE);

	if (rdata->edit_bmesh) {
		const BMEdge *bm_edge = BM_edge_at_index(rdata->edit_bmesh->bm, edge_idx);
		if (BM_elem_flag_test(bm_edge, BM_ELEM_HIDDEN)) {
			return false;
		}
		r_vert_idx[0] = BM_elem_index_get(bm_edge->v1);
		r_vert_idx[1] = BM_elem_index_get(bm_edge->v2);
	}
	else {
		const MEdge *me = &rdata->medge[edge_idx];
		r_vert_idx[0] = me->v1;
		r_vert_idx[1] = me->v2;
	}
	return true;
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
			BMFace *face;
			int i;

			pnors_short = rdata->poly_normals_short = MEM_mallocN(sizeof(*pnors_short) * rdata->poly_len, __func__);
			BM_ITER_MESH_INDEX(face, &fiter, bm, BM_FACES_OF_MESH, i) {
				normal_float_to_short_v3(pnors_short[i], face->no);
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
			BMVert *vert;
			int i;

			vnors_short = rdata->vert_normals_short = MEM_mallocN(sizeof(*vnors_short) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(vert, &viter, bm, BM_VERT, i) {
				normal_float_to_short_v3(vnors_short[i], vert->no);
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
			BMFace *face;
			int i = 0;

			BM_ITER_MESH(face, &fiter, bm, BM_FACES_OF_MESH) {
				BMLoop *l_iter, *l_first;
				l_iter = l_first = BM_FACE_FIRST_LOOP(face);
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
			if (rdata->loopcol == NULL) {
				goto fallback;
			}

			vcol = rdata->vert_color = MEM_mallocN(sizeof(*vcol) * rdata->loop_len, __func__);

			for (int i = 0; i < rdata->loop_len; i++) {
				vcol[i][0] = rdata->loopcol[i].r;
				vcol[i][1] = rdata->loopcol[i].g;
				vcol[i][2] = rdata->loopcol[i].b;
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


/** Ensure #MeshRenderData.vert_color */
static void mesh_render_data_ensure_vert_weight_color(MeshRenderData *rdata, const int defgroup)
{
	float (*vweight)[3] = rdata->vert_weight_color;
	if (vweight == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
			if (cd_dvert_offset == -1) {
				goto fallback;
			}

			BMIter viter;
			BMVert *vert;
			int i;

			vweight = rdata->vert_weight_color = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(vert, &viter, bm, BM_VERT, i) {
				const MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(vert, cd_dvert_offset);
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
		const BMFace *bf = BM_face_at_index(rdata->edit_bmesh->bm, poly);
		if (BM_elem_flag_test(bf, BM_ELEM_HIDDEN)) {
			return false;
		}
		BM_face_calc_center_mean(bf, r_center);
		BM_face_calc_normal(bf, r_pnors);
		*r_selected = (BM_elem_flag_test(bf, BM_ELEM_SELECT) != 0) ? true : false;
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
		BMEdge *bm_edge = BM_edge_at_index(bm, edge_index);
		if (BM_elem_flag_test(bm_edge, BM_ELEM_HIDDEN)) {
			return false;
		}
		*r_vco1 = bm_edge->v1->co;
		*r_vco2 = bm_edge->v2->co;
		if (BM_edge_is_manifold(bm_edge)) {
			*r_pnor1 = bm_edge->l->f->no;
			*r_pnor2 = bm_edge->l->radial_next->f->no;
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

static bool mesh_render_data_looptri_vert_indices_get(
        const MeshRenderData *rdata, const int tri_idx,
        int r_vert_idx[3])
{
	BLI_assert(rdata->types & (MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP));

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}
		r_vert_idx[0] = BM_elem_index_get(bm_looptri[0]->v);
		r_vert_idx[1] = BM_elem_index_get(bm_looptri[1]->v);
		r_vert_idx[2] = BM_elem_index_get(bm_looptri[2]->v);
	}
	else {
		const unsigned int *l_idx = rdata->mlooptri[tri_idx].tri;
		const MLoop *l_tri[3] = {&rdata->mloop[l_idx[0]], &rdata->mloop[l_idx[1]], &rdata->mloop[l_idx[2]]};
		r_vert_idx[0] = l_tri[0]->v;
		r_vert_idx[1] = l_tri[1]->v;
		r_vert_idx[2] = l_tri[2]->v;
	}

	return true;
}

static bool mesh_render_data_looptri_mat_index_get(
        const MeshRenderData *rdata, const int tri_idx,
        short *r_face_mat)
{
	BLI_assert(rdata->types & (MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}
		*r_face_mat = ((BMFace *)bm_looptri[0]->f)->mat_nr;
	}
	else {
		const int poly_idx = rdata->mlooptri[tri_idx].poly; ;
		const MPoly *poly = &rdata->mpoly[poly_idx]; ;
		*r_face_mat = poly->mat_nr;
	}

	return true;
}

/**
 * Version of #mesh_render_data_looptri_verts_indices_get that assigns
 * edge indices too \a r_edges_idx (-1 for non-existant edges).
 */
static bool mesh_render_data_looptri_vert_edge_indices_get(
        const MeshRenderData *rdata, const int tri_idx,
        int r_vert_idx[3], int r_edges_idx[3])
{
	BLI_assert(rdata->types & (MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP));

	unsigned int e_pair_edge[2];
	unsigned int e_pair_loop[2];

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];

		if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}

		/* assign 'r_edges_idx' & 'r_vert_idx' */
		int j, j_next;
		for (j = 2, j_next = 0; j_next < 3; j = j_next++) {
			const BMLoop *l = bm_looptri[j], *l_next = bm_looptri[j_next];
			const BMEdge *e = l->e;
			ARRAY_SET_ITEMS(e_pair_edge, BM_elem_index_get(e->v1), BM_elem_index_get(e->v2));
			ARRAY_SET_ITEMS(e_pair_loop, BM_elem_index_get(l->v), BM_elem_index_get(l_next->v));
			if ((e_pair_edge[0] == e_pair_loop[0] && e_pair_edge[1] == e_pair_loop[1]) ||
			    (e_pair_edge[0] == e_pair_loop[1] && e_pair_edge[1] == e_pair_loop[0]))
			{
				r_edges_idx[j] = BM_elem_index_get(l->e);
			}
			else {
				r_edges_idx[j] = -1;
			}
			r_vert_idx[j] = e_pair_loop[0];  /* BM_elem_index_get(l->v) */
		}
	}
	else {
		const unsigned int *l_idx = rdata->mlooptri[tri_idx].tri;
		const MLoop *l_tri[3] = {&rdata->mloop[l_idx[0]], &rdata->mloop[l_idx[1]], &rdata->mloop[l_idx[2]]};

		/* assign 'r_edges_idx' & 'r_vert_idx' */
		int j, j_next;
		for (j = 2, j_next = 0; j_next < 3; j = j_next++) {
			const MLoop *l = l_tri[j], *l_next = l_tri[j_next];
			const MEdge *e = &rdata->medge[l->e]; \
			ARRAY_SET_ITEMS(e_pair_edge, e->v1, e->v2);
			ARRAY_SET_ITEMS(e_pair_loop, l->v, l_next->v);
			if ((e_pair_edge[0] == e_pair_loop[0] && e_pair_edge[1] == e_pair_loop[1]) ||
			    (e_pair_edge[0] == e_pair_loop[1] && e_pair_edge[1] == e_pair_loop[0]))
			{
				r_edges_idx[j] = l->e;
			}
			else {
				r_edges_idx[j] = -1;
			}
			r_vert_idx[j] = e_pair_loop[0];  /* l->v */
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
		(*r_vert_uvs)[0] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[0], rdata->cd_offset.uv[uv_layer]))->uv;
		(*r_vert_uvs)[1] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[1], rdata->cd_offset.uv[uv_layer]))->uv;
		(*r_vert_uvs)[2] = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(bm_looptri[2], rdata->cd_offset.uv[uv_layer]))->uv;
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_uvs)[0] = rdata->mloopuv[uv_layer][mlt->tri[0]].uv;
		(*r_vert_uvs)[1] = rdata->mloopuv[uv_layer][mlt->tri[1]].uv;
		(*r_vert_uvs)[2] = rdata->mloopuv[uv_layer][mlt->tri[2]].uv;
	}
}

static void mesh_render_data_looptri_cols_get(
        MeshRenderData *rdata, const int tri_idx, const int vcol_layer,
        unsigned char *(*r_vert_cols)[3])
{
	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		(*r_vert_cols)[0] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[0], rdata->cd_offset.vcol[vcol_layer]))->r;
		(*r_vert_cols)[1] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[1], rdata->cd_offset.vcol[vcol_layer]))->r;
		(*r_vert_cols)[2] = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(bm_looptri[2], rdata->cd_offset.vcol[vcol_layer]))->r;
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_cols)[0] = &rdata->mloopcol[vcol_layer][mlt->tri[0]].r;
		(*r_vert_cols)[1] = &rdata->mloopcol[vcol_layer][mlt->tri[1]].r;
		(*r_vert_cols)[2] = &rdata->mloopcol[vcol_layer][mlt->tri[2]].r;
	}
}

static void mesh_render_data_looptri_tans_get(
        MeshRenderData *rdata, const int tri_idx, const int tangent_layer,
        float *(*r_vert_tans)[3])
{
	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		(*r_vert_tans)[0] = rdata->mtangent[tangent_layer][BM_elem_index_get(bm_looptri[0])];
		(*r_vert_tans)[1] = rdata->mtangent[tangent_layer][BM_elem_index_get(bm_looptri[1])];
		(*r_vert_tans)[2] = rdata->mtangent[tangent_layer][BM_elem_index_get(bm_looptri[2])];
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		(*r_vert_tans)[0] = rdata->mtangent[tangent_layer][mlt->tri[0]];
		(*r_vert_tans)[1] = rdata->mtangent[tangent_layer][mlt->tri[1]];
		(*r_vert_tans)[2] = rdata->mtangent[tangent_layer][mlt->tri[2]];
	}
}

static bool mesh_render_data_looptri_cos_nors_smooth_get(
        MeshRenderData *rdata, const int tri_idx,
        float *(*r_vert_cos)[3], short **r_tri_nor, short *(*r_vert_nors)[3], bool *r_is_smooth)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);
	BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
	BLI_assert(rdata->types & MR_DATATYPE_LOOP);
	BLI_assert(rdata->types & MR_DATATYPE_POLY);

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];

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

static bool mesh_render_data_looptri_weights_get(
        MeshRenderData *rdata, const int tri_idx, int defgroup,
        float *(*r_vert_weights)[3])
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT));

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];

		if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}
		mesh_render_data_ensure_vert_weight_color(rdata, defgroup);

		float (*vweight)[3] = rdata->vert_weight_color;

		(*r_vert_weights)[0] = vweight[BM_elem_index_get(bm_looptri[0]->v)];
		(*r_vert_weights)[1] = vweight[BM_elem_index_get(bm_looptri[1]->v)];
		(*r_vert_weights)[2] = vweight[BM_elem_index_get(bm_looptri[2]->v)];
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];

		mesh_render_data_ensure_vert_weight_color(rdata, defgroup);

		float (*vweight)[3] = rdata->vert_weight_color;
		(*r_vert_weights)[0] = vweight[rdata->mloop[mlt->tri[0]].v];
		(*r_vert_weights)[1] = vweight[rdata->mloop[mlt->tri[1]].v];
		(*r_vert_weights)[2] = vweight[rdata->mloop[mlt->tri[2]].v];
	}

	return true;
}

static bool mesh_render_data_looptri_vert_colors_get(
        MeshRenderData *rdata, const int tri_idx,
        char *(*r_vert_colors)[3])
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPCOL));

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];

		mesh_render_data_ensure_vert_color(rdata);

		char (*vcol)[3] = rdata->vert_color;

		(*r_vert_colors)[0] = vcol[BM_elem_index_get(bm_looptri[0]->v)];
		(*r_vert_colors)[1] = vcol[BM_elem_index_get(bm_looptri[1]->v)];
		(*r_vert_colors)[2] = vcol[BM_elem_index_get(bm_looptri[2]->v)];
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];

		mesh_render_data_ensure_poly_normals_short(rdata);
		mesh_render_data_ensure_vert_color(rdata);

		char (*vcol)[3] = rdata->vert_color;

		(*r_vert_colors)[0] = vcol[mlt->tri[0]];
		(*r_vert_colors)[1] = vcol[mlt->tri[1]];
		(*r_vert_colors)[2] = vcol[mlt->tri[2]];
	}

	return true;
}

static bool mesh_render_data_looptri_select_id_get(
        MeshRenderData *rdata, const int tri_idx, const bool use_hide,
        int *r_select_id)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT));

	if (rdata->edit_bmesh) {
		const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[tri_idx];
		const int poly_index = BM_elem_index_get(bm_looptri[0]->f);

		if (use_hide && BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
			return false;
		}

		GPU_select_index_get(poly_index + 1, r_select_id);
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];
		const int poly_index = mlt->poly;

		if (use_hide && (rdata->mpoly[poly_index].flag & ME_HIDE)) {
			return false;
		}
		GPU_select_index_get(poly_index + 1, r_select_id);
	}

	return true;
}

static bool mesh_render_data_edge_cos_sel_get(
        MeshRenderData *rdata, const int edge_idx,
        float r_vert_cos[2][3], int *r_vert_sel,
        bool use_wire, bool use_sel)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_POLY | MR_DATATYPE_LOOP));

	if (rdata->edit_bmesh) {
		return false;
	}
	else {
		const MEdge *ed = &rdata->medge[edge_idx];

		if (!rdata->edge_selection && use_sel) {
			rdata->edge_selection = MEM_callocN(sizeof(*rdata->edge_selection) * rdata->edge_len, __func__);

			for (int i = 0; i < rdata->poly_len; i++) {
				MPoly *poly = &rdata->mpoly[i];

				if (poly->flag & ME_FACE_SEL) {
					for (int j = 0; j < poly->totloop; j++) {
						MLoop *loop = &rdata->mloop[poly->loopstart + j];
						if (use_wire) {
							rdata->edge_selection[loop->e] = true;
						}
						else {
							rdata->edge_selection[loop->e] = !rdata->edge_selection[loop->e];
						}
					}
				}
			}
		}

		if (use_sel && rdata->edge_selection[edge_idx]) {
			*r_vert_sel = true;
		}
		else {
			if (use_wire) {
				*r_vert_sel = false;
			}
			else {
				return false;
			}
		}

		copy_v3_v3(r_vert_cos[0], rdata->mvert[ed->v1].co);
		copy_v3_v3(r_vert_cos[1], rdata->mvert[ed->v2].co);
	}

	return true;
}

static bool mesh_render_data_tri_cos_sel_get(
        MeshRenderData *rdata, const int tri_idx,
        float r_vert_cos[3][3])
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI));

	if (rdata->edit_bmesh) {
		return false;
	}
	else {
		const MLoopTri *mlt = &rdata->mlooptri[tri_idx];

		if (rdata->mpoly[mlt->poly].flag & ME_FACE_SEL) {
			return false;
		}

		copy_v3_v3(r_vert_cos[0], rdata->mvert[rdata->mloop[mlt->tri[0]].v].co);
		copy_v3_v3(r_vert_cos[1], rdata->mvert[rdata->mloop[mlt->tri[1]].v].co);
		copy_v3_v3(r_vert_cos[2], rdata->mvert[rdata->mloop[mlt->tri[2]].v].co);
	}

	return true;
}

static bool mesh_render_data_vert_cos_sel_get(
        MeshRenderData *rdata, const int vert_idx,
        float r_vert_co[3], int *r_vert_sel)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT));

	if (rdata->edit_bmesh) {
		return false;
	}
	else {
		const MVert *mv = &rdata->mvert[vert_idx];

		if (mv->flag & SELECT) {
			*r_vert_sel = true;
		}
		else {
			*r_vert_sel = false;
		}

		copy_v3_v3(r_vert_co, mv->co);
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

static unsigned char mesh_render_data_looptri_flag(MeshRenderData *rdata, const int f)
{
	unsigned char fflag = 0;

	if (rdata->edit_bmesh) {
		BMFace *bf = rdata->edit_bmesh->looptris[f][0]->f;

		if (bf == rdata->efa_act)
			fflag |= VFLAG_FACE_ACTIVE;

		if (BM_elem_flag_test(bf, BM_ELEM_SELECT))
			fflag |= VFLAG_FACE_SELECTED;
	}

	return fflag;
}

static EdgeDrawAttr *mesh_render_data_edge_flag(MeshRenderData *rdata, const int e)
{
	static EdgeDrawAttr eattr;
	memset(&eattr, 0, sizeof(eattr));

	if (e == -1) {
		return &eattr;
	}

	/* if edge exists */
	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMEdge *be = NULL;

		be = BM_edge_at_index(bm, e);

		eattr.e_flag |= VFLAG_EDGE_EXISTS;

		if (be == rdata->eed_act)
			eattr.e_flag |= VFLAG_EDGE_ACTIVE;

		if (BM_elem_flag_test(be, BM_ELEM_SELECT))
			eattr.e_flag |= VFLAG_EDGE_SELECTED;

		if (BM_elem_flag_test(be, BM_ELEM_SEAM))
			eattr.e_flag |= VFLAG_EDGE_SEAM;

		if (!BM_elem_flag_test(be, BM_ELEM_SMOOTH))
			eattr.e_flag |= VFLAG_EDGE_SHARP;

		/* Use a byte for value range */
		if (rdata->cd_offset.crease != -1) {
			float crease = BM_ELEM_CD_GET_FLOAT(be, rdata->cd_offset.crease);
			if (crease > 0) {
				eattr.crease = (char)(crease * 255.0f);
			}
		}

		/* Use a byte for value range */
		if (rdata->cd_offset.bweight != -1) {
			float bweight = BM_ELEM_CD_GET_FLOAT(be, rdata->cd_offset.bweight);
			if (bweight > 0) {
				eattr.bweight = (char)(bweight * 255.0f);
			}
		}
	}
	else {
		eattr.e_flag |= VFLAG_EDGE_EXISTS;
	}

	return &eattr;
}

static unsigned char mesh_render_data_vertex_flag(MeshRenderData *rdata, const int v)
{

	unsigned char vflag = 0;

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMVert *bv = BM_vert_at_index(bm, v);

		/* Current vertex */
		if (bv == rdata->eve_act)
			vflag |= VFLAG_VERTEX_ACTIVE;

		if (BM_elem_flag_test(bv, BM_ELEM_SELECT))
			vflag |= VFLAG_VERTEX_SELECTED;
	}

	return vflag;
}

static void add_overlay_tri(
        MeshRenderData *rdata, VertexBuffer *vbo_pos, VertexBuffer *vbo_nor, VertexBuffer *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int lnor_id, const unsigned int data_id,
        const int tri_vert_idx[3], const int tri_edge_idx[3], const int f, const int base_vert_idx)
{
	EdgeDrawAttr *eattr;
	unsigned char fflag;
	unsigned char vflag;

	if (vbo_pos) {
		for (int i = 0; i < 3; ++i) {
			const float *pos = mesh_render_data_vert_co(rdata, tri_vert_idx[i]);
			VertexBuffer_set_attrib(vbo_pos, pos_id, base_vert_idx + i, pos);
		}
	}

	if (vbo_nor) {
		float *tri_vert_cos[3];
		short *tri_nor, *tri_vert_nors[3];
		bool is_smooth;

		mesh_render_data_looptri_cos_nors_smooth_get(
		        rdata, f, &tri_vert_cos, &tri_nor, &tri_vert_nors, &is_smooth);
		for (int i = 0; i < 3; ++i) {
			/* TODO real loop normal */
			const short *svnor = mesh_render_data_vert_nor(rdata, tri_vert_idx[i]);
			const short *slnor = tri_vert_nors[i];
			fflag = mesh_render_data_looptri_flag(rdata, f);
#if USE_10_10_10
			PackedNormal vnor = convert_i10_s3(svnor);
			PackedNormal lnor = convert_i10_s3(slnor);
			VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx + i, &vnor);
			VertexBuffer_set_attrib(vbo_nor, lnor_id, base_vert_idx + i, &lnor);
#else
			VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx + i, svnor);
			VertexBuffer_set_attrib(vbo_nor, lnor_id, base_vert_idx + i, slnor);
#endif
		}
	}

	if (vbo_data) {
		fflag = mesh_render_data_looptri_flag(rdata, f);
		for (int i = 0; i < 3; ++i) {
			int iedge = (i == 2) ? 0 : i+1;
			eattr = mesh_render_data_edge_flag(rdata, tri_edge_idx[iedge]);
			vflag = mesh_render_data_vertex_flag(rdata, tri_vert_idx[i]);
			eattr->v_flag = fflag | vflag;
			VertexBuffer_set_attrib(vbo_data, data_id, base_vert_idx + i, eattr);
		}
	}
}

static void add_overlay_loose_edge(
        MeshRenderData *rdata, VertexBuffer *vbo_pos, VertexBuffer *vbo_nor, VertexBuffer *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int data_id,
        const int edge_vert_idx[2], const int e, const int base_vert_idx)
{
	if (vbo_pos) {
		for (int i = 0; i < 2; ++i) {
			const float *pos = mesh_render_data_vert_co(rdata, edge_vert_idx[i]);
			VertexBuffer_set_attrib(vbo_pos, pos_id, base_vert_idx + i, pos);
		}
	}

	if (vbo_nor) {
		for (int i = 0; i < 2; ++i) {
			short *nor = mesh_render_data_vert_nor(rdata, edge_vert_idx[i]);
#if USE_10_10_10
			PackedNormal vnor = convert_i10_s3(nor);
			VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx + i, &vnor);
#else
			VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx + i, &nor);
#endif
		}
	}

	if (vbo_data) {
		EdgeDrawAttr *eattr = mesh_render_data_edge_flag(rdata, e);
		for (int i = 0; i < 2; ++i) {
			eattr->v_flag = mesh_render_data_vertex_flag(rdata, edge_vert_idx[i]);
			VertexBuffer_set_attrib(vbo_data, data_id, base_vert_idx + i, eattr);
		}
	}
}

static void add_overlay_loose_vert(
        MeshRenderData *rdata, VertexBuffer *vbo_pos, VertexBuffer *vbo_nor, VertexBuffer *vbo_data,
        const unsigned int pos_id, const unsigned int vnor_id, const unsigned int data_id,
        const int v, const int base_vert_idx)
{
	if (vbo_pos) {
		const float *pos = mesh_render_data_vert_co(rdata, v);
		VertexBuffer_set_attrib(vbo_pos, pos_id, base_vert_idx, pos);
	}

	if (vbo_nor) {
		short *nor = mesh_render_data_vert_nor(rdata, v);
#if USE_10_10_10
		PackedNormal vnor = convert_i10_s3(nor);
		VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx, &vnor);
#else
		VertexBuffer_set_attrib(vbo_nor, vnor_id, base_vert_idx, nor);
#endif
	}

	if (vbo_data) {
		unsigned char vflag[4] = {0, 0, 0, 0};
		vflag[0] = mesh_render_data_vertex_flag(rdata, v);
		VertexBuffer_set_attrib(vbo_data, data_id, base_vert_idx, vflag);
	}
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Mesh Batch Cache
 * \{ */

typedef struct MeshBatchCache {
	VertexBuffer *pos_in_order;
	VertexBuffer *nor_in_order;
	ElementList *edges_in_order;
	ElementList *triangles_in_order;

	Batch *all_verts;
	Batch *all_edges;
	Batch *all_triangles;

	VertexBuffer *pos_with_normals;
	VertexBuffer *tri_aligned_weights;
	VertexBuffer *tri_aligned_vert_colors;
	VertexBuffer *tri_aligned_select_id;
	VertexBuffer *edge_pos_with_sel;
	VertexBuffer *tri_pos_with_sel;
	VertexBuffer *pos_with_sel;
	Batch *triangles_with_normals;
	Batch *triangles_with_weights;
	Batch *triangles_with_vert_colors;
	Batch *triangles_with_select_id;
	Batch *points_with_normals;
	Batch *fancy_edges; /* owns its vertex buffer (not shared) */

	/* Maybe have shaded_triangles_data split into pos_nor and uv_tangent
	 * to minimise data transfer for skinned mesh. */
	VertexFormat shaded_triangles_format;
	VertexBuffer *shaded_triangles_data;
	ElementList **shaded_triangles_in_order;
	Batch **shaded_triangles;

	/* Edit Cage Mesh buffers */
	VertexBuffer *ed_tri_pos;
	VertexBuffer *ed_tri_nor; /* LoopNor, VertNor */
	VertexBuffer *ed_tri_data;

	VertexBuffer *ed_ledge_pos;
	VertexBuffer *ed_ledge_nor; /* VertNor */
	VertexBuffer *ed_ledge_data;

	VertexBuffer *ed_lvert_pos;
	VertexBuffer *ed_lvert_nor; /* VertNor */
	VertexBuffer *ed_lvert_data;

	VertexBuffer *ed_fcenter_pos;
	VertexBuffer *ed_fcenter_nor;

	Batch *overlay_triangles;
	Batch *overlay_triangles_nor; /* PRIM_POINTS */
	Batch *overlay_loose_edges;
	Batch *overlay_loose_edges_nor; /* PRIM_POINTS */
	Batch *overlay_loose_verts;
	Batch *overlay_facedots;

	Batch *overlay_paint_edges;
	Batch *overlay_weight_faces;
	Batch *overlay_weight_verts;

	/* settings to determine if cache is invalid */
	bool is_dirty;
	bool is_paint_dirty;
	int edge_len;
	int tri_len;
	int poly_len;
	int vert_len;
	int mat_len;
	bool is_editmode;
} MeshBatchCache;

/* Batch cache management. */

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

	if (cache->is_paint_dirty) {
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
	cache->is_paint_dirty = false;
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
			VERTEXBUFFER_DISCARD_SAFE(cache->ed_tri_data);
			VERTEXBUFFER_DISCARD_SAFE(cache->ed_ledge_data);
			VERTEXBUFFER_DISCARD_SAFE(cache->ed_lvert_data);
			VERTEXBUFFER_DISCARD_SAFE(cache->ed_fcenter_nor); /* Contains select flag */
			BATCH_DISCARD_SAFE(cache->overlay_triangles);
			BATCH_DISCARD_SAFE(cache->overlay_loose_verts);
			BATCH_DISCARD_SAFE(cache->overlay_loose_edges);

			BATCH_DISCARD_ALL_SAFE(cache->overlay_facedots);
			break;
		case BKE_MESH_BATCH_DIRTY_PAINT:
			cache->is_paint_dirty = true;
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

	BATCH_DISCARD_SAFE(cache->all_verts);
	BATCH_DISCARD_SAFE(cache->all_edges);
	BATCH_DISCARD_SAFE(cache->all_triangles);

	VERTEXBUFFER_DISCARD_SAFE(cache->pos_in_order);
	ELEMENTLIST_DISCARD_SAFE(cache->edges_in_order);
	ELEMENTLIST_DISCARD_SAFE(cache->triangles_in_order);

	VERTEXBUFFER_DISCARD_SAFE(cache->ed_tri_pos);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_tri_nor);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_tri_data);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_ledge_pos);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_ledge_nor);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_ledge_data);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_lvert_pos);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_lvert_nor);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_lvert_data);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_fcenter_pos);
	VERTEXBUFFER_DISCARD_SAFE(cache->ed_fcenter_nor);
	BATCH_DISCARD_SAFE(cache->overlay_triangles);
	BATCH_DISCARD_SAFE(cache->overlay_triangles_nor);
	BATCH_DISCARD_SAFE(cache->overlay_loose_verts);
	BATCH_DISCARD_SAFE(cache->overlay_loose_edges);
	BATCH_DISCARD_SAFE(cache->overlay_loose_edges_nor);

	BATCH_DISCARD_ALL_SAFE(cache->overlay_facedots);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_paint_edges);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_weight_faces);
	BATCH_DISCARD_ALL_SAFE(cache->overlay_weight_verts);

	BATCH_DISCARD_SAFE(cache->triangles_with_normals);
	BATCH_DISCARD_SAFE(cache->points_with_normals);
	VERTEXBUFFER_DISCARD_SAFE(cache->pos_with_normals);
	VERTEXBUFFER_DISCARD_SAFE(cache->tri_aligned_vert_colors);
	VERTEXBUFFER_DISCARD_SAFE(cache->tri_aligned_weights);
	BATCH_DISCARD_SAFE(cache->triangles_with_weights);
	BATCH_DISCARD_SAFE(cache->triangles_with_vert_colors);
	VERTEXBUFFER_DISCARD_SAFE(cache->tri_aligned_select_id);
	BATCH_DISCARD_SAFE(cache->triangles_with_select_id);

	BATCH_DISCARD_ALL_SAFE(cache->fancy_edges);

	VERTEXBUFFER_DISCARD_SAFE(cache->shaded_triangles_data);
	if (cache->shaded_triangles_in_order) {
		for (int i = 0; i < cache->mat_len; ++i) {
			ELEMENTLIST_DISCARD_SAFE(cache->shaded_triangles_in_order[i]);
		}
	}
	if (cache->shaded_triangles) {
		for (int i = 0; i < cache->mat_len; ++i) {
			BATCH_DISCARD_SAFE(cache->shaded_triangles[i]);
		}
	}

	MEM_SAFE_FREE(cache->shaded_triangles_in_order);
	MEM_SAFE_FREE(cache->shaded_triangles);
}

void DRW_mesh_batch_cache_free(Mesh *me)
{
	mesh_batch_cache_clear(me);
	MEM_SAFE_FREE(me->batch_cache);
}

/* Batch cache usage. */

#define USE_COMP_MESH_DATA
static VertexBuffer *mesh_batch_cache_get_tri_shading_data(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (cache->shaded_triangles_data == NULL) {
		unsigned int vidx = 0;
		const char *attrib_name;

		if (rdata->uv_len + rdata->vcol_len == 0) {
			return NULL;
		}

		VertexFormat *format = &cache->shaded_triangles_format;

		VertexFormat_clear(format);

		/* initialize vertex format */
		unsigned int *uv_id = MEM_mallocN(sizeof(*uv_id) * rdata->uv_len, "UV attrib format");
		unsigned int *vcol_id = MEM_mallocN(sizeof(*vcol_id) * rdata->vcol_len, "Vcol attrib format");
		unsigned int *tangent_id = MEM_mallocN(sizeof(*tangent_id) * rdata->uv_len, "Tangent attrib format");

		for (int i = 0; i < rdata->uv_len; i++) {
			/* UV */
			attrib_name = mesh_render_data_uv_layer_uuid_get(rdata, i);
#if defined(USE_COMP_MESH_DATA) && 0 /* these are clamped. Maybe use them as an option in the future */
			uv_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_I16, 2, NORMALIZE_INT_TO_FLOAT);
#else
			uv_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_F32, 2, KEEP_FLOAT);
#endif

			/* Auto Name */
			attrib_name = mesh_render_data_uv_auto_layer_uuid_get(rdata, i);
			VertexFormat_add_alias(format, attrib_name);

			if (i == rdata->uv_active) {
				VertexFormat_add_alias(format, "u");
			}

			/* Tangent */
			attrib_name = mesh_render_data_tangent_layer_uuid_get(rdata, i);
#ifdef USE_COMP_MESH_DATA
#  if USE_10_10_10 && 0 /* Tangents need more precision than this */
			tangent_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
#  else
			tangent_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
#  endif
#else
			tangent_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_F32, 3, KEEP_FLOAT);
#endif

			if (i == rdata->uv_active) {
				VertexFormat_add_alias(format, "t");
			}
		}

		for (int i = 0; i < rdata->vcol_len; i++) {
			attrib_name = mesh_render_data_vcol_layer_uuid_get(rdata, i);
			vcol_id[i] = VertexFormat_add_attrib(format, attrib_name, COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

			/* Auto layer */
			if (rdata->auto_vcol[i]) {
				attrib_name = mesh_render_data_vcol_auto_layer_uuid_get(rdata, i);
				VertexFormat_add_alias(format, attrib_name);
			}

			if (i == rdata->vcol_active) {
				VertexFormat_add_alias(format, "c");
			}
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->shaded_triangles_data = VertexBuffer_create_with_format(format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		/* TODO deduplicate all verts and make use of ElementList in mesh_batch_cache_get_shaded_triangles_in_order. */
		for (int i = 0; i < tri_len; i++) {
			float *tri_uvs[3], *tri_tans[3];
			unsigned char *tri_cols[3];

			if (rdata->edit_bmesh == NULL ||
			    BM_elem_flag_test((rdata->edit_bmesh->looptris[i])[0]->f, BM_ELEM_HIDDEN) == 0)
			{
				/* UVs & TANGENTs */
				for (int j = 0; j < rdata->uv_len; j++) {
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
					VertexBuffer_set_attrib(vbo, uv_id[j], vidx + 0, s_uvs[0]);
					VertexBuffer_set_attrib(vbo, uv_id[j], vidx + 1, s_uvs[1]);
					VertexBuffer_set_attrib(vbo, uv_id[j], vidx + 2, s_uvs[2]);

					/* Tangent */
					mesh_render_data_looptri_tans_get(rdata, i, j, &tri_tans);
#ifdef USE_COMP_MESH_DATA
#  if USE_10_10_10 && 0 /* Tangents need more precision than this */
					PackedNormal s_tan_pack[3] = {
					    convert_i10_v3(tri_tans[0]),
					    convert_i10_v3(tri_tans[1]),
					    convert_i10_v3(tri_tans[2])
					};
					PackedNormal *s_tan[3] = { &s_tan_pack[0], &s_tan_pack[1], &s_tan_pack[2] };
#  else
					short s_tan[3][3];
					normal_float_to_short_v3(s_tan[0], tri_tans[0]);
					normal_float_to_short_v3(s_tan[1], tri_tans[1]);
					normal_float_to_short_v3(s_tan[2], tri_tans[2]);
#  endif
#else
					float **s_tan = tri_tans;
#endif
					VertexBuffer_set_attrib(vbo, tangent_id[j], vidx + 0, s_tan[0]);
					VertexBuffer_set_attrib(vbo, tangent_id[j], vidx + 1, s_tan[1]);
					VertexBuffer_set_attrib(vbo, tangent_id[j], vidx + 2, s_tan[2]);
				}

				/* VCOLs */
				for (int j = 0; j < rdata->vcol_len; j++) {
					mesh_render_data_looptri_cols_get(rdata, i, j, &tri_cols);
					VertexBuffer_set_attrib(vbo, vcol_id[j], vidx + 0, tri_cols[0]);
					VertexBuffer_set_attrib(vbo, vcol_id[j], vidx + 1, tri_cols[1]);
					VertexBuffer_set_attrib(vbo, vcol_id[j], vidx + 2, tri_cols[2]);
				}

				vidx += 3;
			}
		}
		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}

		MEM_freeN(uv_id);
		MEM_freeN(vcol_id);
		MEM_freeN(tangent_id);
	}
	return cache->shaded_triangles_data;
}

static VertexBuffer *mesh_batch_cache_get_tri_pos_and_normals(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (cache->pos_with_normals == NULL) {
		unsigned int vidx = 0, nidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int pos_id, nor_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
#if USE_10_10_10
			nor_id = VertexFormat_add_attrib(&format, "nor", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
#else
			nor_id = VertexFormat_add_attrib(&format, "nor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
#endif
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->pos_with_normals = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			float *tri_vert_cos[3];
			short *tri_nor, *tri_vert_nors[3];
			bool is_smooth;

			if (mesh_render_data_looptri_cos_nors_smooth_get(
			        rdata, i, &tri_vert_cos, &tri_nor, &tri_vert_nors, &is_smooth))
			{
				if (is_smooth) {
#if USE_10_10_10
					PackedNormal snor_pack[3] = {
						convert_i10_s3(tri_vert_nors[0]),
						convert_i10_s3(tri_vert_nors[1]),
						convert_i10_s3(tri_vert_nors[2])
					};
					PackedNormal *snor[3] = { &snor_pack[0], &snor_pack[1], &snor_pack[2] };
#else
					short **snor = tri_vert_nors;
#endif
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor[0]);
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor[1]);
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor[2]);
				}
				else {
#if USE_10_10_10
					PackedNormal snor_pack = convert_i10_s3(tri_nor);
					PackedNormal *snor = &snor_pack;
#else
					short *snor = tri_nor;
#endif
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor);
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor);
					VertexBuffer_set_attrib(vbo, nor_id, nidx++, snor);
				}

				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[0]);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[1]);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[2]);
			}
		}
		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}
	return cache->pos_with_normals;
}

static VertexBuffer *mesh_batch_cache_get_tri_weights(
        MeshRenderData *rdata, MeshBatchCache *cache, int defgroup)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT));

	if (cache->tri_aligned_weights == NULL) {
		unsigned int cidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int col_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			col_id = VertexFormat_add_attrib(&format, "color", COMP_F32, 3, KEEP_FLOAT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->tri_aligned_weights = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			float *tri_vert_weights[3];

			if (mesh_render_data_looptri_weights_get(
			        rdata, i, defgroup, &tri_vert_weights))
			{
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_weights[0]);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_weights[1]);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_weights[2]);
			}
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_weights;
}

static VertexBuffer *mesh_batch_cache_get_tri_vert_colors(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPCOL));

	if (cache->tri_aligned_vert_colors == NULL) {
		unsigned int cidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int col_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			col_id = VertexFormat_add_attrib(&format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->tri_aligned_vert_colors = VertexBuffer_create_with_format(&format);

		const uint vbo_len_capacity = tri_len * 3;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			char *tri_vert_colors[3];

			if (mesh_render_data_looptri_vert_colors_get(
			        rdata, i, &tri_vert_colors))
			{
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_colors[0]);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_colors[1]);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, tri_vert_colors[2]);
			}
		}
		const uint vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_vert_colors;
}

static VertexBuffer *mesh_batch_cache_get_tri_select_id(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_hide)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (cache->tri_aligned_select_id == NULL) {
		unsigned int cidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int col_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			col_id = VertexFormat_add_attrib(&format, "color", COMP_I32, 1, KEEP_INT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->tri_aligned_select_id = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			int select_id;

			if (mesh_render_data_looptri_select_id_get(
			        rdata, i, use_hide, &select_id))
			{
				/* TODO, one elem per tri */
				VertexBuffer_set_attrib(vbo, col_id, cidx++, &select_id);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, &select_id);
				VertexBuffer_set_attrib(vbo, col_id, cidx++, &select_id);
			}
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->tri_aligned_select_id;
}

static VertexBuffer *mesh_batch_cache_get_vert_pos_and_nor_in_order(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->pos_in_order == NULL) {
		static VertexFormat format = { 0 };
		static unsigned pos_id, nor_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			nor_id = VertexFormat_add_attrib(&format, "nor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
		}

		VertexBuffer *vbo = cache->pos_in_order = VertexBuffer_create_with_format(&format);
		const int vbo_len_capacity = mesh_render_data_verts_len_get(rdata);
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);
		for (int i = 0; i < vbo_len_capacity; ++i) {
			VertexBuffer_set_attrib(vbo, pos_id, i, mesh_render_data_vert_co(rdata, i));
			VertexBuffer_set_attrib(vbo, nor_id, i, mesh_render_data_vert_nor(rdata, i));
		}
	}

	return cache->pos_in_order;
}

static VertexFormat *edit_mesh_overlay_pos_format(unsigned int *r_pos_id)
{
	static VertexFormat format_pos = { 0 };
	static unsigned pos_id;
	if (format_pos.attrib_ct == 0) {
		pos_id = VertexFormat_add_attrib(&format_pos, "pos", COMP_F32, 3, KEEP_FLOAT);
	}
	*r_pos_id = pos_id;
	return &format_pos;
}

static VertexFormat *edit_mesh_overlay_nor_format(unsigned int *r_vnor_id, unsigned int *r_lnor_id)
{
	static VertexFormat format_nor = { 0 };
	static VertexFormat format_nor_loop = { 0 };
	static unsigned vnor_id, vnor_loop_id, lnor_id;
	if (format_nor.attrib_ct == 0) {
#if USE_10_10_10
		vnor_id = VertexFormat_add_attrib(&format_nor, "vnor", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
		vnor_loop_id = VertexFormat_add_attrib(&format_nor_loop, "vnor", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
		lnor_id = VertexFormat_add_attrib(&format_nor_loop, "lnor", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
#else
		vnor_id = VertexFormat_add_attrib(&format_nor, "vnor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
		vnor_loop_id = VertexFormat_add_attrib(&format_nor_loop, "vnor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
		lnor_id = VertexFormat_add_attrib(&format_nor_loop, "lnor", COMP_I16, 3, NORMALIZE_INT_TO_FLOAT);
#endif
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

static VertexFormat *edit_mesh_overlay_data_format(unsigned int *r_data_id)
{
	static VertexFormat format_flag = { 0 };
	static unsigned data_id;
	if (format_flag.attrib_ct == 0) {
		data_id = VertexFormat_add_attrib(&format_flag, "data", COMP_U8, 4, KEEP_INT);
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
	VertexBuffer *vbo_pos = NULL;
	static unsigned pos_id;
	if (cache->ed_tri_pos == NULL) {
		vbo_pos = cache->ed_tri_pos = VertexBuffer_create_with_format(edit_mesh_overlay_pos_format(&pos_id));
		VertexBuffer_allocate_data(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	VertexBuffer *vbo_nor = NULL;
	static unsigned vnor_id, lnor_id;
	if (cache->ed_tri_nor == NULL) {
		vbo_nor = cache->ed_tri_nor = VertexBuffer_create_with_format(edit_mesh_overlay_nor_format(&vnor_id, &lnor_id));
		VertexBuffer_allocate_data(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	VertexBuffer *vbo_data = NULL;
	static unsigned data_id;
	if (cache->ed_tri_data == NULL) {
		vbo_data = cache->ed_tri_data = VertexBuffer_create_with_format(edit_mesh_overlay_data_format(&data_id));
		VertexBuffer_allocate_data(vbo_data, vbo_len_capacity);
	}

	for (int i = 0; i < tri_len; ++i) {
		int tri_vert_idx[3], tri_edge_idx[3];
		if (mesh_render_data_looptri_vert_edge_indices_get(rdata, i, tri_vert_idx, tri_edge_idx)) {
			add_overlay_tri(
			        rdata, vbo_pos, vbo_nor, vbo_data,
			        pos_id, vnor_id, lnor_id, data_id,
			        tri_vert_idx, tri_edge_idx, i, vbo_len_used);

			vbo_len_used += 3;
		}
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			VertexBuffer_resize_data(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			VertexBuffer_resize_data(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			VertexBuffer_resize_data(vbo_data, vbo_len_used);
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
	VertexBuffer *vbo_pos = NULL;
	static unsigned pos_id;
	if (cache->ed_ledge_pos == NULL) {
		vbo_pos = cache->ed_ledge_pos = VertexBuffer_create_with_format(edit_mesh_overlay_pos_format(&pos_id));
		VertexBuffer_allocate_data(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	VertexBuffer *vbo_nor = NULL;
	static unsigned vnor_id;
	if (cache->ed_ledge_nor == NULL) {
		vbo_nor = cache->ed_ledge_nor = VertexBuffer_create_with_format(edit_mesh_overlay_nor_format(&vnor_id, NULL));
		VertexBuffer_allocate_data(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	VertexBuffer *vbo_data = NULL;
	static unsigned data_id;
	if (cache->ed_ledge_data == NULL) {
		vbo_data = cache->ed_ledge_data = VertexBuffer_create_with_format(edit_mesh_overlay_data_format(&data_id));
		VertexBuffer_allocate_data(vbo_data, vbo_len_capacity);
	}

	for (int i = 0; i < ledge_len; ++i) {
		int vert_idx[2];
		bool ok = mesh_render_data_edge_verts_indices_get(rdata, rdata->loose_edges[i], vert_idx);
		BLI_assert(ok);  /* we don't add */
		add_overlay_loose_edge(
		        rdata, vbo_pos, vbo_nor, vbo_data,
		        pos_id, vnor_id, data_id,
		        vert_idx, i, vbo_len_used);
		vbo_len_used += 2;
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			VertexBuffer_resize_data(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			VertexBuffer_resize_data(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			VertexBuffer_resize_data(vbo_data, vbo_len_used);
		}
	}
}

static void mesh_batch_cache_create_overlay_lvert_buffers(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	const int lvert_len = mesh_render_data_loose_verts_len_get(rdata);

	const int vbo_len_capacity = lvert_len;
	int vbo_len_used = 0;

	/* Positions */
	VertexBuffer *vbo_pos = NULL;
	static unsigned pos_id;
	if (cache->ed_lvert_pos == NULL) {
		vbo_pos = cache->ed_lvert_pos = VertexBuffer_create_with_format(edit_mesh_overlay_pos_format(&pos_id));
		VertexBuffer_allocate_data(vbo_pos, vbo_len_capacity);
	}

	/* Normals */
	VertexBuffer *vbo_nor = NULL;
	static unsigned vnor_id;
	if (cache->ed_lvert_nor == NULL) {
		vbo_nor = cache->ed_lvert_nor = VertexBuffer_create_with_format(edit_mesh_overlay_nor_format(&vnor_id, NULL));
		VertexBuffer_allocate_data(vbo_nor, vbo_len_capacity);
	}

	/* Data */
	VertexBuffer *vbo_data = NULL;
	static unsigned data_id;
	if (cache->ed_lvert_data == NULL) {
		vbo_data = cache->ed_lvert_data = VertexBuffer_create_with_format(edit_mesh_overlay_data_format(&data_id));
		VertexBuffer_allocate_data(vbo_data, vbo_len_capacity);
	}

	for (int i = 0; i < lvert_len; ++i) {
		add_overlay_loose_vert(
		        rdata, vbo_pos, vbo_nor, vbo_data,
		        pos_id, vnor_id, data_id,
		        rdata->loose_verts[i], vbo_len_used);
		vbo_len_used += 1;
	}

	/* Finish */
	if (vbo_len_used != vbo_len_capacity) {
		if (vbo_pos != NULL) {
			VertexBuffer_resize_data(vbo_pos, vbo_len_used);
		}
		if (vbo_nor != NULL) {
			VertexBuffer_resize_data(vbo_nor, vbo_len_used);
		}
		if (vbo_data != NULL) {
			VertexBuffer_resize_data(vbo_data, vbo_len_used);
		}
	}
}

/* Position */
static VertexBuffer *mesh_batch_cache_get_edit_tri_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_pos == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_pos;
}

static VertexBuffer *mesh_batch_cache_get_edit_ledge_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_pos == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_pos;
}

static VertexBuffer *mesh_batch_cache_get_edit_lvert_pos(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_pos == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_pos;
}

/* Normal */
static VertexBuffer *mesh_batch_cache_get_edit_tri_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_nor == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_nor;
}

static VertexBuffer *mesh_batch_cache_get_edit_ledge_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_nor == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_nor;
}

static VertexBuffer *mesh_batch_cache_get_edit_lvert_nor(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_nor == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_nor;
}

/* Data */
static VertexBuffer *mesh_batch_cache_get_edit_tri_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_tri_data == NULL) {
		mesh_batch_cache_create_overlay_tri_buffers(rdata, cache);
	}

	return cache->ed_tri_data;
}

static VertexBuffer *mesh_batch_cache_get_edit_ledge_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_ledge_data == NULL) {
		mesh_batch_cache_create_overlay_ledge_buffers(rdata, cache);
	}

	return cache->ed_ledge_data;
}

static VertexBuffer *mesh_batch_cache_get_edit_lvert_data(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_lvert_data == NULL) {
		mesh_batch_cache_create_overlay_lvert_buffers(rdata, cache);
	}

	return cache->ed_lvert_data;
}

static ElementList *mesh_batch_cache_get_edges_in_order(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE));

	if (cache->edges_in_order == NULL) {
		printf("Caching edges in order...\n");
		const int vert_len = mesh_render_data_verts_len_get(rdata);
		const int edge_len = mesh_render_data_edges_len_get(rdata);

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, PRIM_LINES, edge_len, vert_len);
		for (int i = 0; i < edge_len; ++i) {
			int vert_idx[2];
			if (mesh_render_data_edge_verts_indices_get(rdata, i, vert_idx)) {
				add_line_vertices(&elb, vert_idx[0], vert_idx[1]);
			}
		}
		cache->edges_in_order = ElementList_build(&elb);
	}

	return cache->edges_in_order;
}

static ElementList *mesh_batch_cache_get_triangles_in_order(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI));

	if (cache->triangles_in_order == NULL) {
		const int vert_len = mesh_render_data_verts_len_get(rdata);
		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		ElementListBuilder elb;
		ElementListBuilder_init(&elb, PRIM_TRIANGLES, tri_len, vert_len);
		for (int i = 0; i < tri_len; ++i) {
			int tri_vert_idx[3];
			if (mesh_render_data_looptri_vert_indices_get(rdata, i, tri_vert_idx)) {
				add_triangle_vertices(&elb, tri_vert_idx[0], tri_vert_idx[1], tri_vert_idx[2]);
			}
		}
		cache->triangles_in_order = ElementList_build(&elb);
	}

	return cache->triangles_in_order;
}

static ElementList **mesh_batch_cache_get_shaded_triangles_in_order(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_POLY));

	if (cache->shaded_triangles_in_order == NULL) {
		const int tri_len = mesh_render_data_looptri_len_get(rdata);
		const int mat_len = mesh_render_data_mat_len_get(rdata);

		int *mat_tri_len = MEM_callocN(sizeof(*mat_tri_len) * mat_len, __func__);
		cache->shaded_triangles_in_order = MEM_callocN(sizeof(*cache->shaded_triangles) * mat_len, __func__);
		ElementListBuilder *elb = MEM_callocN(sizeof(*elb) * mat_len, __func__);

		for (int i = 0; i < tri_len; ++i) {
			short ma_id;
			if (mesh_render_data_looptri_mat_index_get(rdata, i, &ma_id)) {
				mat_tri_len[ma_id] += 1;
			}
		}

		/* Init ELBs. */
		for (int i = 0; i < mat_len; ++i) {
			ElementListBuilder_init(&elb[i], PRIM_TRIANGLES, mat_tri_len[i], tri_len * 3);
		}

		/* Populate ELBs. */
		unsigned int nidx = 0;
		for (int i = 0; i < tri_len; ++i) {
			short ma_id;

			/* TODO deduplicate verts see mesh_batch_cache_get_triangle_shading_data */
			if (mesh_render_data_looptri_mat_index_get(rdata, i, &ma_id)) {
				add_triangle_vertices(&elb[ma_id], nidx + 0, nidx + 1, nidx + 2);
				nidx += 3;
			}
		}

		/* Build ELBs. */
		for (int i = 0; i < mat_len; ++i) {
			cache->shaded_triangles_in_order[i] = ElementList_build(&elb[i]);
		}

		MEM_freeN(mat_tri_len);
		MEM_freeN(elb);
	}

	return cache->shaded_triangles_in_order;
}

static VertexBuffer *mesh_batch_cache_get_edge_pos_with_sel(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_wire, bool use_sel)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_POLY | MR_DATATYPE_LOOP));

	if (!cache->edge_pos_with_sel) {
		unsigned int vidx = 0, cidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int pos_id, sel_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			sel_id = VertexFormat_add_attrib(&format, "select", COMP_U8, 1, KEEP_INT);
		}

		const int edge_len = mesh_render_data_edges_len_get(rdata);

		VertexBuffer *vbo = cache->edge_pos_with_sel = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = edge_len * 2;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < edge_len; i++) {
			static float edge_vert_cos[2][3];
			static int edge_vert_sel;

			if (mesh_render_data_edge_cos_sel_get(
			        rdata, i, edge_vert_cos, &edge_vert_sel, use_wire, use_sel))
			{
				VertexBuffer_set_attrib(vbo, sel_id, cidx++, &edge_vert_sel);
				VertexBuffer_set_attrib(vbo, sel_id, cidx++, &edge_vert_sel);

				VertexBuffer_set_attrib(vbo, pos_id, vidx++, edge_vert_cos[0]);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, edge_vert_cos[1]);
			}
		}

		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->edge_pos_with_sel;
}

static VertexBuffer *mesh_batch_cache_get_tri_pos_with_sel(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI));

	if (cache->tri_pos_with_sel == NULL) {
		unsigned int vidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int pos_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
		}

		const int tri_len = mesh_render_data_looptri_len_get(rdata);

		VertexBuffer *vbo = cache->tri_pos_with_sel = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < tri_len; i++) {
			static float tri_vert_cos[3][3];

			if (mesh_render_data_tri_cos_sel_get(
			        rdata, i, tri_vert_cos))
			{
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[0]);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[1]);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, tri_vert_cos[2]);
			}
		}

		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->tri_pos_with_sel;
}

static VertexBuffer *mesh_batch_cache_get_vert_pos_with_sel(MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT));

	if (cache->pos_with_sel == NULL) {
		unsigned int vidx = 0, cidx = 0;

		static VertexFormat format = { 0 };
		static unsigned int pos_id, sel_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
			sel_id = VertexFormat_add_attrib(&format, "select", COMP_I8, 1, KEEP_INT);
		}

		const int vert_len = mesh_render_data_verts_len_get(rdata);

		VertexBuffer *vbo = cache->pos_with_sel = VertexBuffer_create_with_format(&format);

		const int vbo_len_capacity = vert_len;
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);

		for (int i = 0; i < vert_len; i++) {
			static float vert_co[3];
			static int vert_sel;

			if (mesh_render_data_vert_cos_sel_get(
			        rdata, i, vert_co, &vert_sel))
			{
				VertexBuffer_set_attrib(vbo, sel_id, cidx++, &vert_sel);
				VertexBuffer_set_attrib(vbo, pos_id, vidx++, vert_co);
			}
		}

		vbo_len_used = vidx;

		if (vbo_len_capacity != vbo_len_used) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}
	}

	return cache->pos_with_sel;
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Public API
 * \{ */

Batch *DRW_mesh_batch_cache_get_all_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_edges == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_EDGE;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->all_edges = Batch_create(
		         PRIM_LINES, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache),
		         mesh_batch_cache_get_edges_in_order(rdata, cache));

		mesh_render_data_free(rdata);
	}

	return cache->all_edges;
}

Batch *DRW_mesh_batch_cache_get_all_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_triangles == NULL) {
		/* create batch from DM */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->all_triangles = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache),
		        mesh_batch_cache_get_triangles_in_order(rdata, cache));

		mesh_render_data_free(rdata);
	}

	return cache->all_triangles;
}

Batch *DRW_mesh_batch_cache_get_triangles_with_normals(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_normals == NULL) {
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_normals = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_tri_pos_and_normals(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_normals;
}

Batch *DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(Mesh *me, int defgroup)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_weights == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_DVERT;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_weights = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_tri_weights(rdata, cache, defgroup), NULL);

		VertexBuffer *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		Batch_add_VertexBuffer(cache->triangles_with_weights, vbo);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_weights;
}

Batch *DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_vert_colors == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPCOL;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_vert_colors = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_tri_vert_colors(rdata, cache), NULL);

		VertexBuffer *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		Batch_add_VertexBuffer(cache->triangles_with_vert_colors, vbo);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_vert_colors;
}


struct Batch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me, bool use_hide)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_select_id == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->triangles_with_select_id = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_tri_select_id(rdata, cache, use_hide), NULL);

		VertexBuffer *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		Batch_add_VertexBuffer(cache->triangles_with_select_id, vbo);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_select_id;
}

Batch *DRW_mesh_batch_cache_get_points_with_normals(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->points_with_normals == NULL) {
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->points_with_normals = Batch_create(
		        PRIM_POINTS, mesh_batch_cache_get_tri_pos_and_normals(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->points_with_normals;
}

Batch *DRW_mesh_batch_cache_get_all_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->all_verts == NULL) {
		/* create batch from DM */
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT);

		cache->all_verts = Batch_create(
		        PRIM_POINTS, mesh_batch_cache_get_vert_pos_and_nor_in_order(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->all_verts;
}

Batch *DRW_mesh_batch_cache_get_fancy_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->fancy_edges == NULL) {
		/* create batch from DM */
		static VertexFormat format = { 0 };
		static unsigned int pos_id, n1_id, n2_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);

#if USE_10_10_10 /* takes 1/3 the space */
			n1_id = VertexFormat_add_attrib(&format, "N1", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
			n2_id = VertexFormat_add_attrib(&format, "N2", COMP_I10, 3, NORMALIZE_INT_TO_FLOAT);
#else
			n1_id = VertexFormat_add_attrib(&format, "N1", COMP_F32, 3, KEEP_FLOAT);
			n2_id = VertexFormat_add_attrib(&format, "N2", COMP_F32, 3, KEEP_FLOAT);
#endif
		}
		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);

		MeshRenderData *rdata = mesh_render_data_create(
		        me, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_POLY);

		const int edge_len = mesh_render_data_edges_len_get(rdata);

		const int vbo_len_capacity = edge_len * 2; /* these are PRIM_LINE verts, not mesh verts */
		int vbo_len_used = 0;
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);
		for (int i = 0; i < edge_len; ++i) {
			float *vcos1, *vcos2;
			float *pnor1 = NULL, *pnor2 = NULL;
			bool is_manifold;

			if (mesh_render_data_edge_vcos_manifold_pnors(rdata, i, &vcos1, &vcos2, &pnor1, &pnor2, &is_manifold)) {

#if USE_10_10_10
				PackedNormal n1value = { .x = 0, .y = 0, .z = +511 };
				PackedNormal n2value = { .x = 0, .y = 0, .z = -511 };

				if (is_manifold) {
					n1value = convert_i10_v3(pnor1);
					n2value = convert_i10_v3(pnor2);
				}

				const PackedNormal *n1 = &n1value;
				const PackedNormal *n2 = &n2value;
#else
				const float dummy1[3] = { 0.0f, 0.0f, +1.0f };
				const float dummy2[3] = { 0.0f, 0.0f, -1.0f };

				const float *n1 = (is_manifold) ? pnor1 : dummy1;
				const float *n2 = (is_manifold) ? pnor2 : dummy2;
#endif

				VertexBuffer_set_attrib(vbo, pos_id, 2 * i, vcos1);
				VertexBuffer_set_attrib(vbo, n1_id, 2 * i, n1);
				VertexBuffer_set_attrib(vbo, n2_id, 2 * i, n2);

				VertexBuffer_set_attrib(vbo, pos_id, 2 * i + 1, vcos2);
				VertexBuffer_set_attrib(vbo, n1_id, 2 * i + 1, n1);
				VertexBuffer_set_attrib(vbo, n2_id, 2 * i + 1, n2);

				vbo_len_used += 2;
			}
		}
		if (vbo_len_used != vbo_len_capacity) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}

		cache->fancy_edges = Batch_create(PRIM_LINES, vbo, NULL);

		mesh_render_data_free(rdata);
	}

	return cache->fancy_edges;
}

static void mesh_batch_cache_create_overlay_batches(Mesh *me)
{
	/* Since MR_DATATYPE_OVERLAY is slow to generate, generate them all at once */
	int options = MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI | MR_DATATYPE_OVERLAY;

	MeshBatchCache *cache = mesh_batch_cache_get(me);
	MeshRenderData *rdata = mesh_render_data_create(me, options);

	if (cache->overlay_triangles == NULL) {
		cache->overlay_triangles = Batch_create(PRIM_TRIANGLES, mesh_batch_cache_get_edit_tri_pos(rdata, cache), NULL);
		Batch_add_VertexBuffer(cache->overlay_triangles, mesh_batch_cache_get_edit_tri_nor(rdata, cache));
		Batch_add_VertexBuffer(cache->overlay_triangles, mesh_batch_cache_get_edit_tri_data(rdata, cache));
	}

	if (cache->overlay_loose_edges == NULL) {
		cache->overlay_loose_edges = Batch_create(PRIM_LINES, mesh_batch_cache_get_edit_ledge_pos(rdata, cache), NULL);
		Batch_add_VertexBuffer(cache->overlay_loose_edges, mesh_batch_cache_get_edit_ledge_nor(rdata, cache));
		Batch_add_VertexBuffer(cache->overlay_loose_edges, mesh_batch_cache_get_edit_ledge_data(rdata, cache));
	}

	if (cache->overlay_loose_verts == NULL) {
		cache->overlay_loose_verts = Batch_create(PRIM_POINTS, mesh_batch_cache_get_edit_lvert_pos(rdata, cache), NULL);
		Batch_add_VertexBuffer(cache->overlay_loose_verts, mesh_batch_cache_get_edit_lvert_nor(rdata, cache));
		Batch_add_VertexBuffer(cache->overlay_loose_verts, mesh_batch_cache_get_edit_lvert_data(rdata, cache));
	}

	if (cache->overlay_triangles_nor == NULL) {
		cache->overlay_triangles_nor = Batch_create(PRIM_POINTS, mesh_batch_cache_get_edit_tri_pos(rdata, cache), NULL);
		Batch_add_VertexBuffer(cache->overlay_triangles_nor, mesh_batch_cache_get_edit_tri_nor(rdata, cache));
	}

	if (cache->overlay_loose_edges_nor == NULL) {
		cache->overlay_loose_edges_nor = Batch_create(PRIM_POINTS, mesh_batch_cache_get_edit_ledge_pos(rdata, cache), NULL);
		Batch_add_VertexBuffer(cache->overlay_loose_edges_nor, mesh_batch_cache_get_edit_ledge_nor(rdata, cache));
	}

	mesh_render_data_free(rdata);
}

Batch *DRW_mesh_batch_cache_get_overlay_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_triangles == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_triangles;
}

Batch *DRW_mesh_batch_cache_get_overlay_loose_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_edges == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_edges;
}

Batch *DRW_mesh_batch_cache_get_overlay_loose_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_verts == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_verts;
}

Batch *DRW_mesh_batch_cache_get_overlay_triangles_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_triangles_nor == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_triangles_nor;
}

Batch *DRW_mesh_batch_cache_get_overlay_loose_edges_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_loose_edges_nor == NULL) {
		mesh_batch_cache_create_overlay_batches(me);
	}

	return cache->overlay_loose_edges_nor;
}

Batch *DRW_mesh_batch_cache_get_overlay_facedots(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_facedots == NULL) {
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY);

		static VertexFormat format = { 0 };
		static unsigned pos_id, data_id;
		if (format.attrib_ct == 0) {
			/* initialize vertex format */
			pos_id = VertexFormat_add_attrib(&format, "pos", COMP_F32, 3, KEEP_FLOAT);
#if USE_10_10_10
			data_id = VertexFormat_add_attrib(&format, "norAndFlag", COMP_I10, 4, NORMALIZE_INT_TO_FLOAT);
#else
			data_id = VertexFormat_add_attrib(&format, "norAndFlag", COMP_F32, 4, KEEP_FLOAT);
#endif
		}

		const int vbo_len_capacity = mesh_render_data_polys_len_get(rdata);
		int vidx = 0;

		VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
		VertexBuffer_allocate_data(vbo, vbo_len_capacity);
		for (int i = 0; i < vbo_len_capacity; ++i) {
			float pcenter[3], pnor[3];
			bool selected = false;

			if (mesh_render_data_pnors_pcenter_select_get(rdata, i, pnor, pcenter, &selected)) {

#if USE_10_10_10
				PackedNormal nor = { .x = 0, .y = 0, .z = -511 };
				nor = convert_i10_v3(pnor);
				nor.w = selected ? 1 : 0;
				VertexBuffer_set_attrib(vbo, data_id, vidx, &nor);
#else
				float nor[4] = {pnor[0], pnor[1], pnor[2], selected ? 1 : 0};
				VertexBuffer_set_attrib(vbo, data_id, vidx, nor);
#endif

				VertexBuffer_set_attrib(vbo, pos_id, vidx, pcenter);

				vidx += 1;

			}
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			VertexBuffer_resize_data(vbo, vbo_len_used);
		}

		cache->overlay_facedots = Batch_create(PRIM_POINTS, vbo, NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_facedots;
}

Batch **DRW_mesh_batch_cache_get_surface_shaded(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->shaded_triangles == NULL) {
		/* create batch from DM */
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI |
		        MR_DATATYPE_POLY | MR_DATATYPE_SHADING;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		const int mat_len = mesh_render_data_mat_len_get(rdata);

		cache->shaded_triangles = MEM_callocN(sizeof(*cache->shaded_triangles) * mat_len, __func__);

		ElementList **el = mesh_batch_cache_get_shaded_triangles_in_order(rdata, cache);

		VertexBuffer *vbo = mesh_batch_cache_get_tri_pos_and_normals(rdata, cache);
		for (int i = 0; i < mat_len; ++i) {
			cache->shaded_triangles[i] = Batch_create(
			        PRIM_TRIANGLES, vbo, el[i]);
			VertexBuffer *vbo_shading = mesh_batch_cache_get_tri_shading_data(rdata, cache);
			if (vbo_shading) {
				Batch_add_VertexBuffer(cache->shaded_triangles[i], vbo_shading);
			}
		}


		mesh_render_data_free(rdata);
	}

	return cache->shaded_triangles;
}

Batch *DRW_mesh_batch_cache_get_weight_overlay_edges(Mesh *me, bool use_wire, bool use_sel)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_paint_edges == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_POLY | MR_DATATYPE_LOOP;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->overlay_paint_edges = Batch_create(
		        PRIM_LINES, mesh_batch_cache_get_edge_pos_with_sel(rdata, cache, use_wire, use_sel), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_paint_edges;
}

Batch *DRW_mesh_batch_cache_get_weight_overlay_faces(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_weight_faces == NULL) {
		/* create batch from Mesh */
		const int datatype = MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		cache->overlay_weight_faces = Batch_create(
		        PRIM_TRIANGLES, mesh_batch_cache_get_tri_pos_with_sel(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_weight_faces;
}

Batch *DRW_mesh_batch_cache_get_weight_overlay_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->overlay_weight_verts == NULL) {
		/* create batch from Mesh */
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT);

		cache->overlay_weight_verts = Batch_create(
		        PRIM_POINTS, mesh_batch_cache_get_vert_pos_with_sel(rdata, cache), NULL);

		mesh_render_data_free(rdata);
	}

	return cache->overlay_weight_verts;
}

/** \} */

#undef MESH_RENDER_FUNCTION
