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

/** \file blender/modifiers/intern/MOD_particlesystem.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "MOD_util.h"


static void initData(ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	psmd->psys = NULL;
	psmd->dm_final = NULL;
	psmd->dm_deformed = NULL;
	psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;
}
static void freeData(ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;

	if (psmd->dm_final) {
		psmd->dm_final->needsFree = true;
		psmd->dm_final->release(psmd->dm_final);
		psmd->dm_final = NULL;
		if (psmd->dm_deformed) {
			psmd->dm_deformed->needsFree = true;
			psmd->dm_deformed->release(psmd->dm_deformed);
			psmd->dm_deformed = NULL;
		}
	}
	psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;

	/* ED_object_modifier_remove may have freed this first before calling
	 * modifier_free (which calls this function) */
	if (psmd->psys)
		psmd->psys->flag |= PSYS_DELETE;
}

static void copyData(const ModifierData *md, ModifierData *target)
{
#if 0
	const ParticleSystemModifierData *psmd = (const ParticleSystemModifierData *) md;
#endif
	ParticleSystemModifierData *tpsmd = (ParticleSystemModifierData *) target;

	modifier_copyData_generic(md, target);

	tpsmd->dm_final = NULL;
	tpsmd->dm_deformed = NULL;
	tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	return psys_emitter_customdata_mask(psmd->psys);
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(
        ModifierData *md, Object *ob,
        DerivedMesh *derivedData,
        float (*vertexCos)[3],
        int UNUSED(numVerts),
        ModifierApplyFlag flag)
{
	DerivedMesh *dm = derivedData;
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	ParticleSystem *psys = NULL;
	bool needsFree = false;
	/* float cfra = BKE_scene_frame_get(md->scene); */  /* UNUSED */

	if (ob->particlesystem.first)
		psys = psmd->psys;
	else
		return;

	if (!psys_check_enabled(ob, psys, (flag & MOD_APPLY_RENDER) != 0))
		return;

	if (dm == NULL) {
		dm = get_dm(ob, NULL, NULL, vertexCos, false, true);

		if (!dm)
			return;

		needsFree = true;
	}

	/* clear old dm */
	if (psmd->dm_final) {
		psmd->dm_final->needsFree = true;
		psmd->dm_final->release(psmd->dm_final);
		if (psmd->dm_deformed) {
			psmd->dm_deformed->needsFree = 1;
			psmd->dm_deformed->release(psmd->dm_deformed);
			psmd->dm_deformed = NULL;
		}
	}
	else if (psmd->flag & eParticleSystemFlag_file_loaded) {
		/* in file read dm just wasn't saved in file so no need to reset everything */
		psmd->flag &= ~eParticleSystemFlag_file_loaded;
	}
	else {
		/* no dm before, so recalc particles fully */
		psys->recalc |= PSYS_RECALC_RESET;
	}

	/* make new dm */
	psmd->dm_final = CDDM_copy(dm);
	CDDM_apply_vert_coords(psmd->dm_final, vertexCos);
	CDDM_calc_normals(psmd->dm_final);

	if (needsFree) {
		dm->needsFree = true;
		dm->release(dm);
	}

	/* protect dm */
	psmd->dm_final->needsFree = false;

	DM_ensure_tessface(psmd->dm_final);

	if (!psmd->dm_final->deformedOnly) {
		/* XXX Think we can assume here that if current DM is not only-deformed, ob->deformedOnly has been set.
		 *     This is awfully weak though. :| */
		if (ob->derivedDeform) {
			psmd->dm_deformed = CDDM_copy(ob->derivedDeform);
		}
		else {  /* Can happen in some cases, e.g. when rendering from Edit mode... */
			psmd->dm_deformed = CDDM_from_mesh((Mesh *)ob->data);
		}
		DM_ensure_tessface(psmd->dm_deformed);
	}

	/* report change in mesh structure */
	if (psmd->dm_final->getNumVerts(psmd->dm_final) != psmd->totdmvert ||
	    psmd->dm_final->getNumEdges(psmd->dm_final) != psmd->totdmedge ||
	    psmd->dm_final->getNumTessFaces(psmd->dm_final) != psmd->totdmface)
	{
		psys->recalc |= PSYS_RECALC_RESET;

		psmd->totdmvert = psmd->dm_final->getNumVerts(psmd->dm_final);
		psmd->totdmedge = psmd->dm_final->getNumEdges(psmd->dm_final);
		psmd->totdmface = psmd->dm_final->getNumTessFaces(psmd->dm_final);
	}

	if (!(ob->transflag & OB_NO_PSYS_UPDATE)) {
		psmd->flag &= ~eParticleSystemFlag_psys_updated;
		particle_system_update(G.main, md->scene, ob, psys, (flag & MOD_APPLY_RENDER) != 0);
		psmd->flag |= eParticleSystemFlag_psys_updated;
	}
}

/* disabled particles in editmode for now, until support for proper derivedmesh
 * updates is coded */
#if 0
static void deformVertsEM(
        ModifierData *md, Object *ob, EditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	deformVerts(md, ob, dm, vertexCos, numVerts);

	if (!derivedData) dm->release(dm);
}
#endif


ModifierTypeInfo modifierType_ParticleSystem = {
	/* name */              "ParticleSystem",
	/* structName */        "ParticleSystemModifierData",
	/* structSize */        sizeof(ParticleSystemModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_UsesPointCache /* |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode */,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     NULL,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
