/**
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
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include <math.h>
#include <stdlib.h>
#include "Recast.h"

extern "C"{
#include "BKE_navmesh_conversion.h"

#include "DNA_meshdata_types.h"
#include "BKE_cdderivedmesh.h"
#include "BLI_math.h"
}

inline float area2(const float* a, const float* b, const float* c)
{
	return (b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]);
}

inline bool left(const float* a, const float* b, const float* c)
{
	return area2(a, b, c) < 0;
}

int polyNumVerts(const unsigned short* p, const int vertsPerPoly)
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

bool polyIsConvex(const unsigned short* p, const int vertsPerPoly, const float* verts)
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

float distPointToSegmentSq(const float* point, const float* a, const float* b)
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

bool buildRawVertIndicesData(DerivedMesh* dm, int &nverts, float *&verts, 
									int &ntris, unsigned short *&tris, int *&trisToFacesMap,
									int *&recastData)
{
	nverts = dm->getNumVerts(dm);
	if (nverts>=0xffff)
	{
		printf("Converting navmesh: Error! Too many vertices. Max number of vertices %d\n", 0xffff);
		return false;
	}
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
		tri[3*triIdx+0] = (unsigned short) face->v1;
		tri[3*triIdx+1] = (unsigned short) face->v3;
		tri[3*triIdx+2] = (unsigned short) face->v2;
		trisToFacesMap[triIdx++]=fi;
		if (face->v4)
		{
			tri[3*triIdx+0] = (unsigned short) face->v1;
			tri[3*triIdx+1] = (unsigned short) face->v4;
			tri[3*triIdx+2] = (unsigned short) face->v3;
			trisToFacesMap[triIdx++]=fi;
		}
	}

	//carefully, recast data is just reference to data in derived mesh
	recastData = (int*)CustomData_get_layer(&dm->faceData, CD_RECAST);
	return true;
}

bool buildPolygonsByDetailedMeshes(const int vertsPerPoly, const int npolys, 
										  unsigned short* polys, const unsigned short* dmeshes, 
										  const float* verts, const unsigned short* dtris, 
										  const int* dtrisToPolysMap)
{
	int capacity = vertsPerPoly;
	unsigned short* newPoly =  new unsigned short[capacity];
	memset(newPoly, 0xff, sizeof(unsigned short)*capacity);
	for (int polyidx=0; polyidx<npolys; polyidx++)
	{
		int nv = 0;
		//search border 
		int btri = -1;
		int bedge = -1;
		int dtrisNum = dmeshes[polyidx*4+3];
		int dtrisBase = dmeshes[polyidx*4+2];
		unsigned char *traversedTris = new unsigned char[dtrisNum];
		memset(traversedTris, 0, dtrisNum*sizeof(unsigned char));
		for (int j=0; j<dtrisNum && btri==-1;j++)
		{
			int curpolytri = dtrisBase+j;
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
		traversedTris[tri-dtrisBase] = 1;
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
				traversedTris[tri-dtrisBase] = 1;
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

		bool allBorderTraversed = true;
		for (size_t i=0; i<(size_t)dtrisNum; i++)
		{
			if (traversedTris[i]==0)
			{
				//check whether it has border edges
				int curpolytri = dtrisBase+i;
				for (int k=0; k<3; k++)
				{
					unsigned short neighbortri = dtris[curpolytri*3*2+3+k];
					if ( neighbortri==0xffff || dtrisToPolysMap[neighbortri]!=polyidx+1)
					{
						allBorderTraversed = false;
						break;
					}
				}
			}				
		}

		if (nv<=vertsPerPoly && allBorderTraversed)
		{
			for (int i=0; i<nv; i++)
			{
				polys[polyidx*vertsPerPoly*2+i] = newPoly[i];
			}
		}
	}

returnLabel:
	delete newPoly;
	return true;
}

struct SortContext
{
	const int* recastData;
	const int* trisToFacesMap;
};

#ifdef FREE_WINDOWS
static SortContext *_mingw_context;
static int compareByData(const void * a, const void * b)
{
	return ( _mingw_context->recastData[_mingw_context->trisToFacesMap[*(int*)a]] -
			_mingw_context->recastData[_mingw_context->trisToFacesMap[*(int*)b]] );
}
#else
#if defined(_MSC_VER)
static int compareByData(void* data, const void * a, const void * b)
#elif defined(__APPLE__) || defined(__FreeBSD__)
static int compareByData(void* data, const void * a, const void * b)
#else
static int compareByData(const void * a, const void * b, void* data)
#endif
{
	const SortContext* context = (const SortContext*)data;
	return ( context->recastData[context->trisToFacesMap[*(int*)a]] - 
		context->recastData[context->trisToFacesMap[*(int*)b]] );
}
#endif

bool buildNavMeshData(const int nverts, const float* verts, 
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
#if defined(_MSC_VER)
	qsort_s(trisMapping, ntris, sizeof(int), compareByData, &context);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	qsort_r(trisMapping, ntris, sizeof(int), &context, compareByData);
#elif defined(FREE_WINDOWS)
	_mingw_context = &context;
	qsort(trisMapping, ntris, sizeof(int), compareByData);
#else
	qsort_r(trisMapping, ntris, sizeof(int), compareByData, &context);
#endif
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
	//create new recast data corresponded to dtris and renumber for continuous indices
	int prevPolyIdx=-1, curPolyIdx, newPolyIdx=0;
	dtrisToPolysMap = new int[ndtris];
	for (int i=0; i<ndtris; i++)
	{
		curPolyIdx = recastData[trisToFacesMap[dtrisToTrisMap[i]]];
		if (curPolyIdx!=prevPolyIdx)
		{
			newPolyIdx++;
			prevPolyIdx=curPolyIdx;
		}
		dtrisToPolysMap[i] = newPolyIdx;
	}


	//build adjacency info for detailed mesh triangles
	buildMeshAdjacency(dtris, ndtris, nverts, 3);

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
			dmesh[2] = (unsigned short)i;	//tbase
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


bool buildNavMeshDataByDerivedMesh(DerivedMesh *dm, int& vertsPerPoly, 
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

int polyFindVertex(const unsigned short* p, const int vertsPerPoly, unsigned short vertexIdx)
{
	int res = -1;
	for(int i=0; i<vertsPerPoly; i++)
	{
		if (p[i]==0xffff)
			break;
		if (p[i]==vertexIdx)
		{
			res = i;
			break;
		}
	}
	return res;
}
