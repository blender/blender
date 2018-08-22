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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_bevel.c
 *  \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "MOD_util.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph_query.h"

static void initData(ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *) md;

	bmd->value = 0.1f;
	bmd->res = 1;
	bmd->flags = 0;
	bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
	bmd->lim_flags = 0;
	bmd->e_flags = 0;
	bmd->edge_flags = 0;
	bmd->mat = -1;
	bmd->profile = 0.5f;
	bmd->bevel_angle = DEG2RADF(30.0f);
	bmd->defgrp_name[0] = '\0';
	bmd->hnmode = MOD_BEVEL_HN_NONE;
	bmd->hn_strength = 0.5f;
	bmd->clnordata.faceHash = NULL;
}

static void copyData(const ModifierData *md_src, ModifierData *md_dst, const int UNUSED(flag))
{
	BevelModifierData *bmd_src = (BevelModifierData *)md_src;
	BevelModifierData *bmd_dst = (BevelModifierData *)md_dst;

	*bmd_dst = *bmd_src;
	bmd_dst->clnordata.faceHash = NULL;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (bmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void bevel_set_weighted_normal_face_strength(BMesh *bm, Scene *scene)
{
	BMFace *f;
	BMIter fiter;
	const char *wn_layer_id = MOD_WEIGHTEDNORMALS_FACEWEIGHT_CDLAYER_ID;
	int cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT, wn_layer_id);

	if (cd_prop_int_idx == -1) {
		BM_data_layer_add_named(bm, &bm->pdata, CD_PROP_INT, wn_layer_id);
		cd_prop_int_idx = CustomData_get_named_layer_index(&bm->pdata, CD_PROP_INT, wn_layer_id);
	}
	cd_prop_int_idx -= CustomData_get_layer_index(&bm->pdata, CD_PROP_INT);
	const int cd_prop_int_offset = CustomData_get_n_offset(&bm->pdata, CD_PROP_INT, cd_prop_int_idx);

	const int face_strength = scene->toolsettings->face_strength;

	BM_ITER_MESH(f, &fiter, bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
			int *strength = BM_ELEM_CD_GET_VOID_P(f, cd_prop_int_offset);
			*strength = face_strength;
		}
	}
}

