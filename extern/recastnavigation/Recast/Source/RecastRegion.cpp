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


static unsigned short* calculateDistanceField(rcCompactHeightfield& chf,
											  unsigned short* src, unsigned short* dst,
											  unsigned short& maxDist)
{
	const int w = chf.width;
	const int h = chf.height;
	
	// Init distance and points.
	for (int i = 0; i < chf.spanCount; ++i)
		src[i] = 0xffff;
	
	// Mark boundary cells.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				int nc = 0;
				for (int dir = 0; dir < 4; ++dir)
				{
					if (rcGetCon(s, dir) != 0xf)
						nc++;
				}
				if (nc != 4)
					src[i] = 0;
			}
		}
	}
	
	// Pass 1
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				
				if (rcGetCon(s, 0) != 0xf)
				{
					// (-1,0)
					const int ax = x + rcGetDirOffsetX(0);
					const int ay = y + rcGetDirOffsetY(0);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 0);
					const rcCompactSpan& as = chf.spans[ai];
					if (src[ai]+2 < src[i])
						src[i] = src[ai]+2;
					
					// (-1,-1)
					if (rcGetCon(as, 3) != 0xf)
					{
						const int aax = ax + rcGetDirOffsetX(3);
						const int aay = ay + rcGetDirOffsetY(3);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 3);
						if (src[aai]+3 < src[i])
							src[i] = src[aai]+3;
					}
				}
				if (rcGetCon(s, 3) != 0xf)
				{
					// (0,-1)
					const int ax = x + rcGetDirOffsetX(3);
					const int ay = y + rcGetDirOffsetY(3);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 3);
					const rcCompactSpan& as = chf.spans[ai];
					if (src[ai]+2 < src[i])
						src[i] = src[ai]+2;
					
					// (1,-1)
					if (rcGetCon(as, 2) != 0xf)
					{
						const int aax = ax + rcGetDirOffsetX(2);
						const int aay = ay + rcGetDirOffsetY(2);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 2);
						if (src[aai]+3 < src[i])
							src[i] = src[aai]+3;
					}
				}
			}
		}
	}
	
	// Pass 2
	for (int y = h-1; y >= 0; --y)
	{
		for (int x = w-1; x >= 0; --x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				
				if (rcGetCon(s, 2) != 0xf)
				{
					// (1,0)
					const int ax = x + rcGetDirOffsetX(2);
					const int ay = y + rcGetDirOffsetY(2);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 2);
					const rcCompactSpan& as = chf.spans[ai];
					if (src[ai]+2 < src[i])
						src[i] = src[ai]+2;
					
					// (1,1)
					if (rcGetCon(as, 1) != 0xf)
					{
						const int aax = ax + rcGetDirOffsetX(1);
						const int aay = ay + rcGetDirOffsetY(1);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 1);
						if (src[aai]+3 < src[i])
							src[i] = src[aai]+3;
					}
				}
				if (rcGetCon(s, 1) != 0xf)
				{
					// (0,1)
					const int ax = x + rcGetDirOffsetX(1);
					const int ay = y + rcGetDirOffsetY(1);
					const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, 1);
					const rcCompactSpan& as = chf.spans[ai];
					if (src[ai]+2 < src[i])
						src[i] = src[ai]+2;
					
					// (-1,1)
					if (rcGetCon(as, 0) != 0xf)
					{
						const int aax = ax + rcGetDirOffsetX(0);
						const int aay = ay + rcGetDirOffsetY(0);
						const int aai = (int)chf.cells[aax+aay*w].index + rcGetCon(as, 0);
						if (src[aai]+3 < src[i])
							src[i] = src[aai]+3;
					}
				}
			}
		}
	}	
	
	maxDist = 0;
	for (int i = 0; i < chf.spanCount; ++i)
		maxDist = rcMax(src[i], maxDist);
	
	return src;
	
}

