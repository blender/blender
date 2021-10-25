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
#include <float.h>
#include <string.h>
#include <stdio.h>
#include "DetourStatNavMesh.h"
#include "DetourNode.h"
#include "DetourCommon.h"


//////////////////////////////////////////////////////////////////////////////////////////
dtStatNavMesh::dtStatNavMesh() :
	m_data(0),
	m_dataSize(0),
	m_header(0),
	m_nodePool(0),
	m_openList(0)
{
}

dtStatNavMesh::~dtStatNavMesh()
{
	delete m_nodePool;
	delete m_openList;
	if (m_data)
		delete [] m_data;
}

bool dtStatNavMesh::init(unsigned char* data, int dataSize, bool ownsData)
{
	dtStatNavMeshHeader* header = (dtStatNavMeshHeader*)data;
	
	if (header->magic != DT_STAT_NAVMESH_MAGIC)
		return false;
	if (header->version != DT_STAT_NAVMESH_VERSION)
		return false;

	const int headerSize = sizeof(dtStatNavMeshHeader);
	const int vertsSize = sizeof(float)*3*header->nverts;
	const int polysSize = sizeof(dtStatPoly)*header->npolys;
	const int nodesSize = sizeof(dtStatBVNode)*header->npolys*2;
	const int detailMeshesSize = sizeof(dtStatPolyDetail)*header->ndmeshes;
	const int detailVertsSize = sizeof(float)*3*header->ndverts;
	const int detailTrisSize = sizeof(unsigned char)*4*header->ndtris;
	

	unsigned char* d = data + headerSize;
	header->verts = (float*)d; d += vertsSize;
	header->polys = (dtStatPoly*)d; d += polysSize;
	header->bvtree = (dtStatBVNode*)d; d += nodesSize;
	header->dmeshes = (dtStatPolyDetail*)d; d += detailMeshesSize;
	header->dverts = (float*)d; d += detailVertsSize;
	header->dtris = (unsigned char*)d; d += detailTrisSize;
	
	m_nodePool = new dtNodePool(2048, 256);
	if (!m_nodePool)
		return false;
		
	m_openList = new dtNodeQueue(2048);
	if (!m_openList)
		return false;
	
	if (ownsData)
	{
		m_data = data;
		m_dataSize = dataSize;
	}

	m_header = header;

	return true;
}

const dtStatPoly* dtStatNavMesh::getPolyByRef(dtStatPolyRef ref) const
{
	if (!m_header || ref == 0 || (int)ref > m_header->npolys) return 0;
	return &m_header->polys[ref-1];
}

int dtStatNavMesh::getPolyIndexByRef(dtStatPolyRef ref) const
{
	if (!m_header || ref == 0 || (int)ref > m_header->npolys) return -1;
	return (int)ref-1;
}

