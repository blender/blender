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

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "Recast.h"
#include "RecastLog.h"
#include "RecastTimer.h"


struct rcEdge
{
	unsigned short vert[2];
	unsigned short polyEdge[2];
	unsigned short poly[2];
};

static bool buildMeshAdjacency(unsigned short* polys, const int npolys,
							   const int nverts, const int vertsPerPoly)
{
	// Based on code by Eric Lengyel from:
	// http://www.terathon.com/code/edges.php
	
	int maxEdgeCount = npolys*vertsPerPoly;
	unsigned short* firstEdge = new unsigned short[nverts + maxEdgeCount];
	if (!firstEdge)
		return false;
	unsigned short* nextEdge = firstEdge + nverts;
	int edgeCount = 0;
	
	rcEdge* edges = new rcEdge[maxEdgeCount];
	if (!edges)
		return false;
	
	for (int i = 0; i < nverts; i++)
		firstEdge[i] = 0xffff;
	
	// Invalida indices are marked as 0xffff, the following code
	// handles them just fine.
	
	for (int i = 0; i < npolys; ++i)
	{
		unsigned short* t = &polys[i*vertsPerPoly*2];
		for (int j = 0; j < vertsPerPoly; ++j)
		{
			unsigned short v0 = t[j];
			unsigned short v1 = (j+1 >= vertsPerPoly || t[j+1] == 0xffff) ? t[0] : t[j+1];
			if (v0 < v1)
			{
				rcEdge& edge = edges[edgeCount];
				edge.vert[0] = v0;
				edge.vert[1] = v1;
				edge.poly[0] = (unsigned short)i;
				edge.polyEdge[0] = (unsigned short)j;
				edge.poly[1] = (unsigned short)i;
				edge.polyEdge[1] = 0;
				// Insert edge
				nextEdge[edgeCount] = firstEdge[v0];
				firstEdge[v0] = edgeCount;
				edgeCount++;
			}
		}
	}
	
	for (int i = 0; i < npolys; ++i)
	{
		unsigned short* t = &polys[i*vertsPerPoly*2];
		for (int j = 0; j < vertsPerPoly; ++j)
		{
			unsigned short v0 = t[j];
			unsigned short v1 = (j+1 >= vertsPerPoly || t[j+1] == 0xffff) ? t[0] : t[j+1];
			if (v0 > v1)
			{
				for (unsigned short e = firstEdge[v1]; e != 0xffff; e = nextEdge[e])
				{
					rcEdge& edge = edges[e];
					if (edge.vert[1] == v0 && edge.poly[0] == edge.poly[1])
					{
						edge.poly[1] = (unsigned short)i;
						edge.polyEdge[1] = (unsigned short)j;
						break;
					}
				}
			}
		}
	}
	
	// Store adjacency
	for (int i = 0; i < edgeCount; ++i)
	{
		const rcEdge& e = edges[i];
		if (e.poly[0] != e.poly[1])
		{
			unsigned short* p0 = &polys[e.poly[0]*vertsPerPoly*2];
			unsigned short* p1 = &polys[e.poly[1]*vertsPerPoly*2];
			p0[vertsPerPoly + e.polyEdge[0]] = e.poly[1];
			p1[vertsPerPoly + e.polyEdge[1]] = e.poly[0];
		}
	}
	
	delete [] firstEdge;
	delete [] edges;
	
	return true;
}


static const int VERTEX_BUCKET_COUNT = (1<<12);

inline int computeVertexHash(int x, int y, int z)
{
	const unsigned int h1 = 0x8da6b343; // Large multiplicative constants;
	const unsigned int h2 = 0xd8163841; // here arbitrarily chosen primes
	const unsigned int h3 = 0xcb1ab31f;
	unsigned int n = h1 * x + h2 * y + h3 * z;
	return (int)(n & (VERTEX_BUCKET_COUNT-1));
}

static int addVertex(unsigned short x, unsigned short y, unsigned short z,
					 unsigned short* verts, int* firstVert, int* nextVert, int& nv)
{
	int bucket = computeVertexHash(x, 0, z);
	int i = firstVert[bucket];
	
	while (i != -1)
	{
		const unsigned short* v = &verts[i*3];
		if (v[0] == x && (rcAbs(v[1] - y) <= 2) && v[2] == z)
			return i;
		i = nextVert[i]; // next
	}
	
	// Could not find, create new.
	i = nv; nv++;
	unsigned short* v = &verts[i*3];
	v[0] = x;
	v[1] = y;
	v[2] = z;
	nextVert[i] = firstVert[bucket];
	firstVert[bucket] = i;
	
	return i;
}