static unsigned short* boxBlur(rcCompactHeightfield& chf, int thr,
							   unsigned short* src, unsigned short* dst)
{
	const int w = chf.width;
	const int h = chf.height;
	
	thr *= 2;
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				int cd = (int)src[i];
				if (cd <= thr)
				{
					dst[i] = cd;
					continue;
				}

				int d = cd;
				for (int dir = 0; dir < 4; ++dir)
				{
					if (rcGetCon(s, dir) != 0xf)
					{
						const int ax = x + rcGetDirOffsetX(dir);
						const int ay = y + rcGetDirOffsetY(dir);
						const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, dir);
						d += (int)src[ai];
						
						const rcCompactSpan& as = chf.spans[ai];
						const int dir2 = (dir+1) & 0x3;
						if (rcGetCon(as, dir2) != 0xf)
						{
							const int ax2 = ax + rcGetDirOffsetX(dir2);
							const int ay2 = ay + rcGetDirOffsetY(dir2);
							const int ai2 = (int)chf.cells[ax2+ay2*w].index + rcGetCon(as, dir2);
							d += (int)src[ai2];
						}
						else
						{
							d += cd;
						}
					}
					else
					{
						d += cd*2;
					}
				}
				dst[i] = (unsigned short)((d+5)/9);
			}
		}
	}
	return dst;
}


static bool floodRegion(int x, int y, int i,
						unsigned short level, unsigned short minLevel, unsigned short r,
						rcCompactHeightfield& chf,
						unsigned short* src,
						rcIntArray& stack)
{
	const int w = chf.width;
	
	// Flood fill mark region.
	stack.resize(0);
	stack.push((int)x);
	stack.push((int)y);
	stack.push((int)i);
	src[i*2] = r;
	src[i*2+1] = 0;
	
	unsigned short lev = level >= minLevel+2 ? level-2 : minLevel;
	int count = 0;
	
	while (stack.size() > 0)
	{
		int ci = stack.pop();
		int cy = stack.pop();
		int cx = stack.pop();
		
		const rcCompactSpan& cs = chf.spans[ci];
		
		// Check if any of the neighbours already have a valid region set.
		unsigned short ar = 0;
		for (int dir = 0; dir < 4; ++dir)
		{
			// 8 connected
			if (rcGetCon(cs, dir) != 0xf)
			{
				const int ax = cx + rcGetDirOffsetX(dir);
				const int ay = cy + rcGetDirOffsetY(dir);
				const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(cs, dir);
				unsigned short nr = src[ai*2];
				if (nr != 0 && nr != r)
					ar = nr;
				
				const rcCompactSpan& as = chf.spans[ai];
				
				const int dir2 = (dir+1) & 0x3;
				if (rcGetCon(as, dir2) != 0xf)
				{
					const int ax2 = ax + rcGetDirOffsetX(dir2);
					const int ay2 = ay + rcGetDirOffsetY(dir2);
					const int ai2 = (int)chf.cells[ax2+ay2*w].index + rcGetCon(as, dir2);
					
					unsigned short nr = src[ai2*2];
					if (nr != 0 && nr != r)
						ar = nr;
				}				
			}
		}
		if (ar != 0)
		{
			src[ci*2] = 0;
			continue;
		}
		count++;
		
		// Expand neighbours.
		for (int dir = 0; dir < 4; ++dir)
		{
			if (rcGetCon(cs, dir) != 0xf)
			{
				const int ax = cx + rcGetDirOffsetX(dir);
				const int ay = cy + rcGetDirOffsetY(dir);
				const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(cs, dir);
				if (chf.spans[ai].dist >= lev)
				{
					if (src[ai*2] == 0)
					{
						src[ai*2] = r;
						src[ai*2+1] = 0;
						stack.push(ax);
						stack.push(ay);
						stack.push(ai);
					}
				}
			}
		}
	}
	
	return count > 0;
}

