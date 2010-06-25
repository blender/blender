/*
* $Id$
*
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
* Contributor(s): 
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "DNA_meshdata_types.h"
#include "BLI_math.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"


static void initData(ModifierData *md)
{
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;

	nmmd->cellsize = 0.3f;
	nmmd->cellheight = 0.2f;
	nmmd->agentmaxslope = 45.0f;
	nmmd->agentmaxclimb = 0.9f;
	nmmd->agentheight = 2.0f;
	nmmd->agentradius = 0.6f;
	nmmd->edgemaxlen = 12.0f;
	nmmd->edgemaxerror = 1.3f;
	nmmd->regionminsize = 50.f;
	nmmd->regionmergesize = 20.f;
	nmmd->vertsperpoly = 6;
	nmmd->detailsampledist = 6.0f;
	nmmd->detailsamplemaxerror = 1.0f;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;
	NavMeshModifierData *tnmmd = (NavMeshModifierData*) target;

}

static DerivedMesh *createNavMesh(NavMeshModifierData *mmd,DerivedMesh *dm)
{
	int i;
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);

	numVerts = numEdges = numFaces = 0;

	result = CDDM_from_template(dm, maxVerts * 2, maxEdges * 2, maxFaces * 2);

	for(i = 0; i < maxVerts; i++) {
		MVert inMV;
		MVert *mv = CDDM_get_vert(result, numVerts);
		float co[3];

		dm->getVert(dm, i, &inMV);

		copy_v3_v3(co, inMV.co);
		DM_copy_vert_data(dm, result, i, numVerts, 1);
		*mv = inMV;
		numVerts++;


		{
			MVert *mv2 = CDDM_get_vert(result, numVerts);
			DM_copy_vert_data(dm, result, i, numVerts, 1);
			*mv2 = *mv;
			co[2] +=.5f;
			copy_v3_v3(mv2->co, co);
			numVerts++;
		}

	}

	for(i = 0; i < maxEdges; i++) {
		MEdge inMED;
		MEdge *med = CDDM_get_edge(result, numEdges);

		dm->getEdge(dm, i, &inMED);

		DM_copy_edge_data(dm, result, i, numEdges, 1);
		*med = inMED;
		numEdges++;

		med->v1 = inMED.v1*2;
		med->v2 = inMED.v2*2;
		//med->flag |= ME_EDGEDRAW | ME_EDGERENDER;

		{
			MEdge *med2 = CDDM_get_edge(result, numEdges);

			DM_copy_edge_data(dm, result, i, numEdges, 1);
			*med2 = *med;
			numEdges++;

			med2->v1 += 1;
			med2->v2 += 1;
		}		
	}

	for(i = 0; i < maxFaces; i++) {
		MFace inMF;
		MFace *mf = CDDM_get_face(result, numFaces);

		dm->getFace(dm, i, &inMF);

		DM_copy_face_data(dm, result, i, numFaces, 1);
		*mf = inMF;
		numFaces++;

		mf->v1 = inMF.v1*2;
		mf->v2 = inMF.v2*2;
		mf->v3 = inMF.v3*2;
		mf->v4 = inMF.v4*2;

		{
			MFace *mf2 = CDDM_get_face(result, numFaces);
			DM_copy_face_data(dm, result, i, numFaces, 1);
			*mf2 = *mf;

			mf2->v1 += 1;
			mf2->v2 += 1;
			mf2->v3 += 1;
			if(inMF.v4) mf2->v4 += 1;

			//test_index_face(mf2, &result->faceData, numFaces, inMF.v4?4:3);
			numFaces++;
		}
	}

/*
	CDDM_lower_num_verts(result, numVerts);
	CDDM_lower_num_edges(result, numEdges);
	CDDM_lower_num_faces(result, numFaces);*/	

	return result;
}


static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
								  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;

	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;

	result = createNavMesh(nmmd, derivedData);

	return result;
}


ModifierTypeInfo modifierType_NavMesh = {
	/* name */              "NavMesh",
	/* structName */        "NavMeshModifierData",
	/* structSize */        sizeof(NavMeshModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,
	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
