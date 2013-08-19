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

#ifndef RECAST_C_API_H
#define RECAST_C_API_H

// for size_t
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct recast_polyMesh;
struct recast_polyMeshDetail;
struct recast_heightfield;
struct recast_compactHeightfield;
struct recast_contourSet;

enum recast_SpanFlags
{
	RECAST_WALKABLE = 0x01,
	RECAST_REACHABLE = 0x02
};

int recast_buildMeshAdjacency(unsigned short* polys, const int npolys,
			const int nverts, const int vertsPerPoly);

void recast_calcBounds(const float *verts, int nv, float *bmin, float *bmax);

void recast_calcGridSize(const float *bmin, const float *bmax, float cs, int *w, int *h);

struct recast_heightfield *recast_newHeightfield(void);

void recast_destroyHeightfield(struct recast_heightfield *heightfield);

int recast_createHeightfield(struct recast_heightfield *hf, int width, int height,
			const float *bmin, const float* bmax, float cs, float ch);

void recast_markWalkableTriangles(const float walkableSlopeAngle,const float *verts, int nv,
			const int *tris, int nt, unsigned char *flags);

void recast_rasterizeTriangles(const float *verts, int nv, const int *tris,
			const unsigned char *flags, int nt, struct recast_heightfield *solid);

void recast_filterLedgeSpans(const int walkableHeight, const int walkableClimb,
			struct recast_heightfield *solid);

void recast_filterWalkableLowHeightSpans(int walkableHeight, struct recast_heightfield *solid);

void recast_filterLowHangingWalkableObstacles(const int walkableClimb, struct recast_heightfield *solid);

struct recast_compactHeightfield *recast_newCompactHeightfield(void);

void recast_destroyCompactHeightfield(struct recast_compactHeightfield *compactHeightfield);

int recast_buildCompactHeightfield(const int walkableHeight, const int walkableClimb,
			struct recast_heightfield *hf, struct recast_compactHeightfield *chf);

int recast_erodeWalkableArea(int radius, struct recast_compactHeightfield *chf);

int recast_buildDistanceField(struct recast_compactHeightfield *chf);

int recast_buildRegions(struct recast_compactHeightfield *chf, int borderSize,
	int minRegionSize, int mergeRegionSize);

/* Contour set */

struct recast_contourSet *recast_newContourSet(void);

void recast_destroyContourSet(struct recast_contourSet *contourSet);

int recast_buildContours(struct recast_compactHeightfield *chf,
			const float maxError, const int maxEdgeLen, struct recast_contourSet *cset);

/* Poly mesh */

struct recast_polyMesh *recast_newPolyMesh(void);

void recast_destroyPolyMesh(struct recast_polyMesh *polyMesh);

int recast_buildPolyMesh(struct recast_contourSet *cset, int nvp, struct recast_polyMesh *mesh);

unsigned short *recast_polyMeshGetVerts(struct recast_polyMesh *mesh, int *nverts);

void recast_polyMeshGetBoundbox(struct recast_polyMesh *mesh, float *bmin, float *bmax);

void recast_polyMeshGetCell(struct recast_polyMesh *mesh, float *cs, float *ch);

unsigned short *recast_polyMeshGetPolys(struct recast_polyMesh *mesh, int *npolys, int *nvp);

/* Poly mesh detail */

struct recast_polyMeshDetail *recast_newPolyMeshDetail(void);

void recast_destroyPolyMeshDetail(struct recast_polyMeshDetail *polyMeshDetail);

int recast_buildPolyMeshDetail(const struct recast_polyMesh *mesh, const struct recast_compactHeightfield *chf,
			const float sampleDist, const float sampleMaxError, struct recast_polyMeshDetail *dmesh);

float *recast_polyMeshDetailGetVerts(struct recast_polyMeshDetail *mesh, int *nverts);

unsigned char *recast_polyMeshDetailGetTris(struct recast_polyMeshDetail *mesh, int *ntris);

unsigned int *recast_polyMeshDetailGetMeshes(struct recast_polyMeshDetail *mesh, int *nmeshes);

#ifdef __cplusplus
}
#endif

#endif // RECAST_C_API_H