inline int prev(int i, int n) { return i-1 >= 0 ? i-1 : n-1; }
inline int next(int i, int n) { return i+1 < n ? i+1 : 0; }

inline int area2(const int* a, const int* b, const int* c)
{
	return (b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]);
}

//	Exclusive or: true iff exactly one argument is true.
//	The arguments are negated to ensure that they are 0/1
//	values.  Then the bitwise Xor operator may apply.
//	(This idea is due to Michael Baldwin.)
inline bool xorb(bool x, bool y)
{
	return !x ^ !y;
}

// Returns true iff c is strictly to the left of the directed
// line through a to b.
inline bool left(const int* a, const int* b, const int* c)
{
	return area2(a, b, c) < 0;
}

inline bool leftOn(const int* a, const int* b, const int* c)
{
	return area2(a, b, c) <= 0;
}

inline bool collinear(const int* a, const int* b, const int* c)
{
	return area2(a, b, c) == 0;
}

//	Returns true iff ab properly intersects cd: they share
//	a point interior to both segments.  The properness of the
//	intersection is ensured by using strict leftness.
bool intersectProp(const int* a, const int* b, const int* c, const int* d)
{
	// Eliminate improper cases.
	if (collinear(a,b,c) || collinear(a,b,d) ||
		collinear(c,d,a) || collinear(c,d,b))
		return false;
	
	return xorb(left(a,b,c), left(a,b,d)) && xorb(left(c,d,a), left(c,d,b));
}

// Returns T iff (a,b,c) are collinear and point c lies 
// on the closed segement ab.
static bool between(const int* a, const int* b, const int* c)
{
	if (!collinear(a, b, c))
		return false;
	// If ab not vertical, check betweenness on x; else on y.
	if (a[0] != b[0])
		return	((a[0] <= c[0]) && (c[0] <= b[0])) || ((a[0] >= c[0]) && (c[0] >= b[0]));
	else
		return	((a[2] <= c[2]) && (c[2] <= b[2])) || ((a[2] >= c[2]) && (c[2] >= b[2]));
}

// Returns true iff segments ab and cd intersect, properly or improperly.
static bool intersect(const int* a, const int* b, const int* c, const int* d)
{
	if (intersectProp(a, b, c, d))
		return true;
	else if (between(a, b, c) || between(a, b, d) ||
			 between(c, d, a) || between(c, d, b))
		return true;
	else
		return false;
}

static bool vequal(const int* a, const int* b)
{
	return a[0] == b[0] && a[2] == b[2];
}

// Returns T iff (v_i, v_j) is a proper internal *or* external
// diagonal of P, *ignoring edges incident to v_i and v_j*.
static bool diagonalie(int i, int j, int n, const int* verts, int* indices)
{
	const int* d0 = &verts[(indices[i] & 0x0fffffff) * 4];
	const int* d1 = &verts[(indices[j] & 0x0fffffff) * 4];
	
	// For each edge (k,k+1) of P
	for (int k = 0; k < n; k++)
	{
		int k1 = next(k, n);
		// Skip edges incident to i or j
		if (!((k == i) || (k1 == i) || (k == j) || (k1 == j)))
		{
			const int* p0 = &verts[(indices[k] & 0x0fffffff) * 4];
			const int* p1 = &verts[(indices[k1] & 0x0fffffff) * 4];

			if (vequal(d0, p0) || vequal(d1, p0) || vequal(d0, p1) || vequal(d1, p1))
				continue;
			
			if (intersect(d0, d1, p0, p1))
				return false;
		}
	}
	return true;
}

// Returns true iff the diagonal (i,j) is strictly internal to the 
// polygon P in the neighborhood of the i endpoint.
static bool	inCone(int i, int j, int n, const int* verts, int* indices)
{
	const int* pi = &verts[(indices[i] & 0x0fffffff) * 4];
	const int* pj = &verts[(indices[j] & 0x0fffffff) * 4];
	const int* pi1 = &verts[(indices[next(i, n)] & 0x0fffffff) * 4];
	const int* pin1 = &verts[(indices[prev(i, n)] & 0x0fffffff) * 4];

	// If P[i] is a convex vertex [ i+1 left or on (i-1,i) ].
	if (leftOn(pin1, pi, pi1))
		return left(pi, pj, pin1) && left(pj, pi, pi1);
	// Assume (i-1,i,i+1) not collinear.
	// else P[i] is reflex.
	return !(leftOn(pi, pj, pi1) && leftOn(pj, pi, pin1));
}

