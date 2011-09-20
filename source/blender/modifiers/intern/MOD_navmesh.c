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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#ifdef WITH_GAMEENGINE
#  include "recast-capi.h"
#  include "BKE_navmesh_conversion.h"
#  include "GL/glew.h"
#  include "GPU_buffers.h"
#  include "GPU_draw.h"
#endif

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_customdata.h"
#include "MEM_guardedalloc.h"

static inline int bit(int a, int b)
{
	return (a & (1 << b)) >> b;
}

static inline void intToCol(int i, float* col)
{
	int	r = bit(i, 0) + bit(i, 3) * 2 + 1;
	int	g = bit(i, 1) + bit(i, 4) * 2 + 1;
	int	b = bit(i, 2) + bit(i, 5) * 2 + 1;
	col[0] = 1 - r*63.0f/255.0f;
	col[1] = 1 - g*63.0f/255.0f;
	col[2] = 1 - b*63.0f/255.0f;
}


static void initData(ModifierData *UNUSED(md))
{
	/* NavMeshModifierData *nmmd = (NavMeshModifierData*) md; */ /* UNUSED */
}

static void copyData(ModifierData *UNUSED(md), ModifierData *UNUSED(target))
{
	/* NavMeshModifierData *nmmd = (NavMeshModifierData*) md; */
	/* NavMeshModifierData *tnmmd = (NavMeshModifierData*) target; */

	//.todo - deep copy
}

/*
static void (*drawFacesSolid_original)(DerivedMesh *dm, float (*partial_redraw_planes)[4],
					   int fast, int (*setMaterial)(int, void *attribs)) = NULL;*/

#ifdef WITH_GAMEENGINE

static void drawNavMeshColored(DerivedMesh *dm)
{
	int a, glmode;
	MVert *mvert = (MVert *)CustomData_get_layer(&dm->vertData, CD_MVERT);
	MFace *mface = (MFace *)CustomData_get_layer(&dm->faceData, CD_MFACE);
	int* polygonIdx = (int*)CustomData_get_layer(&dm->faceData, CD_RECAST);
	const float BLACK_COLOR[3] = {0.f, 0.f, 0.f};
	float col[3];

	if (!polygonIdx)
		return;

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
			int polygonIdx = *(int*)CustomData_get(&dm->faceData, a, CD_RECAST);
			if (polygonIdx<=0)
				memcpy(col, BLACK_COLOR, 3*sizeof(float));
			else
				intToCol(polygonIdx, col);

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
	(void) setDrawOptions;

	drawNavMeshColored(dm);
}

static void navDM_drawFacesSolid(DerivedMesh *dm,
								float (*partial_redraw_planes)[4],
								int UNUSED(fast), int (*setMaterial)(int, void *attribs))
{
	(void) partial_redraw_planes;
	(void) setMaterial;

	//drawFacesSolid_original(dm, partial_redraw_planes, fast, setMaterial);
	drawNavMeshColored(dm);
}
#endif /* WITH_GAMEENGINE */