static unsigned short* expandRegions(int maxIter, unsigned short level,
									 rcCompactHeightfield& chf,
									 unsigned short* src,
									 unsigned short* dst,
									 rcIntArray& stack)
{
	const int w = chf.width;
	const int h = chf.height;

	// Find cells revealed by the raised level.
	stack.resize(0);
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				if (chf.spans[i].dist >= level && src[i*2] == 0)
				{
					stack.push(x);
					stack.push(y);
					stack.push(i);
				}
			}
		}
	}
	
	int iter = 0;
	while (stack.size() > 0)
	{
		int failed = 0;
		
		memcpy(dst, src, sizeof(unsigned short)*chf.spanCount*2);
		
		for (int j = 0; j < stack.size(); j += 3)
		{
			int x = stack[j+0];
			int y = stack[j+1];
			int i = stack[j+2];
			if (i < 0)
			{
				failed++;
				continue;
			}
			
			unsigned short r = src[i*2];
			unsigned short d2 = 0xffff;
			const rcCompactSpan& s = chf.spans[i];
			for (int dir = 0; dir < 4; ++dir)
			{
				if (rcGetCon(s, dir) == 0xf) continue;
				const int ax = x + rcGetDirOffsetX(dir);
				const int ay = y + rcGetDirOffsetY(dir);
				const int ai = (int)chf.cells[ax+ay*w].index + rcGetCon(s, dir);
				if (src[ai*2] > 0 && (src[ai*2] & RC_BORDER_REG) == 0)
				{
					if ((int)src[ai*2+1]+2 < (int)d2)
					{
						r = src[ai*2];
						d2 = src[ai*2+1]+2;
					}
				}
			}
			if (r)
			{
				stack[j+2] = -1; // mark as used
				dst[i*2] = r;
				dst[i*2+1] = d2;
			}
			else
			{
				failed++;
			}
		}
		
		// rcSwap source and dest.
		rcSwap(src, dst);
		
		if (failed*3 == stack.size())
			break;
		
		if (level > 0)
		{
			++iter;
			if (iter >= maxIter)
				break;
		}
	}
	
	return src;
}


struct rcRegion
{
	inline rcRegion() : count(0), id(0), remap(false) {}
	
	int count;
	unsigned short id;
	bool remap;
	rcIntArray connections;
	rcIntArray floors;
};

static void removeAdjacentNeighbours(rcRegion& reg)
{
	// Remove adjacent duplicates.
	for (int i = 0; i < reg.connections.size() && reg.connections.size() > 1; )
	{
		int ni = (i+1) % reg.connections.size();
		if (reg.connections[i] == reg.connections[ni])
		{
			// Remove duplicate
			for (int j = i; j < reg.connections.size()-1; ++j)
				reg.connections[j] = reg.connections[j+1];
			reg.connections.pop();
		}
		else
			++i;
	}
}

static void replaceNeighbour(rcRegion& reg, unsigned short oldId, unsigned short newId)
{
	bool neiChanged = false;
	for (int i = 0; i < reg.connections.size(); ++i)
	{
		if (reg.connections[i] == oldId)
		{
			reg.connections[i] = newId;
			neiChanged = true;
		}
	}
	for (int i = 0; i < reg.floors.size(); ++i)
	{
		if (reg.floors[i] == oldId)
			reg.floors[i] = newId;
	}
	if (neiChanged)
		removeAdjacentNeighbours(reg);
}

static bool canMergeWithRegion(rcRegion& reg, unsigned short id)
{
	int n = 0;
	for (int i = 0; i < reg.connections.size(); ++i)
	{
		if (reg.connections[i] == id)
			n++;
	}
	if (n > 1)
		return false;
	for (int i = 0; i < reg.floors.size(); ++i)
	{
		if (reg.floors[i] == id)
			return false;
	}
	return true;
}

static void addUniqueFloorRegion(rcRegion& reg, unsigned short n)
{
	for (int i = 0; i < reg.floors.size(); ++i)
		if (reg.floors[i] == n)
			return;
	reg.floors.push(n);
}

