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

/** \file blender/modifiers/intern/MOD_array.c
 *  \ingroup modifiers
 */


/* Array modifier: duplicates the object multiple times along an axis */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_ghash.h"
#include "BLI_edgehash.h"

#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_tessmesh.h"

#include "depsgraph_private.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
	ArrayModifierData *amd = (ArrayModifierData *) md;
	ArrayModifierData *tamd = (ArrayModifierData *) target;

	tamd->start_cap = amd->start_cap;
	tamd->end_cap = amd->end_cap;
	tamd->curve_ob = amd->curve_ob;
	tamd->offset_ob = amd->offset_ob;
	tamd->count = amd->count;
	copy_v3_v3(tamd->offset, amd->offset);
	copy_v3_v3(tamd->scale, amd->scale);
	tamd->length = amd->length;
	tamd->merge_dist = amd->merge_dist;
	tamd->fit_type = amd->fit_type;
	tamd->offset_type = amd->offset_type;
	tamd->flags = amd->flags;
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

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->offset_ob) {
		DagNode *curNode = dag_get_node(forest, amd->offset_ob);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
}

static float vertarray_size(MVert *mvert, int numVerts, int axis)
{
	int i;
	float min_co, max_co;

	/* if there are no vertices, width is 0 */
	if (numVerts == 0) return 0;

	/* find the minimum and maximum coordinates on the desired axis */
	min_co = max_co = mvert->co[axis];
	++mvert;
	for (i = 1; i < numVerts; ++i, ++mvert) {
		if (mvert->co[axis] < min_co) min_co = mvert->co[axis];
		if (mvert->co[axis] > max_co) max_co = mvert->co[axis];
	}

	return max_co - min_co;
}

static int *find_doubles_index_map(BMesh *bm, BMOperator *dupe_op,
                                   const ArrayModifierData *amd,
                                   int *index_map_length)
{
	BMOperator find_op;
	BMOIter oiter;
	BMVert *v, *v2;
	BMElem *ele;
	int *index_map, i;

	BMO_op_initf(bm, &find_op,
	             "finddoubles verts=%av dist=%f keepverts=%s",
	             amd->merge_dist, dupe_op, "geom");

	BMO_op_exec(bm, &find_op);
			
	i = 0;
	BMO_ITER (ele, &oiter, bm, dupe_op, "geom", BM_ALL) {
		BM_elem_index_set(ele, i); /* set_dirty */
		i++;
	}

	BMO_ITER (ele, &oiter, bm, dupe_op, "newout", BM_ALL) {
		BM_elem_index_set(ele, i); /* set_dirty */
		i++;
	}
	/* above loops over all, so set all to dirty, if this is somehow
	 * setting valid values, this line can be remvoed - campbell */
	bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;

	(*index_map_length) = i;
	index_map = MEM_callocN(sizeof(int) * (*index_map_length), "index_map");

	/*element type argument doesn't do anything here*/
	BMO_ITER (v, &oiter, bm, &find_op, "targetmapout", 0) {
		v2 = BMO_iter_map_value_p(&oiter);

		index_map[BM_elem_index_get(v)] = BM_elem_index_get(v2) + 1;
	}

	BMO_op_finish(bm, &find_op);

	return index_map;
}

/* Used for start/end cap.
 *
 * this function expects all existing vertices to be tagged,
 * so we can know new verts are not tagged.
 *
 * All verts will be tagged on exit.
 */
