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

#include "BLI_buffer.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_math_bits.h"
#include "BLI_math_color.h"
#include "BLI_string.h"
#include "BLI_alloca.h"
#include "BLI_edgehash.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_colorband.h"
#include "BKE_cdderivedmesh.h"

#include "DEG_depsgraph_query.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_draw.h"
#include "GPU_material.h"

#include "DRW_render.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_impl.h"  /* own include */


static void mesh_batch_cache_clear(Mesh *me);

/* Vertex Group Selection and display options */
typedef struct DRW_MeshWeightState {
	int defgroup_active;
	int defgroup_len;

	short flags;
	char alert_mode;

	/* Set of all selected bones for Multipaint. */
	bool *defgroup_sel; /* [defgroup_len] */
	int   defgroup_sel_count;
} DRW_MeshWeightState;

/* DRW_MeshWeightState.flags */
enum {
	DRW_MESH_WEIGHT_STATE_MULTIPAINT          = (1 << 0),
	DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE      = (1 << 1),
};

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

typedef struct EdgeAdjacentVerts {
	int vert_index[2]; /* -1 if none */
} EdgeAdjacentVerts;

typedef struct EdgeDrawAttr {
	uchar v_flag;
	uchar e_flag;
	uchar crease;
	uchar bweight;
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

	/* Support for mapped mesh data. */
	struct {
		/* Must be set if we want to get mapped data. */
		bool use;
		bool supported;

		Mesh *me_cage;

		int vert_len;
		int edge_len;
		int tri_len;
		int loop_len;
		int poly_len;

		int *loose_verts;
		int  loose_vert_len;

		int *loose_edges;
		int  loose_edge_len;

		/* origindex layers */
		int *v_origindex;
		int *e_origindex;
		int *l_origindex;
		int *p_origindex;
	} mapped;

	BMEditMesh *edit_bmesh;
	struct EditMeshData *edit_data;

	Mesh *me;

	MVert *mvert;
	const MEdge *medge;
	const MLoop *mloop;
	const MPoly *mpoly;
	float (*orco)[3];  /* vertex coordinates normalized to bounding box */
	bool is_orco_allocated;
	MDeformVert *dvert;
	MLoopUV *mloopuv;
	MLoopCol *mloopcol;
	float (*loop_normals)[3];

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
#ifdef WITH_FREESTYLE
			int freestyle_edge;
			int freestyle_face;
#endif
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
			short tangent_mask;
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
	float *vert_weight;
	char (*vert_color)[3];
	GPUPackedNormal *poly_normals_pack;
	GPUPackedNormal *vert_normals_pack;
	bool *edge_select_bool;
	bool *edge_visible_bool;
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
	MR_DATATYPE_LOOSE_VERT = 1 << 10,
	MR_DATATYPE_LOOSE_EDGE = 1 << 11,
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

/* Return true is all layers in _b_ are inside _a_. */
static bool mesh_cd_layers_type_overlap(
        const uchar av[CD_NUMTYPES], const ushort al[CD_NUMTYPES],
        const uchar bv[CD_NUMTYPES], const ushort bl[CD_NUMTYPES])
{
	for (int i = 0; i < CD_NUMTYPES; ++i) {
		if ((av[i] & bv[i]) != bv[i]) {
			return false;
		}
		if ((al[i] & bl[i]) != bl[i]) {
			return false;
		}
	}
	return true;
}

static void mesh_cd_layers_type_merge(
        uchar av[CD_NUMTYPES], ushort al[CD_NUMTYPES],
        uchar bv[CD_NUMTYPES], ushort bl[CD_NUMTYPES])
{
	for (int i = 0; i < CD_NUMTYPES; ++i) {
		av[i] |= bv[i];
		al[i] |= bl[i];
	}
}

static void mesh_cd_calc_active_uv_layer(
        const Mesh *me, ushort cd_lused[CD_NUMTYPES])
{
	const CustomData *cd_ldata = (me->edit_btmesh) ? &me->edit_btmesh->bm->ldata : &me->ldata;

	int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
	if (layer != -1) {
		cd_lused[CD_MLOOPUV] |= (1 << layer);
	}
}

static void mesh_cd_calc_active_vcol_layer(
        const Mesh *me, ushort cd_lused[CD_NUMTYPES])
{
	const CustomData *cd_ldata = (me->edit_btmesh) ? &me->edit_btmesh->bm->ldata : &me->ldata;

	int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
	if (layer != -1) {
		cd_lused[CD_MLOOPCOL] |= (1 << layer);
	}
}

static void mesh_cd_calc_used_gpu_layers(
        const Mesh *me, uchar cd_vused[CD_NUMTYPES], ushort cd_lused[CD_NUMTYPES],
        struct GPUMaterial **gpumat_array, int gpumat_array_len)
{
	const CustomData *cd_ldata = (me->edit_btmesh) ? &me->edit_btmesh->bm->ldata : &me->ldata;

	/* See: DM_vertex_attributes_from_gpu for similar logic */
	GPUVertexAttribs gattribs = {{{0}}};

	for (int i = 0; i < gpumat_array_len; i++) {
		GPUMaterial *gpumat = gpumat_array[i];
		if (gpumat) {
			GPU_material_vertex_attributes(gpumat, &gattribs);
			for (int j = 0; j < gattribs.totlayer; j++) {
				const char *name = gattribs.layer[j].name;
				int type = gattribs.layer[j].type;
				int layer = -1;

				if (type == CD_AUTO_FROM_NAME) {
					/* We need to deduct what exact layer is used.
					 *
					 * We do it based on the specified name.
					 */
					if (name[0] != '\0') {
						layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name);
						type = CD_MTFACE;

						if (layer == -1) {
							layer = CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name);
							type = CD_MCOL;
						}
#if 0					/* Tangents are always from UV's - this will never happen. */
						if (layer == -1) {
							layer = CustomData_get_named_layer(cd_ldata, CD_TANGENT, name);
							type = CD_TANGENT;
						}
#endif
						if (layer == -1) {
							continue;
						}
					}
					else {
						/* Fall back to the UV layer, which matches old behavior. */
						type = CD_MTFACE;
					}
				}

				switch (type) {
					case CD_MTFACE:
					{
						if (layer == -1) {
							layer = (name[0] != '\0') ?
							        CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
							        CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
						}
						if (layer != -1) {
							cd_lused[CD_MLOOPUV] |= (1 << layer);
						}
						break;
					}
					case CD_TANGENT:
					{
						if (layer == -1) {
							layer = (name[0] != '\0') ?
							        CustomData_get_named_layer(cd_ldata, CD_MLOOPUV, name) :
							        CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);

							/* Only fallback to orco (below) when we have no UV layers, see: T56545 */
							if (layer == -1 && name[0] != '\0') {
								layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
							}
						}
						if (layer != -1) {
							cd_lused[CD_TANGENT] |= (1 << layer);
						}
						else {
							/* no UV layers at all => requesting orco */
							cd_lused[CD_TANGENT] |= DM_TANGENT_MASK_ORCO;
							cd_vused[CD_ORCO] |= 1;
						}
						break;
					}
					case CD_MCOL:
					{
						if (layer == -1) {
							layer = (name[0] != '\0') ?
							        CustomData_get_named_layer(cd_ldata, CD_MLOOPCOL, name) :
							        CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
						}
						if (layer != -1) {
							cd_lused[CD_MLOOPCOL] |= (1 << layer);
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


static void mesh_render_calc_normals_loop_and_poly(const Mesh *me, const float split_angle, MeshRenderData *rdata)
{
	BLI_assert((me->flag & ME_AUTOSMOOTH) != 0);

	int totloop = me->totloop;
	int totpoly = me->totpoly;
	float (*loop_normals)[3] = MEM_mallocN(sizeof(*loop_normals) * totloop, __func__);
	float (*poly_normals)[3] = MEM_mallocN(sizeof(*poly_normals) * totpoly, __func__);
	short (*clnors)[2] = CustomData_get_layer(&me->ldata, CD_CUSTOMLOOPNORMAL);

	BKE_mesh_calc_normals_poly(
	        me->mvert, NULL, me->totvert,
	        me->mloop, me->mpoly, totloop, totpoly, poly_normals, false);

	BKE_mesh_normals_loop_split(
	        me->mvert, me->totvert, me->medge, me->totedge,
	        me->mloop, loop_normals, totloop, me->mpoly, poly_normals, totpoly,
	        true, split_angle, NULL, clnors, NULL);

	rdata->loop_len = totloop;
	rdata->poly_len = totpoly;
	rdata->loop_normals = loop_normals;
	rdata->poly_normals = poly_normals;
}

static void mesh_cd_extract_auto_layers_names_and_srgb(
        Mesh *me, const ushort cd_lused[CD_NUMTYPES],
        char **r_auto_layers_names, int **r_auto_layers_srgb, int *r_auto_layers_len)
{
	const CustomData *cd_ldata = (me->edit_btmesh) ? &me->edit_btmesh->bm->ldata : &me->ldata;

	int uv_len_used = count_bits_i(cd_lused[CD_MLOOPUV]);
	int vcol_len_used = count_bits_i(cd_lused[CD_MLOOPCOL]);
	int uv_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPUV);
	int vcol_len = CustomData_number_of_layers(cd_ldata, CD_MLOOPCOL);

	uint auto_names_len = 32 * (uv_len_used + vcol_len_used);
	uint auto_ofs = 0;
	/* Allocate max, resize later. */
	char *auto_names = MEM_callocN(sizeof(char) * auto_names_len, __func__);
	int *auto_is_srgb = MEM_callocN(sizeof(int) * (uv_len_used + vcol_len_used), __func__);

	for (int i = 0; i < uv_len; i++) {
		if ((cd_lused[CD_MLOOPUV] & (1 << i)) != 0) {
			const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
			uint hash = BLI_ghashutil_strhash_p(name);
			/* +1 to include '\0' terminator. */
			auto_ofs += 1 + BLI_snprintf_rlen(auto_names + auto_ofs, auto_names_len - auto_ofs, "ba%u", hash);
		}
	}

	uint auto_is_srgb_ofs = uv_len_used;
	for (int i = 0; i < vcol_len; i++) {
		if ((cd_lused[CD_MLOOPCOL] & (1 << i)) != 0) {
			const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
			/* We only do vcols that are not overridden by a uv layer with same name. */
			if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, name) == -1) {
				uint hash = BLI_ghashutil_strhash_p(name);
				/* +1 to include '\0' terminator. */
				auto_ofs += 1 + BLI_snprintf_rlen(auto_names + auto_ofs, auto_names_len - auto_ofs, "ba%u", hash);
				auto_is_srgb[auto_is_srgb_ofs] = true;
				auto_is_srgb_ofs++;
			}
		}
	}

	auto_names = MEM_reallocN(auto_names, sizeof(char) * auto_ofs);
	auto_is_srgb = MEM_reallocN(auto_is_srgb, sizeof(int) * auto_is_srgb_ofs);

	*r_auto_layers_names = auto_names;
	*r_auto_layers_srgb = auto_is_srgb;
	*r_auto_layers_len = auto_is_srgb_ofs;
}

/**
 * TODO(campbell): 'gpumat_array' may include materials linked to the object.
 * While not default, object materials should be supported.
 * Although this only impacts the data that's generated, not the materials that display.
 */
static MeshRenderData *mesh_render_data_create_ex(
        Mesh *me, const int types, const uchar cd_vused[CD_NUMTYPES], const ushort cd_lused[CD_NUMTYPES])
{
	MeshRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
	rdata->types = types;
	rdata->mat_len = mesh_render_mat_len_get(me);

	CustomData_reset(&rdata->cd.output.ldata);

	const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;
	const float split_angle = is_auto_smooth ? me->smoothresh : (float)M_PI;

	if (me->edit_btmesh) {
		BMEditMesh *embm = me->edit_btmesh;
		BMesh *bm = embm->bm;

		rdata->edit_bmesh = embm;
		rdata->edit_data = me->runtime.edit_data;

		if (embm->mesh_eval_cage && (embm->mesh_eval_cage->runtime.is_original == false)) {
			Mesh *me_cage = embm->mesh_eval_cage;

			rdata->mapped.me_cage = me_cage;
			if (types & MR_DATATYPE_VERT) {
				rdata->mapped.vert_len = me_cage->totvert;
			}
			if (types & MR_DATATYPE_EDGE) {
				rdata->mapped.edge_len = me_cage->totedge;
			}
			if (types & MR_DATATYPE_LOOP) {
				rdata->mapped.loop_len = me_cage->totloop;
			}
			if (types & MR_DATATYPE_POLY) {
				rdata->mapped.poly_len = me_cage->totpoly;
			}
			if (types & MR_DATATYPE_LOOPTRI) {
				rdata->mapped.tri_len = poly_to_tri_count(me_cage->totpoly, me_cage->totloop);
			}

			rdata->mapped.v_origindex = CustomData_get_layer(&me_cage->vdata, CD_ORIGINDEX);
			rdata->mapped.e_origindex = CustomData_get_layer(&me_cage->edata, CD_ORIGINDEX);
			rdata->mapped.l_origindex = CustomData_get_layer(&me_cage->ldata, CD_ORIGINDEX);
			rdata->mapped.p_origindex = CustomData_get_layer(&me_cage->pdata, CD_ORIGINDEX);
			rdata->mapped.supported = (
			        rdata->mapped.v_origindex &&
			        rdata->mapped.e_origindex &&
			        rdata->mapped.p_origindex);
		}

		int bm_ensure_types = 0;
		if (types & MR_DATATYPE_VERT) {
			rdata->vert_len = bm->totvert;
			bm_ensure_types |= BM_VERT;
		}
		if (types & MR_DATATYPE_EDGE) {
			rdata->edge_len = bm->totedge;
			bm_ensure_types |= BM_EDGE;
		}
		if (types & MR_DATATYPE_LOOPTRI) {
			bm_ensure_types |= BM_LOOP;
		}
		if (types & MR_DATATYPE_LOOP) {
			int totloop = bm->totloop;
			if (is_auto_smooth) {
				rdata->loop_normals = MEM_mallocN(sizeof(*rdata->loop_normals) * totloop, __func__);
				int cd_loop_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
				BM_loops_calc_normal_vcos(
				        bm, NULL, NULL, NULL, true, split_angle, rdata->loop_normals, NULL, NULL,
				        cd_loop_clnors_offset, false);
			}
			rdata->loop_len = totloop;
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

#ifdef WITH_FREESTYLE
			rdata->cd.offset.freestyle_edge = CustomData_get_offset(&bm->edata, CD_FREESTYLE_EDGE);
			rdata->cd.offset.freestyle_face = CustomData_get_offset(&bm->pdata, CD_FREESTYLE_FACE);
#endif
		}
		if (types & (MR_DATATYPE_DVERT)) {
			bm_ensure_types |= BM_VERT;
		}
		if (rdata->edit_data != NULL) {
			bm_ensure_types |= BM_VERT;
		}

		BM_mesh_elem_index_ensure(bm, bm_ensure_types);
		BM_mesh_elem_table_ensure(bm, bm_ensure_types & ~BM_LOOP);

		if (types & MR_DATATYPE_LOOPTRI) {
			/* Edit mode ensures this is valid, no need to calculate. */
			BLI_assert((bm->totloop == 0) || (embm->looptris != NULL));
			int tottri = embm->tottri;
			MLoopTri *mlooptri = MEM_mallocN(sizeof(*rdata->mlooptri) * embm->tottri, __func__);
			for (int index = 0; index < tottri ; index ++ ) {
				BMLoop **bmtri = embm->looptris[index];
				MLoopTri *mtri = &mlooptri[index];
				mtri->tri[0] = BM_elem_index_get(bmtri[0]);
				mtri->tri[1] = BM_elem_index_get(bmtri[1]);
				mtri->tri[2] = BM_elem_index_get(bmtri[2]);
			}
			rdata->mlooptri = mlooptri;
			rdata->tri_len = tottri;
		}

		if (types & MR_DATATYPE_LOOSE_VERT) {
			BLI_assert(types & MR_DATATYPE_VERT);
			rdata->loose_vert_len = 0;

			{
				int *lverts = MEM_mallocN(rdata->vert_len * sizeof(int), __func__);
				BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
				for (int i = 0; i < bm->totvert; i++) {
					const BMVert *eve = BM_vert_at_index(bm, i);
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						/* Loose vert */
						if (eve->e == NULL || !bm_vert_has_visible_edge(eve)) {
							lverts[rdata->loose_vert_len++] = i;
						}
					}
				}
				rdata->loose_verts = MEM_reallocN(lverts, rdata->loose_vert_len * sizeof(int));
			}

			if (rdata->mapped.supported) {
				Mesh *me_cage = embm->mesh_eval_cage;
				rdata->mapped.loose_vert_len = 0;

				if (rdata->loose_vert_len) {
					int *lverts = MEM_mallocN(me_cage->totvert * sizeof(int), __func__);
					const int *v_origindex = rdata->mapped.v_origindex;
					for (int i = 0; i < me_cage->totvert; i++) {
						const int v_orig = v_origindex[i];
						if (v_orig != ORIGINDEX_NONE) {
							BMVert *eve = BM_vert_at_index(bm, v_orig);
							if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
								/* Loose vert */
								if (eve->e == NULL || !bm_vert_has_visible_edge(eve)) {
									lverts[rdata->mapped.loose_vert_len++] = i;
								}
							}
						}
					}
					rdata->mapped.loose_verts = MEM_reallocN(lverts, rdata->mapped.loose_vert_len * sizeof(int));
				}
			}
		}

		if (types & MR_DATATYPE_LOOSE_EDGE) {
			BLI_assert(types & MR_DATATYPE_EDGE);
			rdata->loose_edge_len = 0;

			{
				int *ledges = MEM_mallocN(rdata->edge_len * sizeof(int), __func__);
				BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
				for (int i = 0; i < bm->totedge; i++) {
					const BMEdge *eed = BM_edge_at_index(bm, i);
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						/* Loose edge */
						if (eed->l == NULL || !bm_edge_has_visible_face(eed)) {
							ledges[rdata->loose_edge_len++] = i;
						}
					}
				}
				rdata->loose_edges = MEM_reallocN(ledges, rdata->loose_edge_len * sizeof(int));
			}

			if (rdata->mapped.supported) {
				Mesh *me_cage = embm->mesh_eval_cage;
				rdata->mapped.loose_edge_len = 0;

				if (rdata->loose_edge_len) {
					int *ledges = MEM_mallocN(me_cage->totedge * sizeof(int), __func__);
					const int *e_origindex = rdata->mapped.e_origindex;
					for (int i = 0; i < me_cage->totedge; i++) {
						const int e_orig = e_origindex[i];
						if (e_orig != ORIGINDEX_NONE) {
							BMEdge *eed = BM_edge_at_index(bm, e_orig);
							if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
								/* Loose edge */
								if (eed->l == NULL || !bm_edge_has_visible_face(eed)) {
									ledges[rdata->mapped.loose_edge_len++] = i;
								}
							}
						}
					}
					rdata->mapped.loose_edges = MEM_reallocN(ledges, rdata->mapped.loose_edge_len * sizeof(int));
				}
			}
		}
	}
	else {
		rdata->me = me;

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
			MLoopTri *mlooptri = MEM_mallocN(sizeof(*mlooptri) * tri_len, __func__);
			BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, mlooptri);
			rdata->mlooptri = mlooptri;
		}
		if (types & MR_DATATYPE_LOOP) {
			rdata->loop_len = me->totloop;
			rdata->mloop = CustomData_get_layer(&me->ldata, CD_MLOOP);

			if (is_auto_smooth) {
				mesh_render_calc_normals_loop_and_poly(me, split_angle, rdata);
			}
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

		BLI_assert(cd_vused != NULL && cd_lused != NULL);

		if (me->edit_btmesh) {
			BMesh *bm = me->edit_btmesh->bm;
			cd_vdata = &bm->vdata;
			cd_ldata = &bm->ldata;
		}
		else {
			cd_vdata = &me->vdata;
			cd_ldata = &me->ldata;
		}

		rdata->cd.layers.uv_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
		rdata->cd.layers.vcol_active = CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL);
		rdata->cd.layers.tangent_active = rdata->cd.layers.uv_active;

#define CD_VALIDATE_ACTIVE_LAYER(active_index, used) \
		if ((active_index != -1) && (used & (1 << active_index)) == 0) { \
			active_index = -1; \
		} ((void)0)

		CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.uv_active, cd_lused[CD_MLOOPUV]);
		CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.tangent_active, cd_lused[CD_TANGENT]);
		CD_VALIDATE_ACTIVE_LAYER(rdata->cd.layers.vcol_active, cd_lused[CD_MLOOPCOL]);

