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

//service function
inline int abs(int a)
{
	return a>=0 ? a: -a;
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

inline float area2(const float* a, const float* b, const float* c)
{
	return (b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]);
}
inline bool left(const float* a, const float* b, const float* c)
{
	return area2(a, b, c) < 0;
}

inline int polyNumVerts(const unsigned short* p, const int vertsPerPoly)
{
	int nv = 0;
	for (int i=0; i<vertsPerPoly; i++)
	{
		if (p[i]==0xffff)
			break;
		nv++;
	}
	return nv;
}

inline bool polyIsConvex(const unsigned short* p, const int vertsPerPoly, const float* verts)
{
	int nv = polyNumVerts(p, vertsPerPoly);
	if (nv<3)
		return false;
	for (int j=0; j<nv; j++)
	{
		const float* v = &verts[3*p[j]];
		const float* v_next = &verts[3*p[(j+1)%nv]];
		const float* v_prev = &verts[3*p[(nv+j-1)%nv]];
		if (!left(v_prev, v, v_next))
			return false;

	}
	return true;
}

static float distPointToSegmentSq(const float* point, const float* a, const float* b)
{
	float abx[3], dx[3];
	vsub(abx, b,a);
	vsub(dx, point,a);
	float d = abx[0]*abx[0]+abx[2]*abx[2];
	float t = abx[0]*dx[0]+abx[2]*dx[2];
	if (d > 0)
		t /= d;
	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	dx[0] = a[0] + t*abx[0] - point[0];
	dx[2] = a[2] + t*abx[2] - point[2];
	return dx[0]*dx[0] + dx[2]*dx[2];
}

static bool buildRawVertIndicesData(DerivedMesh* dm, int &nverts, float *&verts, 
									int &ntris, unsigned short *&tris, int *&trisToFacesMap,
									int *&recastData)
{
	nverts = dm->getNumVerts(dm);
	verts = new float[3*nverts];
	dm->getVertCos(dm, (float(*)[3])verts);
	
	//flip coordinates
	for (int vi=0; vi<nverts; vi++)
	{
		SWAP(float, verts[3*vi+1], verts[3*vi+2]);
	}

	//calculate number of tris
	int nfaces = dm->getNumFaces(dm);
	MFace *faces = dm->getFaceArray(dm);
	ntris = nfaces;
	for (int fi=0; fi<nfaces; fi++)
	{
		MFace* face = &faces[fi];
		if (face->v4)
			ntris++;
	}

	//copy and transform to triangles (reorder on the run)
	trisToFacesMap = new int[ntris];
	tris = new unsigned short[3*ntris];
	unsigned short* tri = tris;
	int triIdx = 0;
	for (int fi=0; fi<nfaces; fi++)
	{
		MFace* face = &faces[fi];
		tri[3*triIdx+0] = face->v1;
		tri[3*triIdx+1] = face->v3;
		tri[3*triIdx+2] = face->v2;
		trisToFacesMap[triIdx++]=fi;
		if (face->v4)
		{
			tri[3*triIdx+0] = face->v1;
			tri[3*triIdx+1] = face->v4;
			tri[3*triIdx+2] = face->v2;
			trisToFacesMap[triIdx++]=fi;
		}
	}

	//carefully, recast data is just reference to data in derived mesh
	recastData = (int*)CustomData_get_layer(&dm->faceData, CD_PROP_INT);
	return true;
}