// Returns T iff (v_i, v_j) is a proper internal
// diagonal of P.
static bool diagonal(int i, int j, int n, const int* verts, int* indices)
{
	return inCone(i, j, n, verts, indices) && diagonalie(i, j, n, verts, indices);
}

int triangulate(int n, const int* verts, int* indices, int* tris)
{
	int ntris = 0;
	int* dst = tris;
	
	// The last bit of the index is used to indicate if the vertex can be removed.
	for (int i = 0; i < n; i++)
	{
		int i1 = next(i, n);
		int i2 = next(i1, n);
		if (diagonal(i, i2, n, verts, indices))
			indices[i1] |= 0x80000000;
	}
	
	while (n > 3)
	{
		int minLen = -1;
		int mini = -1;
		for (int i = 0; i < n; i++)
		{
			int i1 = next(i, n);
			if (indices[i1] & 0x80000000)
			{
				const int* p0 = &verts[(indices[i] & 0x0fffffff) * 4];
				const int* p2 = &verts[(indices[next(i1, n)] & 0x0fffffff) * 4];
				
				int dx = p2[0] - p0[0];
				int dy = p2[2] - p0[2];
				int len = dx*dx + dy*dy;
				
				if (minLen < 0 || len < minLen)
				{
					minLen = len;
					mini = i;
				}
			}
		}
		
		if (mini == -1)
		{
			// Should not happen.
			if (rcGetLog())
				rcGetLog()->log(RC_LOG_WARNING, "triangulate: Failed to triangulate polygon.");
/*			printf("mini == -1 ntris=%d n=%d\n", ntris, n);
			for (int i = 0; i < n; i++)
			{
				printf("%d ", indices[i] & 0x0fffffff);
			}
			printf("\n");*/
			return -ntris;
		}
		
		int i = mini;
		int i1 = next(i, n);
		int i2 = next(i1, n);
		
		*dst++ = indices[i] & 0x0fffffff;
		*dst++ = indices[i1] & 0x0fffffff;
		*dst++ = indices[i2] & 0x0fffffff;
		ntris++;
		
		// Removes P[i1] by copying P[i+1]...P[n-1] left one index.
		n--;
		for (int k = i1; k < n; k++)
			indices[k] = indices[k+1];
		
		if (i1 >= n) i1 = 0;
		i = prev(i1,n);
		// Update diagonal flags.
		if (diagonal(prev(i, n), i1, n, verts, indices))
			indices[i] |= 0x80000000;
		else
			indices[i] &= 0x0fffffff;
		
		if (diagonal(i, next(i1, n), n, verts, indices))
			indices[i1] |= 0x80000000;
		else
			indices[i1] &= 0x0fffffff;
	}
	
	// Append the remaining triangle.
	*dst++ = indices[0] & 0x0fffffff;
	*dst++ = indices[1] & 0x0fffffff;
	*dst++ = indices[2] & 0x0fffffff;
	ntris++;
	
	return ntris;
}

static int countPolyVerts(const unsigned short* p, const int nvp)
{
	for (int i = 0; i < nvp; ++i)
		if (p[i] == 0xffff)
			return i;
	return nvp;
}

inline bool uleft(const unsigned short* a, const unsigned short* b, const unsigned short* c)
{
	return ((int)b[0] - (int)a[0]) * ((int)c[2] - (int)a[2]) -
		   ((int)c[0] - (int)a[0]) * ((int)b[2] - (int)a[2]) < 0;
}

static int getPolyMergeValue(unsigned short* pa, unsigned short* pb,
							 const unsigned short* verts, int& ea, int& eb,
							 const int nvp)
{
	const int na = countPolyVerts(pa, nvp);
	const int nb = countPolyVerts(pb, nvp);
	
	// If the merged polygon would be too big, do not merge.
	if (na+nb-2 > nvp)
		return -1;
	
	// Check if the polygons share an edge.
	ea = -1;
	eb = -1;
	
	for (int i = 0; i < na; ++i)
	{
		unsigned short va0 = pa[i];
		unsigned short va1 = pa[(i+1) % na];
		if (va0 > va1)
			rcSwap(va0, va1);
		for (int j = 0; j < nb; ++j)
		{
			unsigned short vb0 = pb[j];
			unsigned short vb1 = pb[(j+1) % nb];
			if (vb0 > vb1)
				rcSwap(vb0, vb1);
			if (va0 == vb0 && va1 == vb1)
			{
				ea = i;
				eb = j;
				break;
			}
		}
	}
	
	// No common edge, cannot merge.
	if (ea == -1 || eb == -1)
		return -1;
	
	// Check to see if the merged polygon would be convex.
	unsigned short va, vb, vc;
	
	va = pa[(ea+na-1) % na];
	vb = pa[ea];
	vc = pb[(eb+2) % nb];
	if (!uleft(&verts[va*3], &verts[vb*3], &verts[vc*3]))
		return -1;
	
	va = pb[(eb+nb-1) % nb];
	vb = pb[eb];
	vc = pa[(ea+2) % na];
	if (!uleft(&verts[va*3], &verts[vb*3], &verts[vc*3]))
		return -1;
	
	va = pa[ea];
	vb = pa[(ea+1)%na];
	
	int dx = (int)verts[va*3+0] - (int)verts[vb*3+0];
	int dy = (int)verts[va*3+2] - (int)verts[vb*3+2];
	
	return dx*dx + dy*dy;
}

