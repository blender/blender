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


#include "BKE_editmesh.h"
#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "DEG_depsgraph_query.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	psmd->psys = NULL;
	psmd->mesh_final = NULL;
	psmd->mesh_original = NULL;
	psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;
}
static void freeData(ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;

	if (psmd->mesh_final) {
		BKE_id_free(NULL, psmd->mesh_final);
		psmd->mesh_final = NULL;
		if (psmd->mesh_original) {
			BKE_id_free(NULL, psmd->mesh_original);
			psmd->mesh_original = NULL;
		}
	}
	psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;

	/* ED_object_modifier_remove may have freed this first before calling
	 * modifier_free (which calls this function) */
	if (psmd->psys)
		psmd->psys->flag |= PSYS_DELETE;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
	const ParticleSystemModifierData *psmd = (const ParticleSystemModifierData *) md;
#endif
	ParticleSystemModifierData *tpsmd = (ParticleSystemModifierData *) target;

	modifier_copyData_generic(md, target, flag);

	tpsmd->mesh_final = NULL;
	tpsmd->mesh_original = NULL;
	tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	return psys_emitter_customdata_mask(psmd->psys);
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh,
        float (*vertexCos)[3],
        int UNUSED(numVerts))
{
	Mesh *mesh_src = mesh;
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	ParticleSystem *psys = NULL;
	/* float cfra = BKE_scene_frame_get(md->scene); */  /* UNUSED */

	if (ctx->object->particlesystem.first)
		psys = psmd->psys;
	else
		return;

	if (!psys_check_enabled(ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0))
		return;

	if (mesh_src == NULL) {
		mesh_src = MOD_get_mesh_eval(ctx->object, NULL, NULL, vertexCos, false, true);
		if (mesh_src == NULL) {
			return;
		}
	}

	/* clear old dm */
	if (psmd->mesh_final) {
		BKE_id_free(NULL, psmd->mesh_final);
		psmd->mesh_final = NULL;
		if (psmd->mesh_original) {
			BKE_id_free(NULL, psmd->mesh_original);
			psmd->mesh_original = NULL;
		}
	}
	else if (psmd->flag & eParticleSystemFlag_file_loaded) {
		/* in file read mesh just wasn't saved in file so no need to reset everything */
		psmd->flag &= ~eParticleSystemFlag_file_loaded;
	}
	else {
		/* no dm before, so recalc particles fully */
		psys->recalc |= PSYS_RECALC_RESET;
	}

	/* make new mesh */
	BKE_id_copy_ex(NULL, &mesh_src->id, (ID **)&psmd->mesh_final,
	               LIB_ID_CREATE_NO_MAIN |
	               LIB_ID_CREATE_NO_USER_REFCOUNT |
	               LIB_ID_CREATE_NO_DEG_TAG |
	               LIB_ID_COPY_NO_PREVIEW,
	               false);
	BKE_mesh_apply_vert_coords(psmd->mesh_final, vertexCos);
	BKE_mesh_calc_normals(psmd->mesh_final);

	BKE_mesh_tessface_ensure(psmd->mesh_final);

	if (!psmd->mesh_final->runtime.deformed_only) {
		/* Get the original mesh from the object, this is what the particles
		 * are attached to so in case of non-deform modifiers we need to remap
		 * them to the final mesh (typically subdivision surfaces). */
		Mesh *mesh_original = NULL;

		if (ctx->object->type == OB_MESH) {
			BMEditMesh *edit_btmesh = BKE_editmesh_from_object(ctx->object);

			if (edit_btmesh) {
				/* In edit mode get directly from the edit mesh. */
				psmd->mesh_original = BKE_bmesh_to_mesh_nomain(edit_btmesh->bm, &(struct BMeshToMeshParams){0});
			}
			else {
				/* Otherwise get regular mesh. */
				mesh_original = ctx->object->data;
			}
		}
		else {
			mesh_original = mesh_src;
		}

		if (mesh_original) {
			/* Make a persistent copy of the mesh. We don't actually need
			 * all this data, just some topology for remapping. Could be
			 * optimized once. */
			BKE_id_copy_ex(NULL, &mesh_original->id, (ID **)&psmd->mesh_original,
			               LIB_ID_CREATE_NO_MAIN |
			               LIB_ID_CREATE_NO_USER_REFCOUNT |
			               LIB_ID_CREATE_NO_DEG_TAG |
			               LIB_ID_COPY_NO_PREVIEW,
			               false);
		}

		BKE_mesh_tessface_ensure(psmd->mesh_original);
	}

	if (mesh_src != psmd->mesh_final && mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}

	/* report change in mesh structure */
	if (psmd->mesh_final->totvert != psmd->totdmvert ||
	    psmd->mesh_final->totedge != psmd->totdmedge ||
	    psmd->mesh_final->totface != psmd->totdmface)
	{
		psys->recalc |= PSYS_RECALC_RESET;

		psmd->totdmvert = psmd->mesh_final->totvert;
		psmd->totdmedge = psmd->mesh_final->totedge;
		psmd->totdmface = psmd->mesh_final->totface;
	}

	if (!(ctx->object->transflag & OB_NO_PSYS_UPDATE)) {
		struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
		psmd->flag &= ~eParticleSystemFlag_psys_updated;
		particle_system_update(ctx->depsgraph, scene, ctx->object, psys, (ctx->flag & MOD_APPLY_RENDER) != 0);
		psmd->flag |= eParticleSystemFlag_psys_updated;
	}
}

/* disabled particles in editmode for now, until support for proper evaluated mesh
 * updates is coded */
#if 0
static void deformVertsEM(
        ModifierData *md, Object *ob, BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	const bool do_temp_mesh = (mesh == NULL);
	if (do_temp_mesh) {
		mesh = BKE_id_new_nomain(ID_ME, ((ID *)ob->data)->name);
		BM_mesh_bm_to_me(NULL, editData->bm, mesh, &((BMeshToMeshParams){0}));
	}

	deformVerts(md, ob, mesh, vertexCos, numVerts);

	if (derivedData) {
		BKE_id_free(NULL, mesh);
	}
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

	/* deformVerts_DM */    NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatrices_DM */ NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
