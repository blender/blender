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

#include <stdio.h>

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_debug.h"
#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

/* Subd Vertex */

class SubdVert
{
public:
	int id;
	float3 co;
	
	SubdVert(int id_)
	{
		id = id_;
		co = make_float3(0.0f, 0.0f, 0.0f);
	}
};

/* Subd Face */

class SubdFace
{
public:
	int id;
	int numverts;
	int verts[4];

	SubdFace(int id_)
	{
		id = id_;
		numverts = 0;
	}
};

/* Subd Mesh */

SubdMesh::SubdMesh()
{
}

SubdMesh::~SubdMesh()
{
	foreach(SubdVert *vertex, verts)
		delete vertex;
	foreach(SubdFace *face, faces)
		delete face;

	verts.clear();
	faces.clear();
}

SubdVert *SubdMesh::add_vert(const float3& co)
{
	SubdVert *v = new SubdVert(verts.size());
	v->co = co;
	verts.push_back(v);

	return v;
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2)
{
	int index[3] = {v0, v1, v2};
	return add_face(index, 3);
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2, int v3)
{
	int index[4] = {v0, v1, v2, v3};
	return add_face(index, 4);
}

SubdFace *SubdMesh::add_face(int *index, int num)
{
	/* skip ngons */
	if(num < 3 || num > 4)
		return NULL;

	SubdFace *f = new SubdFace(faces.size());

	for(int i = 0; i < num; i++)
		f->verts[i] = index[i];

	f->numverts = num;
	faces.push_back(f);

	return f;
}

bool SubdMesh::finish()
{
	return true;
}

void SubdMesh::tessellate(DiagSplit *split)
{
	int num_faces = faces.size();
		        
	for(int f = 0; f < num_faces; f++) {
		SubdFace *face = faces[f];
		Patch *patch;
		float3 *hull;

		if(face->numverts == 3) {
			LinearTrianglePatch *lpatch = new LinearTrianglePatch();
			hull = lpatch->hull;
			patch = lpatch;
		}
		else if(face->numverts == 4) {
			LinearQuadPatch *lpatch = new LinearQuadPatch();
			hull = lpatch->hull;
			patch = lpatch;
		}
		else {
			assert(0); /* n-gons should have been split already */
			continue;
		}

		for(int i = 0; i < face->numverts; i++)
			hull[i] = verts[face->verts[i]]->co;

		if(face->numverts == 4)
			swap(hull[2], hull[3]);

		if(patch->is_triangle())
			split->split_triangle(patch);
		else
			split->split_quad(patch);

		delete patch;
	}
}

CCL_NAMESPACE_END