static bool mergeRegions(rcRegion& rega, rcRegion& regb)
{
	unsigned short aid = rega.id;
	unsigned short bid = regb.id;
	
	// Duplicate current neighbourhood.
	rcIntArray acon;
	acon.resize(rega.connections.size());
	for (int i = 0; i < rega.connections.size(); ++i)
		acon[i] = rega.connections[i];
	rcIntArray& bcon = regb.connections;
	
	// Find insertion point on A.
	int insa = -1;
	for (int i = 0; i < acon.size(); ++i)
	{
		if (acon[i] == bid)
		{
			insa = i;
			break;
		}
	}
	if (insa == -1)
		return false;
	
	// Find insertion point on B.
	int insb = -1;
	for (int i = 0; i < bcon.size(); ++i)
	{
		if (bcon[i] == aid)
		{
			insb = i;
			break;
		}
	}
	if (insb == -1)
		return false;
	
	// Merge neighbours.
	rega.connections.resize(0);
	for (int i = 0, ni = acon.size(); i < ni-1; ++i)
		rega.connections.push(acon[(insa+1+i) % ni]);
		
	for (int i = 0, ni = bcon.size(); i < ni-1; ++i)
		rega.connections.push(bcon[(insb+1+i) % ni]);
	
	removeAdjacentNeighbours(rega);
	
	for (int j = 0; j < regb.floors.size(); ++j)
		addUniqueFloorRegion(rega, regb.floors[j]);
	rega.count += regb.count;
	regb.count = 0;
	regb.connections.resize(0);

	return true;
}

static bool isRegionConnectedToBorder(const rcRegion& reg)
{
	// Region is connected to border if
	// one of the neighbours is null id.
	for (int i = 0; i < reg.connections.size(); ++i)
	{
		if (reg.connections[i] == 0)
			return true;
	}
	return false;
}

static bool isSolidEdge(rcCompactHeightfield& chf, unsigned short* src,
						int x, int y, int i, int dir)
{
	const rcCompactSpan& s = chf.spans[i];
	unsigned short r = 0;
	if (rcGetCon(s, dir) != 0xf)
	{
		const int ax = x + rcGetDirOffsetX(dir);
		const int ay = y + rcGetDirOffsetY(dir);
		const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(s, dir);
		r = src[ai*2];
	}
	if (r == src[i*2])
		return false;
	return true;
}

