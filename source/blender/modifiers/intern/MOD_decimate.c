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

/** \file blender/modifiers/intern/MOD_decimate.c
 *  \ingroup modifiers
 */

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "MEM_guardedalloc.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_particle.h"
#include "BKE_cdderivedmesh.h"

#include "bmesh.h"
#include "bmesh_tools.h"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;

	dmd->percent = 1.0;
	dmd->angle   = DEG2RADF(5.0f);
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	DecimateModifierData *dmd = (DecimateModifierData *) md;
	DecimateModifierData *tdmd = (DecimateModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (dmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;
	DerivedMesh *dm = derivedData, *result = NULL;
	BMesh *bm;
	bool calc_face_normal;

	float *vweights = NULL;

#ifdef USE_TIMEIT
	TIMEIT_START(decim);
#endif

	/* set up front so we dont show invalid info in the UI */
	dmd->face_count = dm->getNumPolys(dm);

	switch (dmd->mode) {
		case MOD_DECIM_MODE_COLLAPSE:
			if (dmd->percent == 1.0f) {
				return dm;
			}
			calc_face_normal = true;
			break;
		case MOD_DECIM_MODE_UNSUBDIV:
			if (dmd->iter == 0) {
				return dm;
			}
			calc_face_normal = false;
			break;
		case MOD_DECIM_MODE_DISSOLVE:
			if (dmd->angle == 0.0f) {
				return dm;
			}
			calc_face_normal = true;
			break;
		default:
			return dm;
	}

	if (dmd->face_count <= 3) {
		modifier_setError(md, "Modifier requires more than 3 input faces");
		return dm;
	}

	if (dmd->mode == MOD_DECIM_MODE_COLLAPSE) {
		if (dmd->defgrp_name[0]) {
			MDeformVert *dvert;
			int defgrp_index;

			modifier_get_vgroup(ob, dm, dmd->defgrp_name, &dvert, &defgrp_index);

			if (dvert) {
				const unsigned int vert_tot = dm->getNumVerts(dm);
				unsigned int i;

				vweights = MEM_mallocN(vert_tot * sizeof(float), __func__);

				if (dmd->flag & MOD_DECIM_FLAG_INVERT_VGROUP) {
					for (i = 0; i < vert_tot; i++) {
						const float f = 1.0f - defvert_find_weight(&dvert[i], defgrp_index);
						vweights[i] = f > BM_MESH_DECIM_WEIGHT_EPS ? (1.0f / f) : BM_MESH_DECIM_WEIGHT_MAX;
					}
				}
				else {
					for (i = 0; i < vert_tot; i++) {
						const float f = defvert_find_weight(&dvert[i], defgrp_index);
						vweights[i] = f > BM_MESH_DECIM_WEIGHT_EPS ? (1.0f / f) : BM_MESH_DECIM_WEIGHT_MAX;
					}
				}
			}
		}
	}

	bm = DM_to_bmesh(dm, calc_face_normal);

	switch (dmd->mode) {
		case MOD_DECIM_MODE_COLLAPSE:
		{
			const int do_triangulate = (dmd->flag & MOD_DECIM_FLAG_TRIANGULATE) != 0;
			BM_mesh_decimate_collapse(bm, dmd->percent, vweights, do_triangulate);
			break;
		}
		case MOD_DECIM_MODE_UNSUBDIV:
		{
			BM_mesh_decimate_unsubdivide(bm, dmd->iter);
			break;
		}
		case MOD_DECIM_MODE_DISSOLVE:
		{
			const int do_dissolve_boundaries = (dmd->flag & MOD_DECIM_FLAG_ALL_BOUNDARY_VERTS) != 0;
			BM_mesh_decimate_dissolve(bm, dmd->angle, do_dissolve_boundaries, (BMO_Delimit)dmd->delimit);
			break;
		}
	}

	if (vweights) {
		MEM_freeN(vweights);
	}

	/* update for display only */
	dmd->face_count = bm->totface;
	result = CDDM_from_bmesh(bm, false);
	BLI_assert(bm->vtoolflagpool == NULL &&
	           bm->etoolflagpool == NULL &&
	           bm->ftoolflagpool == NULL);  /* make sure we never alloc'd these */
	BLI_assert(bm->vtable == NULL &&
	           bm->etable == NULL &&
	           bm->ftable == NULL);

	BM_mesh_free(bm);

#ifdef USE_TIMEIT
	TIMEIT_END(decim);
#endif

	result->dirty = DM_DIRTY_NORMALS;

	return result;
}

ModifierTypeInfo modifierType_Decimate = {
	/* name */              "Decimate",
	/* structName */        "DecimateModifierData",
	/* structSize */        sizeof(DecimateModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs,
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
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
