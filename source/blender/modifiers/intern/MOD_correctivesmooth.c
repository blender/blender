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
 * Contributor(s): Jack Simpson,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_correctivesmooth.c
 *  \ingroup modifiers
 *
 * Method of smoothing deformation, also known as 'delta-mush'.
 */

#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#include "BLI_strict_flags.h"


// #define DEBUG_TIME

#include "PIL_time.h"
#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

/* minor optimization, calculate this inline */
#define USE_TANGENT_CALC_INLINE

static void initData(ModifierData *md)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

	csmd->bind_coords = NULL;
	csmd->bind_coords_num = 0;

	csmd->lambda = 0.5f;
	csmd->repeat = 5;
	csmd->flag = 0;
	csmd->smooth_type = MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE;

	csmd->defgrp_name[0] = '\0';

	csmd->delta_cache = NULL;
}


static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	const CorrectiveSmoothModifierData *csmd = (const CorrectiveSmoothModifierData *)md;
	CorrectiveSmoothModifierData *tcsmd = (CorrectiveSmoothModifierData *)target;

	modifier_copyData_generic(md, target, flag);

	if (csmd->bind_coords) {
		tcsmd->bind_coords = MEM_dupallocN(csmd->bind_coords);
	}

	tcsmd->delta_cache = NULL;
	tcsmd->delta_cache_num = 0;
}


static void freeBind(CorrectiveSmoothModifierData *csmd)
{
	MEM_SAFE_FREE(csmd->bind_coords);
	MEM_SAFE_FREE(csmd->delta_cache);

	csmd->bind_coords_num = 0;
}


static void freeData(ModifierData *md)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;
	freeBind(csmd);
}


static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;
	CustomDataMask dataMask = 0;
	/* ask for vertex groups if we need them */
	if (csmd->defgrp_name[0]) {
		dataMask |= CD_MASK_MDEFORMVERT;
	}
	return dataMask;
}


/* check individual weights for changes and cache values */
static void mesh_get_weights(
        MDeformVert *dvert, const int defgrp_index,
        const unsigned int numVerts, const bool use_invert_vgroup,
        float *smooth_weights)
{
	unsigned int i;

	for (i = 0; i < numVerts; i++, dvert++) {
		const float w = defvert_find_weight(dvert, defgrp_index);

		if (use_invert_vgroup == false) {
			smooth_weights[i] = w;
		}
		else {
			smooth_weights[i] = 1.0f - w;
		}
	}
}


static void mesh_get_boundaries(Mesh *mesh, float *smooth_weights)
{
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	const MEdge *medge = mesh->medge;
	unsigned int mpoly_num, medge_num, i;
	unsigned short *boundaries;

	mpoly_num = (unsigned int)mesh->totpoly;
	medge_num = (unsigned int)mesh->totedge;

	boundaries = MEM_calloc_arrayN(medge_num, sizeof(*boundaries), __func__);

	/* count the number of adjacent faces */
	for (i = 0; i < mpoly_num; i++) {
		const MPoly *p = &mpoly[i];
		const int totloop = p->totloop;
		int j;
		for (j = 0; j < totloop; j++) {
			boundaries[mloop[p->loopstart + j].e]++;
		}
	}

	for (i = 0; i < medge_num; i++) {
		if (boundaries[i] == 1) {
			smooth_weights[medge[i].v1] = 0.0f;
			smooth_weights[medge[i].v2] = 0.0f;
		}
	}

	MEM_freeN(boundaries);
}


/* -------------------------------------------------------------------- */
/* Simple Weighted Smoothing
 *
 * (average of surrounding verts)
 */