int dtStatNavMesh::findPath(dtStatPolyRef startRef, dtStatPolyRef endRef,
							const float* startPos, const float* endPos,
							dtStatPolyRef* path, const int maxPathSize)
{
	if (!m_header) return 0;
	
	if (!startRef || !endRef)
		return 0;

	if (!maxPathSize)
		return 0;

	if (startRef == endRef)
	{
		path[0] = startRef;
		return 1;
	}

	m_nodePool->clear();
	m_openList->clear();

	static const float H_SCALE = 1.1f;	// Heuristic scale.
	
	dtNode* startNode = m_nodePool->getNode(startRef);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = vdist(startPos, endPos) * H_SCALE;
	startNode->id = startRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);

	dtNode* lastBestNode = startNode;
	float lastBestNodeCost = startNode->total;
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
	
		if (bestNode->id == endRef)
		{
			lastBestNode = bestNode;
			break;
		}

		const dtStatPoly* poly = getPoly(bestNode->id-1);
		for (int i = 0; i < (int)poly->nv; ++i)
		{
			dtStatPolyRef neighbour = poly->n[i];
			if (neighbour)
			{
				// Skip parent node.
				if (bestNode->pidx && m_nodePool->getNodeAtIdx(bestNode->pidx)->id == neighbour)
					continue;

				dtNode* parent = bestNode;
				dtNode newNode;
				newNode.pidx = m_nodePool->getNodeIdx(parent);
				newNode.id = neighbour;

				// Calculate cost.
				float p0[3], p1[3];
				if (!parent->pidx)
					vcopy(p0, startPos);
				else
					getEdgeMidPoint(m_nodePool->getNodeAtIdx(parent->pidx)->id, parent->id, p0);
				getEdgeMidPoint(parent->id, newNode.id, p1);
				newNode.cost = parent->cost + vdist(p0,p1);
				// Special case for last node.
				if (newNode.id == endRef)
					newNode.cost += vdist(p1, endPos);
				
				// Heuristic
				const float h = vdist(p1,endPos)*H_SCALE;
				newNode.total = newNode.cost + h;
				
				dtNode* actualNode = m_nodePool->getNode(newNode.id);
				if (!actualNode)
					continue;
						
				if (!((actualNode->flags & DT_NODE_OPEN) && newNode.total > actualNode->total) &&
					!((actualNode->flags & DT_NODE_CLOSED) && newNode.total > actualNode->total))
				{
					actualNode->flags &= ~DT_NODE_CLOSED;
					actualNode->pidx = newNode.pidx;
					actualNode->cost = newNode.cost;
					actualNode->total = newNode.total;

					if (h < lastBestNodeCost)
					{
						lastBestNodeCost = h;
						lastBestNode = actualNode;
					}

					if (actualNode->flags & DT_NODE_OPEN)
					{
						m_openList->modify(actualNode);
					}
					else
					{
						actualNode->flags |= DT_NODE_OPEN;
						m_openList->push(actualNode);
					}
				}
			}
		}
		bestNode->flags |= DT_NODE_CLOSED;
	}

	// Reverse the path.
	dtNode* prev = 0;
	dtNode* node = lastBestNode;
	do
	{
		dtNode* next = m_nodePool->getNodeAtIdx(node->pidx);
		node->pidx = m_nodePool->getNodeIdx(prev);
		prev = node;
		node = next;
	}
	while (node);
	
	// Store path
	node = prev;
	int n = 0;
	do
	{
		path[n++] = node->id;
		node = m_nodePool->getNodeAtIdx(node->pidx);
	}
	while (node && n < maxPathSize);

	return n;
}

bool dtStatNavMesh::closestPointToPoly(dtStatPolyRef ref, const float* pos, float* closest) const
{
	int idx = getPolyIndexByRef(ref);
	if (idx == -1)
		return false;

	float closestDistSqr = FLT_MAX;
	const dtStatPoly* p = getPoly(idx);
	const dtStatPolyDetail* pd = getPolyDetail(idx);

	for (int j = 0; j < pd->ntris; ++j)
	{
		const unsigned char* t = getDetailTri(pd->tbase+j);
		const float* v[3];
		for (int k = 0; k < 3; ++k)
		{
			if (t[k] < p->nv)
				v[k] = getVertex(p->v[t[k]]);
			else
				v[k] = getDetailVertex(pd->vbase+(t[k]-p->nv));
		}
		float pt[3];
		closestPtPointTriangle(pt, pos, v[0], v[1], v[2]);
		float d = vdistSqr(pos, pt);
		if (d < closestDistSqr)
		{
			vcopy(closest, pt);
			closestDistSqr = d;
		}
	}
	
	return true;
}

bool dtStatNavMesh::getPolyHeight(dtStatPolyRef ref, const float* pos, float* height) const
{
	int idx = getPolyIndexByRef(ref);
	if (idx == -1)
		return false;
	
	const dtStatPoly* p = getPoly(idx);
	const dtStatPolyDetail* pd = getPolyDetail(idx);

	for (int i = 0; i < pd->ntris; ++i)
	{
		const unsigned char* t = getDetailTri(pd->tbase+i);
		const float* v[3];
		for (int j = 0; j < 3; ++j)
		{
			if (t[j] < p->nv)
				v[j] = getVertex(p->v[t[j]]);
			else
				v[j] = getDetailVertex(pd->vbase+(t[j]-p->nv));
		}
		float h;
		if (closestHeightPointTriangle(pos, v[0], v[1], v[2], h))
		{
			if (height)
				*height = h;
			return true;
		}
	}
	
	return false;
}

