/*
 * Copyright 2006, NVIDIA Corporation Ignacio Castano <icastano@nvidia.com>
 *
 * Modifications copyright (c) 2011, Blender Foundation. All rights reserved.
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

#include "subd_build.h"
#include "subd_edge.h"
#include "subd_face.h"
#include "subd_ring.h"
#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_stencil.h"
#include "subd_vert.h"

#include "util_algorithm.h"
#include "util_debug.h"
#include "util_math.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

/* Subd Builder */

SubdBuilder *SubdBuilder::create(bool linear)
{
	if(linear)
		return new SubdLinearBuilder();
	else
		return new SubdAccBuilder();
}

/* Gregory ACC Stencil */

class GregoryAccStencil {
public:
	SubdFaceRing *ring;
	StencilMask stencil[20];

	GregoryAccStencil(SubdFaceRing *ring_)
	{
		ring = ring_;

		for(int i = 0; i < 20; i++)
			stencil[i].resize(ring->num_verts());
	}

	StencilMask& get(int i)
	{
		assert(i < 20);
		return stencil[i];
	}

	float& get(int i, SubdVert *vert)
	{
		assert(i < 20);
		return stencil[i][ring->vert_index(vert)];
	}
};

static float pseudoValence(SubdVert *vert)
{
	float valence = (float)vert->valence();

	if(vert->is_boundary()) {
		/* we treat boundary verts as being half a closed mesh. corners are
		 * special case. n = 4 for corners and n = 2*(n-1) for boundaries. */
		if(valence == 2) return 4;
		return (valence - 1)*2;
	}

	return valence;
}

/* Subd ACC Builder */

SubdAccBuilder::SubdAccBuilder()
{
}

SubdAccBuilder::~SubdAccBuilder()
{
}

Patch *SubdAccBuilder::run(SubdFace *face)
{
	SubdFaceRing ring(face, face->edge);
	GregoryAccStencil stencil(&ring);
	float3 position[20];

	computeCornerStencil(&ring, &stencil);
	computeEdgeStencil(&ring, &stencil);
	computeInteriorStencil(&ring, &stencil);

	ring.evaluate_stencils(position, stencil.stencil, 20);

	if(face->num_edges() == 3) {
		GregoryTrianglePatch *patch = new GregoryTrianglePatch();
		memcpy(patch->hull, position, sizeof(float3)*20);
		return patch;
	}
	else if(face->num_edges() == 4)  {
		GregoryQuadPatch *patch = new GregoryQuadPatch();
		memcpy(patch->hull, position, sizeof(float3)*20);
		return patch;
	}

	assert(0); /* n-gons should have been split already */
	return NULL;
}

/* Gregory Patch */

void SubdAccBuilder::computeCornerStencil(SubdFaceRing *ring, GregoryAccStencil *stencil)
{
	const int cornerIndices[7] = {8, 11, 19, 16,   6, 9, 12};
	int primitiveOffset = ring->is_quad()? 0: 4;

	SubdEdge *firstEdge = ring->firstEdge();

	/* compute corner control points */
	int v = 0;

	for(SubdFace::EdgeIterator it(firstEdge); !it.isDone(); it.advance(), v++) {
		SubdVert *vert = it.current()->from();
		int valence = vert->valence();
		int cid = cornerIndices[primitiveOffset+v];

		if(vert->is_boundary()) {
			/* compute vertex limit position */
			SubdEdge *edge0 = vert->edge;
			SubdEdge *edge1 = vert->edge->prev;

			assert(edge0->face == NULL);
			assert(edge0->to() != vert);
			assert(edge1->face == NULL);
			assert(edge1->from() != vert);

			stencil->get(cid, vert) = 2.0f/3.0f;
			stencil->get(cid, edge0->to()) = 1.0f/6.0f;
			stencil->get(cid, edge1->from()) = 1.0f/6.0f;

			assert(stencil->get(cid).is_normalized());
		}
		else {
			stencil->get(cid, vert) = 3.0f*valence*valence;

			for(SubdVert::EdgeIterator eit(vert->edge); !eit.isDone(); eit.advance()) {
				SubdEdge *edge = eit.current();
				assert(vert->co == edge->from()->co);

				stencil->get(cid, edge->to()) = 12.0f;

				if(SubdFaceRing::is_triangle(edge->face)) {
					/* distribute weight to all verts */
					stencil->get(cid, vert) += 1.0f;
					stencil->get(cid, edge->to()) += 1.0f;
					stencil->get(cid, edge->next->to()) += 1.0f;
				}
				else
					stencil->get(cid, edge->next->to()) = 3.0f;
			}

			/* normalize stencil. */
			stencil->get(cid).normalize();
		}
	}
}