#undef CD_VALIDATE_ACTIVE_LAYER

		rdata->is_orco_allocated = false;
		if (cd_vused[CD_ORCO] & 1) {
			rdata->orco = CustomData_get_layer(cd_vdata, CD_ORCO);
			/* If orco is not available compute it ourselves */
			if (!rdata->orco) {
				rdata->is_orco_allocated = true;
				if (me->edit_btmesh) {
					BMesh *bm = me->edit_btmesh->bm;
					rdata->orco = MEM_mallocN(sizeof(*rdata->orco) * rdata->vert_len, "orco mesh");
					BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
					for (int i = 0; i < bm->totvert; i++) {
						copy_v3_v3(rdata->orco[i], BM_vert_at_index(bm, i)->co);
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
					uint hash = BLI_ghashutil_strhash_p(name);
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
					uint hash = BLI_ghashutil_strhash_p(name);

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

			/* If tangent from orco is requested, decrement tangent_len */
			int actual_tangent_len = (cd_lused[CD_TANGENT] & DM_TANGENT_MASK_ORCO) ?
			        rdata->cd.layers.tangent_len - 1 : rdata->cd.layers.tangent_len;
			if (rdata->edit_bmesh) {
				BMEditMesh *em = rdata->edit_bmesh;
				BMesh *bm = em->bm;

				if (is_auto_smooth && rdata->loop_normals == NULL) {
					/* Should we store the previous array of `loop_normals` in somewhere? */
					rdata->loop_len = bm->totloop;
					rdata->loop_normals = MEM_mallocN(sizeof(*rdata->loop_normals) * rdata->loop_len, __func__);
					BM_loops_calc_normal_vcos(bm, NULL, NULL, NULL, true, split_angle, rdata->loop_normals, NULL, NULL, -1, false);
				}

				bool calc_active_tangent = false;

				BKE_editmesh_loop_tangent_calc(
				        em, calc_active_tangent,
				        tangent_names, actual_tangent_len,
				        rdata->poly_normals, rdata->loop_normals,
				        rdata->orco,
				        &rdata->cd.output.ldata, bm->totloop,
				        &rdata->cd.output.tangent_mask);
			}
			else {
#undef me

				if (is_auto_smooth && rdata->loop_normals == NULL) {
					/* Should we store the previous array of `loop_normals` in CustomData? */
					mesh_render_calc_normals_loop_and_poly(me, split_angle, rdata);
				}

				bool calc_active_tangent = false;

				BKE_mesh_calc_loop_tangent_ex(
				        me->mvert,
				        me->mpoly, me->totpoly,
				        me->mloop,
				        rdata->mlooptri, rdata->tri_len,
				        cd_ldata,
				        calc_active_tangent,
				        tangent_names, actual_tangent_len,
				        rdata->poly_normals, rdata->loop_normals,
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

			int i_dst = 0;
			for (int i_src = 0; i_src < cd_layers_src.uv_len; i_src++, i_dst++) {
				if ((cd_lused[CD_TANGENT] & (1 << i_src)) == 0) {
					i_dst--;
					if (rdata->cd.layers.tangent_active >= i_src) {
						rdata->cd.layers.tangent_active--;
					}
				}
				else {
					const char *name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i_src);
					uint hash = BLI_ghashutil_strhash_p(name);

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
			if (cd_lused[CD_TANGENT] & DM_TANGENT_MASK_ORCO) {
				const char *name = CustomData_get_layer_name(&rdata->cd.output.ldata, CD_TANGENT, i_dst);
				uint hash = BLI_ghashutil_strhash_p(name);
				BLI_snprintf(rdata->cd.uuid.tangent[i_dst], sizeof(*rdata->cd.uuid.tangent), "t%u", hash);

				rdata->cd.layers.tangent[i_dst] = CustomData_get_layer_n(&rdata->cd.output.ldata, CD_TANGENT, i_dst);
			}
		}

#undef me
	}

	return rdata;
}

/* Warning replace mesh pointer. */
#define MBC_GET_FINAL_MESH(me) \
	/* Hack to show the final result. */ \
	const bool _use_em_final = ( \
	        (me)->edit_btmesh && \
	        (me)->edit_btmesh->mesh_eval_final && \
	        ((me)->edit_btmesh->mesh_eval_final->runtime.is_original == false)); \
	Mesh _me_fake; \
	if (_use_em_final) { \
		_me_fake = *(me)->edit_btmesh->mesh_eval_final; \
		_me_fake.mat = (me)->mat; \
		_me_fake.totcol = (me)->totcol; \
		(me) = &_me_fake; \
	} ((void)0)

static void mesh_render_data_free(MeshRenderData *rdata)
{
	if (rdata->is_orco_allocated) {
		MEM_SAFE_FREE(rdata->orco);
	}
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
	MEM_SAFE_FREE(rdata->loop_normals);
	MEM_SAFE_FREE(rdata->poly_normals);
	MEM_SAFE_FREE(rdata->poly_normals_pack);
	MEM_SAFE_FREE(rdata->vert_normals_pack);
	MEM_SAFE_FREE(rdata->vert_weight);
	MEM_SAFE_FREE(rdata->edge_select_bool);
	MEM_SAFE_FREE(rdata->edge_visible_bool);
	MEM_SAFE_FREE(rdata->vert_color);

	MEM_SAFE_FREE(rdata->mapped.loose_verts);
	MEM_SAFE_FREE(rdata->mapped.loose_edges);

	CustomData_free(&rdata->cd.output.ldata, rdata->loop_len);

	MEM_freeN(rdata);
}

static MeshRenderData *mesh_render_data_create(Mesh *me, const int types)
{
	return mesh_render_data_create_ex(me, types, NULL, NULL);
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

static int UNUSED_FUNCTION(mesh_render_data_verts_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);
	return rdata->vert_len;
}
static int mesh_render_data_verts_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);
	return ((rdata->mapped.use == false) ? rdata->vert_len : rdata->mapped.vert_len);
}

static int UNUSED_FUNCTION(mesh_render_data_loose_verts_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return rdata->loose_vert_len;
}
static int mesh_render_data_loose_verts_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return ((rdata->mapped.use == false) ? rdata->loose_vert_len : rdata->mapped.loose_vert_len);
}

static int UNUSED_FUNCTION(mesh_render_data_edges_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_EDGE);
	return rdata->edge_len;
}
static int mesh_render_data_edges_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_EDGE);
	return ((rdata->mapped.use == false) ? rdata->edge_len : rdata->mapped.edge_len);
}

static int UNUSED_FUNCTION(mesh_render_data_loose_edges_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return rdata->loose_edge_len;
}
static int mesh_render_data_loose_edges_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_OVERLAY);
	return ((rdata->mapped.use == false) ? rdata->loose_edge_len : rdata->mapped.loose_edge_len);
}

static int mesh_render_data_looptri_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
	return rdata->tri_len;
}
static int mesh_render_data_looptri_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_LOOPTRI);
	return ((rdata->mapped.use == false) ? rdata->tri_len : rdata->mapped.tri_len);
}

static int UNUSED_FUNCTION(mesh_render_data_mat_len_get)(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_POLY);
	return rdata->mat_len;
}

static int mesh_render_data_loops_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_LOOP);
	return rdata->loop_len;
}

static int mesh_render_data_polys_len_get(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_POLY);
	return rdata->poly_len;
}
static int mesh_render_data_polys_len_get_maybe_mapped(const MeshRenderData *rdata)
{
	BLI_assert(rdata->types & MR_DATATYPE_POLY);
	return ((rdata->mapped.use == false) ? rdata->poly_len : rdata->mapped.poly_len);
}

/** \} */


/* ---------------------------------------------------------------------- */

/* TODO remove prototype. */
static void mesh_create_edit_facedots(MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor_data_facedots);

/** \name Internal Cache (Lazy Initialization)
 * \{ */

/** Ensure #MeshRenderData.poly_normals_pack */
static void mesh_render_data_ensure_poly_normals_pack(MeshRenderData *rdata)
{
	GPUPackedNormal *pnors_pack = rdata->poly_normals_pack;
	if (pnors_pack == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter fiter;
			BMFace *efa;
			int i;

			pnors_pack = rdata->poly_normals_pack = MEM_mallocN(sizeof(*pnors_pack) * rdata->poly_len, __func__);
			if (rdata->edit_data && rdata->edit_data->vertexCos != NULL) {
				BKE_editmesh_cache_ensure_poly_normals(rdata->edit_bmesh, rdata->edit_data);
				const float (*pnors)[3] = rdata->edit_data->polyNos;
				for (i = 0; i < bm->totface; i++) {
					pnors_pack[i] = GPU_normal_convert_i10_v3(pnors[i]);
				}
			}
			else {
				BM_ITER_MESH_INDEX(efa, &fiter, bm, BM_FACES_OF_MESH, i) {
					pnors_pack[i] = GPU_normal_convert_i10_v3(efa->no);
				}
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

			pnors_pack = rdata->poly_normals_pack = MEM_mallocN(sizeof(*pnors_pack) * rdata->poly_len, __func__);
			for (int i = 0; i < rdata->poly_len; i++) {
				pnors_pack[i] = GPU_normal_convert_i10_v3(pnors[i]);
			}
		}
	}
}

/** Ensure #MeshRenderData.vert_normals_pack */
static void mesh_render_data_ensure_vert_normals_pack(MeshRenderData *rdata)
{
	GPUPackedNormal *vnors_pack = rdata->vert_normals_pack;
	if (vnors_pack == NULL) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter viter;
			BMVert *eve;
			int i;

			vnors_pack = rdata->vert_normals_pack = MEM_mallocN(sizeof(*vnors_pack) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(eve, &viter, bm, BM_VERT, i) {
				vnors_pack[i] = GPU_normal_convert_i10_v3(eve->no);
			}
		}
		else {
			/* data from mesh used directly */
			BLI_assert(0);
		}
	}
}


/** Ensure #MeshRenderData.vert_color */
static void UNUSED_FUNCTION(mesh_render_data_ensure_vert_color)(MeshRenderData *rdata)
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

static float evaluate_vertex_weight(const MDeformVert *dvert, const DRW_MeshWeightState *wstate)
{
	float input = 0.0f;
	bool show_alert_color = false;

	if (wstate->flags & DRW_MESH_WEIGHT_STATE_MULTIPAINT) {
		/* Multi-Paint feature */
		input = BKE_defvert_multipaint_collective_weight(
		        dvert, wstate->defgroup_len, wstate->defgroup_sel, wstate->defgroup_sel_count,
		        (wstate->flags & DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE) != 0);

		/* make it black if the selected groups have no weight on a vertex */
		if (input == 0.0f) {
			show_alert_color = true;
		}
	}
	else {
		/* default, non tricky behavior */
		input = defvert_find_weight(dvert, wstate->defgroup_active);

		if (input == 0.0f) {
			switch (wstate->alert_mode) {
				case OB_DRAW_GROUPUSER_ACTIVE:
					show_alert_color = true;
					break;

				case OB_DRAW_GROUPUSER_ALL:
					show_alert_color = defvert_is_weight_zero(dvert, wstate->defgroup_len);
					break;
			}
		}
	}

	if (show_alert_color) {
		return -1.0f;
	}
	else {
		CLAMP(input, 0.0f, 1.0f);
		return input;
	}
}

/** Ensure #MeshRenderData.vert_weight */
static void mesh_render_data_ensure_vert_weight(MeshRenderData *rdata, const struct DRW_MeshWeightState *wstate)
{
	float *vweight = rdata->vert_weight;
	if (vweight == NULL) {
		if (wstate->defgroup_active == -1) {
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

			vweight = rdata->vert_weight = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
			BM_ITER_MESH_INDEX(eve, &viter, bm, BM_VERT, i) {
				const MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				vweight[i] = evaluate_vertex_weight(dvert, wstate);
			}
		}
		else {
			if (rdata->dvert == NULL) {
				goto fallback;
			}

			vweight = rdata->vert_weight = MEM_mallocN(sizeof(*vweight) * rdata->vert_len, __func__);
			for (int i = 0; i < rdata->vert_len; i++) {
				vweight[i] = evaluate_vertex_weight(&rdata->dvert[i], wstate);
			}
		}
	}
	return;

fallback:
	vweight = rdata->vert_weight = MEM_callocN(sizeof(*vweight) * rdata->vert_len, __func__);

	if ((wstate->defgroup_active < 0) && (wstate->defgroup_len > 0)) {
		copy_vn_fl(vweight, rdata->vert_len, -2.0f);
	}
	else if (wstate->alert_mode != OB_DRAW_GROUPUSER_NONE) {
		copy_vn_fl(vweight, rdata->vert_len, -1.0f);
	}
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Internal Cache Generation
 * \{ */

static uchar mesh_render_data_looptri_flag(MeshRenderData *rdata, const BMFace *efa)
{
	uchar fflag = 0;

	if (efa == rdata->efa_act)
		fflag |= VFLAG_FACE_ACTIVE;

	if (BM_elem_flag_test(efa, BM_ELEM_SELECT))
		fflag |= VFLAG_FACE_SELECTED;

#ifdef WITH_FREESTYLE
	if (rdata->cd.offset.freestyle_face != -1) {
		const FreestyleFace *ffa = BM_ELEM_CD_GET_VOID_P(efa, rdata->cd.offset.freestyle_face);
		if (ffa->flag & FREESTYLE_FACE_MARK) {
			fflag |= VFLAG_FACE_FREESTYLE;
		}
	}
#endif

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
			eattr->crease = (uchar)(crease * 255.0f);
		}
	}

	/* Use a byte for value range */
	if (rdata->cd.offset.bweight != -1) {
		float bweight = BM_ELEM_CD_GET_FLOAT(eed, rdata->cd.offset.bweight);
		if (bweight > 0) {
			eattr->bweight = (uchar)(bweight * 255.0f);
		}
	}

#ifdef WITH_FREESTYLE
	if (rdata->cd.offset.freestyle_edge != -1) {
		const FreestyleEdge *fed = BM_ELEM_CD_GET_VOID_P(eed, rdata->cd.offset.freestyle_edge);
		if (fed->flag & FREESTYLE_EDGE_MARK) {
			eattr->e_flag |= VFLAG_EDGE_FREESTYLE;
		}
	}
#endif
}

static uchar mesh_render_data_vertex_flag(MeshRenderData *rdata, const BMVert *eve)
{
	uchar vflag = VFLAG_VERTEX_EXISTS;

	/* Current vertex */
	if (eve == rdata->eve_act)
		vflag |= VFLAG_VERTEX_ACTIVE;

	if (BM_elem_flag_test(eve, BM_ELEM_SELECT))
		vflag |= VFLAG_VERTEX_SELECTED;

	return vflag;
}

static void add_edit_tri(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_lnor, GPUVertBuf *vbo_data, GPUIndexBufBuilder *elb,
        const uint pos_id, const uint vnor_id, const uint lnor_id, const uint data_id,
        const BMLoop **bm_looptri, const int base_vert_idx)
{
	uchar fflag;
	uchar vflag;

	/* Only draw vertices once. */
	if (elb) {
		for (int i = 0; i < 3; ++i) {
			if (!BM_elem_flag_test(bm_looptri[i]->v, BM_ELEM_TAG)) {
				BM_elem_flag_enable(bm_looptri[i]->v, BM_ELEM_TAG);
				GPU_indexbuf_add_generic_vert(elb, base_vert_idx + i);
			}
		}
	}

	if (vbo_pos_nor) {
		/* TODO(sybren): deduplicate this and all the other places it's pasted to in this file. */
		if (rdata->edit_data && rdata->edit_data->vertexCos) {
			for (uint i = 0; i < 3; i++) {
				int vidx = BM_elem_index_get(bm_looptri[i]->v);
				const float *pos = rdata->edit_data->vertexCos[vidx];
				GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);
			}
		}
		else {
			for (uint i = 0; i < 3; i++) {
				const float *pos = bm_looptri[i]->v->co;
				GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);
			}
		}

		for (uint i = 0; i < 3; i++) {
			GPUPackedNormal vnor = GPU_normal_convert_i10_v3(bm_looptri[i]->v->no);
			GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx + i, &vnor);
		}
	}

	if (vbo_lnor) {
		float (*lnors)[3] = rdata->loop_normals;
		for (uint i = 0; i < 3; i++) {
			const float *nor = (lnors) ? lnors[BM_elem_index_get(bm_looptri[i])] : bm_looptri[0]->f->no;
			GPUPackedNormal lnor = GPU_normal_convert_i10_v3(nor);
			GPU_vertbuf_attr_set(vbo_lnor, lnor_id, base_vert_idx + i, &lnor);
		}
	}

	if (vbo_data) {
		fflag = mesh_render_data_looptri_flag(rdata, bm_looptri[0]->f);
		for (uint i = 0; i < 3; i++) {
			const int i_next = (i + 1) % 3;
			const int i_prev = (i + 2) % 3;
			vflag = mesh_render_data_vertex_flag(rdata, bm_looptri[i]->v);
			/* Opposite edge to the vertex at 'i'. */
			EdgeDrawAttr eattr = {0};
			const bool is_edge_real = (bm_looptri[i_next] == bm_looptri[i_prev]->prev);
			if (is_edge_real) {
				mesh_render_data_edge_flag(rdata, bm_looptri[i_next]->e, &eattr);
			}
			eattr.v_flag = fflag | vflag;
			GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr);
		}
	}
}
static bool add_edit_tri_mapped(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_lnor, GPUVertBuf *vbo_data, GPUIndexBufBuilder *elb,
        const uint pos_id, const uint vnor_id, const uint lnor_id, const uint data_id,
        BMFace *efa, const MLoopTri *mlt, const float (*poly_normals)[3], const float (*loop_normals)[3], const int base_vert_idx)
{
	if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return false;
	}

	BMEditMesh *embm = rdata->edit_bmesh;
	BMesh *bm = embm->bm;
	Mesh *me_cage = embm->mesh_eval_cage;

	const MVert *mvert = me_cage->mvert;
	const MEdge *medge = me_cage->medge;
	const MLoop *mloop = me_cage->mloop;

	const int *v_origindex = rdata->mapped.v_origindex;
	const int *e_origindex = rdata->mapped.e_origindex;

	if (elb) {
		for (int i = 0; i < 3; ++i) {
			const int v_orig = v_origindex[mloop[mlt->tri[i]].v];
			if (v_orig == ORIGINDEX_NONE) {
				continue;
			}
			BMVert *v = BM_vert_at_index(bm, v_orig);
			if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
				BM_elem_flag_enable(v, BM_ELEM_TAG);
				GPU_indexbuf_add_generic_vert(elb, base_vert_idx + i);
			}
		}
	}

	if (vbo_pos_nor) {
		for (uint i = 0; i < 3; i++) {
			const float *pos = mvert[mloop[mlt->tri[i]].v].co;
			GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[mloop[mlt->tri[i]].v].no);
			GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);
			GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx + i, &vnor);
		}
	}

	if (vbo_lnor) {
		for (uint i = 0; i < 3; i++) {
			const float *nor = loop_normals ? loop_normals[mlt->tri[i]] : poly_normals[mlt->poly];
			GPUPackedNormal lnor = GPU_normal_convert_i10_v3(nor);
			GPU_vertbuf_attr_set(vbo_lnor, lnor_id, base_vert_idx + i, &lnor);
		}
	}

	if (vbo_data) {
		EdgeDrawAttr eattr[3] = {{0}}; /* Importantly VFLAG_VERTEX_EXISTS is not set. */
		uchar fflag = mesh_render_data_looptri_flag(rdata, efa);
		for (uint i = 0; i < 3; i++) {
			const int i_next = (i + 1) % 3;
			const int i_prev = (i + 2) % 3;
			const int v_orig = v_origindex[mloop[mlt->tri[i]].v];
			if (v_orig != ORIGINDEX_NONE) {
				BMVert *v = BM_vert_at_index(bm, v_orig);
				eattr[i].v_flag |= mesh_render_data_vertex_flag(rdata, v);
			}
			/* Opposite edge to the vertex at 'i'. */
			const int e_idx = mloop[mlt->tri[i_next]].e;
			const int e_orig = e_origindex[e_idx];
			if (e_orig != ORIGINDEX_NONE) {
				const MEdge *ed = &medge[e_idx];
				const uint tri_edge[2]  = {mloop[mlt->tri[i_prev]].v, mloop[mlt->tri[i_next]].v};
				const bool is_edge_real = (
				        ((ed->v1 == tri_edge[0]) && (ed->v2 == tri_edge[1])) ||
				        ((ed->v1 == tri_edge[1]) && (ed->v2 == tri_edge[0])));
				if (is_edge_real) {
					BMEdge *eed = BM_edge_at_index(bm, e_orig);
					mesh_render_data_edge_flag(rdata, eed, &eattr[i]);
					/* Set vertex selected if both original verts are selected. */
					if (BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) &&
					    BM_elem_flag_test(eed->v2, BM_ELEM_SELECT))
					{
						eattr[i_next].v_flag |= VFLAG_VERTEX_SELECTED;
						eattr[i_prev].v_flag |= VFLAG_VERTEX_SELECTED;
					}
				}
			}
		}
		for (uint i = 0; i < 3; i++) {
			eattr[i].v_flag |= fflag;
			GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr[i]);
		}
	}

	return true;
}

static void add_edit_loose_edge(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_data,
        const uint pos_id, const uint vnor_id, const uint data_id,
        const BMEdge *eed, const int base_vert_idx)
{
	if (vbo_pos_nor) {
		/* TODO(sybren): deduplicate this and all the other places it's pasted to in this file. */
		if (rdata->edit_data && rdata->edit_data->vertexCos) {
			for (uint i = 0; i < 2; i++) {
				int vidx = BM_elem_index_get((&eed->v1)[i]);
				const float *pos = rdata->edit_data->vertexCos[vidx];
				GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);
			}
		}
		else {
			for (int i = 0; i < 2; i++) {
				const float *pos = (&eed->v1)[i]->co;
				GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);
			}
		}

		for (int i = 0; i < 2; i++) {
			GPUPackedNormal vnor = GPU_normal_convert_i10_v3((&eed->v1)[i]->no);
			GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx + i, &vnor);
		}
	}

	if (vbo_data) {
		EdgeDrawAttr eattr = {0};
		mesh_render_data_edge_flag(rdata, eed, &eattr);
		for (int i = 0; i < 2; i++) {
			eattr.v_flag = mesh_render_data_vertex_flag(rdata, (&eed->v1)[i]);
			GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr);
		}
	}
}
static void add_edit_loose_edge_mapped(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_data,
        const uint pos_id, const uint vnor_id, const uint data_id,
        BMEdge *eed, const MVert *mvert, const MEdge *ed, const int base_vert_idx)
{
	if (vbo_pos_nor) {
		/* TODO(sybren): deduplicate this and all the other places it's pasted to in this file. */
		for (int i = 0; i < 2; i++) {
			const float *pos = mvert[*(&ed->v1 + i)].co;
			GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx + i, pos);

			GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mvert[*(&ed->v1 + i)].no);
			GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx + i, &vnor);
		}
	}

	if (vbo_data) {
		EdgeDrawAttr eattr = {0};
		mesh_render_data_edge_flag(rdata, eed, &eattr);
		for (int i = 0; i < 2; i++) {
			const int v_orig = rdata->mapped.v_origindex[*(&ed->v1 + i)];
			eattr.v_flag = (v_orig != ORIGINDEX_NONE) ? mesh_render_data_vertex_flag(rdata, (&eed->v1)[i]) : 0;
			GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx + i, &eattr);
		}
	}
}

static void add_edit_loose_vert(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_data,
        const uint pos_id, const uint vnor_id, const uint data_id,
        const BMVert *eve, const int base_vert_idx)
{
	if (vbo_pos_nor) {
		/* TODO(sybren): deduplicate this and all the other places it's pasted to in this file. */
		if (rdata->edit_data && rdata->edit_data->vertexCos) {
			int vidx = BM_elem_index_get(eve);
			const float *pos = rdata->edit_data->vertexCos[vidx];
			GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx, pos);
		}
		else {
			const float *pos = eve->co;
			GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx, pos);
		}

		GPUPackedNormal vnor = GPU_normal_convert_i10_v3(eve->no);
		GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx, &vnor);
	}

	if (vbo_data) {
		uchar vflag[4] = {0, 0, 0, 0};
		vflag[0] = mesh_render_data_vertex_flag(rdata, eve);
		GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx, vflag);
	}
}
static void add_edit_loose_vert_mapped(
        MeshRenderData *rdata, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_data,
        const uint pos_id, const uint vnor_id, const uint data_id,
        const BMVert *eve, const MVert *mv, const int base_vert_idx)
{
	if (vbo_pos_nor) {
		const float *pos = mv->co;
		GPU_vertbuf_attr_set(vbo_pos_nor, pos_id, base_vert_idx, pos);

		GPUPackedNormal vnor = GPU_normal_convert_i10_s3(mv->no);
		GPU_vertbuf_attr_set(vbo_pos_nor, vnor_id, base_vert_idx, &vnor);
	}

	if (vbo_data) {
		uchar vflag[4] = {0, 0, 0, 0};
		vflag[0] = mesh_render_data_vertex_flag(rdata, eve);
		GPU_vertbuf_attr_set(vbo_data, data_id, base_vert_idx, vflag);
	}
}

