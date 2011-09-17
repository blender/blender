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


static int getCornerHeight(int x, int y, int i, int dir,
						   const rcCompactHeightfield& chf,
						   bool& isBorderVertex)
{
	const rcCompactSpan& s = chf.spans[i];
	int ch = (int)s.y;
	int dirp = (dir+1) & 0x3;
	
	unsigned short regs[4] = {0,0,0,0};
	
	regs[0] = s.reg;
	
	if (rcGetCon(s, dir) != 0xf)
	{
		const int ax = x + rcGetDirOffsetX(dir);
		const int ay = y + rcGetDirOffsetY(dir);
		const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(s, dir);
		const rcCompactSpan& as = chf.spans[ai];
		ch = rcMax(ch, (int)as.y);
		regs[1] = as.reg;
		if (rcGetCon(as, dirp) != 0xf)
		{
			const int ax2 = ax + rcGetDirOffsetX(dirp);
			const int ay2 = ay + rcGetDirOffsetY(dirp);
			const int ai2 = (int)chf.cells[ax2+ay2*chf.width].index + rcGetCon(as, dirp);
			const rcCompactSpan& as2 = chf.spans[ai2];
			ch = rcMax(ch, (int)as2.y);
			regs[2] = as2.reg;
		}
	}
	if (rcGetCon(s, dirp) != 0xf)
	{
		const int ax = x + rcGetDirOffsetX(dirp);
		const int ay = y + rcGetDirOffsetY(dirp);
		const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(s, dirp);
		const rcCompactSpan& as = chf.spans[ai];
		ch = rcMax(ch, (int)as.y);
		regs[3] = as.reg;
		if (rcGetCon(as, dir) != 0xf)
		{
			const int ax2 = ax + rcGetDirOffsetX(dir);
			const int ay2 = ay + rcGetDirOffsetY(dir);
			const int ai2 = (int)chf.cells[ax2+ay2*chf.width].index + rcGetCon(as, dir);
			const rcCompactSpan& as2 = chf.spans[ai2];
			ch = rcMax(ch, (int)as2.y);
			regs[2] = as2.reg;
		}
	}

	// Check if the vertex is special edge vertex, these vertices will be removed later.
	for (int j = 0; j < 4; ++j)
	{
		const int a = j;
		const int b = (j+1) & 0x3;
		const int c = (j+2) & 0x3;
		const int d = (j+3) & 0x3;
		
		// The vertex is a border vertex there are two same exterior cells in a row,
		// followed by two interior cells and none of the regions are out of bounds.
		const bool twoSameExts = (regs[a] & regs[b] & RC_BORDER_REG) != 0 && regs[a] == regs[b];
		const bool twoInts = ((regs[c] | regs[d]) & RC_BORDER_REG) == 0;
		const bool noZeros = regs[a] != 0 && regs[b] != 0 && regs[c] != 0 && regs[d] != 0;
		if (twoSameExts && twoInts && noZeros)
		{
			isBorderVertex = true;
			break;
		}
	}
	
	return ch;
}

static void walkContour(int x, int y, int i,
						rcCompactHeightfield& chf,
						unsigned char* flags, rcIntArray& points)
{
	// Choose the first non-connected edge
	unsigned char dir = 0;
	while ((flags[i] & (1 << dir)) == 0)
		dir++;
	
	unsigned char startDir = dir;
	int starti = i;
	
	int iter = 0;
	while (++iter < 40000)
	{
		if (flags[i] & (1 << dir))
		{
			// Choose the edge corner
			bool isBorderVertex = false;
			int px = x;
			int py = getCornerHeight(x, y, i, dir, chf, isBorderVertex);
			int pz = y;
			switch(dir)
			{
				case 0: pz++; break;
				case 1: px++; pz++; break;
				case 2: px++; break;
			}
			int r = 0;
			const rcCompactSpan& s = chf.spans[i];
			if (rcGetCon(s, dir) != 0xf)
			{
				const int ax = x + rcGetDirOffsetX(dir);
				const int ay = y + rcGetDirOffsetY(dir);
				const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(s, dir);
				const rcCompactSpan& as = chf.spans[ai];
				r = (int)as.reg;
			}
			if (isBorderVertex)
				r |= RC_BORDER_VERTEX;
			points.push(px);
			points.push(py);
			points.push(pz);
			points.push(r);
			
			flags[i] &= ~(1 << dir); // Remove visited edges
			dir = (dir+1) & 0x3;  // Rotate CW
		}
		else
		{
			int ni = -1;
			const int nx = x + rcGetDirOffsetX(dir);
			const int ny = y + rcGetDirOffsetY(dir);
			const rcCompactSpan& s = chf.spans[i];
			if (rcGetCon(s, dir) != 0xf)
			{
				const rcCompactCell& nc = chf.cells[nx+ny*chf.width];
				ni = (int)nc.index + rcGetCon(s, dir);
			}
			if (ni == -1)
			{
				// Should not happen.
				return;
			}
			x = nx;
			y = ny;
			i = ni;
			dir = (dir+3) & 0x3;	// Rotate CCW
		}
		
		if (starti == i && startDir == dir)
		{
			break;
		}
	}
}

