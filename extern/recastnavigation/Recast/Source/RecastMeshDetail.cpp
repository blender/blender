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

#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Recast.h"
#include "RecastLog.h"
#include "RecastTimer.h"


struct rcHeightPatch
{
	inline rcHeightPatch() : data(0) {}
	inline ~rcHeightPatch() { delete [] data; }
	unsigned short* data;
	int xmin, ymin, width, height;
};


static int circumCircle(const float xp, const float yp,
						const float x1, const float y1,
						const float x2, const float y2,
						const float x3, const float y3,
						float& xc, float& yc, float& rsqr)
{
	static const float EPSILON = 1e-6f;
	
	const float fabsy1y2 = rcAbs(y1-y2);
	const float fabsy2y3 = rcAbs(y2-y3);
	
	/* Check for coincident points */
	if (fabsy1y2 < EPSILON && fabsy2y3 < EPSILON)
		return 0;
	
	if (fabsy1y2 < EPSILON)
	{
		const float m2 = - (x3-x2) / (y3-y2);
		const float mx2 = (x2 + x3) / 2.0f;
		const float my2 = (y2 + y3) / 2.0f;
		xc = (x2 + x1) / 2.0f;
		yc = m2 * (xc - mx2) + my2;
	}
	else if (fabsy2y3 < EPSILON)
	{
		const float m1 = - (x2-x1) / (y2-y1);
		const float mx1 = (x1 + x2) / 2.0f;
		const float my1 = (y1 + y2) / 2.0f;
		xc = (x3 + x2) / 2.0f;
		yc = m1 * (xc - mx1) + my1;
	}
	else
	{
		const float m1 = - (x2-x1) / (y2-y1);
		const float m2 = - (x3-x2) / (y3-y2);
		const float mx1 = (x1 + x2) / 2.0f;
		const float mx2 = (x2 + x3) / 2.0f;
		const float my1 = (y1 + y2) / 2.0f;
		const float my2 = (y2 + y3) / 2.0f;
		xc = (m1 * mx1 - m2 * mx2 + my2 - my1) / (m1 - m2);
		if (fabsy1y2 > fabsy2y3)
			yc = m1 * (xc - mx1) + my1;
		else
			yc = m2 * (xc - mx2) + my2;
	}
	
	float dx,dy;
	
	dx = x2 - xc;
	dy = y2 - yc;
	rsqr = dx*dx + dy*dy;
	
	dx = xp - xc;
	dy = yp - yc;
	const float drsqr = dx*dx + dy*dy;
	
	return (drsqr <= rsqr) ? 1 : 0;
}

static int ptcmp(void* up, const void *v1, const void *v2)
{
	const float* verts = (const float*)up;
	const float* p1 = &verts[(*(const int*)v1)*3];
	const float* p2 = &verts[(*(const int*)v2)*3];
	if (p1[0] < p2[0])
		return -1;
	else if (p1[0] > p2[0])
		return 1;
	else
		return 0;
}