int dtStatNavMesh::findStraightPath(const float* startPos, const float* endPos,
									const dtStatPolyRef* path, const int pathSize,
									float* straightPath, const int maxStraightPathSize)
{
	if (!m_header) return 0;
	
	if (!maxStraightPathSize)
		return 0;

	if (!path[0])
		return 0;

	int straightPathSize = 0;
	
	float closestStartPos[3];
	if (!closestPointToPoly(path[0], startPos, closestStartPos))
		return 0;

	// Add start point.
	vcopy(&straightPath[straightPathSize*3], closestStartPos);
	straightPathSize++;
	if (straightPathSize >= maxStraightPathSize)
		return straightPathSize;

	float closestEndPos[3];
	if (!closestPointToPoly(path[pathSize-1], endPos, closestEndPos))
		return 0;

	float portalApex[3], portalLeft[3], portalRight[3];

	if (pathSize > 1)
	{
		vcopy(portalApex, closestStartPos);
		vcopy(portalLeft, portalApex);
		vcopy(portalRight, portalApex);
		int apexIndex = 0;
		int leftIndex = 0;
		int rightIndex = 0;

		for (int i = 0; i < pathSize; ++i)
		{
			float left[3], right[3];
			if (i < pathSize-1)
			{
				// Next portal.
				getPortalPoints(path[i], path[i+1], left, right);
			}
			else
			{
				// End of the path.
				vcopy(left, closestEndPos);
				vcopy(right, closestEndPos);
			}

			// Right vertex.
			if (vequal(portalApex, portalRight))
			{
				vcopy(portalRight, right);
				rightIndex = i;
			}
			else
			{
				if (triArea2D(portalApex, portalRight, right) <= 0.0f)
				{
					if (triArea2D(portalApex, portalLeft, right) > 0.0f)
					{
						vcopy(portalRight, right);
						rightIndex = i;
					}
					else
					{
						vcopy(portalApex, portalLeft);
						apexIndex = leftIndex;

						if (!vequal(&straightPath[(straightPathSize-1)*3], portalApex))
						{
							vcopy(&straightPath[straightPathSize*3], portalApex);
							straightPathSize++;
							if (straightPathSize >= maxStraightPathSize)
								return straightPathSize;
						}

						vcopy(portalLeft, portalApex);
						vcopy(portalRight, portalApex);
						leftIndex = apexIndex;
						rightIndex = apexIndex;

						// Restart
						i = apexIndex;

						continue;
					}
				}
			}

			// Left vertex.
			if (vequal(portalApex, portalLeft))
			{
				vcopy(portalLeft, left);
				leftIndex = i;
			}
			else
			{
				if (triArea2D(portalApex, portalLeft, left) >= 0.0f)
				{
					if (triArea2D(portalApex, portalRight, left) < 0.0f)
					{
						vcopy(portalLeft, left);
						leftIndex = i;
					}
					else
					{
						vcopy(portalApex, portalRight);
						apexIndex = rightIndex;

						if (!vequal(&straightPath[(straightPathSize-1)*3], portalApex))
						{
							vcopy(&straightPath[straightPathSize*3], portalApex);
							straightPathSize++;
							if (straightPathSize >= maxStraightPathSize)
								return straightPathSize;
						}

						vcopy(portalLeft, portalApex);
						vcopy(portalRight, portalApex);
						leftIndex = apexIndex;
						rightIndex = apexIndex;

						// Restart
						i = apexIndex;

						continue;
					}
				}
			}
		}
	}

	// Add end point.
	vcopy(&straightPath[straightPathSize*3], closestEndPos);
	straightPathSize++;
	
	return straightPathSize;
}

int dtStatNavMesh::getPolyVerts(dtStatPolyRef ref, float* verts) const
{
	if (!m_header) return 0;
	const dtStatPoly* poly = getPolyByRef(ref);
	if (!poly) return 0;
	float* v = verts;
	for (int i = 0; i < (int)poly->nv; ++i)
	{
		const float* cv = &m_header->verts[poly->v[i]*3];
		*v++ = cv[0];
		*v++ = cv[1];
		*v++ = cv[2];
	}
	return (int)poly->nv;
}

