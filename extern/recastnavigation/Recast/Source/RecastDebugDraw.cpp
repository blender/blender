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
#include "RecastDebugDraw.h"
#include "SDL.h"
#include "SDL_Opengl.h"
#include "MeshLoaderObj.h"
#include "Recast.h"

void rcDebugDrawMesh(const float* verts, int nverts,
					 const int* tris, const float* normals, int ntris,
					 const unsigned char* flags)
{	
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < ntris*3; i += 3)
	{
		float a = (2+normals[i+0]+normals[i+1])/4;
		if (flags && !flags[i/3])
			glColor3f(a,a*0.3f,a*0.1f);
		else
			glColor3f(a,a,a);
		glVertex3fv(&verts[tris[i]*3]);
		glVertex3fv(&verts[tris[i+1]*3]);
		glVertex3fv(&verts[tris[i+2]*3]);
	}
	glEnd();
}

void rcDebugDrawMeshSlope(const float* verts, int nverts,
						  const int* tris, const float* normals, int ntris,
						  const float walkableSlopeAngle)
{
	const float walkableThr = cosf(walkableSlopeAngle/180.0f*(float)M_PI);
	
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < ntris*3; i += 3)
	{
		const float* norm = &normals[i];
		float a = (2+norm[0]+norm[1])/4;
		if (norm[1] > walkableThr)
			glColor3f(a,a,a);
		else
			glColor3f(a,a*0.3f,a*0.1f);
		glVertex3fv(&verts[tris[i]*3]);
		glVertex3fv(&verts[tris[i+1]*3]);
		glVertex3fv(&verts[tris[i+2]*3]);
	}
	glEnd();
}

void drawBoxWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col)
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

void drawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz,
			 const float* col1, const float* col2)
{
	float verts[8*3] =
	{
		minx, miny, minz,
		maxx, miny, minz,
		maxx, miny, maxz,
		minx, miny, maxz,
		minx, maxy, minz,
		maxx, maxy, minz,
		maxx, maxy, maxz,
		minx, maxy, maxz,
	};
	static const float dim[6] =
	{
		0.95f, 0.55f, 0.65f, 0.85f, 0.65f, 0.85f, 
	};
	static const unsigned char inds[6*5] =
	{
		0,  7, 6, 5, 4,
		1,  0, 1, 2, 3,
		2,  1, 5, 6, 2,
		3,  3, 7, 4, 0,
		4,  2, 6, 7, 3,
		5,  0, 4, 5, 1,
	};
	
	const unsigned char* in = inds;
	for (int i = 0; i < 6; ++i)
	{
		float d = dim[*in]; in++;
		if (i == 0)
			glColor4f(d*col2[0],d*col2[1],d*col2[2], col2[3]);
		else
			glColor4f(d*col1[0],d*col1[1],d*col1[2], col1[3]);
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
		glVertex3fv(&verts[*in*3]); in++;
	}
}

void rcDebugDrawCylinderWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col)
{
	static const int NUM_SEG = 16;
	float dir[NUM_SEG*2];
	for (int i = 0; i < NUM_SEG; ++i)
	{
		const float a = (float)i/(float)NUM_SEG*(float)M_PI*2;
		dir[i*2] = cosf(a);
		dir[i*2+1] = sinf(a);
	}

	const float cx = (maxx + minx)/2;
	const float cz = (maxz + minz)/2;
	const float rx = (maxx - minx)/2;
	const float rz = (maxz - minz)/2;
	
	glColor4fv(col);
	glBegin(GL_LINES);
	for (int i = 0, j=NUM_SEG-1; i < NUM_SEG; j=i++)
	{
		glVertex3f(cx+dir[j*2+0]*rx, miny, cz+dir[j*2+1]*rz);
		glVertex3f(cx+dir[i*2+0]*rx, miny, cz+dir[i*2+1]*rz);
		glVertex3f(cx+dir[j*2+0]*rx, maxy, cz+dir[j*2+1]*rz);
		glVertex3f(cx+dir[i*2+0]*rx, maxy, cz+dir[i*2+1]*rz);
	}
	for (int i = 0; i < NUM_SEG; i += NUM_SEG/4)
	{
		glVertex3f(cx+dir[i*2+0]*rx, miny, cz+dir[i*2+1]*rz);
		glVertex3f(cx+dir[i*2+0]*rx, maxy, cz+dir[i*2+1]*rz);
	}
	glEnd();
}

void rcDebugDrawBoxWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col)
{
	glBegin(GL_LINES);
	drawBoxWire(minx, miny, minz, maxx, maxy, maxz, col);
	glEnd();
}

void rcDebugDrawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz,
					const float* col1, const float* col2)
{
	glBegin(GL_QUADS);
	drawBox(minx, miny, minz, maxx, maxy, maxz, col1, col2);
	glEnd();
}


void rcDebugDrawHeightfieldSolid(const rcHeightfield& hf)
{
	static const float col0[4] = { 1,1,1,1 };
	
	const float* orig = hf.bmin;
	const float cs = hf.cs;
	const float ch = hf.ch;
	
	const int w = hf.width;
	const int h = hf.height;
	
	glBegin(GL_QUADS);
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			float fx = orig[0] + x*cs;
			float fz = orig[2] + y*cs;
			const rcSpan* s = hf.spans[x + y*w];
			while (s)
			{
				drawBox(fx, orig[1]+s->smin*ch, fz, fx+cs, orig[1] + s->smax*ch, fz+cs, col0, col0);
				s = s->next;
			}
		}
	}
	glEnd();
}

void rcDebugDrawHeightfieldWalkable(const rcHeightfield& hf)
{
	static const float col0[4] = { 1,1,1,1 };
	static const float col1[4] = { 0.25f,0.44f,0.5f,1 };
	
	const float* orig = hf.bmin;
	const float cs = hf.cs;
	const float ch = hf.ch;
	
	const int w = hf.width;
	const int h = hf.height;
	
	glBegin(GL_QUADS);
	
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			float fx = orig[0] + x*cs;
			float fz = orig[2] + y*cs;
			const rcSpan* s = hf.spans[x + y*w];
			while (s)
			{
				bool csel = (s->flags & 0x1) == 0;
				drawBox(fx, orig[1]+s->smin*ch, fz, fx+cs, orig[1] + s->smax*ch, fz+cs, col0, csel ? col0 : col1);
				s = s->next;
			}
		}
	}
	glEnd();
}

void rcDebugDrawCompactHeightfieldSolid(const rcCompactHeightfield& chf)
{
	const float cs = chf.cs;
	const float ch = chf.ch;

	glColor3ub(64,112,128);

	glBegin(GL_QUADS);
	for (int y = 0; y < chf.height; ++y)
	{
		for (int x = 0; x < chf.width; ++x)
		{
			const float fx = chf.bmin[0] + x*cs;
			const float fz = chf.bmin[2] + y*cs;
			const rcCompactCell& c = chf.cells[x+y*chf.width];

			for (unsigned i = c.index, ni = c.index+c.count; i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				const float fy = chf.bmin[1] + (s.y+1)*ch;
				glVertex3f(fx, fy, fz);
				glVertex3f(fx, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz);
			}
		}
	}
	glEnd();
}

void rcDebugDrawCompactHeightfieldRegions(const rcCompactHeightfield& chf)
{
	const float cs = chf.cs;
	const float ch = chf.ch;

	float col[4] = { 1,1,1,1 };
	
	glBegin(GL_QUADS);
	for (int y = 0; y < chf.height; ++y)
	{
		for (int x = 0; x < chf.width; ++x)
		{
			const float fx = chf.bmin[0] + x*cs;
			const float fz = chf.bmin[2] + y*cs;
			const rcCompactCell& c = chf.cells[x+y*chf.width];
			
			for (unsigned i = c.index, ni = c.index+c.count; i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				if (s.reg)
				{
					intToCol(s.reg, col);
					glColor4fv(col);
				}
				else
				{
					glColor4ub(0,0,0,128);
				}
				const float fy = chf.bmin[1] + (s.y+1)*ch;
				glVertex3f(fx, fy, fz);
				glVertex3f(fx, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz);
			}
		}
	}
	glEnd();
}


