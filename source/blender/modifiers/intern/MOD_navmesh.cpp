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
#include <math.h>
#include "Recast.h"

extern "C"{

#include "DNA_meshdata_types.h"
#include "BLI_math.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "MEM_guardedalloc.h"
#include "BIF_gl.h"
#include "gpu_buffers.h"
#include "GPU_draw.h"
#include "UI_resources.h"


static void initData(ModifierData *md)
{
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;
	NavMeshModifierData *tnmmd = (NavMeshModifierData*) target;

	//.todo - deep copy
}

inline int bit(int a, int b)
{
	return (a & (1 << b)) >> b;
}

inline void intToCol(int i, float* col)
{
	int	r = bit(i, 0) + bit(i, 3) * 2 + 1;
	int	g = bit(i, 1) + bit(i, 4) * 2 + 1;
	int	b = bit(i, 2) + bit(i, 5) * 2 + 1;
	col[0] = 1 - r*63.0f/255.0f;
	col[1] = 1 - g*63.0f/255.0f;
	col[2] = 1 - b*63.0f/255.0f;
}
/*
static void (*drawFacesSolid_original)(DerivedMesh *dm, float (*partial_redraw_planes)[4],
					   int fast, int (*setMaterial)(int, void *attribs)) = NULL;*/

static void drawNavMeshColored(DerivedMesh *dm)
{
	int a, glmode;
	MVert *mvert = (MVert *)CustomData_get_layer(&dm->vertData, CD_MVERT);
	MFace *mface = (MFace *)CustomData_get_layer(&dm->faceData, CD_MFACE);
	int* polygonIdx = (int*)CustomData_get_layer(&dm->faceData, CD_PROP_INT);
	if (!polygonIdx)
		return;
	float col[3];
	/*
	//UI_ThemeColor(TH_WIRE);
	glDisable(GL_LIGHTING);
	glLineWidth(2.0);
	dm->drawEdges(dm, 0, 1);
	glLineWidth(1.0);
	glEnable(GL_LIGHTING);*/

	glDisable(GL_LIGHTING);
	if(GPU_buffer_legacy(dm) ) {
		DEBUG_VBO( "Using legacy code. drawNavMeshColored\n" );
		//glShadeModel(GL_SMOOTH);
		glBegin(glmode = GL_QUADS);
		for(a = 0; a < dm->numFaceData; a++, mface++) {
			int new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;
			int* polygonIdx = (int*)CustomData_get(&dm->faceData, a, CD_PROP_INT);
			intToCol(*polygonIdx, col);

			if(new_glmode != glmode) {
				glEnd();
				glBegin(glmode = new_glmode);
			}
			glColor3fv(col);
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);
			glVertex3fv(mvert[mface->v3].co);
			if(mface->v4) {
				glVertex3fv(mvert[mface->v4].co);
			}
		}
		glEnd();
	}
	glEnable(GL_LIGHTING);
}

static void navDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, MCol *mcol, int matnr))
{
	drawNavMeshColored(dm);
}

static void navDM_drawFacesSolid(DerivedMesh *dm,
								float (*partial_redraw_planes)[4],
								int fast, int (*setMaterial)(int, void *attribs))
{
	//drawFacesSolid_original(dm, partial_redraw_planes, fast, setMaterial);
	drawNavMeshColored(dm);
}

static DerivedMesh *createNavMeshForVisualization(NavMeshModifierData *mmd,DerivedMesh *dm)
{
	int i;
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);

	result = CDDM_copy(dm);
	CustomData_add_layer_named(&result->faceData, CD_PROP_INT, CD_DUPLICATE, 
			CustomData_get_layer(&dm->faceData, CD_PROP_INT), maxFaces, "recastData");
	
	/*result = CDDM_new(maxVerts, maxEdges, maxFaces);
	DM_copy_vert_data(dm, result, 0, 0, maxVerts);
	DM_copy_edge_data(dm, result, 0, 0, maxEdges);
	DM_copy_face_data(dm, result, 0, 0, maxFaces);*/

	
	/*
	if (!drawFacesSolid_original)
		drawFacesSolid_original= result->drawFacesSolid;*/
	result->drawFacesTex =  navDM_drawFacesTex;
	result->drawFacesSolid = navDM_drawFacesSolid;
	
/*
	numVerts = numEdges = numFaces = 0;
	for(i = 0; i < maxVerts; i++) {
		MVert inMV;
		MVert *mv = CDDM_get_vert(result, numVerts);
		float co[3];
		dm->getVert(dm, i, &inMV);
		copy_v3_v3(co, inMV.co);
		*mv = inMV;
		//mv->co[2] +=.5f;
		numVerts++;
	}
	for(i = 0; i < maxEdges; i++) {
		MEdge inMED;
		MEdge *med = CDDM_get_edge(result, numEdges);
		dm->getEdge(dm, i, &inMED);
		*med = inMED;
		numEdges++;
	}
	for(i = 0; i < maxFaces; i++) {
		MFace inMF;
		MFace *mf = CDDM_get_face(result, numFaces);
		dm->getFace(dm, i, &inMF);
		*mf = inMF;
		numFaces++;
	}*/

	return result;
}

/*
static int isDisabled(ModifierData *md, int useRenderParams)
{
	NavMeshModifierData *amd = (NavMeshModifierData*) md;
	return false; 
}*/



static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
								  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result = NULL;
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;

	if (ob->body_type==OB_BODY_TYPE_NAVMESH)
		result = createNavMeshForVisualization(nmmd, derivedData);
	
	return result;
}


ModifierTypeInfo modifierType_NavMesh = {
	/* name */              "NavMesh",
	/* structName */        "NavMeshModifierData",
	/* structSize */        sizeof(NavMeshModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             (ModifierTypeFlag) (eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_NoUserAdd),
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

};