int dtStatNavMesh::raycast(dtStatPolyRef centerRef, const float* startPos, const float* endPos,
					  float& t, dtStatPolyRef* path, const int pathSize)
{
	if (!m_header) return 0;
	if (!centerRef) return 0;
	
	/* dtStatPolyRef prevRef = centerRef; */ /* UNUSED */
	dtStatPolyRef curRef = centerRef;
	t = 0;

	float verts[DT_STAT_VERTS_PER_POLYGON*3];
	int n = 0;

	while (curRef)
	{
		// Cast ray against current polygon.
		int nv = getPolyVerts(curRef, verts);
		if (nv < 3)
		{
			// Hit bad polygon, report hit.
			return n;
		}
		
		float tmin, tmax;
		int segMin, segMax;
		if (!intersectSegmentPoly2D(startPos, endPos, verts, nv, tmin, tmax, segMin, segMax))
		{
			// Could not a polygon, keep the old t and report hit.
			return n;
		}
		// Keep track of furthest t so far.
		if (tmax > t)
			t = tmax;

		if (n < pathSize)
			path[n++] = curRef;

		// Check the neighbour of this polygon.
		const dtStatPoly* poly = getPolyByRef(curRef);
		dtStatPolyRef nextRef = poly->n[segMax];
		if (!nextRef)
		{
			// No neighbour, we hit a wall.
			return n;
		}
		
		// No hit, advance to neighbour polygon.
		/* prevRef = curRef; */ /* UNUSED */
		curRef = nextRef;
	}
	
	return n;
}


float dtStatNavMesh::findDistanceToWall(dtStatPolyRef centerRef, const float* centerPos, float maxRadius,
								  float* hitPos, float* hitNormal)
{
	if (!m_header) return 0;
	if (!centerRef) return 0;
	
	m_nodePool->clear();
	m_openList->clear();
	
	dtNode* startNode = m_nodePool->getNode(centerRef);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = centerRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);
	
	float radiusSqr = sqr(maxRadius);
	
	hitNormal[0] = 1;
	hitNormal[1] = 0;
	hitNormal[2] = 0;
	
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		const dtStatPoly* poly = getPoly(bestNode->id-1);
		
		// Hit test walls.
		for (int i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j = i++)
		{
			// Skip non-solid edges.
			if (poly->n[j]) continue;
			
			// Calc distance to the edge.
			const float* vj = getVertex(poly->v[j]);
			const float* vi = getVertex(poly->v[i]);
			float tseg;
			float distSqr = distancePtSegSqr2D(centerPos, vj, vi, tseg);

			// Edge is too far, skip.
			if (distSqr > radiusSqr)
				continue;
				
			// Hit wall, update radius.
			radiusSqr = distSqr;
			// Calculate hit pos.
			hitPos[0] = vj[0] + (vi[0] - vj[0])*tseg;
			hitPos[1] = vj[1] + (vi[1] - vj[1])*tseg;
			hitPos[2] = vj[2] + (vi[2] - vj[2])*tseg;
		}

		// Check to see if the circle expands to one of the neighbours and expand.
		for (int i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j = i++)
		{
			// Skip solid edges.
			if (!poly->n[j]) continue;
			
			// Expand to neighbour if not visited yet.
			dtStatPolyRef neighbour = poly->n[j];
			
			// Skip parent node.
			if (bestNode->pidx && m_nodePool->getNodeAtIdx(bestNode->pidx)->id == neighbour)
				continue;
			
			// Calc distance to the edge.
			const float* vj = getVertex(poly->v[j]);
			const float* vi = getVertex(poly->v[i]);
			float tseg;
			float distSqr = distancePtSegSqr2D(centerPos, vj, vi, tseg);
			
			// Edge is too far, skip.
			if (distSqr > radiusSqr)
				continue;
			
			dtNode* parent = bestNode;
			dtNode newNode;
			newNode.pidx = m_nodePool->getNodeIdx(parent);
			newNode.id = neighbour;
			
			// Cost
			float p0[3], p1[3];
			if (!parent->pidx)
				vcopy(p0, centerPos);
			else
				getEdgeMidPoint(m_nodePool->getNodeAtIdx(parent->pidx)->id, parent->id, p0);
			getEdgeMidPoint(parent->id, newNode.id, p1);
			newNode.total = parent->total + vdist(p0,p1);
			
			dtNode* actualNode = m_nodePool->getNode(newNode.id);
			if (!actualNode)
				continue;

			if (!((actualNode->flags & DT_NODE_OPEN) && newNode.total > actualNode->total) &&
				!((actualNode->flags & DT_NODE_CLOSED) && newNode.total > actualNode->total))
			{
				actualNode->flags &= ~DT_NODE_CLOSED;
				actualNode->pidx = newNode.pidx;
				actualNode->total = newNode.total;
				
				if (actualNode->flags & DT_NODE_OPEN)
				{
					m_openList->modify(actualNode);
				}
				else
				{
					actualNode->flags |= DT_NODE_OPEN;
					m_openList->push(actualNode);
				}
			}
		}
		bestNode->flags |= DT_NODE_CLOSED;
	}

	// Calc hit normal.
	vsub(hitNormal, centerPos, hitPos);
	vnormalize(hitNormal);
	
	return sqrtf(radiusSqr);
}