static void bm_merge_dm_transform(BMesh *bm, DerivedMesh *dm, float mat[4][4],
                                  const ArrayModifierData *amd,
                                  BMOperator *dupe_op,
                                  const char *dupe_slot_name,
                                  BMOperator *weld_op)
{
	BMVert *v, *v2;
	BMIter iter;

	DM_to_bmesh_ex(dm, bm);

	if (amd->flags & MOD_ARR_MERGE) {
		/* if merging is enabled, find doubles */
		
		BMOIter oiter;
		BMOperator find_op;

		BMO_op_initf(bm, &find_op,
		             "finddoubles verts=%Hv dist=%f keepverts=%s",
		             BM_ELEM_TAG, amd->merge_dist,
		             dupe_op, dupe_slot_name);

		/* append the dupe's geom to the findop input verts */
		BMO_slot_buffer_append(&find_op, "verts", dupe_op, dupe_slot_name);

		/* transform and tag verts */
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
				mul_m4_v3(mat, v->co);
				BM_elem_flag_enable(v, BM_ELEM_TAG);
			}
		}

		BMO_op_exec(bm, &find_op);

		/* add new merge targets to weld operator */
		BMO_ITER (v, &oiter, bm, &find_op, "targetmapout", 0) {
			v2 = BMO_iter_map_value_p(&oiter);
			BMO_slot_map_ptr_insert(bm, weld_op, "targetmap", v, v2);
		}

		BMO_op_finish(bm, &find_op);
	}
	else {
		/* transform and tag verts */
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
				mul_m4_v3(mat, v->co);
				BM_elem_flag_enable(v, BM_ELEM_TAG);
			}
		}
	}
}

static void merge_first_last(BMesh *bm,
                             const ArrayModifierData *amd,
                             BMOperator *dupe_first,
                             BMOperator *dupe_last,
                             BMOperator *weld_op)
{
	BMOperator find_op;
	BMOIter oiter;
	BMVert *v, *v2;

	BMO_op_initf(bm, &find_op,
	             "finddoubles verts=%s dist=%f keepverts=%s",
	             dupe_first, "geom", amd->merge_dist,
	             dupe_first, "geom");

	/* append the last dupe's geom to the findop input verts */
	BMO_slot_buffer_append(&find_op, "verts", dupe_last, "newout");

	BMO_op_exec(bm, &find_op);

	/* add new merge targets to weld operator */
	BMO_ITER (v, &oiter, bm, &find_op, "targetmapout", 0) {
		v2 = BMO_iter_map_value_p(&oiter);
		BMO_slot_map_ptr_insert(bm, weld_op, "targetmap", v, v2);
	}

	BMO_op_finish(bm, &find_op);
}

