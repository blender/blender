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

#include "DetourDebugDraw.h"
#include "DetourStatNavMesh.h"
#include "SDL.h"
#include "SDL_Opengl.h"

void dtDebugDrawStatNavMeshPoly(const dtStatNavMesh* mesh, dtStatPolyRef ref, const float* col)
{
	int idx = mesh->getPolyIndexByRef(ref);
	if (idx == -1) return;

	glColor4f(col[0],col[1],col[2],0.25f);

	if (mesh->getPolyDetailCount())
	{
		const dtStatPoly* p = mesh->getPoly(idx);
		const dtStatPolyDetail* pd = mesh->getPolyDetail(idx);
		glBegin(GL_TRIANGLES);
		for (int j = 0; j < pd->ntris; ++j)
		{
			const unsigned char* t = mesh->getDetailTri(pd->tbase+j);
			for (int k = 0; k < 3; ++k)
			{
				if (t[k] < p->nv)
					glVertex3fv(mesh->getVertex(p->v[t[k]]));
				else
					glVertex3fv(mesh->getDetailVertex(pd->vbase+(t[k]-p->nv)));
			}
		}
		glEnd();
	}
	else
	{
		const dtStatPoly* p = mesh->getPoly(idx);
		glBegin(GL_TRIANGLES);
		unsigned short vi[3];
		for (int j = 2; j < (int)p->nv; ++j)
		{
			vi[0] = p->v[0];
			vi[1] = p->v[j-1];
			vi[2] = p->v[j];
			for (int k = 0; k < 3; ++k)
			{
				const float* v = mesh->getVertex(vi[k]);
				glVertex3f(v[0], v[1]+0.2f, v[2]);
			}
		}
		glEnd();
	}
}

static void drawBoxWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col)
{
	glColor4fv(col);
	
	// Top
	glVertex3f(minx, miny, minz);
	glVertex3f(maxx, miny, minz);
	glVertex3f(maxx, miny, minz);
	glVertex3f(maxx, miny, maxz);
	glVertex3f(maxx, miny, maxz);
	glVertex3f(minx, miny, maxz);
	glVertex3f(minx, miny, maxz);
	glVertex3f(minx, miny, minz);
	
	// bottom
	glVertex3f(minx, maxy, minz);
	glVertex3f(maxx, maxy, minz);
	glVertex3f(maxx, maxy, minz);
	glVertex3f(maxx, maxy, maxz);
	glVertex3f(maxx, maxy, maxz);
	glVertex3f(minx, maxy, maxz);
	glVertex3f(minx, maxy, maxz);
	glVertex3f(minx, maxy, minz);
	
	// Sides
	glVertex3f(minx, miny, minz);
	glVertex3f(minx, maxy, minz);
	glVertex3f(maxx, miny, minz);
	glVertex3f(maxx, maxy, minz);
	glVertex3f(maxx, miny, maxz);
	glVertex3f(maxx, maxy, maxz);
	glVertex3f(minx, miny, maxz);
	glVertex3f(minx, maxy, maxz);
}

void dtDebugDrawStatNavMeshBVTree(const dtStatNavMesh* mesh)
{
	const float col[] = { 1,1,1,0.5f };
	const dtStatNavMeshHeader* hdr = mesh->getHeader();
	
	const dtStatBVNode* nodes = mesh->getBvTreeNodes();
	int nnodes = mesh->getBvTreeNodeCount();
	
	glBegin(GL_LINES);

	for (int i = 0; i < nnodes; ++i)
	{
		const dtStatBVNode* n = &nodes[i];
		if (n->i < 0) // Leaf indices are positive.
			continue;
		drawBoxWire(hdr->bmin[0] + n->bmin[0]*hdr->cs,
					hdr->bmin[1] + n->bmin[1]*hdr->cs,
					hdr->bmin[2] + n->bmin[2]*hdr->cs,
					hdr->bmin[0] + n->bmax[0]*hdr->cs,
					hdr->bmin[1] + n->bmax[1]*hdr->cs,
					hdr->bmin[2] + n->bmax[2]*hdr->cs, col);
	}
	glEnd();
}


static float distancePtLine2d(const float* pt, const float* p, const float* q)
{
	float pqx = q[0] - p[0];
	float pqz = q[2] - p[2];
	float dx = pt[0] - p[0];
	float dz = pt[2] - p[2];
	float d = pqx*pqx + pqz*pqz;
	float t = pqx*dx + pqz*dz;
	if (d != 0) t /= d;
	dx = p[0] + t*pqx - pt[0];
	dz = p[2] + t*pqz - pt[2];
	return dx*dx + dz*dz;
}