static void smooth_iter__simple(
        CorrectiveSmoothModifierData *csmd, Mesh *mesh,
        float (*vertexCos)[3], unsigned int numVerts,
        const float *smooth_weights,
        unsigned int iterations)
{
	const float lambda = csmd->lambda;
	unsigned int i;

	const unsigned int numEdges = (unsigned int)mesh->totedge;
	const MEdge *edges = mesh->medge;
	float *vertex_edge_count_div;

	struct SmoothingData_Simple {
		float delta[3];
	} *smooth_data = MEM_calloc_arrayN(numVerts, sizeof(*smooth_data), __func__);

	vertex_edge_count_div = MEM_calloc_arrayN(numVerts, sizeof(float), __func__);

	/* calculate as floats to avoid int->float conversion in #smooth_iter */
	for (i = 0; i < numEdges; i++) {
		vertex_edge_count_div[edges[i].v1] += 1.0f;
		vertex_edge_count_div[edges[i].v2] += 1.0f;
	}

	/* a little confusing, but we can include 'lambda' and smoothing weight
	 * here to avoid multiplying for every iteration */
	if (smooth_weights == NULL) {
		for (i = 0; i < numVerts; i++) {
			vertex_edge_count_div[i] =
			        lambda * (vertex_edge_count_div[i] ? (1.0f / vertex_edge_count_div[i]) : 1.0f);
		}
	}
	else {
		for (i = 0; i < numVerts; i++) {
			vertex_edge_count_div[i] =
			        smooth_weights[i] * lambda * (vertex_edge_count_div[i] ? (1.0f / vertex_edge_count_div[i]) : 1.0f);
		}
	}

	/* -------------------------------------------------------------------- */
	/* Main Smoothing Loop */

	while (iterations--) {
		for (i = 0; i < numEdges; i++) {
			struct SmoothingData_Simple *sd_v1;
			struct SmoothingData_Simple *sd_v2;
			float edge_dir[3];

			sub_v3_v3v3(edge_dir, vertexCos[edges[i].v2], vertexCos[edges[i].v1]);

			sd_v1 = &smooth_data[edges[i].v1];
			sd_v2 = &smooth_data[edges[i].v2];

			add_v3_v3(sd_v1->delta, edge_dir);
			sub_v3_v3(sd_v2->delta, edge_dir);
		}


		for (i = 0; i < numVerts; i++) {
			struct SmoothingData_Simple *sd = &smooth_data[i];
			madd_v3_v3fl(vertexCos[i], sd->delta, vertex_edge_count_div[i]);
			/* zero for the next iteration (saves memset on entire array) */
			memset(sd, 0, sizeof(*sd));
		}
	}

	MEM_freeN(vertex_edge_count_div);
	MEM_freeN(smooth_data);
}


/* -------------------------------------------------------------------- */
/* Edge-Length Weighted Smoothing
 */