static float distancePtSeg(int x, int y, int z,
						   int px, int py, int pz,
						   int qx, int qy, int qz)
{
/*	float pqx = (float)(qx - px);
	float pqy = (float)(qy - py);
	float pqz = (float)(qz - pz);
	float dx = (float)(x - px);
	float dy = (float)(y - py);
	float dz = (float)(z - pz);
	float d = pqx*pqx + pqy*pqy + pqz*pqz;
	float t = pqx*dx + pqy*dy + pqz*dz;
	if (d > 0)
		t /= d;
	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	
	dx = px + t*pqx - x;
	dy = py + t*pqy - y;
	dz = pz + t*pqz - z;
	
	return dx*dx + dy*dy + dz*dz;*/

	float pqx = (float)(qx - px);
	float pqz = (float)(qz - pz);
	float dx = (float)(x - px);
	float dz = (float)(z - pz);
	float d = pqx*pqx + pqz*pqz;
	float t = pqx*dx + pqz*dz;
	if (d > 0)
		t /= d;
	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	
	dx = px + t*pqx - x;
	dz = pz + t*pqz - z;
	
	return dx*dx + dz*dz;
}

static void simplifyContour(rcIntArray& points, rcIntArray& simplified, float maxError, int maxEdgeLen)
{
	// Add initial points.
	bool noConnections = true;
	for (int i = 0; i < points.size(); i += 4)
	{
		if ((points[i+3] & 0xffff) != 0)
		{
			noConnections = false;
			break;
		}
	}
	
	if (noConnections)
	{
		// If there is no connections at all,
		// create some initial points for the simplification process. 
		// Find lower-left and upper-right vertices of the contour.
		int llx = points[0];
		int lly = points[1];
		int llz = points[2];
		int lli = 0;
		int urx = points[0];
		int ury = points[1];
		int urz = points[2];
		int uri = 0;
		for (int i = 0; i < points.size(); i += 4)
		{
			int x = points[i+0];
			int y = points[i+1];
			int z = points[i+2];
			if (x < llx || (x == llx && z < llz))
			{
				llx = x;
				lly = y;
				llz = z;
				lli = i/4;
			}
			if (x >= urx || (x == urx && z > urz))
			{
				urx = x;
				ury = y;
				urz = z;
				uri = i/4;
			}
		}
		simplified.push(llx);
		simplified.push(lly);
		simplified.push(llz);
		simplified.push(lli);
		
		simplified.push(urx);
		simplified.push(ury);
		simplified.push(urz);
		simplified.push(uri);
	}
	else
	{
		// The contour has some portals to other regions.
		// Add a new point to every location where the region changes.
		for (int i = 0, ni = points.size()/4; i < ni; ++i)
		{
			int ii = (i+1) % ni;
			if ((points[i*4+3] & 0xffff) != (points[ii*4+3] & 0xffff))
			{
				simplified.push(points[i*4+0]);
				simplified.push(points[i*4+1]);
				simplified.push(points[i*4+2]);
				simplified.push(i);
			}
		}	
	}
	
	// Add points until all raw points are within
	// error tolerance to the simplified shape.
	const int pn = points.size()/4;
	for (int i = 0; i < simplified.size()/4; )
	{
		int ii = (i+1) % (simplified.size()/4);
		
		int ax = simplified[i*4+0];
		int ay = simplified[i*4+1];
		int az = simplified[i*4+2];
		int ai = simplified[i*4+3];
		
		int bx = simplified[ii*4+0];
		int by = simplified[ii*4+1];
		int bz = simplified[ii*4+2];
		int bi = simplified[ii*4+3];
		
		// Find maximum deviation from the segment.
		float maxd = 0;
		int maxi = -1;
		int ci = (ai+1) % pn;
		
		// Tesselate only outer edges.
		if ((points[ci*4+3] & 0xffff) == 0)
		{
			while (ci != bi)
			{
				float d = distancePtSeg(points[ci*4+0], points[ci*4+1]/4, points[ci*4+2],
										ax, ay/4, az, bx, by/4, bz);
				if (d > maxd)
				{
					maxd = d;
					maxi = ci;
				}
				ci = (ci+1) % pn;
			}
		}
		
		
		// If the max deviation is larger than accepted error,
		// add new point, else continue to next segment.
		if (maxi != -1 && maxd > (maxError*maxError))
		{
			// Add space for the new point.
			simplified.resize(simplified.size()+4);
			int n = simplified.size()/4;
			for (int j = n-1; j > i; --j)
			{
				simplified[j*4+0] = simplified[(j-1)*4+0];
				simplified[j*4+1] = simplified[(j-1)*4+1];
				simplified[j*4+2] = simplified[(j-1)*4+2];
				simplified[j*4+3] = simplified[(j-1)*4+3];
			}
			// Add the point.
			simplified[(i+1)*4+0] = points[maxi*4+0];
			simplified[(i+1)*4+1] = points[maxi*4+1];
			simplified[(i+1)*4+2] = points[maxi*4+2];
			simplified[(i+1)*4+3] = maxi;
		}
		else
		{
			++i;
		}
	}
	
	// Split too long edges.
	if (maxEdgeLen > 0)
	{
		for (int i = 0; i < simplified.size()/4; )
		{
			int ii = (i+1) % (simplified.size()/4);
			
			int ax = simplified[i*4+0];
			int az = simplified[i*4+2];
			int ai = simplified[i*4+3];
			
			int bx = simplified[ii*4+0];
			int bz = simplified[ii*4+2];
			int bi = simplified[ii*4+3];
			
			// Find maximum deviation from the segment.
			int maxi = -1;
			int ci = (ai+1) % pn;
			
			// Tesselate only outer edges.
			if ((points[ci*4+3] & 0xffff) == 0)
			{
				int dx = bx - ax;
				int dz = bz - az;
				if (dx*dx + dz*dz > maxEdgeLen*maxEdgeLen)
				{
					int n = bi < ai ? (bi+pn - ai) : (bi - ai);
					maxi = (ai + n/2) % pn;
				}
			}
			
			// If the max deviation is larger than accepted error,
			// add new point, else continue to next segment.
			if (maxi != -1)
			{
				// Add space for the new point.
				simplified.resize(simplified.size()+4);
				int n = simplified.size()/4;
				for (int j = n-1; j > i; --j)
				{
					simplified[j*4+0] = simplified[(j-1)*4+0];
					simplified[j*4+1] = simplified[(j-1)*4+1];
					simplified[j*4+2] = simplified[(j-1)*4+2];
					simplified[j*4+3] = simplified[(j-1)*4+3];
				}
				// Add the point.
				simplified[(i+1)*4+0] = points[maxi*4+0];
				simplified[(i+1)*4+1] = points[maxi*4+1];
				simplified[(i+1)*4+2] = points[maxi*4+2];
				simplified[(i+1)*4+3] = maxi;
			}
			else
			{
				++i;
			}
		}
	}
	
	for (int i = 0; i < simplified.size()/4; ++i)
	{
		// The edge vertex flag is take from the current raw point,
		// and the neighbour region is take from the next raw point.
		const int ai = (simplified[i*4+3]+1) % pn;
		const int bi = simplified[i*4+3];
		simplified[i*4+3] = (points[ai*4+3] & 0xffff) | (points[bi*4+3] & RC_BORDER_VERTEX);
	}
	
}