static void drawStatMeshPolyBoundaries(const dtStatNavMesh* mesh, bool inner)
{
	static const float thr = 0.01f*0.01f;

	glBegin(GL_LINES);
	for (int i = 0; i < mesh->getPolyCount(); ++i)
	{
		const dtStatPoly* p = mesh->getPoly(i);
		const dtStatPolyDetail* pd = mesh->getPolyDetail(i);
		
		for (int j = 0, nj = (int)p->nv; j < nj; ++j)
		{
			if (inner)
			{
				// Skip non-connected edges.
				if (p->n[j] == 0) continue;
			}
			else
			{
				// Skip connected edges.
				if (p->n[j] != 0) continue;
			}
				
			const float* v0 = mesh->getVertex(p->v[j]);
			const float* v1 = mesh->getVertex(p->v[(j+1) % nj]);
			
			// Draw detail mesh edges which align with the actual poly edge.
			// This is really slow.
			for (int k = 0; k < pd->ntris; ++k)
			{
				const unsigned char* t = mesh->getDetailTri(pd->tbase+k);
				const float* tv[3];
				for (int m = 0; m < 3; ++m)
				{
					if (t[m] < p->nv)
						tv[m] = mesh->getVertex(p->v[t[m]]);
					else
						tv[m] = mesh->getDetailVertex(pd->vbase+(t[m]-p->nv));
				}
				for (int m = 0, n = 2; m < 3; n=m++)
				{
					if (((t[3] >> (n*2)) & 0x3) == 0) continue;	// Skip inner edges.
					if (distancePtLine2d(tv[n],v0,v1) < thr &&
						distancePtLine2d(tv[m],v0,v1) < thr)
					{
						glVertex3fv(tv[n]);
						glVertex3fv(tv[m]);
					}
				}
			}
		}
	}
	glEnd();
}

void dtDebugDrawStatNavMesh(const dtStatNavMesh* mesh, bool drawClosedList)
{
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < mesh->getPolyDetailCount(); ++i)
	{
		const dtStatPoly* p = mesh->getPoly(i);
		const dtStatPolyDetail* pd = mesh->getPolyDetail(i);
		
		if (drawClosedList && mesh->isInClosedList(i+1))
			glColor4ub(255,196,0,64);
		else
			glColor4ub(0,196,255,64);
			
		for (int j = 0; j < pd->ntris; ++j)
		{
			const unsigned char* t = mesh->getDetailTri(pd->tbase+j);
			for (int k = 0; k < 3; ++k)
			{
				if (t[k] < p->nv)
					glVertex3fv(mesh->getVertex(p->v[t[k]]));
				else
					glVertex3fv(mesh->getDetailVertex(pd->vbase+(t[k]-p->nv)));
			}
		}
	}
	glEnd();
	
	// Draw inter poly boundaries
	glColor4ub(0,48,64,32);
	glLineWidth(1.5f);
	drawStatMeshPolyBoundaries(mesh, true);
	
	// Draw outer poly boundaries
	glLineWidth(2.5f);
	glColor4ub(0,48,64,220);
	drawStatMeshPolyBoundaries(mesh, false);

	glLineWidth(1.0f);
	
	glPointSize(3.0f);
	glColor4ub(0,0,0,196);
	glBegin(GL_POINTS);
	for (int i = 0; i < mesh->getVertexCount(); ++i)
	{
		const float* v = mesh->getVertex(i);
		glVertex3f(v[0], v[1], v[2]);
	}
	glEnd();
	glPointSize(1.0f);	
}