static void smooth_iter__length_weight(
        CorrectiveSmoothModifierData *csmd, Mesh *mesh,
        float (*vertexCos)[3], unsigned int numVerts,
        const float *smooth_weights,
        unsigned int iterations)
{
	const float eps = FLT_EPSILON * 10.0f;
	const unsigned int numEdges = (unsigned int)mesh->totedge;
	/* note: the way this smoothing method works, its approx half as strong as the simple-smooth,
	 * and 2.0 rarely spikes, double the value for consistent behavior. */
	const float lambda = csmd->lambda * 2.0f;
	const MEdge *edges = mesh->medge;
	float *vertex_edge_count;
	unsigned int i;

	struct SmoothingData_Weighted {
		float delta[3];
		float edge_length_sum;
	} *smooth_data = MEM_calloc_arrayN(numVerts, sizeof(*smooth_data), __func__);


	/* calculate as floats to avoid int->float conversion in #smooth_iter */
	vertex_edge_count = MEM_calloc_arrayN(numVerts, sizeof(float), __func__);
	for (i = 0; i < numEdges; i++) {
		vertex_edge_count[edges[i].v1] += 1.0f;
		vertex_edge_count[edges[i].v2] += 1.0f;
	}


	/* -------------------------------------------------------------------- */
	/* Main Smoothing Loop */

	while (iterations--) {
		for (i = 0; i < numEdges; i++) {
			struct SmoothingData_Weighted *sd_v1;
			struct SmoothingData_Weighted *sd_v2;
			float edge_dir[3];
			float edge_dist;

			sub_v3_v3v3(edge_dir, vertexCos[edges[i].v2], vertexCos[edges[i].v1]);
			edge_dist = len_v3(edge_dir);

			/* weight by distance */
			mul_v3_fl(edge_dir, edge_dist);


			sd_v1 = &smooth_data[edges[i].v1];
			sd_v2 = &smooth_data[edges[i].v2];

			add_v3_v3(sd_v1->delta, edge_dir);
			sub_v3_v3(sd_v2->delta, edge_dir);

			sd_v1->edge_length_sum += edge_dist;
			sd_v2->edge_length_sum += edge_dist;
		}

		if (smooth_weights == NULL) {
			/* fast-path */
			for (i = 0; i < numVerts; i++) {
				struct SmoothingData_Weighted *sd = &smooth_data[i];
				/* divide by sum of all neighbour distances (weighted) and amount of neighbors, (mean average) */
				const float div = sd->edge_length_sum * vertex_edge_count[i];
				if (div > eps) {
#if 0
					/* first calculate the new location */
					mul_v3_fl(sd->delta, 1.0f / div);
					/* then interpolate */
					madd_v3_v3fl(vertexCos[i], sd->delta, lambda);
#else
					/* do this in one step */
					madd_v3_v3fl(vertexCos[i], sd->delta, lambda / div);
#endif
				}
				/* zero for the next iteration (saves memset on entire array) */
				memset(sd, 0, sizeof(*sd));
			}
		}
		else {
			for (i = 0; i < numVerts; i++) {
				struct SmoothingData_Weighted *sd = &smooth_data[i];
				const float div = sd->edge_length_sum * vertex_edge_count[i];
				if (div > eps) {
					const float lambda_w = lambda * smooth_weights[i];
					madd_v3_v3fl(vertexCos[i], sd->delta, lambda_w / div);
				}

				memset(sd, 0, sizeof(*sd));
			}
		}
	}

	MEM_freeN(vertex_edge_count);
	MEM_freeN(smooth_data);
}


static void smooth_iter(
        CorrectiveSmoothModifierData *csmd, Mesh *mesh,
        float (*vertexCos)[3], unsigned int numVerts,
        const float *smooth_weights,
        unsigned int iterations)
{
	switch (csmd->smooth_type) {
		case MOD_CORRECTIVESMOOTH_SMOOTH_LENGTH_WEIGHT:
			smooth_iter__length_weight(csmd, mesh, vertexCos, numVerts, smooth_weights, iterations);
			break;

		/* case MOD_CORRECTIVESMOOTH_SMOOTH_SIMPLE: */
		default:
			smooth_iter__simple(csmd, mesh, vertexCos, numVerts, smooth_weights, iterations);
			break;
	}
}

static void smooth_verts(
        CorrectiveSmoothModifierData *csmd, Mesh *mesh,
        MDeformVert *dvert, const int defgrp_index,
        float (*vertexCos)[3], unsigned int numVerts)
{
	float *smooth_weights = NULL;

	if (dvert || (csmd->flag & MOD_CORRECTIVESMOOTH_PIN_BOUNDARY)) {

		smooth_weights = MEM_malloc_arrayN(numVerts, sizeof(float), __func__);

		if (dvert) {
			mesh_get_weights(
			        dvert, defgrp_index,
			        numVerts, (csmd->flag & MOD_CORRECTIVESMOOTH_INVERT_VGROUP) != 0,
			        smooth_weights);
		}
		else {
			copy_vn_fl(smooth_weights, (int)numVerts, 1.0f);
		}

		if (csmd->flag & MOD_CORRECTIVESMOOTH_PIN_BOUNDARY) {
			mesh_get_boundaries(mesh, smooth_weights);
		}
	}

	smooth_iter(csmd, mesh, vertexCos, numVerts, smooth_weights, (unsigned int)csmd->repeat);

	if (smooth_weights) {
		MEM_freeN(smooth_weights);
	}
}