static bool add_edit_facedot(
        MeshRenderData *rdata, GPUVertBuf *vbo,
        const uint fdot_pos_id, const uint fdot_nor_flag_id,
        const int poly, const int base_vert_idx)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));
	float pnor[3], center[3];
	bool selected;
	if (rdata->edit_bmesh) {
		const BMFace *efa = BM_face_at_index(rdata->edit_bmesh->bm, poly);
		if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
			return false;
		}
		if (rdata->edit_data && rdata->edit_data->vertexCos) {
			copy_v3_v3(center, rdata->edit_data->polyCos[poly]);
			copy_v3_v3(pnor, rdata->edit_data->polyNos[poly]);
		}
		else {
			BM_face_calc_center_median(efa, center);
			copy_v3_v3(pnor, efa->no);
		}
		selected = (BM_elem_flag_test(efa, BM_ELEM_SELECT) != 0) ? true : false;
	}
	else {
		MVert *mvert = rdata->mvert;
		const MPoly *mpoly = rdata->mpoly + poly;
		const MLoop *mloop = rdata->mloop + mpoly->loopstart;

		BKE_mesh_calc_poly_center(mpoly, mloop, mvert, center);
		BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, pnor);

		selected = false; /* No selection if not in edit mode */
	}

	GPUPackedNormal nor = GPU_normal_convert_i10_v3(pnor);
	nor.w = (selected) ? 1 : 0;
	GPU_vertbuf_attr_set(vbo, fdot_nor_flag_id, base_vert_idx, &nor);
	GPU_vertbuf_attr_set(vbo, fdot_pos_id, base_vert_idx, center);

	return true;
}
static bool add_edit_facedot_mapped(
        MeshRenderData *rdata, GPUVertBuf *vbo,
        const uint fdot_pos_id, const uint fdot_nor_flag_id,
        const int poly, const int base_vert_idx)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));
	float pnor[3], center[3];
	const int *p_origindex = rdata->mapped.p_origindex;
	const int p_orig = p_origindex[poly];
	if (p_orig == ORIGINDEX_NONE) {
		return false;
	}
	BMEditMesh *em = rdata->edit_bmesh;
	const BMFace *efa = BM_face_at_index(rdata->edit_bmesh->bm, p_orig);
	if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
		return false;
	}

	Mesh *me_cage = em->mesh_eval_cage;
	const MVert *mvert = me_cage->mvert;
	const MLoop *mloop = me_cage->mloop;
	const MPoly *mpoly = me_cage->mpoly;

	const MPoly *mp = mpoly + poly;
	const MLoop *ml = mloop + mp->loopstart;

	BKE_mesh_calc_poly_center(mp, ml, mvert, center);
	BKE_mesh_calc_poly_normal(mp, ml, mvert, pnor);

	GPUPackedNormal nor = GPU_normal_convert_i10_v3(pnor);
	nor.w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) != 0) ? 1 : 0;
	GPU_vertbuf_attr_set(vbo, fdot_nor_flag_id, base_vert_idx, &nor);
	GPU_vertbuf_attr_set(vbo, fdot_pos_id, base_vert_idx, center);

	return true;
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Vertex Group Selection
 * \{ */

/** Reset the selection structure, deallocating heap memory as appropriate. */
static void drw_mesh_weight_state_clear(struct DRW_MeshWeightState *wstate)
{
	MEM_SAFE_FREE(wstate->defgroup_sel);

	memset(wstate, 0, sizeof(*wstate));

	wstate->defgroup_active = -1;
}

/** Copy selection data from one structure to another, including heap memory. */
static void drw_mesh_weight_state_copy(
        struct DRW_MeshWeightState *wstate_dst, const struct DRW_MeshWeightState *wstate_src)
{
	MEM_SAFE_FREE(wstate_dst->defgroup_sel);

	memcpy(wstate_dst, wstate_src, sizeof(*wstate_dst));

	if (wstate_src->defgroup_sel) {
		wstate_dst->defgroup_sel = MEM_dupallocN(wstate_src->defgroup_sel);
	}
}

/** Compare two selection structures. */
static bool drw_mesh_weight_state_compare(const struct DRW_MeshWeightState *a, const struct DRW_MeshWeightState *b)
{
	return a->defgroup_active == b->defgroup_active &&
	       a->defgroup_len == b->defgroup_len &&
	       a->flags == b->flags &&
	       a->alert_mode == b->alert_mode &&
	       a->defgroup_sel_count == b->defgroup_sel_count &&
	       ((!a->defgroup_sel && !b->defgroup_sel) ||
	        (a->defgroup_sel && b->defgroup_sel &&
	         memcmp(a->defgroup_sel, b->defgroup_sel, a->defgroup_len * sizeof(bool)) == 0));
}

static void drw_mesh_weight_state_extract(
        Object *ob, Mesh *me, ToolSettings *ts, bool paint_mode,
        struct DRW_MeshWeightState *wstate)
{
	/* Extract complete vertex weight group selection state and mode flags. */
	memset(wstate, 0, sizeof(*wstate));

	wstate->defgroup_active = ob->actdef - 1;
	wstate->defgroup_len = BLI_listbase_count(&ob->defbase);

	wstate->alert_mode = ts->weightuser;

	if (paint_mode && ts->multipaint) {
		/* Multipaint needs to know all selected bones, not just the active group.
		 * This is actually a relatively expensive operation, but caching would be difficult. */
		wstate->defgroup_sel = BKE_object_defgroup_selected_get(ob, wstate->defgroup_len, &wstate->defgroup_sel_count);

		if (wstate->defgroup_sel_count > 1) {
			wstate->flags |= DRW_MESH_WEIGHT_STATE_MULTIPAINT | (ts->auto_normalize ? DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE : 0);

			if (me->editflag & ME_EDIT_MIRROR_X) {
				BKE_object_defgroup_mirror_selection(
				        ob, wstate->defgroup_len, wstate->defgroup_sel, wstate->defgroup_sel, &wstate->defgroup_sel_count);
			}
		}
		/* With only one selected bone Multipaint reverts to regular mode. */
		else {
			wstate->defgroup_sel_count = 0;
			MEM_SAFE_FREE(wstate->defgroup_sel);
		}
	}
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Mesh GPUBatch Cache
 * \{ */

typedef struct MeshBatchCache {
	/* In order buffers: All verts only specified once.
	 * To be used with a GPUIndexBuf. */
	struct {
		/* Vertex data. */
		GPUVertBuf *pos_nor;
		GPUVertBuf *weights;
		/* Loop data. */
		GPUVertBuf *loop_pos_nor;
		GPUVertBuf *loop_uv_tan;
		GPUVertBuf *loop_vcol;
	} ordered;

	/* Tesselated: (all verts specified for each triangles).
	 * Indices does not match the CPU data structure's. */
	struct {
		GPUVertBuf *pos_nor;
		GPUVertBuf *wireframe_data;
	} tess;

	/* Edit Mesh Data:
	 * Data is also tesselated because of barycentric wireframe rendering. */
	struct {
		GPUVertBuf *pos_nor;
		GPUVertBuf *pos_nor_ledges;
		GPUVertBuf *pos_nor_lverts;
		GPUVertBuf *pos_nor_data_facedots;
		GPUVertBuf *data;
		GPUVertBuf *data_ledges;
		GPUVertBuf *data_lverts;
		GPUVertBuf *lnor;
	} edit;

	/* Index Buffers:
	 * Only need to be updated when topology changes. */
	struct {
		/* Indices to verts. */
		GPUIndexBuf *surf_tris;
		GPUIndexBuf *edges_lines;
		GPUIndexBuf *edges_adj_lines;
		GPUIndexBuf *loose_edges_lines;
		/* Indices to vloops. */
		GPUIndexBuf *loops_tris;
		GPUIndexBuf *loops_lines;
		/* Contains indices to unique edit vertices to not
		 * draw the same vert multiple times (because of tesselation). */
		GPUIndexBuf *edit_verts_points;
	} ibo;

	struct {
		/* Surfaces / Render */
		GPUBatch *surface;
		GPUBatch *surface_weights;
		/* Edit mode */
		GPUBatch *edit_triangles;
		GPUBatch *edit_vertices;
		GPUBatch *edit_loose_edges;
		GPUBatch *edit_loose_verts;
		GPUBatch *edit_triangles_nor;
		GPUBatch *edit_triangles_lnor;
		GPUBatch *edit_loose_edges_nor;
		GPUBatch *edit_facedots;
		/* Common display / Other */
		GPUBatch *all_verts;
		GPUBatch *all_edges;
		GPUBatch *loose_edges;
		GPUBatch *edge_detection;
		GPUBatch *wire_loops; /* Loops around faces. */
		GPUBatch *wire_triangles; /* Triangles for object mode wireframe. */
	} batch;

	GPUIndexBuf **surf_per_mat_tris;
	GPUBatch **surf_per_mat;

	/* OLD BATCH METHOD, thoses needs to be ported and added in the structs above. */

	/* Indices buffers. */
	GPUIndexBuf *edges_in_order;
	GPUIndexBuf *edges_adjacency; /* Store edges with adjacent vertices. */
	GPUIndexBuf *triangles_in_order;
	GPUIndexBuf *ledges_in_order;

	GPUBatch *all_triangles;

	GPUVertBuf *pos_with_normals;
	GPUVertBuf *pos_with_normals_visible_only;
	GPUVertBuf *pos_with_normals_edit;
	GPUVertBuf *pos_with_normals_visible_only_edit;
	GPUVertBuf *tri_aligned_uv;  /* Active UV layer (mloopuv) */

	/**
	 * Other uses are all positions or loose elements.
	 * This stores all visible elements, needed for selection.
	 */
	GPUVertBuf *ed_fcenter_pos_with_nor_and_sel;
	GPUVertBuf *ed_edge_pos;
	GPUVertBuf *ed_vert_pos;

	GPUBatch *triangles_with_normals;
	GPUBatch *ledges_with_normals;

	/* Skip hidden (depending on paint select mode) */
	GPUBatch *triangles_with_weights;
	GPUBatch *triangles_with_vert_colors;
	/* Always skip hidden */
	GPUBatch *triangles_with_select_mask;
	GPUBatch *triangles_with_select_id;
	uint       triangles_with_select_id_offset;

	GPUBatch *facedot_with_select_id;  /* shares vbo with 'edit_facedots' */
	GPUBatch *edges_with_select_id;
	GPUBatch *verts_with_select_id;

	uint facedot_with_select_id_offset;
	uint edges_with_select_id_offset;
	uint verts_with_select_id_offset;

	GPUBatch *points_with_normals;
	GPUBatch *fancy_edges; /* owns its vertex buffer (not shared) */

	GPUBatch *edge_detection;

	/* Texture Paint.*/
	/* per-texture batch */
	GPUBatch **texpaint_triangles;
	GPUBatch  *texpaint_triangles_single;

	GPUBatch *overlay_weight_faces;
	GPUBatch *overlay_weight_verts;
	GPUBatch *overlay_paint_edges;

	/* 2D/UV edit */
	GPUVertBuf *edituv_pos;
	GPUVertBuf *edituv_area;
	GPUVertBuf *edituv_angle;
	GPUVertBuf *edituv_data;

	GPUIndexBuf *edituv_visible_faces;
	GPUIndexBuf *edituv_visible_edges;

	GPUBatch *texpaint_uv_loops;

	GPUBatch *edituv_faces_strech_area;
	GPUBatch *edituv_faces_strech_angle;
	GPUBatch *edituv_faces;
	GPUBatch *edituv_edges;
	GPUBatch *edituv_verts;
	GPUBatch *edituv_facedots;

	char edituv_state;

	/* arrays of bool uniform names (and value) that will be use to
	 * set srgb conversion for auto attribs.*/
	char *auto_layer_names;
	int *auto_layer_is_srgb;
	int auto_layer_len;

	/* settings to determine if cache is invalid */
	bool is_maybe_dirty;
	bool is_dirty; /* Instantly invalidates cache, skipping mesh check */
	int edge_len;
	int tri_len;
	int poly_len;
	int vert_len;
	int mat_len;
	bool is_editmode;

	struct DRW_MeshWeightState weight_state;

	uchar cd_vused[CD_NUMTYPES];
	uchar cd_vneeded[CD_NUMTYPES];
	ushort cd_lused[CD_NUMTYPES];
	ushort cd_lneeded[CD_NUMTYPES];

	/* XXX, only keep for as long as sculpt mode uses shaded drawing. */
	bool is_sculpt_points_tag;

	/* Valid only if edges_adjacency is up to date. */
	bool is_manifold;
} MeshBatchCache;

/* GPUBatch cache management. */

static bool mesh_batch_cache_valid(Mesh *me)
{
	MeshBatchCache *cache = me->runtime.batch_cache;

	if (cache == NULL) {
		return false;
	}

	if (cache->mat_len != mesh_render_mat_len_get(me)) {
		return false;
	}

	if (cache->is_editmode != (me->edit_btmesh != NULL)) {
		return false;
	}

	if (cache->is_dirty) {
		return false;
	}

	if (cache->is_maybe_dirty == false) {
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
	MeshBatchCache *cache = me->runtime.batch_cache;

	if (!cache) {
		cache = me->runtime.batch_cache = MEM_callocN(sizeof(*cache), __func__);
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
	cache->surf_per_mat_tris = MEM_callocN(sizeof(*cache->surf_per_mat_tris) * cache->mat_len, __func__);
	cache->surf_per_mat = MEM_callocN(sizeof(*cache->surf_per_mat) * cache->mat_len, __func__);

	/* TODO Might be wiser to alloc in one chunck. */
	for (int i = 0; i < cache->mat_len; ++i) {
		cache->surf_per_mat_tris[i] = MEM_callocN(sizeof(GPUIndexBuf), "GPUIndexBuf");
		cache->surf_per_mat[i] = MEM_callocN(sizeof(GPUBatch), "GPUBatch");
	}

	cache->is_maybe_dirty = false;
	cache->is_dirty = false;

	drw_mesh_weight_state_clear(&cache->weight_state);
}

static MeshBatchCache *mesh_batch_cache_get(Mesh *me)
{
	if (!mesh_batch_cache_valid(me)) {
		mesh_batch_cache_clear(me);
		mesh_batch_cache_init(me);
	}
	return me->runtime.batch_cache;
}

static void mesh_batch_cache_check_vertex_group(MeshBatchCache *cache, const struct DRW_MeshWeightState *wstate)
{
	if (!drw_mesh_weight_state_compare(&cache->weight_state, wstate)) {
		GPU_BATCH_CLEAR_SAFE(cache->batch.surface_weights);
		GPU_VERTBUF_DISCARD_SAFE(cache->ordered.weights);

		drw_mesh_weight_state_clear(&cache->weight_state);
	}
}

static void mesh_batch_cache_discard_shaded_tri(MeshBatchCache *cache)
{
	GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_pos_nor);
	GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_uv_tan);
	GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_vcol);
	/* TODO */
	// GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_orco);

	if (cache->surf_per_mat_tris) {
		for (int i = 0; i < cache->mat_len; i++) {
			GPU_INDEXBUF_DISCARD_SAFE(cache->surf_per_mat_tris[i]);
		}
	}
	MEM_SAFE_FREE(cache->surf_per_mat_tris);
	if (cache->surf_per_mat) {
		for (int i = 0; i < cache->mat_len; i++) {
			GPU_BATCH_DISCARD_SAFE(cache->surf_per_mat[i]);
		}
	}
	MEM_SAFE_FREE(cache->surf_per_mat);

	MEM_SAFE_FREE(cache->texpaint_triangles);

	MEM_SAFE_FREE(cache->auto_layer_names);
	MEM_SAFE_FREE(cache->auto_layer_is_srgb);

	cache->mat_len = 0;
}

static void mesh_batch_cache_discard_uvedit(MeshBatchCache *cache)
{
	GPU_VERTBUF_DISCARD_SAFE(cache->edituv_pos);
	GPU_VERTBUF_DISCARD_SAFE(cache->edituv_area);
	GPU_VERTBUF_DISCARD_SAFE(cache->edituv_angle);
	GPU_VERTBUF_DISCARD_SAFE(cache->edituv_data);

	GPU_INDEXBUF_DISCARD_SAFE(cache->edituv_visible_faces);
	GPU_INDEXBUF_DISCARD_SAFE(cache->edituv_visible_edges);

	if (cache->edituv_faces_strech_area) {
		gpu_batch_presets_unregister(cache->edituv_faces_strech_area);
	}
	if (cache->edituv_faces_strech_angle) {
		gpu_batch_presets_unregister(cache->edituv_faces_strech_angle);
	}
	if (cache->edituv_faces) {
		gpu_batch_presets_unregister(cache->edituv_faces);
	}
	if (cache->edituv_edges) {
		gpu_batch_presets_unregister(cache->edituv_edges);
	}
	if (cache->edituv_verts) {
		gpu_batch_presets_unregister(cache->edituv_verts);
	}
	if (cache->edituv_facedots) {
		gpu_batch_presets_unregister(cache->edituv_facedots);
	}

	GPU_BATCH_DISCARD_SAFE(cache->edituv_faces_strech_area);
	GPU_BATCH_DISCARD_SAFE(cache->edituv_faces_strech_angle);
	GPU_BATCH_DISCARD_SAFE(cache->edituv_faces);
	GPU_BATCH_DISCARD_SAFE(cache->edituv_edges);
	GPU_BATCH_DISCARD_SAFE(cache->edituv_verts);
	GPU_BATCH_DISCARD_SAFE(cache->edituv_facedots);

	gpu_batch_presets_unregister(cache->texpaint_uv_loops);

	GPU_BATCH_DISCARD_SAFE(cache->texpaint_uv_loops);

	cache->edituv_state = 0;
}

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, int mode)
{
	MeshBatchCache *cache = me->runtime.batch_cache;
	if (cache == NULL) {
		return;
	}
	switch (mode) {
		case BKE_MESH_BATCH_DIRTY_MAYBE_ALL:
			cache->is_maybe_dirty = true;
			break;
		case BKE_MESH_BATCH_DIRTY_SELECT:
			GPU_VERTBUF_DISCARD_SAFE(cache->edit.data);
			GPU_VERTBUF_DISCARD_SAFE(cache->edit.data_ledges);
			GPU_VERTBUF_DISCARD_SAFE(cache->edit.data_lverts);
			GPU_VERTBUF_DISCARD_SAFE(cache->edit.pos_nor_data_facedots);
			GPU_BATCH_DISCARD_SAFE(cache->batch.edit_triangles);
			GPU_BATCH_DISCARD_SAFE(cache->batch.edit_vertices);
			GPU_BATCH_DISCARD_SAFE(cache->batch.edit_loose_verts);
			GPU_BATCH_DISCARD_SAFE(cache->batch.edit_loose_edges);
			GPU_BATCH_DISCARD_SAFE(cache->batch.edit_facedots);
			GPU_VERTBUF_DISCARD_SAFE(cache->ed_edge_pos);
			GPU_VERTBUF_DISCARD_SAFE(cache->ed_vert_pos);
			/* Edit mode selection. */
			GPU_BATCH_DISCARD_SAFE(cache->facedot_with_select_id);
			GPU_BATCH_DISCARD_SAFE(cache->edges_with_select_id);
			GPU_BATCH_DISCARD_SAFE(cache->verts_with_select_id);
			/* Paint mode selection */
			/* TODO only do that in paint mode. */
			GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_pos_nor);
			GPU_BATCH_DISCARD_SAFE(cache->batch.surface);
			GPU_BATCH_DISCARD_SAFE(cache->batch.wire_loops);
			if (cache->surf_per_mat) {
				for (int i = 0; i < cache->mat_len; i++) {
					GPU_BATCH_DISCARD_SAFE(cache->surf_per_mat[i]);
				}
			}
			GPU_BATCH_DISCARD_SAFE(cache->overlay_paint_edges);
			GPU_BATCH_DISCARD_SAFE(cache->overlay_weight_faces);
			GPU_BATCH_DISCARD_SAFE(cache->overlay_weight_verts);
			/* Because visible UVs depends on edit mode selection, discard everything. */
			mesh_batch_cache_discard_uvedit(cache);
			break;
		case BKE_MESH_BATCH_DIRTY_ALL:
			cache->is_dirty = true;
			break;
		case BKE_MESH_BATCH_DIRTY_SHADING:
			mesh_batch_cache_discard_shaded_tri(cache);
			mesh_batch_cache_discard_uvedit(cache);
			break;
		case BKE_MESH_BATCH_DIRTY_SCULPT_COORDS:
			cache->is_sculpt_points_tag = true;
			break;
		case BKE_MESH_BATCH_DIRTY_UVEDIT_ALL:
			mesh_batch_cache_discard_uvedit(cache);
			break;
		case BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT:
			/* For now same as above. */
			mesh_batch_cache_discard_uvedit(cache);
			break;
		default:
			BLI_assert(0);
	}
}

/**
 * This only clear the batches associated to the given vertex buffer.
 **/
static void mesh_batch_cache_clear_selective(Mesh *me, GPUVertBuf *vert)
{
	MeshBatchCache *cache = me->runtime.batch_cache;
	if (!cache) {
		return;
	}

	BLI_assert(vert != NULL);

	if (ELEM(vert, cache->pos_with_normals, cache->pos_with_normals_visible_only,
	         cache->pos_with_normals_edit, cache->pos_with_normals_visible_only_edit))
	{
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_normals);
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_weights);
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_vert_colors);
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_select_id);
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_select_mask);
		GPU_BATCH_DISCARD_SAFE(cache->points_with_normals);
		GPU_BATCH_DISCARD_SAFE(cache->ledges_with_normals);
		// if (cache->shaded_triangles) {
		// 	for (int i = 0; i < cache->mat_len; i++) {
		// 		GPU_BATCH_DISCARD_SAFE(cache->shaded_triangles[i]);
		// 	}
		// }
		// MEM_SAFE_FREE(cache->shaded_triangles);
		// if (cache->texpaint_triangles) {
		// 	for (int i = 0; i < cache->mat_len; i++) {
		// 		GPU_BATCH_DISCARD_SAFE(cache->texpaint_triangles[i]);
		// 	}
		// }
		MEM_SAFE_FREE(cache->texpaint_triangles);
		GPU_BATCH_DISCARD_SAFE(cache->texpaint_triangles_single);
	}
	/* TODO: add the other ones if needed. */
	else {
		/* Does not match any vertbuf in the batch cache! */
		BLI_assert(0);
	}
}