void SubdAccBuilder::computeEdgeStencil(SubdFaceRing *ring, GregoryAccStencil *stencil)
{
	const int cornerIndices[7] = {8, 11, 19, 16,    6,  9, 12};
	const int edge1Indices[7] = {9, 13, 18, 14,    7, 10, 13};
	const int edge2Indices[7] = {12, 10, 15, 17,    14,  8, 11};
	int primitiveOffset = ring->is_quad()? 0: 4;

	float tangentScales[14] = {
		0.0f, 0.0f, 0.0f, 0.667791f, 1.0f,
		1.11268f, 1.1284f, 1.10289f, 1.06062f,
		1.01262f, 0.963949f, 0.916926f, 0.872541f, 0.831134f
	};

	SubdEdge *firstEdge = ring->firstEdge();

	/* compute corner / edge control points */
	int v = 0;

	for(SubdFace::EdgeIterator it(firstEdge); !it.isDone(); it.advance(), v++) {
		SubdVert *vert = it.current()->from();
		int valence = vert->valence();
		int cid = cornerIndices[primitiveOffset+v];

		int i1 = 0, i2 = 0, j = 0;

		for(SubdVert::EdgeIterator eit(vert->edge); !eit.isDone(); eit.advance(), j++) {
			SubdEdge *edge = eit.current();

			/* find index of "our" edge for edge control points */
			if(edge == it.current())
				i1 = j;
			if(edge == it.current()->prev->pair)
				i2 = j;
		}

		if(vert->is_boundary()) {
			int num_verts = ring->num_verts();
			StencilMask	r0(num_verts);
			StencilMask	r1(num_verts);

			computeBoundaryTangentStencils(ring, vert, r0, r1);

			int k = valence - 1;
			float omega = M_PI_F / k;

			int eid1 = edge1Indices[primitiveOffset + v];
			int eid2 = edge2Indices[primitiveOffset + v];

			if(it.current()->is_boundary()) {
				assert(it.current()->from() == vert);

				stencil->get(eid1, vert) = 2.0f / 3.0f;
				stencil->get(eid1, it.current()->to()) = 1.0f / 3.0f;

				assert(stencil->get(eid1).is_normalized());

				if(valence == 2) {
					for(int i = 0; i < num_verts; i++)
						stencil->get(eid1)[i] += r0[i] * 0.0001f;
				}
			}
			else {
				stencil->get(eid1) = stencil->get(cid);

				/* compute index of it.current() around vert */
				int idx = 0;

				for(SubdVert::EdgeIterator eit(vert->edges()); !eit.isDone(); eit.advance(), idx++)
					if(eit.current() == it.current())
						break;

				assert(idx != valence);

				float c = cosf(idx * omega);
				float s = sinf(idx * omega);

				for(int i = 0; i < num_verts; i++)
					stencil->get(eid1)[i] += (r0[i] * s + r1[i] * c) / 3.0f;
			}

			if(it.current()->prev->is_boundary()) {
				assert(it.current()->prev->pair->from() == vert);

				stencil->get(eid2, vert) = 2.0f / 3.0f;
				stencil->get(eid2, it.current()->prev->pair->to()) = 1.0f / 3.0f;

				assert(stencil->get(eid2).is_normalized());

				if(valence == 2) {
					for(int i = 0; i < num_verts; i++)
						stencil->get(eid2)[i] += r0[i] * 0.0001f;
				}
			}
			else {
				stencil->get(eid2) = stencil->get(cid);

				/* compute index of it.current() around vert */
				int idx = 0;

				for(SubdVert::EdgeIterator eit(vert->edges()); !eit.isDone(); eit.advance(), idx++)
					if(eit.current() == it.current()->prev->pair)
						break;

				assert(idx != valence);

				float c = cosf(idx * omega);
				float s = sinf(idx * omega);

				for(int i = 0; i < num_verts; i++)
					stencil->get(eid2)[i] += (r0[i] * s + r1[i] * c) / 3;
			}
		}
		else {
			float costerm = cosf(M_PI_F / valence);
			float sqrtterm = sqrtf(4.0f + costerm*costerm);

			/* float tangentScale = 1.0f; */
			float tangentScale = tangentScales[min(valence, 13U)];

			float alpha = (1.0f +  costerm / sqrtterm) / (3.0f * valence) * tangentScale;
			float beta  = 1.0f / (3.0f * valence * sqrtterm) * tangentScale;


			int eid1 = edge1Indices[primitiveOffset + v];
			int eid2 = edge2Indices[primitiveOffset + v];

			stencil->get(eid1) = stencil->get(cid);
			stencil->get(eid2) = stencil->get(cid);

			int j = 0;
			for(SubdVert::EdgeIterator eit(vert->edges()); !eit.isDone(); eit.advance(), j++) {
				SubdEdge *edge = eit.current();
				assert(vert->co == edge->from()->co);

				float costerm1_a = cosf(M_PI_F * 2 * (j-i1) / valence);
				float costerm1_b = cosf(M_PI_F * (2 * (j-i1)-1) / valence); /* -1 instead of +1 b/c of edge->next->to() */

				float costerm2_a = cosf(M_PI_F * 2 * (j-i2) / valence);
				float costerm2_b = cosf(M_PI_F * (2 * (j-i2)-1) / valence); /* -1 instead of +1 b/c of edge->next->to() */


				stencil->get(eid1, edge->to()) += alpha * costerm1_a;
				stencil->get(eid2, edge->to()) += alpha * costerm2_a;

				if(SubdFaceRing::is_triangle(edge->face)) {
					/* @@ this probably does not provide watertight results!! (1/3 + 1/3 + 1/3 != 1) */

					/* distribute weight to all verts */
					stencil->get(eid1, vert) += beta * costerm1_b / 3.0f;				
					stencil->get(eid1, edge->to()) += beta * costerm1_b / 3.0f;
					stencil->get(eid1, edge->next->to()) += beta * costerm1_b / 3.0f;

					stencil->get(eid2, vert) += beta * costerm2_b / 3.0f;
					stencil->get(eid2, edge->to()) += beta * costerm2_b / 3.0f;
					stencil->get(eid2, edge->next->to()) += beta * costerm2_b / 3.0f;
				}
				else {
					stencil->get(eid1, edge->next->to()) += beta * costerm1_b;
					stencil->get(eid2, edge->next->to()) += beta * costerm2_b;
				}
			}
		}
	}
}

