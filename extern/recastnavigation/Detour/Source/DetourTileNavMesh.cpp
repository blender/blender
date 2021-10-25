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
#include "DetourTileNavMesh.h"
#include "DetourNode.h"
#include "DetourCommon.h"


inline int opposite(int side) { return (side+2) & 0x3; }

inline bool overlapBoxes(const float* amin, const float* amax,
						 const float* bmin, const float* bmax)
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

inline bool overlapRects(const float* amin, const float* amax,
						 const float* bmin, const float* bmax)
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	return overlap;
}

static void calcRect(const float* va, const float* vb,
					 float* bmin, float* bmax,
					 int side, float padx, float pady)
{
	if ((side&1) == 0)
	{
		bmin[0] = min(va[2],vb[2]) + padx;
		bmin[1] = min(va[1],vb[1]) - pady;
		bmax[0] = max(va[2],vb[2]) - padx;
		bmax[1] = max(va[1],vb[1]) + pady;
	}
	else
	{
		bmin[0] = min(va[0],vb[0]) + padx;
		bmin[1] = min(va[1],vb[1]) - pady;
		bmax[0] = max(va[0],vb[0]) - padx;
		bmax[1] = max(va[1],vb[1]) + pady;
	}
}

inline int computeTileHash(int x, int y)
{
	const unsigned int h1 = 0x8da6b343; // Large multiplicative constants;
	const unsigned int h2 = 0xd8163841; // here arbitrarily chosen primes
	unsigned int n = h1 * x + h2 * y;
	return (int)(n & (DT_TILE_LOOKUP_SIZE-1));
}

//////////////////////////////////////////////////////////////////////////////////////////
dtTiledNavMesh::dtTiledNavMesh() :
	m_tileSize(0),
	m_portalHeight(0),
	m_nextFree(0),
	m_tmpLinks(0),
	m_ntmpLinks(0),
	m_nodePool(0),
	m_openList(0)
{
}

dtTiledNavMesh::~dtTiledNavMesh()
{
	for (int i = 0; i < DT_MAX_TILES; ++i)
	{
		if (m_tiles[i].data && m_tiles[i].dataSize < 0)
		{
			delete [] m_tiles[i].data;
			m_tiles[i].data = 0;
			m_tiles[i].dataSize = 0;
		}
	}
	delete [] m_tmpLinks;
	delete m_nodePool;
	delete m_openList;
}
		