static void removeDegenerateSegments(rcIntArray& simplified)
{
	// Remove adjacent vertices which are equal on xz-plane,
	// or else the triangulator will get confused.
	for (int i = 0; i < simplified.size()/4; ++i)
	{
		int ni = i+1;
		if (ni >= (simplified.size()/4))
			ni = 0;
			
		if (simplified[i*4+0] == simplified[ni*4+0] &&
			simplified[i*4+2] == simplified[ni*4+2])
		{
			// Degenerate segment, remove.
			for (int j = i; j < simplified.size()/4-1; ++j)
			{
				simplified[j*4+0] = simplified[(j+1)*4+0];
				simplified[j*4+1] = simplified[(j+1)*4+1];
				simplified[j*4+2] = simplified[(j+1)*4+2];
				simplified[j*4+3] = simplified[(j+1)*4+3];
			}
			simplified.pop();
		}
	}
}

static int calcAreaOfPolygon2D(const int* verts, const int nverts)
{
	int area = 0;
	for (int i = 0, j = nverts-1; i < nverts; j=i++)
	{
		const int* vi = &verts[i*4];
		const int* vj = &verts[j*4];
		area += vi[0] * vj[2] - vj[0] * vi[2];
	}
	return (area+1) / 2;
}

static void getClosestIndices(const int* vertsa, const int nvertsa,
							  const int* vertsb, const int nvertsb,
							  int& ia, int& ib)
{
	int closestDist = 0xfffffff;
	for (int i = 0; i < nvertsa; ++i)
	{
		const int* va = &vertsa[i*4];
		for (int j = 0; j < nvertsb; ++j)
		{
			const int* vb = &vertsb[j*4];
			const int dx = vb[0] - va[0];
			const int dz = vb[2] - va[2];
			const int d = dx*dx + dz*dz;
			if (d < closestDist)
			{
				ia = i;
				ib = j;
				closestDist = d;
			}
		}
	}
}