static void bevel_mod_harden_normals(
        BevelModifierData *bmd, BMesh *bm, const float hn_strength,
        const int hnmode, MDeformVert *dvert, int vgroup)
{
	if (bmd->res > 20 || bmd->value == 0)
		return;

	BM_mesh_normals_update(bm);
	BM_lnorspace_update(bm);
	BM_normals_loops_edges_tag(bm, true);

	const bool vertex_only = (bmd->flags & MOD_BEVEL_VERT) != 0;
	const int cd_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
	const bool do_normal_to_recon = (hn_strength == 1.0f);

	BMFace *f;
	BMLoop *l, *l_cur, *l_first;
	BMIter fiter;
	GHash *faceHash = bmd->clnordata.faceHash;

	/* Iterate throught all loops of a face */
	BM_ITER_MESH(f, &fiter, bm, BM_FACES_OF_MESH) {

		l_cur = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if ((!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG)) ||
			    (!BM_elem_flag_test(l_cur, BM_ELEM_TAG) && BM_loop_check_cyclic_smooth_fan(l_cur)))
			{

				/* previous and next edge is sharp, accumulate face normals into loop */
				if (!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG) && !BM_elem_flag_test(l_cur->prev->e, BM_ELEM_TAG)) {
					const int loop_index = BM_elem_index_get(l_cur);
					short *clnors = BM_ELEM_CD_GET_VOID_P(l_cur, cd_clnors_offset);
					BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[loop_index], f->no, clnors);
				}
				else {
					BMVert *v_pivot = l_cur->v;
					BMEdge *e_next;
					const BMEdge *e_org = l_cur->e;
					BMLoop *lfan_pivot, *lfan_pivot_next;
					UNUSED_VARS_NDEBUG(v_pivot);

					lfan_pivot = l_cur;
					e_next = lfan_pivot->e;
					BLI_SMALLSTACK_DECLARE(loops, BMLoop *);
					float cn_wght[3] = { 0.0f, 0.0f, 0.0f };
					int recon_face_count = 0;		/* Counts number of reconstructed faces current vert is connected to */
					BMFace *recon_face = NULL;		/* Reconstructed face */

					while (true) {
						lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot, &e_next);
						if (lfan_pivot_next) {
							BLI_assert(lfan_pivot_next->v == v_pivot);
						}
						else {
							e_next = (lfan_pivot->e == e_next) ? lfan_pivot->prev->e : lfan_pivot->e;
						}

						BLI_SMALLSTACK_PUSH(loops, lfan_pivot);

						if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
							int weight = BM_elem_float_data_get(&bm->edata, lfan_pivot->f, CD_BWEIGHT);
							if (weight) {
								if (hnmode == MOD_BEVEL_HN_FACE) {
									float cur[3];					//Add area weighted face normals
									mul_v3_v3fl(cur, lfan_pivot->f->no, BM_face_calc_area(lfan_pivot->f));
									add_v3_v3(cn_wght, cur);
								}
								else
									add_v3_v3(cn_wght, lfan_pivot->f->no);		//Else simply add face normals
							}
							else
								add_v3_v3(cn_wght, lfan_pivot->f->no);

						}
						else if (bmd->lim_flags & MOD_BEVEL_VGROUP) {
							const bool has_vgroup = dvert != NULL;
							const bool vert_of_group = (
							        has_vgroup &&
							        (defvert_find_index(&dvert[BM_elem_index_get(l->v)], vgroup) != NULL));

							if (vert_of_group && hnmode == MOD_BEVEL_HN_FACE) {
								float cur[3];
								mul_v3_v3fl(cur, lfan_pivot->f->no, BM_face_calc_area(lfan_pivot->f));
								add_v3_v3(cn_wght, cur);
							}
							else
								add_v3_v3(cn_wght, lfan_pivot->f->no);
						}
						else {
							float cur[3];
							mul_v3_v3fl(cur, lfan_pivot->f->no, BM_face_calc_area(lfan_pivot->f));
							add_v3_v3(cn_wght, cur);
						}
						if (!BLI_ghash_haskey(faceHash, lfan_pivot->f)) {
							recon_face = f;
							recon_face_count++;
						}
						if (!BM_elem_flag_test(e_next, BM_ELEM_TAG) || (e_next == e_org)) {
							break;
						}
						lfan_pivot = lfan_pivot_next;
					}

					normalize_v3(cn_wght);
					mul_v3_fl(cn_wght, hn_strength);
					float n_final[3];

					while ((l = BLI_SMALLSTACK_POP(loops))) {
						const int l_index = BM_elem_index_get(l);
						short *clnors = BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);

						/* If vertex is edge vert with 1 reconnected face */
						if (recon_face_count == 1 || (recon_face != NULL && do_normal_to_recon)) {
							BKE_lnor_space_custom_normal_to_data(
							        bm->lnor_spacearr->lspacearr[l_index], recon_face->no, clnors);
						}
						else if (vertex_only == false || recon_face_count == 0) {
							copy_v3_v3(n_final, l->f->no);
							mul_v3_fl(n_final, 1.0f - hn_strength);
							add_v3_v3(n_final, cn_wght);
							normalize_v3(n_final);
							BKE_lnor_space_custom_normal_to_data(
							        bm->lnor_spacearr->lspacearr[l_index], n_final, clnors);
						}
						else if (BLI_ghash_haskey(faceHash, l->f)) {
							BKE_lnor_space_custom_normal_to_data(
							        bm->lnor_spacearr->lspacearr[l_index], l->v->no, clnors);
						}
					}
				}
			}
		} while ((l_cur = l_cur->next) != l_first);
	}
}