bool dtTiledNavMesh::init(const float* orig, float tileSize, float portalHeight)
{
	vcopy(m_orig, orig);
	m_tileSize = tileSize;
	m_portalHeight = portalHeight;
	
	// Init tiles
	memset(m_tiles, 0, sizeof(dtTile)*DT_MAX_TILES);
	memset(m_posLookup, 0, sizeof(dtTile*)*DT_TILE_LOOKUP_SIZE);
	m_nextFree = 0;
	for (int i = DT_MAX_TILES-1; i >= 0; --i)
	{
		m_tiles[i].next = m_nextFree;
		m_nextFree = &m_tiles[i];
	}

	if (!m_nodePool)
	{
		m_nodePool = new dtNodePool(2048, 256);
		if (!m_nodePool)
			return false;
	}
	
	if (!m_openList)
	{
		m_openList = new dtNodeQueue(2048);
		if (!m_openList)
			return false;
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
int dtTiledNavMesh::findConnectingPolys(const float* va, const float* vb,
										dtTile* tile, int side,
										dtTilePolyRef* con, float* conarea, int maxcon)
{
	if (!tile) return 0;
	dtTileHeader* h = tile->header;
	
	float amin[2], amax[2];
	calcRect(va,vb, amin,amax, side, 0.01f, m_portalHeight);

	// Remove links pointing to 'side' and compact the links array. 
	float bmin[2], bmax[2];
	unsigned short m = 0x8000 | (unsigned short)side;
	int n = 0;
	
	dtTilePolyRef base = getTileId(tile);
	
	for (int i = 0; i < h->npolys; ++i)
	{
		dtTilePoly* poly = &h->polys[i];
		for (int j = 0; j < poly->nv; ++j)
		{
			// Skip edges which do not point to the right side.
			if (poly->n[j] != m) continue;
			// Check if the segments touch.
			const float* vc = &h->verts[poly->v[j]*3];
			const float* vd = &h->verts[poly->v[(j+1) % (int)poly->nv]*3];
			calcRect(vc,vd, bmin,bmax, side, 0.01f, m_portalHeight);
			if (!overlapRects(amin,amax, bmin,bmax)) continue;
			// Add return value.
			if (n < maxcon)
			{
				conarea[n*2+0] = max(amin[0], bmin[0]);
				conarea[n*2+1] = min(amax[0], bmax[0]);
				con[n] = base | (unsigned int)i;
				n++;
			}
			break;
		}
	}
	return n;
}

void dtTiledNavMesh::removeExtLinks(dtTile* tile, int side)
{
	if (!tile) return;
	dtTileHeader* h = tile->header;
	
	// Remove links pointing to 'side' and compact the links array. 
	dtTileLink* pool = m_tmpLinks;
	int nlinks = 0;
	for (int i = 0; i < h->npolys; ++i)
	{
		dtTilePoly* poly = &h->polys[i];
		int plinks = nlinks;
		int nplinks = 0;		
		for (int j = 0; j < poly->nlinks; ++j)
		{
			dtTileLink* link = &h->links[poly->links+j];
			if ((int)link->side != side)
			{
				if (nlinks < h->maxlinks)
				{
					dtTileLink* dst = &pool[nlinks++];
					memcpy(dst, link, sizeof(dtTileLink));
					nplinks++;
				}
			}
		}
		poly->links = plinks;
		poly->nlinks = nplinks;
	}
	h->nlinks = nlinks;
	if (h->nlinks)
		memcpy(h->links, m_tmpLinks, sizeof(dtTileLink)*nlinks);
}

void dtTiledNavMesh::buildExtLinks(dtTile* tile, dtTile* target, int side)
{
	if (!tile) return;
	dtTileHeader* h = tile->header;
	
	// Remove links pointing to 'side' and compact the links array. 
	dtTileLink* pool = m_tmpLinks;
	int nlinks = 0;
	for (int i = 0; i < h->npolys; ++i)
	{
		dtTilePoly* poly = &h->polys[i];
		int plinks = nlinks;
		int nplinks = 0;
		// Copy internal and other external links.
		for (int j = 0; j < poly->nlinks; ++j)
		{
			dtTileLink* link = &h->links[poly->links+j];
			if ((int)link->side != side)
			{
				if (nlinks < h->maxlinks)
				{
					dtTileLink* dst = &pool[nlinks++];
					memcpy(dst, link, sizeof(dtTileLink));
					nplinks++;
				}
			}
		}
		// Create new links.
		unsigned short m = 0x8000 | (unsigned short)side;
		for (int j = 0; j < poly->nv; ++j)
		{
			// Skip edges which do not point to the right side.
			if (poly->n[j] != m) continue;
			
			// Create new links
			const float* va = &h->verts[poly->v[j]*3];
			const float* vb = &h->verts[poly->v[(j+1)%(int)poly->nv]*3];
			dtTilePolyRef nei[4];
			float neia[4*2];
			int nnei = findConnectingPolys(va,vb, target, opposite(side), nei,neia,4);
			for (int k = 0; k < nnei; ++k)
			{
				if (nlinks < h->maxlinks)
				{
					dtTileLink* link = &pool[nlinks++];
					link->ref = nei[k];
					link->p = (unsigned short)i;
					link->e = (unsigned char)j;
					link->side = (unsigned char)side;

					// Compress portal limits to a byte value.
					if (side == 0 || side == 2)
					{
						const float lmin = min(va[2], vb[2]);
						const float lmax = max(va[2], vb[2]);
						link->bmin = (unsigned char)(clamp((neia[k*2+0]-lmin)/(lmax-lmin), 0.0f, 1.0f)*255.0f);
						link->bmax = (unsigned char)(clamp((neia[k*2+1]-lmin)/(lmax-lmin), 0.0f, 1.0f)*255.0f);
					}
					else
					{
						const float lmin = min(va[0], vb[0]);
						const float lmax = max(va[0], vb[0]);
						link->bmin = (unsigned char)(clamp((neia[k*2+0]-lmin)/(lmax-lmin), 0.0f, 1.0f)*255.0f);
						link->bmax = (unsigned char)(clamp((neia[k*2+1]-lmin)/(lmax-lmin), 0.0f, 1.0f)*255.0f);
					}					
					nplinks++;
				}
			}
		}
		
		poly->links = plinks;
		poly->nlinks = nplinks;
	}
	h->nlinks = nlinks;
	if (h->nlinks)
		memcpy(h->links, m_tmpLinks, sizeof(dtTileLink)*nlinks);
}

void dtTiledNavMesh::buildIntLinks(dtTile* tile)
{
	if (!tile) return;
	dtTileHeader* h = tile->header;

	dtTilePolyRef base = getTileId(tile);
	dtTileLink* pool = h->links;
	int nlinks = 0;
	for (int i = 0; i < h->npolys; ++i)
	{
		dtTilePoly* poly = &h->polys[i];
		poly->links = nlinks;
		poly->nlinks = 0;
		for (int j = 0; j < poly->nv; ++j)
		{
			// Skip hard and non-internal edges.
			if (poly->n[j] == 0 || (poly->n[j] & 0x8000)) continue;
			
			if (nlinks < h->maxlinks)
			{
				dtTileLink* link = &pool[nlinks++];
				link->ref = base | (unsigned int)(poly->n[j]-1);
				link->p = (unsigned short)i;
				link->e = (unsigned char)j;
				link->side = 0xff;
				link->bmin = link->bmax = 0;
				poly->nlinks++;
			}
		}
	}
	h->nlinks = nlinks;
}

bool dtTiledNavMesh::addTileAt(int x, int y, unsigned char* data, int dataSize, bool ownsData)
{
	if (getTileAt(x,y))
		return false;
	// Make sure there is enough space for new tile.
	if (!m_nextFree)
		return false;
	// Make sure the data is in right format.
	dtTileHeader* header = (dtTileHeader*)data;
	if (header->magic != DT_TILE_NAVMESH_MAGIC)
		return false;
	if (header->version != DT_TILE_NAVMESH_VERSION)
		return false;
	
	// Make sure the tmp link array is large enough.
	if (header->maxlinks > m_ntmpLinks)
	{
		m_ntmpLinks = header->maxlinks;
		delete [] m_tmpLinks;
		m_tmpLinks = 0;
		m_tmpLinks = new dtTileLink[m_ntmpLinks];
	}
	if (!m_tmpLinks)
		return false;
	
	// Allocate a tile.
	dtTile* tile = m_nextFree;
	m_nextFree = tile->next;
	tile->next = 0;

	// Insert tile into the position lut.
	int h = computeTileHash(x,y);
	tile->next = m_posLookup[h];
	m_posLookup[h] = tile;
	
	// Patch header pointers.
	const int headerSize = sizeof(dtTileHeader);
	const int vertsSize = sizeof(float)*3*header->nverts;
	const int polysSize = sizeof(dtTilePoly)*header->npolys;
	const int linksSize = sizeof(dtTileLink)*(header->maxlinks);
	const int detailMeshesSize = sizeof(dtTilePolyDetail)*header->ndmeshes;
	const int detailVertsSize = sizeof(float)*3*header->ndverts;
	const int detailTrisSize = sizeof(unsigned char)*4*header->ndtris;
	
	unsigned char* d = data + headerSize;
	header->verts = (float*)d; d += vertsSize;
	header->polys = (dtTilePoly*)d; d += polysSize;
	header->links = (dtTileLink*)d; d += linksSize;
	header->dmeshes = (dtTilePolyDetail*)d; d += detailMeshesSize;
	header->dverts = (float*)d; d += detailVertsSize;
	header->dtris = (unsigned char*)d; d += detailTrisSize;

	// Init tile.
	tile->header = header;
	tile->x = x;
	tile->y = y;
	tile->data = data;
	tile->dataSize = dataSize;
	tile->ownsData = ownsData;

	buildIntLinks(tile);

	// Create connections connections.
	for (int i = 0; i < 4; ++i)
	{
		dtTile* nei = getNeighbourTileAt(x,y,i);
		if (nei)
		{
			buildExtLinks(tile, nei, i);
			buildExtLinks(nei, tile, opposite(i));
		}
	}
	
	return true;
}

dtTile* dtTiledNavMesh::getTileAt(int x, int y)
{
	// Find tile based on hash.
	int h = computeTileHash(x,y);
	dtTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->x == x && tile->y == y)
			return tile;
		tile = tile->next;
	}
	return 0;
}