// Based on Paul Bourke's triangulate.c
//  http://astronomy.swin.edu.au/~pbourke/terrain/triangulate/triangulate.c
static void delaunay(const int nv, float *verts, rcIntArray& idx, rcIntArray& tris, rcIntArray& edges)
{
	// Sort vertices
	idx.resize(nv);
	for (int i = 0; i < nv; ++i)
		idx[i] = i;
#ifdef WIN32
	qsort_s(&idx[0], idx.size(), sizeof(int), ptcmp, verts);
#else
	qsort_r(&idx[0], idx.size(), sizeof(int), verts, ptcmp);
#endif

	// Find the maximum and minimum vertex bounds.
	// This is to allow calculation of the bounding triangle
	float xmin = verts[0];
	float ymin = verts[2];
	float xmax = xmin;
	float ymax = ymin;
	for (int i = 1; i < nv; ++i)
	{
		xmin = rcMin(xmin, verts[i*3+0]);
		xmax = rcMax(xmax, verts[i*3+0]);
		ymin = rcMin(ymin, verts[i*3+2]);
		ymax = rcMax(ymax, verts[i*3+2]);
	}
	float dx = xmax - xmin;
	float dy = ymax - ymin;
	float dmax = (dx > dy) ? dx : dy;
	float xmid = (xmax + xmin) / 2.0f;
	float ymid = (ymax + ymin) / 2.0f;
	
	// Set up the supertriangle
	// This is a triangle which encompasses all the sample points.
	// The supertriangle coordinates are added to the end of the
	// vertex list. The supertriangle is the first triangle in
	// the triangle list.
	float sv[3*3];
	
	sv[0] = xmid - 20 * dmax;
	sv[1] = 0;
	sv[2] = ymid - dmax;
	
	sv[3] = xmid;
	sv[4] = 0;
	sv[5] = ymid + 20 * dmax;
	
	sv[6] = xmid + 20 * dmax;
	sv[7] = 0;
	sv[8] = ymid - dmax;
	
	tris.push(-3);
	tris.push(-2);
	tris.push(-1);
	tris.push(0); // not completed
	
	for (int i = 0; i < nv; ++i)
	{
		const float xp = verts[idx[i]*3+0];
		const float yp = verts[idx[i]*3+2];
		
		edges.resize(0);
		
		// Set up the edge buffer.
		// If the point (xp,yp) lies inside the circumcircle then the
		// three edges of that triangle are added to the edge buffer
		// and that triangle is removed.
		for (int j = 0; j < tris.size()/4; ++j)
		{
			int* t = &tris[j*4];
			if (t[3]) // completed?
				continue;
			const float* v1 = t[0] < 0 ? &sv[(t[0]+3)*3] : &verts[idx[t[0]]*3];
			const float* v2 = t[1] < 0 ? &sv[(t[1]+3)*3] : &verts[idx[t[1]]*3];
			const float* v3 = t[2] < 0 ? &sv[(t[2]+3)*3] : &verts[idx[t[2]]*3];
			float xc,yc,rsqr;
			int inside = circumCircle(xp,yp, v1[0],v1[2], v2[0],v2[2], v3[0],v3[2], xc,yc,rsqr);
			if (xc < xp && rcSqr(xp-xc) > rsqr)
				t[3] = 1;
			if (inside)
			{
				// Collect triangle edges.
				edges.push(t[0]);
				edges.push(t[1]);
				edges.push(t[1]);
				edges.push(t[2]);
				edges.push(t[2]);
				edges.push(t[0]);
				// Remove triangle j.
				t[0] = tris[tris.size()-4];
				t[1] = tris[tris.size()-3];
				t[2] = tris[tris.size()-2];
				t[3] = tris[tris.size()-1];
				tris.resize(tris.size()-4);
				j--;
			}
		}
		
		// Remove duplicate edges.
		const int ne = edges.size()/2;
		for (int j = 0; j < ne-1; ++j)
		{
			for (int k = j+1; k < ne; ++k)
			{
				// Dupe?, make null.
				if ((edges[j*2+0] == edges[k*2+1]) && (edges[j*2+1] == edges[k*2+0]))
				{
					edges[j*2+0] = 0;
					edges[j*2+1] = 0;
					edges[k*2+0] = 0;
					edges[k*2+1] = 0;
				}
			}
		}
		
		// Form new triangles for the current point
		// Skipping over any null.
		// All edges are arranged in clockwise order.
		for (int j = 0; j < ne; ++j)
		{
			if (edges[j*2+0] == edges[j*2+1]) continue;
			tris.push(edges[j*2+0]);
			tris.push(edges[j*2+1]);
			tris.push(i);
			tris.push(0); // not completed
		}
	}
	
	// Remove triangles with supertriangle vertices
	// These are triangles which have a vertex number greater than nv
	for (int i = 0; i < tris.size()/4; ++i)
	{
		int* t = &tris[i*4];
		if (t[0] < 0 || t[1] < 0 || t[2] < 0)
		{
			t[0] = tris[tris.size()-4];
			t[1] = tris[tris.size()-3];
			t[2] = tris[tris.size()-2];
			t[3] = tris[tris.size()-1];
			tris.resize(tris.size()-4);
			i--;
		}
	}
	// Triangle vertices are pointing to sorted vertices, remap indices.
	for (int i = 0; i < tris.size(); ++i)
		tris[i] = idx[tris[i]];
}

