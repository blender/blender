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

/** \file blender/modifiers/intern/MOD_multires.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_subsurf.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	MultiresModifierData *mmd = (MultiresModifierData *)md;

	mmd->lvl = 0;
	mmd->sculptlvl = 0;
	mmd->renderlvl = 0;
	mmd->totlvl = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MultiresModifierData *mmd = (MultiresModifierData *) md;
	MultiresModifierData *tmmd = (MultiresModifierData *) target;

	tmmd->lvl = mmd->lvl;
	tmmd->sculptlvl = mmd->sculptlvl;
	tmmd->renderlvl = mmd->renderlvl;
	tmmd->totlvl = mmd->totlvl;
	tmmd->simple = mmd->simple;
	tmmd->flags = mmd->flags;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	MultiresModifierData *mmd = (MultiresModifierData *)md;
	DerivedMesh *result;
	Mesh *me = (Mesh *)ob->data;
	const int useRenderParams = flag & MOD_APPLY_RENDER;
	MultiresFlags flags = 0;

	if (mmd->totlvl) {
		if (!CustomData_get_layer(&me->ldata, CD_MDISPS)) {
			/* multires always needs a displacement layer */
			CustomData_add_layer(&me->ldata, CD_MDISPS, CD_CALLOC, NULL, me->totloop);
		}
	}

	flags = MULTIRES_ALLOC_PAINT_MASK;
	if (useRenderParams)
		flags |= MULTIRES_USE_RENDER_PARAMS;

	result = multires_make_derived_from_derived(dm, mmd, ob, flags);

	if (result == dm)
		return dm;

	if (useRenderParams || !(flag & MOD_APPLY_USECACHE)) {
		DerivedMesh *cddm;
		
		cddm = CDDM_copy(result);

		/* copy hidden/masks to vertices */
		if (!useRenderParams) {
			struct MDisps *mdisps;
			struct GridPaintMask *grid_paint_mask;
			
			mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
			grid_paint_mask = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);
			
			if (mdisps) {
				subsurf_copy_grid_hidden(result, me->mpoly,
				                         cddm->getVertArray(cddm),
				                         mdisps);

				BKE_mesh_flush_hidden_from_verts(cddm->getVertArray(cddm),
				                                 cddm->getLoopArray(cddm),
				                                 cddm->getEdgeArray(cddm),
				                                 cddm->getNumEdges(cddm),
				                                 cddm->getPolyArray(cddm),
				                                 cddm->getNumPolys(cddm));
			}
			if (grid_paint_mask) {
				float *paint_mask = CustomData_add_layer(&cddm->vertData,
				                                         CD_PAINT_MASK,
				                                         CD_CALLOC, NULL,
				                                         cddm->getNumVerts(cddm));

				subsurf_copy_grid_paint_mask(result, me->mpoly,
				                             paint_mask, grid_paint_mask);
			}
		}

		result->release(result);
		result = cddm;
	}

	return result;
}


ModifierTypeInfo modifierType_Multires = {
	/* name */              "Multires",
	/* structName */        "MultiresModifierData",
	/* structSize */        sizeof(MultiresModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_RequiresOriginalData,

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