static bool buildPolygonsByDetailedMeshes(const int vertsPerPoly, const int npolys, 
										  unsigned short* polys, const unsigned short* dmeshes, 
										  const float* verts, const unsigned short* dtris, 
										  const int* dtrisToPolysMap)
{
	bool res = false;
	int capacity = vertsPerPoly;
	unsigned short* newPoly =  new unsigned short[capacity];
	memset(newPoly, 0xff, sizeof(unsigned short)*capacity);
	for (int polyidx=0; polyidx<npolys; polyidx++)
	{
		int nv = 0;
		//search border 
		int btri = -1;
		int bedge = -1;
		for (int j=0; j<dmeshes[polyidx*4+3] && btri==-1;j++)
		{
			int curpolytri = dmeshes[polyidx*4+2]+j;
			for (int k=0; k<3; k++)
			{
				unsigned short neighbortri = dtris[curpolytri*3*2+3+k];
				if ( neighbortri==0xffff || dtrisToPolysMap[neighbortri]!=polyidx+1)
				{
					btri = curpolytri;
					bedge = k;
					break;
				}
			}							
		}
		if (btri==-1 || bedge==-1)
		{
			//can't find triangle with border edge
			return false;
		}

		newPoly[nv++] = dtris[btri*3*2+bedge];

		int tri = btri;
		int edge = (bedge+1)%3;
		while (tri!=btri || edge!=bedge)
		{
			int neighbortri = dtris[tri*3*2+3+edge];
			if (neighbortri==0xffff || dtrisToPolysMap[neighbortri]!=polyidx+1)
			{
				if (nv==capacity)
				{
					capacity += vertsPerPoly;
					unsigned short* newPolyBig =  new unsigned short[capacity];
					memset(newPolyBig, 0xff, sizeof(unsigned short)*capacity);
					memcpy(newPolyBig, newPoly, sizeof(unsigned short)*nv);
					delete newPoly;
					newPoly = newPolyBig;			
				}
				newPoly[nv++] = dtris[tri*3*2+edge];
				//move to next edge					
				edge = (edge+1)%3;
			}
			else
			{
				//move to next tri
				int twinedge = -1;
				for (int k=0; k<3; k++)
				{
					if (dtris[neighbortri*3*2+3+k] == tri)
					{
						twinedge = k;
						break;
					}
				}
				if (twinedge==-1)
				{
					printf("Converting navmesh: Error! Can't find neighbor edge - invalid adjacency info\n");
					goto returnLabel;					
				}
				tri = neighbortri;
				edge = (twinedge+1)%3;
			}
		}

		unsigned short* adjustedPoly = new unsigned short[nv];
		int adjustedNv = 0;
		for (size_t i=0; i<(size_t)nv; i++)
		{
			unsigned short prev = newPoly[(nv+i-1)%nv];
			unsigned short cur = newPoly[i];
			unsigned short next = newPoly[(i+1)%nv];
			float distSq = distPointToSegmentSq(&verts[3*cur], &verts[3*prev], &verts[3*next]);
			static const float tolerance = 0.001f;
			if (distSq>tolerance)
				adjustedPoly[adjustedNv++] = cur;
		}
		memcpy(newPoly, adjustedPoly, adjustedNv*sizeof(unsigned short));
		delete adjustedPoly;
		nv = adjustedNv;

		if (nv<=vertsPerPoly)
		{
			for (int i=0; i<nv; i++)
			{
				polys[polyidx*vertsPerPoly*2+i] = newPoly[i];
			}
		}
		else
		{
			int a=0;
		}
	}
	res = true;

returnLabel:
	delete newPoly;
	return true;
}

struct SortContext
{
	const int* recastData;
	const int* trisToFacesMap;
};
static int compareByData(void* data, const void * a, const void * b){
	SortContext* context = (SortContext*)data;
	return ( context->recastData[context->trisToFacesMap[*(int*)a]] - 
				context->recastData[context->trisToFacesMap[*(int*)b]] );
}

static bool buildNavMeshData(const int nverts, const float* verts, 
							const int ntris, const unsigned short *tris, 
							const int* recastData, const int* trisToFacesMap,
							int &ndtris, unsigned short *&dtris,
							int &npolys, unsigned short *&dmeshes, unsigned short *&polys,
							int &vertsPerPoly, int *&dtrisToPolysMap, int *&dtrisToTrisMap)

{
	if (!recastData)
	{
		printf("Converting navmesh: Error! Can't find recast custom data\n");
		return false;
	}

	//sort the triangles by polygon idx
	int* trisMapping = new int[ntris];
	for (int i=0; i<ntris; i++)
		trisMapping[i]=i;
	SortContext context;
	context.recastData = recastData;
	context.trisToFacesMap = trisToFacesMap;
	qsort_s(trisMapping, ntris, sizeof(int), compareByData, &context);
	
	//search first valid triangle - triangle of convex polygon
	int validTriStart = -1;
	for (int i=0; i< ntris; i++)
	{
		if (recastData[trisToFacesMap[trisMapping[i]]]>0)
		{
			validTriStart = i;
			break;
		}
	}

	if (validTriStart<0)
	{
		printf("Converting navmesh: Error! No valid polygons in mesh\n");
		delete trisMapping;
		return false;
	}

	ndtris = ntris-validTriStart;
	//fill dtris to faces mapping
	dtrisToTrisMap = new int[ndtris];
	memcpy(dtrisToTrisMap, &trisMapping[validTriStart], ndtris*sizeof(int));
	delete trisMapping; trisMapping=NULL;

	//create detailed mesh triangles  - copy only valid triangles
	//and reserve memory for adjacency info
	dtris = new unsigned short[3*2*ndtris];
	memset(dtris, 0xffff, sizeof(unsigned short)*3*2*ndtris);
	for (int i=0; i<ndtris; i++)
	{
		memcpy(dtris+3*2*i, tris+3*dtrisToTrisMap[i], sizeof(unsigned short)*3);
	}
	//create new recast data corresponded to dtris
	dtrisToPolysMap = new int[ndtris];
	for (int i=0; i<ndtris; i++)
	{
		dtrisToPolysMap[i] = recastData[trisToFacesMap[dtrisToTrisMap[i]]];
	}
	
	
	//build adjacency info for detailed mesh triangles
	buildMeshAdjacency(dtris, ntris, nverts, 3);

	//create detailed mesh description for each navigation polygon
	npolys = dtrisToPolysMap[ndtris-1];
	dmeshes = new unsigned short[npolys*4];
	memset(dmeshes, 0, npolys*4*sizeof(unsigned short));
	unsigned short *dmesh = NULL;
	int prevpolyidx = 0;
	for (int i=0; i<ndtris; i++)
	{
		int curpolyidx = dtrisToPolysMap[i];
		if (curpolyidx!=prevpolyidx)
		{
			if (curpolyidx!=prevpolyidx+1)
			{
				printf("Converting navmesh: Error! Wrong order of detailed mesh faces\n");
				return false;
			}
			dmesh = dmesh==NULL ? dmeshes : dmesh+4;
			dmesh[2] = i;	//tbase
			dmesh[3] = 0;	//tnum
			prevpolyidx = curpolyidx;
		}
		dmesh[3]++;
	}

	//create navigation polygons
	vertsPerPoly = 6;
	polys = new unsigned short[npolys*vertsPerPoly*2];
	memset(polys, 0xff, sizeof(unsigned short)*vertsPerPoly*2*npolys);

	buildPolygonsByDetailedMeshes(vertsPerPoly, npolys, polys, dmeshes, verts, dtris, dtrisToPolysMap);

	return true;
}