inline float vdot2(const float* a, const float* b)
{
	return a[0]*b[0] + a[2]*b[2];
}

static float distPtTri(const float* p, const float* a, const float* b, const float* c)
{
	float v0[3], v1[3], v2[3];
	vsub(v0, c,a);
	vsub(v1, b,a);
	vsub(v2, p,a);

	const float dot00 = vdot2(v0, v0);
	const float dot01 = vdot2(v0, v1);
	const float dot02 = vdot2(v0, v2);
	const float dot11 = vdot2(v1, v1);
	const float dot12 = vdot2(v1, v2);
	
	// Compute barycentric coordinates
	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	
	// If point lies inside the triangle, return interpolated y-coord.
	static const float EPS = 1e-4f;
	if (u >= -EPS && v >= -EPS && (u+v) <= 1+EPS)
	{
		float y = a[1] + v0[1]*u + v1[1]*v;
		return fabsf(y-p[1]);
	}
	return FLT_MAX;
}

static float distancePtSeg(const float* pt, const float* p, const float* q)
{
	float pqx = q[0] - p[0];
	float pqy = q[1] - p[1];
	float pqz = q[2] - p[2];
	float dx = pt[0] - p[0];
	float dy = pt[1] - p[1];
	float dz = pt[2] - p[2];
	float d = pqx*pqx + pqy*pqy + pqz*pqz;
	float t = pqx*dx + pqy*dy + pqz*dz;
	if (d > 0)
		t /= d;
	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	
	dx = p[0] + t*pqx - pt[0];
	dy = p[1] + t*pqy - pt[1];
	dz = p[2] + t*pqz - pt[2];
	
	return dx*dx + dy*dy + dz*dz;
}

static float distancePtSeg2d(const float* pt, const float* p, const float* q)
{
	float pqx = q[0] - p[0];
	float pqz = q[2] - p[2];
	float dx = pt[0] - p[0];
	float dz = pt[2] - p[2];
	float d = pqx*pqx + pqz*pqz;
	float t = pqx*dx + pqz*dz;
	if (d > 0)
		t /= d;
	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	
	dx = p[0] + t*pqx - pt[0];
	dz = p[2] + t*pqz - pt[2];
	
	return dx*dx + dz*dz;
}

static float distToTriMesh(const float* p, const float* verts, int nverts, const int* tris, int ntris)
{
	float dmin = FLT_MAX;
	for (int i = 0; i < ntris; ++i)
	{
		const float* va = &verts[tris[i*4+0]*3];
		const float* vb = &verts[tris[i*4+1]*3];
		const float* vc = &verts[tris[i*4+2]*3];
		float d = distPtTri(p, va,vb,vc);
		if (d < dmin)
			dmin = d;
	}
	if (dmin == FLT_MAX) return -1;
	return dmin;
}

static float distToPoly(int nvert, const float* verts, const float* p)
{

	float dmin = FLT_MAX;
	int i, j, c = 0;
	for (i = 0, j = nvert-1; i < nvert; j = i++)
	{
		const float* vi = &verts[i*3];
		const float* vj = &verts[j*3];
		if (((vi[2] > p[2]) != (vj[2] > p[2])) &&
			(p[0] < (vj[0]-vi[0]) * (p[2]-vi[2]) / (vj[2]-vi[2]) + vi[0]) )
			c = !c;
		dmin = rcMin(dmin, distancePtSeg2d(p, vj, vi));
	}
	return c ? -dmin : dmin;
}


