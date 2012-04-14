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

/** \file blender/blenkernel/intern/navmesh_conversion.c
 *  \ingroup bke
 */

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BKE_navmesh_conversion.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "recast-capi.h"

BLI_INLINE float area2(const float* a, const float* b, const float* c)
{
	return (b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]);
}

BLI_INLINE int left(const float* a, const float* b, const float* c)
{
	return area2(a, b, c) < 0;
}

int polyNumVerts(const unsigned short* p, const int vertsPerPoly)
{
	int i, nv = 0;
	for (i=0; i<vertsPerPoly; i++)
	{
		if (p[i]==0xffff)
			break;
		nv++;
	}
	return nv;
}

int polyIsConvex(const unsigned short* p, const int vertsPerPoly, const float* verts)
{
	int j, nv = polyNumVerts(p, vertsPerPoly);
	if (nv<3)
		return 0;
	for (j=0; j<nv; j++)
	{
		const float* v = &verts[3*p[j]];
		const float* v_next = &verts[3*p[(j+1)%nv]];
		const float* v_prev = &verts[3*p[(nv+j-1)%nv]];
		if (!left(v_prev, v, v_next))
			return 0;

	}
	return 1;
}

float distPointToSegmentSq(const float* point, const float* a, const float* b)
{
	float abx[3], dx[3];
	float d, t;

	sub_v3_v3v3(abx, b,a);
	sub_v3_v3v3(dx, point,a);

	d = abx[0]*abx[0]+abx[2]*abx[2];
	t = abx[0]*dx[0]+abx[2]*dx[2];

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

int buildRawVertIndicesData(DerivedMesh* dm, int *nverts_r, float **verts_r, 
									int *ntris_r, unsigned short **tris_r, int **trisToFacesMap_r,
									int **recastData)
{
	int vi, fi, triIdx;
	int nverts, ntris;
	int *trisToFacesMap;
	float *verts;
	unsigned short *tris, *tri;
	int nfaces;
	MFace *faces;

	nverts = dm->getNumVerts(dm);
	if (nverts>=0xffff)
	{
		printf("Converting navmesh: Error! Too many vertices. Max number of vertices %d\n", 0xffff);
		return 0;
	}
	verts = MEM_callocN(sizeof(float)*3*nverts, "buildRawVertIndicesData verts");
	dm->getVertCos(dm, (float(*)[3])verts);

	//flip coordinates
	for (vi=0; vi<nverts; vi++)
	{
		SWAP(float, verts[3*vi+1], verts[3*vi+2]);
	}

	//calculate number of tris
	nfaces = dm->getNumTessFaces(dm);
	faces = dm->getTessFaceArray(dm);
	ntris = nfaces;
	for (fi=0; fi<nfaces; fi++)
	{
		MFace* face = &faces[fi];
		if (face->v4)
			ntris++;
	}

	//copy and transform to triangles (reorder on the run)
	trisToFacesMap = MEM_callocN(sizeof(int)*ntris, "buildRawVertIndicesData trisToFacesMap");
	tris = MEM_callocN(sizeof(unsigned short)*3*ntris, "buildRawVertIndicesData tris");
	tri = tris;
	triIdx = 0;
	for (fi=0; fi<nfaces; fi++)
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
	*recastData = (int*)CustomData_get_layer(&dm->faceData, CD_RECAST);

	*nverts_r = nverts;
	*verts_r = verts;
	*ntris_r = ntris;
	*tris_r = tris;
	*trisToFacesMap_r = trisToFacesMap;

	return 1;
}

int buildPolygonsByDetailedMeshes(const int vertsPerPoly, const int npolys, 
										  unsigned short* polys, const unsigned short* dmeshes, 
										  const float* verts, const unsigned short* dtris, 
										  const int* dtrisToPolysMap)
{
	int polyidx;
	int capacity = vertsPerPoly;
	unsigned short* newPoly = MEM_callocN(sizeof(unsigned short)*capacity, "buildPolygonsByDetailedMeshes newPoly");
	memset(newPoly, 0xff, sizeof(unsigned short)*capacity);

	for (polyidx=0; polyidx<npolys; polyidx++)
	{
		size_t i;
		int j, k;
		int nv = 0;
		//search border 
		int tri, btri = -1;
		int edge, bedge = -1;
		int dtrisNum = dmeshes[polyidx*4+3];
		int dtrisBase = dmeshes[polyidx*4+2];
		unsigned char *traversedTris = MEM_callocN(sizeof(unsigned char)*dtrisNum, "buildPolygonsByDetailedMeshes traversedTris");
		unsigned short* adjustedPoly;
		int adjustedNv;
		int allBorderTraversed;

		for (j=0; j<dtrisNum && btri==-1;j++)
		{
			int curpolytri = dtrisBase+j;
			for (k=0; k<3; k++)
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
			MEM_freeN(traversedTris);
			MEM_freeN(newPoly);

			return 0;
		}

		newPoly[nv++] = dtris[btri*3*2+bedge];
		tri = btri;
		edge = (bedge+1)%3;
		traversedTris[tri-dtrisBase] = 1;
		while (tri!=btri || edge!=bedge)
		{
			int neighbortri = dtris[tri*3*2+3+edge];
			if (neighbortri==0xffff || dtrisToPolysMap[neighbortri]!=polyidx+1)
			{
				if (nv==capacity)
				{
					unsigned short* newPolyBig;
					capacity += vertsPerPoly;
					newPolyBig = MEM_callocN(sizeof(unsigned short)*capacity, "buildPolygonsByDetailedMeshes newPolyBig");
					memset(newPolyBig, 0xff, sizeof(unsigned short)*capacity);
					memcpy(newPolyBig, newPoly, sizeof(unsigned short)*nv);
					MEM_freeN(newPoly);
					newPoly = newPolyBig;			
				}
				newPoly[nv++] = dtris[tri*3*2+edge];
				//move to next edge					
				edge = (edge+1)%3;
			}
			else {
				//move to next tri
				int twinedge = -1;
				for (k=0; k<3; k++)
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
					MEM_freeN(traversedTris);
					goto returnLabel;
				}
				tri = neighbortri;
				edge = (twinedge+1)%3;
				traversedTris[tri-dtrisBase] = 1;
			}
		}

		adjustedPoly = MEM_callocN(sizeof(unsigned short)*nv, "buildPolygonsByDetailedMeshes adjustedPoly");
		adjustedNv = 0;
		for (i=0; i<nv; i++)
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
		MEM_freeN(adjustedPoly);
		nv = adjustedNv;

		allBorderTraversed = 1;
		for (i=0; i<dtrisNum; i++)
		{
			if (traversedTris[i]==0)
			{
				//check whether it has border edges
				int curpolytri = dtrisBase+i;
				for (k=0; k<3; k++)
				{
					unsigned short neighbortri = dtris[curpolytri*3*2+3+k];
					if ( neighbortri==0xffff || dtrisToPolysMap[neighbortri]!=polyidx+1)
					{
						allBorderTraversed = 0;
						break;
					}
				}
			}				
		}

		if (nv<=vertsPerPoly && allBorderTraversed)
		{
			for (i=0; i<nv; i++)
			{
				polys[polyidx*vertsPerPoly*2+i] = newPoly[i];
			}
		}

		MEM_freeN(traversedTris);
	}

returnLabel:
	MEM_freeN(newPoly);

	return 1;
}

