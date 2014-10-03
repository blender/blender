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
 *                 Campbell Barton,
 *                 Patrice Bertrand
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_array.c
 *  \ingroup modifiers
 *
 * Array modifier: duplicates the object multiple times along an axis.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_curve.h"
#include "BKE_modifier.h"

#include "MOD_util.h"

#include "depsgraph_private.h"

/* Due to cyclic dependencies it's possible that curve used for
 * deformation here is not evaluated at the time of evaluating
 * this modifier.
 */
#define CYCLIC_DEPENDENCY_WORKAROUND

static void initData(ModifierData *md)
{
	ArrayModifierData *amd = (ArrayModifierData *) md;

	/* default to 2 duplicates distributed along the x-axis by an
	 * offset of 1 object-width
	 */
	amd->start_cap = amd->end_cap = amd->curve_ob = amd->offset_ob = NULL;
	amd->count = 2;
	zero_v3(amd->offset);
	amd->scale[0] = 1;
	amd->scale[1] = amd->scale[2] = 0;
	amd->length = 0;
	amd->merge_dist = 0.01;
	amd->fit_type = MOD_ARR_FIXEDCOUNT;
	amd->offset_type = MOD_ARR_OFF_RELATIVE;
	amd->flags = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ArrayModifierData *amd = (ArrayModifierData *) md;
	ArrayModifierData *tamd = (ArrayModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        void (*walk)(void *userData, Object *ob, Object **obpoin),
        void *userData)
{
	ArrayModifierData *amd = (ArrayModifierData *) md;

	walk(userData, ob, &amd->start_cap);
	walk(userData, ob, &amd->end_cap);
	walk(userData, ob, &amd->curve_ob);
	walk(userData, ob, &amd->offset_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene), Object *UNUSED(ob), DagNode *obNode)
{
	ArrayModifierData *amd = (ArrayModifierData *) md;

	if (amd->start_cap) {
		DagNode *curNode = dag_get_node(forest, amd->start_cap);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->end_cap) {
		DagNode *curNode = dag_get_node(forest, amd->end_cap);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->curve_ob) {
		DagNode *curNode = dag_get_node(forest, amd->curve_ob);
		curNode->eval_flags |= DAG_EVAL_NEED_CURVE_PATH;

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->offset_ob) {
		DagNode *curNode = dag_get_node(forest, amd->offset_ob);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
}

static float vertarray_size(const MVert *mvert, int numVerts, int axis)
{
	int i;
	float min_co, max_co;

	/* if there are no vertices, width is 0 */
	if (numVerts == 0) return 0;

	/* find the minimum and maximum coordinates on the desired axis */
	min_co = max_co = mvert->co[axis];
	mvert++;
	for (i = 1; i < numVerts; ++i, ++mvert) {
		if (mvert->co[axis] < min_co) min_co = mvert->co[axis];
		if (mvert->co[axis] > max_co) max_co = mvert->co[axis];
	}

	return max_co - min_co;
}

BLI_INLINE float sum_v3(const float v[3])
{
	return v[0] + v[1] + v[2];
}

/* Structure used for sorting vertices, when processing doubles */
typedef struct SortVertsElem {
	int vertex_num;     /* The original index of the vertex, prior to sorting */
	float co[3];        /* Its coordinates */
	float sum_co;       /* sum_v3(co), just so we don't do the sum many times.  */
} SortVertsElem;


static int svert_sum_cmp(const void *e1, const void *e2)
{
	const SortVertsElem *sv1 = (SortVertsElem *)e1;
	const SortVertsElem *sv2 = (SortVertsElem *)e2;

	if      (sv1->sum_co > sv2->sum_co) return  1;
	else if (sv1->sum_co < sv2->sum_co) return -1;
	else                                return  0;
}

static void svert_from_mvert(SortVertsElem *sv, const MVert *mv, const int i_begin, const int i_end)
{
	int i;
	for (i = i_begin; i < i_end; i++, sv++, mv++) {
		sv->vertex_num = i;
		copy_v3_v3(sv->co, mv->co);
		sv->sum_co = sum_v3(mv->co);
	}
}

/**
 * Take as inputs two sets of verts, to be processed for detection of doubles and mapping.
 * Each set of verts is defined by its start within mverts array and its num_verts;
 * It builds a mapping for all vertices within source, to vertices within target, or -1 if no double found
 * The int doubles_map[num_verts_source] array must have been allocated by caller.
 */
static void dm_mvert_map_doubles(
        int *doubles_map,
        const MVert *mverts,
        const int target_start,
        const int target_num_verts,
        const int source_start,
        const int source_num_verts,
        const float dist,
        const bool with_follow)
{
	const float dist3 = (M_SQRT3 + 0.00005f) * dist;   /* Just above sqrt(3) */
	int i_source, i_target, i_target_low_bound, target_end, source_end;
	SortVertsElem *sorted_verts_target, *sorted_verts_source;
	SortVertsElem *sve_source, *sve_target, *sve_target_low_bound;
	bool target_scan_completed;

	target_end = target_start + target_num_verts;
	source_end = source_start + source_num_verts;

	/* build array of MVerts to be tested for merging */
	sorted_verts_target = MEM_mallocN(sizeof(SortVertsElem) * target_num_verts, __func__);
	sorted_verts_source = MEM_mallocN(sizeof(SortVertsElem) * source_num_verts, __func__);

	/* Copy target vertices index and cos into SortVertsElem array */
	svert_from_mvert(sorted_verts_target, mverts + target_start, target_start, target_end);

	/* Copy source vertices index and cos into SortVertsElem array */
	svert_from_mvert(sorted_verts_source, mverts + source_start, source_start, source_end);

	/* sort arrays according to sum of vertex coordinates (sumco) */
	qsort(sorted_verts_target, target_num_verts, sizeof(SortVertsElem), svert_sum_cmp);
	qsort(sorted_verts_source, source_num_verts, sizeof(SortVertsElem), svert_sum_cmp);

	sve_target_low_bound = sorted_verts_target;
	i_target_low_bound = 0;
	target_scan_completed = false;

	/* Scan source vertices, in SortVertsElem sorted array, */
	/* all the while maintaining the lower bound of possible doubles in target vertices */
	for (i_source = 0, sve_source = sorted_verts_source;
	     i_source < source_num_verts;
	     i_source++, sve_source++)
	{
		bool double_found;
		float sve_source_sumco;

		/* If source has already been assigned to a target (in an earlier call, with other chunks) */
		if (doubles_map[sve_source->vertex_num] != -1) {
			continue;
		}

		/* If target fully scanned already, then all remaining source vertices cannot have a double */
		if (target_scan_completed) {
			doubles_map[sve_source->vertex_num] = -1;
			continue;
		}

		sve_source_sumco = sum_v3(sve_source->co);

		/* Skip all target vertices that are more than dist3 lower in terms of sumco */
		/* and advance the overall lower bound, applicable to all remaining vertices as well. */
		while ((i_target_low_bound < target_num_verts) &&
		       (sve_target_low_bound->sum_co < sve_source_sumco - dist3))
		{
			i_target_low_bound++;
			sve_target_low_bound++;
		}
		/* If end of target list reached, then no more possible doubles */
		if (i_target_low_bound >= target_num_verts) {
			doubles_map[sve_source->vertex_num] = -1;
			target_scan_completed = true;
			continue;
		}
		/* Test target candidates starting at the low bound of possible doubles, ordered in terms of sumco */
		i_target = i_target_low_bound;
		sve_target = sve_target_low_bound;

		/* i_target will scan vertices in the [v_source_sumco - dist3;  v_source_sumco + dist3] range */

		double_found = false;
		while ((i_target < target_num_verts) &&
		       (sve_target->sum_co <= sve_source_sumco + dist3))
		{
			/* Testing distance for candidate double in target */
			/* v_target is within dist3 of v_source in terms of sumco;  check real distance */
			if (compare_len_v3v3(sve_source->co, sve_target->co, dist)) {
				/* Double found */
				/* If double target is itself already mapped to other vertex,
				 * behavior depends on with_follow option */
				int target_vertex = sve_target->vertex_num;
				if (doubles_map[target_vertex] != -1) {
					if (with_follow) { /* with_follow option:  map to initial target */
						target_vertex = doubles_map[target_vertex];
					}
					else {
						/* not with_follow: if target is mapped, then we do not map source, and stop searching  */
						break;
					}
				}
				doubles_map[sve_source->vertex_num] = target_vertex;
				double_found = true;
				break;
			}
			i_target++;
			sve_target++;
		}
		/* End of candidate scan: if none found then no doubles */
		if (!double_found) {
			doubles_map[sve_source->vertex_num] = -1;
		}
	}

	MEM_freeN(sorted_verts_source);
	MEM_freeN(sorted_verts_target);
}


static void dm_merge_transform(
        DerivedMesh *result, DerivedMesh *cap_dm, float cap_offset[4][4],
        unsigned int cap_verts_index, unsigned int cap_edges_index, int cap_loops_index, int cap_polys_index,
        int cap_nverts, int cap_nedges, int cap_nloops, int cap_npolys)
{
	int *index_orig;
	int i;
	MVert *mv;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;

	/* needed for subsurf so arrays are allocated */
	cap_dm->getVertArray(cap_dm);
	cap_dm->getEdgeArray(cap_dm);
	cap_dm->getNumLoops(cap_dm);
	cap_dm->getNumPolys(cap_dm);

	DM_copy_vert_data(cap_dm, result, 0, cap_verts_index, cap_nverts);
	DM_copy_edge_data(cap_dm, result, 0, cap_edges_index, cap_nedges);
	DM_copy_loop_data(cap_dm, result, 0, cap_loops_index, cap_nloops);
	DM_copy_poly_data(cap_dm, result, 0, cap_polys_index, cap_npolys);

	mv = CDDM_get_verts(result) + cap_verts_index;

	for (i = 0; i < cap_nverts; i++, mv++) {
		mul_m4_v3(cap_offset, mv->co);
		/* Reset MVert flags for caps */
		mv->flag = mv->bweight = 0;
	}

	/* adjust cap edge vertex indices */
	me = CDDM_get_edges(result) + cap_edges_index;
	for (i = 0; i < cap_nedges; i++, me++) {
		me->v1 += cap_verts_index;
		me->v2 += cap_verts_index;
	}

	/* adjust cap poly loopstart indices */
	mp = CDDM_get_polys(result) + cap_polys_index;
	for (i = 0; i < cap_npolys; i++, mp++) {
		mp->loopstart += cap_loops_index;
	}

	/* adjust cap loop vertex and edge indices */
	ml = CDDM_get_loops(result) + cap_loops_index;
	for (i = 0; i < cap_nloops; i++, ml++) {
		ml->v += cap_verts_index;
		ml->e += cap_edges_index;
	}

	/* set origindex */
	index_orig = result->getVertDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		fill_vn_i(index_orig + cap_verts_index, cap_nverts, ORIGINDEX_NONE);
	}

	index_orig = result->getEdgeDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		fill_vn_i(index_orig + cap_edges_index, cap_nedges, ORIGINDEX_NONE);
	}

	index_orig = result->getPolyDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		fill_vn_i(index_orig + cap_polys_index, cap_npolys, ORIGINDEX_NONE);
	}

	index_orig = result->getLoopDataArray(result, CD_ORIGINDEX);
	if (index_orig) {
		fill_vn_i(index_orig + cap_loops_index, cap_nloops, ORIGINDEX_NONE);
	}
}