static void walkContour(int x, int y, int i, int dir,
						rcCompactHeightfield& chf,
						unsigned short* src,
						rcIntArray& cont)
{
	int startDir = dir;
	int starti = i;

	const rcCompactSpan& ss = chf.spans[i];
	unsigned short curReg = 0;
	if (rcGetCon(ss, dir) != 0xf)
	{
		const int ax = x + rcGetDirOffsetX(dir);
		const int ay = y + rcGetDirOffsetY(dir);
		const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(ss, dir);
		curReg = src[ai*2];
	}
	cont.push(curReg);
			
	int iter = 0;
	while (++iter < 40000)
	{
		const rcCompactSpan& s = chf.spans[i];
		
		if (isSolidEdge(chf, src, x, y, i, dir))
		{
			// Choose the edge corner
			unsigned short r = 0;
			if (rcGetCon(s, dir) != 0xf)
			{
				const int ax = x + rcGetDirOffsetX(dir);
				const int ay = y + rcGetDirOffsetY(dir);
				const int ai = (int)chf.cells[ax+ay*chf.width].index + rcGetCon(s, dir);
				r = src[ai*2];
			}
			if (r != curReg)
			{
				curReg = r;
				cont.push(curReg);
			}
			
			dir = (dir+1) & 0x3;  // Rotate CW
		}
		else
		{
			int ni = -1;
			const int nx = x + rcGetDirOffsetX(dir);
			const int ny = y + rcGetDirOffsetY(dir);
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

	// Remove adjacent duplicates.
	if (cont.size() > 1)
	{
		for (int i = 0; i < cont.size(); )
		{
			int ni = (i+1) % cont.size();
			if (cont[i] == cont[ni])
			{
				for (int j = i; j < cont.size()-1; ++j)
					cont[j] = cont[j+1];
				cont.pop();
			}
			else
				++i;
		}
	}
}

static bool filterSmallRegions(int minRegionSize, int mergeRegionSize,
							   unsigned short& maxRegionId,
							   rcCompactHeightfield& chf,
							   unsigned short* src)
{
	const int w = chf.width;
	const int h = chf.height;

	int nreg = maxRegionId+1;
	rcRegion* regions = new rcRegion[nreg];
	if (!regions)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "filterSmallRegions: Out of memory 'regions' (%d).", nreg);
		return false;
	}
	
	for (int i = 0; i < nreg; ++i)
		regions[i].id = (unsigned short)i;

	// Find edge of a region and find connections around the contour.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				unsigned short r = src[i*2];
				if (r == 0 || r >= nreg)
					continue;
				
				rcRegion& reg = regions[r];
				reg.count++;
				

				// Update floors.
				for (int j = (int)c.index; j < ni; ++j)
				{
					if (i == j) continue;
					unsigned short floorId = src[j*2];
					if (floorId == 0 || floorId >= nreg)
						continue;
					addUniqueFloorRegion(reg, floorId);
				}
				
				// Have found contour
				if (reg.connections.size() > 0)
					continue;
				
				// Check if this cell is next to a border.
				int ndir = -1;
				for (int dir = 0; dir < 4; ++dir)
				{
					if (isSolidEdge(chf, src, x, y, i, dir))
					{
						ndir = dir;
						break;
					}
				}
				
				if (ndir != -1)
				{
					// The cell is at border.
					// Walk around the contour to find all the neighbours.
					walkContour(x, y, i, ndir, chf, src, reg.connections);
				}
			}
		}
	}
	
	// Remove too small unconnected regions.
	for (int i = 0; i < nreg; ++i)
	{
		rcRegion& reg = regions[i];
		if (reg.id == 0 || (reg.id & RC_BORDER_REG))
			continue;			
		if (reg.count == 0)
			continue;
		
		if (reg.connections.size() == 1 && reg.connections[0] == 0)
		{
			if (reg.count < minRegionSize)
			{
				// Non-connected small region, remove.
				reg.count = 0;
				reg.id = 0;
			}
		}
	}
		
		
	// Merge too small regions to neighbour regions.
	int mergeCount = 0 ;
	do
	{
		mergeCount = 0;
		for (int i = 0; i < nreg; ++i)
		{
			rcRegion& reg = regions[i];
			if (reg.id == 0 || (reg.id & RC_BORDER_REG))
				continue;			
			if (reg.count == 0)
				continue;
				
			// Check to see if the region should be merged.
			if (reg.count > mergeRegionSize && isRegionConnectedToBorder(reg))
				continue;
				
			// Small region with more than 1 connection.
			// Or region which is not connected to a border at all.
			// Find smallest neighbour region that connects to this one.
			int smallest = 0xfffffff;
			unsigned short mergeId = reg.id;
			for (int j = 0; j < reg.connections.size(); ++j)
			{
				if (reg.connections[j] & RC_BORDER_REG) continue;
				rcRegion& mreg = regions[reg.connections[j]];
				if (mreg.id == 0 || (mreg.id & RC_BORDER_REG)) continue;
				if (mreg.count < smallest &&
					canMergeWithRegion(reg, mreg.id) &&
					canMergeWithRegion(mreg, reg.id))
				{
					smallest = mreg.count;
					mergeId = mreg.id;
				}
			}
			// Found new id.
			if (mergeId != reg.id)
			{
				unsigned short oldId = reg.id;
				rcRegion& target = regions[mergeId];
				
				// Merge neighbours.
				if (mergeRegions(target, reg))
				{
					// Fixup regions pointing to current region.
					for (int j = 0; j < nreg; ++j)
					{
						if (regions[j].id == 0 || (regions[j].id & RC_BORDER_REG)) continue;
						// If another region was already merged into current region
						// change the nid of the previous region too.
						if (regions[j].id == oldId)
							regions[j].id = mergeId;
						// Replace the current region with the new one if the
						// current regions is neighbour.
						replaceNeighbour(regions[j], oldId, mergeId);
					}
					mergeCount++;
				}
			}
		}
	}
	while (mergeCount > 0);

	// Compress region Ids.
	for (int i = 0; i < nreg; ++i)
	{
		regions[i].remap = false;
		if (regions[i].id == 0) continue;	// Skip nil regions.
		if (regions[i].id & RC_BORDER_REG) continue;	// Skip external regions.
		regions[i].remap = true;
	}

	unsigned short regIdGen = 0;
	for (int i = 0; i < nreg; ++i)
	{
		if (!regions[i].remap)
			continue;
		unsigned short oldId = regions[i].id;
		unsigned short newId = ++regIdGen;
		for (int j = i; j < nreg; ++j)
		{
			if (regions[j].id == oldId)
			{
				regions[j].id = newId;
				regions[j].remap = false;
			}
		}
	}
	maxRegionId = regIdGen;
		
	// Remap regions.
	for (int i = 0; i < chf.spanCount; ++i)
	{
		if ((src[i*2] & RC_BORDER_REG) == 0)
			src[i*2] = regions[src[i*2]].id;
	}
	
	delete [] regions;
	
	return true;
}

