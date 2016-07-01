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

/** \file blender/modifiers/intern/MOD_boolean.c
 *  \ingroup modifiers
 */

// #ifdef DEBUG_TIME
// #define USE_BMESH
#ifdef WITH_MOD_BOOLEAN
#  define USE_CARVE WITH_MOD_BOOLEAN
#endif

#include <stdio.h>

#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_boolean_util.h"
#include "MOD_util.h"


#ifdef USE_BMESH
#include "BLI_alloca.h"
#include "BLI_math_geom.h"
#include "BKE_material.h"
#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "tools/bmesh_intersect.h"
#endif

#ifdef DEBUG_TIME
#include "PIL_time.h"
#include "PIL_time_utildefines.h"
#endif

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	BooleanModifierData *bmd = (BooleanModifierData *) md;
	BooleanModifierData *tbmd = (BooleanModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	BooleanModifierData *bmd = (BooleanModifierData *) md;

	return !bmd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	BooleanModifierData *bmd = (BooleanModifierData *) md;

	walk(userData, ob, &bmd->object, IDWALK_NOP);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	BooleanModifierData *bmd = (BooleanModifierData *) md;

	if (bmd->object) {
		DagNode *curNode = dag_get_node(forest, bmd->object);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Boolean Modifier");
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	BooleanModifierData *bmd = (BooleanModifierData *)md;
	if (bmd->object != NULL) {
		DEG_add_object_relation(node, bmd->object, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
		DEG_add_object_relation(node, bmd->object, DEG_OB_COMP_GEOMETRY, "Boolean Modifier");
	}
	/* We need own transformation as well. */
	DEG_add_object_relation(node, ob, DEG_OB_COMP_TRANSFORM, "Boolean Modifier");
}

#if defined(USE_CARVE) || defined(USE_BMESH)

static DerivedMesh *get_quick_derivedMesh(
        Object *ob_self,  DerivedMesh *dm_self,
        Object *ob_other, DerivedMesh *dm_other,
        int operation)
{
	DerivedMesh *result = NULL;

	if (dm_self->getNumPolys(dm_self) == 0 || dm_other->getNumPolys(dm_other) == 0) {
		switch (operation) {
			case eBooleanModifierOp_Intersect:
				result = CDDM_new(0, 0, 0, 0, 0);
				break;

			case eBooleanModifierOp_Union:
				if (dm_self->getNumPolys(dm_self) != 0) {
					result = dm_self;
				}
				else {
					result = CDDM_copy(dm_other);

					float imat[4][4];
					float omat[4][4];

					invert_m4_m4(imat, ob_self->obmat);
					mul_m4_m4m4(omat, imat, ob_other->obmat);

					const int mverts_len = result->getNumVerts(result);
					MVert *mv = CDDM_get_verts(result);

					for (int i = 0; i < mverts_len; i++, mv++) {
						mul_m4_v3(omat, mv->co);
					}

					result->dirty |= DM_DIRTY_NORMALS;
				}

				break;

			case eBooleanModifierOp_Difference:
				result = dm_self;
				break;
		}
	}

	return result;
}
#endif  /* defined(USE_CARVE) || defined(USE_BMESH) */


/* -------------------------------------------------------------------- */
/* BMESH */

#ifdef USE_BMESH

/* has no meaning for faces, do this so we can tell which face is which */
#define BM_FACE_TAG BM_ELEM_DRAW

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
	return BM_elem_flag_test(f, BM_FACE_TAG) ? 1 : 0;
}

static DerivedMesh *applyModifier_bmesh(
        ModifierData *md, Object *ob,
        DerivedMesh *dm,
        ModifierApplyFlag flag)
{
	BooleanModifierData *bmd = (BooleanModifierData *) md;
	DerivedMesh *dm_other;

	if (!bmd->object)
		return dm;

	dm_other = get_dm_for_modifier(bmd->object, flag);

	if (dm_other) {
		DerivedMesh *result;

		/* when one of objects is empty (has got no faces) we could speed up
		 * calculation a bit returning one of objects' derived meshes (or empty one)
		 * Returning mesh is depended on modifiers operation (sergey) */
		result = get_quick_derivedMesh(ob, dm, bmd->object, dm_other, bmd->operation);

		if (result == NULL) {
			BMesh *bm;
			const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_DM(dm, dm_other);

#ifdef DEBUG_TIME
			TIMEIT_START(boolean_bmesh);
#endif
			bm = BM_mesh_create_ex(&allocsize, );

			DM_to_bmesh_ex(dm_other, bm, true);
			DM_to_bmesh_ex(dm, bm, true);

			/* main bmesh intersection setup */
			{
				/* create tessface & intersect */
				const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
				int tottri;
				BMLoop *(*looptris)[3];

				looptris = MEM_mallocN(sizeof(*looptris) * looptris_tot, __func__);

				BM_mesh_calc_tessellation(bm, looptris, &tottri);

				/* postpone this until after tessellating
				 * so we can use the original normals before the vertex are moved */
				{
					BMIter iter;
					int i;
					const int i_verts_end = dm_other->getNumVerts(dm_other);
					const int i_faces_end = dm_other->getNumPolys(dm_other);

					float imat[4][4];
					float omat[4][4];

					invert_m4_m4(imat, ob->obmat);
					mul_m4_m4m4(omat, imat, bmd->object->obmat);


					BMVert *eve;
					i = 0;
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						mul_m4_v3(omat, eve->co);
						if (++i == i_verts_end) {
							break;
						}
					}

					/* we need face normals because of 'BM_face_split_edgenet'
					 * we could calculate on the fly too (before calling split). */
					{
						float nmat[4][4];
						invert_m4_m4(nmat, omat);

						const short ob_src_totcol = bmd->object->totcol;
						short *material_remap = BLI_array_alloca(material_remap, ob_src_totcol ? ob_src_totcol : 1);

						BKE_material_remap_object_calc(ob, bmd->object, material_remap);

						BMFace *efa;
						i = 0;
						BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
							mul_transposed_mat3_m4_v3(nmat, efa->no);
							normalize_v3(efa->no);
							BM_elem_flag_enable(efa, BM_FACE_TAG);  /* temp tag to test which side split faces are from */

							/* remap material */
							if (LIKELY(efa->mat_nr < ob_src_totcol)) {
								efa->mat_nr = material_remap[efa->mat_nr];
							}

							if (++i == i_faces_end) {
								break;
							}
						}
					}
				}

				/* not needed, but normals for 'dm' will be invalid,
				 * currently this is ok for 'BM_mesh_intersect' */
				// BM_mesh_normals_update(bm);

				BM_mesh_intersect(
				        bm,
				        looptris, tottri,
				        bm_face_isect_pair, NULL,
				        false,
				        (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_Separate) != 0,
				        (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoDissolve) == 0,
				        (bmd->bm_flag & eBooleanModifierBMeshFlag_BMesh_NoConnectRegions) == 0,
				        bmd->operation,
				        bmd->threshold);

				MEM_freeN(looptris);
			}

			result = CDDM_from_bmesh(bm, true);

			BM_mesh_free(bm);

			result->dirty |= DM_DIRTY_NORMALS;

#ifdef DEBUG_TIME
			TIMEIT_END(boolean_bmesh);
#endif

			return result;
		}

		/* if new mesh returned, return it; otherwise there was
		 * an error, so delete the modifier object */
		if (result)
			return result;
		else
			modifier_setError(md, "Cannot execute boolean operation");
	}

	return dm;
}
#endif  /* USE_BMESH */