dtTile* dtTiledNavMesh::getTile(int i)
{
	return &m_tiles[i];
}

const dtTile* dtTiledNavMesh::getTile(int i) const
{
	return &m_tiles[i];
}

dtTile* dtTiledNavMesh::getNeighbourTileAt(int x, int y, int side)
{
	switch (side)
	{
	case 0: x++; break;
	case 1: y++; break;
	case 2: x--; break;
	case 3: y--; break;
	};
	return getTileAt(x,y);
}

bool dtTiledNavMesh::removeTileAt(int x, int y, unsigned char** data, int* dataSize)
{
	// Remove tile from hash lookup.
	int h = computeTileHash(x,y);
	dtTile* prev = 0;
	dtTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->x == x && tile->y == y)
		{
			if (prev)
				prev->next = tile->next;
			else
				m_posLookup[h] = tile->next;
			break;
		}
		prev = tile;
		tile = tile->next;
	}
	if (!tile)
		return false;
	
	// Remove connections to neighbour tiles.
	for (int i = 0; i < 4; ++i)
	{
		dtTile* nei = getNeighbourTileAt(x,y,i);
		if (!nei) continue;
		removeExtLinks(nei, opposite(i));
	}
	
	
	// Reset tile.
	if (tile->ownsData)
	{
		// Owns data
		delete [] tile->data;
		tile->data = 0;
		tile->dataSize = 0;
		if (data) *data = 0;
		if (dataSize) *dataSize = 0;
	}
	else
	{
		if (data) *data = tile->data;
		if (dataSize) *dataSize = tile->dataSize;
	}
	tile->header = 0;
	tile->x = tile->y = 0;
	tile->salt++;

	// Add to free list.
	tile->next = m_nextFree;
	m_nextFree = tile;

	return true;
}