static void mesh_batch_cache_clear(Mesh *me)
{
	MeshBatchCache *cache = me->runtime.batch_cache;
	if (!cache) {
		return;
	}

	for (int i = 0; i < sizeof(cache->ordered) / sizeof(void *); ++i) {
		GPUVertBuf **vbo = (GPUVertBuf **)&cache->ordered;
		GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
	}
	for (int i = 0; i < sizeof(cache->tess) / sizeof(void *); ++i) {
		GPUVertBuf **vbo = (GPUVertBuf **)&cache->tess;
		GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
	}
	for (int i = 0; i < sizeof(cache->edit) / sizeof(void *); ++i) {
		GPUVertBuf **vbo = (GPUVertBuf **)&cache->edit;
		GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
	}
	for (int i = 0; i < sizeof(cache->ibo) / sizeof(void *); ++i) {
		GPUIndexBuf **ibo = (GPUIndexBuf **)&cache->ibo;
		GPU_INDEXBUF_DISCARD_SAFE(ibo[i]);
	}
	for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); ++i) {
		GPUBatch **batch = (GPUBatch **)&cache->batch;
		GPU_BATCH_DISCARD_SAFE(batch[i]);
	}

	GPU_BATCH_DISCARD_SAFE(cache->all_triangles);

	GPU_INDEXBUF_DISCARD_SAFE(cache->edges_in_order);
	GPU_INDEXBUF_DISCARD_SAFE(cache->triangles_in_order);
	GPU_INDEXBUF_DISCARD_SAFE(cache->ledges_in_order);

	GPU_BATCH_DISCARD_SAFE(cache->overlay_weight_faces);
	GPU_BATCH_DISCARD_SAFE(cache->overlay_weight_verts);
	GPU_BATCH_DISCARD_SAFE(cache->overlay_paint_edges);

	GPU_BATCH_DISCARD_SAFE(cache->triangles_with_normals);
	GPU_BATCH_DISCARD_SAFE(cache->points_with_normals);
	GPU_BATCH_DISCARD_SAFE(cache->ledges_with_normals);
	GPU_VERTBUF_DISCARD_SAFE(cache->pos_with_normals);
	GPU_VERTBUF_DISCARD_SAFE(cache->pos_with_normals_visible_only);
	GPU_VERTBUF_DISCARD_SAFE(cache->pos_with_normals_edit);
	GPU_VERTBUF_DISCARD_SAFE(cache->pos_with_normals_visible_only_edit);
	GPU_BATCH_DISCARD_SAFE(cache->triangles_with_weights);
	GPU_BATCH_DISCARD_SAFE(cache->triangles_with_vert_colors);
	GPU_VERTBUF_DISCARD_SAFE(cache->tri_aligned_uv);
	GPU_VERTBUF_DISCARD_SAFE(cache->ed_fcenter_pos_with_nor_and_sel);
	GPU_VERTBUF_DISCARD_SAFE(cache->ed_edge_pos);
	GPU_VERTBUF_DISCARD_SAFE(cache->ed_vert_pos);
	GPU_BATCH_DISCARD_SAFE(cache->triangles_with_select_mask);
	GPU_BATCH_DISCARD_SAFE(cache->triangles_with_select_id);
	GPU_BATCH_DISCARD_SAFE(cache->facedot_with_select_id);
	GPU_BATCH_DISCARD_SAFE(cache->edges_with_select_id);
	GPU_BATCH_DISCARD_SAFE(cache->verts_with_select_id);

	GPU_BATCH_DISCARD_SAFE(cache->fancy_edges);

	GPU_INDEXBUF_DISCARD_SAFE(cache->edges_adjacency);
	GPU_BATCH_DISCARD_SAFE(cache->edge_detection);

	mesh_batch_cache_discard_shaded_tri(cache);

	mesh_batch_cache_discard_uvedit(cache);

	if (cache->texpaint_triangles) {
		for (int i = 0; i < cache->mat_len; i++) {
			GPU_BATCH_DISCARD_SAFE(cache->texpaint_triangles[i]);
		}
	}
	MEM_SAFE_FREE(cache->texpaint_triangles);

	drw_mesh_weight_state_clear(&cache->weight_state);
}

void DRW_mesh_batch_cache_free(Mesh *me)
{
	mesh_batch_cache_clear(me);
	MEM_SAFE_FREE(me->runtime.batch_cache);
}

/* GPUBatch cache usage. */

static void mesh_create_pos_and_nor_tess(MeshRenderData *rdata, GPUVertBuf *vbo, bool use_hide)
{
	static GPUVertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
		GPU_vertformat_triple_load(&format);
	}

	GPU_vertbuf_init_with_format(vbo, &format);

	const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);
	const int vbo_len_capacity = tri_len * 3;
	int vbo_len_used = 0;
	GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

	GPUVertBufRaw pos_step, nor_step;
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.nor, &nor_step);

	if (rdata->mapped.use == false) {
		float (*lnors)[3] = rdata->loop_normals;
		if (rdata->edit_bmesh) {
			GPUPackedNormal *pnors_pack, *vnors_pack;

			if (lnors == NULL) {
				mesh_render_data_ensure_poly_normals_pack(rdata);
				mesh_render_data_ensure_vert_normals_pack(rdata);

				pnors_pack = rdata->poly_normals_pack;
				vnors_pack = rdata->vert_normals_pack;
			}

			for (int i = 0; i < tri_len; i++) {
				const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				const BMFace *bm_face = bm_looptri[0]->f;

				/* use_hide always for edit-mode */
				if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
					continue;
				}

				if (lnors) {
					for (uint t = 0; t < 3; t++) {
						const float *nor = lnors[BM_elem_index_get(bm_looptri[t])];
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = GPU_normal_convert_i10_v3(nor);
					}
				}
				else if (BM_elem_flag_test(bm_face, BM_ELEM_SMOOTH)) {
					for (uint t = 0; t < 3; t++) {
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = vnors_pack[BM_elem_index_get(bm_looptri[t]->v)];
					}
				}
				else {
					const GPUPackedNormal *snor_pack = &pnors_pack[BM_elem_index_get(bm_face)];
					for (uint t = 0; t < 3; t++) {
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = *snor_pack;
					}
				}

				/* TODO(sybren): deduplicate this and all the other places it's pasted to in this file. */
				if (rdata->edit_data && rdata->edit_data->vertexCos) {
					for (uint t = 0; t < 3; t++) {
						int vidx = BM_elem_index_get(bm_looptri[t]->v);
						const float *pos = rdata->edit_data->vertexCos[vidx];
						copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), pos);
					}
				}
				else {
					for (uint t = 0; t < 3; t++) {
						copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), bm_looptri[t]->v->co);
					}
				}
			}
		}
		else {
			if (lnors == NULL) {
				/* Use normals from vertex. */
				mesh_render_data_ensure_poly_normals_pack(rdata);
			}

			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				const MPoly *mp = &rdata->mpoly[mlt->poly];

				if (use_hide && (mp->flag & ME_HIDE)) {
					continue;
				}

				const uint vtri[3] = {
					rdata->mloop[mlt->tri[0]].v,
					rdata->mloop[mlt->tri[1]].v,
					rdata->mloop[mlt->tri[2]].v,
				};

				if (lnors) {
					for (uint t = 0; t < 3; t++) {
						const float *nor = lnors[mlt->tri[t]];
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = GPU_normal_convert_i10_v3(nor);
					}
				}
				else if (mp->flag & ME_SMOOTH) {
					for (uint t = 0; t < 3; t++) {
						const MVert *mv = &rdata->mvert[vtri[t]];
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = GPU_normal_convert_i10_s3(mv->no);
					}
				}
				else {
					const GPUPackedNormal *pnors_pack = &rdata->poly_normals_pack[mlt->poly];
					for (uint t = 0; t < 3; t++) {
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = *pnors_pack;
					}
				}

				for (uint t = 0; t < 3; t++) {
					const MVert *mv = &rdata->mvert[vtri[t]];
					copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mv->co);
				}
			}
		}
	}
	else {
		/* Note: mapped doesn't support lnors yet. */
		BMesh *bm = rdata->edit_bmesh->bm;
		Mesh *me_cage = rdata->mapped.me_cage;

		/* TODO(campbell): unlike non-mapped modes we don't generate these on demand, just use if they exist.
		 * this seems like a low priority TODO since mapped meshes typically
		 * use the final mesh evaluated mesh for showing faces. */
		const float (*lnors)[3] = CustomData_get_layer(&me_cage->ldata, CD_NORMAL);

		/* TODO(campbell): this is quite an expensive operation for something
		 * that's not used unless 'normal' display option is enabled. */
		if (!CustomData_has_layer(&me_cage->pdata, CD_NORMAL)) {
			/* TODO(campbell): this is quite an expensive operation for something
			 * that's not used unless 'normal' display option is enabled. */
			BKE_mesh_ensure_normals_for_display(me_cage);
		}
		const float (*polynors)[3] = CustomData_get_layer(&me_cage->pdata, CD_NORMAL);

		const MVert *mvert = rdata->mapped.me_cage->mvert;
		const MLoop *mloop = rdata->mapped.me_cage->mloop;
		const MPoly *mpoly = rdata->mapped.me_cage->mpoly;

		const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
		for (int i = 0; i < tri_len; i++) {
			const MLoopTri *mlt = &mlooptri[i];
			const int p_orig = rdata->mapped.p_origindex[mlt->poly];
			if (p_orig != ORIGINDEX_NONE) {
				/* Assume 'use_hide' */
				BMFace *efa = BM_face_at_index(bm, p_orig);
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					const MPoly *mp = &mpoly[mlt->poly];
					const uint vtri[3] = {
						mloop[mlt->tri[0]].v,
						mloop[mlt->tri[1]].v,
						mloop[mlt->tri[2]].v,
					};

					if (lnors) {
						for (uint t = 0; t < 3; t++) {
							const float *nor = lnors[mlt->tri[t]];
							*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = GPU_normal_convert_i10_v3(nor);
						}
					}
					else if (mp->flag & ME_SMOOTH) {
						for (uint t = 0; t < 3; t++) {
							const MVert *mv = &mvert[vtri[t]];
							*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = GPU_normal_convert_i10_s3(mv->no);
						}
					}
					else {
						/* we don't have cached 'rdata->poly_normals_pack'. */
						const GPUPackedNormal pnor = GPU_normal_convert_i10_v3(polynors[mlt->poly]);
						for (uint t = 0; t < 3; t++) {
							*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = pnor;
						}
					}

					for (uint t = 0; t < 3; t++) {
						const MVert *mv = &mvert[vtri[t]];
						copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mv->co);
					}
				}
			}
		}
	}

	vbo_len_used = GPU_vertbuf_raw_used(&pos_step);
	BLI_assert(vbo_len_used == GPU_vertbuf_raw_used(&nor_step));

	if (vbo_len_capacity != vbo_len_used) {
		GPU_vertbuf_data_resize(vbo, vbo_len_used);
	}
}

static GPUVertBuf *mesh_batch_cache_get_tri_pos_and_normals_ex(
        MeshRenderData *rdata, const bool use_hide,
        GPUVertBuf **r_vbo)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (*r_vbo == NULL) {
		*r_vbo = GPU_vertbuf_create(GPU_USAGE_STATIC);
		mesh_create_pos_and_nor_tess(rdata, *r_vbo, use_hide);
	}
	return *r_vbo;
}

static GPUVertBuf *mesh_batch_cache_get_tri_pos_and_normals_edit(
        MeshRenderData *rdata, MeshBatchCache *cache, bool use_hide)
{
	return mesh_batch_cache_get_tri_pos_and_normals_ex(
	        rdata, use_hide,
	        use_hide ? &cache->pos_with_normals_visible_only_edit : &cache->pos_with_normals_edit);
}

/* DEPRECATED Need to be ported */
static GPUVertBuf *mesh_batch_cache_get_facedot_pos_with_normals_and_flag(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	if (cache->edit.pos_nor_data_facedots == NULL) {
		cache->edit.pos_nor_data_facedots = GPU_vertbuf_create(GPU_USAGE_STATIC);
		mesh_create_edit_facedots(rdata, cache->edit.pos_nor_data_facedots);
	}

	return cache->edit.pos_nor_data_facedots;
}

static GPUVertBuf *mesh_batch_cache_get_edges_visible(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE));

	if (cache->ed_edge_pos == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos, data; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		const int vbo_len_capacity = mesh_render_data_edges_len_get_maybe_mapped(rdata) * 2;
		int vidx = 0;

		GPUVertBuf *vbo = cache->ed_edge_pos = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (rdata->mapped.use == false) {
			if (rdata->edit_bmesh) {
				BMesh *bm = rdata->edit_bmesh->bm;
				BMIter iter;
				BMEdge *eed;

				BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, eed->v1->co);
						vidx += 1;
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, eed->v2->co);
						vidx += 1;
					}
				}
			}
			else {
				/* not yet done! */
				BLI_assert(0);
			}
		}
		else {
			BMesh *bm = rdata->edit_bmesh->bm;
			const MVert *mvert = rdata->mapped.me_cage->mvert;
			const MEdge *medge = rdata->mapped.me_cage->medge;
			const int *e_origindex = rdata->mapped.e_origindex;
			for (int i = 0; i < rdata->mapped.edge_len; i++) {
				const int e_orig = e_origindex[i];
				if (e_orig != ORIGINDEX_NONE) {
					BMEdge *eed = BM_edge_at_index(bm, e_orig);
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						const MEdge *ed = &medge[i];
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, mvert[ed->v1].co);
						vidx += 1;
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, mvert[ed->v2].co);
						vidx += 1;
					}
				}
			}
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}
		UNUSED_VARS_NDEBUG(vbo_len_used);
	}

	return cache->ed_edge_pos;
}

static GPUVertBuf *mesh_batch_cache_get_verts_visible(
        MeshRenderData *rdata, MeshBatchCache *cache)
{
	BLI_assert(rdata->types & MR_DATATYPE_VERT);

	if (cache->ed_vert_pos == NULL) {
		static GPUVertFormat format = { 0 };
		static struct { uint pos, data; } attr_id;
		if (format.attr_len == 0) {
			attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		}

		const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);
		uint vidx = 0;

		GPUVertBuf *vbo = cache->ed_vert_pos = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		if (rdata->mapped.use == false) {
			if (rdata->edit_bmesh) {
				BMesh *bm = rdata->edit_bmesh->bm;
				BMIter iter;
				BMVert *eve;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, eve->co);
						vidx += 1;
					}
				}
			}
			else {
				for (int i = 0; i < vbo_len_capacity; i++) {
					const MVert *mv = &rdata->mvert[i];
					if (!(mv->flag & ME_HIDE)) {
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, mv->co);
						vidx += 1;
					}
				}
			}
		}
		else {
			BMesh *bm = rdata->edit_bmesh->bm;
			const MVert *mvert = rdata->mapped.me_cage->mvert;
			const int *v_origindex = rdata->mapped.v_origindex;
			for (int i = 0; i < vbo_len_capacity; i++) {
				const int v_orig = v_origindex[i];
				if (v_orig != ORIGINDEX_NONE) {
					BMVert *eve = BM_vert_at_index(bm, v_orig);
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						const MVert *mv = &mvert[i];
						GPU_vertbuf_attr_set(vbo, attr_id.pos, vidx, mv->co);
						vidx += 1;
					}
				}
			}
		}
		const uint vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}

		UNUSED_VARS_NDEBUG(vbo_len_used);
	}

	return cache->ed_vert_pos;
}

static GPUVertBuf *mesh_create_facedot_select_id(
        MeshRenderData *rdata, uint select_id_offset)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	GPUVertBuf *vbo;
	{
		static GPUVertFormat format = { 0 };
		static struct { uint pos, col; } attr_id;
		if (format.attr_len == 0) {
			attr_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_I32, 1, GPU_FETCH_INT);
		}

		const int vbo_len_capacity = mesh_render_data_polys_len_get(rdata);
		int vidx = 0;

		vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
		uint select_index = select_id_offset;

		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter;
			BMEdge *efa;

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					int select_id;
					GPU_select_index_get(select_index, &select_id);
					GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
					vidx += 1;
				}
				select_index += 1;
			}
		}
		else {
			/* not yet done! */
			BLI_assert(0);
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return vbo;
}

static GPUVertBuf *mesh_create_edges_select_id(
        MeshRenderData *rdata, uint select_id_offset)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_EDGE));

	GPUVertBuf *vbo;
	{
		static GPUVertFormat format = { 0 };
		static struct { uint pos, col; } attr_id;
		if (format.attr_len == 0) {
			attr_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_I32, 1, GPU_FETCH_INT);
		}

		const int vbo_len_capacity = mesh_render_data_edges_len_get_maybe_mapped(rdata) * 2;
		int vidx = 0;

		vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (rdata->mapped.use == false) {
			uint select_index = select_id_offset;
			if (rdata->edit_bmesh) {
				BMesh *bm = rdata->edit_bmesh->bm;
				BMIter iter;
				BMEdge *eed;

				BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						int select_id;
						GPU_select_index_get(select_index, &select_id);
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
					}
					select_index += 1;
				}
			}
			else {
				/* not yet done! */
				BLI_assert(0);
			}
		}
		else {
			BMesh *bm = rdata->edit_bmesh->bm;
			const int *e_origindex = rdata->mapped.e_origindex;
			for (int i = 0; i < rdata->mapped.edge_len; i++) {
				const int e_orig = e_origindex[i];
				if (e_orig != ORIGINDEX_NONE) {
					BMEdge *eed = BM_edge_at_index(bm, e_orig);
					if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
						uint select_index = select_id_offset + e_orig;
						int select_id;
						GPU_select_index_get(select_index, &select_id);
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
					}
				}
			}
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return vbo;
}

static GPUVertBuf *mesh_create_verts_select_id(
        MeshRenderData *rdata, uint select_id_offset)
{
	BLI_assert(rdata->types & (MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	GPUVertBuf *vbo;
	{
		static GPUVertFormat format = { 0 };
		static struct { uint pos, col; } attr_id;
		if (format.attr_len == 0) {
			attr_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_I32, 1, GPU_FETCH_INT);
		}

		const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);
		int vidx = 0;

		vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

		if (rdata->mapped.use == false) {
			uint select_index = select_id_offset;
			if (rdata->edit_bmesh) {
				BMesh *bm = rdata->edit_bmesh->bm;
				BMIter iter;
				BMVert *eve;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						int select_id;
						GPU_select_index_get(select_index, &select_id);
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
					}
					select_index += 1;
				}
			}
			else {
				for (int i = 0; i < vbo_len_capacity; i++) {
					const MVert *mv = &rdata->mvert[i];
					if (!(mv->flag & ME_HIDE)) {
						int select_id;
						GPU_select_index_get(select_index, &select_id);
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
					}
					select_index += 1;
				}
			}
		}
		else {
			BMesh *bm = rdata->edit_bmesh->bm;
			const int *v_origindex = rdata->mapped.v_origindex;
			for (int i = 0; i < vbo_len_capacity; i++) {
				const int v_orig = v_origindex[i];
				if (v_orig != ORIGINDEX_NONE) {
					BMVert *eve = BM_vert_at_index(bm, v_orig);
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						uint select_index = select_id_offset + v_orig;
						int select_id;
						GPU_select_index_get(select_index, &select_id);
						GPU_vertbuf_attr_set(vbo, attr_id.col, vidx, &select_id);
						vidx += 1;
					}
				}
			}
		}
		const int vbo_len_used = vidx;
		if (vbo_len_used != vbo_len_capacity) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}

	return vbo;
}

static GPUVertBuf *mesh_create_tri_select_id(
        MeshRenderData *rdata, bool use_hide, uint select_id_offset)
{
	BLI_assert(
	        rdata->types &
	        (MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY));

	GPUVertBuf *vbo;
	{
		uint cidx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint col; } attr_id;
		if (format.attr_len == 0) {
			attr_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_I32, 1, GPU_FETCH_INT);
		}

		const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);

		vbo = GPU_vertbuf_create_with_format(&format);

		const int vbo_len_capacity = tri_len * 3;
		int vbo_len_used = 0;
		GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);


		if (rdata->mapped.use == false) {
			if (rdata->edit_bmesh) {
				for (int i = 0; i < tri_len; i++) {
					const BMLoop **ltri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
					/* Assume 'use_hide' */
					if (!BM_elem_flag_test(ltri[0]->f, BM_ELEM_HIDDEN)) {
						const int poly_index = BM_elem_index_get(ltri[0]->f);
						int select_id;
						GPU_select_index_get(poly_index + select_id_offset, &select_id);
						for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
							GPU_vertbuf_attr_set(vbo, attr_id.col, cidx++, &select_id);
						}
					}
				}
			}
			else {
				const int *p_origindex = NULL;
				if (rdata->me != NULL) {
					p_origindex = CustomData_get_layer(&rdata->me->pdata, CD_ORIGINDEX);
				}

				for (int i = 0; i < tri_len; i++) {
					const MLoopTri *mlt = &rdata->mlooptri[i];
					const int poly_index = mlt->poly;
					if (!(use_hide && (rdata->mpoly[poly_index].flag & ME_HIDE))) {
						int orig_index = p_origindex ? p_origindex[poly_index] : poly_index;
						if (orig_index != ORIGINDEX_NONE) {
							int select_id;
							GPU_select_index_get(orig_index + select_id_offset, &select_id);
							for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
								GPU_vertbuf_attr_set(vbo, attr_id.col, cidx++, &select_id);
							}
						}
					}
				}
			}
		}
		else {
			BMesh *bm = rdata->edit_bmesh->bm;
			Mesh *me_cage = rdata->mapped.me_cage;
			const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &mlooptri[i];
				const int p_orig = rdata->mapped.p_origindex[mlt->poly];
				if (p_orig != ORIGINDEX_NONE) {
					/* Assume 'use_hide' */
					BMFace *efa = BM_face_at_index(bm, p_orig);
					if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
						int select_id;
						GPU_select_index_get(select_id_offset + p_orig, &select_id);
						for (uint tri_corner = 0; tri_corner < 3; tri_corner++) {
							GPU_vertbuf_attr_set(vbo, attr_id.col, cidx++, &select_id);
						}
					}
				}
			}
		}
		vbo_len_used = cidx;

		if (vbo_len_capacity != vbo_len_used) {
			GPU_vertbuf_data_resize(vbo, vbo_len_used);
		}
	}
	return vbo;
}

static void mesh_create_pos_and_nor(MeshRenderData *rdata, GPUVertBuf *vbo)
{
	static GPUVertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}

	GPU_vertbuf_init_with_format(vbo, &format);
	const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);
	GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter;
			BMVert *eve;
			uint i;

			mesh_render_data_ensure_vert_normals_pack(rdata);
			GPUPackedNormal *vnor = rdata->vert_normals_pack;

			BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i, eve->co);
				GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor[i]);
			}
			BLI_assert(i == vbo_len_capacity);
		}
		else {
			for (int i = 0; i < vbo_len_capacity; i++) {
				const MVert *mv = &rdata->mvert[i];
				GPUPackedNormal vnor_pack = GPU_normal_convert_i10_s3(mv->no);
				vnor_pack.w = (mv->flag & ME_HIDE) ? -1 : ((mv->flag & SELECT) ? 1 : 0);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i, rdata->mvert[i].co);
				GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor_pack);
			}
		}
	}
	else {
		const MVert *mvert = rdata->mapped.me_cage->mvert;
		const int *v_origindex = rdata->mapped.v_origindex;
		for (int i = 0; i < vbo_len_capacity; i++) {
			const int v_orig = v_origindex[i];
			if (v_orig != ORIGINDEX_NONE) {
				const MVert *mv = &mvert[i];
				GPUPackedNormal vnor_pack = GPU_normal_convert_i10_s3(mv->no);
				vnor_pack.w = (mv->flag & ME_HIDE) ? -1 : ((mv->flag & SELECT) ? 1 : 0);
				GPU_vertbuf_attr_set(vbo, attr_id.pos, i, mv->co);
				GPU_vertbuf_attr_set(vbo, attr_id.nor, i, &vnor_pack);
			}
		}
	}
}