static DerivedMesh *arrayModifier_doArray(
        ArrayModifierData *amd,
        Scene *scene, Object *ob, DerivedMesh *dm,
        ModifierApplyFlag flag)
{
	const float eps = 1e-6f;
	const MVert *src_mvert;
	MVert *mv, *mv_prev, *result_dm_verts;

	MEdge *me;
	MLoop *ml;
	MPoly *mp;
	int i, j, c, count;
	float length = amd->length;
	/* offset matrix */
	float offset[4][4];
	float scale[3];
	bool offset_has_scale;
	float current_offset[4][4];
	float final_offset[4][4];
	int *full_doubles_map = NULL;
	int tot_doubles;

	const bool use_merge = amd->flags & MOD_ARR_MERGE;
	const bool use_recalc_normals = (dm->dirty & DM_DIRTY_NORMALS) || use_merge;
	const bool use_offset_ob = ((amd->offset_type & MOD_ARR_OFF_OBJ) && amd->offset_ob);
	/* allow pole vertices to be used by many faces */
	const bool with_follow = use_offset_ob;

	int start_cap_nverts = 0, start_cap_nedges = 0, start_cap_npolys = 0, start_cap_nloops = 0;
	int end_cap_nverts = 0, end_cap_nedges = 0, end_cap_npolys = 0, end_cap_nloops = 0;
	int result_nverts = 0, result_nedges = 0, result_npolys = 0, result_nloops = 0;
	int chunk_nverts, chunk_nedges, chunk_nloops, chunk_npolys;
	int first_chunk_start, first_chunk_nverts, last_chunk_start, last_chunk_nverts;

	DerivedMesh *result, *start_cap_dm = NULL, *end_cap_dm = NULL;

	chunk_nverts = dm->getNumVerts(dm);
	chunk_nedges = dm->getNumEdges(dm);
	chunk_nloops = dm->getNumLoops(dm);
	chunk_npolys = dm->getNumPolys(dm);

	count = amd->count;

	if (amd->start_cap && amd->start_cap != ob && amd->start_cap->type == OB_MESH) {
		start_cap_dm = get_dm_for_modifier(amd->start_cap, flag);
		if (start_cap_dm) {
			start_cap_nverts = start_cap_dm->getNumVerts(start_cap_dm);
			start_cap_nedges = start_cap_dm->getNumEdges(start_cap_dm);
			start_cap_nloops = start_cap_dm->getNumLoops(start_cap_dm);
			start_cap_npolys = start_cap_dm->getNumPolys(start_cap_dm);
		}
	}
	if (amd->end_cap && amd->end_cap != ob && amd->end_cap->type == OB_MESH) {
		end_cap_dm = get_dm_for_modifier(amd->end_cap, flag);
		if (end_cap_dm) {
			end_cap_nverts = end_cap_dm->getNumVerts(end_cap_dm);
			end_cap_nedges = end_cap_dm->getNumEdges(end_cap_dm);
			end_cap_nloops = end_cap_dm->getNumLoops(end_cap_dm);
			end_cap_npolys = end_cap_dm->getNumPolys(end_cap_dm);
		}
	}

	/* Build up offset array, cumulating all settings options */

	unit_m4(offset);
	src_mvert = dm->getVertArray(dm);

	if (amd->offset_type & MOD_ARR_OFF_CONST)
		add_v3_v3v3(offset[3], offset[3], amd->offset);

	if (amd->offset_type & MOD_ARR_OFF_RELATIVE) {
		for (j = 0; j < 3; j++)
			offset[3][j] += amd->scale[j] * vertarray_size(src_mvert, chunk_nverts, j);
	}

	if (use_offset_ob) {
		float obinv[4][4];
		float result_mat[4][4];

		if (ob)
			invert_m4_m4(obinv, ob->obmat);
		else
			unit_m4(obinv);

		mul_m4_series(result_mat, offset,
		              obinv, amd->offset_ob->obmat);
		copy_m4_m4(offset, result_mat);
	}

	/* Check if there is some scaling.  If scaling, then we will not translate mapping */
	mat4_to_size(scale, offset);
	offset_has_scale = !is_one_v3(scale);

	if (amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob) {
		Curve *cu = amd->curve_ob->data;
		if (cu) {
#ifdef CYCLIC_DEPENDENCY_WORKAROUND
			if (amd->curve_ob->curve_cache == NULL) {
				BKE_displist_make_curveTypes(scene, amd->curve_ob, false);
			}
#endif

			if (amd->curve_ob->curve_cache && amd->curve_ob->curve_cache->path) {
				float scale = mat4_to_scale(amd->curve_ob->obmat);
				length = scale * amd->curve_ob->curve_cache->path->totdist;
			}
		}
	}

	/* calculate the maximum number of copies which will fit within the
	 * prescribed length */
	if (amd->fit_type == MOD_ARR_FITLENGTH || amd->fit_type == MOD_ARR_FITCURVE) {
		float dist = len_v3(offset[3]);

		if (dist > eps) {
			/* this gives length = first copy start to last copy end
			 * add a tiny offset for floating point rounding errors */
			count = (length + eps) / dist;
		}
		else {
			/* if the offset has no translation, just make one copy */
			count = 1;
		}
	}

	if (count < 1)
		count = 1;

	/* The number of verts, edges, loops, polys, before eventually merging doubles */
	result_nverts = chunk_nverts * count + start_cap_nverts + end_cap_nverts;
	result_nedges = chunk_nedges * count + start_cap_nedges + end_cap_nedges;
	result_nloops = chunk_nloops * count + start_cap_nloops + end_cap_nloops;
	result_npolys = chunk_npolys * count + start_cap_npolys + end_cap_npolys;

	/* Initialize a result dm */
	result = CDDM_from_template(dm, result_nverts, result_nedges, 0, result_nloops, result_npolys);
	result_dm_verts = CDDM_get_verts(result);

	if (use_merge) {
		/* Will need full_doubles_map for handling merge */
		full_doubles_map = MEM_mallocN(sizeof(int) * result_nverts, "mod array doubles map");
		fill_vn_i(full_doubles_map, result_nverts, -1);
	}

	/* copy customdata to original geometry */
	DM_copy_vert_data(dm, result, 0, 0, chunk_nverts);
	DM_copy_edge_data(dm, result, 0, 0, chunk_nedges);
	DM_copy_loop_data(dm, result, 0, 0, chunk_nloops);
	DM_copy_poly_data(dm, result, 0, 0, chunk_npolys);

	/* subsurf for eg wont have mesh data in the
	 * now add mvert/medge/mface layers */

	if (!CustomData_has_layer(&dm->vertData, CD_MVERT)) {
		dm->copyVertArray(dm, result_dm_verts);
	}
	if (!CustomData_has_layer(&dm->edgeData, CD_MEDGE)) {
		dm->copyEdgeArray(dm, CDDM_get_edges(result));
	}
	if (!CustomData_has_layer(&dm->polyData, CD_MPOLY)) {
		dm->copyLoopArray(dm, CDDM_get_loops(result));
		dm->copyPolyArray(dm, CDDM_get_polys(result));
	}

	/* Remember first chunk, in case of cap merge */
	first_chunk_start = 0;
	first_chunk_nverts = chunk_nverts;

	unit_m4(current_offset);
	for (c = 1; c < count; c++) {
		/* copy customdata to new geometry */
		DM_copy_vert_data(result, result, 0, c * chunk_nverts, chunk_nverts);
		DM_copy_edge_data(result, result, 0, c * chunk_nedges, chunk_nedges);
		DM_copy_loop_data(result, result, 0, c * chunk_nloops, chunk_nloops);
		DM_copy_poly_data(result, result, 0, c * chunk_npolys, chunk_npolys);

		mv_prev = result_dm_verts;
		mv = mv_prev + c * chunk_nverts;

		/* recalculate cumulative offset here */
		mul_m4_m4m4(current_offset, current_offset, offset);

		/* apply offset to all new verts */
		for (i = 0; i < chunk_nverts; i++, mv++, mv_prev++) {
			mul_m4_v3(current_offset, mv->co);

			/* We have to correct normals too, if we do not tag them as dirty! */
			if (!use_recalc_normals) {
				float no[3];
				normal_short_to_float_v3(no, mv->no);
				mul_mat3_m4_v3(current_offset, no);
				normalize_v3(no);
				normal_float_to_short_v3(mv->no, no);
			}
		}

		/* adjust edge vertex indices */
		me = CDDM_get_edges(result) + c * chunk_nedges;
		for (i = 0; i < chunk_nedges; i++, me++) {
			me->v1 += c * chunk_nverts;
			me->v2 += c * chunk_nverts;
		}

		mp = CDDM_get_polys(result) + c * chunk_npolys;
		for (i = 0; i < chunk_npolys; i++, mp++) {
			mp->loopstart += c * chunk_nloops;
		}

		/* adjust loop vertex and edge indices */
		ml = CDDM_get_loops(result) + c * chunk_nloops;
		for (i = 0; i < chunk_nloops; i++, ml++) {
			ml->v += c * chunk_nverts;
			ml->e += c * chunk_nedges;
		}

		/* Handle merge between chunk n and n-1 */
		if (use_merge && (c >= 1)) {
			if (!offset_has_scale && (c >= 2)) {
				/* Mapping chunk 3 to chunk 2 is a translation of mapping 2 to 1
				 * ... that is except if scaling makes the distance grow */
				int k;
				int this_chunk_index = c * chunk_nverts;
				int prev_chunk_index = (c - 1) * chunk_nverts;
				for (k = 0; k < chunk_nverts; k++, this_chunk_index++, prev_chunk_index++) {
					int target = full_doubles_map[prev_chunk_index];
					if (target != -1) {
						target += chunk_nverts; /* translate mapping */
						if (!with_follow) {
							/* The rule here is to not follow mapping to chunk N-2, which could be too far
							 * so if target vertex was itself mapped, then this vertex is not mapped */
							if (full_doubles_map[target] != -1) {
								target = -1;
							}
						}
					}
					full_doubles_map[this_chunk_index] = target;
				}
			}
			else {
				dm_mvert_map_doubles(
				        full_doubles_map,
				        result_dm_verts,
				        (c - 1) * chunk_nverts,
				        chunk_nverts,
				        c * chunk_nverts,
				        chunk_nverts,
				        amd->merge_dist,
				        with_follow);
			}
		}
	}

	last_chunk_start = (count - 1) * chunk_nverts;
	last_chunk_nverts = chunk_nverts;

	copy_m4_m4(final_offset, current_offset);

	if (use_merge && (amd->flags & MOD_ARR_MERGEFINAL) && (count > 1)) {
		/* Merge first and last copies */
		dm_mvert_map_doubles(
		        full_doubles_map,
		        result_dm_verts,
		        last_chunk_start,
		        last_chunk_nverts,
		        first_chunk_start,
		        first_chunk_nverts,
		        amd->merge_dist,
		        with_follow);
	}

	/* start capping */
	if (start_cap_dm) {
		float start_offset[4][4];
		int start_cap_start = result_nverts - start_cap_nverts - end_cap_nverts;
		invert_m4_m4(start_offset, offset);
		dm_merge_transform(
		        result, start_cap_dm, start_offset,
		        result_nverts - start_cap_nverts - end_cap_nverts,
		        result_nedges - start_cap_nedges - end_cap_nedges,
		        result_nloops - start_cap_nloops - end_cap_nloops,
		        result_npolys - start_cap_npolys - end_cap_npolys,
		        start_cap_nverts, start_cap_nedges, start_cap_nloops, start_cap_npolys);
		/* Identify doubles with first chunk */
		if (use_merge) {
			dm_mvert_map_doubles(
			        full_doubles_map,
			        result_dm_verts,
			        first_chunk_start,
			        first_chunk_nverts,
			        start_cap_start,
			        start_cap_nverts,
			        amd->merge_dist,
			        false);
		}
	}

	if (end_cap_dm) {
		float end_offset[4][4];
		int end_cap_start = result_nverts - end_cap_nverts;
		mul_m4_m4m4(end_offset, current_offset, offset);
		dm_merge_transform(
		        result, end_cap_dm, end_offset,
		        result_nverts - end_cap_nverts,
		        result_nedges - end_cap_nedges,
		        result_nloops - end_cap_nloops,
		        result_npolys - end_cap_npolys,
		        end_cap_nverts, end_cap_nedges, end_cap_nloops, end_cap_npolys);
		/* Identify doubles with last chunk */
		if (use_merge) {
			dm_mvert_map_doubles(
			        full_doubles_map,
			        result_dm_verts,
			        last_chunk_start,
			        last_chunk_nverts,
			        end_cap_start,
			        end_cap_nverts,
			        amd->merge_dist,
			        false);
		}
	}
	/* done capping */

	/* In case org dm has dirty normals, or we made some merging, mark normals as dirty in new dm!
	 * TODO: we may need to set other dirty flags as well?
	 */
	if (use_recalc_normals) {
		result->dirty |= DM_DIRTY_NORMALS;
	}

	/* Handle merging */
	tot_doubles = 0;
	if (use_merge) {
		for (i = 0; i < result_nverts; i++) {
			if (full_doubles_map[i] != -1) {
				if (i == full_doubles_map[i]) {
					full_doubles_map[i] = -1;
				}
				else {
					tot_doubles++;
				}
			}
		}
		if (tot_doubles > 0) {
			result = CDDM_merge_verts(result, full_doubles_map, tot_doubles, CDDM_MERGE_VERTS_DUMP_IF_EQUAL);
		}
		MEM_freeN(full_doubles_map);
	}
	return result;
}


static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	ArrayModifierData *amd = (ArrayModifierData *) md;
	return arrayModifier_doArray(amd, md->scene, ob, dm, flag);
}


ModifierTypeInfo modifierType_Array = {
	/* name */              "Array",
	/* structName */        "ArrayModifierData",
	/* structSize */        sizeof(ArrayModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
