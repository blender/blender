/*
 * Copyright 2006, NVIDIA Corporation Ignacio Castano <icastano@nvidia.com>
 * 
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "subd_face.h"
#include "subd_ring.h"
#include "subd_stencil.h"
#include "subd_vert.h"

#include "util_algorithm.h"
#include "util_debug.h"
#include "util_math.h"

CCL_NAMESPACE_BEGIN

/* Utility for sorting verts by index */

class vertex_compare
{
public:
	const vector<int>& index;
	vertex_compare(const vector<int>& index_) : index(index_) {}
	bool operator()(int a, int b) const { return index[a] < index[b]; }
};

/* Subd Face Ring */

SubdFaceRing::SubdFaceRing(SubdFace *face, SubdEdge *firstEdge)
{
	m_face = face;
	m_firstEdge = firstEdge;
	m_num_edges = face->num_edges();

	assert(m_num_edges == 3 || m_num_edges == 4);

	initVerts();
}

int SubdFaceRing::num_verts()
{
	return m_verts.size();
}

SubdVert *SubdFaceRing::vertexAt(int i)
{
	return m_verts[i];
}

int SubdFaceRing::vert_index(SubdVert *vertex)
{
	int count = this->num_verts();

	for(int i = 0; i < count; i++)
		if(m_verts[i] == vertex)
			return i;
	
	assert(0);
	return 0;
}

void SubdFaceRing::evaluate_stencils(float3 *P, StencilMask *mask, int num)
{
	/* first we sort verts by id. this way verts will always be added
	 * in the same order to ensure the exact same float ops happen for control
	 * points of other patches, so we get water-tight patches */
	int num_verts = m_verts.size();

	vector<int> vmap(num_verts);
	vector<int> vertid(num_verts);

	for(int v = 0; v < num_verts; v++) {
		vmap[v] = v;
		vertid[v] = m_verts[v]->id;
	}

	sort(vmap.begin(), vmap.end(), vertex_compare(vertid));

	/* then evaluate the stencils */
	for(int j = 0; j < num; j++) {
		float3 p = make_float3(0.0f, 0.0f, 0.0f);

		for(int i = 0; i < num_verts; i++) {
			int idx = vmap[i];
			p += m_verts[idx]->co * mask[j][idx];
		}

		P[j] = p;
	}
}

bool SubdFaceRing::is_triangle()
{
	return m_num_edges == 3;
}

bool SubdFaceRing::is_quad()
{
	return m_num_edges == 4;
}

int SubdFaceRing::num_edges()
{
	return m_num_edges;
}

bool SubdFaceRing::is_regular(SubdFace *face)
{
	if(!is_quad(face))
		return false;

	for(SubdFace::EdgeIterator it(face->edges()); !it.isDone(); it.advance()) {
		SubdEdge *edge = it.current();

		/* regular faces don't have boundary edges */
		if(edge->is_boundary())
			return false;

		/* regular faces only have ordinary verts */
		if(edge->vert->valence() != 4)
			return false;

		/* regular faces are only adjacent to quads */
		for(SubdVert::EdgeIterator eit(edge); !eit.isDone(); eit.advance())
			if(eit.current()->face == NULL || eit.current()->face->num_edges() != 4)
				return false;
	}

	return true;
}

bool SubdFaceRing::is_triangle(SubdFace *face)
{
	return face->num_edges() == 3;
}
bool SubdFaceRing::is_quad(SubdFace *face)
{
	return face->num_edges() == 4;
}

bool SubdFaceRing::is_boundary(SubdFace *face)
{
	/* note that face->is_boundary() returns a different result. That function
	 * returns true when any of the *edges* are on the boundary. however, this
	 * function returns true if any of the face *verts* are on the boundary.  */

	for(SubdFace::EdgeIterator it(face->edges()); !it.isDone(); it.advance()) {
		SubdEdge *edge = it.current();

		if(edge->vert->is_boundary())
			return true;
	}

	return false;
}

void SubdFaceRing::initVerts()
{
	m_verts.reserve(16);

	/* add face verts */
	for(SubdFace::EdgeIterator it(m_firstEdge); !it.isDone(); it.advance()) {
		SubdEdge *edge = it.current();
		m_verts.push_back(edge->from());
	}
	
	// @@ Add support for non manifold surfaces!
	// The fix: 
	// - not all colocals should point to the same edge.
	// - multiple colocals could belong to different boundaries, make sure they point to the right one.

	// @@ When the face neighborhood wraps that could result in overlapping stencils. 

	// Add surronding verts.
	for(SubdFace::EdgeIterator it(m_firstEdge); !it.isDone(); it.advance()) {
		SubdEdge *firstEdge = it.current();
	
		int i = 0;

		/* traverse edges around vertex */
		for(SubdVert::ReverseEdgeIterator eit(firstEdge); !eit.isDone(); eit.advance(), i++) {
			SubdEdge *edge = eit.current();

			assert(edge->from()->co == firstEdge->from()->co);

			add_vert(edge->to());

			if(edge->face != NULL)
				add_vert(edge->next->to());
		}

		assert(i == firstEdge->from()->valence());
	}
}


void SubdFaceRing::add_vert(SubdVert *vertex)
{
	/* avoid having duplicate verts */
	if(!has_vert(vertex))
		m_verts.push_back(vertex);
}

bool SubdFaceRing::has_vert(SubdVert *vertex)
{
	int num_verts = m_verts.size();

	for(int i = 0; i < num_verts; i++)
		if(m_verts[i]->co == vertex->co)
			return true;

	return false;
}

CCL_NAMESPACE_END