static void bevel_fix_normal_shading_continuity(BevelModifierData *bmd, BMesh *bm)
{
	const bool vertex_only = (bmd->flags & MOD_BEVEL_VERT) != 0;
	if (bmd->value == 0 || (bmd->clnordata.faceHash == NULL && vertex_only))
		return;

	BM_mesh_normals_update(bm);
	BM_lnorspace_update(bm);

	GHash *faceHash = bmd->clnordata.faceHash;
	BMEdge *e;
	BMLoop *l;
	BMIter liter, eiter;

	const int cd_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
	const float hn_strength = bmd->hn_strength;
	float ref = 10.0f;

	BM_ITER_MESH(e, &eiter, bm, BM_EDGES_OF_MESH) {
		BMFace *f_a, *f_b;
		BM_edge_face_pair(e, &f_a, &f_b);

		bool has_f_a = false, has_f_b = false;
		if (f_a)
			has_f_a = BLI_ghash_haskey(faceHash, f_a);
		if (f_b)
			has_f_b = BLI_ghash_haskey(faceHash, f_b);
		if (has_f_a ^ has_f_b) {
		/* If one of both faces is present in faceHash then we are at a border
		*  between new vmesh created and reconstructed face */

			for (int i = 0; i < 2; i++) {
				BMVert *v = (i == 0) ? e->v1 : e->v2;
				BM_ITER_ELEM(l, &liter, v, BM_LOOPS_OF_VERT) {

					if (l->f == f_a || l->f == f_b) {
						const int l_index = BM_elem_index_get(l);
						short *clnors = BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);
						float n_final[3], pow_a[3], pow_b[3];

						zero_v3(n_final);
						copy_v3_v3(pow_a, f_a->no);
						copy_v3_v3(pow_b, f_b->no);
						if (has_f_a) {
							mul_v3_fl(pow_a, bmd->res / ref);
							mul_v3_fl(pow_b, ref / bmd->res);
						}
						else {
							mul_v3_fl(pow_b, bmd->res / ref);
							mul_v3_fl(pow_a, ref / bmd->res);
						}
						add_v3_v3(n_final, pow_a);
						add_v3_v3(n_final, pow_b);
						normalize_v3(n_final);

						BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], n_final, clnors);
					}
				}
			}
		}
		else if (has_f_a == true  && has_f_b == true) {
		/* Else if both faces are present we assign clnor corresponding
		*  to vert normal and face normal */
			for (int i = 0; i < 2; i++) {
				BMVert *v = (i == 0) ? e->v1 : e->v2;
				BM_ITER_ELEM(l, &liter, v, BM_LOOPS_OF_VERT) {

					if (l->f == f_a || l->f == f_b) {
						const int l_index = BM_elem_index_get(l);
						short *clnors = BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);
						float n_final[3], cn_wght[3];

						copy_v3_v3(n_final, v->no);
						mul_v3_fl(n_final, hn_strength);

						copy_v3_v3(cn_wght, l->f->no);
						mul_v3_fl(cn_wght, 1.0f - hn_strength);

						add_v3_v3(n_final, cn_wght);
						normalize_v3(n_final);
						BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], n_final, clnors);
					}
				}
			}
		}
	}
}

/*
 * This calls the new bevel code (added since 2.64)
 */
