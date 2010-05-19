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

#ifndef RECAST_DEBUGDRAW_H
#define RECAST_DEBUGDRAW_H

inline int bit(int a, int b)
{
	return (a & (1 << b)) >> b;
}

inline void intToCol(int i, float* col)
{
	int	r = bit(i, 0) + bit(i, 3) * 2 + 1;
	int	g = bit(i, 1) + bit(i, 4) * 2 + 1;
	int	b = bit(i, 2) + bit(i, 5) * 2 + 1;
	col[0] = 1 - r*63.0f/255.0f;
	col[1] = 1 - g*63.0f/255.0f;
	col[2] = 1 - b*63.0f/255.0f;
}

void rcDebugDrawHeightfieldSolid(const struct rcHeightfield& hf);
void rcDebugDrawHeightfieldWalkable(const struct rcHeightfield& hf);

void rcDebugDrawMesh(const float* verts, int nverts, const int* tris, const float* normals, int ntris, const unsigned char* flags);
void rcDebugDrawMeshSlope(const float* verts, int nverts, const int* tris, const float* normals, int ntris, const float walkableSlopeAngle);

void rcDebugDrawCompactHeightfieldSolid(const struct rcCompactHeightfield& chf);
void rcDebugDrawCompactHeightfieldRegions(const struct rcCompactHeightfield& chf);
void rcDebugDrawCompactHeightfieldDistance(const struct rcCompactHeightfield& chf);

void rcDebugDrawRegionConnections(const struct rcContourSet& cset, const float alpha = 1.0f);
void rcDebugDrawRawContours(const struct rcContourSet& cset, const float alpha = 1.0f);
void rcDebugDrawContours(const struct rcContourSet& cset, const float alpha = 1.0f);
void rcDebugDrawPolyMesh(const struct rcPolyMesh& mesh);
void rcDebugDrawPolyMeshDetail(const struct rcPolyMeshDetail& dmesh);

void rcDebugDrawCylinderWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col);
void rcDebugDrawBoxWire(float minx, float miny, float minz, float maxx, float maxy, float maxz, const float* col);
void rcDebugDrawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz,
					const float* col1, const float* col2);
void rcDrawArc(const float* p0, const float* p1);

#endif // RECAST_DEBUGDRAW_H
