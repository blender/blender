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
 * Contributor(s): Antony Riakiotakis
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_triangulate.c
 *  \ingroup modifiers
 */
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_array.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_tessmesh.h"

#include "MOD_util.h"

/* triangulation modifier, directly calls the bmesh operator */

static DerivedMesh *triangulate(DerivedMesh *dm, char use_beauty)
{
	DerivedMesh *result;
	BMesh *bm;

	bm = DM_to_bmesh(dm);

	BMO_push(bm, NULL);
	BMO_op_callf(bm, BMO_FLAG_DEFAULTS,
				  "triangulate faces=%af use_beauty=%b", use_beauty);

	BMO_pop(bm);

	result = CDDM_from_bmesh(bm, FALSE);
	BM_mesh_free(bm);

	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	return result;
}


static void initData(ModifierData *md)
{
	TriangulateModifierData *tmd = (TriangulateModifierData *)md;

	/* Enable in editmode by default */
	md->mode |= eModifierMode_Editmode;
	tmd->beauty = TRUE;
}


static void copyData(ModifierData *md, ModifierData *target)
{
	TriangulateModifierData *smd = (TriangulateModifierData *) md;
	TriangulateModifierData *tsmd = (TriangulateModifierData *) target;

	*tsmd = *smd;
}

static DerivedMesh *applyModifierEM(ModifierData *md,
                                    Object *UNUSED(ob),
                                    struct BMEditMesh *UNUSED(em),
                                    DerivedMesh *dm)
{
	TriangulateModifierData *tmd = (TriangulateModifierData *)md;
	DerivedMesh *result;
	if (!(result = triangulate(dm, tmd->beauty))) {
		return dm;
	}

	return result;
}

static DerivedMesh *applyModifier(ModifierData *md,
                                  Object *UNUSED(ob),
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	TriangulateModifierData *tmd = (TriangulateModifierData *)md;
	DerivedMesh *result;
	if(!(result = triangulate(dm, tmd->beauty))) {
		return dm;
	}

	return result;
}

ModifierTypeInfo modifierType_Triangulate = {
	/* name */              "Triangulate",
	/* structName */        "TriangulateModifierData",
	/* structSize */        sizeof(TriangulateModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
							eModifierTypeFlag_SupportsEditmode |
							eModifierTypeFlag_SupportsMapping |
							eModifierTypeFlag_EnableInEditmode |
							eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL, //requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