void SubdAccBuilder::computeInteriorStencil(SubdFaceRing *ring, GregoryAccStencil *stencil)
{
	static int corner1Indices[7] = {8, 11, 19, 16,    6, 9, 12};
	static int corner2Indices[7] = {11, 19, 16, 8,    9, 12, 6};
	static int edge1Indices[7] = {9, 13, 18, 14,    7, 10, 13};
	static int edge2Indices[7] = {10, 15, 17, 12,    8, 11, 14};
	static int interior1Indices[7] = {1, 3, 6, 4,    1, 3, 5};
	static int interior2Indices[7] = {2, 7, 5, 0,    2, 4, 0};

	int primitiveOffset = ring->is_quad()? 0: 4;

	SubdFace * face = ring->face();
	SubdEdge *firstEdge = ring->firstEdge();

	/* interior control points */
	int v = 0;
	for(SubdFace::EdgeIterator it(firstEdge); !it.isDone(); it.advance(), v++) {
		SubdEdge *edge = it.current();

		if(edge->is_boundary()) {
			float valence1 = pseudoValence(edge->from());
			float valence2 = pseudoValence(edge->to());

			float weights1[4];
			float weights2[4];

			if(ring->is_quad()) {
				weights1[0] = 3 * valence1;
				weights1[1] = 6;
				weights1[2] = 3;
				weights1[3] = 6;

				weights2[0] = 6;
				weights2[1] = 3 * valence2;
				weights2[2] = 6;
				weights2[3] = 3;
			}
			else {
				assert(ring->is_triangle());
				weights1[0] = 3 * valence1 + 1;
				weights1[1] = 7;
				weights1[2] = 7;

				weights2[0] = 7;
				weights2[1] = 3 * valence2 + 1;
				weights2[2] = 7;
			}

			int idx1 = interior1Indices[primitiveOffset+v];
			int idx2 = interior2Indices[primitiveOffset+v];

			int i = 0;
			for(SubdFace::EdgeIterator it(face->edges(edge)); !it.isDone(); it.advance(), i++) {
				SubdVert *vert = it.current()->from();
				stencil->get(idx1, vert) += weights1[i];
				stencil->get(idx2, vert) += weights2[i];
			}

			stencil->get(idx1).normalize();
			stencil->get(idx2).normalize();
		}
		else {
			SubdVert *e0 = edge->from();
			float costerm0 = cosf(2.0f * M_PI_F / pseudoValence(e0));

			SubdVert *f0 = edge->to();
			float costerm1 = cosf(2.0f * M_PI_F / pseudoValence(f0));

			/*  p0 +------+ q0
			 *	 |	  |
			 *  f0 +======+ e0 <=== current edge
			 *	 |	  |
			 *  p1 +------+ q1
			 */

			SubdVert *q0 = edge->next->to();
			SubdVert *p0 = edge->prev->from();

			SubdVert *p1 = edge->pair->next->to();
			SubdVert *q1 = edge->pair->prev->from();


			StencilMask	x(ring->num_verts());
			StencilMask	y(ring->num_verts());

			for(int i = 0; i < ring->num_verts(); i++) {
				x[i] =
					(costerm1 * stencil->get(corner1Indices[primitiveOffset+v])[i] -
					(2*costerm0 + costerm1) * stencil->get(edge1Indices[primitiveOffset+v])[i] +
					2*costerm0 * stencil->get(edge2Indices[primitiveOffset+v])[i]) / 3.0f;
			}

			/* y = (2*( midedgeA1 - midedgeB1) + 4*(centroidA - centroidB))/18.0f; */
			y[ring->vert_index(p0)] = 1;
			y[ring->vert_index(p1)] = -1;

			/* add centroidA */
			if(ring->is_triangle()) {
				y[ring->vert_index(p0)] += 4.0f / 3.0f;
				y[ring->vert_index(e0)] += 4.0f / 3.0f;
				y[ring->vert_index(f0)] += 4.0f / 3.0f;
			}
			else {
				y[ring->vert_index(p0)] += 1;
				y[ring->vert_index(q0)] += 1;
				y[ring->vert_index(e0)] += 1;
				y[ring->vert_index(f0)] += 1;
			}

			/* sub centroidB */
			if(SubdFaceRing::is_triangle(edge->pair->face)) {
				y[ring->vert_index(p1)] -= 4.0f / 3.0f;
				y[ring->vert_index(e0)] -= 4.0f / 3.0f;
				y[ring->vert_index(f0)] -= 4.0f / 3.0f;

			}
			else {
				y[ring->vert_index(p1)] -= 1;
				y[ring->vert_index(q1)] -= 1;
				y[ring->vert_index(e0)] -= 1;
				y[ring->vert_index(f0)] -= 1;
			}

			y /= 18.0f;

			if(ring->is_triangle()) {
				x *= 3.0f / 4.0f;
				y *= 3.0f / 4.0f;
			}

			/* this change makes the triangle boundaries smoother, but distorts the quads next to them */
#if 0
			if(ring->is_triangle() || SubdFaceRing::is_triangle(edge->pair->face)) {
				y *= 4.0f / 3.0f;
			}
#endif

			stencil->get(interior1Indices[primitiveOffset+v]) = stencil->get(edge1Indices[primitiveOffset+v]);
			stencil->get(interior1Indices[primitiveOffset+v]) += x;
			stencil->get(interior1Indices[primitiveOffset+v]) += y;

			for(int i = 0; i < ring->num_verts(); i++) {
				x[i] =
					(costerm0 * stencil->get(corner2Indices[primitiveOffset+v])[i] -
					(2*costerm1 + costerm0) * stencil->get(edge2Indices[primitiveOffset+v])[i] + 
					2*costerm1 * stencil->get(edge1Indices[primitiveOffset+v])[i]) / 3.0f;
			}

			/* y = (2*( midedgeA2 - midedgeB2) + 4*(centroidA - centroidB))/18.0f; */
			y = 0.0f;

			/* (2*( midedgeA2 - midedgeB2) */
			y[ring->vert_index(q0)] = 1;
			y[ring->vert_index(q1)] = -1;

			/* add centroidA */
			if(ring->is_triangle()) {
				y[ring->vert_index(p0)] += 4.0f / 3.0f;
				y[ring->vert_index(e0)] += 4.0f / 3.0f;
				y[ring->vert_index(f0)] += 4.0f / 3.0f;
			}
			else {
				y[ring->vert_index(p0)] += 1;
				y[ring->vert_index(q0)] += 1;
				y[ring->vert_index(e0)] += 1;
				y[ring->vert_index(f0)] += 1;
			}

			/* sub centroidB */
			if(SubdFaceRing::is_triangle(edge->pair->face)) {
				y[ring->vert_index(p1)] -= 4.0f / 3.0f;
				y[ring->vert_index(e0)] -= 4.0f / 3.0f;
				y[ring->vert_index(f0)] -= 4.0f / 3.0f;

			}
			else {
				y[ring->vert_index(p1)] -= 1;
				y[ring->vert_index(q1)] -= 1;
				y[ring->vert_index(e0)] -= 1;
				y[ring->vert_index(f0)] -= 1;
			}

			y /= 18.0f;

			if(ring->is_triangle()) {
				x *= 3.0f / 4.0f;
				y *= 3.0f / 4.0f;
			}

			/* this change makes the triangle boundaries smoother, but distorts the quads next to them. */
#if 0
			if(ring->is_triangle() || SubdFaceRing::is_triangle(edge->pair->face))
				y *= 4.0f / 3.0f;
#endif

			stencil->get(interior2Indices[primitiveOffset+v]) = stencil->get(edge2Indices[primitiveOffset+v]);
			stencil->get(interior2Indices[primitiveOffset+v]) += x;
			stencil->get(interior2Indices[primitiveOffset+v]) += y;
		}
	}
}

