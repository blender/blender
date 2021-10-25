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

/** \file blender/modifiers/intern/MOD_collision.c
 *  \ingroup modifiers
 */

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_collision.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md) 
{
	CollisionModifierData *collmd = (CollisionModifierData *) md;
	
	collmd->x = NULL;
	collmd->xnew = NULL;
	collmd->current_x = NULL;
	collmd->current_xnew = NULL;
	collmd->current_v = NULL;
	collmd->time_x = collmd->time_xnew = -1000;
	collmd->mvert_num = 0;
	collmd->tri_num = 0;
	collmd->is_static = false;
	collmd->bvhtree = NULL;
}

static void freeData(ModifierData *md)
{
	CollisionModifierData *collmd = (CollisionModifierData *) md;
	
	if (collmd) {
		if (collmd->bvhtree) {
			BLI_bvhtree_free(collmd->bvhtree);
			collmd->bvhtree = NULL;
		}

		MEM_SAFE_FREE(collmd->x);
		MEM_SAFE_FREE(collmd->xnew);
		MEM_SAFE_FREE(collmd->current_x);
		MEM_SAFE_FREE(collmd->current_xnew);
		MEM_SAFE_FREE(collmd->current_v);

		if (collmd->tri) {
			MEM_freeN((void *)collmd->tri);
			collmd->tri = NULL;
		}

		collmd->time_x = collmd->time_xnew = -1000;
		collmd->mvert_num = 0;
		collmd->tri_num = 0;
		collmd->is_static = false;
	}
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int UNUSED(numVerts),
                        ModifierApplyFlag UNUSED(flag))
{
	CollisionModifierData *collmd = (CollisionModifierData *) md;
	DerivedMesh *dm = NULL;
	MVert *tempVert = NULL;
	
	/* if possible use/create DerivedMesh */
	if (derivedData) dm = CDDM_copy(derivedData);
	else if (ob->type == OB_MESH) dm = CDDM_from_mesh(ob->data);
	
	if (!ob->pd) {
		printf("CollisionModifier deformVerts: Should not happen!\n");
		return;
	}
	
	if (dm) {
		float current_time = 0;
		unsigned int mvert_num = 0;

		CDDM_apply_vert_coords(dm, vertexCos);
		CDDM_calc_normals(dm);
		
		current_time = BKE_scene_frame_get(md->scene);
		
		if (G.debug_value > 0)
			printf("current_time %f, collmd->time_xnew %f\n", current_time, collmd->time_xnew);
		
		mvert_num = dm->getNumVerts(dm);
		
		if (current_time > collmd->time_xnew) {
			unsigned int i;

			/* check if mesh has changed */
			if (collmd->x && (mvert_num != collmd->mvert_num))
				freeData((ModifierData *)collmd);

			if (collmd->time_xnew == -1000) { /* first time */

				collmd->x = dm->dupVertArray(dm); /* frame start position */

				for (i = 0; i < mvert_num; i++) {
					/* we save global positions */
					mul_m4_v3(ob->obmat, collmd->x[i].co);
				}
				
				collmd->xnew = MEM_dupallocN(collmd->x); // frame end position
				collmd->current_x = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_xnew = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_v = MEM_dupallocN(collmd->x); // inter-frame

				collmd->mvert_num = mvert_num;

				collmd->tri_num = dm->getNumLoopTri(dm);
				{
					const MLoop *mloop = dm->getLoopArray(dm);
					const MLoopTri *looptri = dm->getLoopTriArray(dm);
					MVertTri *tri = MEM_malloc_arrayN(collmd->tri_num, sizeof(*tri), __func__);
					DM_verttri_from_looptri(tri, mloop, looptri, collmd->tri_num);
					collmd->tri = tri;
				}

				/* create bounding box hierarchy */
				collmd->bvhtree = bvhtree_build_from_mvert(
				        collmd->x,
				        collmd->tri, collmd->tri_num,
				        ob->pd->pdef_sboft);

				collmd->time_x = collmd->time_xnew = current_time;
				collmd->is_static = true;
			}
			else if (mvert_num == collmd->mvert_num) {
				/* put positions to old positions */
				tempVert = collmd->x;
				collmd->x = collmd->xnew;
				collmd->xnew = tempVert;
				collmd->time_x = collmd->time_xnew;

				memcpy(collmd->xnew, dm->getVertArray(dm), mvert_num * sizeof(MVert));

				bool is_static = true;

				for (i = 0; i < mvert_num; i++) {
					/* we save global positions */
					mul_m4_v3(ob->obmat, collmd->xnew[i].co);

					/* detect motion */
					is_static = is_static && equals_v3v3(collmd->x[i].co, collmd->xnew[i].co);
				}

				memcpy(collmd->current_xnew, collmd->x, mvert_num * sizeof(MVert));
				memcpy(collmd->current_x, collmd->x, mvert_num * sizeof(MVert));

				/* check if GUI setting has changed for bvh */
				if (collmd->bvhtree) {
					if (ob->pd->pdef_sboft != BLI_bvhtree_get_epsilon(collmd->bvhtree)) {
						BLI_bvhtree_free(collmd->bvhtree);
						collmd->bvhtree = bvhtree_build_from_mvert(
						        collmd->current_x,
						        collmd->tri, collmd->tri_num,
						        ob->pd->pdef_sboft);
					}
			
				}
				
				/* happens on file load (ONLY when i decomment changes in readfile.c) */
				if (!collmd->bvhtree) {
					collmd->bvhtree = bvhtree_build_from_mvert(
					        collmd->current_x,
					        collmd->tri, collmd->tri_num,
					        ob->pd->pdef_sboft);
				}
				else if (!collmd->is_static || !is_static) {
					/* recalc static bounding boxes */
					bvhtree_update_from_mvert(
					        collmd->bvhtree,
					        collmd->current_x, collmd->current_xnew,
					        collmd->tri, collmd->tri_num,
					        true);
				}

				collmd->is_static = is_static;
				collmd->time_xnew = current_time;
			}
			else if (mvert_num != collmd->mvert_num) {
				freeData((ModifierData *)collmd);
			}
			
		}
		else if (current_time < collmd->time_xnew) {
			freeData((ModifierData *)collmd);
		}
		else {
			if (mvert_num != collmd->mvert_num) {
				freeData((ModifierData *)collmd);
			}
		}
	}
	
	if (dm)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Collision = {
	/* name */              "Collision",
	/* structName */        "CollisionModifierData",
	/* structSize */        sizeof(CollisionModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_Single,

	/* copyData */          NULL,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