bool rcBuildDistanceField(rcCompactHeightfield& chf)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	unsigned short* dist0 = new unsigned short[chf.spanCount];
	if (!dist0)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildDistanceField: Out of memory 'dist0' (%d).", chf.spanCount);
		return false;
	}
	unsigned short* dist1 = new unsigned short[chf.spanCount];
	if (!dist1)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildDistanceField: Out of memory 'dist1' (%d).", chf.spanCount);
		delete [] dist0;
		return false;
	}
	
	unsigned short* src = dist0;
	unsigned short* dst = dist1;

	unsigned short maxDist = 0;

	rcTimeVal distStartTime = rcGetPerformanceTimer();
	
	if (calculateDistanceField(chf, src, dst, maxDist) != src)
		rcSwap(src, dst);
	
	chf.maxDistance = maxDist;
	
	rcTimeVal distEndTime = rcGetPerformanceTimer();
	
	rcTimeVal blurStartTime = rcGetPerformanceTimer();
	
	// Blur
	if (boxBlur(chf, 1, src, dst) != src)
		rcSwap(src, dst);
	
	// Store distance.
	for (int i = 0; i < chf.spanCount; ++i)
		chf.spans[i].dist = src[i];
	
	rcTimeVal blurEndTime = rcGetPerformanceTimer();
	
	delete [] dist0;
	delete [] dist1;
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
/*	if (rcGetLog())
	{
		rcGetLog()->log(RC_LOG_PROGRESS, "Build distance field: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - dist: %.3f ms", rcGetDeltaTimeUsec(distStartTime, distEndTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - blur: %.3f ms", rcGetDeltaTimeUsec(blurStartTime, blurEndTime)/1000.0f);
	}*/
	if (rcGetBuildTimes())
	{
		rcGetBuildTimes()->buildDistanceField += rcGetDeltaTimeUsec(startTime, endTime);
		rcGetBuildTimes()->buildDistanceFieldDist += rcGetDeltaTimeUsec(distStartTime, distEndTime);
		rcGetBuildTimes()->buildDistanceFieldBlur += rcGetDeltaTimeUsec(blurStartTime, blurEndTime);
	}
	
	return true;
}

static void paintRectRegion(int minx, int maxx, int miny, int maxy,
							unsigned short regId, unsigned short minLevel,
							rcCompactHeightfield& chf, unsigned short* src)
{
	const int w = chf.width;
	for (int y = miny; y < maxy; ++y)
	{
		for (int x = minx; x < maxx; ++x)
		{
			const rcCompactCell& c = chf.cells[x+y*w];
			for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
			{
				if (chf.spans[i].dist >= minLevel)
					src[i*2] = regId;
			}
		}
	}
}

