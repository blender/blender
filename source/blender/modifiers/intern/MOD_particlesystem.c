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

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "MOD_util.h"


static void initData(ModifierData *md) 
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	psmd->psys= NULL;
	psmd->dm= NULL;
	psmd->totdmvert= psmd->totdmedge= psmd->totdmface= 0;
}
static void freeData(ModifierData *md)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;

	if(psmd->dm){
		psmd->dm->needsFree = 1;
		psmd->dm->release(psmd->dm);
		psmd->dm = NULL;
	}

	/* ED_object_modifier_remove may have freed this first before calling
	 * modifier_free (which calls this function) */
	if(psmd->psys)
		psmd->psys->flag |= PSYS_DELETE;
}
static void copyData(ModifierData *md, ModifierData *target)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	ParticleSystemModifierData *tpsmd= (ParticleSystemModifierData*) target;

	tpsmd->dm = NULL;
	tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
	//tpsmd->facepa = 0;
	tpsmd->flag = psmd->flag;
	/* need to keep this to recognise a bit later in copy_object */
	tpsmd->psys = psmd->psys;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	CustomDataMask dataMask = 0;
	MTex *mtex;
	int i;

	if(!psmd->psys->part)
		return 0;

	for(i=0; i<MAX_MTEX; i++) {
		mtex = psmd->psys->part->mtex[i];
		if(mtex && mtex->mapto && (mtex->texco & TEXCO_UV))
			dataMask |= CD_MASK_MTFACE;
	}

	if(psmd->psys->part->tanfac != 0.0f)
		dataMask |= CD_MASK_MTFACE;

	/* ask for vertexgroups if we need them */
	for(i=0; i<PSYS_TOT_VG; i++){
		if(psmd->psys->vgroup[i]){
			dataMask |= CD_MASK_MDEFORMVERT;
			break;
		}
	}
	
	/* particles only need this if they are after a non deform modifier, and
	 * the modifier stack will only create them in that case. */
	dataMask |= CD_MASK_ORIGSPACE_MLOOP|CD_MASK_ORIGINDEX;

	dataMask |= CD_MASK_ORCO;
	
	return dataMask;
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						float (*vertexCos)[3],
						int UNUSED(numVerts),
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *dm = derivedData;
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	ParticleSystem * psys= NULL;
	int needsFree=0;

	if(ob->particlesystem.first)
		psys=psmd->psys;
	else
		return;
	
	if(!psys_check_enabled(ob, psys))
		return;

	if(dm==NULL) {
		dm= get_dm(ob, NULL, NULL, vertexCos, 1);

		if(!dm)
			return;

		needsFree= 1;
	}

	/* clear old dm */
	if(psmd->dm){
		psmd->dm->needsFree = 1;
		psmd->dm->release(psmd->dm);
	}
	else if(psmd->flag & eParticleSystemFlag_file_loaded) {
		/* in file read dm just wasn't saved in file so no need to reset everything */
		psmd->flag &= ~eParticleSystemFlag_file_loaded;
	}
	else {
		/* no dm before, so recalc particles fully */
		psys->recalc |= PSYS_RECALC_RESET;
	}

	/* make new dm */
	psmd->dm=CDDM_copy(dm);
	CDDM_apply_vert_coords(psmd->dm, vertexCos);
	CDDM_calc_normals(psmd->dm);

	if(needsFree){
		dm->needsFree = 1;
		dm->release(dm);
	}

	/* protect dm */
	psmd->dm->needsFree = 0;

	/* report change in mesh structure */
	if(psmd->dm->getNumVerts(psmd->dm)!=psmd->totdmvert ||
		  psmd->dm->getNumEdges(psmd->dm)!=psmd->totdmedge ||
		  psmd->dm->getNumTessFaces(psmd->dm)!=psmd->totdmface){

		psys->recalc |= PSYS_RECALC_RESET;

		psmd->totdmvert= psmd->dm->getNumVerts(psmd->dm);
		psmd->totdmedge= psmd->dm->getNumEdges(psmd->dm);
		psmd->totdmface= psmd->dm->getNumTessFaces(psmd->dm);
	}

	if(psys) {
		psmd->flag &= ~eParticleSystemFlag_psys_updated;
		particle_system_update(md->scene, ob, psys);
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

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	deformVerts(md, ob, dm, vertexCos, numVerts);

	if(!derivedData) dm->release(dm);
}
#endif


ModifierTypeInfo modifierType_ParticleSystem = {
	/* name */              "ParticleSystem",
	/* structName */        "ParticleSystemModifierData",
	/* structSize */        sizeof(ParticleSystemModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_UsesPointCache /*
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode */,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     NULL /* deformVertsEM */ ,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