static unsigned short getHeight(const float* pos, const float* bmin, const float ics, const rcHeightPatch& hp)
{
	int ix = (int)floorf((pos[0]-bmin[0])*ics + 0.01f);
	int iz = (int)floorf((pos[2]-bmin[2])*ics + 0.01f);
	ix = rcClamp(ix-hp.xmin, 0, hp.width);
	iz = rcClamp(iz-hp.ymin, 0, hp.height);
	unsigned short h = hp.data[ix+iz*hp.width];
	return h;
}

static bool buildPolyDetail(const float* in, const int nin, unsigned short reg,
							const float sampleDist, const float sampleMaxError,
							const rcCompactHeightfield& chf, const rcHeightPatch& hp,
							float* verts, int& nverts, rcIntArray& tris,
							rcIntArray& edges, rcIntArray& idx, rcIntArray& samples)
{
	static const int MAX_VERTS = 256;
	static const int MAX_EDGE = 64;
	float edge[(MAX_EDGE+1)*3];

	nverts = 0;

	for (int i = 0; i < nin; ++i)
		vcopy(&verts[i*3], &in[i*3]);
	nverts = nin;
	
	const float ics = 1.0f/chf.cs;
	
	// Tesselate outlines.
	// This is done in separate pass in order to ensure
	// seamless height values across the ply boundaries.
	if (sampleDist > 0)
	{
		for (int i = 0, j = nin-1; i < nin; j=i++)
		{
			const float* vj = &in[j*3];
			const float* vi = &in[i*3];
			// Make sure the segments are always handled in same order
			// using lexological sort or else there will be seams.
			if (fabsf(vj[0]-vi[0]) < 1e-6f)
			{
				if (vj[2] > vi[2])
					rcSwap(vj,vi);
			}
			else
			{
				if (vj[0] > vi[0])
					rcSwap(vj,vi);
			}
			// Create samples along the edge.
			float dx = vi[0] - vj[0];
			float dy = vi[1] - vj[1];
			float dz = vi[2] - vj[2];
			float d = sqrtf(dx*dx + dz*dz);
			int nn = 1 + (int)floorf(d/sampleDist);
			if (nn > MAX_EDGE) nn = MAX_EDGE;
			if (nverts+nn >= MAX_VERTS)
				nn = MAX_VERTS-1-nverts;
			for (int k = 0; k <= nn; ++k)
			{
				float u = (float)k/(float)nn;
				float* pos = &edge[k*3];
				pos[0] = vj[0] + dx*u;
				pos[1] = vj[1] + dy*u;
				pos[2] = vj[2] + dz*u;
				pos[1] = chf.bmin[1] + getHeight(pos, chf.bmin, ics, hp)*chf.ch;
			}
			// Simplify samples.
			int idx[MAX_EDGE] = {0,nn};
			int nidx = 2;
			for (int k = 0; k < nidx-1; )
			{
				const int a = idx[k];
				const int b = idx[k+1];
				const float* va = &edge[a*3];
				const float* vb = &edge[b*3];
				// Find maximum deviation along the segment.
				float maxd = 0;
				int maxi = -1;
				for (int m = a+1; m < b; ++m)
				{
					float d = distancePtSeg(&edge[m*3],va,vb);
					if (d > maxd)
					{
						maxd = d;
						maxi = m;
					}
				}
				// If the max deviation is larger than accepted error,
				// add new point, else continue to next segment.
				if (maxi != -1 && maxd > rcSqr(sampleMaxError))
				{
					for (int m = nidx; m > k; --m)
						idx[m] = idx[m-1];
					idx[k+1] = maxi;
					nidx++;
				}
				else
				{
					++k;
				}
			}
			// Add new vertices.
			for (int k = 1; k < nidx-1; ++k)
			{
				vcopy(&verts[nverts*3], &edge[idx[k]*3]);
				nverts++;
			}
		}
	}
	
	// Tesselate the base mesh.
	edges.resize(0);
	tris.resize(0);
	idx.resize(0);
	delaunay(nverts, verts, idx, tris, edges);

	if (sampleDist > 0)
	{
		// Create sample locations in a grid.
		float bmin[3], bmax[3];
		vcopy(bmin, in);
		vcopy(bmax, in);
		for (int i = 1; i < nin; ++i)
		{
			vmin(bmin, &in[i*3]);
			vmax(bmax, &in[i*3]);
		}
		int x0 = (int)floorf(bmin[0]/sampleDist);
		int x1 = (int)ceilf(bmax[0]/sampleDist);
		int z0 = (int)floorf(bmin[2]/sampleDist);
		int z1 = (int)ceilf(bmax[2]/sampleDist);
		samples.resize(0);
		for (int z = z0; z < z1; ++z)
		{
			for (int x = x0; x < x1; ++x)
			{
				float pt[3];
				pt[0] = x*sampleDist;
				pt[2] = z*sampleDist;
				// Make sure the samples are not too close to the edges.
				if (distToPoly(nin,in,pt) > -sampleDist/2) continue;
				samples.push(x);
				samples.push(getHeight(pt, chf.bmin, ics, hp));
				samples.push(z);
			}
		}
				
		// Add the samples starting from the one that has the most
		// error. The procedure stops when all samples are added
		// or when the max error is within treshold.
		const int nsamples = samples.size()/3;
		for (int iter = 0; iter < nsamples; ++iter)
		{
			// Find sample with most error.
			float bestpt[3];
			float bestd = 0;
			for (int i = 0; i < nsamples; ++i)
			{
				float pt[3];
				pt[0] = samples[i*3+0]*sampleDist;
				pt[1] = chf.bmin[1] + samples[i*3+1]*chf.ch;
				pt[2] = samples[i*3+2]*sampleDist;
				float d = distToTriMesh(pt, verts, nverts, &tris[0], tris.size()/4);
				if (d < 0) continue; // did not hit the mesh.
				if (d > bestd)
				{
					bestd = d;
					vcopy(bestpt,pt);
				}
			}
			// If the max error is within accepted threshold, stop tesselating.
			if (bestd <= sampleMaxError)
				break;

			// Add the new sample point.
			vcopy(&verts[nverts*3],bestpt);
			nverts++;
			
			// Create new triangulation.
			// TODO: Incremental add instead of full rebuild.
			edges.resize(0);
			tris.resize(0);
			idx.resize(0);
			delaunay(nverts, verts, idx, tris, edges);

			if (nverts >= MAX_VERTS)
				break;
		}
	}

	return true;
}