static void mesh_create_weights(MeshRenderData *rdata, GPUVertBuf *vbo, DRW_MeshWeightState *wstate)
{
	static GPUVertFormat format = { 0 };
	static struct { uint weight; } attr_id;
	if (format.attr_len == 0) {
		attr_id.weight = GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	}

	const int vbo_len_capacity = mesh_render_data_verts_len_get_maybe_mapped(rdata);

	mesh_render_data_ensure_vert_weight(rdata, wstate);
	const float *vert_weight = rdata->vert_weight;

	GPU_vertbuf_init_with_format(vbo, &format);
	/* Meh, another allocation / copy for no benefit.
	 * Needed because rdata->vert_weight is freed afterwards and
	 * GPU module don't have a GPU_vertbuf_data_from_memory or similar. */
	/* TODO get rid of the extra allocation/copy. */
	GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);
	GPU_vertbuf_attr_fill(vbo, attr_id.weight, vert_weight);
}

static void mesh_create_loop_pos_and_nor(MeshRenderData *rdata, GPUVertBuf *vbo, const bool use_face_sel)
{
	/* TODO deduplicate format creation*/
	static GPUVertFormat format = { 0 };
	static struct { uint pos, nor; } attr_id;
	if (format.attr_len == 0) {
		attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		attr_id.nor = GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}
	const int poly_len = mesh_render_data_polys_len_get(rdata);
	const int loop_len = mesh_render_data_loops_len_get(rdata);

	GPU_vertbuf_init_with_format(vbo, &format);
	GPU_vertbuf_data_alloc(vbo, loop_len);

	GPUVertBufRaw pos_step, nor_step;
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.pos, &pos_step);
	GPU_vertbuf_attr_get_raw_data(vbo, attr_id.nor, &nor_step);

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			const GPUPackedNormal *vnor, *pnor;
			const float (*lnors)[3] = rdata->loop_normals;
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter_efa, iter_loop;
			BMFace *efa;
			BMLoop *loop;
			uint f;

			if (rdata->loop_normals == NULL) {
				mesh_render_data_ensure_poly_normals_pack(rdata);
				mesh_render_data_ensure_vert_normals_pack(rdata);
				vnor = rdata->vert_normals_pack;
				pnor = rdata->poly_normals_pack;
			}

			BM_ITER_MESH_INDEX (efa, &iter_efa, bm, BM_FACES_OF_MESH, f) {
				const bool face_smooth = BM_elem_flag_test(efa, BM_ELEM_SMOOTH);

				BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
					BLI_assert(GPU_vertbuf_raw_used(&pos_step) == BM_elem_index_get(loop));
					copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), loop->v->co);

					if (lnors) {
						GPUPackedNormal plnor = GPU_normal_convert_i10_v3(lnors[BM_elem_index_get(loop)]);
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = plnor;
					}
					else if (!face_smooth) {
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = pnor[f];
					}
					else {
						*((GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step)) = vnor[BM_elem_index_get(loop->v)];
					}
				}
			}
			BLI_assert(GPU_vertbuf_raw_used(&pos_step) == loop_len);
		}
		else {
			const MVert *mvert = rdata->mvert;
			const MPoly *mpoly = rdata->mpoly;

			if (rdata->loop_normals == NULL) {
				mesh_render_data_ensure_poly_normals_pack(rdata);
			}

			for (int a = 0; a < poly_len; a++, mpoly++) {
				const MLoop *mloop = rdata->mloop + mpoly->loopstart;
				const float (*lnors)[3] = (rdata->loop_normals) ? &rdata->loop_normals[mpoly->loopstart] : NULL;
				const GPUPackedNormal *fnor = (mpoly->flag & ME_SMOOTH) ? NULL : &rdata->poly_normals_pack[a];
				for (int b = 0; b < mpoly->totloop; b++, mloop++) {
					copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop->v].co);
					GPUPackedNormal *pnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step);
					if (lnors) {
						*pnor = GPU_normal_convert_i10_v3(lnors[b]);
					}
					else if (fnor) {
						*pnor = *fnor;
					}
					else {
						*pnor = GPU_normal_convert_i10_s3(mvert[mloop->v].no);
					}
					if (use_face_sel) {
						pnor->w = (mpoly->flag & ME_HIDE) ? -1 : ((mpoly->flag & ME_FACE_SEL) ? 1 : 0);
					}
				}
			}

			BLI_assert(loop_len == GPU_vertbuf_raw_used(&pos_step));
		}
	}
	else {
		const int *p_origindex = rdata->mapped.p_origindex;
		const MVert *mvert = rdata->mvert;
		const MPoly *mpoly = rdata->mpoly;

		if (rdata->loop_normals == NULL) {
			mesh_render_data_ensure_poly_normals_pack(rdata);
		}

		for (int a = 0; a < poly_len; a++, mpoly++) {
			const MLoop *mloop = rdata->mloop + mpoly->loopstart;
			const float (*lnors)[3] = (rdata->loop_normals) ? &rdata->loop_normals[mpoly->loopstart] : NULL;
			const GPUPackedNormal *fnor = (mpoly->flag & ME_SMOOTH) ? NULL : &rdata->poly_normals_pack[a];
			if (p_origindex[a] == ORIGINDEX_NONE) {
				continue;
			}
			for (int b = 0; b < mpoly->totloop; b++, mloop++) {
				copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), mvert[mloop->v].co);
				GPUPackedNormal *pnor = (GPUPackedNormal *)GPU_vertbuf_raw_step(&nor_step);
				if (lnors) {
					*pnor = GPU_normal_convert_i10_v3(lnors[b]);
				}
				else if (fnor) {
					*pnor = *fnor;
				}
				else {
					*pnor = GPU_normal_convert_i10_s3(mvert[mloop->v].no);
				}
			}
		}
	}

	int vbo_len_used = GPU_vertbuf_raw_used(&pos_step);
	if (vbo_len_used < loop_len) {
		GPU_vertbuf_data_resize(vbo, vbo_len_used);
	}
}

static void mesh_create_loop_uv_and_tan(MeshRenderData *rdata, GPUVertBuf *vbo)
{
	const uint loops_len = mesh_render_data_loops_len_get(rdata);
	const uint uv_len = rdata->cd.layers.uv_len;
	const uint tangent_len = rdata->cd.layers.tangent_len;
	const uint layers_combined_len = uv_len + tangent_len;

	GPUVertBufRaw *layers_combined_step = BLI_array_alloca(layers_combined_step, layers_combined_len);
	GPUVertBufRaw *uv_step      = layers_combined_step;
	GPUVertBufRaw *tangent_step = uv_step + uv_len;

	uint *layers_combined_id = BLI_array_alloca(layers_combined_id, layers_combined_len);
	uint *uv_id = layers_combined_id;
	uint *tangent_id = uv_id + uv_len;

	/* initialize vertex format */
	GPUVertFormat format = { 0 };

	for (uint i = 0; i < uv_len; i++) {
		const char *attrib_name = mesh_render_data_uv_layer_uuid_get(rdata, i);
#if 0 /* these are clamped. Maybe use them as an option in the future */
		uv_id[i] = GPU_vertformat_attr_add(&format, attrib_name, GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
#else
		uv_id[i] = GPU_vertformat_attr_add(&format, attrib_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
#endif
		/* Auto Name */
		attrib_name = mesh_render_data_uv_auto_layer_uuid_get(rdata, i);
		GPU_vertformat_alias_add(&format, attrib_name);

		if (i == rdata->cd.layers.uv_active) {
			GPU_vertformat_alias_add(&format, "u");
		}
	}

	for (uint i = 0; i < tangent_len; i++) {
		const char *attrib_name = mesh_render_data_tangent_layer_uuid_get(rdata, i);
#ifdef USE_COMP_MESH_DATA
		tangent_id[i] = GPU_vertformat_attr_add(&format, attrib_name, GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
#else
		tangent_id[i] = GPU_vertformat_attr_add(&format, attrib_name, GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
#endif
		if (i == rdata->cd.layers.tangent_active) {
			GPU_vertformat_alias_add(&format, "t");
		}
	}

	GPU_vertbuf_init_with_format(vbo, &format);
	GPU_vertbuf_data_alloc(vbo, loops_len);

	for (uint i = 0; i < uv_len; i++) {
		GPU_vertbuf_attr_get_raw_data(vbo, uv_id[i], &uv_step[i]);
	}
	for (uint i = 0; i < tangent_len; i++) {
		GPU_vertbuf_attr_get_raw_data(vbo, tangent_id[i], &tangent_step[i]);
	}

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMIter iter_efa, iter_loop;
		BMFace *efa;
		BMLoop *loop;

		BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
				/* UVs */
				for (uint j = 0; j < uv_len; j++) {
					const uint layer_offset = rdata->cd.offset.uv[j];
					const float *elem = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(loop, layer_offset))->uv;
					copy_v2_v2(GPU_vertbuf_raw_step(&uv_step[j]), elem);
				}
				/* TANGENTs */
				for (uint j = 0; j < tangent_len; j++) {
					float (*layer_data)[4] = rdata->cd.layers.tangent[j];
					const float *elem = layer_data[BM_elem_index_get(loop)];
#ifdef USE_COMP_MESH_DATA
					normal_float_to_short_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#else
					copy_v4_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#endif
				}
			}
		}
	}
	else {
		for (uint loop = 0; loop < loops_len; loop++) {
			/* UVs */
			for (uint j = 0; j < uv_len; j++) {
				const MLoopUV *layer_data = rdata->cd.layers.uv[j];
				const float *elem = layer_data[loop].uv;
				copy_v2_v2(GPU_vertbuf_raw_step(&uv_step[j]), elem);
			}
			/* TANGENTs */
			for (uint j = 0; j < tangent_len; j++) {
				float (*layer_data)[4] = rdata->cd.layers.tangent[j];
				const float *elem = layer_data[loop];
#ifdef USE_COMP_MESH_DATA
				normal_float_to_short_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#else
				copy_v4_v4(GPU_vertbuf_raw_step(&tangent_step[j]), elem);
#endif
			}
		}
	}

#ifndef NDEBUG
	/* Check all layers are write aligned. */
	if (layers_combined_len > 0) {
		int vbo_len_used = GPU_vertbuf_raw_used(&layers_combined_step[0]);
		for (uint i = 0; i < layers_combined_len; i++) {
			BLI_assert(vbo_len_used == GPU_vertbuf_raw_used(&layers_combined_step[i]));
		}
	}
#endif

#undef USE_COMP_MESH_DATA
}

static void mesh_create_loop_vcol(MeshRenderData *rdata, GPUVertBuf *vbo)
{
	const uint loops_len = mesh_render_data_loops_len_get(rdata);
	const uint vcol_len = rdata->cd.layers.vcol_len;

	GPUVertBufRaw *vcol_step = BLI_array_alloca(vcol_step, vcol_len);
	uint *vcol_id = BLI_array_alloca(vcol_id, vcol_len);

	/* initialize vertex format */
	GPUVertFormat format = { 0 };

	for (uint i = 0; i < vcol_len; i++) {
		const char *attrib_name = mesh_render_data_vcol_layer_uuid_get(rdata, i);
		vcol_id[i] = GPU_vertformat_attr_add(&format, attrib_name, GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
		/* Auto layer */
		if (rdata->cd.layers.auto_vcol[i]) {
			attrib_name = mesh_render_data_vcol_auto_layer_uuid_get(rdata, i);
			GPU_vertformat_alias_add(&format, attrib_name);
		}
		if (i == rdata->cd.layers.vcol_active) {
			GPU_vertformat_alias_add(&format, "c");
		}
	}

	GPU_vertbuf_init_with_format(vbo, &format);
	GPU_vertbuf_data_alloc(vbo, loops_len);

	for (uint i = 0; i < vcol_len; i++) {
		GPU_vertbuf_attr_get_raw_data(vbo, vcol_id[i], &vcol_step[i]);
	}

	if (rdata->edit_bmesh) {
		BMesh *bm = rdata->edit_bmesh->bm;
		BMIter iter_efa, iter_loop;
		BMFace *efa;
		BMLoop *loop;

		BM_ITER_MESH (efa, &iter_efa, bm, BM_FACES_OF_MESH) {
			BM_ITER_ELEM (loop, &iter_loop, efa, BM_LOOPS_OF_FACE) {
				for (uint j = 0; j < vcol_len; j++) {
					const uint layer_offset = rdata->cd.offset.vcol[j];
					const uchar *elem = &((MLoopCol *)BM_ELEM_CD_GET_VOID_P(loop, layer_offset))->r;
					copy_v3_v3_uchar(GPU_vertbuf_raw_step(&vcol_step[j]), elem);
				}
			}
		}
	}
	else {
		for (uint loop = 0; loop < loops_len; loop++) {
			for (uint j = 0; j < vcol_len; j++) {
				const MLoopCol *layer_data = rdata->cd.layers.vcol[j];
				const uchar *elem = &layer_data[loop].r;
				copy_v3_v3_uchar(GPU_vertbuf_raw_step(&vcol_step[j]), elem);
			}
		}
	}

#ifndef NDEBUG
	/* Check all layers are write aligned. */
	if (vcol_len > 0) {
		int vbo_len_used = GPU_vertbuf_raw_used(&vcol_step[0]);
		for (uint i = 0; i < vcol_len; i++) {
			BLI_assert(vbo_len_used == GPU_vertbuf_raw_used(&vcol_step[i]));
		}
	}
#endif

#undef USE_COMP_MESH_DATA
}

static GPUVertFormat *edit_mesh_pos_nor_format(uint *r_pos_id, uint *r_nor_id)
{
	static GPUVertFormat format_pos_nor = { 0 };
	static uint pos_id, nor_id;
	if (format_pos_nor.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format_pos_nor, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		nor_id = GPU_vertformat_attr_add(&format_pos_nor, "vnor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}
	*r_pos_id = pos_id;
	*r_nor_id = nor_id;
	return &format_pos_nor;
}

static GPUVertFormat *edit_mesh_lnor_format(uint *r_lnor_id)
{
	static GPUVertFormat format_lnor = { 0 };
	static uint lnor_id;
	if (format_lnor.attr_len == 0) {
		lnor_id = GPU_vertformat_attr_add(&format_lnor, "lnor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}
	*r_lnor_id = lnor_id;
	return &format_lnor;
}

static GPUVertFormat *edit_mesh_data_format(uint *r_data_id)
{
	static GPUVertFormat format_flag = { 0 };
	static uint data_id;
	if (format_flag.attr_len == 0) {
		data_id = GPU_vertformat_attr_add(&format_flag, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
		GPU_vertformat_triple_load(&format_flag);
	}
	*r_data_id = data_id;
	return &format_flag;
}

static GPUVertFormat *edit_mesh_facedot_format(uint *r_pos_id, uint *r_nor_flag_id)
{
	static GPUVertFormat format_facedots = { 0 };
	static uint pos_id, nor_flag_id;
	if (format_facedots.attr_len == 0) {
		pos_id = GPU_vertformat_attr_add(&format_facedots, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		nor_flag_id = GPU_vertformat_attr_add(&format_facedots, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
	}
	*r_pos_id = pos_id;
	*r_nor_flag_id = nor_flag_id;
	return &format_facedots;
}

static void mesh_create_edit_tris_and_verts(
        MeshRenderData *rdata,
        GPUVertBuf *vbo_data, GPUVertBuf *vbo_pos_nor, GPUVertBuf *vbo_lnor, GPUIndexBuf *ibo_verts)
{
	BMesh *bm = rdata->edit_bmesh->bm;
	BMIter iter;
	BMVert *ev;
	const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);
	int tri_len_used = 0;
	int points_len = bm->totvert;
	int verts_tri_len = tri_len * 3;
	struct { uint pos, vnor, lnor, data; } attr_id;
	GPUVertFormat *pos_nor_format = edit_mesh_pos_nor_format(&attr_id.pos, &attr_id.vnor);
	GPUVertFormat *data_format = edit_mesh_data_format(&attr_id.data);
	GPUVertFormat *lnor_format = edit_mesh_lnor_format(&attr_id.lnor);

	/* Positions & Vert Normals */
	if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor)) {
		GPU_vertbuf_init_with_format(vbo_pos_nor, pos_nor_format);
		GPU_vertbuf_data_alloc(vbo_pos_nor, verts_tri_len);
	}
	/* Overlay data */
	if (DRW_TEST_ASSIGN_VBO(vbo_data)) {
		GPU_vertbuf_init_with_format(vbo_data, data_format);
		GPU_vertbuf_data_alloc(vbo_data, verts_tri_len);
	}
	/* Loop Normals */
	if (DRW_TEST_ASSIGN_VBO(vbo_lnor)) {
		GPU_vertbuf_init_with_format(vbo_lnor, lnor_format);
		GPU_vertbuf_data_alloc(vbo_lnor, verts_tri_len);
	}
	/* Verts IBO */
	GPUIndexBufBuilder elb, *elbp = NULL;
	if (DRW_TEST_ASSIGN_IBO(ibo_verts)) {
		elbp = &elb;
		GPU_indexbuf_init(elbp, GPU_PRIM_POINTS, points_len, verts_tri_len);
		/* Clear tag */
		BM_ITER_MESH(ev, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_disable(ev, BM_ELEM_TAG);
		}
	}

	if (rdata->mapped.use == false) {
		for (int i = 0; i < tri_len; i++) {
			const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
			if (!BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
				add_edit_tri(rdata, vbo_pos_nor, vbo_lnor, vbo_data, elbp,
				             attr_id.pos, attr_id.vnor, attr_id.lnor, attr_id.data,
				             bm_looptri, tri_len_used);
				tri_len_used += 3;
			}
		}
	}
	else {
		Mesh *me_cage = rdata->mapped.me_cage;

		/* TODO(fclem): Maybe move data generation to mesh_render_data_create() */
		const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
		if (vbo_lnor && !CustomData_has_layer(&me_cage->pdata, CD_NORMAL)) {
			BKE_mesh_ensure_normals_for_display(me_cage);
		}
		const float (*polynors)[3] = CustomData_get_layer(&me_cage->pdata, CD_NORMAL);
		const float (*loopnors)[3] = CustomData_get_layer(&me_cage->ldata, CD_NORMAL);

		for (int i = 0; i < tri_len; i++) {
			const MLoopTri *mlt = &mlooptri[i];
			const int p_orig = rdata->mapped.p_origindex[mlt->poly];
			if (p_orig != ORIGINDEX_NONE) {
				BMFace *efa = BM_face_at_index(bm, p_orig);
				if (add_edit_tri_mapped(rdata, vbo_pos_nor, vbo_lnor, vbo_data, elbp,
				                        attr_id.pos, attr_id.vnor, attr_id.lnor, attr_id.data,
				                        efa, mlt, polynors, loopnors, tri_len_used))
				{
					tri_len_used += 3;
				}
			}
		}
	}

	/* Resize & Finish */
	if (elbp != NULL) {
		GPU_indexbuf_build_in_place(elbp, ibo_verts);
	}
	if (tri_len_used != verts_tri_len) {
		if (vbo_pos_nor != NULL) {
			GPU_vertbuf_data_resize(vbo_pos_nor, tri_len_used);
		}
		if (vbo_lnor != NULL) {
			GPU_vertbuf_data_resize(vbo_lnor, tri_len_used);
		}
		if (vbo_data != NULL) {
			GPU_vertbuf_data_resize(vbo_data, tri_len_used);
		}
	}
}

static void mesh_create_edit_loose_edges(
        MeshRenderData *rdata,
        GPUVertBuf *vbo_data_ledges, GPUVertBuf *vbo_pos_nor_ledges)
{
	BMesh *bm = rdata->edit_bmesh->bm;
	const int loose_edge_len = mesh_render_data_loose_edges_len_get_maybe_mapped(rdata);
	const int verts_ledges_len = loose_edge_len * 2;
	int ledges_len_used = 0;

	struct { uint pos, vnor, data; } attr_id;
	GPUVertFormat *pos_nor_format = edit_mesh_pos_nor_format(&attr_id.pos, &attr_id.vnor);
	GPUVertFormat *data_format = edit_mesh_data_format(&attr_id.data);

	/* Positions & Vert Normals */
	if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor_ledges)) {
		GPU_vertbuf_init_with_format(vbo_pos_nor_ledges, pos_nor_format);
		GPU_vertbuf_data_alloc(vbo_pos_nor_ledges, verts_ledges_len);
	}
	/* Overlay data */
	if (DRW_TEST_ASSIGN_VBO(vbo_data_ledges)) {
		GPU_vertbuf_init_with_format(vbo_data_ledges, data_format);
		GPU_vertbuf_data_alloc(vbo_data_ledges, verts_ledges_len);
	}

	if (rdata->mapped.use == false) {
		for (uint i = 0; i < loose_edge_len; i++) {
			const BMEdge *eed = BM_edge_at_index(bm, rdata->loose_edges[i]);
			add_edit_loose_edge(rdata, vbo_pos_nor_ledges, vbo_data_ledges,
			                    attr_id.pos, attr_id.vnor, attr_id.data,
			                    eed, ledges_len_used);
			ledges_len_used += 2;
		}
	}
	else {
		Mesh *me_cage = rdata->mapped.me_cage;
		const MVert *mvert = me_cage->mvert;
		const MEdge *medge = me_cage->medge;
		const int *e_origindex = rdata->mapped.e_origindex;

		for (uint i_iter = 0; i_iter < loose_edge_len; i_iter++) {
			const int i = rdata->mapped.loose_edges[i_iter];
			const int e_orig = e_origindex[i];
			BMEdge *eed = BM_edge_at_index(bm, e_orig);
			add_edit_loose_edge_mapped(rdata, vbo_pos_nor_ledges, vbo_data_ledges,
			                           attr_id.pos, attr_id.vnor, attr_id.data,
			                           eed, mvert, &medge[i], ledges_len_used);
			ledges_len_used += 2;
		}
	}
	BLI_assert(ledges_len_used == verts_ledges_len);
}

static void mesh_create_edit_loose_verts(
        MeshRenderData *rdata,
        GPUVertBuf *vbo_data_lverts, GPUVertBuf *vbo_pos_nor_lverts)
{
	BMesh *bm = rdata->edit_bmesh->bm;
	const int loose_verts_len = mesh_render_data_loose_verts_len_get_maybe_mapped(rdata);
	const int verts_lverts_len = loose_verts_len;
	int lverts_len_used = 0;

	struct { uint pos, vnor, data; } attr_id;
	GPUVertFormat *pos_nor_format = edit_mesh_pos_nor_format(&attr_id.pos, &attr_id.vnor);
	GPUVertFormat *data_format = edit_mesh_data_format(&attr_id.data);

	/* Positions & Vert Normals */
	if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor_lverts)) {
		GPU_vertbuf_init_with_format(vbo_pos_nor_lverts, pos_nor_format);
		GPU_vertbuf_data_alloc(vbo_pos_nor_lverts, verts_lverts_len);
	}
	/* Overlay data */
	if (DRW_TEST_ASSIGN_VBO(vbo_data_lverts)) {
		GPU_vertbuf_init_with_format(vbo_data_lverts, data_format);
		GPU_vertbuf_data_alloc(vbo_data_lverts, verts_lverts_len);
	}

	if (rdata->mapped.use == false) {
		for (uint i = 0; i < loose_verts_len; i++) {
			BMVert *eve = BM_vert_at_index(bm, rdata->loose_verts[i]);
			add_edit_loose_vert(rdata, vbo_pos_nor_lverts, vbo_data_lverts,
			                    attr_id.pos, attr_id.vnor, attr_id.data,
			                    eve, lverts_len_used);
			lverts_len_used += 1;
		}
	}
	else {
		Mesh *me_cage = rdata->mapped.me_cage;
		const MVert *mvert = me_cage->mvert;
		const int *v_origindex = rdata->mapped.v_origindex;

		for (uint i_iter = 0; i_iter < loose_verts_len; i_iter++) {
			const int i = rdata->mapped.loose_verts[i_iter];
			const int v_orig = v_origindex[i];
			BMVert *eve = BM_vert_at_index(bm, v_orig);
			add_edit_loose_vert_mapped(rdata, vbo_pos_nor_lverts, vbo_data_lverts,
			                           attr_id.pos, attr_id.vnor, attr_id.data,
			                           eve, &mvert[i], lverts_len_used);
			lverts_len_used += 1;
		}
	}
	BLI_assert(lverts_len_used == verts_lverts_len);
}

static void mesh_create_edit_facedots(
        MeshRenderData *rdata,
        GPUVertBuf *vbo_pos_nor_data_facedots)
{
	const int poly_len = mesh_render_data_polys_len_get_maybe_mapped(rdata);
	const int verts_facedot_len = poly_len;
	int facedot_len_used = 0;

	struct { uint fdot_pos, fdot_nor_flag; } attr_id;
	GPUVertFormat *facedot_format = edit_mesh_facedot_format(&attr_id.fdot_pos, &attr_id.fdot_nor_flag);

	if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor_data_facedots)) {
		GPU_vertbuf_init_with_format(vbo_pos_nor_data_facedots, facedot_format);
		GPU_vertbuf_data_alloc(vbo_pos_nor_data_facedots, verts_facedot_len);
		/* TODO(fclem): Maybe move data generation to mesh_render_data_create() */
		if (rdata->edit_bmesh) {
			if (rdata->edit_data && rdata->edit_data->vertexCos != NULL) {
				BKE_editmesh_cache_ensure_poly_normals(rdata->edit_bmesh, rdata->edit_data);
				BKE_editmesh_cache_ensure_poly_centers(rdata->edit_bmesh, rdata->edit_data);
			}
		}
	}

	if (rdata->mapped.use == false) {
		for (int i = 0; i < poly_len; i++) {
			if (add_edit_facedot(rdata, vbo_pos_nor_data_facedots,
			                     attr_id.fdot_pos, attr_id.fdot_nor_flag,
			                     i, facedot_len_used))
			{
				facedot_len_used += 1;
			}
		}
	}
	else {
#if 0 /* TODO(fclem): Mapped facedots are not following the original face. */
		Mesh *me_cage = rdata->mapped.me_cage;
		const MVert *mvert = me_cage->mvert;
		const MEdge *medge = me_cage->medge;
		const int *e_origindex = rdata->mapped.e_origindex;
		const int *v_origindex = rdata->mapped.v_origindex;
#endif
		for (int i = 0; i < poly_len; i++) {
			if (add_edit_facedot_mapped(rdata, vbo_pos_nor_data_facedots,
			                            attr_id.fdot_pos, attr_id.fdot_nor_flag,
			                            i, facedot_len_used))
			{
				facedot_len_used += 1;
			}
		}
	}

	/* Resize & Finish */
	if (facedot_len_used != verts_facedot_len) {
		if (vbo_pos_nor_data_facedots != NULL) {
			GPU_vertbuf_data_resize(vbo_pos_nor_data_facedots, facedot_len_used);
		}
	}
}

