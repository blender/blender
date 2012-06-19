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

//  qsort based on FreeBSD source (libkern\qsort.c)
typedef int	cmp_t(void *, const void *, const void *);
static inline char	*med3(char *, char *, char *, cmp_t *, void *);
static inline void	 swapfunc(char *, char *, int, int);

#define min(a, b)	(a) < (b) ? a : b
#define swapcode(TYPE, parmi, parmj, n)		\
{											\
	long i = (n) / sizeof(TYPE); 			\
	TYPE *pi = (TYPE *) (parmi); 			\
	TYPE *pj = (TYPE *) (parmj); 			\
	do { 									\
		TYPE	t = *pi;					\
		*pi++ = *pj;						\
		*pj++ = t;							\
	} while (--i > 0);						\
}
#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static inline void swapfunc(char* a, char* b, int n, int swaptype)
{
	if(swaptype <= 1)
		swapcode(long, a, b, n)
	else
	swapcode(char, a, b, n)
}

#define swap(a, b)					\
	if (swaptype == 0) {			\
		long t = *(long *)(a);		\
		*(long *)(a) = *(long *)(b);\
		*(long *)(b) = t;			\
	} else							\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) 	if ((n) > 0) swapfunc(a, b, n, swaptype)
#define	CMP(t, x, y) (cmp((t), (x), (y)))

static inline char * med3(char *a, char *b, char *c, cmp_t *cmp, void *thunk)
{
	return CMP(thunk, a, b) < 0 ?
		(CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a ))
		:(CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c ));
}

void recast_qsort(void *a, size_t n, size_t es, void *thunk, cmp_t *cmp)
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int d, r, swaptype, swap_cnt;

loop:	
	SWAPINIT(a, es);
	swap_cnt = 0;
	if (n < 7) {
		for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
			for (pl = pm; 
				pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
				pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *)a + (n / 2) * es;
	if (n > 7) {
		pl = (char *)a;
		pn = (char *)a + (n - 1) * es;
		if (n > 40) {
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
			pm = med3(pm - d, pm, pm + d, cmp, thunk);
			pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
		}
		pm = med3(pl, pm, pn, cmp, thunk);
	}
	swap((char *)a, pm);
	pa = pb = (char *)a + es;

	pc = pd = (char *)a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (r = CMP(thunk, pb, a)) <= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (r = CMP(thunk, pc, a)) >= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		swap_cnt = 1;
		pb += es;
		pc -= es;
	}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
		for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
			for (pl = pm; 
				pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
				pl -= es)
				swap(pl, pl - es);
		return;
	}

	pn = (char *)a + n * es;
	r = min(pa - (char *)a, pb - pa);
	vecswap((char *)a, pb - r, r);
	r = min(pd - pc, pn - pd - es);
	vecswap(pb, pn - r, r);
	if ((r = pb - pa) > es)
		recast_qsort(a, r / es, es, thunk, cmp);
	if ((r = pd - pc) > es) {
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		n = r / es;
		goto loop;
	}
}

