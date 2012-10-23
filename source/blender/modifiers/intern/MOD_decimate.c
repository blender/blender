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

#include "BKE_tessmesh.h"
#include "bmesh.h"

// #define USE_TIMEIT

#ifdef USE_TIMEIT
#  include "PIL_time.h"
#endif

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;

	dmd->percent = 1.0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;
	DecimateModifierData *tdmd = (DecimateModifierData *) target;

	tdmd->percent = dmd->percent;
	tdmd->iter = dmd->iter;
	BLI_strncpy(tdmd->defgrp_name, dmd->defgrp_name, sizeof(tdmd->defgrp_name));
	tdmd->flag = dmd->flag;
	tdmd->mode = dmd->mode;
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
	BMEditMesh *em;
	BMesh *bm;

	float *vweights = NULL;

#ifdef USE_TIMEIT
	 TIMEIT_START(decim);
#endif

	if (dmd->percent == 1.0f) {
		return dm;
	}
	else if (dm->getNumPolys(dm) <= 3) {
		modifier_setError(md, "%s", TIP_("Modifier requires more than 3 input faces"));
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
						vweights[i] = 1.0f - defvert_find_weight(&dvert[i], defgrp_index);
					}
				}
				else {
					for (i = 0; i < vert_tot; i++) {
						vweights[i] = defvert_find_weight(&dvert[i], defgrp_index);
					}
				}
			}
		}
	}

	em = DM_to_editbmesh(dm, NULL, FALSE);
	bm = em->bm;

	switch (dmd->mode) {
		case MOD_DECIM_MODE_COLLAPSE:
			BM_mesh_decimate_collapse(bm, dmd->percent, vweights);
			break;
		case MOD_DECIM_MODE_UNSUBDIV:
			BM_mesh_decimate_unsubdivide(bm, dmd->iter);
			break;
	}


	if (vweights) {
		MEM_freeN(vweights);
	}

	/* update for display only */
	dmd->face_count = bm->totface;

	BLI_assert(em->looptris == NULL);
	result = CDDM_from_BMEditMesh(em, NULL, TRUE, FALSE);
	BMEdit_Free(em);
	MEM_freeN(em);

#ifdef USE_TIMEIT
	 TIMEIT_END(decim);
#endif

	return result;
}

ModifierTypeInfo modifierType_Decimate = {
	/* name */              "Decimate",
	/* structName */        "DecimateModifierData",
	/* structSize */        sizeof(DecimateModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,
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
