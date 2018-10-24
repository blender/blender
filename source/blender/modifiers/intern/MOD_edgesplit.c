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

/** \file blender/modifiers/intern/MOD_edgesplit.c
 *  \ingroup modifiers
 *
 * EdgeSplit modifier
 *
 * Splits edges in the mesh according to sharpness flag
 * or edge angle (can be used to achieve autosmoothing)
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "MOD_modifiertypes.h"

static Mesh *doEdgeSplit(Mesh *mesh, EdgeSplitModifierData *emd, const ModifierEvalContext *ctx)
{
	Mesh *result;
	BMesh *bm;
	BMIter iter;
	BMEdge *e;
	float threshold = cosf(emd->split_angle + 0.000000175f);
	const bool calc_face_normals = (emd->flags & MOD_EDGESPLIT_FROMANGLE) != 0;

	bm = BKE_mesh_to_bmesh_ex(
	        mesh,
	        &(struct BMeshCreateParams){0},
	        &(struct BMeshFromMeshParams){
	            .calc_face_normal = calc_face_normals,
	            .add_key_index = false,
	            .use_shapekey = true,
	            .active_shapekey = ctx->object->shapenr,
	            .cd_mask_extra = CD_MASK_ORIGINDEX,
	        });

	if (emd->flags & MOD_EDGESPLIT_FROMANGLE) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			/* check for 1 edge having 2 face users */
			BMLoop *l1, *l2;
			if ((l1 = e->l) &&
			    (l2 = e->l->radial_next) != l1)
			{
				if (/* 3+ faces on this edge, always split */
				    UNLIKELY(l1 != l2->radial_next) ||
				    /* 2 face edge - check angle*/
				    (dot_v3v3(l1->f->no, l2->f->no) < threshold))
				{
					BM_elem_flag_enable(e, BM_ELEM_TAG);
				}
			}
		}
	}

	if (emd->flags & MOD_EDGESPLIT_FROMFLAG) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			/* check for 2 or more edge users */
			if ((e->l) &&
			    (e->l->next != e->l))
			{
				if (!BM_elem_flag_test(e, BM_ELEM_SMOOTH)) {
					BM_elem_flag_enable(e, BM_ELEM_TAG);
				}
			}
		}
	}

	BM_mesh_edgesplit(bm, false, true, false);

	/* BM_mesh_validate(bm); */ /* for troubleshooting */

	result = BKE_mesh_from_bmesh_for_eval_nomain(bm, 0);
	BM_mesh_free(bm);

	result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
	return result;
}

static void initData(ModifierData *md)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData *) md;

	/* default to 30-degree split angle, sharpness from both angle & flag */
	emd->split_angle = DEG2RADF(30.0f);
	emd->flags = MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG;
}

static Mesh *applyModifier(
        ModifierData *md,
        const ModifierEvalContext *ctx,
        Mesh *mesh)
{
	Mesh *result;
	EdgeSplitModifierData *emd = (EdgeSplitModifierData *) md;

	if (!(emd->flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG)))
		return mesh;

	result = doEdgeSplit(mesh, emd, ctx);

	return result;
}


ModifierTypeInfo modifierType_EdgeSplit = {
	/* name */              "EdgeSplit",
	/* structName */        "EdgeSplitModifierData",
	/* structSize */        sizeof(EdgeSplitModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          modifier_copyData_generic,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