static void drawTilePolyBoundaries(const dtTileHeader* header, bool inner)
{
	static const float thr = 0.01f*0.01f;

	glBegin(GL_LINES);
	for (int i = 0; i < header->npolys; ++i)
	{
		const dtTilePoly* p = &header->polys[i];
		const dtTilePolyDetail* pd = &header->dmeshes[i];
		
		for (int j = 0, nj = (int)p->nv; j < nj; ++j)
		{
			if (inner)
			{
				if (p->n[j] == 0) continue;
				if (p->n[j] & 0x8000)
				{
					bool con = false;
					for (int k = 0; k < p->nlinks; ++k)
					{
						if (header->links[p->links+k].e == j)
						{
							con = true;
							break;
						}
					}
					if (con)
						glColor4ub(255,255,255,128);
					else
						glColor4ub(0,0,0,128);
				}
				else
					glColor4ub(0,48,64,32);
			}
			else
			{
				if (p->n[j] != 0) continue;
			}
			
			const float* v0 = &header->verts[p->v[j]*3];
			const float* v1 = &header->verts[p->v[(j+1)%nj]*3];
			
			// Draw detail mesh edges which align with the actual poly edge.
			// This is really slow.
			for (int k = 0; k < pd->ntris; ++k)
			{
				const unsigned char* t = &header->dtris[(pd->tbase+k)*4];
				const float* tv[3];
				for (int m = 0; m < 3; ++m)
				{
					if (t[m] < p->nv)
						tv[m] = &header->verts[p->v[t[m]]*3];
					else
						tv[m] = &header->dverts[(pd->vbase+(t[m]-p->nv))*3];
				}
				for (int m = 0, n = 2; m < 3; n=m++)
				{
					if (((t[3] >> (n*2)) & 0x3) == 0) continue;	// Skip inner detail edges.
					if (distancePtLine2d(tv[n],v0,v1) < thr &&
						distancePtLine2d(tv[m],v0,v1) < thr)
					{
						glVertex3fv(tv[n]);
						glVertex3fv(tv[m]);
					}
				}
			}
		}
	}
	glEnd();
}

static void drawTile(const dtTileHeader* header)
{
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < header->npolys; ++i)
	{
		const dtTilePoly* p = &header->polys[i];
		const dtTilePolyDetail* pd = &header->dmeshes[i];
		
		glColor4ub(0,196,255,64);
		
		for (int j = 0; j < pd->ntris; ++j)
		{
			const unsigned char* t = &header->dtris[(pd->tbase+j)*4];
			for (int k = 0; k < 3; ++k)
			{
				if (t[k] < p->nv)
					glVertex3fv(&header->verts[p->v[t[k]]*3]);
				else
					glVertex3fv(&header->dverts[(pd->vbase+t[k]-p->nv)*3]);
			}
		}
	}
	glEnd();
	
	// Draw inter poly boundaries
	glColor4ub(0,48,64,32);
	glLineWidth(1.5f);
	
	drawTilePolyBoundaries(header, true);

	// Draw outer poly boundaries
	glLineWidth(2.5f);
	glColor4ub(0,48,64,220);

	drawTilePolyBoundaries(header, false);
	
	glLineWidth(1.0f);
	
	glPointSize(3.0f);
	glColor4ub(0,0,0,196);
	glBegin(GL_POINTS);
	for (int i = 0; i < header->nverts; ++i)
	{
		const float* v = &header->verts[i*3];
		glVertex3f(v[0], v[1], v[2]);
	}
	glEnd();
	glPointSize(1.0f);	
	
	// Draw portals