static bool buildNavMeshDataByDerivedMesh(DerivedMesh *dm, int& vertsPerPoly, 
											int &nverts, float *&verts,
											int &ndtris, unsigned short *&dtris,
											int& npolys, unsigned short *&dmeshes,
											unsigned short*& polys, int *&dtrisToPolysMap,
											int *&dtrisToTrisMap, int *&trisToFacesMap)
{
	bool res = true;
	int ntris =0, *recastData=NULL;
	unsigned short *tris=NULL;
	res = buildRawVertIndicesData(dm, nverts, verts, ntris, tris, trisToFacesMap, recastData);
	if (!res)
	{
		printf("Converting navmesh: Error! Can't get vertices and indices from mesh\n");
		goto exit;
	}

	res = buildNavMeshData(nverts, verts, ntris, tris, recastData, trisToFacesMap,
							ndtris, dtris, npolys, dmeshes,polys, vertsPerPoly, 
							dtrisToPolysMap, dtrisToTrisMap);
	if (!res)
	{
		printf("Converting navmesh: Error! Can't get vertices and indices from mesh\n");
		goto exit;
	}

exit:
	if (tris)
		delete tris;

	return res;
}


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
	const float BLACK_COLOR[3] = {0.f, 0.f, 0.f};
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
			int polygonIdx = *(int*)CustomData_get(&dm->faceData, a, CD_PROP_INT);
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
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);

	result = CDDM_copy(dm);
	int *recastData = (int*)CustomData_get_layer(&dm->faceData, CD_PROP_INT);
	CustomData_add_layer_named(&result->faceData, CD_PROP_INT, CD_DUPLICATE, 
			recastData, maxFaces, "recastData");
	recastData = (int*)CustomData_get_layer(&result->faceData, CD_PROP_INT);
	result->drawFacesTex =  navDM_drawFacesTex;
	result->drawFacesSolid = navDM_drawFacesSolid;
	
	
	//process mesh
	int vertsPerPoly=0, nverts=0, ndtris=0, npolys=0; 
	float* verts=NULL;
	unsigned short *dtris=NULL, *dmeshes=NULL, *polys=NULL;
	int *dtrisToPolysMap=NULL, *dtrisToTrisMap=NULL, *trisToFacesMap=NULL;

	bool res  = buildNavMeshDataByDerivedMesh(dm, vertsPerPoly, nverts, verts, ndtris, dtris,
										npolys, dmeshes, polys, dtrisToPolysMap, dtrisToTrisMap,
										trisToFacesMap);
	if (res)
	{
		//invalidate concave polygon
		for (size_t polyIdx=0; polyIdx<(size_t)npolys; polyIdx++)
		{
			unsigned short* poly = &polys[polyIdx*2*vertsPerPoly];
			if (!polyIsConvex(poly, vertsPerPoly, verts))
			{
				//set negative polygon idx to all faces
				unsigned short *dmesh = &dmeshes[4*polyIdx];
				unsigned short tbase = dmesh[2];
				unsigned short tnum = dmesh[3];
				for (unsigned short ti=0; ti<tnum; ti++)
				{
					unsigned short triidx = dtrisToTrisMap[tbase+ti];
					unsigned short faceidx = trisToFacesMap[triidx];
					if (recastData[triidx]>0)
						recastData[triidx] = -recastData[triidx];
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
		delete verts;
	if (dtris!=NULL)
		delete dtris;
	if (dmeshes!=NULL)
		delete dmeshes;
	if (polys!=NULL)
		delete polys;
	if (dtrisToPolysMap!=NULL)
		delete dtrisToPolysMap;
	if (dtrisToTrisMap!=NULL)
		delete dtrisToTrisMap;	
	if (trisToFacesMap!=NULL)
		delete trisToFacesMap;		

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