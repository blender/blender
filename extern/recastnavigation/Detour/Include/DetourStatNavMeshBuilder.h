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

#ifndef DETOURSTATNAVMESHBUILDER_H
#define DETOURSTATNAVMESHBUILDER_H

bool dtCreateNavMeshData(const unsigned short* verts, const int nverts,
						 const unsigned short* polys, const int npolys, const int nvp,
						 const float* bmin, const float* bmax, float cs, float ch,
						 const unsigned short* dmeshes, const float* dverts, const int ndverts,
						 const unsigned char* dtris, const int ndtris, 
						 unsigned char** outData, int* outDataSize);

int createBVTree(const unsigned short* verts, const int nverts,
						const unsigned short* polys, const int npolys, const int nvp,
						float cs, float ch, int nnodes, dtStatBVNode* nodes);

#endif // DETOURSTATNAVMESHBUILDER_H