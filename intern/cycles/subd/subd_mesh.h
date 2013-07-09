/*
 * Original code in the public domain -- castanyo@yahoo.es
 *
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SUBD_MESH_H__
#define __SUBD_MESH_H__

#include "util_map.h"
#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class SubdFace;
class SubdVert;
class SubdEdge;

class DiagSplit;
class Mesh;

/* Subd Mesh, half edge based for dynamic mesh manipulation */

class SubdMesh
{
public:
	vector<SubdVert*> verts;
	vector<SubdEdge*> edges;
	vector<SubdFace*> faces;

	SubdMesh();
	~SubdMesh();

	SubdVert *add_vert(const float3& co);
	
	SubdFace *add_face(int v0, int v1, int v2);
	SubdFace *add_face(int v0, int v1, int v2, int v3);
	SubdFace *add_face(int *index, int num);

	bool link_boundary();
	void tessellate(DiagSplit *split, bool linear,
		Mesh *mesh, int shader, bool smooth);

protected:
	bool can_add_face(int *index, int num);
	bool can_add_edge(int i, int j);
	SubdEdge *add_edge(int i, int j);
	SubdEdge *find_edge(int i, int j);
	void link_boundary_edge(SubdEdge *edge);
	
	struct Key {
		Key() {}
		Key(int v0, int v1) : p0(v0), p1(v1) {}

		bool operator<(const Key& k) const
		{ return (p0 < k.p0 || (p0 == k.p0 && p1 < k.p1)); }

		int p0, p1;
	};

	map<Key, SubdEdge *> edge_map;
};

CCL_NAMESPACE_END

#endif /* __SUBD_MESH_H__ */