static void getHeightData(const rcCompactHeightfield& chf,
						  const unsigned short* poly, const int npoly,
						  const unsigned short* verts,
						  rcHeightPatch& hp, rcIntArray& stack)
{
	// Floodfill the heightfield to get 2D height data,
	// starting at vertex locations as seeds.
	
	memset(hp.data, 0xff, sizeof(unsigned short)*hp.width*hp.height);

	stack.resize(0);
	
	// Use poly vertices as seed points for the flood fill.
	for (int j = 0; j < npoly; ++j)
	{
		const int ax = (int)verts[poly[j]*3+0];
		const int ay = (int)verts[poly[j]*3+1];
		const int az = (int)verts[poly[j]*3+2];
		if (ax < hp.xmin || ax >= hp.xmin+hp.width ||
			az < hp.ymin || az >= hp.ymin+hp.height)
			continue;
			
		const rcCompactCell& c = chf.cells[ax+az*chf.width];
		int dmin = 0xffff;
		int ai = -1;
		for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
		{
			const rcCompactSpan& s = chf.spans[i];
			int d = rcAbs(ay - (int)s.y);
			if (d < dmin)
			{
				ai = i;
				dmin = d;
			}
		}
		if (ai != -1)
		{
			stack.push(ax);
			stack.push(az);
			stack.push(ai);
		}
	}

	while (stack.size() > 0)
	{
		int ci = stack.pop();
		int cy = stack.pop();
		int cx = stack.pop();

		// Skip already visited locations.
		int idx = cx-hp.xmin+(cy-hp.ymin)*hp.width;
		if (hp.data[idx] != 0xffff)
			continue;
		
		const rcCompactSpan& cs = chf.spans[ci];
		hp.data[idx] = cs.y;
		
		for (int dir = 0; dir < 4; ++dir)
		{
			if (rcGetCon(cs, dir) == 0xf) continue;
			
			const int ax = cx + rcGetDirOffsetX(dir);
			const int ay = cy + rcGetDirOffsetY(dir);
		
			if (ax < hp.xmin || ax >= (hp.xmin+hp.width) ||
				ay < hp.ymin || ay >= (hp.ymin+hp.height))
				continue;

			if (hp.data[ax-hp.xmin+(ay-hp.ymin)*hp.width] != 0xffff)
				continue;

			const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(cs, dir);
			
			stack.push(ax);
			stack.push(ay);
			stack.push(ai);
		}
	}	
}