/*	glBegin(GL_LINES);

	for (int i = 0; i < header->nportals[0]; ++i)
	{
		const dtTilePortal* p = &header->portals[0][i];		
		if (p->ncon)
			glColor4ub(255,255,255,192);
		else
			glColor4ub(255,0,0,64);
		glVertex3f(header->bmax[0]-0.1f, p->bmin[1], p->bmin[0]);
		glVertex3f(header->bmax[0]-0.1f, p->bmax[1], p->bmin[0]);

		glVertex3f(header->bmax[0]-0.1f, p->bmax[1], p->bmin[0]);
		glVertex3f(header->bmax[0]-0.1f, p->bmax[1], p->bmax[0]);

		glVertex3f(header->bmax[0]-0.1f, p->bmax[1], p->bmax[0]);
		glVertex3f(header->bmax[0]-0.1f, p->bmin[1], p->bmax[0]);

		glVertex3f(header->bmax[0]-0.1f, p->bmin[1], p->bmax[0]);
		glVertex3f(header->bmax[0]-0.1f, p->bmin[1], p->bmin[0]);
	}
	for (int i = 0; i < header->nportals[1]; ++i)
	{
		const dtTilePortal* p = &header->portals[1][i];
		if (p->ncon)
			glColor4ub(255,255,255,192);
		else
			glColor4ub(255,0,0,64);
		glVertex3f(p->bmin[0], p->bmin[1], header->bmax[2]-0.1f);
		glVertex3f(p->bmin[0], p->bmax[1], header->bmax[2]-0.1f);
		
		glVertex3f(p->bmin[0], p->bmax[1], header->bmax[2]-0.1f);
		glVertex3f(p->bmax[0], p->bmax[1], header->bmax[2]-0.1f);
		
		glVertex3f(p->bmax[0], p->bmax[1], header->bmax[2]-0.1f);
		glVertex3f(p->bmax[0], p->bmin[1], header->bmax[2]-0.1f);
		
		glVertex3f(p->bmax[0], p->bmin[1], header->bmax[2]-0.1f);
		glVertex3f(p->bmin[0], p->bmin[1], header->bmax[2]-0.1f);
	}
	for (int i = 0; i < header->nportals[2]; ++i)
	{
		const dtTilePortal* p = &header->portals[2][i];
		if (p->ncon)
			glColor4ub(255,255,255,192);
		else
			glColor4ub(255,0,0,64);
		glVertex3f(header->bmin[0]+0.1f, p->bmin[1], p->bmin[0]);
		glVertex3f(header->bmin[0]+0.1f, p->bmax[1], p->bmin[0]);
		
		glVertex3f(header->bmin[0]+0.1f, p->bmax[1], p->bmin[0]);
		glVertex3f(header->bmin[0]+0.1f, p->bmax[1], p->bmax[0]);
		
		glVertex3f(header->bmin[0]+0.1f, p->bmax[1], p->bmax[0]);
		glVertex3f(header->bmin[0]+0.1f, p->bmin[1], p->bmax[0]);
		
		glVertex3f(header->bmin[0]+0.1f, p->bmin[1], p->bmax[0]);
		glVertex3f(header->bmin[0]+0.1f, p->bmin[1], p->bmin[0]);
	}
	for (int i = 0; i < header->nportals[3]; ++i)
	{
		const dtTilePortal* p = &header->portals[3][i];
		if (p->ncon)
			glColor4ub(255,255,255,192);
		else
			glColor4ub(255,0,0,64);
		glVertex3f(p->bmin[0], p->bmin[1], header->bmin[2]+0.1f);
		glVertex3f(p->bmin[0], p->bmax[1], header->bmin[2]+0.1f);
		
		glVertex3f(p->bmin[0], p->bmax[1], header->bmin[2]+0.1f);
		glVertex3f(p->bmax[0], p->bmax[1], header->bmin[2]+0.1f);
		
		glVertex3f(p->bmax[0], p->bmax[1], header->bmin[2]+0.1f);
		glVertex3f(p->bmax[0], p->bmin[1], header->bmin[2]+0.1f);
		
		glVertex3f(p->bmax[0], p->bmin[1], header->bmin[2]+0.1f);
		glVertex3f(p->bmin[0], p->bmin[1], header->bmin[2]+0.1f);
	}
	glEnd();*/
}

void dtDebugDrawTiledNavMesh(const dtTiledNavMesh* mesh)
{
	if (!mesh) return;
	
	for (int i = 0; i < DT_MAX_TILES; ++i)
	{
		const dtTile* tile = mesh->getTile(i);
		if (!tile->header) continue;

		drawTile(tile->header);
	}
}

void dtDebugDrawTiledNavMeshPoly(const dtTiledNavMesh* mesh, dtTilePolyRef ref, const float* col)
{
	unsigned int salt, it, ip;
	dtDecodeTileId(ref, salt, it, ip);
	if (it >= DT_MAX_TILES) return;
	const dtTile* tile = mesh->getTile(it);
	if (tile->salt != salt || tile->header == 0) return;
	const dtTileHeader* header = tile->header;
	
	if (ip >= (unsigned int)header->npolys) return;
	
	glColor4f(col[0],col[1],col[2],0.25f);

	const dtTilePoly* p = &header->polys[ip];
	const dtTilePolyDetail* pd = &header->dmeshes[ip];
	
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < pd->ntris; ++i)
	{
		const unsigned char* t = &header->dtris[(pd->tbase+i)*4];
		for (int j = 0; j < 3; ++j)
		{
			if (t[j] < p->nv)
				glVertex3fv(&header->verts[p->v[t[j]]*3]);
			else
				glVertex3fv(&header->dverts[(pd->vbase+t[j]-p->nv)*3]);
		}
	}
	glEnd();
}