/* -------------------------------------------------------------------- */
/* CARVE */

#ifdef USE_CARVE
static DerivedMesh *applyModifier_carve(
        ModifierData *md, Object *ob,
        DerivedMesh *derivedData,
        ModifierApplyFlag flag)
{
	BooleanModifierData *bmd = (BooleanModifierData *) md;
	DerivedMesh *dm;

	if (!bmd->object)
		return derivedData;

	dm = get_dm_for_modifier(bmd->object, flag);

	if (dm) {
		DerivedMesh *result;

		/* when one of objects is empty (has got no faces) we could speed up
		 * calculation a bit returning one of objects' derived meshes (or empty one)
		 * Returning mesh is depended on modifiers operation (sergey) */
		result = get_quick_derivedMesh(ob, derivedData, bmd->object, dm, bmd->operation);

		if (result == NULL) {
#ifdef DEBUG_TIME
			TIMEIT_START(boolean_carve);
#endif

			result = NewBooleanDerivedMesh(dm, bmd->object, derivedData, ob,
			                               1 + bmd->operation);
#ifdef DEBUG_TIME
			TIMEIT_END(boolean_carve);
#endif
		}

		/* if new mesh returned, return it; otherwise there was
		 * an error, so delete the modifier object */
		if (result)
			return result;
		else
			modifier_setError(md, "Cannot execute boolean operation");
	}
	
	return derivedData;
}
#endif  /* USE_CARVE */


static DerivedMesh *applyModifier_nop(
        ModifierData *UNUSED(md), Object *UNUSED(ob),
        DerivedMesh *derivedData,
        ModifierApplyFlag UNUSED(flag))
{
	return derivedData;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = CD_MASK_MTFACE | CD_MASK_MEDGE;

	dataMask |= CD_MASK_MDEFORMVERT;
	
	return dataMask;
}

static DerivedMesh *applyModifier(
        ModifierData *md, Object *ob,
        DerivedMesh *derivedData,
        ModifierApplyFlag flag)
{
	BooleanModifierData *bmd = (BooleanModifierData *)md;
	const int method = (bmd->bm_flag & eBooleanModifierBMeshFlag_Enabled) ? 1 : 0;

	switch (method) {
#ifdef USE_CARVE
		case 0:
			return applyModifier_carve(md, ob, derivedData, flag);
#endif
#ifdef USE_BMESH
		case 1:
			return applyModifier_bmesh(md, ob, derivedData, flag);
#endif
		default:
			return applyModifier_nop(md, ob, derivedData, flag);
	}
}


ModifierTypeInfo modifierType_Boolean = {
	/* name */              "Boolean",
	/* structName */        "BooleanModifierData",
	/* structSize */        sizeof(BooleanModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          NULL,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
