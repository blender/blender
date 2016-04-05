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
			const int *tris, int nt, unsigned char *areas)
{
	INIT_SCTX();
	rcMarkWalkableTriangles(sctx, walkableSlopeAngle, verts, nv, tris, nt, areas);
}

void recast_clearUnwalkableTriangles(const float walkableSlopeAngle, const float* verts, int nv,
			const int* tris, int nt, unsigned char* areas)
{
	INIT_SCTX();
	rcClearUnwalkableTriangles(sctx, walkableSlopeAngle, verts, nv, tris, nt, areas);
}

int recast_addSpan(struct recast_heightfield *hf, const int x, const int y,
			const unsigned short smin, const unsigned short smax,
			const unsigned char area, const int flagMergeThr)
{
	INIT_SCTX();
	return rcAddSpan(sctx, *(rcHeightfield *) hf, x, y, smin, smax, area, flagMergeThr);
}

int recast_rasterizeTriangle(const float *v0, const float *v1, const float *v2,
			const unsigned char area, struct recast_heightfield *solid,
			const int flagMergeThr)
{
	INIT_SCTX();
	return rcRasterizeTriangle(sctx, v0, v1, v2, area, *(rcHeightfield *) solid, flagMergeThr);
}

int recast_rasterizeTriangles(const float *verts, const int nv, const int *tris,
			const unsigned char *areas, const int nt, struct recast_heightfield *solid,
			const int flagMergeThr)
{
	INIT_SCTX();
	return rcRasterizeTriangles(sctx, verts, nv, tris, areas, nt, *(rcHeightfield *) solid, flagMergeThr);
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

int recast_getHeightFieldSpanCount(struct recast_heightfield *hf)
{
	INIT_SCTX();
	return rcGetHeightFieldSpanCount(sctx, *(rcHeightfield *) hf);
}

struct recast_heightfieldLayerSet *recast_newHeightfieldLayerSet(void)
{
	return (struct recast_heightfieldLayerSet *) rcAllocHeightfieldLayerSet();
}

void recast_destroyHeightfieldLayerSet(struct recast_heightfieldLayerSet *lset)
{
	rcFreeHeightfieldLayerSet( (rcHeightfieldLayerSet *) lset);
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

int recast_medianFilterWalkableArea(struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	return rcMedianFilterWalkableArea(sctx, *(rcCompactHeightfield *) chf);
}

void recast_markBoxArea(const float *bmin, const float *bmax, unsigned char areaId,
			struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	rcMarkBoxArea(sctx, bmin, bmax, areaId, *(rcCompactHeightfield *) chf);
}

void recast_markConvexPolyArea(const float* verts, const int nverts,
			const float hmin, const float hmax, unsigned char areaId,
			struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	rcMarkConvexPolyArea(sctx, verts, nverts, hmin, hmax, areaId, *(rcCompactHeightfield *) chf);
}

int recast_offsetPoly(const float* verts, const int nverts,
			const float offset, float *outVerts, const int maxOutVerts)
{
	return rcOffsetPoly(verts, nverts, offset, outVerts, maxOutVerts);
}

void recast_markCylinderArea(const float* pos, const float r, const float h,
			unsigned char areaId, struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	rcMarkCylinderArea(sctx, pos, r, h, areaId, *(rcCompactHeightfield *) chf);
}

int recast_buildDistanceField(struct recast_compactHeightfield *chf)
{
	INIT_SCTX();
	return rcBuildDistanceField(sctx, *(rcCompactHeightfield *) chf);
}

int recast_buildRegions(struct recast_compactHeightfield *chf,
			const int borderSize, const int minRegionArea, const int mergeRegionArea)
{
	INIT_SCTX();
	return rcBuildRegions(sctx, *(rcCompactHeightfield *) chf, borderSize,
				minRegionArea, mergeRegionArea);
}

int recast_buildLayerRegions(struct recast_compactHeightfield *chf,
			const int borderSize, const int minRegionArea)
{
	INIT_SCTX();
	return rcBuildLayerRegions(sctx, *(rcCompactHeightfield *) chf, borderSize,
				minRegionArea);
}

int recast_buildRegionsMonotone(struct recast_compactHeightfield *chf,
			const int borderSize, const int minRegionArea, const int mergeRegionArea)
{
	INIT_SCTX();
	return rcBuildRegionsMonotone(sctx, *(rcCompactHeightfield *) chf, borderSize,
				minRegionArea, mergeRegionArea);
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
			const float maxError, const int maxEdgeLen, struct recast_contourSet *cset,
			const int buildFlags)
{
	INIT_SCTX();
	return rcBuildContours(sctx, *(rcCompactHeightfield *) chf, maxError, maxEdgeLen, *(rcContourSet *) cset, buildFlags);
}

struct recast_polyMesh *recast_newPolyMesh(void)
{
	return (recast_polyMesh *) rcAllocPolyMesh();
}

void recast_destroyPolyMesh(struct recast_polyMesh *polyMesh)
{
	rcFreePolyMesh((rcPolyMesh *) polyMesh);
}

int recast_buildPolyMesh(struct recast_contourSet *cset, const int nvp, struct recast_polyMesh *mesh)
{
	INIT_SCTX();
	return rcBuildPolyMesh(sctx, *(rcContourSet *) cset, nvp, *(rcPolyMesh *) mesh);
}

int recast_mergePolyMeshes(struct recast_polyMesh **meshes, const int nmeshes, struct recast_polyMesh *mesh)
{
	INIT_SCTX();
	return rcMergePolyMeshes(sctx, (rcPolyMesh **) meshes, nmeshes, *(rcPolyMesh *) mesh);
}

int recast_copyPolyMesh(const struct recast_polyMesh *src, struct recast_polyMesh *dst)
{
	INIT_SCTX();
	return rcCopyPolyMesh(sctx, *(const rcPolyMesh *) src, *(rcPolyMesh *) dst);
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

int recast_mergePolyMeshDetails(struct recast_polyMeshDetail **meshes, const int nmeshes, struct recast_polyMeshDetail *mesh)
{
	INIT_SCTX();
	return rcMergePolyMeshDetails(sctx, (rcPolyMeshDetail **) meshes, nmeshes, *(rcPolyMeshDetail *) mesh);
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