static void mergePolys(unsigned short* pa, unsigned short* pb,
					   const unsigned short* verts, int ea, int eb,
					   unsigned short* tmp, const int nvp)
{
	const int na = countPolyVerts(pa, nvp);
	const int nb = countPolyVerts(pb, nvp);
	
	// Merge polygons.
	memset(tmp, 0xff, sizeof(unsigned short)*nvp);
	int n = 0;
	// Add pa
	for (int i = 0; i < na-1; ++i)
		tmp[n++] = pa[(ea+1+i) % na];
	// Add pb
	for (int i = 0; i < nb-1; ++i)
		tmp[n++] = pb[(eb+1+i) % nb];
	
	memcpy(pa, tmp, sizeof(unsigned short)*nvp);
}

static void pushFront(int v, int* arr, int& an)
{
	an++;
	for (int i = an-1; i > 0; --i) arr[i] = arr[i-1];
	arr[0] = v;
}

static void pushBack(int v, int* arr, int& an)
{
	arr[an] = v;
	an++;
}

static bool removeVertex(rcPolyMesh& mesh, const unsigned short rem, const int maxTris)
{
	static const int nvp = mesh.nvp;

	int* edges = 0;
	int nedges = 0;
	int* hole = 0;
	int nhole = 0;
	int* hreg = 0;
	int nhreg = 0;
	int* tris = 0;
	int* tverts = 0;
	int* thole = 0;
	unsigned short* polys = 0;
	unsigned short* pregs = 0;
	int npolys = 0;

	// Count number of polygons to remove.
	int nrem = 0;
	for (int i = 0; i < mesh.npolys; ++i)
	{
		unsigned short* p = &mesh.polys[i*nvp*2];
		for (int j = 0; j < nvp; ++j)
			if (p[j] == rem) { nrem++; break; }
	}

	edges = new int[nrem*nvp*3];
	if (!edges)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'edges' (%d).", nrem*nvp*3);
		goto failure;
	}

	hole = new int[nrem*nvp];
	if (!hole)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'hole' (%d).", nrem*nvp);
		goto failure;
	}
	hreg = new int[nrem*nvp];
	if (!hreg)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'hreg' (%d).", nrem*nvp);
		goto failure;
	}
	
		
	for (int i = 0; i < mesh.npolys; ++i)
	{
		unsigned short* p = &mesh.polys[i*nvp*2];
		const int nv = countPolyVerts(p, nvp);
		bool hasRem = false;
		for (int j = 0; j < nv; ++j)
			if (p[j] == rem) hasRem = true;
		if (hasRem)
		{
			// Collect edges which does not touch the removed vertex.
			for (int j = 0, k = nv-1; j < nv; k = j++)
			{
				if (p[j] != rem && p[k] != rem)
				{
					int* e = &edges[nedges*3];
					e[0] = p[k];
					e[1] = p[j];
					e[2] = mesh.regs[i];
					nedges++;
				}
			}
			// Remove the polygon.
			unsigned short* p2 = &mesh.polys[(mesh.npolys-1)*nvp*2];
			memcpy(p,p2,sizeof(unsigned short)*nvp);
			mesh.regs[i] = mesh.regs[mesh.npolys-1];
			mesh.npolys--;
			--i;
		}
	}
	
	// Remove vertex.
	for (int i = (int)rem; i < mesh.nverts; ++i)
	{
		mesh.verts[i*3+0] = mesh.verts[(i+1)*3+0];
		mesh.verts[i*3+1] = mesh.verts[(i+1)*3+1];
		mesh.verts[i*3+2] = mesh.verts[(i+1)*3+2];
	}
	mesh.nverts--;

	// Adjust indices to match the removed vertex layout.
	for (int i = 0; i < mesh.npolys; ++i)
	{
		unsigned short* p = &mesh.polys[i*nvp*2];
		const int nv = countPolyVerts(p, nvp);
		for (int j = 0; j < nv; ++j)
			if (p[j] > rem) p[j]--;
	}
	for (int i = 0; i < nedges; ++i)
	{
		if (edges[i*3+0] > rem) edges[i*3+0]--;
		if (edges[i*3+1] > rem) edges[i*3+1]--;
	}

	if (nedges == 0)
		return true;

	hole[nhole] = edges[0];
	hreg[nhole] = edges[2];
	nhole++;
	
	while (nedges)
	{
		bool match = false;
		
		for (int i = 0; i < nedges; ++i)
		{
			const int ea = edges[i*3+0];
			const int eb = edges[i*3+1];
			const int r = edges[i*3+2];
			bool add = false;
			if (hole[0] == eb)
			{
				pushFront(ea, hole, nhole);
				pushFront(r, hreg, nhreg);
				add = true;
			}
			else if (hole[nhole-1] == ea)
			{
				pushBack(eb, hole, nhole);
				pushBack(r, hreg, nhreg);
				add = true;
			}
			if (add)
			{
				// Remove edge.
				edges[i*3+0] = edges[(nedges-1)*3+0];
				edges[i*3+1] = edges[(nedges-1)*3+1];
				edges[i*3+2] = edges[(nedges-1)*3+2];
				--nedges;
				match = true;
				--i;
			}
		}
		
		if (!match)
			break;
	}

	tris = new int[nhole*3];
	if (!tris)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'tris' (%d).", nhole*3);
		goto failure;
	}

	tverts = new int[nhole*4];
	if (!tverts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'tverts' (%d).", nhole*4);
		goto failure;
	}

	thole = new int[nhole];
	if (!tverts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'thole' (%d).", nhole);
		goto failure;
	}

	// Generate temp vertex array for triangulation.
	for (int i = 0; i < nhole; ++i)
	{
		const int pi = hole[i];
		tverts[i*4+0] = mesh.verts[pi*3+0];
		tverts[i*4+1] = mesh.verts[pi*3+1];
		tverts[i*4+2] = mesh.verts[pi*3+2];
		tverts[i*4+3] = 0;
		thole[i] = i;
	}

	// Triangulate the hole.
	int ntris = triangulate(nhole, &tverts[0], &thole[0], tris);

	// Merge the hole triangles back to polygons.
	polys = new unsigned short[(ntris+1)*nvp];
	if (!polys)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'polys' (%d).", (ntris+1)*nvp);
		goto failure;
	}
	pregs = new unsigned short[ntris];
	if (!pregs)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_WARNING, "removeVertex: Out of memory 'pregs' (%d).", ntris);
		goto failure;
	}
	
	unsigned short* tmpPoly = &polys[ntris*nvp];
			
	// Build initial polygons.
	memset(polys, 0xff, ntris*nvp*sizeof(unsigned short));
	for (int j = 0; j < ntris; ++j)
	{
		int* t = &tris[j*3];
		if (t[0] != t[1] && t[0] != t[2] && t[1] != t[2])
		{
			polys[npolys*nvp+0] = (unsigned short)hole[t[0]];
			polys[npolys*nvp+1] = (unsigned short)hole[t[1]];
			polys[npolys*nvp+2] = (unsigned short)hole[t[2]];
			pregs[npolys] = hreg[t[0]];
			npolys++;
		}
	}
	if (!npolys)
		return true;
	
	// Merge polygons.
	if (nvp > 3)
	{
		while (true)
		{
			// Find best polygons to merge.
			int bestMergeVal = 0;
			int bestPa, bestPb, bestEa, bestEb;
			
			for (int j = 0; j < npolys-1; ++j)
			{
				unsigned short* pj = &polys[j*nvp];
				for (int k = j+1; k < npolys; ++k)
				{
					unsigned short* pk = &polys[k*nvp];
					int ea, eb;
					int v = getPolyMergeValue(pj, pk, mesh.verts, ea, eb, nvp);
					if (v > bestMergeVal)
					{
						bestMergeVal = v;
						bestPa = j;
						bestPb = k;
						bestEa = ea;
						bestEb = eb;
					}
				}
			}
			
			if (bestMergeVal > 0)
			{
				// Found best, merge.
				unsigned short* pa = &polys[bestPa*nvp];
				unsigned short* pb = &polys[bestPb*nvp];
				mergePolys(pa, pb, mesh.verts, bestEa, bestEb, tmpPoly, nvp);
				memcpy(pb, &polys[(npolys-1)*nvp], sizeof(unsigned short)*nvp);
				pregs[bestPb] = pregs[npolys-1];
				npolys--;
			}
			else
			{
				// Could not merge any polygons, stop.
				break;
			}
		}
	}
	
	// Store polygons.
	for (int i = 0; i < npolys; ++i)
	{
		if (mesh.npolys >= maxTris) break;
		unsigned short* p = &mesh.polys[mesh.npolys*nvp*2];
		memset(p,0xff,sizeof(unsigned short)*nvp*2);
		for (int j = 0; j < nvp; ++j)
			p[j] = polys[i*nvp+j];
		mesh.regs[mesh.npolys] = pregs[i];
		mesh.npolys++;
	}
	
	delete [] edges;
	delete [] hole;
	delete [] hreg;
	delete [] tris;
	delete [] thole;
	delete [] tverts;
	delete [] polys;
	delete [] pregs;
	
	return true;