static bool mergeContours(rcContour& ca, rcContour& cb, int ia, int ib)
{
	const int maxVerts = ca.nverts + cb.nverts + 2;
	int* verts = new int[maxVerts*4];
	if (!verts)
		return false;

	int nv = 0;

	// Copy contour A.
	for (int i = 0; i <= ca.nverts; ++i)
	{
		int* dst = &verts[nv*4];
		const int* src = &ca.verts[((ia+i)%ca.nverts)*4];
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		nv++;
	}

	// Copy contour B
	for (int i = 0; i <= cb.nverts; ++i)
	{
		int* dst = &verts[nv*4];
		const int* src = &cb.verts[((ib+i)%cb.nverts)*4];
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		nv++;
	}
	
	delete [] ca.verts;
	ca.verts = verts;
	ca.nverts = nv;

	delete [] cb.verts;
	cb.verts = 0;
	cb.nverts = 0;
	
	return true;
}

bool rcBuildContours(rcCompactHeightfield& chf,
					 const float maxError, const int maxEdgeLen,
					 rcContourSet& cset)
{
	const int w = chf.width;
	const int h = chf.height;
	
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	vcopy(cset.bmin, chf.bmin);
	vcopy(cset.bmax, chf.bmax);
	cset.cs = chf.cs;
	cset.ch = chf.ch;
	
	const int maxContours = chf.maxRegions*2;
	cset.conts = new rcContour[maxContours];
	if (!cset.conts)
		return false;
	cset.nconts = 0;
	
	unsigned char* flags = new unsigned char[chf.spanCount];
	if (!flags)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildContours: Out of memory 'flags'.");
		return false;
	}
	
	rcTimeVal traceStartTime = rcGetPerformanceTimer();
					
	
	// Mark boundaries.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				unsigned char res = 0;
				const rcCompactSpan& s = chf.spans[i];
				if (!s.reg || (s.reg & RC_BORDER_REG))
				{
					flags[i] = 0;
					continue;
				}
				for (int dir = 0; dir < 4; ++dir)
				{
					unsigned short r = 0;
					if (rcGetCon(s, dir) != 0xf)
					{
						const int ax = x + rcGetDirOffsetX(dir);
						const int ay = y + rcGetDirOffsetY(dir);
						const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, dir);
						const rcCompactSpan& as = chf.spans[ai];
						r = as.reg;
					}
					if (r == s.reg)
						res |= (1 << dir);
				}
				flags[i] = res ^ 0xf; // Inverse, mark non connected edges.
			}
		}
	}
	
	rcTimeVal traceEndTime = rcGetPerformanceTimer();
	
	rcTimeVal simplifyStartTime = rcGetPerformanceTimer();
	
	rcIntArray verts(256);
	rcIntArray simplified(64);
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				if (flags[i] == 0 || flags[i] == 0xf)
				{
					flags[i] = 0;
					continue;
				}
				unsigned short reg = chf.spans[i].reg;
				if (!reg || (reg & RC_BORDER_REG))
					continue;
				
				verts.resize(0);
				simplified.resize(0);
				walkContour(x, y, i, chf, flags, verts);
				simplifyContour(verts, simplified, maxError, maxEdgeLen);
				removeDegenerateSegments(simplified);
				
				// Store region->contour remap info.
				// Create contour.
				if (simplified.size()/4 >= 3)
				{
					if (cset.nconts >= maxContours)
					{
						if (rcGetLog())
							rcGetLog()->log(RC_LOG_ERROR, "rcBuildContours: Too many contours %d, max %d.", cset.nconts, maxContours);
						return false;
					}
						
					rcContour* cont = &cset.conts[cset.nconts++];
					
					cont->nverts = simplified.size()/4;
					cont->verts = new int[cont->nverts*4];
					memcpy(cont->verts, &simplified[0], sizeof(int)*cont->nverts*4);
					
					cont->nrverts = verts.size()/4;
					cont->rverts = new int[cont->nrverts*4];
					memcpy(cont->rverts, &verts[0], sizeof(int)*cont->nrverts*4);
					
/*					cont->cx = cont->cy = cont->cz = 0;
					for (int i = 0; i < cont->nverts; ++i)
					{
						cont->cx += cont->verts[i*4+0];
						cont->cy += cont->verts[i*4+1];
						cont->cz += cont->verts[i*4+2];
					}
					cont->cx /= cont->nverts;
					cont->cy /= cont->nverts;
					cont->cz /= cont->nverts;*/
					
					cont->reg = reg;
				}
			}
		}
	}
	
	// Check and merge droppings.
	// Sometimes the previous algorithms can fail and create several countours
	// per area. This pass will try to merge the holes into the main region.
	for (int i = 0; i < cset.nconts; ++i)
	{
		rcContour& cont = cset.conts[i];
		// Check if the contour is would backwards.
		if (calcAreaOfPolygon2D(cont.verts, cont.nverts) < 0)
		{
			// Find another contour which has the same region ID.
			int mergeIdx = -1;
			for (int j = 0; j < cset.nconts; ++j)
			{
				if (i == j) continue;
				if (cset.conts[j].nverts && cset.conts[j].reg == cont.reg)
				{
					// Make sure the polygon is correctly oriented.
					if (calcAreaOfPolygon2D(cset.conts[j].verts, cset.conts[j].nverts))
					{
						mergeIdx = j;
						break;
					}
				}
			}
			if (mergeIdx == -1)
			{
				if (rcGetLog())
					rcGetLog()->log(RC_LOG_WARNING, "rcBuildContours: Could not find merge target for bad contour %d.", i);
			}
			else
			{
				rcContour& mcont = cset.conts[mergeIdx];
				// Merge by closest points.
				int ia, ib;
				getClosestIndices(mcont.verts, mcont.nverts, cont.verts, cont.nverts, ia, ib);
				if (!mergeContours(mcont, cont, ia, ib))
				{
					if (rcGetLog())
						rcGetLog()->log(RC_LOG_WARNING, "rcBuildContours: Failed to merge contours %d and %d.", i, mergeIdx);
				}
			}
		}
	}
	
		
	delete [] flags;
	
	rcTimeVal simplifyEndTime = rcGetPerformanceTimer();
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
//	if (rcGetLog())
//	{
//		rcGetLog()->log(RC_LOG_PROGRESS, "Create contours: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
//		rcGetLog()->log(RC_LOG_PROGRESS, " - boundary: %.3f ms", rcGetDeltaTimeUsec(boundaryStartTime, boundaryEndTime)/1000.0f);
//		rcGetLog()->log(RC_LOG_PROGRESS, " - contour: %.3f ms", rcGetDeltaTimeUsec(contourStartTime, contourEndTime)/1000.0f);
//	}

	if (rcGetBuildTimes())
	{
		rcGetBuildTimes()->buildContours += rcGetDeltaTimeUsec(startTime, endTime);
		rcGetBuildTimes()->buildContoursTrace += rcGetDeltaTimeUsec(traceStartTime, traceEndTime);
		rcGetBuildTimes()->buildContoursSimplify += rcGetDeltaTimeUsec(simplifyStartTime, simplifyEndTime);
	}
	
	return true;
}
