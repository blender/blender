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

/** \file blender/modifiers/intern/MOD_cloth.c
 *  \ingroup modifiers
 */

#include <string.h>

#include "DNA_cloth_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	ClothModifierData *clmd = (ClothModifierData *) md;

	clmd->sim_parms = MEM_callocN(sizeof(ClothSimSettings), "cloth sim parms");
	clmd->coll_parms = MEM_callocN(sizeof(ClothCollSettings), "cloth coll parms");
	clmd->point_cache = BKE_ptcache_add(&clmd->ptcaches);

	/* check for alloc failing */
	if (!clmd->sim_parms || !clmd->coll_parms || !clmd->point_cache)
		return;

	cloth_init(clmd);
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh, float (*vertexCos)[3],
        int numVerts)
{
	Mesh *mesh_src;
	ClothModifierData *clmd = (ClothModifierData *) md;
	Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);

	/* check for alloc failing */
	if (!clmd->sim_parms || !clmd->coll_parms) {
		initData(md);

		if (!clmd->sim_parms || !clmd->coll_parms)
			return;
	}

	if (mesh == NULL) {
		mesh_src = MOD_get_mesh_eval(ctx->object, NULL, NULL, NULL, false, false);
	}
	else {
		/* Not possible to use get_mesh() in this case as we'll modify its vertices
		 * and get_mesh() would return 'mesh' directly. */
		BKE_id_copy_ex(
		        NULL, (ID *)mesh, (ID **)&mesh_src,
		        LIB_ID_CREATE_NO_MAIN |
		        LIB_ID_CREATE_NO_USER_REFCOUNT |
		        LIB_ID_CREATE_NO_DEG_TAG |
		        LIB_ID_COPY_NO_PREVIEW,
		        false);
	}

	/* TODO(sergey): For now it actually duplicates logic from DerivedMesh.c
	 * and needs some more generic solution. But starting experimenting with
	 * this so close to the release is not that nice..
	 *
	 * Also hopefully new cloth system will arrive soon..
	 */
	if (mesh == NULL && clmd->sim_parms->shapekey_rest) {
		KeyBlock *kb = BKE_keyblock_from_key(BKE_key_from_object(ctx->object),
		                                     clmd->sim_parms->shapekey_rest);
		if (kb && kb->data != NULL) {
			float (*layerorco)[3];
			if (!(layerorco = CustomData_get_layer(&mesh_src->vdata, CD_CLOTH_ORCO))) {
				layerorco = CustomData_add_layer(&mesh_src->vdata, CD_CLOTH_ORCO, CD_CALLOC, NULL, mesh_src->totvert);
			}

			memcpy(layerorco, kb->data, sizeof(float) * 3 * numVerts);
		}
	}

	BKE_mesh_apply_vert_coords(mesh_src, vertexCos);

	clothModifier_do(clmd, ctx->depsgraph, scene, ctx->object, mesh_src, vertexCos);

	BKE_id_free(NULL, mesh_src);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	ClothModifierData *clmd = (ClothModifierData *)md;
	if (clmd != NULL) {
		DEG_add_collision_relations(ctx->node, ctx->object, clmd->coll_parms->group, eModifierType_Collision, NULL, "Cloth Collision");
		DEG_add_forcefield_relations(ctx->node, ctx->object, clmd->sim_parms->effector_weights, true, 0, "Cloth Field");
	}
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	CustomDataMask dataMask = 0;
	ClothModifierData *clmd = (ClothModifierData *)md;

	if (cloth_uses_vgroup(clmd))
		dataMask |= CD_MASK_MDEFORMVERT;

	if (clmd->sim_parms->shapekey_rest != 0)
		dataMask |= CD_MASK_CLOTH_ORCO;

	return dataMask;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	const ClothModifierData *clmd = (const ClothModifierData *) md;
	ClothModifierData *tclmd = (ClothModifierData *) target;

	if (tclmd->sim_parms) {
		if (tclmd->sim_parms->effector_weights)
			MEM_freeN(tclmd->sim_parms->effector_weights);
		MEM_freeN(tclmd->sim_parms);
	}

	if (tclmd->coll_parms)
		MEM_freeN(tclmd->coll_parms);

	BKE_ptcache_free_list(&tclmd->ptcaches);
	if (flag & LIB_ID_CREATE_NO_MAIN) {
		/* Share the cache with the original object's modifier. */
		tclmd->modifier.flag |= eModifierFlag_SharedCaches;
		tclmd->ptcaches = clmd->ptcaches;
		tclmd->point_cache = clmd->point_cache;
	}
	else {
		tclmd->point_cache = BKE_ptcache_add(&tclmd->ptcaches);
		tclmd->point_cache->step = 1;
	}

	tclmd->sim_parms = MEM_dupallocN(clmd->sim_parms);
	if (clmd->sim_parms->effector_weights)
		tclmd->sim_parms->effector_weights = MEM_dupallocN(clmd->sim_parms->effector_weights);
	tclmd->coll_parms = MEM_dupallocN(clmd->coll_parms);
	tclmd->clothObject = NULL;
	tclmd->hairdata = NULL;
	tclmd->solver_result = NULL;
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void freeData(ModifierData *md)
{
	ClothModifierData *clmd = (ClothModifierData *) md;

	if (clmd) {
		if (G.debug_value > 0)
			printf("clothModifier_freeData\n");

		cloth_free_modifier_extern(clmd);

		if (clmd->sim_parms) {
			if (clmd->sim_parms->effector_weights)
				MEM_freeN(clmd->sim_parms->effector_weights);
			MEM_freeN(clmd->sim_parms);
		}
		if (clmd->coll_parms)
			MEM_freeN(clmd->coll_parms);

		if (md->flag & eModifierFlag_SharedCaches) {
			BLI_listbase_clear(&clmd->ptcaches);
		}
		else {
			BKE_ptcache_free_list(&clmd->ptcaches);
		}
		clmd->point_cache = NULL;

		if (clmd->hairdata)
			MEM_freeN(clmd->hairdata);

		if (clmd->solver_result)
			MEM_freeN(clmd->solver_result);
	}
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	ClothModifierData *clmd = (ClothModifierData *) md;

	if (clmd->coll_parms) {
		walk(userData, ob, (ID **)&clmd->coll_parms->group, IDWALK_CB_NOP);
	}

	if (clmd->sim_parms && clmd->sim_parms->effector_weights) {
		walk(userData, ob, (ID **)&clmd->sim_parms->effector_weights->group, IDWALK_CB_NOP);
	}
}

ModifierTypeInfo modifierType_Cloth = {
	/* name */              "Cloth",
	/* structName */        "ClothModifierData",
	/* structSize */        sizeof(ClothModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
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
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};
