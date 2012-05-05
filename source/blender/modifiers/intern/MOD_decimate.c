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


#ifdef WITH_MOD_DECIMATE
#include "LOD_decimation.h"
#endif

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;

	dmd->percent = 1.0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;
	DecimateModifierData *tdmd = (DecimateModifierData*) target;

	tdmd->percent = dmd->percent;
}

#ifdef WITH_MOD_DECIMATE
static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;
	DerivedMesh *dm = derivedData, *result = NULL;
	MVert *mvert;
	MFace *mface;
	LOD_Decimation_Info lod;
	int totvert, totface;
	int a, numTris;

	DM_ensure_tessface(dm); /* BMESH - UNTIL MODIFIER IS UPDATED FOR MPoly */

	mvert = dm->getVertArray(dm);
	mface = dm->getTessFaceArray(dm);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumTessFaces(dm);

	numTris = 0;
	for (a=0; a<totface; a++) {
		MFace *mf = &mface[a];
		numTris++;
		if (mf->v4) numTris++;
	}

	if (numTris<3) {
		modifier_setError(md, "%s", TIP_("Modifier requires more than 3 input faces (triangles)."));
		dm = CDDM_copy(dm);
		return dm;
	}

	lod.vertex_buffer= MEM_mallocN(3*sizeof(float)*totvert, "vertices");
	lod.vertex_normal_buffer= MEM_mallocN(3*sizeof(float)*totvert, "normals");
	lod.triangle_index_buffer= MEM_mallocN(3*sizeof(int)*numTris, "trias");
	lod.vertex_num= totvert;
	lod.face_num= numTris;

	for (a=0; a<totvert; a++) {
		MVert *mv = &mvert[a];
		float *vbCo = &lod.vertex_buffer[a*3];
		float *vbNo = &lod.vertex_normal_buffer[a*3];

		copy_v3_v3(vbCo, mv->co);
		normal_short_to_float_v3(vbNo, mv->no);
	}

	numTris = 0;
	for (a=0; a<totface; a++) {
		MFace *mf = &mface[a];
		int *tri = &lod.triangle_index_buffer[3*numTris++];
		tri[0]= mf->v1;
		tri[1]= mf->v2;
		tri[2]= mf->v3;

		if (mf->v4) {
			tri = &lod.triangle_index_buffer[3*numTris++];
			tri[0]= mf->v1;
			tri[1]= mf->v3;
			tri[2]= mf->v4;
		}
	}

	dmd->faceCount = 0;
	if (LOD_LoadMesh(&lod) ) {
		if ( LOD_PreprocessMesh(&lod) ) {
			/* we assume the decim_faces tells how much to reduce */

			while (lod.face_num > numTris*dmd->percent) {
				if ( LOD_CollapseEdge(&lod) == 0) break;
			}

			if (lod.vertex_num>2) {
				result = CDDM_new(lod.vertex_num, 0, lod.face_num, 0, 0);
				dmd->faceCount = lod.face_num;
			}
			else
				result = CDDM_new(lod.vertex_num, 0, 0, 0, 0);

			mvert = CDDM_get_verts(result);
			for (a=0; a<lod.vertex_num; a++) {
				MVert *mv = &mvert[a];
				float *vbCo = &lod.vertex_buffer[a*3];
				
				copy_v3_v3(mv->co, vbCo);
			}

			if (lod.vertex_num>2) {
				mface = CDDM_get_tessfaces(result);
				for (a = 0; a < lod.face_num; a++) {
					MFace *mf = &mface[a];
					int *tri = &lod.triangle_index_buffer[a*3];
					mf->v1 = tri[0];
					mf->v2 = tri[1];
					mf->v3 = tri[2];
					test_index_face(mf, NULL, 0, 3);
				}
			}

			CDDM_calc_edges_tessface(result);
		}
		else
			modifier_setError(md, "%s", TIP_("Out of memory."));

		LOD_FreeDecimationData(&lod);
	}
	else
		modifier_setError(md, "%s", TIP_("Non-manifold mesh as input."));

	MEM_freeN(lod.vertex_buffer);
	MEM_freeN(lod.vertex_normal_buffer);
	MEM_freeN(lod.triangle_index_buffer);

	if (result) {
		CDDM_tessfaces_to_faces(result); /*builds ngon faces from tess (mface) faces*/

		return result;
	}
	else {
		return dm;
	}
}
#else // WITH_MOD_DECIMATE
static DerivedMesh *applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	return derivedData;
}
#endif // WITH_MOD_DECIMATE

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