void rcDebugDrawCompactHeightfieldDistance(const rcCompactHeightfield& chf)
{
	const float cs = chf.cs;
	const float ch = chf.ch;
		
	float maxd = chf.maxDistance;
	if (maxd < 1.0f) maxd = 1;
	float dscale = 1.0f / maxd;
	
	glBegin(GL_QUADS);
	for (int y = 0; y < chf.height; ++y)
	{
		for (int x = 0; x < chf.width; ++x)
		{
			const float fx = chf.bmin[0] + x*cs;
			const float fz = chf.bmin[2] + y*cs;
			const rcCompactCell& c = chf.cells[x+y*chf.width];
			
			for (unsigned i = c.index, ni = c.index+c.count; i < ni; ++i)
			{
				const rcCompactSpan& s = chf.spans[i];
				const float fy = chf.bmin[1] + (s.y+1)*ch;
				float cd = (float)s.dist * dscale;
				glColor3f(cd, cd, cd);
				glVertex3f(fx, fy, fz);
				glVertex3f(fx, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz+cs);
				glVertex3f(fx+cs, fy, fz);
			}
		}
	}
	glEnd();
}

static void getContourCenter(const rcContour* cont, const float* orig, float cs, float ch, float* center)
{
	center[0] = 0;
	center[1] = 0;
	center[2] = 0;
	if (!cont->nverts)
		return;
	for (int i = 0; i < cont->nverts; ++i)
	{
		const int* v = &cont->verts[i*4];
		center[0] += (float)v[0];
		center[1] += (float)v[1];
		center[2] += (float)v[2];
	}
	const float s = 1.0f / cont->nverts;
	center[0] *= s * cs;
	center[1] *= s * ch;
	center[2] *= s * cs;
	center[0] += orig[0];
	center[1] += orig[1] + 4*ch;
	center[2] += orig[2];
}

static const rcContour* findContourFromSet(const rcContourSet& cset, unsigned short reg)
{
	for (int i = 0; i < cset.nconts; ++i)
	{
		if (cset.conts[i].reg == reg)
			return &cset.conts[i];
	}
	return 0;
}

static void drawArc(const float* p0, const float* p1)
{
	static const int NPTS = 8;
	float pts[NPTS*3];
	float dir[3];
	vsub(dir, p1, p0);
	const float len = sqrtf(vdistSqr(p0, p1));
	for (int i = 0; i < NPTS; ++i)
	{
		float u = (float)i / (float)(NPTS-1);
		float* p = &pts[i*3];
		p[0] = p0[0] + dir[0] * u;
		p[1] = p0[1] + dir[1] * u + (len/4) * (1-rcSqr(u*2-1));
		p[2] = p0[2] + dir[2] * u;
	}
	for (int i = 0; i < NPTS-1; ++i)
	{
		glVertex3fv(&pts[i*3]);
		glVertex3fv(&pts[(i+1)*3]);
	}
}

void rcDrawArc(const float* p0, const float* p1)
{
	glBegin(GL_LINES);
	drawArc(p0, p1);
	glEnd();
}