failure:
	delete [] edges;
	delete [] hole;
	delete [] hreg;
	delete [] tris;
	delete [] thole;
	delete [] tverts;
	delete [] polys;
	delete [] pregs;
	
	return false;
}


bool rcBuildPolyMesh(rcContourSet& cset, int nvp, rcPolyMesh& mesh)
{
	rcTimeVal startTime = rcGetPerformanceTimer();

	vcopy(mesh.bmin, cset.bmin);
	vcopy(mesh.bmax, cset.bmax);
	mesh.cs = cset.cs;
	mesh.ch = cset.ch;
	
	int maxVertices = 0;
	int maxTris = 0;
	int maxVertsPerCont = 0;
	for (int i = 0; i < cset.nconts; ++i)
	{
		maxVertices += cset.conts[i].nverts;
		maxTris += cset.conts[i].nverts - 2;
		maxVertsPerCont = rcMax(maxVertsPerCont, cset.conts[i].nverts);
	}
	
	if (maxVertices >= 0xfffe)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Too many vertices %d.", maxVertices);
		return false;
	}
	
	unsigned char* vflags = 0;
	int* nextVert = 0;
	int* firstVert = 0;
	int* indices = 0;
	int* tris = 0;
	unsigned short* polys = 0;
	
	vflags = new unsigned char[maxVertices];
	if (!vflags)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'mesh.verts' (%d).", maxVertices);
		goto failure;
	}
	memset(vflags, 0, maxVertices);
	
	mesh.verts = new unsigned short[maxVertices*3];
	if (!mesh.verts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'mesh.verts' (%d).", maxVertices);
		goto failure;
	}
	mesh.polys = new unsigned short[maxTris*nvp*2];
	if (!mesh.polys)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'mesh.polys' (%d).", maxTris*nvp*2);
		goto failure;
	}
	mesh.regs = new unsigned short[maxTris];
	if (!mesh.regs)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'mesh.regs' (%d).", maxTris);
		goto failure;
	}
	mesh.nverts = 0;
	mesh.npolys = 0;
	mesh.nvp = nvp;
	
	memset(mesh.verts, 0, sizeof(unsigned short)*maxVertices*3);
	memset(mesh.polys, 0xff, sizeof(unsigned short)*maxTris*nvp*2);
	memset(mesh.regs, 0, sizeof(unsigned short)*maxTris);
	
	nextVert = new int[maxVertices];
	if (!nextVert)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'nextVert' (%d).", maxVertices);
		goto failure;
	}
	memset(nextVert, 0, sizeof(int)*maxVertices);
	
	firstVert = new int[VERTEX_BUCKET_COUNT];
	if (!firstVert)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'firstVert' (%d).", VERTEX_BUCKET_COUNT);
		goto failure;
	}
	for (int i = 0; i < VERTEX_BUCKET_COUNT; ++i)
		firstVert[i] = -1;
	
	indices = new int[maxVertsPerCont];
	if (!indices)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'indices' (%d).", maxVertsPerCont);
		goto failure;
	}
	tris = new int[maxVertsPerCont*3];
	if (!tris)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'tris' (%d).", maxVertsPerCont*3);
		goto failure;
	}
	polys = new unsigned short[(maxVertsPerCont+1)*nvp];
	if (!polys)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Out of memory 'polys' (%d).", maxVertsPerCont*nvp);
		goto failure;
	}
	unsigned short* tmpPoly = &polys[maxVertsPerCont*nvp];

	for (int i = 0; i < cset.nconts; ++i)
	{
		rcContour& cont = cset.conts[i];
		
		// Skip empty contours.
		if (cont.nverts < 3)
			continue;
		
		// Triangulate contour
		for (int j = 0; j < cont.nverts; ++j)
			indices[j] = j;
			
		int ntris = triangulate(cont.nverts, cont.verts, &indices[0], &tris[0]);
		if (ntris <= 0)
		{
			// Bad triangulation, should not happen.
/*			for (int k = 0; k < cont.nverts; ++k)
			{
				const int* v = &cont.verts[k*4];
				printf("\t\t%d,%d,%d,%d,\n", v[0], v[1], v[2], v[3]);
				if (nBadPos < 100)
				{
					badPos[nBadPos*3+0] = v[0];
					badPos[nBadPos*3+1] = v[1];
					badPos[nBadPos*3+2] = v[2];
					nBadPos++;
				}
			}*/
			ntris = -ntris;
		}
		// Add and merge vertices.
		for (int j = 0; j < cont.nverts; ++j)
		{
			const int* v = &cont.verts[j*4];
			indices[j] = addVertex((unsigned short)v[0], (unsigned short)v[1], (unsigned short)v[2],
								   mesh.verts, firstVert, nextVert, mesh.nverts);
			if (v[3] & RC_BORDER_VERTEX)
			{
				// This vertex should be removed.
				vflags[indices[j]] = 1;
			}
		}
		
		// Build initial polygons.
		int npolys = 0;
		memset(polys, 0xff, maxVertsPerCont*nvp*sizeof(unsigned short));
		for (int j = 0; j < ntris; ++j)
		{
			int* t = &tris[j*3];
			if (t[0] != t[1] && t[0] != t[2] && t[1] != t[2])
			{
				polys[npolys*nvp+0] = (unsigned short)indices[t[0]];
				polys[npolys*nvp+1] = (unsigned short)indices[t[1]];
				polys[npolys*nvp+2] = (unsigned short)indices[t[2]];
				npolys++;
			}
		}
		if (!npolys)
			continue;
		
		// Merge polygons.
		if (nvp > 3)
		{
			while (true)
			{
				// Find best polygons to merge.
				int bestMergeVal = 0;
				int bestPa, bestPb, bestEa, bestEb;
				
				for (int j = 0; j < npolys-1; ++j)
				{
					unsigned short* pj = &polys[j*nvp];
					for (int k = j+1; k < npolys; ++k)
					{
						unsigned short* pk = &polys[k*nvp];
						int ea, eb;
						int v = getPolyMergeValue(pj, pk, mesh.verts, ea, eb, nvp);
						if (v > bestMergeVal)
						{
							bestMergeVal = v;
							bestPa = j;
							bestPb = k;
							bestEa = ea;
							bestEb = eb;
						}
					}
				}
				
				if (bestMergeVal > 0)
				{
					// Found best, merge.
					unsigned short* pa = &polys[bestPa*nvp];
					unsigned short* pb = &polys[bestPb*nvp];
					mergePolys(pa, pb, mesh.verts, bestEa, bestEb, tmpPoly, nvp);
					memcpy(pb, &polys[(npolys-1)*nvp], sizeof(unsigned short)*nvp);
					npolys--;
				}
				else
				{
					// Could not merge any polygons, stop.
					break;
				}
			}
		}
		
		
		// Store polygons.
		for (int j = 0; j < npolys; ++j)
		{
			unsigned short* p = &mesh.polys[mesh.npolys*nvp*2];
			unsigned short* q = &polys[j*nvp];
			for (int k = 0; k < nvp; ++k)
				p[k] = q[k];
			mesh.regs[mesh.npolys] = cont.reg;
			mesh.npolys++;
		}
	}
	
	
	// Remove edge vertices.
	for (int i = 0; i < mesh.nverts; ++i)
	{
		if (vflags[i])
		{
			if (!removeVertex(mesh, i, maxTris))
				goto failure;
			for (int j = i; j < mesh.nverts-1; ++j)
				vflags[j] = vflags[j+1];
			--i;
		}
	}

	delete [] vflags;
	delete [] firstVert;
	delete [] nextVert;
	delete [] indices;
	delete [] tris;
	
	// Calculate adjacency.
	if (!buildMeshAdjacency(mesh.polys, mesh.npolys, mesh.nverts, nvp))
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildPolyMesh: Adjacency failed.");
		return false;
	}
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
//	if (rcGetLog())
//		rcGetLog()->log(RC_LOG_PROGRESS, "Build polymesh: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
	if (rcGetBuildTimes())
		rcGetBuildTimes()->buildPolymesh += rcGetDeltaTimeUsec(startTime, endTime);
	
	return true;

