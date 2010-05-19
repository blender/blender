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
#include <stdio.h>
#include "Recast.h"
#include "RecastLog.h"
#include "RecastTimer.h"


void rcFilterLedgeSpans(const int walkableHeight,
						const int walkableClimb,
						rcHeightfield& solid)
{
	rcTimeVal startTime = rcGetPerformanceTimer();

	const int w = solid.width;
	const int h = solid.height;
	const int MAX_HEIGHT = 0xffff;
	
	// Mark border spans.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			for (rcSpan* s = solid.spans[x + y*w]; s; s = s->next)
			{
				// Skip non walkable spans.
				if ((s->flags & RC_WALKABLE) == 0)
					continue;
				
				const int bot = (int)s->smax;
				const int top = (int)s->next ? (int)s->next->smin : MAX_HEIGHT;
				
				// Find neighbours minimum height.
				int minh = MAX_HEIGHT;

				for (int dir = 0; dir < 4; ++dir)
				{
					int dx = x + rcGetDirOffsetX(dir);
					int dy = y + rcGetDirOffsetY(dir);
					// Skip neighbours which are out of bounds.
					if (dx < 0 || dy < 0 || dx >= w || dy >= h)
					{
						minh = rcMin(minh, -walkableClimb - bot);
						continue;
					}

					// From minus infinity to the first span.
					rcSpan* ns = solid.spans[dx + dy*w];
					int nbot = -walkableClimb;
					int ntop = ns ? (int)ns->smin : MAX_HEIGHT;
					// Skip neightbour if the gap between the spans is too small.
					if (rcMin(top,ntop) - rcMax(bot,nbot) > walkableHeight)
						minh = rcMin(minh, nbot - bot);
					
					// Rest of the spans.
					for (ns = solid.spans[dx + dy*w]; ns; ns = ns->next)
					{
						nbot = (int)ns->smax;
						ntop = (int)ns->next ? (int)ns->next->smin : MAX_HEIGHT;
						// Skip neightbour if the gap between the spans is too small.
						if (rcMin(top,ntop) - rcMax(bot,nbot) > walkableHeight)
							minh = rcMin(minh, nbot - bot);
					}
				}
				
				// The current span is close to a ledge if the drop to any
				// neighbour span is less than the walkableClimb.
				if (minh < -walkableClimb)
					s->flags &= ~RC_WALKABLE;
				
			}
		}
	}
	
	rcTimeVal endTime = rcGetPerformanceTimer();
//	if (rcGetLog())
//		rcGetLog()->log(RC_LOG_PROGRESS, "Filter border: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
	if (rcGetBuildTimes())
		rcGetBuildTimes()->filterBorder += rcGetDeltaTimeUsec(startTime, endTime);
}	

void rcFilterWalkableLowHeightSpans(int walkableHeight,
									rcHeightfield& solid)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	const int w = solid.width;
	const int h = solid.height;
	const int MAX_HEIGHT = 0xffff;
	
	// Remove walkable flag from spans which do not have enough
	// space above them for the agent to stand there.
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			for (rcSpan* s = solid.spans[x + y*w]; s; s = s->next)
			{
				const int bot = (int)s->smax;
				const int top = (int)s->next ? (int)s->next->smin : MAX_HEIGHT;
				if ((top - bot) <= walkableHeight)
					s->flags &= ~RC_WALKABLE;
			}
		}
	}
	
	rcTimeVal endTime = rcGetPerformanceTimer();

//	if (rcGetLog())
//		rcGetLog()->log(RC_LOG_PROGRESS, "Filter walkable: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
	if (rcGetBuildTimes())
		rcGetBuildTimes()->filterWalkable += rcGetDeltaTimeUsec(startTime, endTime);
}


struct rcReachableSeed
{
	inline void set(int ix, int iy, rcSpan* is)
	{
		x = (unsigned short)ix;
		y = (unsigned short)iy;
		s = is;
	}
	unsigned short x, y;
	rcSpan* s;
};

bool rcMarkReachableSpans(const int walkableHeight,
						  const int walkableClimb,
						  rcHeightfield& solid)
{
	const int w = solid.width;
	const int h = solid.height;
	const int MAX_HEIGHT = 0xffff;
	
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	// Build navigable space.
	const int MAX_SEEDS = w*h;
	rcReachableSeed* stack = new rcReachableSeed[MAX_SEEDS];
	if (!stack)
	{
		if (rcGetLog())
			rcGetLog()->log(RC_LOG_ERROR, "rcMarkReachableSpans: Out of memory 'stack' (%d).", MAX_SEEDS);
		return false;
	}
	int stackSize = 0;
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			rcSpan* topSpan = solid.spans[x + y*w];
			if (!topSpan)
				continue;
			while (topSpan->next)
				topSpan = topSpan->next;
			
			// If the span is not walkable, skip it.
			if ((topSpan->flags & RC_WALKABLE) == 0)
				continue;
			// If the span has been visited already, skip it.
			if (topSpan->flags & RC_REACHABLE)
				continue;
			
			// Start flood fill.
			topSpan->flags |= RC_REACHABLE;
			stackSize = 0;
			stack[stackSize].set(x, y, topSpan);
			stackSize++;
			
			while (stackSize)
			{
				// Pop a seed from the stack.
				stackSize--;
				rcReachableSeed cur = stack[stackSize];
				
				const int bot = (int)cur.s->smax;
				const int top = (int)cur.s->next ? (int)cur.s->next->smin : MAX_HEIGHT;
				
				// Visit neighbours in all 4 directions.
				for (int dir = 0; dir < 4; ++dir)
				{
					int dx = (int)cur.x + rcGetDirOffsetX(dir);
					int dy = (int)cur.y + rcGetDirOffsetY(dir);
					// Skip neighbour which are out of bounds.
					if (dx < 0 || dy < 0 || dx >= w || dy >= h)
						continue;
					for (rcSpan* ns = solid.spans[dx + dy*w]; ns; ns = ns->next)
					{
						// Skip neighbour if it is not walkable.
						if ((ns->flags & RC_WALKABLE) == 0)
							continue;
						// Skip the neighbour if it has been visited already.
						if (ns->flags & RC_REACHABLE)
							continue;
						
						const int nbot = (int)ns->smax;
						const int ntop = (int)ns->next ? (int)ns->next->smin : MAX_HEIGHT;
						// Skip neightbour if the gap between the spans is too small.
						if (rcMin(top,ntop) - rcMax(bot,nbot) < walkableHeight)
							continue;
						// Skip neightbour if the climb height to the neighbour is too high.
						if (rcAbs(nbot - bot) >= walkableClimb)
							continue;
						
						// This neighbour has not been visited yet.
						// Mark it as reachable and add it to the seed stack.
						ns->flags |= RC_REACHABLE;
						if (stackSize < MAX_SEEDS)
						{
							stack[stackSize].set(dx, dy, ns);
							stackSize++;
						}
					}
				}
			}
		}
	}
	
	delete [] stack;	
	
	rcTimeVal endTime = rcGetPerformanceTimer();
	
//	if (rcGetLog())
//		rcGetLog()->log(RC_LOG_PROGRESS, "Mark reachable: %.3f ms", rcGetDeltaTimeUsec(startTime, endTime)/1000.0f);
	if (rcGetBuildTimes())
		rcGetBuildTimes()->filterMarkReachable += rcGetDeltaTimeUsec(startTime, endTime);
	
	return true;
}