int dtStatNavMesh::findPolysAround(dtStatPolyRef centerRef, const float* centerPos, float radius,
								   dtStatPolyRef* resultRef, dtStatPolyRef* resultParent, float* resultCost,
								   const int maxResult)
{
	if (!m_header) return 0;
	if (!centerRef) return 0;

	m_nodePool->clear();
	m_openList->clear();

	dtNode* startNode = m_nodePool->getNode(centerRef);
	startNode->pidx = 0;
	startNode->cost = 0;
	startNode->total = 0;
	startNode->id = centerRef;
	startNode->flags = DT_NODE_OPEN;
	m_openList->push(startNode);

	int n = 0;
	if (n < maxResult)
	{
		if (resultRef)
			resultRef[n] = startNode->id;
		if (resultParent)
			resultParent[n] = 0;
		if (resultCost)
			resultCost[n] = 0;
		++n;
	}

	const float radiusSqr = sqr(radius);

	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		const dtStatPoly* poly = getPoly(bestNode->id-1);
		for (unsigned i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j=i++)
		{
			dtStatPolyRef neighbour = poly->n[j];

			if (neighbour)
			{
				// Skip parent node.
				if (bestNode->pidx && m_nodePool->getNodeAtIdx(bestNode->pidx)->id == neighbour)
					continue;
					
				// Calc distance to the edge.
				const float* vj = getVertex(poly->v[j]);
				const float* vi = getVertex(poly->v[i]);
				float tseg;
				float distSqr = distancePtSegSqr2D(centerPos, vj, vi, tseg);
				
				// If the circle is not touching the next polygon, skip it.
				if (distSqr > radiusSqr)
					continue;
				
				dtNode* parent = bestNode;
				dtNode newNode;
				newNode.pidx = m_nodePool->getNodeIdx(parent);
				newNode.id = neighbour;

				// Cost
				float p0[3], p1[3];
				if (!parent->pidx)
					vcopy(p0, centerPos);
				else
					getEdgeMidPoint(m_nodePool->getNodeAtIdx(parent->pidx)->id, parent->id, p0);
				getEdgeMidPoint(parent->id, newNode.id, p1);
				newNode.total = parent->total + vdist(p0,p1);
				
				dtNode* actualNode = m_nodePool->getNode(newNode.id);
				if (!actualNode)
					continue;

				if (!((actualNode->flags & DT_NODE_OPEN) && newNode.total > actualNode->total) &&
					!((actualNode->flags & DT_NODE_CLOSED) && newNode.total > actualNode->total))
				{
					actualNode->flags &= ~DT_NODE_CLOSED;
					actualNode->pidx = newNode.pidx;
					actualNode->total = newNode.total;

					if (actualNode->flags & DT_NODE_OPEN)
					{
						m_openList->modify(actualNode);
					}
					else
					{
						if (n < maxResult)
						{
							if (resultRef)
								resultRef[n] = actualNode->id;
							if (resultParent)
								resultParent[n] = m_nodePool->getNodeAtIdx(actualNode->pidx)->id;
							if (resultCost)
								resultCost[n] = actualNode->total;
							++n;
						}
						actualNode->flags |= DT_NODE_OPEN;
						m_openList->push(actualNode);
					}
				}
			}
		}
		bestNode->flags |= DT_NODE_CLOSED;
		
	}

	return n;
}