/* Indices */

#define NO_EDGE INT_MAX
static void mesh_create_edges_adjacency_lines(
        MeshRenderData *rdata, GPUIndexBuf *ibo, bool *r_is_manifold, const bool use_hide)
{
	const MLoopTri *mlooptri;
	const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
	const int tri_len = mesh_render_data_looptri_len_get_maybe_mapped(rdata);

	*r_is_manifold = true;

	/* Allocate max but only used indices are sent to GPU. */
	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, tri_len * 3, vert_len);

	if (rdata->mapped.use) {
		Mesh *me_cage = rdata->mapped.me_cage;
		mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
	}
	else {
		mlooptri = rdata->mlooptri;
	}

	EdgeHash *eh = BLI_edgehash_new_ex(__func__, tri_len * 3);
	/* Create edges for each pair of triangles sharing an edge. */
	for (int i = 0; i < tri_len; i++) {
		for (int e = 0; e < 3; e++) {
			uint v0, v1, v2;
			if (rdata->mapped.use) {
				const MLoop *mloop = rdata->mloop;
				const MLoopTri *mlt = mlooptri + i;
				const int p_orig = rdata->mapped.p_origindex[mlt->poly];
				if (p_orig != ORIGINDEX_NONE) {
					BMesh *bm = rdata->edit_bmesh->bm;
					BMFace *efa = BM_face_at_index(bm, p_orig);
					/* Assume 'use_hide' */
					if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
						break;
					}
				}
				v0 = mloop[mlt->tri[e]].v;
				v1 = mloop[mlt->tri[(e + 1) % 3]].v;
				v2 = mloop[mlt->tri[(e + 2) % 3]].v;
			}
			else if (rdata->edit_bmesh) {
				const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
					break;
				}
				v0 = BM_elem_index_get(bm_looptri[e]->v);
				v1 = BM_elem_index_get(bm_looptri[(e + 1) % 3]->v);
				v2 = BM_elem_index_get(bm_looptri[(e + 2) % 3]->v);
			}
			else {
				const MLoop *mloop = rdata->mloop;
				const MLoopTri *mlt = mlooptri + i;
				const MPoly *mp = &rdata->mpoly[mlt->poly];
				if (use_hide && (mp->flag & ME_HIDE)) {
					break;
				}
				v0 = mloop[mlt->tri[e]].v;
				v1 = mloop[mlt->tri[(e + 1) % 3]].v;
				v2 = mloop[mlt->tri[(e + 2) % 3]].v;
			}
			bool inv_indices = (v1 > v2);
			void **pval;
			bool value_is_init = BLI_edgehash_ensure_p(eh, v1, v2, &pval);
			int v_data = POINTER_AS_INT(*pval);
			if (!value_is_init || v_data == NO_EDGE) {
				/* Save the winding order inside the sign bit. Because the
				 * edgehash sort the keys and we need to compare winding later. */
				int value = (int)v0 + 1; /* Int 0 bm_looptricannot be signed */
				*pval = POINTER_FROM_INT((inv_indices) ? -value : value);
			}
			else {
				/* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
				*pval = POINTER_FROM_INT(NO_EDGE);
				bool inv_opposite = (v_data < 0);
				uint v_opposite = (uint)abs(v_data) - 1;

				if (inv_opposite == inv_indices) {
					/* Don't share edge if triangles have non matching winding. */
					GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v0);
					GPU_indexbuf_add_line_adj_verts(&elb, v_opposite, v1, v2, v_opposite);
					*r_is_manifold = false;
				}
				else {
					GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v_opposite);
				}
			}
		}
	}
	/* Create edges for remaning non manifold edges. */
	EdgeHashIterator *ehi;
	for (ehi = BLI_edgehashIterator_new(eh);
	     BLI_edgehashIterator_isDone(ehi) == false;
	     BLI_edgehashIterator_step(ehi))
	{
		uint v1, v2;
		int v_data = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
		if (v_data == NO_EDGE) {
			continue;
		}
		BLI_edgehashIterator_getKey(ehi, &v1, &v2);
		uint v0 = (uint)abs(v_data) - 1;
		if (v_data < 0) { /* inv_opposite  */
			SWAP(uint, v1, v2);
		}
		GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v0);
		*r_is_manifold = false;
	}
	BLI_edgehashIterator_free(ehi);
	BLI_edgehash_free(eh, NULL);

	GPU_indexbuf_build_in_place(&elb, ibo);
}
#undef NO_EDGE

static EdgeHash *create_looptri_edge_adjacency_hash(MeshRenderData *rdata, EdgeAdjacentVerts **r_adj_data)
{
	const int tri_len = mesh_render_data_looptri_len_get(rdata);
	/* Create adjacency info in looptri */
	EdgeHash *eh = BLI_edgehash_new_ex(__func__, tri_len * 3);
	/* TODO allocate less memory (based on edge count) */
	EdgeAdjacentVerts *adj_data = MEM_mallocN(tri_len * 3 * sizeof(EdgeAdjacentVerts), __func__);
	*r_adj_data = adj_data;
	/* Create edges for each pair of triangles sharing an edge. */
	for (int i = 0; i < tri_len; i++) {
		for (int e = 0; e < 3; e++) {
			uint v0, v1, v2;
			if (rdata->edit_bmesh) {
				const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				if (BM_elem_flag_test(bm_looptri[0]->f, BM_ELEM_HIDDEN)) {
					break;
				}
				v0 = BM_elem_index_get(bm_looptri[e]->v);
				v1 = BM_elem_index_get(bm_looptri[(e + 1) % 3]->v);
				v2 = BM_elem_index_get(bm_looptri[(e + 2) % 3]->v);
			}
			else {
				const MLoop *mloop = rdata->mloop;
				const MLoopTri *mlt = rdata->mlooptri + i;
				v0 = mloop[mlt->tri[e]].v;
				v1 = mloop[mlt->tri[(e + 1) % 3]].v;
				v2 = mloop[mlt->tri[(e + 2) % 3]].v;
			}

			EdgeAdjacentVerts **eav;
			bool value_is_init = BLI_edgehash_ensure_p(eh, v1, v2, (void ***)&eav);
			if (!value_is_init) {
				*eav = adj_data++;
				(*eav)->vert_index[0] = v0;
				(*eav)->vert_index[1] = -1;
			}
			else {
				if ((*eav)->vert_index[1] == -1) {
					(*eav)->vert_index[1] = v0;
				}
				else {
					/* Not a manifold edge. */
				}
			}
		}
	}
	return eh;
}

static void mesh_create_wireframe_data_tess(MeshRenderData *rdata, GPUVertBuf *vbo)
{
	static uint data_id;
	static GPUVertFormat format = {0};
	if (format.attr_len == 0) {
		data_id = GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
		GPU_vertformat_triple_load(&format);
	}

	GPU_vertbuf_init_with_format(vbo, &format);

	const int tri_len = mesh_render_data_looptri_len_get(rdata);
	int vbo_len_capacity = tri_len * 3;
	GPU_vertbuf_data_alloc(vbo, vbo_len_capacity);

	int vidx = 0;
	EdgeHash *eh = NULL;
	EdgeAdjacentVerts *adj_data = NULL;
	eh = create_looptri_edge_adjacency_hash(rdata, &adj_data);

	for (int i = 0; i < tri_len; i++) {
		uchar vdata[3] = {0, 0, 0};

		const MVert *mvert = rdata->mvert;
		const MEdge *medge = rdata->medge;
		const MLoop *mloop = rdata->mloop;
		const MLoopTri *mlt = rdata->mlooptri + i;

		int j, j_next;
		for (j = 2, j_next = 0; j_next < 3; j = j_next++) {
			const MEdge *ed = &medge[mloop[mlt->tri[j]].e];
			const uint tri_edge[2] = {mloop[mlt->tri[j]].v, mloop[mlt->tri[j_next]].v};

			if ((((ed->v1 == tri_edge[0]) && (ed->v2 == tri_edge[1])) ||
			     ((ed->v1 == tri_edge[1]) && (ed->v2 == tri_edge[0]))))
			{
				/* Real edge. */
				/* Temp Workaround. If a mesh has a subdiv mod we should not
				 * compute the edge sharpness. Instead, we just mix both for now. */
				vdata[j] = ((ed->flag & ME_EDGERENDER) != 0) ? 0xFD : 0xFE;
			}
		}

		/* If at least one edge is real. */
		if (vdata[0] || vdata[1] || vdata[2]) {
			float fnor[3];
			normal_tri_v3(fnor,
			              mvert[mloop[mlt->tri[0]].v].co,
			              mvert[mloop[mlt->tri[1]].v].co,
			              mvert[mloop[mlt->tri[2]].v].co);

			for (int e = 0; e < 3; e++) {
				/* Non-real edge. */
				if (vdata[e] == 0) {
					continue;
				}
				int v0 = mloop[mlt->tri[e]].v;
				int v1 = mloop[mlt->tri[(e + 1) % 3]].v;
				EdgeAdjacentVerts *eav = BLI_edgehash_lookup(eh, v0, v1);
				/* If Non Manifold. */
				if (eav->vert_index[1] == -1) {
					vdata[e] = 0xFF;
				}
				else if (vdata[e] == 0xFD) {
					int v2 = mloop[mlt->tri[(e + 2) % 3]].v;
					/* Select the right opposite vertex */
					v2 = (eav->vert_index[1] == v2) ? eav->vert_index[0] : eav->vert_index[1];
					float fnor_adj[3];
					normal_tri_v3(fnor_adj,
					              mvert[v1].co,
					              mvert[v0].co,
					              mvert[v2].co);
					float fac = dot_v3v3(fnor_adj, fnor);
					fac = fac * fac * 50.0f - 49.0f;
					CLAMP(fac, 0.0f, 0.999f);
					/* Shorten the range to make the non-ME_EDGERENDER fade first.
					 * Add one because 0x0 is no edges. */
					vdata[e] = (uchar)(0xDF * fac) + 1;
					if (vdata[e] < 0.999f) {
						/* TODO construct fast face wire index buffer. */
					}
				}
			}
		}

		for (int e = 0; e < 3; e++) {
			GPU_vertbuf_attr_set(vbo, data_id, vidx++, &vdata[e]);
		}
	}

	BLI_edgehash_free(eh, NULL);
	MEM_freeN(adj_data);
}

static void mesh_create_edges_lines(MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
	const int verts_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
	const int edges_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edges_len, verts_len);

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter;
			BMEdge *eed;

			BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
				/* use_hide always for edit-mode */
				if (BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
					continue;
				}
				GPU_indexbuf_add_line_verts(&elb, BM_elem_index_get(eed->v1), BM_elem_index_get(eed->v2));
			}
		}
		else {
			const MEdge *ed = rdata->medge;
			for (int i = 0; i < edges_len; i++, ed++) {
				if ((ed->flag & ME_EDGERENDER) == 0) {
					continue;
				}
				if (!(use_hide && (ed->flag & ME_HIDE))) {
					GPU_indexbuf_add_line_verts(&elb, ed->v1, ed->v2);
				}
			}
		}
	}
	else {
		BMesh *bm = rdata->edit_bmesh->bm;
		const MEdge *edge = rdata->medge;
		for (int i = 0; i < edges_len; i++, edge++) {
			const int p_orig = rdata->mapped.e_origindex[i];
			if (p_orig != ORIGINDEX_NONE) {
				BMEdge *eed = BM_edge_at_index(bm, p_orig);
				if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
					GPU_indexbuf_add_line_verts(&elb, edge->v1, edge->v2);
				}
			}
		}
	}

	GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_surf_tris(MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
	const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
	const int tri_len = mesh_render_data_looptri_len_get(rdata);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len * 3);

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; i++) {
				const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				const BMFace *bm_face = bm_looptri[0]->f;
				/* use_hide always for edit-mode */
				if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
					continue;
				}
				GPU_indexbuf_add_tri_verts(
				        &elb,
				        BM_elem_index_get(bm_looptri[0]->v),
				        BM_elem_index_get(bm_looptri[1]->v),
				        BM_elem_index_get(bm_looptri[2]->v));
			}
		}
		else {
			const MLoop *loops = rdata->mloop;
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				const MPoly *mp = &rdata->mpoly[mlt->poly];
				if (use_hide && (mp->flag & ME_HIDE)) {
					continue;
				}
				GPU_indexbuf_add_tri_verts(&elb, loops[mlt->tri[0]].v, loops[mlt->tri[1]].v, loops[mlt->tri[2]].v);
			}
		}
	}
	else {
		/* Note: mapped doesn't support lnors yet. */
		BMesh *bm = rdata->edit_bmesh->bm;
		Mesh *me_cage = rdata->mapped.me_cage;

		const MLoop *loops = rdata->mloop;
		const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
		for (int i = 0; i < tri_len; i++) {
			const MLoopTri *mlt = &mlooptri[i];
			const int p_orig = rdata->mapped.p_origindex[mlt->poly];
			if (p_orig != ORIGINDEX_NONE) {
				/* Assume 'use_hide' */
				BMFace *efa = BM_face_at_index(bm, p_orig);
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					GPU_indexbuf_add_tri_verts(&elb, loops[mlt->tri[0]].v, loops[mlt->tri[1]].v, loops[mlt->tri[2]].v);
				}
			}
		}
	}

	GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loops_lines(
        MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
	const int loop_len = mesh_render_data_loops_len_get(rdata);
	const int poly_len = mesh_render_data_polys_len_get(rdata);

	GPUIndexBufBuilder elb;
	GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, loop_len + poly_len * 2, loop_len, true);

	uint v_index = 0;
	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter iter;
			BMFace *bm_face;

			BM_ITER_MESH (bm_face, &iter, bm, BM_FACES_OF_MESH) {
				/* use_hide always for edit-mode */
				if (!BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
					for (int i = 0; i < bm_face->len; i++) {
						GPU_indexbuf_add_generic_vert(&elb, v_index + i);
					}
					/* Finish loop and restart primitive. */
					GPU_indexbuf_add_generic_vert(&elb, v_index);
					GPU_indexbuf_add_primitive_restart(&elb);
				}
				v_index += bm_face->len;
			}
		}
		else {
			for (int poly = 0; poly < poly_len; poly++) {
				const MPoly *mp = &rdata->mpoly[poly];
				if (!(use_hide && (mp->flag & ME_HIDE))) {
					const int loopend = mp->loopstart + mp->totloop;
					for (int j = mp->loopstart; j < loopend; j++) {
						GPU_indexbuf_add_generic_vert(&elb, j);
					}
					/* Finish loop and restart primitive. */
					GPU_indexbuf_add_generic_vert(&elb, mp->loopstart);
					GPU_indexbuf_add_primitive_restart(&elb);
				}
				v_index += mp->totloop;
			}
		}
	}
	else {
		/* Implement ... eventually if needed. */
		BLI_assert(0);
	}

	GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loose_edges_lines(
        MeshRenderData *rdata, GPUIndexBuf *ibo, const bool use_hide)
{
	const int vert_len = mesh_render_data_verts_len_get_maybe_mapped(rdata);
	const int edge_len = mesh_render_data_edges_len_get_maybe_mapped(rdata);

	/* Alloc max (edge_len) and upload only needed range. */
	GPUIndexBufBuilder elb;
	GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, vert_len);

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			/* No need to support since edit mesh already draw them.
			 * But some engines may want them ... */
			BMesh *bm = rdata->edit_bmesh->bm;
			BMIter eiter;
			BMEdge *eed;
			BM_ITER_MESH(eed, &eiter, bm, BM_EDGES_OF_MESH) {
				if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) &&
				    (eed->l == NULL || !bm_edge_has_visible_face(eed)))
				{
					GPU_indexbuf_add_line_verts(&elb, BM_elem_index_get(eed->v1),  BM_elem_index_get(eed->v2));
				}
			}
		}
		else {
			for (int i = 0; i < edge_len; i++) {
				const MEdge *medge = &rdata->medge[i];
				if ((medge->flag & ME_LOOSEEDGE) &&
				    !(use_hide && (medge->flag & ME_HIDE)))
				{
					GPU_indexbuf_add_line_verts(&elb, medge->v1, medge->v2);
				}
			}
		}
	}
	else {
		/* Hidden checks are already done when creating the loose edge list. */
		Mesh *me_cage = rdata->mapped.me_cage;
		for (int i_iter = 0; i_iter < rdata->mapped.loose_edge_len; i_iter++) {
			const int i = rdata->mapped.loose_edges[i_iter];
			const MEdge *medge = &me_cage->medge[i];
			GPU_indexbuf_add_line_verts(&elb, medge->v1, medge->v2);
		}
	}

	GPU_indexbuf_build_in_place(&elb, ibo);
}

static void mesh_create_loops_tris(
        MeshRenderData *rdata, GPUIndexBuf **ibo, int ibo_len, const bool use_hide)
{
	const int loop_len = mesh_render_data_loops_len_get(rdata);
	const int tri_len = mesh_render_data_looptri_len_get(rdata);

	GPUIndexBufBuilder *elb = BLI_array_alloca(elb, ibo_len);

	for (int i = 0; i < ibo_len; ++i) {
		/* TODO alloc minmum necessary. */
		GPU_indexbuf_init(&elb[i], GPU_PRIM_TRIS, tri_len, loop_len * 3);
	}

	if (rdata->mapped.use == false) {
		if (rdata->edit_bmesh) {
			for (int i = 0; i < tri_len; i++) {
				const BMLoop **bm_looptri = (const BMLoop **)rdata->edit_bmesh->looptris[i];
				const BMFace *bm_face = bm_looptri[0]->f;
				/* use_hide always for edit-mode */
				if (BM_elem_flag_test(bm_face, BM_ELEM_HIDDEN)) {
					continue;
				}
				int mat = (ibo_len > 1) ? bm_face->mat_nr : 0;
				GPU_indexbuf_add_tri_verts(
				        &elb[mat],
				        BM_elem_index_get(bm_looptri[0]),
				        BM_elem_index_get(bm_looptri[1]),
				        BM_elem_index_get(bm_looptri[2]));
			}
		}
		else {
			for (int i = 0; i < tri_len; i++) {
				const MLoopTri *mlt = &rdata->mlooptri[i];
				const MPoly *mp = &rdata->mpoly[mlt->poly];
				if (use_hide && (mp->flag & ME_HIDE)) {
					continue;
				}
				int mat = (ibo_len > 1) ? mp->mat_nr : 0;
				GPU_indexbuf_add_tri_verts(&elb[mat], mlt->tri[0], mlt->tri[1], mlt->tri[2]);
			}
		}
	}
	else {
		/* Note: mapped doesn't support lnors yet. */
		BMesh *bm = rdata->edit_bmesh->bm;
		Mesh *me_cage = rdata->mapped.me_cage;

		const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(me_cage);
		for (int i = 0; i < tri_len; i++) {
			const MLoopTri *mlt = &mlooptri[i];
			const int p_orig = rdata->mapped.p_origindex[mlt->poly];
			if (p_orig != ORIGINDEX_NONE) {
				/* Assume 'use_hide' */
				BMFace *efa = BM_face_at_index(bm, p_orig);
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
					int mat = (ibo_len > 1) ? efa->mat_nr : 0;
					GPU_indexbuf_add_tri_verts(&elb[mat], mlt->tri[0], mlt->tri[1], mlt->tri[2]);
				}
			}
		}
	}

	for (int i = 0; i < ibo_len; ++i) {
		GPU_indexbuf_build_in_place(&elb[i], ibo[i]);
	}
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Public API
 * \{ */

static void texpaint_request_active_uv(MeshBatchCache *cache, Mesh *me)
{
	uchar cd_vneeded[CD_NUMTYPES] = {0};
	ushort cd_lneeded[CD_NUMTYPES] = {0};
	mesh_cd_calc_active_uv_layer(me, cd_lneeded);
	if (cd_lneeded[CD_MLOOPUV] == 0) {
		/* This should not happen. */
		BLI_assert(!"No uv layer available in texpaint, but batches requested anyway!");
	}
	bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_vused, cache->cd_lused,
	                                              cd_vneeded, cd_lneeded);
	if (cd_overlap == false) {
		/* XXX TODO(fclem): We are writting to batch cache here. Need to make this thread safe. */
		mesh_cd_layers_type_merge(cache->cd_vneeded, cache->cd_lneeded,
		                          cd_vneeded, cd_lneeded);
	}
}