void SubdAccBuilder::computeBoundaryTangentStencils(SubdFaceRing *ring, SubdVert *vert, StencilMask & r0, StencilMask & r1)
{
	assert(vert->is_boundary());
	assert(r0.size() == ring->num_verts());
	assert(r1.size() == ring->num_verts());

	SubdEdge *edge0 = vert->edge;
	assert(edge0->face == NULL);
	assert(edge0->to() != vert);

	SubdEdge *edgek = vert->edge->prev;
	assert(edgek->face == NULL);
	assert(edgek->from() != vert);

	int valence = vert->valence();

	int k = valence - 1;
	float omega = M_PI_F / k;
	float s = sinf(omega);
	float c = cosf(omega);

	float factor = 1.0f / (3 * k + c);

	float gamma = -4 * s * factor;
	r0[ring->vert_index(vert)] = gamma;
	/* r1[ring->vert_index(vert)] = 0; */

	float salpha0 = -((1 + 2 * c) * sqrtf(1 + c)) * factor / sqrtf(1 - c);
	float calpha0 = 1.0f / 2.0f;

	r0[ring->vert_index(edge0->to())] = salpha0;
	r1[ring->vert_index(edge0->to())] = calpha0;

	float salphak = salpha0;
	float calphak = -1.0f / 2.0f;

	r0[ring->vert_index(edgek->from())] = salphak;
	r1[ring->vert_index(edgek->from())] = calphak;

	int j = 0;
	for(SubdVert::EdgeIterator it(vert->edges()); !it.isDone(); it.advance(), j++) {
		SubdEdge *edge = it.current();

		if(j == k) break;

		SubdVert *p = edge->to();
		SubdVert *q = edge->pair->prev->from();

		float alphaj = 4 * sinf(j * omega) * factor;
		float betaj = (sinf(j * omega) + sinf((j + 1) * omega)) * factor;

		if(j != 0)
			r0[ring->vert_index(p)] += alphaj;

		if(edge->pair->prev->prev->prev == edge->pair) {
			r0[ring->vert_index(vert)] += betaj / 3.0f;
			r0[ring->vert_index(edge->pair->from())] += betaj / 3.0f;
			r0[ring->vert_index(q)] += betaj / 3.0f;
		}
		else
			r0[ring->vert_index(q)] += betaj;
	}

	if(valence == 2) {
		/* r0 perpendicular to r1 */
		r0[ring->vert_index(vert)] = -4.0f / 3.0f;
		r0[ring->vert_index(edgek->from())] = 1.0f / 2.0f;
		r0[ring->vert_index(edge0->to())] = 1.0f / 2.0f;
		r0[ring->vert_index(edge0->next->to())] = 1.0f / 3.0f;
	}
}

/* Subd Linear Builder */

SubdLinearBuilder::SubdLinearBuilder()
{
}

SubdLinearBuilder::~SubdLinearBuilder()
{
}

Patch *SubdLinearBuilder::run(SubdFace *face)
{
	Patch *patch;
	float3 *hull;

	if(face->num_edges() == 3) {
		LinearTrianglePatch *lpatch = new LinearTrianglePatch();
		hull = lpatch->hull;
		patch = lpatch;
	}
	else if(face->num_edges() == 4)  {
		LinearQuadPatch *lpatch = new LinearQuadPatch();
		hull = lpatch->hull;
		patch = lpatch;
	}
	else {
		assert(0); /* n-gons should have been split already */
		return NULL;
	}

	int i = 0;

	for(SubdFace::EdgeIterator it(face->edge); !it.isDone(); it.advance())
		hull[i++] = it.current()->from()->co;

	if(face->num_edges() == 4)
		swap(hull[2], hull[3]);

	return patch;
}

CCL_NAMESPACE_END

