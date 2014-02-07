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
 
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"

#include "MOD_util.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"


static void initData(ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *) md;

	bmd->value = 0.1f;
	bmd->res = 1;
	bmd->flags = 0;
	bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
	bmd->lim_flags = 0;
	bmd->e_flags = 0;
	bmd->profile = 0.5f;
	bmd->bevel_angle = DEG2RADF(30.0f);
	bmd->defgrp_name[0] = '\0';
}

static void copyData(ModifierData *md, ModifierData *target)
{
	BevelModifierData *bmd = (BevelModifierData *) md;
	BevelModifierData *tbmd = (BevelModifierData *) target;

	tbmd->value = bmd->value;
	tbmd->res = bmd->res;
	tbmd->flags = bmd->flags;
	tbmd->val_flags = bmd->val_flags;
	tbmd->lim_flags = bmd->lim_flags;
	tbmd->e_flags = bmd->e_flags;
	tbmd->profile = bmd->profile;
	tbmd->bevel_angle = bmd->bevel_angle;
	BLI_strncpy(tbmd->defgrp_name, bmd->defgrp_name, sizeof(tbmd->defgrp_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (bmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

/*
 * This calls the new bevel code (added since 2.64)
 */
static DerivedMesh *applyModifier(ModifierData *md, struct Object *ob,
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *result;
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

	bm = DM_to_bmesh(dm, true);
	if ((bmd->lim_flags & MOD_BEVEL_VGROUP) && bmd->defgrp_name[0])
		modifier_get_vgroup(ob, dm, bmd->defgrp_name, &dvert, &vgroup);

	if (vertex_only) {
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (!BM_vert_is_manifold(v))
				continue;
			if (vgroup != -1) {
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
	              dvert, vgroup);

	result = CDDM_from_bmesh(bm, true);

	BLI_assert(bm->vtoolflagpool == NULL &&
	           bm->etoolflagpool == NULL &&
	           bm->ftoolflagpool == NULL);  /* make sure we never alloc'd these */
	BM_mesh_free(bm);

	result->dirty |= DM_DIRTY_NORMALS;

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
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
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
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