bool dtTiledNavMesh::closestPointToPoly(dtTilePolyRef ref, const float* pos, float* closest) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return false;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return false;
	const dtTileHeader* header = m_tiles[it].header;

	if (ip >= (unsigned int)header->npolys) return false;
	const dtTilePoly* poly = &header->polys[ip];
	
	float closestDistSqr = FLT_MAX;
	const dtTilePolyDetail* pd = &header->dmeshes[ip];
	
	for (int j = 0; j < pd->ntris; ++j)
	{
		const unsigned char* t = &header->dtris[(pd->tbase+j)*4];
		const float* v[3];
		for (int k = 0; k < 3; ++k)
		{
			if (t[k] < poly->nv)
				v[k] = &header->verts[poly->v[t[k]]*3];
			else
				v[k] = &header->dverts[(pd->vbase+(t[k]-poly->nv))*3];
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

bool dtTiledNavMesh::getPolyHeight(dtTilePolyRef ref, const float* pos, float* height) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return false;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return false;
	const dtTileHeader* header = m_tiles[it].header;
	
	if (ip >= (unsigned int)header->npolys) return false;
	const dtTilePoly* poly = &header->polys[ip];
	
	const dtTilePolyDetail* pd = &header->dmeshes[ip];
	for (int j = 0; j < pd->ntris; ++j)
	{
		const unsigned char* t = &header->dtris[(pd->tbase+j)*4];
		const float* v[3];
		for (int k = 0; k < 3; ++k)
		{
			if (t[k] < poly->nv)
				v[k] = &header->verts[poly->v[t[k]]*3];
			else
				v[k] = &header->dverts[(pd->vbase+(t[k]-poly->nv))*3];
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


dtTilePolyRef dtTiledNavMesh::findNearestPoly(const float* center, const float* extents)
{
	// Get nearby polygons from proximity grid.
	dtTilePolyRef polys[128];
	int npolys = queryPolygons(center, extents, polys, 128);
	
	// Find nearest polygon amongst the nearby polygons.
	dtTilePolyRef nearest = 0;
	float nearestDistanceSqr = FLT_MAX;
	for (int i = 0; i < npolys; ++i)
	{
		dtTilePolyRef ref = polys[i];
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

dtTilePolyRef dtTiledNavMesh::getTileId(dtTile* tile)
{
	if (!tile) return 0;
	const unsigned int it = tile - m_tiles;
	return dtEncodeTileId(tile->salt, it, 0);
}

int dtTiledNavMesh::queryTilePolygons(dtTile* tile,
									  const float* qmin, const float* qmax,
									  dtTilePolyRef* polys, const int maxPolys)
{
	float bmin[3], bmax[3];
	const dtTileHeader* header = tile->header;
	int n = 0;
	dtTilePolyRef base = getTileId(tile);
	for (int i = 0; i < header->npolys; ++i)
	{
		// Calc polygon bounds.
		dtTilePoly* p = &header->polys[i];
		const float* v = &header->verts[p->v[0]*3];
		vcopy(bmin, v);
		vcopy(bmax, v);
		for (int j = 1; j < p->nv; ++j)
		{
			v = &header->verts[p->v[j]*3];
			vmin(bmin, v);
			vmax(bmax, v);
		}
		if (overlapBoxes(qmin,qmax, bmin,bmax))
		{
			if (n < maxPolys)
				polys[n++] = base | (dtTilePolyRef)i;
		}
	}
	return n;
}

int dtTiledNavMesh::queryPolygons(const float* center, const float* extents,
								  dtTilePolyRef* polys, const int maxPolys)
{
	float bmin[3], bmax[3];
	bmin[0] = center[0] - extents[0];
	bmin[1] = center[1] - extents[1];
	bmin[2] = center[2] - extents[2];
	
	bmax[0] = center[0] + extents[0];
	bmax[1] = center[1] + extents[1];
	bmax[2] = center[2] + extents[2];
	
	// Find tiles the query touches.
	const int minx = (int)floorf((bmin[0]-m_orig[0]) / m_tileSize);
	const int maxx = (int)ceilf((bmax[0]-m_orig[0]) / m_tileSize);

	const int miny = (int)floorf((bmin[2]-m_orig[2]) / m_tileSize);
	const int maxy = (int)ceilf((bmax[2]-m_orig[2]) / m_tileSize);

	int n = 0;
	for (int y = miny; y < maxy; ++y)
	{
		for (int x = minx; x < maxx; ++x)
		{
			dtTile* tile = getTileAt(x,y);
			if (!tile) continue;
			n += queryTilePolygons(tile, bmin, bmax, polys+n, maxPolys-n);
			if (n >= maxPolys) return n;
		}
	}

	return n;
}

int dtTiledNavMesh::findPath(dtTilePolyRef startRef, dtTilePolyRef endRef,
							 const float* startPos, const float* endPos,
							 dtTilePolyRef* path, const int maxPathSize)
{
	if (!startRef || !endRef)
		return 0;
	
	if (!maxPathSize)
		return 0;
	
	if (!getPolyByRef(startRef) || !getPolyByRef(endRef))
		return 0;
	
	if (startRef == endRef)
	{
		path[0] = startRef;
		return 1;
	}
	
	if (!m_nodePool || !m_openList)
		return 0;
		
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

		// Get poly and tile.
		unsigned int salt, it, ip;
		dtDecodeTileId(bestNode->id, salt, it, ip);
		// The API input has been cheked already, skip checking internal data.
		const dtTileHeader* header = m_tiles[it].header;
		const dtTilePoly* poly = &header->polys[ip];
		
		for (int i = 0; i < poly->nlinks; ++i)
		{
			dtTilePolyRef neighbour = header->links[poly->links+i].ref;
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
					actualNode->flags &= DT_NODE_CLOSED;
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

int dtTiledNavMesh::findStraightPath(const float* startPos, const float* endPos,
									 const dtTilePolyRef* path, const int pathSize,
									 float* straightPath, const int maxStraightPathSize)
{
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
				if (!getPortalPoints(path[i], path[i+1], left, right))
				{
					if (!closestPointToPoly(path[i], endPos, closestEndPos))
						return 0;
					vcopy(&straightPath[straightPathSize*3], closestEndPos);
					straightPathSize++;
					return straightPathSize;
				}
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

// Returns portal points between two polygons.
bool dtTiledNavMesh::getPortalPoints(dtTilePolyRef from, dtTilePolyRef to, float* left, float* right) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(from, salt, it, ip);
	if (it >= DT_MAX_TILES) return false;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return false;
	if (ip >= (unsigned int)m_tiles[it].header->npolys) return false;
	const dtTileHeader* fromHeader = m_tiles[it].header;
	const dtTilePoly* fromPoly = &fromHeader->polys[ip];

	for (int i = 0; i < fromPoly->nlinks; ++i)
	{
		const dtTileLink* link = &fromHeader->links[fromPoly->links+i];
		if (link->ref == to)
		{
			// Find portal vertices.
			const int v0 = fromPoly->v[link->e];
			const int v1 = fromPoly->v[(link->e+1) % fromPoly->nv];
			vcopy(left, &fromHeader->verts[v0*3]);
			vcopy(right, &fromHeader->verts[v1*3]);
			// If the link is at tile boundary, clamp the vertices to
			// the link width.
			if (link->side == 0 || link->side == 2)
			{
				// Unpack portal limits.
				const float smin = min(left[2],right[2]);
				const float smax = max(left[2],right[2]);
				const float s = (smax-smin) / 255.0f;
				const float lmin = smin + link->bmin*s;
				const float lmax = smin + link->bmax*s;
				left[2] = max(left[2],lmin);
				left[2] = min(left[2],lmax);
				right[2] = max(right[2],lmin);
				right[2] = min(right[2],lmax);
			}
			else if (link->side == 1 || link->side == 3)
			{
				// Unpack portal limits.
				const float smin = min(left[0],right[0]);
				const float smax = max(left[0],right[0]);
				const float s = (smax-smin) / 255.0f;
				const float lmin = smin + link->bmin*s;
				const float lmax = smin + link->bmax*s;
				left[0] = max(left[0],lmin);
				left[0] = min(left[0],lmax);
				right[0] = max(right[0],lmin);
				right[0] = min(right[0],lmax);
			}
			return true;
		}
	}
	return false;
}

// Returns edge mid point between two polygons.
bool dtTiledNavMesh::getEdgeMidPoint(dtTilePolyRef from, dtTilePolyRef to, float* mid) const
{
	float left[3], right[3];
	if (!getPortalPoints(from, to, left,right)) return false;
	mid[0] = (left[0]+right[0])*0.5f;
	mid[1] = (left[1]+right[1])*0.5f;
	mid[2] = (left[2]+right[2])*0.5f;
	return true;
}

int dtTiledNavMesh::raycast(dtTilePolyRef centerRef, const float* startPos, const float* endPos,
							float& t, dtTilePolyRef* path, const int pathSize)
{
	t = 0;
	
	if (!centerRef || !getPolyByRef(centerRef))
		return 0;
	
	dtTilePolyRef curRef = centerRef;
	float verts[DT_TILE_VERTS_PER_POLYGON*3];	
	int n = 0;
	
	while (curRef)
	{
		// Cast ray against current polygon.
		
		// The API input has been cheked already, skip checking internal data.
		unsigned int salt, it, ip;
		dtDecodeTileId(curRef, salt, it, ip);
		const dtTileHeader* header = m_tiles[it].header;
		const dtTilePoly* poly = &header->polys[ip];

		// Collect vertices.
		int nv = 0;
		for (int i = 0; i < (int)poly->nv; ++i)
		{
			vcopy(&verts[nv*3], &header->verts[poly->v[i]*3]);
			nv++;
		}		
		if (nv < 3)
		{
			// Hit bad polygon, report hit.
			return n;
		}
		
		float tmin, tmax;
		int segMin, segMax;
		if (!intersectSegmentPoly2D(startPos, endPos, verts, nv, tmin, tmax, segMin, segMax))
		{
			// Could not hit the polygon, keep the old t and report hit.
			return n;
		}
		// Keep track of furthest t so far.
		if (tmax > t)
			t = tmax;

		if (n < pathSize)
			path[n++] = curRef;
		
		// Follow neighbours.
		dtTilePolyRef nextRef = 0;
		for (int i = 0; i < poly->nlinks; ++i)
		{
			const dtTileLink* link = &header->links[poly->links+i];
			if ((int)link->e == segMax)
			{
				// If the link is internal, just return the ref.
				if (link->side == 0xff)
				{
					nextRef = link->ref;
					break;
				}
				
				// If the link is at tile boundary,
				const int v0 = poly->v[link->e];
				const int v1 = poly->v[(link->e+1) % poly->nv];
				const float* left = &header->verts[v0*3];
				const float* right = &header->verts[v1*3];
				
				// Check that the intersection lies inside the link portal.
				if (link->side == 0 || link->side == 2)
				{
					// Calculate link size.
					const float smin = min(left[2],right[2]);
					const float smax = max(left[2],right[2]);
					const float s = (smax-smin) / 255.0f;
					const float lmin = smin + link->bmin*s;
					const float lmax = smin + link->bmax*s;
					// Find Z intersection.
					float z = startPos[2] + (endPos[2]-startPos[2])*tmax;
					if (z >= lmin && z <= lmax)
					{
						nextRef = link->ref;
						break;
					}
				}
				else if (link->side == 1 || link->side == 3)
				{
					// Calculate link size.
					const float smin = min(left[0],right[0]);
					const float smax = max(left[0],right[0]);
					const float s = (smax-smin) / 255.0f;
					const float lmin = smin + link->bmin*s;
					const float lmax = smin + link->bmax*s;
					// Find X intersection.
					float x = startPos[0] + (endPos[0]-startPos[0])*tmax;
					if (x >= lmin && x <= lmax)
					{
						nextRef = link->ref;
						break;
					}
				}
			}
		}
		
		if (!nextRef)
		{
			// No neighbour, we hit a wall.
			return n;
		}
		
		// No hit, advance to neighbour polygon.
		curRef = nextRef;
	}
	
	return n;
}

int dtTiledNavMesh::findPolysAround(dtTilePolyRef centerRef, const float* centerPos, float radius,
									dtTilePolyRef* resultRef, dtTilePolyRef* resultParent, float* resultCost,
									const int maxResult)
{
	if (!centerRef) return 0;
	if (!getPolyByRef(centerRef)) return 0;
	if (!m_nodePool || !m_openList) return 0;
	
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

		// Get poly and tile.
		unsigned int salt, it, ip;
		dtDecodeTileId(bestNode->id, salt, it, ip);
		// The API input has been cheked already, skip checking internal data.
		const dtTileHeader* header = m_tiles[it].header;
		const dtTilePoly* poly = &header->polys[ip];
		
		for (int i = 0; i < poly->nlinks; ++i)
		{
			const dtTileLink* link = &header->links[poly->links+i];
			dtTilePolyRef neighbour = link->ref;
			if (neighbour)
			{
				// Skip parent node.
				if (bestNode->pidx && m_nodePool->getNodeAtIdx(bestNode->pidx)->id == neighbour)
					continue;
				
				// Calc distance to the edge.
				const float* va = &header->verts[poly->v[link->e]*3];
				const float* vb = &header->verts[poly->v[(link->e+1)%poly->nv]*3];
				float tseg;
				float distSqr = distancePtSegSqr2D(centerPos, va, vb, tseg);
				
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
						actualNode->flags = DT_NODE_OPEN;
						m_openList->push(actualNode);
					}
				}
			}
		}
	}
	
	return n;
}

float dtTiledNavMesh::findDistanceToWall(dtTilePolyRef centerRef, const float* centerPos, float maxRadius,
						 float* hitPos, float* hitNormal)
{
	if (!centerRef) return 0;
	if (!getPolyByRef(centerRef)) return 0;
	if (!m_nodePool || !m_openList) return 0;
	
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
	
	while (!m_openList->empty())
	{
		dtNode* bestNode = m_openList->pop();
		
		// Get poly and tile.
		unsigned int salt, it, ip;
		dtDecodeTileId(bestNode->id, salt, it, ip);
		// The API input has been cheked already, skip checking internal data.
		const dtTileHeader* header = m_tiles[it].header;
		const dtTilePoly* poly = &header->polys[ip];
		
		// Hit test walls.
		for (int i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j = i++)
		{
			// Skip non-solid edges.
			if (poly->n[j] & 0x8000)
			{
				// Tile border.
				bool solid = true;
				for (int i = 0; i < poly->nlinks; ++i)
				{
					const dtTileLink* link = &header->links[poly->links+i];
					if (link->e == j && link->ref != 0)
					{
						solid = false;
						break;
					}
				}
				if (!solid) continue;
			}
			else if (poly->n[j])
			{
				// Internal edge
				continue;
			}
			
			// Calc distance to the edge.
			const float* vj = &header->verts[poly->v[j]*3];
			const float* vi = &header->verts[poly->v[i]*3];
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
		
		for (int i = 0; i < poly->nlinks; ++i)
		{
			const dtTileLink* link = &header->links[poly->links+i];
			dtTilePolyRef neighbour = link->ref;
			if (neighbour)
			{
				// Skip parent node.
				if (bestNode->pidx && m_nodePool->getNodeAtIdx(bestNode->pidx)->id == neighbour)
					continue;
				
				// Calc distance to the edge.
				const float* va = &header->verts[poly->v[link->e]*3];
				const float* vb = &header->verts[poly->v[(link->e+1)%poly->nv]*3];
				float tseg;
				float distSqr = distancePtSegSqr2D(centerPos, va, vb, tseg);
				
				// If the circle is not touching the next polygon, skip it.
				if (distSqr > radiusSqr)
					continue;
				
				dtNode* parent = bestNode;
				dtNode newNode;
				newNode.pidx = m_nodePool->getNodeIdx(parent);
				newNode.id = neighbour;

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
						actualNode->flags = DT_NODE_OPEN;
						m_openList->push(actualNode);
					}
				}
			}
		}
	}
	
	// Calc hit normal.
	vsub(hitNormal, centerPos, hitPos);
	vnormalize(hitNormal);
	
	return sqrtf(radiusSqr);
}

const dtTilePoly* dtTiledNavMesh::getPolyByRef(dtTilePolyRef ref) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return 0;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return 0;
	if (ip >= (unsigned int)m_tiles[it].header->npolys) return 0;
	return &m_tiles[it].header->polys[ip];
}

const float* dtTiledNavMesh::getPolyVertsByRef(dtTilePolyRef ref) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return 0;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return 0;
	if (ip >= (unsigned int)m_tiles[it].header->npolys) return 0;
	return m_tiles[it].header->verts;
}

const dtTileLink* dtTiledNavMesh::getPolyLinksByRef(dtTilePolyRef ref) const
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return 0;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return 0;
	if (ip >= (unsigned int)m_tiles[it].header->npolys) return 0;
	return m_tiles[it].header->links;
}