// Returns polygons which are withing certain radius from the query location.
int dtStatNavMesh::queryPolygons(const float* center, const float* extents,
								 dtStatPolyRef* polys, const int maxIds)
{
	if (!m_header) return 0;
	
	const dtStatBVNode* node = &m_header->bvtree[0];
	const dtStatBVNode* end = &m_header->bvtree[m_header->nnodes];

	// Calculate quantized box
	const float ics = 1.0f / m_header->cs;
	unsigned short bmin[3], bmax[3];
	// Clamp query box to world box.
	float minx = clamp(center[0] - extents[0], m_header->bmin[0], m_header->bmax[0]) - m_header->bmin[0];
	float miny = clamp(center[1] - extents[1], m_header->bmin[1], m_header->bmax[1]) - m_header->bmin[1];
	float minz = clamp(center[2] - extents[2], m_header->bmin[2], m_header->bmax[2]) - m_header->bmin[2];
	float maxx = clamp(center[0] + extents[0], m_header->bmin[0], m_header->bmax[0]) - m_header->bmin[0];
	float maxy = clamp(center[1] + extents[1], m_header->bmin[1], m_header->bmax[1]) - m_header->bmin[1];
	float maxz = clamp(center[2] + extents[2], m_header->bmin[2], m_header->bmax[2]) - m_header->bmin[2];
	// Quantize
	bmin[0] = (unsigned short)(ics * minx) & 0xfffe;
	bmin[1] = (unsigned short)(ics * miny) & 0xfffe;
	bmin[2] = (unsigned short)(ics * minz) & 0xfffe;
	bmax[0] = (unsigned short)(ics * maxx + 1) | 1;
	bmax[1] = (unsigned short)(ics * maxy + 1) | 1;
	bmax[2] = (unsigned short)(ics * maxz + 1) | 1;
	
	// Traverse tree
	int n = 0;
	while (node < end)
	{
		bool overlap = checkOverlapBox(bmin, bmax, node->bmin, node->bmax);
		bool isLeafNode = node->i >= 0;
		
		if (isLeafNode && overlap)
		{
			if (n < maxIds)
			{
				polys[n] = (dtStatPolyRef)node->i;
				n++;
			}
		}
		
		if (overlap || isLeafNode)
			node++;
		else
		{
			const int escapeIndex = -node->i;
			node += escapeIndex;
		}
	}
	
	return n;
}

dtStatPolyRef dtStatNavMesh::findNearestPoly(const float* center, const float* extents)
{
	if (!m_header) return 0;
	
	// Get nearby polygons from proximity grid.
	dtStatPolyRef polys[128];
	int npolys = queryPolygons(center, extents, polys, 128);

	// Find nearest polygon amongst the nearby polygons.
	dtStatPolyRef nearest = 0;
	float nearestDistanceSqr = FLT_MAX;
	for (int i = 0; i < npolys; ++i)
	{
		dtStatPolyRef ref = polys[i];
		float closest[3];
		if (!closestPointToPoly(ref, center, closest))
			continue;
		float d = vdistSqr(center, closest);
		if (d < nearestDistanceSqr)
		{
			nearestDistanceSqr = d;
			nearest = ref;
		}
	}

	return nearest;
}

bool dtStatNavMesh::getPortalPoints(dtStatPolyRef from, dtStatPolyRef to, float* left, float* right) const
{
	const dtStatPoly* fromPoly = getPolyByRef(from);
	if (!fromPoly)
		return false;

	// Find common edge between the polygons and returns the segment end points.
	for (unsigned i = 0, j = (int)fromPoly->nv - 1; i < (int)fromPoly->nv; j = i++)
	{
		unsigned short neighbour = fromPoly->n[j];
		if (neighbour == to)
		{
			vcopy(left, getVertex(fromPoly->v[j]));
			vcopy(right, getVertex(fromPoly->v[i]));
			return true;
		}
	}

	return false;
}

bool dtStatNavMesh::getEdgeMidPoint(dtStatPolyRef from, dtStatPolyRef to, float* mid) const
{
	float left[3], right[3];
	if (!getPortalPoints(from, to, left,right)) return false;
	mid[0] = (left[0]+right[0])*0.5f;
	mid[1] = (left[1]+right[1])*0.5f;
	mid[2] = (left[2]+right[2])*0.5f;
	return true;
}

bool dtStatNavMesh::isInClosedList(dtStatPolyRef ref) const
{
	if (!m_nodePool) return false;
	const dtNode* node = m_nodePool->findNode(ref);
	return node && node->flags & DT_NODE_CLOSED;
}

int dtStatNavMesh::getMemUsed() const
{
	if (!m_nodePool || ! m_openList)
		return 0;
	return sizeof(*this) + m_dataSize +
			m_nodePool->getMemUsed() +
			m_openList->getMemUsed();
}	