static unsigned char getEdgeFlags(const float* va, const float* vb,
								  const float* vpoly, const int npoly)
{
	// Return true if edge (va,vb) is part of the polygon.
	static const float thrSqr = rcSqr(0.001f);
	for (int i = 0, j = npoly-1; i < npoly; j=i++)
	{
		if (distancePtSeg2d(va, &vpoly[j*3], &vpoly[i*3]) < thrSqr && 
			distancePtSeg2d(vb, &vpoly[j*3], &vpoly[i*3]) < thrSqr)
			return 1;
	}
	return 0;
}

static unsigned char getTriFlags(const float* va, const float* vb, const float* vc,
								 const float* vpoly, const int npoly)
{
	unsigned char flags = 0;
	flags |= getEdgeFlags(va,vb,vpoly,npoly) << 0;
	flags |= getEdgeFlags(vb,vc,vpoly,npoly) << 2;
	flags |= getEdgeFlags(vc,va,vpoly,npoly) << 4;
	return flags;
}



bool rcBuildPolyMeshDetail(const rcPolyMesh& mesh, const rcCompactHeightfield& chf,
						   const float sampleDist, const float sampleMaxError,
						   rcPolyMeshDetail& dmesh)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	if (mesh.nverts == 0 || mesh.npolys == 0)
		return true;
	
	const int nvp = mesh.nvp;
	const float cs = mesh.cs;
	const float ch = mesh.ch;
	const float* orig = mesh.bmin;
	
	rcIntArray edges(64);
	rcIntArray tris(512);
	rcIntArray idx(512);
	rcIntArray stack(512);
	rcIntArray samples(512);
	float verts[256*3];
	float* poly = 0;
	int* bounds = 0;
	rcHeightPatch hp;
	int nPolyVerts = 0;
	int maxhw = 0, maxhh = 0;
	
	bounds = new int[mesh.npolys*4];
	if (!bounds)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'bounds' (%d).", mesh.npolys*4);
		goto failure;
	}
	poly = new float[nvp*3];
	if (!bounds)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'poly' (%d).", nvp*3);
		goto failure;
	}
	
	// Find max size for a polygon area.
	for (int i = 0; i < mesh.npolys; ++i)
	{
		const unsigned short* p = &mesh.polys[i*nvp*2];
		int& xmin = bounds[i*4+0];
		int& xmax = bounds[i*4+1];
		int& ymin = bounds[i*4+2];
		int& ymax = bounds[i*4+3];
		xmin = chf.width;
		xmax = 0;
		ymin = chf.height;
		ymax = 0;
		for (int j = 0; j < nvp; ++j)
		{
			if(p[j] == 0xffff) break;
			const unsigned short* v = &mesh.verts[p[j]*3];
			xmin = rcMin(xmin, (int)v[0]);
			xmax = rcMax(xmax, (int)v[0]);
			ymin = rcMin(ymin, (int)v[2]);
			ymax = rcMax(ymax, (int)v[2]);
			nPolyVerts++;
		}
		xmin = rcMax(0,xmin-1);
		xmax = rcMin(chf.width,xmax+1);
		ymin = rcMax(0,ymin-1);
		ymax = rcMin(chf.height,ymax+1);
		if (xmin >= xmax || ymin >= ymax) continue;
		maxhw = rcMax(maxhw, xmax-xmin);
		maxhh = rcMax(maxhh, ymax-ymin);
	}
	
	hp.data = new unsigned short[maxhw*maxhh];
	if (!hp.data)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'hp.data' (%d).", maxhw*maxhh);
		goto failure;
	}
		
	dmesh.nmeshes = mesh.npolys;
	dmesh.nverts = 0;
	dmesh.ntris = 0;
	dmesh.meshes = new unsigned short[dmesh.nmeshes*4];
	if (!dmesh.meshes)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'dmesh.meshes' (%d).", dmesh.nmeshes*4);
		goto failure;
	}

	int vcap = nPolyVerts+nPolyVerts/2;
	int tcap = vcap*2;

	dmesh.nverts = 0;
	dmesh.verts = new float[vcap*3];
	if (!dmesh.verts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'dmesh.verts' (%d).", vcap*3);
		goto failure;
	}
	dmesh.ntris = 0;
	dmesh.tris = new unsigned char[tcap*4];
	if (!dmesh.tris)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'dmesh.tris' (%d).", tcap*4);
		goto failure;
	}
	
	for (int i = 0; i < mesh.npolys; ++i)
	{
		const unsigned short* p = &mesh.polys[i*nvp*2];
		
		// Find polygon bounding box.
		int npoly = 0;
		for (int j = 0; j < nvp; ++j)
		{
			if(p[j] == 0xffff) break;
			const unsigned short* v = &mesh.verts[p[j]*3];
			poly[j*3+0] = orig[0] + v[0]*cs;
			poly[j*3+1] = orig[1] + v[1]*ch;
			poly[j*3+2] = orig[2] + v[2]*cs;
			npoly++;
		}
		
		// Get the height data from the area of the polygon.
		hp.xmin = bounds[i*4+0];
		hp.ymin = bounds[i*4+2];
		hp.width = bounds[i*4+1]-bounds[i*4+0];
		hp.height = bounds[i*4+3]-bounds[i*4+2];
		getHeightData(chf, p, npoly, mesh.verts, hp, stack);
		
		// Build detail mesh.
		int nverts = 0;
		if (!buildPolyDetail(poly, npoly, mesh.regs[i],
							 sampleDist, sampleMaxError,
							 chf, hp, verts, nverts, tris,
							 edges, idx, samples))
		{
			goto failure;
		}

		// Offset detail vertices, unnecassary?
		for (int j = 0; j < nverts; ++j)
			verts[j*3+1] += chf.ch;
	
		// Store detail submesh.
		const int ntris = tris.size()/4;
		
		dmesh.meshes[i*4+0] = dmesh.nverts;
		dmesh.meshes[i*4+1] = (unsigned short)nverts;
		dmesh.meshes[i*4+2] = dmesh.ntris;
		dmesh.meshes[i*4+3] = (unsigned short)ntris;
		
		// Store vertices, allocate more memory if necessary.
		if (dmesh.nverts+nverts > vcap)
		{
			while (dmesh.nverts+nverts > vcap)
				vcap += 256;
				
			float* newv = new float[vcap*3];
			if (!newv)
			{
				if (rcGetLog())
					rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'newv' (%d).", vcap*3);
				goto failure;
			}
			if (dmesh.nverts)
				memcpy(newv, dmesh.verts, sizeof(float)*3*dmesh.nverts);
			delete [] dmesh.verts;
			dmesh.verts = newv;
		}
		for (int j = 0; j < nverts; ++j)
		{
			dmesh.verts[dmesh.nverts*3+0] = verts[j*3+0];
			dmesh.verts[dmesh.nverts*3+1] = verts[j*3+1];
			dmesh.verts[dmesh.nverts*3+2] = verts[j*3+2];
			dmesh.nverts++;
		}
		
		// Store triangles, allocate more memory if necessary.
		if (dmesh.ntris+ntris > tcap)
		{
			while (dmesh.ntris+ntris > tcap)
				tcap += 256;
			unsigned char* newt = new unsigned char[tcap*4];
			if (!newt)
			{
				if (rcGetLog())
					rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'newt' (%d).", tcap*4);
				goto failure;
			}
			if (dmesh.ntris)
				memcpy(newt, dmesh.tris, sizeof(unsigned char)*4*dmesh.ntris);
			delete [] dmesh.tris;
			dmesh.tris = newt;
		}
		for (int j = 0; j < ntris; ++j)
		{
			const int* t = &tris[j*4];
			dmesh.tris[dmesh.ntris*4+0] = (unsigned char)t[0];
			dmesh.tris[dmesh.ntris*4+1] = (unsigned char)t[1];
			dmesh.tris[dmesh.ntris*4+2] = (unsigned char)t[2];
			dmesh.tris[dmesh.ntris*4+3] = getTriFlags(&verts[t[0]*3], &verts[t[1]*3], &verts[t[2]*3], poly, npoly);
			dmesh.ntris++;
		}
	}
	
	delete [] bounds;
	delete [] poly;
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
	if (rcGetBuildTimes())
		rcGetBuildTimes()->buildDetailMesh += rcGetDeltaTimeUsec(startTime, endTime);

	return true;