static void texpaint_request_active_vcol(MeshBatchCache *cache, Mesh *me)
{
	uchar cd_vneeded[CD_NUMTYPES] = {0};
	ushort cd_lneeded[CD_NUMTYPES] = {0};
	mesh_cd_calc_active_vcol_layer(me, cd_lneeded);
	if (cd_lneeded[CD_MLOOPCOL] == 0) {
		/* This should not happen. */
		BLI_assert(!"No vcol layer available in vertpaint, but batches requested anyway!");
	}
	bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_vused, cache->cd_lused,
	                                              cd_vneeded, cd_lneeded);
	if (cd_overlap == false) {
		/* XXX TODO(fclem): We are writting to batch cache here. Need to make this thread safe. */
		mesh_cd_layers_type_merge(cache->cd_vneeded, cache->cd_lneeded,
		                          cd_vneeded, cd_lneeded);
	}
}

GPUBatch *DRW_mesh_batch_cache_get_all_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.all_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_all_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.all_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.loose_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_weights(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.surface_weights);
}

GPUBatch *DRW_mesh_batch_cache_get_edge_detection(Mesh *me, bool *r_is_manifold)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	/* Even if is_manifold is not correct (not updated),
	 * the default (not manifold) is just the worst case. */
	if (r_is_manifold) {
		*r_is_manifold = cache->is_manifold;
	}
	return DRW_batch_request(&cache->batch.edge_detection);
}

GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.wire_triangles);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_triangles);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_vertices);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_loose_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_loose_edges);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_loose_verts(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_loose_verts);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_triangles_nor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles_lnor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_triangles_lnor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_loose_edges_nor(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_loose_edges_nor);
}

GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.edit_facedots);
}

GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(
        Mesh *me, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	uchar cd_vneeded[CD_NUMTYPES] = {0};
	ushort cd_lneeded[CD_NUMTYPES] = {0};
	mesh_cd_calc_used_gpu_layers(me, cd_vneeded, cd_lneeded, gpumat_array, gpumat_array_len);

	BLI_assert(gpumat_array_len == cache->mat_len);

	bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_vused, cache->cd_lused,
	                                              cd_vneeded, cd_lneeded);
	if (cd_overlap == false) {
		/* XXX TODO(fclem): We are writting to batch cache here. Need to make this thread safe. */
		mesh_cd_layers_type_merge(cache->cd_vneeded, cache->cd_lneeded,
		                          cd_vneeded, cd_lneeded);

		mesh_cd_extract_auto_layers_names_and_srgb(me,
		                                           cache->cd_lneeded,
		                                           &cache->auto_layer_names,
		                                           &cache->auto_layer_is_srgb,
		                                           &cache->auto_layer_len);
	}
	if (auto_layer_names) {
		*auto_layer_names = cache->auto_layer_names;
		*auto_layer_is_srgb = cache->auto_layer_is_srgb;
		*auto_layer_count = cache->auto_layer_len;
	}
	for (int i = 0; i < cache->mat_len; ++i) {
		DRW_batch_request(&cache->surf_per_mat[i]);
	}
	return cache->surf_per_mat;
}

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	texpaint_request_active_uv(cache, me);
	for (int i = 0; i < cache->mat_len; ++i) {
		DRW_batch_request(&cache->surf_per_mat[i]);
	}
	return cache->surf_per_mat;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	texpaint_request_active_uv(cache, me);
	return DRW_batch_request(&cache->batch.surface);
}

GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	texpaint_request_active_vcol(cache, me);
	return DRW_batch_request(&cache->batch.surface);
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name Edit Mode selection API
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh *me, bool use_hide, uint select_id_offset)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->triangles_with_select_id_offset != select_id_offset) {
		cache->triangles_with_select_id_offset = select_id_offset;
		GPU_BATCH_DISCARD_SAFE(cache->triangles_with_select_id);
	}

	if (cache->triangles_with_select_id == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);
		if (rdata->mapped.supported) {
			rdata->mapped.use = true;
		}

		cache->triangles_with_select_id = GPU_batch_create_ex(
		        GPU_PRIM_TRIS, mesh_create_tri_select_id(rdata, use_hide, select_id_offset), NULL, GPU_BATCH_OWNS_VBO);

		GPUVertBuf *vbo_tris = mesh_batch_cache_get_tri_pos_and_normals_edit(rdata, cache, use_hide);
		GPU_batch_vertbuf_add(cache->triangles_with_select_id, vbo_tris);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_select_id;
}

/**
 * Same as #DRW_mesh_batch_cache_get_triangles_with_select_id
 * without the ID's, use to mask out geometry, eg - dont select face-dots behind other faces.
 */
GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_mask(Mesh *me, bool use_hide)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	if (cache->triangles_with_select_mask == NULL) {
		const int datatype =
		        MR_DATATYPE_VERT | MR_DATATYPE_LOOPTRI | MR_DATATYPE_LOOP | MR_DATATYPE_POLY;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);
		if (rdata->mapped.supported) {
			rdata->mapped.use = true;
		}

		GPUVertBuf *vbo_tris = mesh_batch_cache_get_tri_pos_and_normals_edit(rdata, cache, use_hide);

		cache->triangles_with_select_mask = GPU_batch_create(
		        GPU_PRIM_TRIS, vbo_tris, NULL);

		mesh_render_data_free(rdata);
	}

	return cache->triangles_with_select_mask;
}


GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh *me, uint select_id_offset)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->facedot_with_select_id_offset != select_id_offset) {
		cache->facedot_with_select_id_offset = select_id_offset;
		GPU_BATCH_DISCARD_SAFE(cache->edges_with_select_id);
	}

	if (cache->facedot_with_select_id == NULL) {
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY);

		/* We only want the 'pos', not the normals or flag.
		 * Use since this is almost certainly already created. */
		cache->facedot_with_select_id = GPU_batch_create(
		        GPU_PRIM_POINTS, mesh_batch_cache_get_facedot_pos_with_normals_and_flag(rdata, cache), NULL);

		GPU_batch_vertbuf_add_ex(
		        cache->facedot_with_select_id,
		        mesh_create_facedot_select_id(rdata, select_id_offset), true);

		mesh_render_data_free(rdata);
	}

	return cache->facedot_with_select_id;
}

GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh *me, uint select_id_offset)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->edges_with_select_id_offset != select_id_offset) {
		cache->edges_with_select_id_offset = select_id_offset;
		GPU_BATCH_DISCARD_SAFE(cache->edges_with_select_id);
	}

	if (cache->edges_with_select_id == NULL) {
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT | MR_DATATYPE_EDGE);
		if (rdata->mapped.supported) {
			rdata->mapped.use = true;
		}

		cache->edges_with_select_id = GPU_batch_create(
		        GPU_PRIM_LINES, mesh_batch_cache_get_edges_visible(rdata, cache), NULL);

		GPU_batch_vertbuf_add_ex(
		        cache->edges_with_select_id,
		        mesh_create_edges_select_id(rdata, select_id_offset), true);

		mesh_render_data_free(rdata);
	}

	return cache->edges_with_select_id;
}

GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh *me, uint select_id_offset)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->verts_with_select_id_offset != select_id_offset) {
		cache->verts_with_select_id_offset = select_id_offset;
		GPU_BATCH_DISCARD_SAFE(cache->verts_with_select_id);
	}

	if (cache->verts_with_select_id == NULL) {
		MeshRenderData *rdata = mesh_render_data_create(me, MR_DATATYPE_VERT);
		if (rdata->mapped.supported) {
			rdata->mapped.use = true;
		}

		cache->verts_with_select_id = GPU_batch_create(
		        GPU_PRIM_POINTS, mesh_batch_cache_get_verts_visible(rdata, cache), NULL);

		GPU_batch_vertbuf_add_ex(
		        cache->verts_with_select_id,
		        mesh_create_verts_select_id(rdata, select_id_offset), true);

		mesh_render_data_free(rdata);
	}

	return cache->verts_with_select_id;
}

/** \} */

/* ---------------------------------------------------------------------- */

/** \name UV Image editor API
 * \{ */

/* TODO port to batch request. Is basically batch.wire_loops. */
GPUBatch *DRW_mesh_batch_cache_get_texpaint_loop_wire(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	if (cache->texpaint_uv_loops == NULL) {
		/* create batch from DM */
		const int datatype = MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPUV;
		MeshRenderData *rdata = mesh_render_data_create(me, datatype);

		const MLoopUV *mloopuv_base = rdata->mloopuv;
		if (mloopuv_base == NULL) {
			return NULL;
		}

		uint vidx = 0;

		static GPUVertFormat format = { 0 };
		static struct { uint uv; } attr_id;
		if (format.attr_len == 0) {
			attr_id.uv = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		}

		const uint vert_len = mesh_render_data_loops_len_get(rdata);
		const uint poly_len = mesh_render_data_polys_len_get(rdata);
		const uint idx_len = vert_len + poly_len;

		GPUIndexBufBuilder elb;
		GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_LOOP, idx_len, vert_len, true);

		GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(vbo, vert_len);

		const MPoly *mpoly = rdata->mpoly;
		for (int a = 0; a < poly_len; a++, mpoly++) {
			const MLoopUV *mloopuv = mloopuv_base + mpoly->loopstart;
			for (int b = 0; b < mpoly->totloop; b++, mloopuv++) {
				GPU_vertbuf_attr_set(vbo, attr_id.uv, vidx, mloopuv->uv);
				GPU_indexbuf_add_generic_vert(&elb, vidx++);
			}
			GPU_indexbuf_add_primitive_restart(&elb);
		}

		cache->texpaint_uv_loops = GPU_batch_create_ex(GPU_PRIM_LINE_LOOP,
		                                               vbo, GPU_indexbuf_build(&elb),
		                                               GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
		gpu_batch_presets_register(cache->texpaint_uv_loops);
		mesh_render_data_free(rdata);
	}
	return cache->texpaint_uv_loops;
}

GPUBatch *DRW_mesh_batch_cache_get_surface_edges(Mesh *me)
{
	MeshBatchCache *cache = mesh_batch_cache_get(me);
	return DRW_batch_request(&cache->batch.wire_loops);
}

/**
 * Needed for when we draw with shaded data.
 */
void DRW_mesh_cache_sculpt_coords_ensure(Mesh *me)
{
	if (me->runtime.batch_cache) {
		MeshBatchCache *cache = mesh_batch_cache_get(me);
		if (cache && cache->pos_with_normals && cache->is_sculpt_points_tag) {
			/* XXX Force update of all the batches that contains the pos_with_normals buffer.
			 * TODO(fclem): Ideally, Gawain should provide a way to update a buffer without destroying it. */
			mesh_batch_cache_clear_selective(me, cache->pos_with_normals);
			GPU_VERTBUF_DISCARD_SAFE(cache->pos_with_normals);
		}
		cache->is_sculpt_points_tag = false;
	}
}

static uchar mesh_batch_cache_validate_edituvs(MeshBatchCache *cache, uchar state)
{
	if ((cache->edituv_state & UVEDIT_SYNC_SEL) != (state & UVEDIT_SYNC_SEL)) {
		mesh_batch_cache_discard_uvedit(cache);
		return state;
	}
	else {
		return ((cache->edituv_state & state) ^ state);
	}
}

/* Compute 3D & 2D areas and their sum. */
BLI_INLINE void edit_uv_preprocess_stretch_area(
        float (*tf_uv)[2], BMFace *efa, const float asp[2], const int cd_loop_uv_offset, uint fidx,
        float *totarea, float *totuvarea, float (*faces_areas)[2])
{
	BMLoop *l;
	BMIter liter;
	int i;
	BM_ITER_ELEM_INDEX(l, &liter, efa, BM_LOOPS_OF_FACE, i) {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		mul_v2_v2v2(tf_uv[i], luv->uv, asp);
	}
	faces_areas[fidx][0] = BM_face_calc_area(efa);
	faces_areas[fidx][1] = area_poly_v2(tf_uv, efa->len);

	*totarea += faces_areas[fidx][0];
	*totuvarea += faces_areas[fidx][1];
}

BLI_INLINE float edit_uv_get_stretch_area(float area, float uvarea)
{
	if (area < FLT_EPSILON || uvarea < FLT_EPSILON) {
		return 1.0f;
	}
	else if (area > uvarea) {
		return 1.0f - (uvarea / area);
	}
	else {
		return 1.0f - (area / uvarea);
	}
}

/* Compute face's normalized contour vectors. */
BLI_INLINE void edit_uv_preprocess_stretch_angle(
        float (*auv)[2], float (*av)[3], const int cd_loop_uv_offset, BMFace *efa, float asp[2])
{
	BMLoop *l;
	BMIter liter;
	int i;
	BM_ITER_ELEM_INDEX(l, &liter, efa, BM_LOOPS_OF_FACE, i) {
		MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
		MLoopUV *luv_prev = BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset);

		sub_v2_v2v2(auv[i], luv_prev->uv, luv->uv);
		mul_v2_v2(auv[i], asp);
		normalize_v2(auv[i]);

		sub_v3_v3v3(av[i], l->prev->v->co, l->v->co);
		normalize_v3(av[i]);
	}
}

BLI_INLINE float edit_uv_get_loop_stretch_angle(
        const float auv0[2], const float auv1[2], const float av0[3], const float av1[3])
{
	float uvang = angle_normalized_v2v2(auv0, auv1);
	float ang = angle_normalized_v3v3(av0, av1);
	float stretch = fabsf(uvang - ang) / (float)M_PI;
	return 1.0f - pow2f(1.0f - stretch);
}

#define VERTEX_SELECT (1 << 0)
#define VERTEX_PINNED (1 << 1)
#define FACE_SELECT (1 << 2)
#define FACE_ACTIVE (1 << 3)
#define EDGE_SELECT (1 << 4)

BLI_INLINE uchar edit_uv_get_face_flag(BMFace *efa, BMFace *efa_act, const int cd_loop_uv_offset, Scene *scene)
{
	uchar flag = 0;
	flag |= uvedit_face_select_test(scene, efa, cd_loop_uv_offset) ? FACE_SELECT : 0;
	flag |= (efa == efa_act) ? FACE_ACTIVE : 0;
	return flag;
}

BLI_INLINE uchar edit_uv_get_loop_flag(BMLoop *l, const int cd_loop_uv_offset, Scene *scene)
{
	MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
	uchar flag = 0;
	flag |= uvedit_uv_select_test(scene, l, cd_loop_uv_offset) ? VERTEX_SELECT : 0;
	flag |= uvedit_edge_select_test(scene, l, cd_loop_uv_offset) ? EDGE_SELECT : 0;
	flag |= (luv->flag & MLOOPUV_PINNED) ? VERTEX_PINNED : 0;
	return flag;
}

static struct EditUVFormatIndex {
	uint uvs, area, angle, flag, fdots_uvs, fdots_flag;
} uv_attr_id = {0};

static void uvedit_fill_buffer_data(
        Object *ob, struct SpaceImage *sima, Scene *scene, uchar state, MeshBatchCache *cache,
        GPUIndexBufBuilder *elb_faces, GPUIndexBufBuilder *elb_edges, GPUVertBuf **facedots_vbo)
{
	Mesh *me = ob->data;
	BMEditMesh *embm = me->edit_btmesh;
	BMesh *bm = embm->bm;
	BMIter iter, liter;
	BMFace *efa;
	BMLoop *l;
	MLoopUV *luv;
	uint vidx, fidx, i;
	float (*faces_areas)[2] = NULL;
	float asp[2];
	float totarea = 0.0f, totuvarea = 0.0f;
	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	Image *ima = sima->image;
	BMFace *efa_act = EDBM_uv_active_face_get(embm, false, false); /* will be set to NULL if hidden */

	if (state & (UVEDIT_STRETCH_AREA | UVEDIT_STRETCH_ANGLE)) {
		ED_space_image_get_uv_aspect(sima, &asp[0], &asp[1]);
	}

	BLI_buffer_declare_static(vec3f, vec3_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_buffer_declare_static(vec2f, vec2_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

	if (state & UVEDIT_STRETCH_AREA) {
		faces_areas = MEM_mallocN(sizeof(float) * 2 * bm->totface, "EDITUV faces areas");
	}

	/* Preprocess */
	fidx = 0;
	BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
		/* Tag hidden faces */
		BM_elem_flag_set(efa, BM_ELEM_TAG, uvedit_face_visible_test(scene, ob, ima, efa));

		if ((state & UVEDIT_STRETCH_AREA) &&
		    BM_elem_flag_test(efa, BM_ELEM_TAG))
		{
			const int efa_len = efa->len;
			float (*tf_uv)[2] = (float (*)[2])BLI_buffer_reinit_data(&vec2_buf, vec2f, efa_len);
			edit_uv_preprocess_stretch_area(tf_uv, efa, asp, cd_loop_uv_offset, fidx++,
			                                &totarea, &totuvarea, faces_areas);
		}
	}

	vidx = 0;
	fidx = 0;
	BM_ITER_MESH(efa, &iter, bm, BM_FACES_OF_MESH) {
		const int efa_len = efa->len;
		float fdot[2] = {0.0f, 0.0f};
		float (*av)[3], (*auv)[2];
		ushort area_stretch;
		/* Skip hidden faces. */
		if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
			continue;

		uchar face_flag = edit_uv_get_face_flag(efa, efa_act, cd_loop_uv_offset, scene);
		/* Face preprocess */
		if (state & UVEDIT_STRETCH_AREA) {
			area_stretch = edit_uv_get_stretch_area(faces_areas[fidx][0] / totarea,
			                                        faces_areas[fidx][1] / totuvarea) * 65534.0f;
		}
		if (state & UVEDIT_STRETCH_ANGLE) {
			av  = (float (*)[3])BLI_buffer_reinit_data(&vec3_buf, vec3f, efa_len);
			auv = (float (*)[2])BLI_buffer_reinit_data(&vec2_buf, vec2f, efa_len);
			edit_uv_preprocess_stretch_angle(auv, av, cd_loop_uv_offset, efa, asp);
		}

		BM_ITER_ELEM_INDEX(l, &liter, efa, BM_LOOPS_OF_FACE, i) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			uchar flag = face_flag | edit_uv_get_loop_flag(l, cd_loop_uv_offset, scene);

			if (state & UVEDIT_STRETCH_AREA) {
				GPU_vertbuf_attr_set(cache->edituv_area, uv_attr_id.area, vidx, &area_stretch);
			}
			if (state & UVEDIT_STRETCH_ANGLE) {
				ushort angle = 65534.0f * edit_uv_get_loop_stretch_angle(auv[i], auv[(i + 1) % efa_len],
				                                                         av[i],  av[(i + 1) % efa_len]);
				GPU_vertbuf_attr_set(cache->edituv_angle, uv_attr_id.angle, vidx, &angle);
			}
			if (state & UVEDIT_EDGES) {
				GPU_vertbuf_attr_set(cache->edituv_pos, uv_attr_id.uvs, vidx, luv->uv);
			}
			if (state & UVEDIT_DATA) {
				GPU_vertbuf_attr_set(cache->edituv_data, uv_attr_id.flag, vidx, &flag);
			}
			if (state & UVEDIT_FACES) {
				GPU_indexbuf_add_generic_vert(elb_faces, vidx);
			}
			if (state & UVEDIT_EDGES) {
				GPU_indexbuf_add_generic_vert(elb_edges, vidx);
			}

			if (state & UVEDIT_FACEDOTS) {
				add_v2_v2(fdot, luv->uv);
			}
			vidx++;
		}

		if (state & UVEDIT_FACES) {
			GPU_indexbuf_add_primitive_restart(elb_faces);
		}
		if (state & UVEDIT_EDGES) {
			GPU_indexbuf_add_primitive_restart(elb_edges);
		}

		if (state & UVEDIT_FACEDOTS) {
			mul_v2_fl(fdot, 1.0f / (float)efa->len);
			GPU_vertbuf_attr_set(*facedots_vbo, uv_attr_id.fdots_uvs, fidx, fdot);
			GPU_vertbuf_attr_set(*facedots_vbo, uv_attr_id.fdots_flag, fidx, &face_flag);
		}
		fidx++;
	}

	if (faces_areas) {
		MEM_freeN(faces_areas);
	}

	BLI_buffer_free(&vec3_buf);
	BLI_buffer_free(&vec2_buf);

	if (vidx == 0) {
		GPU_VERTBUF_DISCARD_SAFE(cache->edituv_area);
		GPU_VERTBUF_DISCARD_SAFE(cache->edituv_angle);
		GPU_VERTBUF_DISCARD_SAFE(cache->edituv_pos);
		GPU_VERTBUF_DISCARD_SAFE(cache->edituv_data);
		GPU_VERTBUF_DISCARD_SAFE(*facedots_vbo);
	}

	if (vidx < bm->totloop) {
		if (cache->edituv_area && (state & UVEDIT_STRETCH_AREA)) {
			GPU_vertbuf_data_resize(cache->edituv_area, vidx);
		}
		if (cache->edituv_angle && (state & UVEDIT_STRETCH_ANGLE)) {
			GPU_vertbuf_data_resize(cache->edituv_angle, vidx);
		}
		if (cache->edituv_pos && (state & UVEDIT_EDGES)) {
			GPU_vertbuf_data_resize(cache->edituv_pos, vidx);
		}
		if (cache->edituv_data && (state & UVEDIT_DATA)) {
			GPU_vertbuf_data_resize(cache->edituv_data, vidx);
		}
	}
	if (fidx < bm->totface) {
		if (*facedots_vbo) {
			GPU_vertbuf_data_resize(*facedots_vbo, fidx);
		}
	}
}

