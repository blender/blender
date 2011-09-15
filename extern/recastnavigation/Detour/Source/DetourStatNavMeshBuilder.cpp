//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "DetourStatNavMesh.h"

struct BVItem
{
	unsigned short bmin[3];
	unsigned short bmax[3];
	int i;
};

static int compareItemX(const void* va, const void* vb)
{
	const BVItem* a = (const BVItem*)va;
	const BVItem* b = (const BVItem*)vb;
	if (a->bmin[0] < b->bmin[0])
		return -1;
	if (a->bmin[0] > b->bmin[0])
		return 1;
	return 0;
}

static int compareItemY(const void* va, const void* vb)
{
	const BVItem* a = (const BVItem*)va;
	const BVItem* b = (const BVItem*)vb;
	if (a->bmin[1] < b->bmin[1])
		return -1;
	if (a->bmin[1] > b->bmin[1])
		return 1;
	return 0;
}

static int compareItemZ(const void* va, const void* vb)
{
	const BVItem* a = (const BVItem*)va;
	const BVItem* b = (const BVItem*)vb;
	if (a->bmin[2] < b->bmin[2])
		return -1;
	if (a->bmin[2] > b->bmin[2])
		return 1;
	return 0;
}

static void calcExtends(BVItem* items, int nitems, int imin, int imax,
						unsigned short* bmin, unsigned short* bmax)
{
	bmin[0] = items[imin].bmin[0];
	bmin[1] = items[imin].bmin[1];
	bmin[2] = items[imin].bmin[2];
	
	bmax[0] = items[imin].bmax[0];
	bmax[1] = items[imin].bmax[1];
	bmax[2] = items[imin].bmax[2];
	
	for (int i = imin+1; i < imax; ++i)
	{
		const BVItem& it = items[i];
		if (it.bmin[0] < bmin[0]) bmin[0] = it.bmin[0];
		if (it.bmin[1] < bmin[1]) bmin[1] = it.bmin[1];
		if (it.bmin[2] < bmin[2]) bmin[2] = it.bmin[2];
		
		if (it.bmax[0] > bmax[0]) bmax[0] = it.bmax[0];
		if (it.bmax[1] > bmax[1]) bmax[1] = it.bmax[1];
		if (it.bmax[2] > bmax[2]) bmax[2] = it.bmax[2];
	}
}

inline int longestAxis(unsigned short x, unsigned short y, unsigned short z)
{
	int	axis = 0;
	unsigned short maxVal = x;
	if (y > maxVal)
	{
		axis = 1;
		maxVal = y;
	}
	if (z > maxVal)
	{
		axis = 2;
		maxVal = z;
	}
	return axis;
}

static void subdivide(BVItem* items, int nitems, int imin, int imax, int& curNode, dtStatBVNode* nodes)
{
	int inum = imax - imin;
	int icur = curNode;
	
	dtStatBVNode& node = nodes[curNode++];
	
	if (inum == 1)
	{
		// Leaf
		node.bmin[0] = items[imin].bmin[0];
		node.bmin[1] = items[imin].bmin[1];
		node.bmin[2] = items[imin].bmin[2];
		
		node.bmax[0] = items[imin].bmax[0];
		node.bmax[1] = items[imin].bmax[1];
		node.bmax[2] = items[imin].bmax[2];
		
		node.i = items[imin].i;
	}
	else
	{
		// Split
		calcExtends(items, nitems, imin, imax, node.bmin, node.bmax);
		
		int	axis = longestAxis(node.bmax[0] - node.bmin[0],
							   node.bmax[1] - node.bmin[1],
							   node.bmax[2] - node.bmin[2]);
		
		if (axis == 0)
		{
			// Sort along x-axis
			qsort(items+imin, inum, sizeof(BVItem), compareItemX);
		}
		else if (axis == 1)
		{
			// Sort along y-axis
			qsort(items+imin, inum, sizeof(BVItem), compareItemY);
		}
		else
		{
			// Sort along z-axis
			qsort(items+imin, inum, sizeof(BVItem), compareItemZ);
		}
		
		int isplit = imin+inum/2;
		
		// Left
		subdivide(items, nitems, imin, isplit, curNode, nodes);
		// Right
		subdivide(items, nitems, isplit, imax, curNode, nodes);
		
		int iescape = curNode - icur;
		// Negative index means escape.
		node.i = -iescape;
	}
}

/*static*/ int createBVTree(const unsigned short* verts, const int nverts,
						const unsigned short* polys, const int npolys, const int nvp,
						float cs, float ch,
						int nnodes, dtStatBVNode* nodes)
{
	// Build tree
	BVItem* items = new BVItem[npolys];
	for (int i = 0; i < npolys; i++)
	{
		BVItem& it = items[i];
		it.i = i+1;
		// Calc polygon bounds.
		const unsigned short* p = &polys[i*nvp*2];
		it.bmin[0] = it.bmax[0] = verts[p[0]*3+0];
		it.bmin[1] = it.bmax[1] = verts[p[0]*3+1];
		it.bmin[2] = it.bmax[2] = verts[p[0]*3+2];
		
		for (int j = 1; j < nvp; ++j)
		{
			if (p[j] == 0xffff) break;
			unsigned short x = verts[p[j]*3+0];
			unsigned short y = verts[p[j]*3+1];
			unsigned short z = verts[p[j]*3+2];
			
			if (x < it.bmin[0]) it.bmin[0] = x;
			if (y < it.bmin[1]) it.bmin[1] = y;
			if (z < it.bmin[2]) it.bmin[2] = z;
			
			if (x > it.bmax[0]) it.bmax[0] = x;
			if (y > it.bmax[1]) it.bmax[1] = y;
			if (z > it.bmax[2]) it.bmax[2] = z;
		}
		// Remap y
		it.bmin[1] = (unsigned short)floorf((float)it.bmin[1]*ch/cs);
		it.bmax[1] = (unsigned short)ceilf((float)it.bmax[1]*ch/cs);
	}
	
	int curNode = 0;
	subdivide(items, npolys, 0, npolys, curNode, nodes);
	
	delete [] items;
	
	return curNode;
}