struct SortContext
{
	const int* recastData;
	const int* trisToFacesMap;
};

static int compareByData(void *ctx, const void * a, const void * b)
{
	return (((struct SortContext *)ctx)->recastData[((struct SortContext *)ctx)->trisToFacesMap[*(int*)a]] -
			((struct SortContext *)ctx)->recastData[((struct SortContext *)ctx)->trisToFacesMap[*(int*)b]] );
}

int buildNavMeshData(const int nverts, const float* verts, 
							 const int ntris, const unsigned short *tris, 
							 const int* recastData, const int* trisToFacesMap,
							 int *ndtris_r, unsigned short **dtris_r,
							 int *npolys_r, unsigned short **dmeshes_r, unsigned short **polys_r,
							 int *vertsPerPoly_r, int **dtrisToPolysMap_r, int **dtrisToTrisMap_r)

{
	int *trisMapping;
	int i;
	struct SortContext context;
	int validTriStart, prevPolyIdx, curPolyIdx, newPolyIdx, prevpolyidx;
	unsigned short *dmesh;

	int ndtris, npolys, vertsPerPoly;
	unsigned short *dtris, *dmeshes, *polys;
	int *dtrisToPolysMap, *dtrisToTrisMap;

	if (!recastData)
	{
		printf("Converting navmesh: Error! Can't find recast custom data\n");
		return 0;
	}

	trisMapping = MEM_callocN(sizeof(int)*ntris, "buildNavMeshData trisMapping");

	//sort the triangles by polygon idx
	for (i=0; i<ntris; i++)
		trisMapping[i]=i;
	context.recastData = recastData;
	context.trisToFacesMap = trisToFacesMap;
	recast_qsort(trisMapping, ntris, sizeof(int), &context, compareByData);

	//search first valid triangle - triangle of convex polygon
	validTriStart = -1;
	for (i=0; i< ntris; i++)
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
		MEM_freeN(trisMapping);
		return 0;
	}

	ndtris = ntris-validTriStart;
	//fill dtris to faces mapping
	dtrisToTrisMap = MEM_callocN(sizeof(int)*ndtris, "buildNavMeshData dtrisToTrisMap");
	memcpy(dtrisToTrisMap, &trisMapping[validTriStart], ndtris*sizeof(int));
	MEM_freeN(trisMapping);

	//create detailed mesh triangles  - copy only valid triangles
	//and reserve memory for adjacency info
	dtris = MEM_callocN(sizeof(unsigned short)*3*2*ndtris, "buildNavMeshData dtris");
	memset(dtris, 0xffff, sizeof(unsigned short)*3*2*ndtris);
	for (i=0; i<ndtris; i++)
	{
		memcpy(dtris+3*2*i, tris+3*dtrisToTrisMap[i], sizeof(unsigned short)*3);
	}

	//create new recast data corresponded to dtris and renumber for continuous indices
	prevPolyIdx = -1;
	newPolyIdx = 0;
	dtrisToPolysMap = MEM_callocN(sizeof(int)*ndtris, "buildNavMeshData dtrisToPolysMap");
	for (i=0; i<ndtris; i++)
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
	recast_buildMeshAdjacency(dtris, ndtris, nverts, 3);

	//create detailed mesh description for each navigation polygon
	npolys = dtrisToPolysMap[ndtris-1];
	dmeshes = MEM_callocN(sizeof(unsigned short)*npolys*4, "buildNavMeshData dmeshes");
	memset(dmeshes, 0, npolys*4*sizeof(unsigned short));
	dmesh = NULL;
	prevpolyidx = 0;
	for (i=0; i<ndtris; i++)
	{
		int curpolyidx = dtrisToPolysMap[i];
		if (curpolyidx!=prevpolyidx)
		{
			if (curpolyidx!=prevpolyidx+1)
			{
				printf("Converting navmesh: Error! Wrong order of detailed mesh faces\n");
				return 0;
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
	polys = MEM_callocN(sizeof(unsigned short)*npolys*vertsPerPoly*2, "buildNavMeshData polys");
	memset(polys, 0xff, sizeof(unsigned short)*vertsPerPoly*2*npolys);

	buildPolygonsByDetailedMeshes(vertsPerPoly, npolys, polys, dmeshes, verts, dtris, dtrisToPolysMap);

	*ndtris_r = ndtris;
	*npolys_r = npolys;
	*vertsPerPoly_r = vertsPerPoly;
	*dtris_r = dtris;
	*dmeshes_r = dmeshes;
	*polys_r = polys;
	*dtrisToPolysMap_r = dtrisToPolysMap;
	*dtrisToTrisMap_r = dtrisToTrisMap;

	return 1;
}


int buildNavMeshDataByDerivedMesh(DerivedMesh *dm, int *vertsPerPoly, 
										  int *nverts, float **verts,
										  int *ndtris, unsigned short **dtris,
										  int *npolys, unsigned short **dmeshes,
										  unsigned short **polys, int **dtrisToPolysMap,
										  int **dtrisToTrisMap, int **trisToFacesMap)
{
	int res = 1;
	int ntris = 0, *recastData=NULL;
	unsigned short *tris=NULL;

	res = buildRawVertIndicesData(dm, nverts, verts, &ntris, &tris, trisToFacesMap, &recastData);
	if (!res)
	{
		printf("Converting navmesh: Error! Can't get vertices and indices from mesh\n");
		goto exit;
	}

	res = buildNavMeshData(*nverts, *verts, ntris, tris, recastData, *trisToFacesMap,
		ndtris, dtris, npolys, dmeshes,polys, vertsPerPoly, 
		dtrisToPolysMap, dtrisToTrisMap);
	if (!res)
	{
		printf("Converting navmesh: Error! Can't get vertices and indices from mesh\n");
		goto exit;
	}

exit:
	if (tris)
		MEM_freeN(tris);

	return res;
}

int polyFindVertex(const unsigned short* p, const int vertsPerPoly, unsigned short vertexIdx)
{
	int i, res = -1;
	for (i=0; i<vertsPerPoly; i++)
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