failure:

	delete [] bounds;
	delete [] poly;

	return false;
}

bool rcMergePolyMeshDetails(rcPolyMeshDetail** meshes, const int nmeshes, rcPolyMeshDetail& mesh)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	int maxVerts = 0;
	int maxTris = 0;
	int maxMeshes = 0;

	for (int i = 0; i < nmeshes; ++i)
	{
		if (!meshes[i]) continue;
		maxVerts += meshes[i]->nverts;
		maxTris += meshes[i]->ntris;
		maxMeshes += meshes[i]->nmeshes;
	}

	mesh.nmeshes = 0;
	mesh.meshes = new unsigned short[maxMeshes*4];
	if (!mesh.meshes)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'pmdtl.meshes' (%d).", maxMeshes*4);
		return false;
	}

	mesh.ntris = 0;
	mesh.tris = new unsigned char[maxTris*4];
	if (!mesh.tris)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'dmesh.tris' (%d).", maxTris*4);
		return false;
	}

	mesh.nverts = 0;
	mesh.verts = new float[maxVerts*3];
	if (!mesh.verts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMeshDetail: Out of memory 'dmesh.verts' (%d).", maxVerts*3);
		return false;
	}
	
	// Merge datas.
	for (int i = 0; i < nmeshes; ++i)
	{
		rcPolyMeshDetail* dm = meshes[i];
		if (!dm) continue;
		for (int j = 0; j < dm->nmeshes; ++j)
		{
			unsigned short* dst = &mesh.meshes[mesh.nmeshes*4];
			unsigned short* src = &dm->meshes[j*4];
			dst[0] = mesh.nverts+src[0];
			dst[1] = src[1];
			dst[2] = mesh.ntris+src[2];
			dst[3] = src[3];
			mesh.nmeshes++;
		}
			
		for (int k = 0; k < dm->nverts; ++k)
		{
			vcopy(&mesh.verts[mesh.nverts*3], &dm->verts[k*3]);
			mesh.nverts++;
		}
		for (int k = 0; k < dm->ntris; ++k)
		{
			mesh.tris[mesh.ntris*4+0] = dm->tris[k*4+0];
			mesh.tris[mesh.ntris*4+1] = dm->tris[k*4+1];
			mesh.tris[mesh.ntris*4+2] = dm->tris[k*4+2];
			mesh.tris[mesh.ntris*4+3] = dm->tris[k*4+3];
			mesh.ntris++;
		}
	}

	rcTimeVal endTime = rcGetPerformanceTimer();
	
	if (rcGetBuildTimes())
		rcGetBuildTimes()->mergePolyMeshDetail += rcGetDeltaTimeUsec(startTime, endTime);
	
	return true;
}