bool dtCreateNavMeshData(const unsigned short* verts, const int nverts,
						 const unsigned short* polys, const int npolys, const int nvp,
						 const float* bmin, const float* bmax, float cs, float ch,
						 const unsigned short* dmeshes, const float* dverts, const int ndverts,
						 const unsigned char* dtris, const int ndtris, 
						 unsigned char** outData, int* outDataSize)
{
	if (nvp > DT_STAT_VERTS_PER_POLYGON)
		return false;
	if (nverts >= 0xffff)
		return false;
		
	if (!nverts)
		return false;
	if (!npolys)
		return false;
	if (!dmeshes || !dverts || ! dtris)
		return false;
	
	// Find unique detail vertices.
	int uniqueDetailVerts = 0;
	if (dmeshes)
	{
		for (int i = 0; i < npolys; ++i)
		{
			const unsigned short* p = &polys[i*nvp*2];
			int ndv = dmeshes[i*4+1];
			int nv = 0;
			for (int j = 0; j < nvp; ++j)
			{
				if (p[j] == 0xffff) break;
				nv++;
			}
			ndv -= nv;
			uniqueDetailVerts += ndv;
		}
	}
	
	// Calculate data size
	const int headerSize = sizeof(dtStatNavMeshHeader);
	const int vertsSize = sizeof(float)*3*nverts;
	const int polysSize = sizeof(dtStatPoly)*npolys;
	const int nodesSize = sizeof(dtStatBVNode)*npolys*2;
	const int detailMeshesSize = sizeof(dtStatPolyDetail)*npolys;
	const int detailVertsSize = sizeof(float)*3*uniqueDetailVerts;
	const int detailTrisSize = sizeof(unsigned char)*4*ndtris;
	
	const int dataSize = headerSize + vertsSize + polysSize + nodesSize +
						 detailMeshesSize + detailVertsSize + detailTrisSize;
	unsigned char* data = new unsigned char[dataSize];
	if (!data)
		return false;
	memset(data, 0, dataSize);

	unsigned char* d = data;
	dtStatNavMeshHeader* header = (dtStatNavMeshHeader*)d; d += headerSize;
	float* navVerts = (float*)d; d += vertsSize;
	dtStatPoly* navPolys = (dtStatPoly*)d; d += polysSize;
	dtStatBVNode* navNodes = (dtStatBVNode*)d; d += nodesSize;
	dtStatPolyDetail* navDMeshes = (dtStatPolyDetail*)d; d += detailMeshesSize;
	float* navDVerts = (float*)d; d += detailVertsSize;
	unsigned char* navDTris = (unsigned char*)d; d += detailTrisSize;
	
	// Store header
	header->magic = DT_STAT_NAVMESH_MAGIC;
	header->version = DT_STAT_NAVMESH_VERSION;
	header->npolys = npolys;
	header->nverts = nverts;
	header->cs = cs;
	header->bmin[0] = bmin[0];
	header->bmin[1] = bmin[1];
	header->bmin[2] = bmin[2];
	header->bmax[0] = bmax[0];
	header->bmax[1] = bmax[1];
	header->bmax[2] = bmax[2];
	header->ndmeshes = dmeshes ? npolys : 0;
	header->ndverts = dmeshes ? uniqueDetailVerts : 0;
	header->ndtris = dmeshes ? ndtris : 0;
	
	// Store vertices
	for (int i = 0; i < nverts; ++i)
	{
		const unsigned short* iv = &verts[i*3];
		float* v = &navVerts[i*3];
		v[0] = bmin[0] + iv[0] * cs;
		v[1] = bmin[1] + iv[1] * ch;
		v[2] = bmin[2] + iv[2] * cs;
	}
	
	// Store polygons
	const unsigned short* src = polys;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPoly* p = &navPolys[i];
		p->nv = 0;
		for (int j = 0; j < nvp; ++j)
		{
			if (src[j] == 0xffff) break;
			p->v[j] = src[j];
			p->n[j] = src[nvp+j]+1;
			p->nv++;
		}
		src += nvp*2;
	}
	
	header->nnodes = createBVTree(verts, nverts, polys, npolys, nvp,
					 cs, ch, npolys*2, navNodes);
	

	// Store detail meshes and vertices.
	// The nav polygon vertices are stored as the first vertices on each mesh.
	// We compress the mesh data by skipping them and using the navmesh coordinates.
	unsigned short vbase = 0;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPolyDetail& dtl = navDMeshes[i];
		const int vb = dmeshes[i*4+0];
		const int ndv = dmeshes[i*4+1];
		const int nv = navPolys[i].nv;
		dtl.vbase = vbase;
		dtl.nverts = ndv-nv;
		dtl.tbase = dmeshes[i*4+2];
		dtl.ntris = dmeshes[i*4+3];
		// Copy vertices except the first 'nv' verts which are equal to nav poly verts.
		if (ndv-nv)
		{
			memcpy(&navDVerts[vbase*3], &dverts[(vb+nv)*3], sizeof(float)*3*(ndv-nv));
			vbase += ndv-nv;
		}
	}
	// Store triangles.
	memcpy(navDTris, dtris, sizeof(unsigned char)*4*ndtris);
	
	*outData = data;
	*outDataSize = dataSize;
	
	return true;
}