static DerivedMesh *createNavMeshForVisualization(NavMeshModifierData *UNUSED(mmd), DerivedMesh *dm)
{
#ifdef WITH_GAMEENGINE
	DerivedMesh *result;
	int maxFaces = dm->getNumFaces(dm);
	int *recastData;
	int vertsPerPoly=0, nverts=0, ndtris=0, npolys=0; 
	float* verts=NULL;
	unsigned short *dtris=NULL, *dmeshes=NULL, *polys=NULL;
	int *dtrisToPolysMap=NULL, *dtrisToTrisMap=NULL, *trisToFacesMap=NULL;
	int res;

	result = CDDM_copy(dm);
	if (!CustomData_has_layer(&result->faceData, CD_RECAST)) 
	{
		int *sourceRecastData = (int*)CustomData_get_layer(&dm->faceData, CD_RECAST);
		CustomData_add_layer_named(&result->faceData, CD_RECAST, CD_DUPLICATE, 
			sourceRecastData, maxFaces, "recastData");
	}
	recastData = (int*)CustomData_get_layer(&result->faceData, CD_RECAST);
	result->drawFacesTex =  navDM_drawFacesTex;
	result->drawFacesSolid = navDM_drawFacesSolid;
	
	
	//process mesh
	res  = buildNavMeshDataByDerivedMesh(dm, &vertsPerPoly, &nverts, &verts, &ndtris, &dtris,
										&npolys, &dmeshes, &polys, &dtrisToPolysMap, &dtrisToTrisMap,
										&trisToFacesMap);
	if (res)
	{
		size_t polyIdx;

		//invalidate concave polygon
		for (polyIdx=0; polyIdx<(size_t)npolys; polyIdx++)
		{
			unsigned short* poly = &polys[polyIdx*2*vertsPerPoly];
			if (!polyIsConvex(poly, vertsPerPoly, verts))
			{
				//set negative polygon idx to all faces
				unsigned short *dmesh = &dmeshes[4*polyIdx];
				unsigned short tbase = dmesh[2];
				unsigned short tnum = dmesh[3];
				unsigned short ti;

				for (ti=0; ti<tnum; ti++)
				{
					unsigned short triidx = dtrisToTrisMap[tbase+ti];
					unsigned short faceidx = trisToFacesMap[triidx];
					if (recastData[faceidx]>0)
						recastData[faceidx] = -recastData[faceidx];
				}				
			}
		}

	}
	else
	{
		printf("Error during creation polygon infos\n");
	}

	//clean up
	if (verts!=NULL)
		MEM_freeN(verts);
	if (dtris!=NULL)
		MEM_freeN(dtris);
	if (dmeshes!=NULL)
		MEM_freeN(dmeshes);
	if (polys!=NULL)
		MEM_freeN(polys);
	if (dtrisToPolysMap!=NULL)
		MEM_freeN(dtrisToPolysMap);
	if (dtrisToTrisMap!=NULL)
		MEM_freeN(dtrisToTrisMap);
	if (trisToFacesMap!=NULL)
		MEM_freeN(trisToFacesMap);

	return result;
#else // WITH_GAMEENGINE
	return dm;
#endif // WITH_GAMEENGINE
}

/*
static int isDisabled(ModifierData *md, int useRenderParams)
{
	NavMeshModifierData *amd = (NavMeshModifierData*) md;
	return false; 
}*/



static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
								  int UNUSED(useRenderParams), int UNUSED(isFinalCalc))
{
	DerivedMesh *result = NULL;
	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;
	int hasRecastData = CustomData_has_layer(&derivedData->faceData, CD_RECAST)>0;
	if (ob->body_type!=OB_BODY_TYPE_NAVMESH || !hasRecastData )
	{
		//convert to nav mesh object:
		//1)set physics type
		ob->gameflag &= ~OB_COLLISION;
		ob->gameflag |= OB_NAVMESH;
		ob->body_type = OB_BODY_TYPE_NAVMESH;
		//2)add and init recast data layer
		if (!hasRecastData)
		{
			Mesh* obmesh = (Mesh *)ob->data;
			if (obmesh)
			{
				int i;
				int numFaces = obmesh->totface;
				int* recastData;
				CustomData_add_layer_named(&obmesh->fdata, CD_RECAST, CD_CALLOC, NULL, numFaces, "recastData");
				recastData = (int*)CustomData_get_layer(&obmesh->fdata, CD_RECAST);
				for (i=0; i<numFaces; i++)
				{
					recastData[i] = i+1;
				}
				CustomData_add_layer_named(&derivedData->faceData, CD_RECAST, CD_REFERENCE, recastData, numFaces, "recastData");
			}
		}
	}

	result = createNavMeshForVisualization(nmmd, derivedData);
	
	return result;
}


ModifierTypeInfo modifierType_NavMesh = {
	/* name */              "NavMesh",
	/* structName */        "NavMeshModifierData",
	/* structSize */        sizeof(NavMeshModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             (ModifierTypeFlag) (eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_Single),
	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformMatrices */    0,
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