bool rcBuildRegions(rcCompactHeightfield& chf,
					int walkableRadius, int borderSize,
					int minRegionSize, int mergeRegionSize)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	const int w = chf.width;
	const int h = chf.height;
	
	unsigned short* tmp1 = new unsigned short[chf.spanCount*2];
	if (!tmp1)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildDistanceField: Out of memory 'tmp1' (%d).", chf.spanCount*2);
		return false;
	}
	unsigned short* tmp2 = new unsigned short[chf.spanCount*2];
	if (!tmp2)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcBuildDistanceField: Out of memory 'tmp2' (%d).", chf.spanCount*2);
		delete [] tmp1;
		return false;
	}
	
	rcTimeVal regStartTime = rcGetPerformanceTimer();
	
	rcIntArray stack(1024);
	rcIntArray visited(1024);
	
	unsigned short* src = tmp1;
	unsigned short* dst = tmp2;
	
	memset(src, 0, sizeof(unsigned short) * chf.spanCount*2);
	
	unsigned short regionId = 1;
	unsigned short level = (chf.maxDistance+1) & ~1;
	
	unsigned short minLevel = (unsigned short)(walkableRadius*2);
	
	const int expandIters = 4 + walkableRadius * 2;

	// Mark border regions.
	paintRectRegion(0, borderSize, 0, h, regionId|RC_BORDER_REG, minLevel, chf, src); regionId++;
	paintRectRegion(w-borderSize, w, 0, h, regionId|RC_BORDER_REG, minLevel, chf, src); regionId++;
	paintRectRegion(0, w, 0, borderSize, regionId|RC_BORDER_REG, minLevel, chf, src); regionId++;
	paintRectRegion(0, w, h-borderSize, h, regionId|RC_BORDER_REG, minLevel, chf, src); regionId++;

	rcTimeVal expTime = 0;
	rcTimeVal floodTime = 0;
	
	while (level > minLevel)
	{
		level = level >= 2 ? level-2 : 0;
		
		rcTimeVal expStartTime = rcGetPerformanceTimer();
		
		// Expand current regions until no empty connected cells found.
		if (expandRegions(expandIters, level, chf, src, dst, stack) != src)
			rcSwap(src, dst);
		
		expTime += rcGetPerformanceTimer() - expStartTime;
		
		rcTimeVal floodStartTime = rcGetPerformanceTimer();
		
		// Mark new regions with IDs.
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				const rcCompactCell& c = chf.cells[x+y*w];
				for (int i = (int)c.index, ni = (int)(c.index+c.count); i < ni; ++i)
				{
					if (chf.spans[i].dist < level || src[i*2] != 0)
						continue;
					
					if (floodRegion(x, y, i, minLevel, level, regionId, chf, src, stack))
						regionId++;
				}
			}
		}
		
		floodTime += rcGetPerformanceTimer() - floodStartTime;
		
	}
	
	// Expand current regions until no empty connected cells found.
	if (expandRegions(expandIters*8, minLevel, chf, src, dst, stack) != src)
		rcSwap(src, dst);
	
	rcTimeVal regEndTime = rcGetPerformanceTimer();
	
	rcTimeVal filterStartTime = rcGetPerformanceTimer();
	
	// Filter out small regions.
	chf.maxRegions = regionId;
	if (!filterSmallRegions(minRegionSize, mergeRegionSize, chf.maxRegions, chf, src))
		return false;
	
	rcTimeVal filterEndTime = rcGetPerformanceTimer();
	
	// Write the result out.
	for (int i = 0; i < chf.spanCount; ++i)
		chf.spans[i].reg = src[i*2];
	
	delete [] tmp1;
	delete [] tmp2;
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
/*	if (rcGetLog())
	{
		rcGetLog()->log(RC_LOG_PROGRESS, "Build regions: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - reg: %.3f ms", rcGetDeltaTimeUsec(regStartTime, regEndTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - exp: %.3f ms", rcGetDeltaTimeUsec(0, expTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - flood: %.3f ms", rcGetDeltaTimeUsec(0, floodTime)/1000.0f);
		rcGetLog()->log(RC_LOG_PROGRESS, " - filter: %.3f ms", rcGetDeltaTimeUsec(filterStartTime, filterEndTime)/1000.0f);
	}
*/
	if (rcGetBuildTimes())
	{
		rcGetBuildTimes()->buildRegions += rcGetDeltaTimeUsec(startTime, endTime);
		rcGetBuildTimes()->buildRegionsReg += rcGetDeltaTimeUsec(regStartTime, regEndTime);
		rcGetBuildTimes()->buildRegionsExp += rcGetDeltaTimeUsec(0, expTime);
		rcGetBuildTimes()->buildRegionsFlood += rcGetDeltaTimeUsec(0, floodTime);
		rcGetBuildTimes()->buildRegionsFilter += rcGetDeltaTimeUsec(filterStartTime, filterEndTime);
	}
		
	return true;
}