/**
 * finalize after accumulation.
 */
static void calc_tangent_ortho(float ts[3][3])
{
	float v_tan_a[3], v_tan_b[3];
	float t_vec_a[3], t_vec_b[3];

	normalize_v3(ts[2]);

	copy_v3_v3(v_tan_a, ts[0]);
	copy_v3_v3(v_tan_b, ts[1]);

	cross_v3_v3v3(ts[1], ts[2], v_tan_a);
	mul_v3_fl(ts[1], dot_v3v3(ts[1], v_tan_b) < 0.0f ? -1.0f : 1.0f);

	/* orthognalise tangent */
	mul_v3_v3fl(t_vec_a, ts[2], dot_v3v3(ts[2], v_tan_a));
	sub_v3_v3v3(ts[0], v_tan_a, t_vec_a);

	/* orthognalise bitangent */
	mul_v3_v3fl(t_vec_a, ts[2], dot_v3v3(ts[2], ts[1]));
	mul_v3_v3fl(t_vec_b, ts[0], dot_v3v3(ts[0], ts[1]) / dot_v3v3(v_tan_a, v_tan_a));
	sub_v3_v3(ts[1], t_vec_a);
	sub_v3_v3(ts[1], t_vec_b);

	normalize_v3(ts[0]);
	normalize_v3(ts[1]);
}

/**
 * accumulate edge-vectors from all polys.
 */
static void calc_tangent_loop_accum(
        const float v_dir_prev[3],
        const float v_dir_next[3],
        float r_tspace[3][3])
{
	add_v3_v3v3(r_tspace[1], v_dir_prev, v_dir_next);

	if (compare_v3v3(v_dir_prev, v_dir_next, FLT_EPSILON * 10.0f) == false) {
		const float weight = fabsf(acosf(dot_v3v3(v_dir_next, v_dir_prev)));
		float nor[3];

		cross_v3_v3v3(nor, v_dir_prev, v_dir_next);
		normalize_v3(nor);

		cross_v3_v3v3(r_tspace[0], r_tspace[1], nor);

		mul_v3_fl(nor, weight);
		/* accumulate weighted normals */
		add_v3_v3(r_tspace[2], nor);
	}
}


static void calc_tangent_spaces(
        Mesh *mesh, float (*vertexCos)[3],
        float (*r_tangent_spaces)[3][3])
{
	const unsigned int mpoly_num = (unsigned int)mesh->totpoly;
#ifndef USE_TANGENT_CALC_INLINE
	const unsigned int mvert_num = (unsigned int)dm->getNumVerts(dm);
#endif
	const MPoly *mpoly = mesh->mpoly;
	const MLoop *mloop = mesh->mloop;
	unsigned int i;

	for (i = 0; i < mpoly_num; i++) {
		const MPoly *mp = &mpoly[i];
		const MLoop *l_next = &mloop[mp->loopstart];
		const MLoop *l_term = l_next + mp->totloop;
		const MLoop *l_prev = l_term - 2;
		const MLoop *l_curr = l_term - 1;

		/* loop directions */
		float v_dir_prev[3], v_dir_next[3];

		/* needed entering the loop */
		sub_v3_v3v3(v_dir_prev, vertexCos[l_prev->v], vertexCos[l_curr->v]);
		normalize_v3(v_dir_prev);

		for (;
		     l_next != l_term;
		     l_prev = l_curr, l_curr = l_next, l_next++)
		{
			float (*ts)[3] = r_tangent_spaces[l_curr->v];

			/* re-use the previous value */
#if 0
			sub_v3_v3v3(v_dir_prev, vertexCos[l_prev->v], vertexCos[l_curr->v]);
			normalize_v3(v_dir_prev);
#endif
			sub_v3_v3v3(v_dir_next, vertexCos[l_curr->v], vertexCos[l_next->v]);
			normalize_v3(v_dir_next);

			calc_tangent_loop_accum(v_dir_prev, v_dir_next, ts);

			copy_v3_v3(v_dir_prev, v_dir_next);
		}
	}

	/* do inline */
#ifndef USE_TANGENT_CALC_INLINE
	for (i = 0; i < mvert_num; i++) {
		float (*ts)[3] = r_tangent_spaces[i];
		calc_tangent_ortho(ts);
	}
#endif
}

