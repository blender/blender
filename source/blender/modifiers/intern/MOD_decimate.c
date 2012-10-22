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

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "MEM_guardedalloc.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_cdderivedmesh.h"

#include "BKE_tessmesh.h"
#include "bmesh.h"

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
}

static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DecimateModifierData *dmd = (DecimateModifierData *) md;
	DerivedMesh *dm = derivedData, *result = NULL;
	BMEditMesh *em;
	BMesh *bm;

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

	em = DM_to_editbmesh(dm, NULL, FALSE);
	bm = em->bm;

	BM_mesh_decimate(bm, dmd->percent);

	dmd->faceCount = bm->totface;

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
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
