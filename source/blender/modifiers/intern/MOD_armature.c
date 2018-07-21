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

/** \file blender/modifiers/intern/MOD_armature.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_editmesh.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"

#include "MOD_util.h"


static void initData(ModifierData *md)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	amd->deformflag = ARM_DEF_VGROUP;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
	const ArmatureModifierData *amd = (const ArmatureModifierData *) md;
#endif
	ArmatureModifierData *tamd = (ArmatureModifierData *) target;

	modifier_copyData_generic(md, target, flag);
	tamd->prevCos = NULL;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups */
	dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	return !amd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	walk(userData, ob, &amd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	ArmatureModifierData *amd = (ArmatureModifierData *)md;
	if (amd->object != NULL) {
		DEG_add_object_relation(ctx->node, amd->object, DEG_OB_COMP_EVAL_POSE, "Armature Modifier");
		DEG_add_object_relation(ctx->node, amd->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh,
        float (*vertexCos)[3],
        int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

	armature_deform_verts(amd->object, ctx->object, mesh, vertexCos, NULL,
	                      numVerts, amd->deformflag, (float(*)[3])amd->prevCos, amd->defgrp_name);

	/* free cache */
	if (amd->prevCos) {
		MEM_freeN(amd->prevCos);
		amd->prevCos = NULL;
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *em,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, em, mesh, NULL, false, false);

	MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

	armature_deform_verts(amd->object, ctx->object, mesh_src, vertexCos, NULL,
	                      numVerts, amd->deformflag, (float(*)[3])amd->prevCos, amd->defgrp_name);

	/* free cache */
	if (amd->prevCos) {
		MEM_freeN(amd->prevCos);
		amd->prevCos = NULL;
	}

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformMatricesEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *em,
        Mesh *mesh, float (*vertexCos)[3],
        float (*defMats)[3][3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, em, mesh, NULL, false, false);

	armature_deform_verts(amd->object, ctx->object, mesh_src, vertexCos, defMats, numVerts,
	                      amd->deformflag, NULL, amd->defgrp_name);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformMatrices(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        float (*vertexCos)[3], float (*defMats)[3][3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	armature_deform_verts(amd->object, ctx->object, mesh_src, vertexCos, defMats, numVerts,
	                      amd->deformflag, NULL, amd->defgrp_name);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

ModifierTypeInfo modifierType_Armature = {
	/* name */              "Armature",
	/* structName */        "ArmatureModifierData",
	/* structSize */        sizeof(ArmatureModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_AcceptsLattice |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    deformMatrices,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  deformMatricesEM,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