static DerivedMesh *arrayModifier_doArray(ArrayModifierData *amd,
                                          Scene *scene, Object *ob, DerivedMesh *dm,
                                          int UNUSED(initFlags))
{
	DerivedMesh *result;
	BMEditMesh *em = DM_to_editbmesh(dm, NULL, FALSE);
	BMOperator first_dupe_op, dupe_op, old_dupe_op, weld_op;
	BMVert **first_geom = NULL;
	int i, j, indexLen;
	/* offset matrix */
	float offset[4][4];
	float final_offset[4][4];
	float tmp_mat[4][4];
	float length = amd->length;
	int count = amd->count, maxVerts;
	int *indexMap = NULL;
	DerivedMesh *start_cap = NULL, *end_cap = NULL;
	MVert *src_mvert;

	/* need to avoid infinite recursion here */
	if (amd->start_cap && amd->start_cap != ob)
		start_cap = mesh_get_derived_final(scene, amd->start_cap, CD_MASK_MESH);
	if (amd->end_cap && amd->end_cap != ob)
		end_cap = mesh_get_derived_final(scene, amd->end_cap, CD_MASK_MESH);

	unit_m4(offset);

	src_mvert = dm->getVertArray(dm);
	maxVerts = dm->getNumVerts(dm);

	if (amd->offset_type & MOD_ARR_OFF_CONST)
		add_v3_v3v3(offset[3], offset[3], amd->offset);
	if (amd->offset_type & MOD_ARR_OFF_RELATIVE) {
		for (j = 0; j < 3; j++)
			offset[3][j] += amd->scale[j] * vertarray_size(src_mvert, maxVerts, j);
	}

	if ((amd->offset_type & MOD_ARR_OFF_OBJ) && (amd->offset_ob)) {
		float obinv[4][4];
		float result_mat[4][4];

		if (ob)
			invert_m4_m4(obinv, ob->obmat);
		else
			unit_m4(obinv);

		mul_serie_m4(result_mat, offset,
		             obinv, amd->offset_ob->obmat,
		             NULL, NULL, NULL, NULL, NULL);
		copy_m4_m4(offset, result_mat);
	}

	if (amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob) {
		Curve *cu = amd->curve_ob->data;
		if (cu) {
			float tmp_mat[3][3];
			float scale;
			
			BKE_object_to_mat3(amd->curve_ob, tmp_mat);
			scale = mat3_to_scale(tmp_mat);
				
			if (!cu->path) {
				cu->flag |= CU_PATH; // needed for path & bevlist
				BKE_displist_make_curveTypes(scene, amd->curve_ob, 0);
			}
			if (cu->path)
				length = scale * cu->path->totdist;
		}
	}

	/* calculate the maximum number of copies which will fit within the
	 * prescribed length */
	if (amd->fit_type == MOD_ARR_FITLENGTH || amd->fit_type == MOD_ARR_FITCURVE) {
		float dist = sqrt(dot_v3v3(offset[3], offset[3]));

		if (dist > 1e-6f)
			/* this gives length = first copy start to last copy end
			 * add a tiny offset for floating point rounding errors */
			count = (length + 1e-6f) / dist;
		else
			/* if the offset has no translation, just make one copy */
			count = 1;
	}

	if (count < 1)
		count = 1;

	/* calculate the offset matrix of the final copy (for merging) */
	unit_m4(final_offset);

	for (j = 0; j < count - 1; j++) {
		mult_m4_m4m4(tmp_mat, offset, final_offset);
		copy_m4_m4(final_offset, tmp_mat);
	}

	/* BMESH_TODO: bumping up the stack level avoids computing the normals
	 * after every top-level operator execution (and this modifier has the
	 * potential to execute a *lot* of top-level BMOps. There should be a
	 * cleaner way to do this. One possibility: a "mirror" BMOp would
	 * certainly help by compressing it all into one top-level BMOp that
	 * executes a lot of second-level BMOps. */
	BMO_push(em->bm, NULL);
	bmesh_edit_begin(em->bm, 0);

	if (amd->flags & MOD_ARR_MERGE)
		BMO_op_init(em->bm, &weld_op, "weldverts");

	BMO_op_initf(em->bm, &dupe_op, "dupe geom=%avef");
	first_dupe_op = dupe_op;

	for (j = 0; j < count - 1; j++) {
		BMVert *v, *v2, *v3;
		BMOpSlot *geom_slot;
		BMOpSlot *newout_slot;
		BMOIter oiter;

		if (j != 0)
			BMO_op_initf(em->bm, &dupe_op, "dupe geom=%s", &old_dupe_op, "newout");
		BMO_op_exec(em->bm, &dupe_op);

		geom_slot = BMO_slot_get(&dupe_op, "geom");
		newout_slot = BMO_slot_get(&dupe_op, "newout");

		if ((amd->flags & MOD_ARR_MERGEFINAL) && j == 0) {
			int first_geom_bytes = sizeof(BMVert *) * geom_slot->len;
				
			/* make a copy of the initial geometry ordering so the
			 * last duplicate can be merged into it */
			first_geom = MEM_mallocN(first_geom_bytes, "first_geom");
			memcpy(first_geom, geom_slot->data.buf, first_geom_bytes);
		}

		/* apply transformation matrix */
		BMO_ITER (v, &oiter, em->bm, &dupe_op, "newout", BM_VERT) {
			mul_m4_v3(offset, v->co);
		}

		if (amd->flags & MOD_ARR_MERGE) {
			/*calculate merge mapping*/
			if (j == 0) {
				indexMap = find_doubles_index_map(em->bm, &dupe_op,
				                                  amd, &indexLen);
			}

			#define _E(s, i) ((BMVert **)(s)->data.buf)[i]

			for (i = 0; i < indexLen; i++) {
				if (!indexMap[i]) continue;

				/* merge v (from 'newout') into v2 (from old 'geom') */
				v = _E(newout_slot, i - geom_slot->len);
				v2 = _E(geom_slot, indexMap[i] - 1);

				/* check in case the target vertex (v2) is already marked
				 * for merging */
				while ((v3 = BMO_slot_map_ptr_get(em->bm, &weld_op, "targetmap", v2))) {
					v2 = v3;
				}

				BMO_slot_map_ptr_insert(em->bm, &weld_op, "targetmap", v, v2);
			}

			#undef _E
		}

		/* already copied earlier, but after executation more slot
		 * memory may be allocated */
		if (j == 0)
			first_dupe_op = dupe_op;
		
		if (j >= 2)
			BMO_op_finish(em->bm, &old_dupe_op);
		old_dupe_op = dupe_op;
	}

	if ((amd->flags & MOD_ARR_MERGE) &&
	    (amd->flags & MOD_ARR_MERGEFINAL) &&
	    (count > 1))
	{
		/* Merge first and last copies. Note that we can't use the
		 * indexMap for this because (unless the array is forming a
		 * loop) the offset between first and last is different from
		 * dupe X to dupe X+1. */

		merge_first_last(em->bm, amd, &first_dupe_op, &dupe_op, &weld_op);
	}

	/* start capping */
	if (start_cap || end_cap) {
		BM_mesh_elem_hflag_enable_all(em->bm, BM_VERT, BM_ELEM_TAG, FALSE);

		if (start_cap) {
			float startoffset[4][4];
			invert_m4_m4(startoffset, offset);
			bm_merge_dm_transform(em->bm, start_cap, startoffset, amd,
			                      &first_dupe_op, "geom", &weld_op);
		}

		if (end_cap) {
			float endoffset[4][4];
			mult_m4_m4m4(endoffset, offset, final_offset);
			bm_merge_dm_transform(em->bm, end_cap, endoffset, amd,
			                      &dupe_op, count == 1 ? "geom" : "newout", &weld_op);
		}
	}
	/* done capping */

	/* free remaining dupe operators */
	BMO_op_finish(em->bm, &first_dupe_op);
	if (count > 2)
		BMO_op_finish(em->bm, &dupe_op);

	/* run merge operator */
	if (amd->flags & MOD_ARR_MERGE) {
		BMO_op_exec(em->bm, &weld_op);
		BMO_op_finish(em->bm, &weld_op);
	}

	/* Bump the stack level back down to match the adjustment up above */
	BMO_pop(em->bm);

	BLI_assert(em->looptris == NULL);
	result = CDDM_from_BMEditMesh(em, NULL, FALSE, FALSE);

	if ((amd->offset_type & MOD_ARR_OFF_OBJ) && (amd->offset_ob)) {
		/* Update normals in case offset object has rotation. */
		
		/* BMESH_TODO: check if normal recalc needed under any other
		 * conditions? */

		CDDM_calc_normals(result);
	}

	BMEdit_Free(em);
	MEM_freeN(em);
	if (indexMap)
		MEM_freeN(indexMap);
	if (first_geom)
		MEM_freeN(first_geom);

	return result;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *dm,
                                  int UNUSED(useRenderParams),
                                  int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	ArrayModifierData *amd = (ArrayModifierData *) md;

	result = arrayModifier_doArray(amd, md->scene, ob, dm, 0);

	//if (result != dm)
	//	CDDM_calc_normals_mapping(result);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *dm)
{
	return applyModifier(md, ob, dm, 0, 1);
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
	/* applyModifierEM */   applyModifierEM,
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