static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
	Mesh *result;
	BMesh *bm;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	float weight, weight2;
	int vgroup = -1;
	MDeformVert *dvert = NULL;
	BevelModifierData *bmd = (BevelModifierData *) md;
	const float threshold = cosf(bmd->bevel_angle + 0.000000175f);
	const bool vertex_only = (bmd->flags & MOD_BEVEL_VERT) != 0;
	const bool do_clamp = !(bmd->flags & MOD_BEVEL_OVERLAP_OK);
	const int offset_type = bmd->val_flags;
	const int mat = CLAMPIS(bmd->mat, -1, ctx->object->totcol - 1);
	const bool loop_slide = (bmd->flags & MOD_BEVEL_EVEN_WIDTHS) == 0;
	const bool mark_seam = (bmd->edge_flags & MOD_BEVEL_MARK_SEAM);
	const bool mark_sharp = (bmd->edge_flags & MOD_BEVEL_MARK_SHARP);
	const bool set_wn_strength = (bmd->flags & MOD_BEVEL_SET_WN_STR);

	struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

	bm = BKE_mesh_to_bmesh_ex(
	        mesh,
	        &(struct BMeshCreateParams){0},
	        &(struct BMeshFromMeshParams){
	            .calc_face_normal = true,
	            .add_key_index = false,
	            .use_shapekey = true,
	            .active_shapekey = ctx->object->shapenr,
	        });

	if ((bmd->lim_flags & MOD_BEVEL_VGROUP) && bmd->defgrp_name[0])
		MOD_get_vgroup(ctx->object, mesh, bmd->defgrp_name, &dvert, &vgroup);

	if (vertex_only) {
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_vert_is_manifold(v))
				continue;
			if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
				weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
				if (weight == 0.0f)
					continue;
			}
			else if (vgroup != -1) {
				weight = defvert_array_find_weight_safe(dvert, BM_elem_index_get(v), vgroup);
				/* Check is against 0.5 rather than != 0.0 because cascaded bevel modifiers will
				 * interpolate weights for newly created vertices, and may cause unexpected "selection" */
				if (weight < 0.5f)
					continue;
			}
			BM_elem_flag_enable(v, BM_ELEM_TAG);
		}
	}
	else if (bmd->lim_flags & MOD_BEVEL_ANGLE) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			/* check for 1 edge having 2 face users */
			BMLoop *l_a, *l_b;
			if (BM_edge_loop_pair(e, &l_a, &l_b)) {
				if (dot_v3v3(l_a->f->no, l_b->f->no) < threshold) {
					BM_elem_flag_enable(e, BM_ELEM_TAG);
					BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
					BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
				}
			}
		}
	}
	else {
		/* crummy, is there a way just to operator on all? - campbell */
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_edge_is_manifold(e)) {
				if (bmd->lim_flags & MOD_BEVEL_WEIGHT) {
					weight = BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT);
					if (weight == 0.0f)
						continue;
				}
				else if (vgroup != -1) {
					weight = defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v1), vgroup);
					weight2 = defvert_array_find_weight_safe(dvert, BM_elem_index_get(e->v2), vgroup);
					if (weight < 0.5f || weight2 < 0.5f)
						continue;
				}
				BM_elem_flag_enable(e, BM_ELEM_TAG);
				BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
				BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
			}
		}
	}

	BM_mesh_bevel(bm, bmd->value, offset_type, bmd->res, bmd->profile,
	              vertex_only, bmd->lim_flags & MOD_BEVEL_WEIGHT, do_clamp,
	              dvert, vgroup, mat, loop_slide, mark_seam, mark_sharp, bmd->hnmode, &bmd->clnordata);

	if (bmd->hnmode != BEVEL_HN_FIX_SHA && bmd->hnmode != MOD_BEVEL_HN_NONE) {
		bevel_mod_harden_normals(bmd, bm, bmd->hn_strength, bmd->hnmode, dvert, vgroup);
	}
	if (bmd->hnmode == BEVEL_HN_FIX_SHA)
		bevel_fix_normal_shading_continuity(bmd, bm);
	if (set_wn_strength)
		bevel_set_weighted_normal_face_strength(bm, scene);

	result = BKE_bmesh_to_mesh_nomain(bm, &(struct BMeshToMeshParams){0});

	BLI_assert(bm->vtoolflagpool == NULL &&
	           bm->etoolflagpool == NULL &&
	           bm->ftoolflagpool == NULL);  /* make sure we never alloc'd these */
	BM_mesh_free(bm);

	if (bmd->clnordata.faceHash)
		BLI_ghash_free(bmd->clnordata.faceHash, NULL, NULL);

	result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

	return result;
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
	return true;
}

ModifierTypeInfo modifierType_Bevel = {
	/* name */              "Bevel",
	/* structName */        "BevelModifierData",
	/* structSize */        sizeof(BevelModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