void rcDebugDrawRegionConnections(const rcContourSet& cset, const float alpha)
{
	const float* orig = cset.bmin;
	const float cs = cset.cs;
	const float ch = cset.ch;
	
	// Draw centers
	float pos[3], pos2[3];

	glColor4ub(0,0,0,196);

	glLineWidth(2.0f);
	glBegin(GL_LINES);
	for (int i = 0; i < cset.nconts; ++i)
	{
		const rcContour* cont = &cset.conts[i];
		getContourCenter(cont, orig, cs, ch, pos);
		for (int j = 0; j < cont->nverts; ++j)
		{
			const int* v = &cont->verts[j*4];
			if (v[3] == 0 || (unsigned short)v[3] < cont->reg) continue;
			const rcContour* cont2 = findContourFromSet(cset, (unsigned short)v[3]);
			if (cont2)
			{
				getContourCenter(cont2, orig, cs, ch, pos2);
				drawArc(pos, pos2);
			}
		}
	}
	glEnd();

	float col[4] = { 1,1,1,alpha };
	
	glPointSize(7.0f);
	glBegin(GL_POINTS);
	for (int i = 0; i < cset.nconts; ++i)
	{
		const rcContour* cont = &cset.conts[i];
		intToCol(cont->reg, col);
		col[0] *= 0.5f;
		col[1] *= 0.5f;
		col[2] *= 0.5f;
		glColor4fv(col);
		getContourCenter(cont, orig, cs, ch, pos);
		glVertex3fv(pos);
	}
	glEnd();
	
	
	glLineWidth(1.0f);
	glPointSize(1.0f);
}

void rcDebugDrawRawContours(const rcContourSet& cset, const float alpha)
{
	const float* orig = cset.bmin;
	const float cs = cset.cs;
	const float ch = cset.ch;
	float col[4] = { 1,1,1,alpha };
	glLineWidth(2.0f);
	glPointSize(2.0f);
	for (int i = 0; i < cset.nconts; ++i)
	{
		const rcContour& c = cset.conts[i];
		intToCol(c.reg, col);
		glColor4fv(col);
		glBegin(GL_LINE_LOOP);
		for (int j = 0; j < c.nrverts; ++j)
		{
			const int* v = &c.rverts[j*4];
			float fx = orig[0] + v[0]*cs;
			float fy = orig[1] + (v[1]+1+(i&1))*ch;
			float fz = orig[2] + v[2]*cs;
			glVertex3f(fx,fy,fz);
		}
		glEnd();

		col[0] *= 0.5f;
		col[1] *= 0.5f;
		col[2] *= 0.5f;
		glColor4fv(col);		

		glBegin(GL_POINTS);
		for (int j = 0; j < c.nrverts; ++j)
		{
			const int* v = &c.rverts[j*4];
			
			float off = 0;
			if (v[3] & RC_BORDER_VERTEX)
			{
				glColor4ub(255,255,255,255);
				off = ch*2;
			}
			else
			{
				glColor4fv(col);
			}
			
			float fx = orig[0] + v[0]*cs;
			float fy = orig[1] + (v[1]+1+(i&1))*ch + off;
			float fz = orig[2] + v[2]*cs;
			glVertex3f(fx,fy,fz);
		}
		glEnd();
	}
	glLineWidth(1.0f);
	glPointSize(1.0f);
}

void rcDebugDrawContours(const rcContourSet& cset, const float alpha)
{
	const float* orig = cset.bmin;
	const float cs = cset.cs;
	const float ch = cset.ch;
	float col[4] = { 1,1,1,1 };
	glLineWidth(2.5f);
	glPointSize(3.0f);
	for (int i = 0; i < cset.nconts; ++i)
	{
		const rcContour& c = cset.conts[i];
		intToCol(c.reg, col);
		glColor4fv(col);

		glBegin(GL_LINE_LOOP);
		for (int j = 0; j < c.nverts; ++j)
		{
			const int* v = &c.verts[j*4];
			float fx = orig[0] + v[0]*cs;
			float fy = orig[1] + (v[1]+1+(i&1))*ch;
			float fz = orig[2] + v[2]*cs;
			glVertex3f(fx,fy,fz);
		}
		glEnd();

		col[0] *= 0.5f;
		col[1] *= 0.5f;
		col[2] *= 0.5f;
		glColor4fv(col);		
		glBegin(GL_POINTS);
		for (int j = 0; j < c.nverts; ++j)
		{
			const int* v = &c.verts[j*4];
			float off = 0;
			if (v[3] & RC_BORDER_VERTEX)
			{
				glColor4ub(255,255,255,255);
				off = ch*2;
			}
			else
			{
				glColor4fv(col);
			}

			float fx = orig[0] + v[0]*cs;
			float fy = orig[1] + (v[1]+1+(i&1))*ch + off;
			float fz = orig[2] + v[2]*cs;
			glVertex3f(fx,fy,fz);
		}
		glEnd();
	}
	glLineWidth(1.0f);
	glPointSize(1.0f);
}

