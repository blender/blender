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

	//.todo - deep copy
}

static DerivedMesh *buildNavMesh(NavMeshModifierData *mmd,DerivedMesh *dm)
{
	const int nverts = dm->getNumVerts(dm);
	MVert *mvert = dm->getVertArray(dm);
	const int nfaces = dm->getNumFaces(dm);
	MFace *mface = dm->getFaceArray(dm);
	float* verts;
	int *tris, *tri;
	float bmin[3], bmax[3];
	int i,j;
	DerivedMesh* result = NULL;
	rcHeightfield* solid;
	unsigned char *triflags;
	rcCompactHeightfield* chf;
	rcContourSet *cset;
	rcPolyMesh* pmesh;
	rcPolyMeshDetail* dmesh;
	int numVerts, numEdges, numFaces;

	//calculate count of tris
	int ntris = nfaces;
	for (i=0; i<nfaces; i++)
	{
		MFace* mf = &mface[i];
		if (mf->v4)
			ntris+=1;
	}

	//create verts
	verts = (float*) MEM_mallocN(sizeof(float)*3*nverts, "verts");
	for (i=0; i<nverts; i++)
	{
		MVert *v = &mvert[i];
		verts[3*i+0] = v->co[0];
		verts[3*i+1] = v->co[2];
		verts[3*i+2] = v->co[1];
	}
	//create tris
	tris = (int*) MEM_mallocN(sizeof(int)*3*ntris, "faces");
	tri = tris;
	for (i=0; i<nfaces; i++)
	{
		MFace* mf = &mface[i]; 
		tri[0]= mf->v1; tri[1]= mf->v3;	tri[2]= mf->v2; 
		tri += 3;
		if (mf->v4)
		{
			tri[0]= mf->v1; tri[1]= mf->v4; tri[2]= mf->v3; 
			tri += 3;
		}
	}

	rcCalcBounds(verts, nverts, bmin, bmax);

	//
	// Step 1. Initialize build config.
	//
	rcConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = mmd->cellsize;
	cfg.ch = mmd->cellheight;
	cfg.walkableSlopeAngle = mmd->agentmaxslope;
	cfg.walkableHeight = (int)ceilf(mmd->agentheight/ cfg.ch);
	cfg.walkableClimb = (int)floorf(mmd->agentmaxclimb / cfg.ch);
	cfg.walkableRadius = (int)ceilf(mmd->agentradius / cfg.cs);
	cfg.maxEdgeLen = (int)(mmd->edgemaxlen/ mmd->cellsize);
	cfg.maxSimplificationError = mmd->edgemaxerror;
	cfg.minRegionSize = (int)rcSqr(mmd->regionminsize);
	cfg.mergeRegionSize = (int)rcSqr(mmd->regionmergesize);
	cfg.maxVertsPerPoly = mmd->vertsperpoly;
	cfg.detailSampleDist = mmd->detailsampledist< 0.9f ? 0 : mmd->cellsize * mmd->detailsampledist;
	cfg.detailSampleMaxError = mmd->cellheight * mmd->detailsamplemaxerror;

	// Set the area where the navigation will be build.
	vcopy(cfg.bmin, bmin);
	vcopy(cfg.bmax, bmax);
	rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);

	//
	// Step 2. Rasterize input polygon soup.
	//
	// Allocate voxel heightfield where we rasterize our input data to.
	solid = new rcHeightfield;
	if (!solid)
		return NULL;

	if (!rcCreateHeightfield(*solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
		return NULL;

	// Allocate array that can hold triangle flags.
	triflags = (unsigned char*) MEM_mallocN(sizeof(unsigned char)*ntris, "triflags");
	if (!triflags)
		return NULL;
	// Find triangles which are walkable based on their slope and rasterize them.
	memset(triflags, 0, ntris*sizeof(unsigned char));
	rcMarkWalkableTriangles(cfg.walkableSlopeAngle, verts, nverts, tris, ntris, triflags);
	rcRasterizeTriangles(verts, nverts, tris, triflags, ntris, *solid);
	MEM_freeN(triflags);
	MEM_freeN(verts);
	MEM_freeN(tris);

	//
	// Step 3. Filter walkables surfaces.
	//
	rcFilterLedgeSpans(cfg.walkableHeight, cfg.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(cfg.walkableHeight, *solid);

	//
	// Step 4. Partition walkable surface to simple regions.
	//

	chf = new rcCompactHeightfield;
	if (!chf)
		return NULL;
	if (!rcBuildCompactHeightfield(cfg.walkableHeight, cfg.walkableClimb, RC_WALKABLE, *solid, *chf))
		return NULL;

	delete solid; 

	// Prepare for region partitioning, by calculating distance field along the walkable surface.
	if (!rcBuildDistanceField(*chf))
		return NULL;

	// Partition the walkable surface into simple regions without holes.
	if (!rcBuildRegions(*chf, cfg.walkableRadius, cfg.borderSize, cfg.minRegionSize, cfg.mergeRegionSize))
		return NULL;
	
	//
	// Step 5. Trace and simplify region contours.
	//
	// Create contours.
	cset = new rcContourSet;
	if (!cset)
		return NULL;

	if (!rcBuildContours(*chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset))
		return NULL;

	//
	// Step 6. Build polygons mesh from contours.
	//
	pmesh = new rcPolyMesh;
	if (!pmesh)
		return NULL;
	if (!rcBuildPolyMesh(*cset, cfg.maxVertsPerPoly, *pmesh))
		return NULL;


	//
	// Step 7. Create detail mesh which allows to access approximate height on each polygon.
	//

	dmesh = new rcPolyMeshDetail;
	if (!dmesh)
		return NULL;

	if (!rcBuildPolyMeshDetail(*pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh))
		return NULL;

	delete chf;
	delete cset;


	//
	// Create blender mesh from detail poly mesh 
	//

	numVerts = dmesh->nverts;
	numFaces = dmesh->ntris;
	numEdges = dmesh->ntris*3;

	result = CDDM_new(numVerts, numEdges, numFaces);
	//copy verts
	for(i = 0; i < numVerts; i++) {
		MVert *mv = CDDM_get_vert(result, i);
		copy_v3_v3(mv->co, &dmesh->verts[3*i]);
		SWAP(float, mv->co[1], mv->co[2]);
	}

	//create faces and edges
	numFaces = numEdges = 0;
	for (i=0; i<dmesh->nmeshes; i++)
	{
		unsigned short vbase = dmesh->meshes[4*i+0];
		unsigned short vnum = dmesh->meshes[4*i+1];
		unsigned short tribase = dmesh->meshes[4*i+2];
		unsigned short trinum = dmesh->meshes[4*i+3];

		for (j=0; j<trinum; j++)
		{
			unsigned char* tri = &dmesh->tris[4*(tribase+j)];
			MFace *mf = CDDM_get_face(result, numFaces);
			MEdge *med;
			mf->v1 = vbase + tri[0];
			mf->v2 = vbase + tri[1];
			mf->v3 = vbase + tri[2];
			numFaces++;
			
			{
				int e1=0, e2=2;
				for (;e1<3; e2=e1++)
				{
					med = CDDM_get_edge(result, numEdges);
					med->v1 = vbase + tri[e2];
					med->v2 = vbase + tri[e1];
					numEdges++;
				}
			}
		}
	}

	
	delete pmesh;
	delete dmesh;

	return result;
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

static void navDM_drawFacesSolid(DerivedMesh *dm,
								float (*partial_redraw_planes)[4],
								int fast, int (*setMaterial)(int, void *attribs))
{
	int a, glmode;
	MVert *mvert = (MVert *)CustomData_get_layer(&dm->vertData, CD_MVERT);
	MFace *mface = (MFace *)CustomData_get_layer(&dm->faceData, CD_MFACE);
	int* polygonIdx = (int*)CustomData_get_layer(&dm->faceData, CD_PROP_INT);
	float col[3];
	col[0] = 1.f;
	col[1] = 0.f;
	col[2] = 0.f;
	if(GPU_buffer_legacy(dm) ) {
		DEBUG_VBO( "Using legacy code. navDM_drawFacesSolid\n" );
		//glShadeModel(GL_SMOOTH);
		glBegin(glmode = GL_QUADS);
		for(a = 0; a < dm->numFaceData; a++, mface++, polygonIdx++) {
			int new_glmode = mface->v4?GL_QUADS:GL_TRIANGLES;
			intToCol(a, col);
			//intToCol(*polygonIdx, col);

			if(new_glmode != glmode) {
				glEnd();
				glBegin(glmode = new_glmode);
			}
			//glColor3fv(col);
			glVertex3fv(mvert[mface->v1].co);
			glVertex3fv(mvert[mface->v2].co);
			glVertex3fv(mvert[mface->v3].co);
			if(mface->v4) {
				glVertex3fv(mvert[mface->v4].co);
			}
		}
		glEnd();
	}
}

static DerivedMesh *testCreateNavMesh(NavMeshModifierData *mmd,DerivedMesh *dm)
{
	int i;
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);

/*	MVert *mv;
	MEdge *med;
	MFace *mf;
	numVerts = numEdges = numFaces = 0;

	result = CDDM_new(3, 3, 1);
	mv = CDDM_get_vert(result, 0);
	mv->co[0] = -10; mv->co[1] = -10; mv->co[2] = 0;
	mv = CDDM_get_vert(result, 1);
	mv->co[0] = -10; mv->co[1] = 10; mv->co[2] = 0;
	mv = CDDM_get_vert(result, 2);
	mv->co[0] = 10; mv->co[1] = -10; mv->co[2] = 0;
	
	med = CDDM_get_edge(result, 0);
	med->v1 = 0; med->v1 = 1;
	med = CDDM_get_edge(result, 1);
	med->v1 = 1; med->v1 = 2;
	med = CDDM_get_edge(result, 2);
	med->v1 = 2; med->v1 = 0;

	mf = CDDM_get_face(result, 0);
	mf->v1 = 0; mf->v2 = 1; mf->v3 = 2;
*/

	int actualFaces = 0;
	for(i = 0; i < maxFaces; i++) {
		int* polygonIdx = (int*)CustomData_get(&dm->faceData, i, CD_PROP_INT);
		if (*polygonIdx==1)
			actualFaces++;
	}


	result = CDDM_new(maxVerts, maxEdges, maxFaces);//maxFaces actualFaces
	result->drawFacesSolid = navDM_drawFacesSolid;
	numVerts = numEdges = numFaces = 0;
	for(i = 0; i < maxVerts; i++) {
		MVert inMV;
		MVert *mv = CDDM_get_vert(result, numVerts);
		float co[3];
		dm->getVert(dm, i, &inMV);
		copy_v3_v3(co, inMV.co);
		*mv = inMV;
		mv->co[2] +=.5f;
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
		/*
		int* polygonIdx = (int*)CustomData_get(&dm->faceData, i, CD_PROP_INT);
		if (*polygonIdx!=2)
			continue;*/
		
		MFace inMF;
		MFace *mf = CDDM_get_face(result, numFaces);
		dm->getFace(dm, i, &inMF);
		*mf = inMF;
		numFaces++;
	}

	return result;
}


static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
								  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;

	NavMeshModifierData *nmmd = (NavMeshModifierData*) md;


	//for test
	//result = testCreateNavMesh(nmmd, derivedData);
	//result = buildNavMesh(nmmd, derivedData);

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

};