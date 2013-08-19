/*
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "recast-capi.h"

#include <math.h>
#include "Recast.h"

static rcContext *sctx;

#define INIT_SCTX()			\
	if (sctx == NULL) sctx = new rcContext(false)

int recast_buildMeshAdjacency(unsigned short* polys, const int npolys,
			const int nverts, const int vertsPerPoly)
{
	return (int) buildMeshAdjacency(polys, npolys, nverts, vertsPerPoly);
}

void recast_calcBounds(const float *verts, int nv, float *bmin, float *bmax)
{
	rcCalcBounds(verts, nv, bmin, bmax);
}

void recast_calcGridSize(const float *bmin, const float *bmax, float cs, int *w, int *h)
{
	rcCalcGridSize(bmin, bmax, cs, w, h);
}

struct recast_heightfield *recast_newHeightfield(void)
{
	return (struct recast_heightfield *) rcAllocHeightfield();
}

void recast_destroyHeightfield(struct recast_heightfield *heightfield)
{
	rcFreeHeightField((rcHeightfield *) heightfield);
}

int recast_createHeightfield(struct recast_heightfield *hf, int width, int height,
			const float *bmin, const float* bmax, float cs, float ch)
{
	INIT_SCTX();
	return rcCreateHeightfield(sctx, *(rcHeightfield *)hf, width, height, bmin, bmax, cs, ch);
}

void recast_markWalkableTriangles(const float walkableSlopeAngle,const float *verts, int nv,
			const int *tris, int nt, unsigned char *flags)
{
	INIT_SCTX();
	rcMarkWalkableTriangles(sctx, walkableSlopeAngle, verts, nv, tris, nt, flags);
}

void recast_rasterizeTriangles(const float *verts, int nv, const int *tris,
			const unsigned char *flags, int nt, struct recast_heightfield *solid)
{
	INIT_SCTX();
	rcRasterizeTriangles(sctx, verts, nv, tris, flags, nt, *(rcHeightfield *) solid);
}

void recast_filterLedgeSpans(const int walkableHeight, const int walkableClimb,
			struct recast_heightfield *solid)
{
	INIT_SCTX();
	rcFilterLedgeSpans(sctx, walkableHeight, walkableClimb, *(rcHeightfield *) solid);
}

void recast_filterWalkableLowHeightSpans(int walkableHeight, struct recast_heightfield *solid)
{
	INIT_SCTX();
	rcFilterWalkableLowHeightSpans(sctx, walkableHeight, *(rcHeightfield *) solid);
}

void recast_filterLowHangingWalkableObstacles(const int walkableClimb, struct recast_heightfield *solid)
{
	INIT_SCTX();
	rcFilterLowHangingWalkableObstacles(sctx, walkableClimb, *(rcHeightfield *) solid);
}

struct recast_compactHeightfield *recast_newCompactHeightfield(void)
{
	return (struct recast_compactHeightfield *) rcAllocCompactHeightfield();
}

void recast_destroyCompactHeightfield(struct recast_compactHeightfield *compactHeightfield)
{
	rcFreeCompactHeightfield( (rcCompactHeightfield *) compactHeightfield);
}

int recast_buildCompactHeightfield(const int walkableHeight, const int walkableClimb,
			struct recast_heightfield *hf, struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	return rcBuildCompactHeightfield(sctx, walkableHeight, walkableClimb, 
		*(rcHeightfield *) hf, *(rcCompactHeightfield *) chf);
}

int recast_erodeWalkableArea(int radius, struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	return rcErodeWalkableArea(sctx, radius, *(rcCompactHeightfield *) chf);
}

int recast_buildDistanceField(struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	return rcBuildDistanceField(sctx, *(rcCompactHeightfield *) chf);
}

int recast_buildRegions(struct recast_compactHeightfield *chf, int borderSize,
	int minRegionSize, int mergeRegionSize)
{
	INIT_SCTX();
	return rcBuildRegions(sctx, *(rcCompactHeightfield *) chf, borderSize,
				minRegionSize, mergeRegionSize);
}

struct recast_contourSet *recast_newContourSet(void)
{
	return (struct recast_contourSet *) rcAllocContourSet();
}

void recast_destroyContourSet(struct recast_contourSet *contourSet)
{
	rcFreeContourSet((rcContourSet *) contourSet);
}

int recast_buildContours(struct recast_compactHeightfield *chf,
			const float maxError, const int maxEdgeLen, struct recast_contourSet *cset)
{
	INIT_SCTX();
	return rcBuildContours(sctx, *(rcCompactHeightfield *) chf, maxError, maxEdgeLen, *(rcContourSet *) cset);
}

struct recast_polyMesh *recast_newPolyMesh(void)
{
	return (recast_polyMesh *) rcAllocPolyMesh();
}

void recast_destroyPolyMesh(struct recast_polyMesh *polyMesh)
{
	rcFreePolyMesh((rcPolyMesh *) polyMesh);
}

int recast_buildPolyMesh(struct recast_contourSet *cset, int nvp, struct recast_polyMesh *mesh)
{
	INIT_SCTX();
	return rcBuildPolyMesh(sctx, *(rcContourSet *) cset, nvp, * (rcPolyMesh *) mesh);
}

unsigned short *recast_polyMeshGetVerts(struct recast_polyMesh *mesh, int *nverts)
{
	rcPolyMesh *pmesh = (rcPolyMesh *)mesh;

	if (nverts)
		*nverts = pmesh->nverts;

	return pmesh->verts;
}

void recast_polyMeshGetBoundbox(struct recast_polyMesh *mesh, float *bmin, float *bmax)
{
	rcPolyMesh *pmesh = (rcPolyMesh *)mesh;

	if (bmin) {
		bmin[0] = pmesh->bmin[0];
		bmin[1] = pmesh->bmin[1];
		bmin[2] = pmesh->bmin[2];
	}

	if (bmax) {
		bmax[0] = pmesh->bmax[0];
		bmax[1] = pmesh->bmax[1];
		bmax[2] = pmesh->bmax[2];
	}
}

void recast_polyMeshGetCell(struct recast_polyMesh *mesh, float *cs, float *ch)
{
	rcPolyMesh *pmesh = (rcPolyMesh *)mesh;

	if (cs)
		*cs = pmesh->cs;

	if (ch)
		*ch = pmesh->ch;
}

unsigned short *recast_polyMeshGetPolys(struct recast_polyMesh *mesh, int *npolys, int *nvp)
{
	rcPolyMesh *pmesh = (rcPolyMesh *)mesh;

	if (npolys)
		*npolys = pmesh->npolys;

	if (nvp)
		*nvp = pmesh->nvp;

	return pmesh->polys;
}

struct recast_polyMeshDetail *recast_newPolyMeshDetail(void)
{
	return (struct recast_polyMeshDetail *) rcAllocPolyMeshDetail();
}

void recast_destroyPolyMeshDetail(struct recast_polyMeshDetail *polyMeshDetail)
{
	rcFreePolyMeshDetail((rcPolyMeshDetail *) polyMeshDetail);
}

int recast_buildPolyMeshDetail(const struct recast_polyMesh *mesh, const struct recast_compactHeightfield *chf,
			const float sampleDist, const float sampleMaxError, struct recast_polyMeshDetail *dmesh)
{
	INIT_SCTX();
	return rcBuildPolyMeshDetail(sctx, *(rcPolyMesh *) mesh, *(rcCompactHeightfield *) chf,
			sampleDist, sampleMaxError, *(rcPolyMeshDetail *) dmesh);
}

float *recast_polyMeshDetailGetVerts(struct recast_polyMeshDetail *mesh, int *nverts)
{
	rcPolyMeshDetail *dmesh = (rcPolyMeshDetail *)mesh;

	if (nverts)
		*nverts = dmesh->nverts;

	return dmesh->verts;
}

unsigned char *recast_polyMeshDetailGetTris(struct recast_polyMeshDetail *mesh, int *ntris)
{
	rcPolyMeshDetail *dmesh = (rcPolyMeshDetail *)mesh;

	if (ntris)
		*ntris = dmesh->ntris;

	return dmesh->tris;
}

unsigned int *recast_polyMeshDetailGetMeshes(struct recast_polyMeshDetail *mesh, int *nmeshes)
{
	rcPolyMeshDetail *dmesh = (rcPolyMeshDetail *)mesh;

	if (nmeshes)
		*nmeshes = dmesh->nmeshes;

	return dmesh->meshes;
}