/**
 * This calculates #CorrectiveSmoothModifierData.delta_cache
 * It's not run on every update (during animation for example).
 */
static void calc_deltas(
        CorrectiveSmoothModifierData *csmd, Mesh *mesh,
        MDeformVert *dvert, const int defgrp_index,
        const float (*rest_coords)[3], unsigned int numVerts)
{
	float (*smooth_vertex_coords)[3] = MEM_dupallocN(rest_coords);
	float (*tangent_spaces)[3][3];
	unsigned int i;

	tangent_spaces = MEM_calloc_arrayN(numVerts, sizeof(float[3][3]), __func__);

	if (csmd->delta_cache_num != numVerts) {
		MEM_SAFE_FREE(csmd->delta_cache);
	}

	/* allocate deltas if they have not yet been allocated, otheriwse we will just write over them */
	if (!csmd->delta_cache) {
		csmd->delta_cache_num = numVerts;
		csmd->delta_cache = MEM_malloc_arrayN(numVerts, sizeof(float[3]), __func__);
	}

	smooth_verts(csmd, mesh, dvert, defgrp_index, smooth_vertex_coords, numVerts);

	calc_tangent_spaces(mesh, smooth_vertex_coords, tangent_spaces);

	for (i = 0; i < numVerts; i++) {
		float imat[3][3], delta[3];

#ifdef USE_TANGENT_CALC_INLINE
		calc_tangent_ortho(tangent_spaces[i]);
#endif

		sub_v3_v3v3(delta, rest_coords[i], smooth_vertex_coords[i]);
		if (UNLIKELY(!invert_m3_m3(imat, tangent_spaces[i]))) {
			transpose_m3_m3(imat, tangent_spaces[i]);
		}
		mul_v3_m3v3(csmd->delta_cache[i], imat, delta);
	}

	MEM_freeN(tangent_spaces);
	MEM_freeN(smooth_vertex_coords);
}


static void correctivesmooth_modifier_do(
        ModifierData *md, Object *ob, Mesh *mesh,
        float (*vertexCos)[3], unsigned int numVerts,
        struct BMEditMesh *em)
{
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;