static void mesh_batch_cache_create_uvedit_buffers(
        Object *ob, struct SpaceImage *sima, Scene *scene, MeshBatchCache *cache, uchar state)
{
	GPUVertBuf *facedots_vbo = NULL;

	if (state == 0) {
		return;
	}

	Mesh *me = ob->data;
	BMEditMesh *embm = me->edit_btmesh;
	BMesh *bm = embm->bm;

	static GPUVertFormat format_pos = { 0 };
	static GPUVertFormat format_area = { 0 };
	static GPUVertFormat format_angle = { 0 };
	static GPUVertFormat format_flag = { 0 };
	static GPUVertFormat format_facedots = { 0 };

	if (format_pos.attr_len == 0) {
		uv_attr_id.uvs   = GPU_vertformat_attr_add(&format_pos,   "pos",     GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		uv_attr_id.area  = GPU_vertformat_attr_add(&format_area,  "stretch", GPU_COMP_U16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
		uv_attr_id.angle = GPU_vertformat_attr_add(&format_angle, "stretch", GPU_COMP_U16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
		uv_attr_id.flag  = GPU_vertformat_attr_add(&format_flag,  "flag",    GPU_COMP_U8,  1, GPU_FETCH_INT);

		uv_attr_id.fdots_uvs  = GPU_vertformat_attr_add(&format_facedots, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		uv_attr_id.fdots_flag = GPU_vertformat_attr_add(&format_facedots, "flag", GPU_COMP_U8, 1, GPU_FETCH_INT);
	}

	const uint vert_len = bm->totloop;
	const uint idx_len = bm->totloop + bm->totface;
	const uint face_len = bm->totface;

	if (state & UVEDIT_EDGES) {
		cache->edituv_pos = GPU_vertbuf_create_with_format(&format_pos);
		GPU_vertbuf_data_alloc(cache->edituv_pos, vert_len);
	}
	if (state & UVEDIT_DATA) {
		cache->edituv_data = GPU_vertbuf_create_with_format(&format_flag);
		GPU_vertbuf_data_alloc(cache->edituv_data, vert_len);
	}
	if (state & UVEDIT_STRETCH_AREA) {
		cache->edituv_area = GPU_vertbuf_create_with_format(&format_area);
		GPU_vertbuf_data_alloc(cache->edituv_area, vert_len);
	}
	if (state & UVEDIT_STRETCH_ANGLE) {
		cache->edituv_angle = GPU_vertbuf_create_with_format(&format_angle);
		GPU_vertbuf_data_alloc(cache->edituv_angle, vert_len);
	}
	if (state & UVEDIT_FACEDOTS) {
		facedots_vbo = GPU_vertbuf_create_with_format(&format_facedots);
		GPU_vertbuf_data_alloc(facedots_vbo, face_len);
	}

	/* NOTE: we could use the same index buffer for both primitive type (it's the same indices)
	 * but since GPU_PRIM_LINE_LOOP does not exist in vulkan, make it future proof. */
	GPUIndexBufBuilder elb_faces, elb_edges;
	if (state & UVEDIT_EDGES) {
		GPU_indexbuf_init_ex(&elb_edges, GPU_PRIM_LINE_LOOP, idx_len, vert_len, true);
	}
	if (state & UVEDIT_FACES) {
		GPU_indexbuf_init_ex(&elb_faces, GPU_PRIM_TRI_FAN, idx_len, vert_len, true);
	}

	uvedit_fill_buffer_data(ob, sima, scene, state, cache, &elb_faces, &elb_edges, &facedots_vbo);

	if (state & UVEDIT_EDGES) {
		cache->edituv_visible_edges = GPU_indexbuf_build(&elb_edges);
	}
	if (state & UVEDIT_FACES) {
		cache->edituv_visible_faces = GPU_indexbuf_build(&elb_faces);
	}
	if ((state & UVEDIT_FACEDOTS) && facedots_vbo) {
		cache->edituv_facedots = GPU_batch_create_ex(GPU_PRIM_POINTS, facedots_vbo, NULL, GPU_BATCH_OWNS_VBO);
		gpu_batch_presets_register(cache->edituv_facedots);
	}

	cache->edituv_state |= state;
}

void DRW_mesh_cache_uvedit(
        Object *ob, struct SpaceImage *sima, Scene *scene, uchar state,
        GPUBatch **faces, GPUBatch **edges, GPUBatch **verts, GPUBatch **facedots)
{
	Mesh *me = ob->data;
	MeshBatchCache *cache = mesh_batch_cache_get(me);

	uchar missing_state = mesh_batch_cache_validate_edituvs(cache, state);

	mesh_batch_cache_create_uvedit_buffers(ob, sima, scene, cache, missing_state);

	/* Bail out if there is nothing to draw. */
	if (cache->edituv_data == NULL) {
		*faces = *edges = *verts = *facedots = NULL;
		return;
	}

	/* Faces */
	if (state & UVEDIT_STRETCH_AREA) {
		if (cache->edituv_faces_strech_area == NULL) {
			cache->edituv_faces_strech_area = GPU_batch_create(GPU_PRIM_TRI_FAN,
			                                                   cache->edituv_pos,
			                                                   cache->edituv_visible_faces);
			GPU_batch_vertbuf_add_ex(cache->edituv_faces_strech_area,
			                         cache->edituv_area, false);
			gpu_batch_presets_register(cache->edituv_faces_strech_area);
		}
		*faces = cache->edituv_faces_strech_area;
	}
	else if (state & UVEDIT_STRETCH_ANGLE) {
		if (cache->edituv_faces_strech_angle == NULL) {
			cache->edituv_faces_strech_angle = GPU_batch_create(GPU_PRIM_TRI_FAN,
			                                                    cache->edituv_pos,
			                                                    cache->edituv_visible_faces);
			GPU_batch_vertbuf_add_ex(cache->edituv_faces_strech_angle,
			                         cache->edituv_angle, false);
			gpu_batch_presets_register(cache->edituv_faces_strech_angle);
		}
		*faces = cache->edituv_faces_strech_angle;
	}
	else if (state & UVEDIT_FACES) {
		if (cache->edituv_faces == NULL) {
			cache->edituv_faces = GPU_batch_create(GPU_PRIM_TRI_FAN,
			                                       cache->edituv_pos,
			                                       cache->edituv_visible_faces);
			GPU_batch_vertbuf_add_ex(cache->edituv_faces,
			                         cache->edituv_data, false);
			gpu_batch_presets_register(cache->edituv_faces);
		}
		*faces = cache->edituv_faces;
	}
	else {
		*faces = NULL;
	}

	{
		if (cache->edituv_edges == NULL) {
			cache->edituv_edges = GPU_batch_create(GPU_PRIM_LINE_LOOP,
			                                       cache->edituv_pos,
			                                       cache->edituv_visible_edges);
			GPU_batch_vertbuf_add_ex(cache->edituv_edges,
			                         cache->edituv_data, false);
			gpu_batch_presets_register(cache->edituv_edges);
		}
		*edges = cache->edituv_edges;
	}

	{
		if (cache->edituv_verts == NULL) {
			cache->edituv_verts = GPU_batch_create(GPU_PRIM_POINTS,
			                                       cache->edituv_pos,
			                                       NULL);
			GPU_batch_vertbuf_add_ex(cache->edituv_verts,
			                         cache->edituv_data, false);
			gpu_batch_presets_register(cache->edituv_verts);
		}
		*verts = cache->edituv_verts;
	}

	if (state & UVEDIT_FACEDOTS) {
		*facedots = cache->edituv_facedots;
	}
	else {
		*facedots = NULL;
	}
}

/** \} */


/* ---------------------------------------------------------------------- */

/** \name Grouped batch generation
 * \{ */

/* Can be called for any surface type. Mesh *me is the final mesh. */
void DRW_mesh_batch_cache_create_requested(Object *ob, Mesh *me)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const int mode = CTX_data_mode_enum_ex(draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);
	const bool is_paint_mode = ELEM(mode, CTX_MODE_PAINT_TEXTURE, CTX_MODE_PAINT_VERTEX, CTX_MODE_PAINT_WEIGHT);
	const bool use_hide = (
	        (ob->type == OB_MESH) &&
	        ((is_paint_mode && (ob == draw_ctx->obact)) ||
	         ((mode == CTX_MODE_EDIT_MESH) && BKE_object_is_in_editmode(ob))));
	bool use_face_sel = false;

	/* Tex paint face select */
	if (is_paint_mode && (ob->type == OB_MESH) && (draw_ctx->obact == ob)) {
		const Mesh *me_orig = DEG_get_original_object(ob)->data;
		use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	}

	MeshBatchCache *cache = mesh_batch_cache_get(me);

	/* Check vertex weights. */
	if (cache->batch.surface_weights != 0) {
		struct DRW_MeshWeightState wstate;
		BLI_assert(ob->type == OB_MESH);
		drw_mesh_weight_state_extract(ob, me, draw_ctx->scene->toolsettings, is_paint_mode, &wstate);
		mesh_batch_cache_check_vertex_group(cache, &wstate);
		drw_mesh_weight_state_copy(&cache->weight_state, &wstate);
		drw_mesh_weight_state_clear(&wstate);
	}

	/* Verify that all surface batches have needed attrib layers. */
	/* TODO(fclem): We could be a bit smarter here and only do it per material. */
	bool cd_overlap = mesh_cd_layers_type_overlap(cache->cd_vused, cache->cd_lused,
	                                              cache->cd_vneeded, cache->cd_lneeded);
	if (cd_overlap == false) {
		for (int type = 0; type < CD_NUMTYPES; ++type) {
			if ((cache->cd_vused[type] & cache->cd_vneeded[type]) != cache->cd_vneeded[type]) {
				switch (type) {
					case CD_MLOOPUV:
					case CD_TANGENT:
						GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_uv_tan);
						break;
					case CD_MLOOPCOL:
						GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_vcol);
						break;
					case CD_ORCO:
						/* TODO */
						// GPU_VERTBUF_DISCARD_SAFE(cache->ordered.loop_orco);
						break;
				}
			}
		}
		/* We can't discard batches at this point as they have been
		 * referenced for drawing. Just clear them in place. */
		for (int i = 0; i < cache->mat_len; ++i) {
			GPU_BATCH_CLEAR_SAFE(cache->surf_per_mat[i]);
		}
		GPU_BATCH_CLEAR_SAFE(cache->batch.surface);

		mesh_cd_layers_type_merge(cache->cd_vused, cache->cd_lused,
		                          cache->cd_vneeded, cache->cd_lneeded);

	}

	memset(cache->cd_lneeded, 0, sizeof(cache->cd_lneeded));
	memset(cache->cd_vneeded, 0, sizeof(cache->cd_vneeded));

	/* Init batches and request VBOs & IBOs */
	if (DRW_batch_requested(cache->batch.surface, GPU_PRIM_TRIS)) {
		DRW_ibo_request(cache->batch.surface, &cache->ibo.loops_tris);
		DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_pos_nor);
		/* For paint overlay. Active layer should have been queried. */
		if (cache->cd_lused[CD_MLOOPUV] != 0) {
			DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_uv_tan);
		}
		if (cache->cd_lused[CD_MLOOPCOL] != 0) {
			DRW_vbo_request(cache->batch.surface, &cache->ordered.loop_vcol);
		}
	}
	if (DRW_batch_requested(cache->batch.all_verts, GPU_PRIM_POINTS)) {
		DRW_vbo_request(cache->batch.all_verts, &cache->ordered.pos_nor);
	}
	if (DRW_batch_requested(cache->batch.all_edges, GPU_PRIM_LINES)) {
		DRW_ibo_request(cache->batch.all_edges, &cache->ibo.edges_lines);
		DRW_vbo_request(cache->batch.all_edges, &cache->ordered.pos_nor);
	}
	if (DRW_batch_requested(cache->batch.loose_edges, GPU_PRIM_LINES)) {
		DRW_ibo_request(cache->batch.loose_edges, &cache->ibo.loose_edges_lines);
		DRW_vbo_request(cache->batch.loose_edges, &cache->ordered.pos_nor);
	}
	if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
		DRW_ibo_request(cache->batch.edge_detection, &cache->ibo.edges_adj_lines);
		DRW_vbo_request(cache->batch.edge_detection, &cache->ordered.pos_nor);
	}
	if (DRW_batch_requested(cache->batch.surface_weights, GPU_PRIM_TRIS)) {
		DRW_ibo_request(cache->batch.surface_weights, &cache->ibo.surf_tris);
		DRW_vbo_request(cache->batch.surface_weights, &cache->ordered.pos_nor);
		DRW_vbo_request(cache->batch.surface_weights, &cache->ordered.weights);
	}
	if (DRW_batch_requested(cache->batch.wire_loops, GPU_PRIM_LINE_STRIP)) {
		DRW_ibo_request(cache->batch.wire_loops, &cache->ibo.loops_lines);
		DRW_vbo_request(cache->batch.wire_loops, &cache->ordered.loop_pos_nor);
	}
	if (DRW_batch_requested(cache->batch.wire_triangles, GPU_PRIM_TRIS)) {
		DRW_vbo_request(cache->batch.wire_triangles, &cache->tess.pos_nor);
		DRW_vbo_request(cache->batch.wire_triangles, &cache->tess.wireframe_data);
	}

	if (DRW_batch_requested(cache->batch.edit_triangles, GPU_PRIM_TRIS)) {
		DRW_vbo_request(cache->batch.edit_triangles, &cache->edit.pos_nor);
		DRW_vbo_request(cache->batch.edit_triangles, &cache->edit.data);
	}
	if (DRW_batch_requested(cache->batch.edit_vertices, GPU_PRIM_POINTS)) {
		DRW_ibo_request(cache->batch.edit_vertices, &cache->ibo.edit_verts_points);
		DRW_vbo_request(cache->batch.edit_vertices, &cache->edit.pos_nor);
		DRW_vbo_request(cache->batch.edit_vertices, &cache->edit.data);
	}
	if (DRW_batch_requested(cache->batch.edit_loose_edges, GPU_PRIM_LINES)) {
		DRW_vbo_request(cache->batch.edit_loose_edges, &cache->edit.pos_nor_ledges);
		DRW_vbo_request(cache->batch.edit_loose_edges, &cache->edit.data_ledges);
	}
	if (DRW_batch_requested(cache->batch.edit_loose_verts, GPU_PRIM_POINTS)) {
		DRW_vbo_request(cache->batch.edit_loose_verts, &cache->edit.pos_nor_lverts);
		DRW_vbo_request(cache->batch.edit_loose_verts, &cache->edit.data_lverts);
	}
	if (DRW_batch_requested(cache->batch.edit_triangles_nor, GPU_PRIM_POINTS)) {
		DRW_ibo_request(cache->batch.edit_triangles_nor, &cache->ibo.edit_verts_points);
		DRW_vbo_request(cache->batch.edit_triangles_nor, &cache->edit.pos_nor);
	}
	if (DRW_batch_requested(cache->batch.edit_triangles_lnor, GPU_PRIM_POINTS)) {
		DRW_vbo_request(cache->batch.edit_triangles_lnor, &cache->edit.pos_nor);
		DRW_vbo_request(cache->batch.edit_triangles_lnor, &cache->edit.lnor);
	}
	if (DRW_batch_requested(cache->batch.edit_loose_edges_nor, GPU_PRIM_POINTS)) {
		DRW_vbo_request(cache->batch.edit_loose_edges_nor, &cache->edit.pos_nor_ledges);
	}
	if (DRW_batch_requested(cache->batch.edit_facedots, GPU_PRIM_POINTS)) {
		DRW_vbo_request(cache->batch.edit_facedots, &cache->edit.pos_nor_data_facedots);
	}

	for (int i = 0; i < cache->mat_len; ++i) {
		if (DRW_batch_requested(cache->surf_per_mat[i], GPU_PRIM_TRIS)) {
			if (cache->mat_len > 1) {
				DRW_ibo_request(cache->surf_per_mat[i], &cache->surf_per_mat_tris[i]);
			}
			else {
				DRW_ibo_request(cache->surf_per_mat[i], &cache->ibo.loops_tris);
			}
			DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_pos_nor);
			if ((cache->cd_lused[CD_MLOOPUV] != 0) ||
			    (cache->cd_lused[CD_TANGENT] != 0))
			{
				DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_uv_tan);
			}
			if (cache->cd_lused[CD_MLOOPCOL] != 0) {
				DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_vcol);
			}
			/* TODO */
			// if (cache->cd_vused[CD_ORCO] != 0) {
			// 	DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_orco);
			// }
		}
	}

	/* Generate MeshRenderData flags */
	int mr_flag = 0, mr_edit_flag = 0;
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.pos_nor, MR_DATATYPE_VERT);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.weights, MR_DATATYPE_VERT | MR_DATATYPE_DVERT);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_pos_nor, MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_uv_tan, MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_SHADING);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_vcol, MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOP | MR_DATATYPE_SHADING);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->tess.pos_nor, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI | MR_DATATYPE_POLY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->tess.wireframe_data, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.surf_tris, MR_DATATYPE_VERT | MR_DATATYPE_LOOP |  MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.loops_tris, MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.loops_lines, MR_DATATYPE_LOOP | MR_DATATYPE_POLY);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edges_lines, MR_DATATYPE_VERT | MR_DATATYPE_EDGE);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edges_adj_lines, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.loose_edges_lines, MR_DATATYPE_VERT | MR_DATATYPE_EDGE);
	for (int i = 0; i < cache->mat_len; ++i) {
		DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->surf_per_mat_tris[i], MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);
	}

	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.data, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI | MR_DATATYPE_POLY | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.data_ledges, MR_DATATYPE_LOOSE_EDGE | MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.data_lverts, MR_DATATYPE_LOOSE_VERT | MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.pos_nor, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI | MR_DATATYPE_POLY | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.pos_nor_ledges, MR_DATATYPE_VERT | MR_DATATYPE_EDGE | MR_DATATYPE_LOOSE_EDGE | MR_DATATYPE_LOOP | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.pos_nor_lverts, MR_DATATYPE_VERT | MR_DATATYPE_LOOSE_VERT | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.pos_nor_data_facedots, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_POLY | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_edit_flag, cache->edit.lnor, MR_DATATYPE_VERT | MR_DATATYPE_LOOP | MR_DATATYPE_LOOPTRI | MR_DATATYPE_OVERLAY);
	DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_edit_flag, cache->ibo.edit_verts_points, MR_DATATYPE_VERT | MR_DATATYPE_POLY | MR_DATATYPE_LOOPTRI);

	Mesh *me_original = me;
	MBC_GET_FINAL_MESH(me);

	if (me_original == me) {
		mr_flag |= mr_edit_flag;
	}

	MeshRenderData *rdata = mesh_render_data_create_ex(me, mr_flag, cache->cd_vused, cache->cd_lused);

	/* Generate VBOs */
	if (DRW_vbo_requested(cache->ordered.pos_nor)) {
		mesh_create_pos_and_nor(rdata, cache->ordered.pos_nor);
	}
	if (DRW_vbo_requested(cache->ordered.weights)) {
		mesh_create_weights(rdata, cache->ordered.weights, &cache->weight_state);
	}
	if (DRW_vbo_requested(cache->ordered.loop_pos_nor)) {
		mesh_create_loop_pos_and_nor(rdata, cache->ordered.loop_pos_nor, use_face_sel);
	}
	if (DRW_vbo_requested(cache->ordered.loop_uv_tan)) {
		mesh_create_loop_uv_and_tan(rdata, cache->ordered.loop_uv_tan);
	}
	if (DRW_vbo_requested(cache->ordered.loop_vcol)) {
		mesh_create_loop_vcol(rdata, cache->ordered.loop_vcol);
	}
	if (DRW_vbo_requested(cache->tess.wireframe_data)) {
		mesh_create_wireframe_data_tess(rdata, cache->tess.wireframe_data);
	}
	if (DRW_vbo_requested(cache->tess.pos_nor)) {
		mesh_create_pos_and_nor_tess(rdata, cache->tess.pos_nor, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.edges_lines)) {
		mesh_create_edges_lines(rdata, cache->ibo.edges_lines, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.edges_adj_lines)) {
		mesh_create_edges_adjacency_lines(rdata, cache->ibo.edges_adj_lines, &cache->is_manifold, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.loose_edges_lines)) {
		mesh_create_loose_edges_lines(rdata, cache->ibo.loose_edges_lines, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.surf_tris)) {
		mesh_create_surf_tris(rdata, cache->ibo.surf_tris, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.loops_lines)) {
		mesh_create_loops_lines(rdata, cache->ibo.loops_lines, use_hide);
	}
	if (DRW_ibo_requested(cache->ibo.loops_tris)) {
		mesh_create_loops_tris(rdata, &cache->ibo.loops_tris, 1, use_hide);
	}
	if (DRW_ibo_requested(cache->surf_per_mat_tris[0])) {
		mesh_create_loops_tris(rdata, cache->surf_per_mat_tris, cache->mat_len, use_hide);
	}

	/* Use original Mesh* to have the correct edit cage. */
	if (me_original != me) {
		mesh_render_data_free(rdata);
		rdata = mesh_render_data_create(me_original, mr_edit_flag);
	}

	if (rdata->mapped.supported) {
		rdata->mapped.use = true;
	}

	if (DRW_vbo_requested(cache->edit.data) ||
	    DRW_vbo_requested(cache->edit.pos_nor) ||
	    DRW_vbo_requested(cache->edit.lnor) ||
	    DRW_ibo_requested(cache->ibo.edit_verts_points))
	{
		mesh_create_edit_tris_and_verts(
		        rdata,
		        cache->edit.data, cache->edit.pos_nor,
		        cache->edit.lnor, cache->ibo.edit_verts_points);
	}
	if (DRW_vbo_requested(cache->edit.data_ledges) || DRW_vbo_requested(cache->edit.pos_nor_ledges)) {
		mesh_create_edit_loose_edges(rdata, cache->edit.data_ledges, cache->edit.pos_nor_ledges);
	}
	if (DRW_vbo_requested(cache->edit.data_lverts) || DRW_vbo_requested(cache->edit.pos_nor_lverts)) {
		mesh_create_edit_loose_verts(rdata, cache->edit.data_lverts, cache->edit.pos_nor_lverts);
	}
	if (DRW_vbo_requested(cache->edit.pos_nor_data_facedots)) {
		mesh_create_edit_facedots(rdata, cache->edit.pos_nor_data_facedots);
	}

	mesh_render_data_free(rdata);

#ifdef DEBUG
	/* Make sure all requested batches have been setup. */
	for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); ++i) {
		BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
	}
#endif
}

/** \} */