void rcDebugDrawPolyMesh(const struct rcPolyMesh& mesh)
{
	const int nvp = mesh.nvp;
	const float cs = mesh.cs;
	const float ch = mesh.ch;
	const float* orig = mesh.bmin;
	float col[4] = {1,1,1,0.75f};
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < mesh.npolys; ++i)
	{
		const unsigned short* p = &mesh.polys[i*nvp*2];
		intToCol(i, col);
		glColor4fv(col);
		unsigned short vi[3];
		for (int j = 2; j < nvp; ++j)
		{
			if (p[j] == 0xffff) break;
			vi[0] = p[0];
			vi[1] = p[j-1];
			vi[2] = p[j];
			for (int k = 0; k < 3; ++k)
			{
				const unsigned short* v = &mesh.verts[vi[k]*3];
				const float x = orig[0] + v[0]*cs;
				const float y = orig[1] + (v[1]+1)*ch;
				const float z = orig[2] + v[2]*cs;
				glVertex3f(x, y, z);
			}
		}
	}
	glEnd();

	// Draw tri boundaries
	glColor4ub(0,48,64,32);
	glLineWidth(1.5f);
	glBegin(GL_LINES);
	for (int i = 0; i < mesh.npolys; ++i)
	{
		const unsigned short* poly = &mesh.polys[i*nvp*2];
		for (int j = 0; j < nvp; ++j)
		{
			if (poly[j] == 0xffff) break;
			if (poly[nvp+j] == 0xffff) continue;
			int vi[2];
			vi[0] = poly[j];
			if (j+1 >= nvp || poly[j+1] == 0xffff)
				vi[1] = poly[0];
			else
				vi[1] = poly[j+1];
			for (int k = 0; k < 2; ++k)
			{
				const unsigned short* v = &mesh.verts[vi[k]*3];
				const float x = orig[0] + v[0]*cs;
				const float y = orig[1] + (v[1]+1)*ch + 0.1f;
				const float z = orig[2] + v[2]*cs;
				glVertex3f(x, y, z);
			}
		}
	}
	glEnd();
	
	// Draw boundaries
	glLineWidth(2.5f);
	glColor4ub(0,48,64,220);
	glBegin(GL_LINES);
	for (int i = 0; i < mesh.npolys; ++i)
	{
		const unsigned short* poly = &mesh.polys[i*nvp*2];
		for (int j = 0; j < nvp; ++j)
		{
			if (poly[j] == 0xffff) break;
			if (poly[nvp+j] != 0xffff) continue;
			int vi[2];
			vi[0] = poly[j];
			if (j+1 >= nvp || poly[j+1] == 0xffff)
				vi[1] = poly[0];
			else
				vi[1] = poly[j+1];
			for (int k = 0; k < 2; ++k)
			{
				const unsigned short* v = &mesh.verts[vi[k]*3];
				const float x = orig[0] + v[0]*cs;
				const float y = orig[1] + (v[1]+1)*ch + 0.1f;
				const float z = orig[2] + v[2]*cs;
				glVertex3f(x, y, z);
			}
		}
	}
	glEnd();
	glLineWidth(1.0f);
	
	glPointSize(3.0f);
	glColor4ub(0,0,0,220);
	glBegin(GL_POINTS);
	for (int i = 0; i < mesh.nverts; ++i)
	{
		const unsigned short* v = &mesh.verts[i*3];
		const float x = orig[0] + v[0]*cs;
		const float y = orig[1] + (v[1]+1)*ch + 0.1f;
		const float z = orig[2] + v[2]*cs;
		glVertex3f(x, y, z);
	}
	glEnd();
	glPointSize(1.0f);
}