	const bool force_delta_cache_update =
	        /* XXX, take care! if mesh data its self changes we need to forcefully recalculate deltas */
	        ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO) &&
	         (((ID *)ob->data)->recalc & ID_RECALC));

	bool use_only_smooth = (csmd->flag & MOD_CORRECTIVESMOOTH_ONLY_SMOOTH) != 0;
	MDeformVert *dvert = NULL;
	int defgrp_index;

	MOD_get_vgroup(ob, mesh, csmd->defgrp_name, &dvert, &defgrp_index);

	/* if rest bind_coords not are defined, set them (only run during bind) */
	if ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) &&
	    /* signal to recalculate, whoever sets MUST also free bind coords */
	    (csmd->bind_coords_num == (unsigned int)-1))
	{
		BLI_assert(csmd->bind_coords == NULL);
		csmd->bind_coords = MEM_dupallocN(vertexCos);
		csmd->bind_coords_num = numVerts;
		BLI_assert(csmd->bind_coords != NULL);
	}

	if (UNLIKELY(use_only_smooth)) {
		smooth_verts(csmd, mesh, dvert, defgrp_index, vertexCos, numVerts);
		return;
	}

	if ((csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) && (csmd->bind_coords == NULL)) {
		modifier_setError(md, "Bind data required");
		goto error;
	}

	/* If the number of verts has changed, the bind is invalid, so we do nothing */
	if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
		if (csmd->bind_coords_num != numVerts) {
			modifier_setError(md, "Bind vertex count mismatch: %u to %u", csmd->bind_coords_num, numVerts);
			goto error;
		}
	}
	else {
		/* MOD_CORRECTIVESMOOTH_RESTSOURCE_ORCO */
		if (ob->type != OB_MESH) {
			modifier_setError(md, "Object is not a mesh");
			goto error;
		}
		else {
			unsigned int me_numVerts = (unsigned int)((em) ? em->bm->totvert : ((Mesh *)ob->data)->totvert);

			if (me_numVerts != numVerts) {
				modifier_setError(md, "Original vertex count mismatch: %u to %u", me_numVerts, numVerts);
				goto error;
			}
		}
	}

	/* check to see if our deltas are still valid */
	if (!csmd->delta_cache || (csmd->delta_cache_num != numVerts) || force_delta_cache_update) {
		const float (*rest_coords)[3];
		bool is_rest_coords_alloc = false;

		if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
			/* caller needs to do sanity check here */
			csmd->bind_coords_num = numVerts;
			rest_coords = (const float (*)[3])csmd->bind_coords;
		}
		else {
			int me_numVerts;
			rest_coords = (const float (*)[3]) ((em) ?
			        BKE_editmesh_vertexCos_get_orco(em, &me_numVerts) :
			        BKE_mesh_vertexCos_get(ob->data, &me_numVerts));

			BLI_assert((unsigned int)me_numVerts == numVerts);
			is_rest_coords_alloc = true;
		}

#ifdef DEBUG_TIME
	TIMEIT_START(corrective_smooth_deltas);
#endif

		calc_deltas(csmd, mesh, dvert, defgrp_index, rest_coords, numVerts);

#ifdef DEBUG_TIME
	TIMEIT_END(corrective_smooth_deltas);
#endif
		if (is_rest_coords_alloc) {
			MEM_freeN((void *)rest_coords);
		}
	}

	if (csmd->rest_source == MOD_CORRECTIVESMOOTH_RESTSOURCE_BIND) {
		/* this could be a check, but at this point it _must_ be valid */
		BLI_assert(csmd->bind_coords_num == numVerts && csmd->delta_cache);
	}


#ifdef DEBUG_TIME
	TIMEIT_START(corrective_smooth);
#endif

	/* do the actual delta mush */
	smooth_verts(csmd, mesh, dvert, defgrp_index, vertexCos, numVerts);

	{
		unsigned int i;

		float (*tangent_spaces)[3][3];

		/* calloc, since values are accumulated */
		tangent_spaces = MEM_calloc_arrayN(numVerts, sizeof(float[3][3]), __func__);

		calc_tangent_spaces(mesh, vertexCos, tangent_spaces);

		for (i = 0; i < numVerts; i++) {
			float delta[3];

#ifdef USE_TANGENT_CALC_INLINE
			calc_tangent_ortho(tangent_spaces[i]);
#endif

			mul_v3_m3v3(delta, tangent_spaces[i], csmd->delta_cache[i]);
			add_v3_v3(vertexCos[i], delta);
		}

		MEM_freeN(tangent_spaces);
	}

#ifdef DEBUG_TIME
	TIMEIT_END(corrective_smooth);
#endif

	return;

	/* when the modifier fails to execute */
error:
	MEM_SAFE_FREE(
        csmd->delta_cache);
	csmd->delta_cache_num = 0;

}


static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	correctivesmooth_modifier_do(md, ctx->object, mesh_src, vertexCos, (unsigned int)numVerts, NULL);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}


static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, editData, mesh, NULL, false, false);

	correctivesmooth_modifier_do(md, ctx->object, mesh_src, vertexCos, (unsigned int)numVerts, editData);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}


ModifierTypeInfo modifierType_CorrectiveSmooth = {
	/* name */              "CorrectiveSmooth",
	/* structName */        "CorrectiveSmoothModifierData",
	/* structSize */        sizeof(CorrectiveSmoothModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