failure:
	delete [] vflags;
	delete [] tmpPoly;
	delete [] firstVert;
	delete [] nextVert;
	delete [] indices;
	delete [] tris;

	return false;
}

bool rcMergePolyMeshes(rcPolyMesh** meshes, const int nmeshes, rcPolyMesh& mesh)
{
	if (!nmeshes || !meshes)
		return true;

	rcTimeVal startTime = rcGetPerformanceTimer();

	int* nextVert = 0;
	int* firstVert = 0;
	unsigned short* vremap = 0;

	mesh.nvp = meshes[0]->nvp;
	mesh.cs = meshes[0]->cs;
	mesh.ch = meshes[0]->ch;
	vcopy(mesh.bmin, meshes[0]->bmin);
	vcopy(mesh.bmax, meshes[0]->bmax);

	int maxVerts = 0;
	int maxPolys = 0;
	int maxVertsPerMesh = 0;
	for (int i = 0; i < nmeshes; ++i)
	{
		vmin(mesh.bmin, meshes[i]->bmin);
		vmax(mesh.bmax, meshes[i]->bmax);
		maxVertsPerMesh = rcMax(maxVertsPerMesh, meshes[i]->nverts);
		maxVerts += meshes[i]->nverts;
		maxPolys += meshes[i]->npolys;
	}
	
	mesh.nverts = 0;
	mesh.verts = new unsigned short[maxVerts*3];
	if (!mesh.verts)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'mesh.verts' (%d).", maxVerts*3);
		return false;
	}

	mesh.npolys = 0;
	mesh.polys = new unsigned short[maxPolys*2*mesh.nvp];
	if (!mesh.polys)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'mesh.polys' (%d).", maxPolys*2*mesh.nvp);
		return false;
	}
	memset(mesh.polys, 0xff, sizeof(unsigned short)*maxPolys*2*mesh.nvp);

	mesh.regs = new unsigned short[maxPolys];
	if (!mesh.regs)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'mesh.regs' (%d).", maxPolys);
		return false;
	}
	memset(mesh.regs, 0, sizeof(unsigned short)*maxPolys);
	
	nextVert = new int[maxVerts];
	if (!nextVert)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'nextVert' (%d).", maxVerts);
		goto failure;
	}
	memset(nextVert, 0, sizeof(int)*maxVerts);
	
	firstVert = new int[VERTEX_BUCKET_COUNT];
	if (!firstVert)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'firstVert' (%d).", VERTEX_BUCKET_COUNT);
		goto failure;
	}
	for (int i = 0; i < VERTEX_BUCKET_COUNT; ++i)
		firstVert[i] = -1;

	vremap = new unsigned short[maxVertsPerMesh];
	if (!vremap)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Out of memory 'vremap' (%d).", maxVertsPerMesh);
		goto failure;
	}
	memset(nextVert, 0, sizeof(int)*maxVerts);
	
	for (int i = 0; i < nmeshes; ++i)
	{
		const rcPolyMesh* pmesh = meshes[i];
		
		const unsigned short ox = (unsigned short)floorf((pmesh->bmin[0]-mesh.bmin[0])/mesh.cs+0.5f);
		const unsigned short oz = (unsigned short)floorf((pmesh->bmin[2]-mesh.bmin[2])/mesh.cs+0.5f);
		
		for (int j = 0; j < pmesh->nverts; ++j)
		{
			unsigned short* v = &pmesh->verts[j*3];
			vremap[j] = addVertex(v[0]+ox, v[1], v[2]+oz,
						   mesh.verts, firstVert, nextVert, mesh.nverts);
		}
		
		for (int j = 0; j < pmesh->npolys; ++j)
		{
			unsigned short* tgt = &mesh.polys[mesh.npolys*2*mesh.nvp];
			unsigned short* src = &pmesh->polys[j*2*mesh.nvp];
			mesh.regs[mesh.npolys] = pmesh->regs[j];
			mesh.npolys++;
			for (int k = 0; k < mesh.nvp; ++k)
			{
				if (src[k] == 0xffff) break;
				tgt[k] = vremap[src[k]];
			}
		}
	}

	// Calculate adjacency.
	if (!buildMeshAdjacency(mesh.polys, mesh.npolys, mesh.nverts, mesh.nvp))
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMergePolyMeshes: Adjacency failed.");
		return false;
	}
		

	delete [] firstVert;
	delete [] nextVert;
	delete [] vremap;
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
	if (rcGetBuildTimes())
		rcGetBuildTimes()->mergePolyMesh += rcGetDeltaTimeUsec(startTime, endTime);
	
	return true;
	
failure:
	delete [] firstVert;
	delete [] nextVert;
	delete [] vremap;
	
	return false;
}