void rcDebugDrawPolyMeshDetail(const struct rcPolyMeshDetail& dmesh)
{
	float col[4] = {1,1,1,0.75f};
	
	glBegin(GL_TRIANGLES);
	for (int i = 0; i < dmesh.nmeshes; ++i)
	{
		const unsigned short* m = &dmesh.meshes[i*4];
		const unsigned short bverts = m[0];
		const unsigned short btris = m[2];
		const unsigned short ntris = m[3];
		const float* verts = &dmesh.verts[bverts*3];
		const unsigned char* tris = &dmesh.tris[btris*4];

		intToCol(i, col);
		glColor4fv(col);
		for (int j = 0; j < ntris; ++j)
		{
			glVertex3fv(&verts[tris[j*4+0]*3]);
			glVertex3fv(&verts[tris[j*4+1]*3]);
			glVertex3fv(&verts[tris[j*4+2]*3]);
		}
	}
	glEnd();

	// Internal edges.
	glLineWidth(1.0f);
	glColor4ub(0,0,0,64);
	glBegin(GL_LINES);
	for (int i = 0; i < dmesh.nmeshes; ++i)
	{
		const unsigned short* m = &dmesh.meshes[i*4];
		const unsigned short bverts = m[0];
		const unsigned short btris = m[2];
		const unsigned short ntris = m[3];
		const float* verts = &dmesh.verts[bverts*3];
		const unsigned char* tris = &dmesh.tris[btris*4];
		
		for (int j = 0; j < ntris; ++j)
		{
			const unsigned char* t = &tris[j*4];
			for (int k = 0, kp = 2; k < 3; kp=k++)
			{
				unsigned char ef = (t[3] >> (kp*2)) & 0x3;
				if (ef == 0)
				{
					// Internal edge
					if (t[kp] < t[k])
					{
						glVertex3fv(&verts[t[kp]*3]);
						glVertex3fv(&verts[t[k]*3]);
					}
				}
			}
		}
	}
	glEnd();
	
	// External edges.
	glLineWidth(2.0f);
	glColor4ub(0,0,0,64);
	glBegin(GL_LINES);
	for (int i = 0; i < dmesh.nmeshes; ++i)
	{
		const unsigned short* m = &dmesh.meshes[i*4];
		const unsigned short bverts = m[0];
		const unsigned short btris = m[2];
		const unsigned short ntris = m[3];
		const float* verts = &dmesh.verts[bverts*3];
		const unsigned char* tris = &dmesh.tris[btris*4];
		
		for (int j = 0; j < ntris; ++j)
		{
			const unsigned char* t = &tris[j*4];
			for (int k = 0, kp = 2; k < 3; kp=k++)
			{
				unsigned char ef = (t[3] >> (kp*2)) & 0x3;
				if (ef != 0)
				{
					// Ext edge
					glVertex3fv(&verts[t[kp]*3]);
					glVertex3fv(&verts[t[k]*3]);
				}
			}
		}
	}
	glEnd();
	
	glLineWidth(1.0f);

	glPointSize(3.0f);
	glBegin(GL_POINTS);
	for (int i = 0; i < dmesh.nmeshes; ++i)
	{
		const unsigned short* m = &dmesh.meshes[i*4];
		const unsigned short bverts = m[0];
		const unsigned short nverts = m[1];
		const float* verts = &dmesh.verts[bverts*3];
		for (int j = 0; j < nverts; ++j)
		{
			glColor4ub(0,0,0,64);
			glVertex3fv(&verts[j*3]);
		}
	}
	glEnd();
	glPointSize(1.0f);
}
